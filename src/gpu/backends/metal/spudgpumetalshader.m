//
// SpudGPU Metal backend - shader modules and pipeline state.
// SPIR-V is cross-compiled to MSL via SPIRV-Cross's C API (the same
// dependency the D3D12 backend already vendors for its own SPIR-V -> HLSL
// step, built here with the MSL backend instead - see CMakeLists.txt). The
// C API, not the C++ one, since this file is plain Objective-C and can't
// link against SPIRV-Cross's C++ template-heavy interface directly.
//

#if SPUDGPU_COMPILE_METAL_API

#include "spudgpu.h"
#include "spudcore.h"
#include "spudgpumetal.h"
#include "../../spudgpushared.h"
#include <Metal/Metal.h>
#include <spirv_cross_c.h>

// -----------------------------------------------------------------------
// SPIR-V -> MSL cross-compilation
// -----------------------------------------------------------------------

static SpvExecutionModel spudgpumetal___internal_spirv_execution_model(SPUDGPU_SHADER_STAGE stage) {
	switch (stage) {
	case SPUDGPU_SHADER_STAGE_VERTEX:   return SpvExecutionModelVertex;
	case SPUDGPU_SHADER_STAGE_FRAGMENT: return SpvExecutionModelFragment;
	case SPUDGPU_SHADER_STAGE_COMPUTE:  return SpvExecutionModelGLCompute;
	case SPUDGPU_SHADER_STAGE_MESH:     return SpvExecutionModelMeshEXT;
	case SPUDGPU_SHADER_STAGE_TASK:     return SpvExecutionModelTaskEXT;
	default:                            return SpvExecutionModelVertex;
	}
}

