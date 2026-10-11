
#if SPUDGPU_COMPILE_D3D12_API

#include "spudgpu_d3d12_natives.h"
#include "spudgpud3d12.hpp"

#include <dxcapi.h>
#include <spirv_cross/spirv_hlsl.hpp>

static const wchar_t *stage_to_dxc_profile(SPUDGPU_SHADER_STAGE stage) {
	switch (stage) {
	case SPUDGPU_SHADER_STAGE_VERTEX:
		return L"vs_6_0";
	case SPUDGPU_SHADER_STAGE_FRAGMENT:
		return L"ps_6_0";
	case SPUDGPU_SHADER_STAGE_COMPUTE:
		return L"cs_6_0";
	case SPUDGPU_SHADER_STAGE_GEOMETRY:
		return L"gs_6_0";
	case SPUDGPU_SHADER_STAGE_TESSELLATION_CONTROL:
		return L"hs_6_0";
	case SPUDGPU_SHADER_STAGE_TESSELLATION_EVALUATION:
		return L"ds_6_0";
	case SPUDGPU_SHADER_STAGE_MESH:
		return L"ms_6_5";
	case SPUDGPU_SHADER_STAGE_TASK:
		return L"as_6_5";
	default:
		return nullptr;
	}
}

static D3D12_BLEND spudgpu_d3d12_blend_factor(SPUDGPU_BLEND_FACTOR f) {
	switch (f) {
	case SPUDGPU_BLEND_FACTOR_ZERO:
		return D3D12_BLEND_ZERO;
	case SPUDGPU_BLEND_FACTOR_ONE:
		return D3D12_BLEND_ONE;
	case SPUDGPU_BLEND_FACTOR_SRC_ALPHA:
		return D3D12_BLEND_SRC_ALPHA;
	case SPUDGPU_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA:
		return D3D12_BLEND_INV_SRC_ALPHA;
	case SPUDGPU_BLEND_FACTOR_DST_ALPHA:
		return D3D12_BLEND_DEST_ALPHA;
	case SPUDGPU_BLEND_FACTOR_ONE_MINUS_DST_ALPHA:
		return D3D12_BLEND_INV_DEST_ALPHA;
	case SPUDGPU_BLEND_FACTOR_SRC_COLOR:
		return D3D12_BLEND_SRC_COLOR;
	case SPUDGPU_BLEND_FACTOR_ONE_MINUS_SRC_COLOR:
		return D3D12_BLEND_INV_SRC_COLOR;
	case SPUDGPU_BLEND_FACTOR_DST_COLOR:
		return D3D12_BLEND_DEST_COLOR;
	case SPUDGPU_BLEND_FACTOR_ONE_MINUS_DST_COLOR:
		return D3D12_BLEND_INV_DEST_COLOR;
	default:
		return D3D12_BLEND_ZERO;
	}
}

static D3D_PRIMITIVE_TOPOLOGY spudgpu_d3d12_primitive_topology(
    SPUDGPU_PRIMITIVE_TOPOLOGY t, uint32_t patch_points) {
	switch (t) {
	case SPUDGPU_PRIMITIVE_TOPOLOGY_POINT_LIST:
		return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
	case SPUDGPU_PRIMITIVE_TOPOLOGY_LINE_LIST:
		return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
	case SPUDGPU_PRIMITIVE_TOPOLOGY_LINE_STRIP:
		return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
	case SPUDGPU_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
		return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
	case SPUDGPU_PRIMITIVE_TOPOLOGY_PATCH_LIST: {
		uint32_t n = patch_points ? patch_points : 3;
		return (
		    D3D_PRIMITIVE_TOPOLOGY)(D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST -
		                            1 + n);
	}
	default:
		return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	}
}

static D3D12_PRIMITIVE_TOPOLOGY_TYPE
spudgpu_d3d12_primitive_topology_type(SPUDGPU_PRIMITIVE_TOPOLOGY t) {
	switch (t) {
	case SPUDGPU_PRIMITIVE_TOPOLOGY_POINT_LIST:
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
	case SPUDGPU_PRIMITIVE_TOPOLOGY_LINE_LIST:
	case SPUDGPU_PRIMITIVE_TOPOLOGY_LINE_STRIP:
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
	case SPUDGPU_PRIMITIVE_TOPOLOGY_PATCH_LIST:
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
	default:
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	}
}

