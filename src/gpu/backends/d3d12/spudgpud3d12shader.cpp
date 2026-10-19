
#if SPUDGPU_COMPILE_D3D12_API

#include "spudgpu_d3d12_natives.h"
#include "spudgpud3d12.hpp"
#include "../../spudgpushared.h"

#include <dxcapi.h>
#include <spirv_hlsl.hpp>
#include "../../../spudstructbase.h"

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

// The blend state of [desc]: a render target for each colour attachment.
// IndependentBlendEnable only when they differ, which every D3D12 device
// allows (SPUDGPU_DEVICE_PROPERTIES::independent_blend).
static D3D12_BLEND_DESC spudgpu_d3d12___blend_desc(const spudgpu_shader_pipeline_desc *desc) {
	D3D12_BLEND_DESC blendDesc       = {};
	blendDesc.AlphaToCoverageEnable  = FALSE;
	blendDesc.IndependentBlendEnable = FALSE;
	for (uint32_t i = 1; i < desc->color_attachment_count; ++i)
		if (!spudgpu___internal_blend_attachments_equal(&desc->blend_attachments[0], &desc->blend_attachments[i]))
			blendDesc.IndependentBlendEnable = TRUE;
	// With none, target 0 still has a valid state, unused.
	const uint32_t count = desc->color_attachment_count ? desc->color_attachment_count : 1;
	for (uint32_t i = 0; i < count; ++i) {
		// SPUDGPU blend op + 1 == D3D12_BLEND_OP.
		const spudgpu_blend_attachment_desc &ba = desc->blend_attachments[i];
		D3D12_RENDER_TARGET_BLEND_DESC &rt      = blendDesc.RenderTarget[i];
		rt.BlendEnable                          = ba.blend_enable ? TRUE : FALSE;
		rt.LogicOpEnable                        = FALSE;
		rt.SrcBlend                             = spudgpu_d3d12_blend_factor(ba.src_color_blend_factor);
		rt.DestBlend                            = spudgpu_d3d12_blend_factor(ba.dst_color_blend_factor);
		rt.BlendOp                              = (D3D12_BLEND_OP)(ba.color_blend_op + 1);
		rt.SrcBlendAlpha                        = spudgpu_d3d12_blend_factor(ba.src_alpha_blend_factor);
		rt.DestBlendAlpha                       = spudgpu_d3d12_blend_factor(ba.dst_alpha_blend_factor);
		rt.BlendOpAlpha                         = (D3D12_BLEND_OP)(ba.alpha_blend_op + 1);
		rt.LogicOp                              = D3D12_LOGIC_OP_NOOP;
		rt.RenderTargetWriteMask                = D3D12_COLOR_WRITE_ENABLE_ALL;
	}
	return blendDesc;
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
// constant ranges, and says in [out_layout] where each set's tables and the
// push constants were put. Push constants are mapped to b0 in a space past
// every set's (see below).
//
// A descriptor table reads from one descriptor heap, and CBV/SRV/UAV
// descriptors and samplers live in heaps of different types, so a set gets
// one table per heap it uses: CBV/SRV/UAV first, then samplers, in set
// order. A COMBINED_IMAGE_SAMPLER binding is in both: its SRV range and its
// sampler range have the same register number (SPIRV-Cross emits t<n> and
// s<n> for it). A set with no descriptors gets no table.
static HRESULT spudgpu_d3d12_build_root_signature(
    ID3D12Device *device,
    uint32_t set_count,
    void *const *set_layouts,
    uint32_t pc_range_count,
    const spudgpu_push_constant_range_desc *pc_ranges,
    D3D12_ROOT_SIGNATURE_FLAGS flags,
    ID3D12RootSignature **out_sig,
    spudgpu_d3d12_root_layout *out_layout) {
	if (set_count > SPUDGPU_MAX_DESCRIPTOR_SET_LAYOUTS)
		return E_INVALIDARG;

	D3D12_DESCRIPTOR_RANGE
	csv_ranges[SPUDGPU_MAX_DESCRIPTOR_SET_LAYOUTS]
	          [SPUDGPU_MAX_DESCRIPTOR_BINDINGS_PER_SET];
	D3D12_DESCRIPTOR_RANGE
	sampler_ranges[SPUDGPU_MAX_DESCRIPTOR_SET_LAYOUTS]
	              [SPUDGPU_MAX_DESCRIPTOR_BINDINGS_PER_SET];
	uint32_t csv_counts[SPUDGPU_MAX_DESCRIPTOR_SET_LAYOUTS]     = {};
	uint32_t sampler_counts[SPUDGPU_MAX_DESCRIPTOR_SET_LAYOUTS] = {};

	for (uint32_t s = 0; s < set_count; ++s) {
		auto *layout = (spudgpu_descriptor_set_layout_d3d12 *)set_layouts[s];
		if (!layout)
			continue;
		for (uint32_t b = 0; b < layout->_desc.binding_count; ++b) {
			const auto &bind = layout->_desc.bindings[b];
			bool has_csv     = true;
			bool has_sampler = false;
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
				rt = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
				break;
			case SPUDGPU_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
				rt          = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
				has_sampler = true;
				break;
			case SPUDGPU_DESCRIPTOR_TYPE_SAMPLER:
				rt          = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
				has_csv     = false;
				has_sampler = true;
				break;
			default:
				continue;
			}

			// Ranges are appended in binding order, the same order
			// spudgpu_create_descriptor_set_layout lays out each heap's
			// slots in, so OFFSET_APPEND lands on the same slots.
			D3D12_DESCRIPTOR_RANGE range;
			range.NumDescriptors                    = bind.count ? bind.count : 1;
			range.BaseShaderRegister                = bind.binding;
			range.RegisterSpace                     = s;
			range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
			if (has_csv) {
				range.RangeType                = rt;
				csv_ranges[s][csv_counts[s]++] = range;
			}
			if (has_sampler) {
				range.RangeType                        = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
				sampler_ranges[s][sampler_counts[s]++] = range;
			}
		}
	}

	// At most two tables per set, and the push constants.
	D3D12_ROOT_PARAMETER params[2 * SPUDGPU_MAX_DESCRIPTOR_SET_LAYOUTS + 1];
	uint32_t param_count = 0;
	spudgpu_d3d12_root_layout root_layout;
	for (uint32_t s = 0; s < SPUDGPU_MAX_DESCRIPTOR_SET_LAYOUTS; ++s) {
		root_layout.cbv_srv_uav_param[s] = SPUDGPU_D3D12_NO_ROOT_PARAMETER;
		root_layout.sampler_param[s]     = SPUDGPU_D3D12_NO_ROOT_PARAMETER;
	}
	root_layout.push_constant_param = SPUDGPU_D3D12_NO_ROOT_PARAMETER;

	for (uint32_t s = 0; s < set_count; ++s) {
		if (csv_counts[s]) {
			root_layout.cbv_srv_uav_param[s]                        = param_count;
			params[param_count].ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			params[param_count].DescriptorTable.NumDescriptorRanges = csv_counts[s];
			params[param_count].DescriptorTable.pDescriptorRanges   = csv_ranges[s];
			params[param_count].ShaderVisibility                    = D3D12_SHADER_VISIBILITY_ALL;
			++param_count;
		}
		if (sampler_counts[s]) {
			root_layout.sampler_param[s]                            = param_count;
			params[param_count].ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			params[param_count].DescriptorTable.NumDescriptorRanges = sampler_counts[s];
			params[param_count].DescriptorTable.pDescriptorRanges   = sampler_ranges[s];
			params[param_count].ShaderVisibility                    = D3D12_SHADER_VISIBILITY_ALL;
			++param_count;
		}
	}

	uint32_t total_pc_words = 0;
	for (uint32_t p = 0; p < pc_range_count; ++p) {
		uint32_t end = (pc_ranges[p].offset + pc_ranges[p].size + 3) / 4;
		if (end > total_pc_words)
			total_pc_words = end;
	}

	if (total_pc_words > 0) {
		root_layout.push_constant_param = param_count;
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

	hr = device->CreateRootSignature(
	    0, sig_blob->GetBufferPointer(), sig_blob->GetBufferSize(),
	    IID_PPV_ARGS(out_sig));
	if (SUCCEEDED(hr))
		*out_layout = root_layout;
	return hr;
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
	rastDesc.DepthBias             = desc->depth_bias_constant;
	rastDesc.DepthBiasClamp        = desc->depth_bias_clamp;
	rastDesc.SlopeScaledDepthBias  = desc->depth_bias_slope;
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

	// Blend, one target for each colour attachment.
	const D3D12_BLEND_DESC blendDesc = spudgpu_d3d12___blend_desc(desc);

	D3D12_RT_FORMAT_ARRAY rtvFormats = {};
	rtvFormats.NumRenderTargets = desc->color_attachment_count;
	for (uint32_t i = 0; i < desc->color_attachment_count; ++i)
		rtvFormats.RTFormats[i] = spudgpu_d3d12_get_dxgi_format(desc->color_attachment_formats[i]);

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

	if (spud_debug_name_set(pResult, desc->debug_name) != SPUD_SUCCESS) {
		spudgpu_destroy_shader_pipeline(pResult);
		return SPUDRESULT_OUT_OF_MEMORY;
	}
	pResult->_desc.debug_name = pResult->_debug_name;

	*out_pipeline = pResult;
	return SPUD_SUCCESS;
}

// HLSL has no RWTextureCube: a storage image of a cube, or of a cube array, is
// an RWTexture2DArray there, with the face (or cube * 6 + face) as its array
// index, which is the third coordinate a SPIR-V store to a cube image already
// takes. The storage view it is bound to is a 2D array of the same layers
// (spudgpud3d12image.cpp). Returns a malloc'd copy of [words] with every
// storage cube image type made a 2D array image type, NULL with
// [out_needed] false if there is none, or NULL with [out_needed] true when
// the copy can't be allocated. An imageSize() of a cube, a 2-component
// result, takes the width and height of the array's three.
static uint32_t *spudgpu_d3d12_storage_cubes_as_arrays(const uint32_t *words, size_t word_count, bool *out_needed) {
	// The header, then instructions: word 0 is the word count << 16 | opcode.
	// OpTypeImage (25) is: result, sampled type, Dim, Depth, Arrayed, MS,
	// Sampled, Format.
	const uint32_t op_type_image = 25;
	const uint32_t dim_2d        = 1;
	const uint32_t dim_cube      = 3;
	const uint32_t sampled_storage = 2;
	*out_needed = false;
	uint32_t *copy = nullptr;
	size_t i       = 5;
	while (i < word_count) {
		const uint32_t count  = words[i] >> 16;
		const uint32_t opcode = words[i] & 0xFFFFu;
		if (count == 0 || count > word_count - i)
			break; // Malformed: left to SPIRV-Cross to report.
		if (opcode == op_type_image && count >= 9 && words[i + 3] == dim_cube && words[i + 7] == sampled_storage) {
			if (!copy) {
				*out_needed = true;
				copy        = (uint32_t *)malloc(word_count * sizeof(uint32_t));
				if (!copy)
					return nullptr;
				memcpy(copy, words, word_count * sizeof(uint32_t));
			}
			copy[i + 3] = dim_2d;
			copy[i + 5] = 1;
		}
		i += count;
	}
	return copy;
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
	const SPUDRESULT base_sr = spud_struct_check(&desc->base, SPUD_STRUCT_TYPE_GPU_SHADER_MODULE_DESC);
	if (base_sr != SPUD_SUCCESS)
		return base_sr;
	if (!out_module)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	if (!desc->spirv_code || desc->spirv_size == 0)
		return SPUDRESULT_GPU_NULL_SPIRV;
	if (desc->spirv_size % 4 != 0)
		return SPUDRESULT_GPU_INVALID_SPIRV_ALIGNMENT;

	auto *pDev = (spudgpu_device_d3d12 *)device;

	const uint32_t *spirv_words = (const uint32_t *)desc->spirv_code;
	size_t word_count           = desc->spirv_size / sizeof(uint32_t);

	bool cubes_needed           = false;
	uint32_t *cubes_as_arrays   = spudgpu_d3d12_storage_cubes_as_arrays(spirv_words, word_count, &cubes_needed);
	if (cubes_needed && !cubes_as_arrays)
		return SPUDRESULT_OUT_OF_MEMORY;
	// The compiler parses the words when it is made and keeps none of them.
	spirv_cross::CompilerHLSL hlsl_compiler(cubes_as_arrays ? cubes_as_arrays : spirv_words, word_count);
	free(cubes_as_arrays);

	spirv_cross::CompilerHLSL::Options opts;
	// Mesh/task shaders need SM 6.5+ (DXC's ms_6_5/as_6_5 profiles below
	// reject anything lower) - every other stage keeps the project's usual
	// 6.0 baseline.
	opts.shader_model =
	    (desc->stage == SPUDGPU_SHADER_STAGE_MESH || desc->stage == SPUDGPU_SHADER_STAGE_TASK) ? 65 : 60;
	// A SPUDGPU_DESCRIPTOR_TYPE_STORAGE_BUFFER is a UAV in this backend: its
	// root signature range (spudgpu_d3d12_build_root_signature) and the
	// descriptor written for it are both UAVs. SPIRV-Cross would otherwise
	// declare a GLSL readonly buffer as an SRV (t<n>), which no root
	// signature range covers, and pipeline creation fails.
	opts.force_storage_buffer_as_uav = true;
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

	if (spud_debug_name_set(pResult, desc->debug_name) != SPUD_SUCCESS) {
		spudgpu_destroy_shader_module(pResult);
		return SPUDRESULT_OUT_OF_MEMORY;
	}
	pResult->_desc.debug_name = pResult->_debug_name;

	*out_module = (spudgpu_shader_module)pResult;
	return SPUD_SUCCESS;
}

void spudgpu_destroy_shader_module(spudgpu_shader_module shader_module) {
	if (!shader_module)
		return;
	free((void *)shader_module->_debug_name);
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
	const SPUDRESULT base_sr = spud_struct_check(&desc->base, SPUD_STRUCT_TYPE_GPU_SHADER_PIPELINE_DESC);
	if (base_sr != SPUD_SUCCESS)
		return base_sr;
	if (!out_pipeline)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;

	// A fragment shader plus either a vertex module or a mesh module is
	// required; the two are mutually exclusive.
	if ((!desc->vertex_module && !desc->mesh_module) || !desc->fragment_module)
		return SPUDRESULT_GPU_VERTEX_AND_FRAGMENT_SHADER_REQUIRED;
	if (desc->vertex_module && desc->mesh_module)
		return SPUDRESULT_GPU_INVALID_SHADER_STAGE;
	SPUDRESULT outputs = spudgpu___internal_check_pipeline_outputs(desc, &device->_properties);
	if (outputs != SPUD_SUCCESS)
		return outputs;

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
	    &pResult->_d3d_root_signature, &pResult->_root_layout);
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
	rastDesc.DepthBias             = desc->depth_bias_constant;
	rastDesc.DepthBiasClamp        = desc->depth_bias_clamp;
	rastDesc.SlopeScaledDepthBias  = desc->depth_bias_slope;
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

	// Blend, one target for each colour attachment.
	const D3D12_BLEND_DESC blendDesc = spudgpu_d3d12___blend_desc(desc);

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
	psoDesc.NumRenderTargets = desc->color_attachment_count;
	for (uint32_t i = 0; i < desc->color_attachment_count; ++i)
		psoDesc.RTVFormats[i] = spudgpu_d3d12_get_dxgi_format(desc->color_attachment_formats[i]);
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

	if (spud_debug_name_set(pResult, desc->debug_name) != SPUD_SUCCESS) {
		spudgpu_destroy_shader_pipeline(pResult);
		return SPUDRESULT_OUT_OF_MEMORY;
	}
	pResult->_desc.debug_name = pResult->_debug_name;

	*out_pipeline = pResult;
	return SPUD_SUCCESS;
}