static SPUDRESULT spudgpumetal___internal_cross_compile_spirv_to_msl(
    const void *spirv_code,
    size_t spirv_size,
    SPUDGPU_SHADER_STAGE stage,
    NSString **out_msl_source,
    uint32_t *out_local_size_x,
    uint32_t *out_local_size_y,
    uint32_t *out_local_size_z) {
	spvc_context context = NULL;
	if (spvc_context_create(&context) != SPVC_SUCCESS)
		return SPUDRESULT_GPU_SHADER_COMPILATION_FAILED;

	spvc_parsed_ir parsed_ir = NULL;
	spvc_result result = spvc_context_parse_spirv(
	    context, (const SpvId *)spirv_code, spirv_size / sizeof(SpvId), &parsed_ir);
	if (result != SPVC_SUCCESS) {
		printf("spudgpu: SPIR-V parse failed: %s\n", spvc_context_get_last_error_string(context));
		spvc_context_destroy(context);
		return SPUDRESULT_GPU_SHADER_COMPILATION_FAILED;
	}

	spvc_compiler compiler = NULL;
	result = spvc_context_create_compiler(
	    context, SPVC_BACKEND_MSL, parsed_ir, SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &compiler);
	if (result != SPVC_SUCCESS) {
		spvc_context_destroy(context);
		return SPUDRESULT_GPU_SHADER_COMPILATION_FAILED;
	}

	spvc_compiler_options options = NULL;
	spvc_compiler_create_compiler_options(compiler, &options);
	// 2.4.0 encoded as (major * 10000 + minor * 100 + patch), matching
	// CompilerMSL::Options::make_msl_version - a recent-enough MSL version
	// for everything this backend's command-encoding side already uses
	// (base vertex/instance draws, argument buffers) without depending on
	// spvc_msl_make_version() being available in every SPIRV-Cross revision.
	spvc_compiler_options_set_uint(options, SPVC_COMPILER_OPTION_MSL_VERSION, 20400u);
	spvc_compiler_options_set_uint(options, SPVC_COMPILER_OPTION_MSL_PLATFORM, SPVC_MSL_PLATFORM_MACOS);
	// Every SPUDGPU_DESCRIPTOR_TYPE_* descriptor set becomes a real Metal 2
	// argument buffer (see spudgpu.h's descriptor set layout docs: "On Metal
	// it creates an MTLArgumentEncoder schema") rather than discrete
	// per-resource bindings - Tier2 (1) is required for the writable
	// resources SPUDGPU_DESCRIPTOR_TYPE_STORAGE_BUFFER/STORAGE_IMAGE need and
	// is supported on every Apple Silicon GPU this backend targets.
	spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_MSL_ARGUMENT_BUFFERS, SPVC_TRUE);
	spvc_compiler_options_set_uint(options, SPVC_COMPILER_OPTION_MSL_ARGUMENT_BUFFERS_TIER, 1u);
	// Without this, SPIRV-Cross assigns each argument-buffer member's [[id(N)]]
	// sequentially in its own internal resource order (empirically: order of
	// first use in the shader body, via glslang's lazy SPIR-V ID allocation) -
	// completely decoupled from the resource's actual GLSL `binding=`/SPIR-V
	// Binding decoration. That default silently matched every binding=0-only
	// set this backend had exercised so far, but breaks the moment a set has
	// more than one binding (see spudgpumetaldescriptors.m, which assumes
	// [[id(binding)]] == binding). This option forces [[id(N)]] to be the
	// resource's real Binding decoration, matching Vulkan/D3D12 semantics and
	// the write-by-binding-index assumption every *_bind_descriptor_sets*
	// function makes.
	spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_MSL_ENABLE_DECORATION_BINDING, SPVC_TRUE);
	// A set's argument buffer is encoded once, from its layout's every
	// binding (spudgpumetaldescriptors.m), and its members sit where the
	// layout's order puts them. SPIRV-Cross by default leaves out of a
	// shader's argument buffer struct each resource the entry point doesn't
	// use, which moves every member after it: a shader that read binding 5
	// of a set it used nothing else of below 5 but 0 read the wrong argument
	// (apricottests' test_lit_sky, 2026-10-17). With this every resource the
	// shader declares is in its struct, used or not. A shader that doesn't
	// even declare a binding of the layout below one it reads is still out
	// of step: CLAUDE.md's known gaps say so.
	spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_MSL_FORCE_ACTIVE_ARGUMENT_BUFFER_RESOURCES, SPVC_TRUE);
	// SpudGPU's clip space is Vulkan's: +Y points down the screen. Metal's
	// points up (like D3D12's), so flip gl_Position.y to render the same image
	// - and front-face winding - as Vulkan. Same fix as the D3D12 backend.
	spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_FLIP_VERTEX_Y, SPVC_TRUE);
	spvc_compiler_install_compiler_options(compiler, options);

	// Pin every possible descriptor set's whole argument buffer to a fixed,
	// collision-free MSL buffer index up front (SPVC_MSL_ARGUMENT_BUFFER_BINDING
	// is the sentinel `binding` value meaning "the argument buffer for this
	// desc_set as a whole", not an individual member within it - members keep
	// SPIRV-Cross's default [[id(binding)]] assignment, which
	// spudgpumetaldescriptors.m relies on matching the API's own binding
	// numbers). Registering all SPUDGPU_MAX_DESCRIPTOR_SET_LAYOUTS slots
	// unconditionally is harmless - SPIRV-Cross ignores bindings for sets a
	// given shader stage doesn't actually declare.
	SpvExecutionModel execution_model = spudgpumetal___internal_spirv_execution_model(stage);
	for (uint32_t set = 0; set < SPUDGPU_MAX_DESCRIPTOR_SET_LAYOUTS; set++) {
		spvc_msl_resource_binding_2 binding;
		spvc_msl_resource_binding_init_2(&binding);
		binding.stage      = execution_model;
		binding.desc_set   = set;
		binding.binding    = SPVC_MSL_ARGUMENT_BUFFER_BINDING;
		binding.msl_buffer = SPUDGPU_METAL_DESCRIPTOR_SET_BUFFER_INDEX_BASE + set;
		spvc_compiler_msl_add_resource_binding_2(compiler, &binding);
	}

	// Without this, SPIRV-Cross auto-assigns the push-constant block's own
	// buffer index from its normal free-slot pool, which only happens to
	// avoid colliding with the pins above when every one of
	// SPUDGPU_MAX_DESCRIPTOR_SET_LAYOUTS sets is actually declared by the
	// shader (an unused set's pin above is a no-op, per SPIRV-Cross's own
	// docs, so that slot silently re-enters the free pool). No sample
	// exercised push constants + Metal cross-compilation with fewer than 4
	// sets declared until this one, where auto-assignment landed on buffer
	// 28 - colliding with descriptor set 2's reserved slot and leaving
	// spudgpu_cmd_push_constants' hardcoded SPUDGPU_METAL_PUSH_CONSTANTS_BUFFER_INDEX
	// (30) writing to a buffer index the shader never actually bound.
	// Pinning it explicitly here makes it collision-proof regardless of how
	// many sets a given shader declares.
	{
		spvc_msl_resource_binding_2 push_constant_binding;
		spvc_msl_resource_binding_init_2(&push_constant_binding);
		push_constant_binding.stage      = execution_model;
		push_constant_binding.desc_set   = SPVC_MSL_PUSH_CONSTANT_DESC_SET;
		push_constant_binding.binding    = SPVC_MSL_PUSH_CONSTANT_BINDING;
		push_constant_binding.msl_buffer = SPUDGPU_METAL_PUSH_CONSTANTS_BUFFER_INDEX;
		spvc_compiler_msl_add_resource_binding_2(compiler, &push_constant_binding);
	}

	// GLSL's layout(local_size_x/y/z) becomes SPIR-V's LocalSize execution
	// mode - meaningful for compute and mesh modules alike, and only after
	// spvc_context_create_compiler above has parsed it out of the module's
	// declared execution modes. Metal has no equivalent of Vulkan/D3D12
	// reading this straight out of the bound pipeline at dispatch time
	// (see spudgpu_shader_module_metal::_local_size_x/y/z in spudgpumetal.h),
	// so it's reflected here and threaded through to spudgpu_cmd_dispatch/
	// spudgpu_cmd_dispatch_mesh.
	if (stage == SPUDGPU_SHADER_STAGE_COMPUTE || stage == SPUDGPU_SHADER_STAGE_MESH) {
		if (out_local_size_x) *out_local_size_x = spvc_compiler_get_execution_mode_argument_by_index(compiler, SpvExecutionModeLocalSize, 0);
		if (out_local_size_y) *out_local_size_y = spvc_compiler_get_execution_mode_argument_by_index(compiler, SpvExecutionModeLocalSize, 1);
		if (out_local_size_z) *out_local_size_z = spvc_compiler_get_execution_mode_argument_by_index(compiler, SpvExecutionModeLocalSize, 2);
	}

	const char *msl_source = NULL;
	result = spvc_compiler_compile(compiler, &msl_source);
	if (result != SPVC_SUCCESS || !msl_source) {
		printf("spudgpu: SPIR-V -> MSL cross-compile failed: %s\n", spvc_context_get_last_error_string(context));
		spvc_context_destroy(context);
		return SPUDRESULT_GPU_SHADER_COMPILATION_FAILED;
	}

	// -stringWithUTF8String: copies the bytes into a new NSString, so the
	// context (which owns msl_source's storage) can be torn down right
	// after this - nothing above needs to outlive this function.
	*out_msl_source = [NSString stringWithUTF8String:msl_source];
	spvc_context_destroy(context);
	return SPUD_SUCCESS;
}