// Builds and creates a root signature from descriptor set layouts and push
// constant ranges. Push constants are mapped to b0, space1 to avoid
// collision with per-set CBV/SRV/UAV descriptors (which use space0..spaceN).
static HRESULT spudgpu_d3d12_build_root_signature(
    ID3D12Device *device,
    uint32_t set_count,
    void *const *set_layouts,
    uint32_t pc_range_count,
    const spudgpu_push_constant_range_desc *pc_ranges,
    D3D12_ROOT_SIGNATURE_FLAGS flags,
    ID3D12RootSignature **out_sig) {

	D3D12_DESCRIPTOR_RANGE
	ranges[SPUDGPU_MAX_DESCRIPTOR_SET_LAYOUTS]
	      [SPUDGPU_MAX_DESCRIPTOR_BINDINGS_PER_SET];
	uint32_t range_counts[SPUDGPU_MAX_DESCRIPTOR_SET_LAYOUTS] = {};

	for (uint32_t s = 0; s < set_count; ++s) {
		auto *layout = (spudgpu_descriptor_set_layout_d3d12 *)set_layouts[s];
		if (!layout)
			continue;
		uint32_t rc = 0;
		for (uint32_t b = 0; b < layout->_desc.binding_count; ++b) {
			const auto &bind = layout->_desc.bindings[b];
			D3D12_DESCRIPTOR_RANGE_TYPE rt;
			switch (bind.descriptor_type) {
			case SPUDGPU_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
				rt = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
				break;
			case SPUDGPU_DESCRIPTOR_TYPE_STORAGE_BUFFER:
			case SPUDGPU_DESCRIPTOR_TYPE_STORAGE_IMAGE:
				rt = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
				break;
			case SPUDGPU_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
			case SPUDGPU_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
				rt = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
				break;
			case SPUDGPU_DESCRIPTOR_TYPE_SAMPLER:
				// A sampler-only set (like this sample's sampler_layout)
				// produces a single-range, sampler-only table here, which
				// is valid. A layout that mixed SAMPLER with CBV/SRV/UAV
				// bindings would need two root parameters (D3D12 tables
				// can't span both the sampler heap and the CBV/SRV/UAV
				// heap) -- not needed today, since no current layout mixes
				// them, but worth knowing if one ever does.
				rt = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
				break;
			default:
				continue;
			}
			ranges[s][rc].RangeType          = rt;
			ranges[s][rc].NumDescriptors     = bind.count ? bind.count : 1;
			ranges[s][rc].BaseShaderRegister = bind.binding;
			ranges[s][rc].RegisterSpace      = s;
			ranges[s][rc].OffsetInDescriptorsFromTableStart =
			    D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
			++rc;
		}
		range_counts[s] = rc;
	}

	D3D12_ROOT_PARAMETER params[SPUDGPU_MAX_DESCRIPTOR_SET_LAYOUTS + 1];
	uint32_t param_count = 0;

	for (uint32_t s = 0; s < set_count; ++s) {
		params[param_count].ParameterType =
		    D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		params[param_count].DescriptorTable.NumDescriptorRanges =
		    range_counts[s];
		params[param_count].DescriptorTable.pDescriptorRanges = ranges[s];
		params[param_count].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		++param_count;
	}

	uint32_t total_pc_words = 0;
	for (uint32_t p = 0; p < pc_range_count; ++p) {
		uint32_t end = (pc_ranges[p].offset + pc_ranges[p].size + 3) / 4;
		if (end > total_pc_words)
			total_pc_words = end;
	}

	if (total_pc_words > 0) {
		params[param_count].ParameterType =
		    D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		params[param_count].Constants.ShaderRegister = 0; // b0
		// Descriptor sets occupy space0..space(set_count-1) (RegisterSpace = s
		// above), so a hardcoded space1 only avoided collision when a
		// pipeline used exactly one descriptor set -- with two or more (the
		// first real case: SpudGPUMeshShaders' mesh-data set=0 + globals
		// set=1), push constants at space1 collided directly with set 1's
		// own bindings. SPUDGPU_MAX_DESCRIPTOR_SET_LAYOUTS is always at
		// least one past the highest space any actual set can use, so it's
		// collision-free regardless of set_count -- this has to match
		// set_root_constant_layouts' rc.space in spudgpu_create_shader_module
		// exactly, since that's what the cross-compiled HLSL actually
		// expects.
		params[param_count].Constants.RegisterSpace = SPUDGPU_MAX_DESCRIPTOR_SET_LAYOUTS;
		params[param_count].Constants.Num32BitValues = total_pc_words;
		params[param_count].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		++param_count;
	}

	D3D12_ROOT_SIGNATURE_DESC sigDesc = {};
	sigDesc.NumParameters             = param_count;
	sigDesc.pParameters               = params;
	// Static samplers (baked into the root signature, zero descriptor-heap
	// cost) aren't supported yet — see the "no static/immutable sampler
	// support" gap in ../../../CLAUDE.md. Every sampler goes through the
	// dynamic, descriptor-bound spudgpu_sampler path today.
	sigDesc.NumStaticSamplers         = 0;
	sigDesc.pStaticSamplers           = nullptr;
	sigDesc.Flags                     = flags;

	Microsoft::WRL::ComPtr<ID3DBlob> sig_blob;
	Microsoft::WRL::ComPtr<ID3DBlob> error_blob;
	HRESULT hr = D3D12SerializeRootSignature(
	    &sigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sig_blob, &error_blob);
	if (FAILED(hr)) {
		// D3D12SerializeRootSignature reports the actual reason through
		// error_blob, not the D3D12 debug layer/ID3D12InfoQueue -- without
		// printing it, a failure here is silent (no debug-layer message at
		// all), unlike every other failure path in this file.
		if (error_blob)
			printf("spudgpu: D3D12SerializeRootSignature failed: %s\n",
			    (const char *)error_blob->GetBufferPointer());
		else
			printf("spudgpu: D3D12SerializeRootSignature failed: hr=0x%08lx\n", (unsigned long)hr);
		return hr;
	}

	return device->CreateRootSignature(
	    0, sig_blob->GetBufferPointer(), sig_blob->GetBufferSize(),
	    IID_PPV_ARGS(out_sig));
}