void spudgpu_destroy_shader_pipeline(spudgpu_shader_pipeline pipeline) {
	if (!pipeline)
		return;
	free((void *)pipeline->_debug_name);
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
	out_desc->base = spud_struct_base(SPUD_STRUCT_TYPE_GPU_SHADER_PIPELINE_DESC);
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
	if (pipeline->_root_layout.push_constant_param == SPUDGPU_D3D12_NO_ROOT_PARAMETER)
		return;
	// [data] is the bytes to write, starting at its first byte; [offset] is
	// only where in the block they go.
	uint32_t rootIdx  = pipeline->_root_layout.push_constant_param;
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
	if (pipeline->_root_layout.push_constant_param == SPUDGPU_D3D12_NO_ROOT_PARAMETER)
		return;
	uint32_t rootIdx  = pipeline->_root_layout.push_constant_param;
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
	const SPUDRESULT base_sr = spud_struct_check(&desc->base, SPUD_STRUCT_TYPE_GPU_COMPUTE_PIPELINE_DESC);
	if (base_sr != SPUD_SUCCESS)
		return base_sr;
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
	    D3D12_ROOT_SIGNATURE_FLAG_NONE, &pResult->_d3d_root_signature,
	    &pResult->_root_layout);
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

	if (spud_debug_name_set(pResult, desc->debug_name) != SPUD_SUCCESS) {
		spudgpu_destroy_compute_pipeline(pResult);
		return SPUDRESULT_OUT_OF_MEMORY;
	}
	pResult->_desc.debug_name = pResult->_debug_name;

	*out_pipeline = pResult;
	return SPUD_SUCCESS;
}

void spudgpu_destroy_compute_pipeline(spudgpu_compute_pipeline pipeline) {
	if (!pipeline)
		return;
	free((void *)pipeline->_debug_name);
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
	out_desc->base = spud_struct_base(SPUD_STRUCT_TYPE_GPU_COMPUTE_PIPELINE_DESC);
	return SPUD_SUCCESS;
}

} // extern "C"

#endif // SPUDGPU_COMPILE_D3D12_API