// The descriptor sets [bindings] - a function's bindings in a pipeline's
// reflection - read: bit n for each buffer argument it uses at set n's
// argument buffer index. All bits when [bindings] is nil.
static uint32_t spudgpumetal___internal_set_mask(NSArray<id<MTLBinding>> *bindings) {
	if (!bindings)
		return 0xffffffffu;
	uint32_t mask = 0;
	for (id<MTLBinding> binding in bindings) {
		if (binding.type != MTLBindingTypeBuffer || !binding.used)
			continue;
		NSUInteger index = binding.index;
		if (index >= SPUDGPU_METAL_DESCRIPTOR_SET_BUFFER_INDEX_BASE &&
		    index < SPUDGPU_METAL_DESCRIPTOR_SET_BUFFER_INDEX_BASE + SPUDGPU_MAX_DESCRIPTOR_SET_LAYOUTS)
			mask |= 1u << (uint32_t)(index - SPUDGPU_METAL_DESCRIPTOR_SET_BUFFER_INDEX_BASE);
	}
	return mask;
}

// -----------------------------------------------------------------------
// Shader modules
// -----------------------------------------------------------------------

SPUDRESULT spudgpu_create_shader_module(
    spudgpu_device device,
    const spudgpu_shader_module_desc *desc,
    spudgpu_shader_module *out_module) {
	if (!device)
		return SPUDRESULT_GPU_INVALID_DEVICE;
	if (!desc)
		return SPUDRESULT_NULL_DESC;
	if (!out_module)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	if (!desc->spirv_code || desc->spirv_size == 0)
		return SPUDRESULT_GPU_NULL_SPIRV;
	if (desc->spirv_size % 4 != 0)
		return SPUDRESULT_GPU_INVALID_SPIRV_ALIGNMENT;

	spudgpu_device_metal *device_metal = (spudgpu_device_metal *)device;
	SPUDRESULT sr                      = SPUD_SUCCESS;

	spudgpu_shader_module_metal *module_metal =
	    (spudgpu_shader_module_metal *)calloc(1, sizeof(spudgpu_shader_module_metal));
	if (!module_metal) {
		sr = SPUDRESULT_GENERAL_FAILURE;
		goto failedattempt;
	}
	module_metal->_parent_device = device_metal;
	module_metal->_desc          = *desc;

	{
		NSString *msl_source = nil;
		sr = spudgpumetal___internal_cross_compile_spirv_to_msl(
		    desc->spirv_code, desc->spirv_size, desc->stage, &msl_source,
		    &module_metal->_local_size_x, &module_metal->_local_size_y, &module_metal->_local_size_z);
		if (sr != SPUD_SUCCESS)
			goto failedattempt;

		NSError *compile_error             = nil;
		MTLCompileOptions *compile_options = [[MTLCompileOptions alloc] init];
		module_metal->_library_mtl         = [device_metal->_device_mtl newLibraryWithSource:msl_source
		                                                                             options:compile_options
		                                                                               error:&compile_error];
		[compile_options release];
		if (!module_metal->_library_mtl) {
			printf(
			    "spudgpu: MSL compile failed: %s\n",
			    compile_error ? compile_error.localizedDescription.UTF8String : "(unknown)");
			sr = SPUDRESULT_GPU_SHADER_COMPILATION_FAILED;
			goto failedattempt;
		}

		// SPIRV-Cross's MSL backend always renames a SPIR-V module's entry
		// point to "main0" when the module has exactly one - the expected
		// shape for this project's per-stage-compiled shaders - since MSL
		// reserves the literal name "main". spudgpu_shader_pipeline_desc's/
		// spudgpu_compute_pipeline_desc's *_entry_point strings are accepted
		// for cross-backend API symmetry but not consulted on this backend.
		module_metal->_function_mtl = [module_metal->_library_mtl newFunctionWithName:@"main0"];
		if (!module_metal->_function_mtl) {
			sr = SPUDRESULT_GPU_SHADER_COMPILATION_FAILED;
			goto failedattempt;
		}
	}

#if _DEBUG
	if (spud_debug_name_set(module_metal, desc->debug_name) != SPUD_SUCCESS) {
		spudgpu_destroy_shader_module((spudgpu_shader_module)module_metal);
		return SPUDRESULT_OUT_OF_MEMORY;
	}
	module_metal->_desc.debug_name = module_metal->_debug_name;
#endif

	*out_module = (spudgpu_shader_module)module_metal;

	return sr;

failedattempt:
	if (module_metal) {
		if (module_metal->_function_mtl) {
			[module_metal->_function_mtl release];
		}
		if (module_metal->_library_mtl) {
			[module_metal->_library_mtl release];
		}
		free(module_metal);
	}
	return sr;
}

void spudgpu_destroy_shader_module(spudgpu_shader_module shader_module) {
	spudgpu_shader_module_metal *module_metal = (spudgpu_shader_module_metal *)shader_module;
	if (module_metal) {
		if (module_metal->_function_mtl) {
			[module_metal->_function_mtl release];
		}
		if (module_metal->_library_mtl) {
			[module_metal->_library_mtl release];
		}
#if _DEBUG
		free((void *)module_metal->_debug_name);
#endif
		free(module_metal);
	}
}

// -----------------------------------------------------------------------
// Render (shader) pipelines
// -----------------------------------------------------------------------