// ---------------------------------------------------------------------------
// SPUDGPU_EXT_MESH_SHADING - mesh pipeline creation.
//
// A mesh pipeline has no input-assembler/vertex-fetch stage at all, so it
// can't go through CreateGraphicsPipelineState's D3D12_GRAPHICS_PIPELINE_STATE_DESC
// (which hard-requires an InputLayout/PrimitiveTopologyType shaped around
// IA). It must go through CreatePipelineState's generic
// D3D12_PIPELINE_STATE_STREAM_DESC instead. The vendored d3dx12.h predates
// Microsoft's own CD3DX12_PIPELINE_STATE_STREAM_MS/_AS typedefs and
// CD3DX12_MESH_SHADER_PIPELINE_STATE_DESC helper (see spudlib/CLAUDE.md's
// D3D12 backend-status note) - rather than patch that vendored, third-party
// file, the two missing subobject typedefs are built here from its
// already-present generic CD3DX12_PIPELINE_STATE_STREAM_SUBOBJECT template,
// the exact mechanism Microsoft's own typedefs use internally. Every other
// subobject below (BLEND_DESC, RASTERIZER, DEPTH_STENCIL, ...) already
// exists in the vendored file and is reused as-is.
// ---------------------------------------------------------------------------

typedef CD3DX12_PIPELINE_STATE_STREAM_SUBOBJECT<
    D3D12_SHADER_BYTECODE, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_AS>
    CD3DX12_PIPELINE_STATE_STREAM_AS;
typedef CD3DX12_PIPELINE_STATE_STREAM_SUBOBJECT<
    D3D12_SHADER_BYTECODE, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS>
    CD3DX12_PIPELINE_STATE_STREAM_MS;

struct spudgpu_d3d12_mesh_pipeline_stream {
	CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
	CD3DX12_PIPELINE_STATE_STREAM_AS AS;
	CD3DX12_PIPELINE_STATE_STREAM_MS MS;
	CD3DX12_PIPELINE_STATE_STREAM_PS PS;
	CD3DX12_PIPELINE_STATE_STREAM_BLEND_DESC BlendState;
	CD3DX12_PIPELINE_STATE_STREAM_SAMPLE_MASK SampleMask;
	CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER RasterizerState;
	CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL1 DepthStencilState;
	CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
	CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
	CD3DX12_PIPELINE_STATE_STREAM_SAMPLE_DESC SampleDesc;
	CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
};

// Takes ownership of pResult on both success and failure (releases it on
// failure, hands it to *out_pipeline on success) - matches every other
// pipeline-creation path in this file. Called only after
// spudgpu_create_shader_pipeline has already built pResult's root
// signature with the mesh-appropriate flags.
static SPUDRESULT spudgpu_d3d12___create_mesh_shader_pipeline(
    spudgpu_device device,
    const spudgpu_shader_pipeline_desc *desc,
    spudgpu_shader_pipeline_d3d12 *pResult,
    spudgpu_shader_pipeline *out_pipeline) {

	D3D12_RASTERIZER_DESC rastDesc = {};
	rastDesc.FillMode =
	    desc->wireframe ? D3D12_FILL_MODE_WIREFRAME : D3D12_FILL_MODE_SOLID;
	rastDesc.CullMode              = (D3D12_CULL_MODE)(desc->cull_mode + 1);
	rastDesc.FrontCounterClockwise = desc->front_face_ccw ? TRUE : FALSE;
	rastDesc.DepthBias             = D3D12_DEFAULT_DEPTH_BIAS;
	rastDesc.DepthBiasClamp        = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	rastDesc.SlopeScaledDepthBias  = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	rastDesc.DepthClipEnable       = TRUE;
	rastDesc.MultisampleEnable     = FALSE;
	rastDesc.AntialiasedLineEnable = FALSE;
	rastDesc.ForcedSampleCount     = 0;
	rastDesc.ConservativeRaster    = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	D3D12_DEPTH_STENCILOP_DESC noOp = {
	    D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP,
	    D3D12_COMPARISON_FUNC_ALWAYS};
	D3D12_DEPTH_STENCIL_DESC dsDesc = {};
	dsDesc.DepthEnable    = desc->depth_test_enable ? TRUE : FALSE;
	dsDesc.DepthWriteMask = desc->depth_write_enable
	                            ? D3D12_DEPTH_WRITE_MASK_ALL
	                            : D3D12_DEPTH_WRITE_MASK_ZERO;
	dsDesc.DepthFunc        = (D3D12_COMPARISON_FUNC)(desc->depth_compare_op + 1);
	dsDesc.StencilEnable    = FALSE;
	dsDesc.StencilReadMask  = D3D12_DEFAULT_STENCIL_READ_MASK;
	dsDesc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
	dsDesc.FrontFace        = noOp;
	dsDesc.BackFace         = noOp;

	const auto &ba             = desc->blend_attachment;
	D3D12_BLEND_DESC blendDesc = {};
	blendDesc.AlphaToCoverageEnable         = FALSE;
	blendDesc.IndependentBlendEnable        = FALSE;
	blendDesc.RenderTarget[0].BlendEnable   = ba.blend_enable ? TRUE : FALSE;
	blendDesc.RenderTarget[0].LogicOpEnable = FALSE;
	blendDesc.RenderTarget[0].SrcBlend =
	    spudgpu_d3d12_blend_factor(ba.src_color_blend_factor);
	blendDesc.RenderTarget[0].DestBlend =
	    spudgpu_d3d12_blend_factor(ba.dst_color_blend_factor);
	blendDesc.RenderTarget[0].BlendOp = (D3D12_BLEND_OP)(ba.color_blend_op + 1);
	blendDesc.RenderTarget[0].SrcBlendAlpha =
	    spudgpu_d3d12_blend_factor(ba.src_alpha_blend_factor);
	blendDesc.RenderTarget[0].DestBlendAlpha =
	    spudgpu_d3d12_blend_factor(ba.dst_alpha_blend_factor);
	blendDesc.RenderTarget[0].BlendOpAlpha =
	    (D3D12_BLEND_OP)(ba.alpha_blend_op + 1);
	blendDesc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
	blendDesc.RenderTarget[0].RenderTargetWriteMask =
	    D3D12_COLOR_WRITE_ENABLE_ALL;

	D3D12_RT_FORMAT_ARRAY rtvFormats = {};
	rtvFormats.NumRenderTargets = 1;
	rtvFormats.RTFormats[0] =
	    spudgpu_d3d12_get_dxgi_format(desc->color_attachment_format);

	spudgpu_d3d12_mesh_pipeline_stream stream = {};
	stream.pRootSignature = pResult->_d3d_root_signature.Get();
	if (desc->task_module) {
		stream.AS = D3D12_SHADER_BYTECODE{
		    desc->task_module->_d3d_blob->GetBufferPointer(),
		    desc->task_module->_d3d_blob->GetBufferSize()};
	}
	stream.MS = D3D12_SHADER_BYTECODE{
	    desc->mesh_module->_d3d_blob->GetBufferPointer(),
	    desc->mesh_module->_d3d_blob->GetBufferSize()};
	if (desc->fragment_module) {
		stream.PS = D3D12_SHADER_BYTECODE{
		    desc->fragment_module->_d3d_blob->GetBufferPointer(),
		    desc->fragment_module->_d3d_blob->GetBufferSize()};
	}
	stream.BlendState        = CD3DX12_BLEND_DESC(blendDesc);
	stream.SampleMask        = UINT_MAX;
	stream.RasterizerState   = CD3DX12_RASTERIZER_DESC(rastDesc);
	CD3DX12_DEPTH_STENCIL_DESC1 meshDsDesc1(dsDesc);
	meshDsDesc1.DepthBoundsTestEnable = desc->depth_bounds_test_enable ? TRUE : FALSE;
	stream.DepthStencilState = meshDsDesc1;
	stream.DSVFormat         = (desc->depth_format != SPUDGPU_FORMAT_UNKNOWN)
	                               ? spudgpu_d3d12_get_dxgi_format(desc->depth_format)
	                               : DXGI_FORMAT_UNKNOWN;
	stream.RTVFormats             = rtvFormats;
	stream.SampleDesc             = DXGI_SAMPLE_DESC{1, 0};
	stream.PrimitiveTopologyType =
	    spudgpu_d3d12_primitive_topology_type(desc->primitive_topology);

	D3D12_PIPELINE_STATE_STREAM_DESC streamDesc = {};
	streamDesc.SizeInBytes                  = sizeof(stream);
	streamDesc.pPipelineStateSubobjectStream = &stream;

	HRESULT hr = device->_d3d_device->CreatePipelineState(
	    &streamDesc, IID_PPV_ARGS(&pResult->_d3d_pipeline_state));
	if (FAILED(hr)) {
		pResult->~spudgpu_shader_pipeline_d3d12();
		free(pResult);
		return SPUDRESULT_API_SPECIFIC_FAILURE;
	}

	// Unused by mesh draws (spudgpu_cmd_bind_pipeline skips
	// IASetPrimitiveTopology when _is_mesh_pipeline is set - a mesh
	// pipeline's output topology comes from the mesh shader's own
	// [outputtopology(...)] attribute, not the input assembler) - left at
	// its zero-initialized value.

#if _DEBUG
	if (spud_debug_name_set(pResult, desc->debug_name) != SPUD_SUCCESS) {
		spudgpu_destroy_shader_pipeline(pResult);
		return SPUDRESULT_OUT_OF_MEMORY;
	}
	pResult->_desc.debug_name = pResult->_debug_name;
#endif

	*out_pipeline = pResult;
	return SPUD_SUCCESS;
}