static MTLVertexFormat spudgpumetal___internal_vertex_format(SPUDGPU_FORMAT format) {
	switch (format) {
	case SPUDGPU_FORMAT_R8_UNORM:            return MTLVertexFormatUCharNormalized;
	case SPUDGPU_FORMAT_R8_SNORM:            return MTLVertexFormatCharNormalized;
	case SPUDGPU_FORMAT_R8_UINT:             return MTLVertexFormatUChar;
	case SPUDGPU_FORMAT_R8_SINT:             return MTLVertexFormatChar;
	case SPUDGPU_FORMAT_R8G8_UNORM:          return MTLVertexFormatUChar2Normalized;
	case SPUDGPU_FORMAT_R8G8_SNORM:          return MTLVertexFormatChar2Normalized;
	case SPUDGPU_FORMAT_R8G8_UINT:           return MTLVertexFormatUChar2;
	case SPUDGPU_FORMAT_R8G8_SINT:           return MTLVertexFormatChar2;
	case SPUDGPU_FORMAT_R8G8B8A8_UNORM:      return MTLVertexFormatUChar4Normalized;
	case SPUDGPU_FORMAT_R8G8B8A8_SNORM:      return MTLVertexFormatChar4Normalized;
	case SPUDGPU_FORMAT_R8G8B8A8_UINT:       return MTLVertexFormatUChar4;
	case SPUDGPU_FORMAT_R8G8B8A8_SINT:       return MTLVertexFormatChar4;
	case SPUDGPU_FORMAT_R16_FLOAT:           return MTLVertexFormatHalf;
	case SPUDGPU_FORMAT_R16_UNORM:           return MTLVertexFormatUShortNormalized;
	case SPUDGPU_FORMAT_R16_SNORM:           return MTLVertexFormatShortNormalized;
	case SPUDGPU_FORMAT_R16_UINT:            return MTLVertexFormatUShort;
	case SPUDGPU_FORMAT_R16_SINT:            return MTLVertexFormatShort;
	case SPUDGPU_FORMAT_R16G16_FLOAT:        return MTLVertexFormatHalf2;
	case SPUDGPU_FORMAT_R16G16_UNORM:        return MTLVertexFormatUShort2Normalized;
	case SPUDGPU_FORMAT_R16G16_SNORM:        return MTLVertexFormatShort2Normalized;
	case SPUDGPU_FORMAT_R16G16_UINT:         return MTLVertexFormatUShort2;
	case SPUDGPU_FORMAT_R16G16_SINT:         return MTLVertexFormatShort2;
	case SPUDGPU_FORMAT_R16G16B16A16_FLOAT:  return MTLVertexFormatHalf4;
	case SPUDGPU_FORMAT_R16G16B16A16_UNORM:  return MTLVertexFormatUShort4Normalized;
	case SPUDGPU_FORMAT_R16G16B16A16_SNORM:  return MTLVertexFormatShort4Normalized;
	case SPUDGPU_FORMAT_R16G16B16A16_UINT:   return MTLVertexFormatUShort4;
	case SPUDGPU_FORMAT_R16G16B16A16_SINT:   return MTLVertexFormatShort4;
	case SPUDGPU_FORMAT_R32_FLOAT:           return MTLVertexFormatFloat;
	case SPUDGPU_FORMAT_R32_UINT:            return MTLVertexFormatUInt;
	case SPUDGPU_FORMAT_R32_SINT:            return MTLVertexFormatInt;
	case SPUDGPU_FORMAT_R32G32_FLOAT:        return MTLVertexFormatFloat2;
	case SPUDGPU_FORMAT_R32G32_UINT:         return MTLVertexFormatUInt2;
	case SPUDGPU_FORMAT_R32G32_SINT:         return MTLVertexFormatInt2;
	// Unlike MTLPixelFormat, MTLVertexFormat does have 3-component 32-bit
	// variants - vertex buffers aren't going through the texture unit, so
	// the same restriction spudgpumetal___internal_image_pixel_format
	// documents doesn't apply here.
	case SPUDGPU_FORMAT_R32G32B32_FLOAT:     return MTLVertexFormatFloat3;
	case SPUDGPU_FORMAT_R32G32B32_UINT:      return MTLVertexFormatUInt3;
	case SPUDGPU_FORMAT_R32G32B32_SINT:      return MTLVertexFormatInt3;
	case SPUDGPU_FORMAT_R32G32B32A32_FLOAT:  return MTLVertexFormatFloat4;
	case SPUDGPU_FORMAT_R32G32B32A32_UINT:   return MTLVertexFormatUInt4;
	case SPUDGPU_FORMAT_R32G32B32A32_SINT:   return MTLVertexFormatInt4;
	case SPUDGPU_FORMAT_R10G10B10A2_UNORM:   return MTLVertexFormatUInt1010102Normalized;
	default:                                 return MTLVertexFormatInvalid;
	}
}

// The class of primitive a topology draws. Patches have none here, for the
// reason spudgpumetal___internal_primitive_type gives, and fall back the
// same way.
static MTLPrimitiveTopologyClass spudgpumetal___internal_topology_class(SPUDGPU_PRIMITIVE_TOPOLOGY topology) {
	switch (topology) {
	case SPUDGPU_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:  return MTLPrimitiveTopologyClassTriangle;
	case SPUDGPU_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP: return MTLPrimitiveTopologyClassTriangle;
	case SPUDGPU_PRIMITIVE_TOPOLOGY_LINE_LIST:      return MTLPrimitiveTopologyClassLine;
	case SPUDGPU_PRIMITIVE_TOPOLOGY_LINE_STRIP:     return MTLPrimitiveTopologyClassLine;
	case SPUDGPU_PRIMITIVE_TOPOLOGY_POINT_LIST:     return MTLPrimitiveTopologyClassPoint;
	default:                                        return MTLPrimitiveTopologyClassTriangle;
	}
}

static MTLCompareFunction spudgpumetal___internal_compare_function(SPUDGPU_COMPARE_OP op) {
	switch (op) {
	case SPUDGPU_COMPARE_OP_NEVER:            return MTLCompareFunctionNever;
	case SPUDGPU_COMPARE_OP_LESS:             return MTLCompareFunctionLess;
	case SPUDGPU_COMPARE_OP_EQUAL:            return MTLCompareFunctionEqual;
	case SPUDGPU_COMPARE_OP_LESS_OR_EQUAL:    return MTLCompareFunctionLessEqual;
	case SPUDGPU_COMPARE_OP_GREATER:          return MTLCompareFunctionGreater;
	case SPUDGPU_COMPARE_OP_NOT_EQUAL:        return MTLCompareFunctionNotEqual;
	case SPUDGPU_COMPARE_OP_GREATER_OR_EQUAL: return MTLCompareFunctionGreaterEqual;
	default:                                  return MTLCompareFunctionAlways;
	}
}

static MTLBlendFactor spudgpumetal___internal_blend_factor(SPUDGPU_BLEND_FACTOR factor) {
	switch (factor) {
	case SPUDGPU_BLEND_FACTOR_ONE:                 return MTLBlendFactorOne;
	case SPUDGPU_BLEND_FACTOR_SRC_ALPHA:           return MTLBlendFactorSourceAlpha;
	case SPUDGPU_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA: return MTLBlendFactorOneMinusSourceAlpha;
	case SPUDGPU_BLEND_FACTOR_DST_ALPHA:           return MTLBlendFactorDestinationAlpha;
	case SPUDGPU_BLEND_FACTOR_ONE_MINUS_DST_ALPHA: return MTLBlendFactorOneMinusDestinationAlpha;
	case SPUDGPU_BLEND_FACTOR_SRC_COLOR:           return MTLBlendFactorSourceColor;
	case SPUDGPU_BLEND_FACTOR_ONE_MINUS_SRC_COLOR: return MTLBlendFactorOneMinusSourceColor;
	case SPUDGPU_BLEND_FACTOR_DST_COLOR:           return MTLBlendFactorDestinationColor;
	case SPUDGPU_BLEND_FACTOR_ONE_MINUS_DST_COLOR: return MTLBlendFactorOneMinusDestinationColor;
	default:                                       return MTLBlendFactorZero;
	}
}

static MTLBlendOperation spudgpumetal___internal_blend_operation(SPUDGPU_BLEND_OP op) {
	switch (op) {
	case SPUDGPU_BLEND_OP_SUBTRACT:         return MTLBlendOperationSubtract;
	case SPUDGPU_BLEND_OP_REVERSE_SUBTRACT: return MTLBlendOperationReverseSubtract;
	case SPUDGPU_BLEND_OP_MIN:              return MTLBlendOperationMin;
	case SPUDGPU_BLEND_OP_MAX:              return MTLBlendOperationMax;
	default:                                return MTLBlendOperationAdd;
	}
}

// The pipeline's colour attachments, each with its format and blend state
// (spudgpu_shader_pipeline_desc::color_attachment_formats and
// blend_attachments). Every format has been checked.
static void spudgpumetal___internal_set_color_attachments(
    MTLRenderPipelineColorAttachmentDescriptorArray *attachments,
    const spudgpu_shader_pipeline_desc *desc) {
	for (uint32_t i = 0; i < desc->color_attachment_count; ++i) {
		MTLRenderPipelineColorAttachmentDescriptor *attachment = attachments[i];
		const spudgpu_blend_attachment_desc *blend             = &desc->blend_attachments[i];
		attachment.pixelFormat     = spudgpumetal___internal_image_pixel_format(desc->color_attachment_formats[i]);
		attachment.blendingEnabled = blend->blend_enable;
		if (blend->blend_enable) {
			attachment.sourceRGBBlendFactor        = spudgpumetal___internal_blend_factor(blend->src_color_blend_factor);
			attachment.destinationRGBBlendFactor   = spudgpumetal___internal_blend_factor(blend->dst_color_blend_factor);
			attachment.rgbBlendOperation           = spudgpumetal___internal_blend_operation(blend->color_blend_op);
			attachment.sourceAlphaBlendFactor      = spudgpumetal___internal_blend_factor(blend->src_alpha_blend_factor);
			attachment.destinationAlphaBlendFactor = spudgpumetal___internal_blend_factor(blend->dst_alpha_blend_factor);
			attachment.alphaBlendOperation         = spudgpumetal___internal_blend_operation(blend->alpha_blend_op);
		}
	}
}