extern "C" {

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

	auto *pDev = (spudgpu_device_d3d12 *)device;

	const uint32_t *spirv_words = (const uint32_t *)desc->spirv_code;
	size_t word_count           = desc->spirv_size / sizeof(uint32_t);

	spirv_cross::CompilerHLSL hlsl_compiler(spirv_words, word_count);

	spirv_cross::CompilerHLSL::Options opts;
	// Mesh/task shaders need SM 6.5+ (DXC's ms_6_5/as_6_5 profiles below
	// reject anything lower) - every other stage keeps the project's usual
	// 6.0 baseline.
	opts.shader_model =
	    (desc->stage == SPUDGPU_SHADER_STAGE_MESH || desc->stage == SPUDGPU_SHADER_STAGE_TASK) ? 65 : 60;
	hlsl_compiler.set_hlsl_options(opts);

	// SpudGPU's clip space is Vulkan's (the reference backend, and SPIR-V's
	// native convention): +Y points down the screen. D3D12's points up, so
	// without this the same shader renders vertically mirrored here. Flipping
	// gl_Position.y makes the final image - and so front-face winding -
	// identical to Vulkan's.
	spirv_cross::CompilerGLSL::Options common_opts = hlsl_compiler.get_common_options();
	common_opts.vertex.flip_vert_y                  = true;
	hlsl_compiler.set_common_options(common_opts);

	// Pin push constants to b0, space SPUDGPU_MAX_DESCRIPTOR_SET_LAYOUTS so
	// they can't collide with descriptor set bindings (which occupy
	// space0..space(set_count-1), and set_count can be as high as
	// SPUDGPU_MAX_DESCRIPTOR_SET_LAYOUTS itself) -- must match
	// spudgpu_d3d12_build_root_signature's own RegisterSpace exactly, see its
	// comment for why a plain space1 isn't safe.
	auto rcVec = std::vector<spirv_cross::RootConstants>();
	spirv_cross::RootConstants rc;
	rc.start   = 0;
	rc.end     = 0xFFFFFFFFu;
	rc.binding = 0;
	rc.space   = SPUDGPU_MAX_DESCRIPTOR_SET_LAYOUTS;
	rcVec.push_back(rc);
	hlsl_compiler.set_root_constant_layouts(rcVec);

	std::string hlsl_src = hlsl_compiler.compile();

	const wchar_t *profile = stage_to_dxc_profile(desc->stage);
	if (!profile)
		return SPUDRESULT_GPU_INVALID_SHADER_STAGE;

	Microsoft::WRL::ComPtr<IDxcLibrary> dxc_lib;
	if (FAILED(DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&dxc_lib))))
		return SPUDRESULT_API_SPECIFIC_FAILURE;

	Microsoft::WRL::ComPtr<IDxcCompiler> dxc_compiler;
	if (FAILED(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxc_compiler))))
		return SPUDRESULT_API_SPECIFIC_FAILURE;

	Microsoft::WRL::ComPtr<IDxcBlobEncoding> src_blob;
	if (FAILED(dxc_lib->CreateBlobWithEncodingFromPinned(
	        hlsl_src.c_str(), (UINT32)hlsl_src.size(), CP_UTF8, &src_blob)))
		return SPUDRESULT_API_SPECIFIC_FAILURE;

	LPCWSTR args[] = {L"-Zpr"};

	Microsoft::WRL::ComPtr<IDxcOperationResult> result;
	HRESULT hr = dxc_compiler->Compile(
	    src_blob.Get(), L"shader", L"main", profile, args, ARRAYSIZE(args),
	    nullptr, 0, nullptr, &result);

	if (SUCCEEDED(hr))
		result->GetStatus(&hr);
	if (FAILED(hr)) {
		// Same issue as D3D12SerializeRootSignature above: DXC reports the
		// actual reason through GetErrorBuffer, not the D3D12 debug layer,
		// so without printing it a compile failure here is silent.
		Microsoft::WRL::ComPtr<IDxcBlobEncoding> errors;
		// result is null when Compile itself failed, as opposed to the
		// compile it ran reporting errors.
		if (result && SUCCEEDED(result->GetErrorBuffer(&errors)) && errors && errors->GetBufferSize())
			printf("spudgpu: DXC compile failed (%ls): %s\n", profile,
			    (const char *)errors->GetBufferPointer());
		else
			printf("spudgpu: DXC compile failed (%ls): hr=0x%08lx\n", profile, (unsigned long)hr);
		return SPUDRESULT_GPU_SHADER_COMPILATION_FAILED;
	}

	// The blob is fetched and converted before the module is allocated, so
	// a module that exists always has a _d3d_blob.
	Microsoft::WRL::ComPtr<IDxcBlob> dxil_blob;
	if (FAILED(result->GetResult(&dxil_blob)))
		return SPUDRESULT_GPU_SHADER_COMPILATION_FAILED;
	if (!dxil_blob)
		return SPUDRESULT_GPU_SHADER_COMPILATION_FAILED;
	Microsoft::WRL::ComPtr<ID3DBlob> d3d_blob;
	if (FAILED(dxil_blob.As(&d3d_blob)))
		return SPUDRESULT_API_SPECIFIC_FAILURE;

	spudgpu_shader_module_d3d12 *pResult =
	    (spudgpu_shader_module_d3d12 *)malloc(
	        sizeof(spudgpu_shader_module_d3d12));
	if (!pResult)
		return SPUDRESULT_OUT_OF_MEMORY;
	pResult = new (pResult) spudgpu_shader_module_d3d12();

	pResult->_desc   = *desc;
	pResult->_device = pDev;
	pResult->_d3d_blob = d3d_blob;