SPUDRESULT spudgpu_create_shader_pipeline(
    spudgpu_device device,
    const spudgpu_shader_pipeline_desc *desc,
    spudgpu_shader_pipeline *out_pipeline) {
	if (!device)
		return SPUDRESULT_GPU_INVALID_DEVICE;
	if (!desc)
		return SPUDRESULT_NULL_DESC;
	if (!out_pipeline)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	if ((!desc->vertex_module && !desc->mesh_module) || !desc->fragment_module)
		return SPUDRESULT_GPU_VERTEX_AND_FRAGMENT_SHADER_REQUIRED;
	if (desc->vertex_module && desc->mesh_module)
		return SPUDRESULT_GPU_INVALID_SHADER_STAGE; // Mutually exclusive - see spudgpu.h.
	SPUDRESULT outputs = spudgpu___internal_check_pipeline_outputs(desc, &((spudgpu_device_metal *)device)->_properties);
	if (outputs != SPUD_SUCCESS)
		return outputs;
	// Metal has no geometry-shader stage at all, and tessellation goes
	// through a completely different pipeline shape (post-tessellation
	// vertex functions + -drawPatches:/-drawIndexedPatches:, not a
	// tess-control/tess-eval stage pair) that this backend doesn't
	// implement - reject rather than silently dropping these stages.
	if (desc->geometry_module || desc->tess_control_module || desc->tess_eval_module)
		return SPUDRESULT_GPU_INVALID_SHADER_STAGE;

	spudgpu_shader_module_metal *vertex_module_metal   = (spudgpu_shader_module_metal *)desc->vertex_module;
	spudgpu_shader_module_metal *mesh_module_metal      = (spudgpu_shader_module_metal *)desc->mesh_module;
	spudgpu_shader_module_metal *fragment_module_metal = (spudgpu_shader_module_metal *)desc->fragment_module;
	if (vertex_module_metal && !vertex_module_metal->_function_mtl)
		return SPUDRESULT_GPU_INVALID_SHADER_MODULE;
	if (mesh_module_metal && !mesh_module_metal->_function_mtl)
		return SPUDRESULT_GPU_INVALID_SHADER_MODULE;
	if (!fragment_module_metal->_function_mtl)
		return SPUDRESULT_GPU_INVALID_SHADER_MODULE;

	for (uint32_t i = 0; i < desc->color_attachment_count; ++i)
		if (spudgpumetal___internal_image_pixel_format(desc->color_attachment_formats[i]) == MTLPixelFormatInvalid)
			return SPUDRESULT_GPU_INVALID_FORMAT;
	MTLPixelFormat depth_format = MTLPixelFormatInvalid;
	if (desc->depth_format != SPUDGPU_FORMAT_UNKNOWN) {
		depth_format = spudgpumetal___internal_image_pixel_format(desc->depth_format);
		if (depth_format == MTLPixelFormatInvalid)
			return SPUDRESULT_GPU_INVALID_FORMAT;
	}

	spudgpu_device_metal *device_metal = (spudgpu_device_metal *)device;
	SPUDRESULT sr                      = SPUD_SUCCESS;

	spudgpu_shader_pipeline_metal *pipeline_metal =
	    (spudgpu_shader_pipeline_metal *)calloc(1, sizeof(spudgpu_shader_pipeline_metal));
	if (!pipeline_metal) {
		sr = SPUDRESULT_GENERAL_FAILURE;
		goto failedattempt;
	}
	pipeline_metal->_parent_device = device_metal;
	pipeline_metal->_desc          = *desc;

	if (mesh_module_metal) {
		// Mesh-shader pipeline: no vertex input state/input-assembler at
		// all - the mesh function supplies its own geometry. See
		// spudgpu_shader_pipeline_desc::mesh_module in spudgpu.h.
		MTLMeshRenderPipelineDescriptor *pipeline_desc = [[MTLMeshRenderPipelineDescriptor alloc] init];
		pipeline_desc.meshFunction                     = mesh_module_metal->_function_mtl;
		pipeline_desc.fragmentFunction                 = fragment_module_metal->_function_mtl;
		spudgpumetal___internal_set_color_attachments(pipeline_desc.colorAttachments, desc);

		if (depth_format != MTLPixelFormatInvalid) {
			pipeline_desc.depthAttachmentPixelFormat = depth_format;
			if (depth_format == MTLPixelFormatDepth32Float_Stencil8) {
				pipeline_desc.stencilAttachmentPixelFormat = depth_format;
			}
		}

		NSError *pipeline_error                 = nil;
		MTLRenderPipelineReflection *reflection = nil;
		pipeline_metal->_render_pipeline_state_mtl = [device_metal->_device_mtl
		    newRenderPipelineStateWithMeshDescriptor:pipeline_desc
		                                      options:MTLPipelineOptionBindingInfo
		                                   reflection:&reflection
		                                        error:&pipeline_error];
		// Autoreleased: read now, before anything can release it.
		pipeline_metal->_non_fragment_set_mask =
		    reflection ? (spudgpumetal___internal_set_mask(reflection.meshBindings) | spudgpumetal___internal_set_mask(reflection.objectBindings))
		               : 0xffffffffu;
		pipeline_metal->_fragment_set_mask = reflection ? spudgpumetal___internal_set_mask(reflection.fragmentBindings) : 0xffffffffu;
		[pipeline_desc release];
		if (!pipeline_metal->_render_pipeline_state_mtl) {
			printf(
			    "spudgpu: MTLRenderPipelineState (mesh) creation failed: %s\n",
			    pipeline_error ? pipeline_error.localizedDescription.UTF8String : "(unknown)");
			sr = SPUDRESULT_GPU_SHADER_COMPILATION_FAILED;
			goto failedattempt;
		}

		pipeline_metal->_is_mesh_pipeline    = true;
		pipeline_metal->_mesh_local_size_x   = mesh_module_metal->_local_size_x;
		pipeline_metal->_mesh_local_size_y   = mesh_module_metal->_local_size_y;
		pipeline_metal->_mesh_local_size_z   = mesh_module_metal->_local_size_z;
	} else {
		// descriptor_set_layouts is intentionally unconsumed here: unlike
		// Vulkan's VkPipelineLayout or D3D12's ID3D12RootSignature, building
		// an MTLRenderPipelineState needs no resource-layout object at all -
		// the (set, binding) -> flat-argument-index scheme lives entirely in
		// the already-cross-compiled MSL source (SPIRV-Cross's own default
		// assignment, for now - see spudgpumetaldescriptors.m, which will
		// need to agree with whatever scheme is used once it's implemented).
		MTLVertexDescriptor *vertex_desc = [MTLVertexDescriptor vertexDescriptor];
		for (uint32_t i = 0; i < desc->vertex_attribute_count; i++) {
			const spudgpu_vertex_attribute_desc *a = &desc->vertex_attributes[i];
			MTLVertexAttributeDescriptor *attr     = vertex_desc.attributes[a->location];
			attr.format                            = spudgpumetal___internal_vertex_format(a->format);
			attr.offset                            = a->offset;
			attr.bufferIndex                       = a->binding;
		}
		for (uint32_t i = 0; i < desc->vertex_binding_count; i++) {
			const spudgpu_vertex_binding_desc *b   = &desc->vertex_bindings[i];
			MTLVertexBufferLayoutDescriptor *layout = vertex_desc.layouts[b->binding];
			layout.stride                           = b->stride;
			layout.stepFunction = b->per_instance ? MTLVertexStepFunctionPerInstance : MTLVertexStepFunctionPerVertex;
			layout.stepRate     = 1;
		}

		MTLRenderPipelineDescriptor *pipeline_desc = [[MTLRenderPipelineDescriptor alloc] init];
		pipeline_desc.vertexFunction               = vertex_module_metal->_function_mtl;
		pipeline_desc.fragmentFunction              = fragment_module_metal->_function_mtl;
		pipeline_desc.vertexDescriptor              = vertex_desc;
		// Metal refuses a pipeline in a layered pass
		// (spudgpu_rendering_begin_desc::layer_count) unless this is given,
		// and a pipeline isn't created against a pass, so it is always given.
		pipeline_desc.inputPrimitiveTopology = spudgpumetal___internal_topology_class(desc->primitive_topology);
		spudgpumetal___internal_set_color_attachments(pipeline_desc.colorAttachments, desc);

		if (depth_format != MTLPixelFormatInvalid) {
			pipeline_desc.depthAttachmentPixelFormat = depth_format;
			// Depth32Float_Stencil8 is the only depth/stencil combined
			// format this backend maps images to (see
			// spudgpumetalimage.m) - a texture in that format is
			// implicitly usable as a stencil attachment too.
			if (depth_format == MTLPixelFormatDepth32Float_Stencil8) {
				pipeline_desc.stencilAttachmentPixelFormat = depth_format;
			}
		}

		NSError *pipeline_error                     = nil;
		MTLRenderPipelineReflection *reflection     = nil;
		pipeline_metal->_render_pipeline_state_mtl  = [device_metal->_device_mtl
		    newRenderPipelineStateWithDescriptor:pipeline_desc
		                                 options:MTLPipelineOptionBindingInfo
		                              reflection:&reflection
		                                    error:&pipeline_error];
		pipeline_metal->_non_fragment_set_mask = reflection ? spudgpumetal___internal_set_mask(reflection.vertexBindings) : 0xffffffffu;
		pipeline_metal->_fragment_set_mask     = reflection ? spudgpumetal___internal_set_mask(reflection.fragmentBindings) : 0xffffffffu;
		[pipeline_desc release];
		if (!pipeline_metal->_render_pipeline_state_mtl) {
			printf(
			    "spudgpu: MTLRenderPipelineState creation failed: %s\n",
			    pipeline_error ? pipeline_error.localizedDescription.UTF8String : "(unknown)");
			sr = SPUDRESULT_GPU_SHADER_COMPILATION_FAILED;
			goto failedattempt;
		}
	}

	{
		// Vulkan/D3D12 bake depth/stencil test config into the same
		// monolithic pipeline object as everything above; Metal keeps it as
		// a wholly separate native object - see spudgpu_cmd_bind_pipeline in
		// spudgpumetalcommand.m, which binds both together.
		MTLDepthStencilDescriptor *depth_stencil_desc = [[MTLDepthStencilDescriptor alloc] init];
		depth_stencil_desc.depthCompareFunction =
		    desc->depth_test_enable ? spudgpumetal___internal_compare_function(desc->depth_compare_op) : MTLCompareFunctionAlways;
		depth_stencil_desc.depthWriteEnabled = desc->depth_test_enable && desc->depth_write_enable;
		pipeline_metal->_depth_stencil_state_mtl =
		    [device_metal->_device_mtl newDepthStencilStateWithDescriptor:depth_stencil_desc];
		[depth_stencil_desc release];
		if (!pipeline_metal->_depth_stencil_state_mtl) {
			sr = SPUDRESULT_GPU_SHADER_COMPILATION_FAILED;
			goto failedattempt;
		}
	}

#if _DEBUG
	if (spud_debug_name_set(pipeline_metal, desc->debug_name) != SPUD_SUCCESS) {
		spudgpu_destroy_shader_pipeline((spudgpu_shader_pipeline)pipeline_metal);
		return SPUDRESULT_OUT_OF_MEMORY;
	}
	pipeline_metal->_desc.debug_name = pipeline_metal->_debug_name;
#endif

	*out_pipeline = (spudgpu_shader_pipeline)pipeline_metal;

	return sr;

failedattempt:
	if (pipeline_metal) {
		if (pipeline_metal->_depth_stencil_state_mtl) {
			[pipeline_metal->_depth_stencil_state_mtl release];
		}
		if (pipeline_metal->_render_pipeline_state_mtl) {
			[pipeline_metal->_render_pipeline_state_mtl release];
		}
		free(pipeline_metal);
	}
	return sr;
}

void spudgpu_destroy_shader_pipeline(spudgpu_shader_pipeline pipeline) {
	spudgpu_shader_pipeline_metal *pipeline_metal = (spudgpu_shader_pipeline_metal *)pipeline;
	if (pipeline_metal) {
		if (pipeline_metal->_depth_stencil_state_mtl) {
			[pipeline_metal->_depth_stencil_state_mtl release];
		}
		if (pipeline_metal->_render_pipeline_state_mtl) {
			[pipeline_metal->_render_pipeline_state_mtl release];
		}
#if _DEBUG
		free((void *)pipeline_metal->_debug_name);
#endif
		free(pipeline_metal);
	}
}

// -----------------------------------------------------------------------
// Mesh shading capabilities
// -----------------------------------------------------------------------

#if SPUDGPU_EXT_MESH_SHADING
SPUDRESULT spudgpu_get_mesh_shading_capabilities(
    spudgpu_device device,
    spudgpu_mesh_shading_capabilities *out_caps) {
	if (!device)
		return SPUDRESULT_GPU_INVALID_DEVICE;
	if (!out_caps)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;

	// Metal 3 mesh shading is required on every device this backend targets
	// (see spudgpu.h's bindless section for the same Metal-3-only baseline).
	// Metal has no single MTLDevice-level query for max mesh output vertex/
	// primitive counts the way Vulkan/D3D12 report fixed hardware limits -
	// Metal's own limit is a per-pipeline maxTotalThreadsPerMeshThreadgroup
	// (memory/thread-count driven, not a fixed vertex/primitive cap). These
	// are the conventional NV/AMD-style meshlet limits every sample in this
	// project's mesh-shader family targets, not a hardware-queried ceiling.
	out_caps->supported                       = true;
	out_caps->max_mesh_output_vertices         = 256;
	out_caps->max_mesh_output_primitives       = 256;
	out_caps->max_mesh_workgroup_invocations   = 1024;
	return SPUD_SUCCESS;
}
#endif // SPUDGPU_EXT_MESH_SHADING