#if _DEBUG
	if (spud_debug_name_set(pResult, desc->debug_name) != SPUD_SUCCESS) {
		spudgpu_destroy_shader_module(pResult);
		return SPUDRESULT_OUT_OF_MEMORY;
	}
	pResult->_desc.debug_name = pResult->_debug_name;
#endif

	*out_module = (spudgpu_shader_module)pResult;
	return SPUD_SUCCESS;
}

void spudgpu_destroy_shader_module(spudgpu_shader_module shader_module) {
	if (!shader_module)
		return;
#if _DEBUG
	free((void *)shader_module->_debug_name);
#endif
	shader_module->~spudgpu_shader_module_d3d12();
	free(shader_module);
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

	// A fragment shader plus either a vertex module or a mesh module is
	// required; the two are mutually exclusive.
	if ((!desc->vertex_module && !desc->mesh_module) || !desc->fragment_module)
		return SPUDRESULT_GPU_VERTEX_AND_FRAGMENT_SHADER_REQUIRED;
	if (desc->vertex_module && desc->mesh_module)
		return SPUDRESULT_GPU_INVALID_SHADER_STAGE;

	spudgpu_shader_pipeline_d3d12 *pResult =
	    (spudgpu_shader_pipeline_d3d12 *)malloc(
	        sizeof(spudgpu_shader_pipeline_d3d12));
	if (!pResult)
		return SPUDRESULT_OUT_OF_MEMORY;
	pResult                   = new (pResult) spudgpu_shader_pipeline_d3d12();
	pResult->_device          = device;
	pResult->_desc            = *desc;
	pResult->_is_mesh_pipeline = desc->mesh_module != nullptr;

	// Root signature. A mesh pipeline has no input-assembler stage at all,
	// so D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT must
	// not be set for it - D3D12 validation rejects that combination.
	HRESULT hr = spudgpu_d3d12_build_root_signature(
	    device->_d3d_device.Get(), desc->descriptor_set_layout_count,
	    (void *const *)desc->descriptor_set_layouts,
	    desc->push_constant_range_count, desc->push_constant_ranges,
	    pResult->_is_mesh_pipeline
	        ? D3D12_ROOT_SIGNATURE_FLAG_NONE
	        : D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT,
	    &pResult->_d3d_root_signature);
	if (FAILED(hr)) {
		pResult->~spudgpu_shader_pipeline_d3d12();
		free(pResult);
		return SPUDRESULT_API_SPECIFIC_FAILURE;
	}

	if (pResult->_is_mesh_pipeline) {
		return spudgpu_d3d12___create_mesh_shader_pipeline(device, desc, pResult, out_pipeline);
	}

	// Vertex input layout (SPIRV-Cross HLSL emits TEXCOORD<location> semantics)
	D3D12_INPUT_ELEMENT_DESC inputElements[SPUDGPU_MAX_VERTEX_ATTRIBUTES];
	for (uint32_t i = 0; i < desc->vertex_attribute_count; ++i) {
		const auto &attr = desc->vertex_attributes[i];
		D3D12_INPUT_CLASSIFICATION cls =
		    D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
		UINT stepRate = 0;
		for (uint32_t b = 0; b < desc->vertex_binding_count; ++b) {
			if (desc->vertex_bindings[b].binding == attr.binding) {
				if (desc->vertex_bindings[b].per_instance) {
					cls      = D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;
					stepRate = 1;
				}
				break;
			}
		}
		inputElements[i].SemanticName  = "TEXCOORD";
		inputElements[i].SemanticIndex = attr.location;
		inputElements[i].Format    = spudgpu_d3d12_get_dxgi_format(attr.format);
		inputElements[i].InputSlot = attr.binding;
		inputElements[i].AlignedByteOffset    = attr.offset;
		inputElements[i].InputSlotClass       = cls;
		inputElements[i].InstanceDataStepRate = stepRate;
	}

	// Rasterizer
	D3D12_RASTERIZER_DESC rastDesc = {};
	rastDesc.FillMode =
	    desc->wireframe ? D3D12_FILL_MODE_WIREFRAME : D3D12_FILL_MODE_SOLID;
	rastDesc.CullMode              = (D3D12_CULL_MODE)(desc->cull_mode + 1);
	rastDesc.FrontCounterClockwise = desc->front_face_ccw ? TRUE : FALSE;
	rastDesc.DepthBias             = D3D12_DEFAULT_DEPTH_BIAS;
	rastDesc.DepthBiasClamp        = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	rastDesc.SlopeScaledDepthBias  = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	rastDesc.DepthClipEnable       = TRUE;
	rastDesc.MultisampleEnable     = FALSE;
	rastDesc.AntialiasedLineEnable = FALSE;
	rastDesc.ForcedSampleCount     = 0;
	rastDesc.ConservativeRaster    = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	// Depth/stencil  (SPUDGPU compare op + 1 == D3D12_COMPARISON_FUNC)
	D3D12_DEPTH_STENCILOP_DESC noOp = {
	    D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP,
	    D3D12_COMPARISON_FUNC_ALWAYS};
	D3D12_DEPTH_STENCIL_DESC dsDesc = {};
	dsDesc.DepthEnable              = desc->depth_test_enable ? TRUE : FALSE;
	dsDesc.DepthWriteMask           = desc->depth_write_enable
	                                      ? D3D12_DEPTH_WRITE_MASK_ALL
	                                      : D3D12_DEPTH_WRITE_MASK_ZERO;
	dsDesc.DepthFunc     = (D3D12_COMPARISON_FUNC)(desc->depth_compare_op + 1);
	dsDesc.StencilEnable = FALSE;
	dsDesc.StencilReadMask  = D3D12_DEFAULT_STENCIL_READ_MASK;
	dsDesc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
	dsDesc.FrontFace        = noOp;
	dsDesc.BackFace         = noOp;

	// Blend  (SPUDGPU blend op + 1 == D3D12_BLEND_OP)
	const auto &ba                          = desc->blend_attachment;
	D3D12_BLEND_DESC blendDesc              = {};
	blendDesc.AlphaToCoverageEnable         = FALSE;
	blendDesc.IndependentBlendEnable        = FALSE;
	blendDesc.RenderTarget[0].BlendEnable   = ba.blend_enable ? TRUE : FALSE;
	blendDesc.RenderTarget[0].LogicOpEnable = FALSE;
	blendDesc.RenderTarget[0].SrcBlend =
	    spudgpu_d3d12_blend_factor(ba.src_color_blend_factor);
	blendDesc.RenderTarget[0].DestBlend =
	    spudgpu_d3d12_blend_factor(ba.dst_color_blend_factor);
	blendDesc.RenderTarget[0].BlendOp = (D3D12_BLEND_OP)(ba.color_blend_op + 1);
	blendDesc.RenderTarget[0].SrcBlendAlpha =
	    spudgpu_d3d12_blend_factor(ba.src_alpha_blend_factor);
	blendDesc.RenderTarget[0].DestBlendAlpha =
	    spudgpu_d3d12_blend_factor(ba.dst_alpha_blend_factor);
	blendDesc.RenderTarget[0].BlendOpAlpha =
	    (D3D12_BLEND_OP)(ba.alpha_blend_op + 1);
	blendDesc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
	blendDesc.RenderTarget[0].RenderTargetWriteMask =
	    D3D12_COLOR_WRITE_ENABLE_ALL;

	D3D_PRIMITIVE_TOPOLOGY topology = spudgpu_d3d12_primitive_topology(
	    desc->primitive_topology, desc->patch_control_points);

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.pRootSignature = pResult->_d3d_root_signature.Get();
	if (desc->vertex_module) {
		psoDesc.VS = {
		    desc->vertex_module->_d3d_blob->GetBufferPointer(),
		    desc->vertex_module->_d3d_blob->GetBufferSize()};
	}
	if (desc->fragment_module) {
		psoDesc.PS = {
		    desc->fragment_module->_d3d_blob->GetBufferPointer(),
		    desc->fragment_module->_d3d_blob->GetBufferSize()};
	}
	if (desc->geometry_module) {
		psoDesc.GS = {
		    desc->geometry_module->_d3d_blob->GetBufferPointer(),
		    desc->geometry_module->_d3d_blob->GetBufferSize()};
	}
	if (desc->tess_control_module) {
		psoDesc.HS = {
		    desc->tess_control_module->_d3d_blob->GetBufferPointer(),
		    desc->tess_control_module->_d3d_blob->GetBufferSize()};
	}
	if (desc->tess_eval_module) {
		psoDesc.DS = {
		    desc->tess_eval_module->_d3d_blob->GetBufferPointer(),
		    desc->tess_eval_module->_d3d_blob->GetBufferSize()};
	}
	psoDesc.StreamOutput      = {};
	psoDesc.BlendState        = blendDesc;
	psoDesc.SampleMask        = UINT_MAX;
	psoDesc.RasterizerState   = rastDesc;
	psoDesc.DepthStencilState = dsDesc;
	psoDesc.InputLayout       = {inputElements, desc->vertex_attribute_count};
	psoDesc.IBStripCutValue   = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
	psoDesc.PrimitiveTopologyType =
	    spudgpu_d3d12_primitive_topology_type(desc->primitive_topology);
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] =
	    spudgpu_d3d12_get_dxgi_format(desc->color_attachment_format);
	psoDesc.DSVFormat  = (desc->depth_format != SPUDGPU_FORMAT_UNKNOWN)
	                         ? spudgpu_d3d12_get_dxgi_format(desc->depth_format)
	                         : DXGI_FORMAT_UNKNOWN;
	psoDesc.SampleDesc = {1, 0};
	psoDesc.NodeMask   = 0;
	psoDesc.Flags      = D3D12_PIPELINE_STATE_FLAG_NONE;

	// CreateGraphicsPipelineState's D3D12_GRAPHICS_PIPELINE_STATE_DESC only
	// carries a D3D12_DEPTH_STENCIL_DESC (no DepthBoundsTestEnable - that
	// field only exists on D3D12_DEPTH_STENCIL_DESC1). CD3DX12_PIPELINE_
	// STATE_STREAM1 round-trips psoDesc losslessly (see its
	// D3D12_GRAPHICS_PIPELINE_STATE_DESC constructor in d3dx12.h) while
	// upgrading DepthStencilState to the "1" variant, so every pipeline
	// created here goes through CreatePipelineState instead - not just the
	// ones that ask for depth_bounds_test_enable - to avoid two divergent
	// creation paths for what should behave identically otherwise.
	CD3DX12_PIPELINE_STATE_STREAM1 pipelineStateStream(psoDesc);
	CD3DX12_DEPTH_STENCIL_DESC1 dsDesc1(dsDesc);
	dsDesc1.DepthBoundsTestEnable = desc->depth_bounds_test_enable ? TRUE : FALSE;
	pipelineStateStream.DepthStencilState = dsDesc1;

	D3D12_PIPELINE_STATE_STREAM_DESC streamDesc = {};
	streamDesc.SizeInBytes                   = sizeof(pipelineStateStream);
	streamDesc.pPipelineStateSubobjectStream = &pipelineStateStream;

	hr = device->_d3d_device->CreatePipelineState(
	    &streamDesc, IID_PPV_ARGS(&pResult->_d3d_pipeline_state));
	if (FAILED(hr)) {
		pResult->~spudgpu_shader_pipeline_d3d12();
		free(pResult);
		return SPUDRESULT_API_SPECIFIC_FAILURE;
	}

	pResult->_d3d_primitive_topology = topology;