// -----------------------------------------------------------------------
// Depth bounds test capabilities
// -----------------------------------------------------------------------

#if SPUDGPU_EXT_DEPTH_BOUNDS_TEST
SPUDRESULT spudgpu_get_depth_bounds_capabilities(
    spudgpu_device device,
    spudgpu_depth_bounds_capabilities *out_caps) {
	if (!device)
		return SPUDRESULT_GPU_INVALID_DEVICE;
	if (!out_caps)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;

	// Set once at device enumeration (spudgpumetalcontext.m) from
	// [_device_mtl supportsFamily:MTLGPUFamilyApple10].
	out_caps->supported = device->_depth_bounds_test_supported;
	return SPUD_SUCCESS;
}
#endif // SPUDGPU_EXT_DEPTH_BOUNDS_TEST

// -----------------------------------------------------------------------
// Compute pipelines
// -----------------------------------------------------------------------

SPUDRESULT spudgpu_create_compute_pipeline(
    spudgpu_device device,
    const spudgpu_compute_pipeline_desc *desc,
    spudgpu_compute_pipeline *out_pipeline) {
	if (!device)
		return SPUDRESULT_GPU_INVALID_DEVICE;
	if (!desc)
		return SPUDRESULT_NULL_DESC;
	if (!out_pipeline)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	if (!desc->compute_module)
		return SPUDRESULT_GPU_INVALID_COMPUTE_MODULE;

	spudgpu_shader_module_metal *compute_module_metal = (spudgpu_shader_module_metal *)desc->compute_module;
	if (!compute_module_metal->_function_mtl)
		return SPUDRESULT_GPU_INVALID_SHADER_MODULE;

	spudgpu_device_metal *device_metal = (spudgpu_device_metal *)device;
	SPUDRESULT sr                      = SPUD_SUCCESS;

	spudgpu_compute_pipeline_metal *pipeline_metal =
	    (spudgpu_compute_pipeline_metal *)calloc(1, sizeof(spudgpu_compute_pipeline_metal));
	if (!pipeline_metal) {
		sr = SPUDRESULT_GENERAL_FAILURE;
		goto failedattempt;
	}
	pipeline_metal->_parent_device = device_metal;
	pipeline_metal->_desc          = *desc;
	pipeline_metal->_local_size_x  = compute_module_metal->_local_size_x;
	pipeline_metal->_local_size_y  = compute_module_metal->_local_size_y;
	pipeline_metal->_local_size_z  = compute_module_metal->_local_size_z;

	{
		// Same reasoning as spudgpu_create_shader_pipeline above -
		// descriptor_set_layouts needs no consumption here either; a
		// MTLComputePipelineState needs no resource-layout object.
		NSError *pipeline_error = nil;
		pipeline_metal->_compute_pipeline_state_mtl =
		    [device_metal->_device_mtl newComputePipelineStateWithFunction:compute_module_metal->_function_mtl
		                                                              error:&pipeline_error];
		if (!pipeline_metal->_compute_pipeline_state_mtl) {
			printf(
			    "spudgpu: MTLComputePipelineState creation failed: %s\n",
			    pipeline_error ? pipeline_error.localizedDescription.UTF8String : "(unknown)");
			sr = SPUDRESULT_GPU_SHADER_COMPILATION_FAILED;
			goto failedattempt;
		}
	}

#if _DEBUG
	if (spud_debug_name_set(pipeline_metal, desc->debug_name) != SPUD_SUCCESS) {
		spudgpu_destroy_compute_pipeline((spudgpu_compute_pipeline)pipeline_metal);
		return SPUDRESULT_OUT_OF_MEMORY;
	}
	pipeline_metal->_desc.debug_name = pipeline_metal->_debug_name;
#endif

	*out_pipeline = (spudgpu_compute_pipeline)pipeline_metal;

	return sr;

failedattempt:
	if (pipeline_metal) {
		if (pipeline_metal->_compute_pipeline_state_mtl) {
			[pipeline_metal->_compute_pipeline_state_mtl release];
		}
		free(pipeline_metal);
	}
	return sr;
}

void spudgpu_destroy_compute_pipeline(spudgpu_compute_pipeline pipeline) {
	spudgpu_compute_pipeline_metal *pipeline_metal = (spudgpu_compute_pipeline_metal *)pipeline;
	if (pipeline_metal) {
		if (pipeline_metal->_compute_pipeline_state_mtl) {
			[pipeline_metal->_compute_pipeline_state_mtl release];
		}
#if _DEBUG
		free((void *)pipeline_metal->_debug_name);
#endif
		free(pipeline_metal);
	}
}

SPUDRESULT spudgpu_get_compute_pipeline_desc(
    spudgpu_compute_pipeline pipeline,
    spudgpu_compute_pipeline_desc *out_desc) {
	spudgpu_compute_pipeline_metal *pipeline_metal = (spudgpu_compute_pipeline_metal *)pipeline;
	if (!pipeline_metal)
		return SPUDRESULT_GPU_INVALID_COMPUTE_PIPELINE;
	if (!out_desc)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	*out_desc = pipeline_metal->_desc;
	return SPUD_SUCCESS;
}

#endif // SPUDGPU_COMPILE_METAL_API