#if _DEBUG
	if (spud_debug_name_set(pResult, desc->debug_name) != SPUD_SUCCESS) {
		spudgpu_destroy_shader_pipeline(pResult);
		return SPUDRESULT_OUT_OF_MEMORY;
	}
	pResult->_desc.debug_name = pResult->_debug_name;
#endif

	*out_pipeline = pResult;
	return SPUD_SUCCESS;
}

void spudgpu_destroy_shader_pipeline(spudgpu_shader_pipeline pipeline) {
	if (!pipeline)
		return;
#if _DEBUG
	free((void *)pipeline->_debug_name);
#endif
	pipeline->~spudgpu_shader_pipeline_d3d12();
	free(pipeline);
}

SPUDRESULT spudgpu_get_shader_pipeline_desc(
    spudgpu_shader_pipeline pipeline, spudgpu_shader_pipeline_desc *out_desc) {
	if (!pipeline)
		return SPUDRESULT_GPU_INVALID_SHADER_PIPELINE;
	if (!out_desc)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	*out_desc = pipeline->_desc;
	return SPUD_SUCCESS;
}

void spudgpu_cmd_bind_pipeline(
    spudgpu_command_list cmd, spudgpu_shader_pipeline pipeline) {
	if (!cmd || !pipeline)
		return;
	ID3D12GraphicsCommandList *cmdList = cmd->_d3d_cmd_list.Get();
	cmdList->SetPipelineState(pipeline->_d3d_pipeline_state.Get());
	cmdList->SetGraphicsRootSignature(pipeline->_d3d_root_signature.Get());
	// A mesh pipeline has no input-assembler stage - its output topology
	// comes from the mesh shader's own [outputtopology(...)] attribute, not
	// IASetPrimitiveTopology, and _d3d_primitive_topology was never set to
	// anything meaningful for it (see spudgpu_d3d12___create_mesh_shader_pipeline).
	if (!pipeline->_is_mesh_pipeline)
		cmdList->IASetPrimitiveTopology(pipeline->_d3d_primitive_topology);
}

void spudgpu_cmd_bind_compute_pipeline(
    spudgpu_command_list cmd, spudgpu_compute_pipeline pipeline) {
	if (!cmd || !pipeline)
		return;
	ID3D12GraphicsCommandList *cmdList = cmd->_d3d_cmd_list.Get();
	cmdList->SetPipelineState(pipeline->_d3d_pipeline_state.Get());
	cmdList->SetComputeRootSignature(pipeline->_d3d_root_signature.Get());
}

void spudgpu_cmd_push_constants(
    spudgpu_command_list cmd,
    spudgpu_shader_pipeline pipeline,
    uint32_t offset,
    uint32_t size,
    const void *data) {
	if (!cmd || !pipeline || !data || !size)
		return;
	if (!pipeline->_desc.push_constant_range_count)
		return;
	// Push constants are at root param index = descriptor_set_layout_count.
	// [data] is the bytes to write, starting at its first byte; [offset] is
	// only where in the block they go.
	uint32_t rootIdx  = pipeline->_desc.descriptor_set_layout_count;
	uint32_t offset32 = offset / 4;
	uint32_t count32  = (size + 3) / 4;
	cmd->_d3d_cmd_list->SetGraphicsRoot32BitConstants(
	    rootIdx, count32, data, offset32);
}

void spudgpu_cmd_push_constants_compute(
    spudgpu_command_list cmd,
    spudgpu_compute_pipeline pipeline,
    uint32_t offset,
    uint32_t size,
    const void *data) {
	if (!cmd || !pipeline || !data || !size)
		return;
	if (!pipeline->_desc.push_constant_range_count)
		return;
	// The compute root signature is built by the same
	// spudgpu_d3d12_build_root_signature as a graphics one, so the constants
	// are at the same place: root param index = descriptor_set_layout_count.
	uint32_t rootIdx  = pipeline->_desc.descriptor_set_layout_count;
	uint32_t offset32 = offset / 4;
	uint32_t count32  = (size + 3) / 4;
	cmd->_d3d_cmd_list->SetComputeRoot32BitConstants(
	    rootIdx, count32, data, offset32);
}

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

	spudgpu_compute_pipeline_d3d12 *pResult =
	    (spudgpu_compute_pipeline_d3d12 *)malloc(
	        sizeof(spudgpu_compute_pipeline_d3d12));
	if (!pResult)
		return SPUDRESULT_OUT_OF_MEMORY;
	pResult          = new (pResult) spudgpu_compute_pipeline_d3d12();
	pResult->_device = device;
	pResult->_desc   = *desc;

	HRESULT hr = spudgpu_d3d12_build_root_signature(
	    device->_d3d_device.Get(), desc->descriptor_set_layout_count,
	    (void *const *)desc->descriptor_set_layouts,
	    desc->push_constant_range_count, desc->push_constant_ranges,
	    D3D12_ROOT_SIGNATURE_FLAG_NONE, &pResult->_d3d_root_signature);
	if (FAILED(hr)) {
		pResult->~spudgpu_compute_pipeline_d3d12();
		free(pResult);
		return SPUDRESULT_API_SPECIFIC_FAILURE;
	}

	D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.pRootSignature = pResult->_d3d_root_signature.Get();
	if (desc->compute_module) {
		psoDesc.CS = {
		    desc->compute_module->_d3d_blob->GetBufferPointer(),
		    desc->compute_module->_d3d_blob->GetBufferSize()};
	}
	psoDesc.NodeMask = 0;
	psoDesc.Flags    = D3D12_PIPELINE_STATE_FLAG_NONE;

	hr = device->_d3d_device->CreateComputePipelineState(
	    &psoDesc, IID_PPV_ARGS(&pResult->_d3d_pipeline_state));
	if (FAILED(hr)) {
		pResult->~spudgpu_compute_pipeline_d3d12();
		free(pResult);
		return SPUDRESULT_API_SPECIFIC_FAILURE;
	}

#if _DEBUG
	if (spud_debug_name_set(pResult, desc->debug_name) != SPUD_SUCCESS) {
		spudgpu_destroy_compute_pipeline(pResult);
		return SPUDRESULT_OUT_OF_MEMORY;
	}
	pResult->_desc.debug_name = pResult->_debug_name;
#endif

	*out_pipeline = pResult;
	return SPUD_SUCCESS;
}

void spudgpu_destroy_compute_pipeline(spudgpu_compute_pipeline pipeline) {
	if (!pipeline)
		return;
#if _DEBUG
	free((void *)pipeline->_debug_name);
#endif
	pipeline->~spudgpu_compute_pipeline_d3d12();
	free(pipeline);
}

SPUDRESULT spudgpu_get_compute_pipeline_desc(
    spudgpu_compute_pipeline pipeline,
    spudgpu_compute_pipeline_desc *out_desc) {
	if (!pipeline)
		return SPUDRESULT_GPU_INVALID_COMPUTE_PIPELINE;
	if (!out_desc)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	*out_desc = pipeline->_desc;
	return SPUD_SUCCESS;
}

} // extern "C"

#endif // SPUDGPU_COMPILE_D3D12_API
