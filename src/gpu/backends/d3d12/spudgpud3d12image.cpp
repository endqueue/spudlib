
#if SPUDGPU_COMPILE_D3D12_API

#include "spudgpud3d12.hpp"

static SPUDRESULT spudgpu_d3d12_create_resource_desc_from_image(
    const spudgpu_image_desc *desc,
    D3D12_RESOURCE_DESC *out_d3d_resource_desc) {
	if (!desc)
		return SPUDRESULT_NULL_DESC;
	if (!out_d3d_resource_desc)
		return SPUD_SUCCESS;
	D3D12_RESOURCE_FLAGS d3dResourceFlags =
	    spudgpu_d3d12_get_image_resource_flags(desc->usage);
	DXGI_FORMAT dxgiFmt = spudgpu_d3d12_get_dxgi_format(desc->format);
	CD3DX12_RESOURCE_DESC result;
	switch (desc->type) {
	case SPUDGPU_IMAGE_TYPE_1D:
		result = CD3DX12_RESOURCE_DESC::Tex1D(
		    dxgiFmt, desc->width,
		    (UINT16)desc->array_layers, (UINT16)desc->mip_levels,
		    d3dResourceFlags);
		break;
	case SPUDGPU_IMAGE_TYPE_2D:
		result = CD3DX12_RESOURCE_DESC::Tex2D(
		    dxgiFmt, desc->width, desc->height,
		    (UINT16)desc->array_layers, (UINT16)desc->mip_levels,
		    1, 0, d3dResourceFlags);
		break;
	case SPUDGPU_IMAGE_TYPE_3D:
		result = CD3DX12_RESOURCE_DESC::Tex3D(
		    dxgiFmt, desc->width, desc->height, (UINT16)desc->depth,
		    (UINT16)desc->mip_levels, d3dResourceFlags);
		break;
	default:
		return SPUDRESULT_GPU_INVALID_IMAGE_TYPE;
	}
	*out_d3d_resource_desc = result;
	return SPUD_SUCCESS;
}

static D3D12_CLEAR_VALUE
spudgpu_d3d12_get_clear_value(const SPUDGPU_CLEAR_VALUE *c) {
	D3D12_CLEAR_VALUE result = {};
	result.Format            = spudgpu_d3d12_get_dxgi_format(c->format);
	result.DepthStencil.Depth   = c->depth_stencil.depth;
	result.DepthStencil.Stencil = c->depth_stencil.stencil;
	memcpy(&result.Color, &c->color, sizeof(float) * 4);
	return result;
}

extern "C" {

SPUDRESULT spudgpu_create_image(
    spudgpu_device device,
    const spudgpu_image_desc *desc,
    spudgpu_image *out_image) {
	if (!device)
		return SPUDRESULT_GPU_INVALID_DEVICE;
	if (!desc)
		return SPUDRESULT_NULL_DESC;
	if (!out_image)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	if (!desc->width || !desc->height || !desc->depth || !desc->array_layers || !desc->mip_levels)
		return SPUDRESULT_DESC_INVALID_PARAMETERS;
	if (desc->usage == SPUDGPU_IMAGE_USAGE_NONE)
		return SPUDRESULT_GPU_INVALID_IMAGE_USAGE;
	// SPUDGPU_IMAGE_USAGE_TRANSIENT_ATTACHMENT's contract (see spudgpu.h) is
	// enforced identically on every backend, not just the ones that act on
	// it.
	if (desc->usage & SPUDGPU_IMAGE_USAGE_TRANSIENT_ATTACHMENT) {
		if (!(desc->usage & (SPUDGPU_IMAGE_USAGE_COLOR_ATTACHMENT | SPUDGPU_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT)))
			return SPUDRESULT_GPU_INVALID_IMAGE_USAGE;
		if (desc->usage & (SPUDGPU_IMAGE_USAGE_SAMPLED | SPUDGPU_IMAGE_USAGE_STORAGE |
		                   SPUDGPU_IMAGE_USAGE_TRANSFER_SRC | SPUDGPU_IMAGE_USAGE_TRANSFER_DST |
		                   SPUDGPU_IMAGE_USAGE_PRESENTABLE))
			return SPUDRESULT_GPU_INVALID_IMAGE_USAGE;
	}
	if (desc->format == SPUDGPU_FORMAT_UNKNOWN)
		return SPUDRESULT_GPU_INVALID_FORMAT;

	spudgpu_image_d3d12 *pResult =
	    (spudgpu_image_d3d12 *)malloc(sizeof(spudgpu_image_d3d12));
	if (!pResult)
		return SPUDRESULT_OUT_OF_MEMORY;
	pResult          = new (pResult) spudgpu_image_d3d12();
	pResult->_device = device;
	pResult->_desc   = *desc;

	SPUDRESULT sr = spudgpu_d3d12_create_resource_desc_from_image(
	    desc, &pResult->_d3d_resource_desc);
	if (sr != SPUD_SUCCESS) {
		pResult->~spudgpu_image_d3d12();
		free(pResult);
		return sr;
	}

	// An image is always in the device's memory (see spudgpu_image_desc).
	D3D12_HEAP_PROPERTIES d3dHeapProperties =
	    spudgpu_d3d12_get_heap_properties_from_memory_kind(SPUDGPU_MEMORY_KIND_DEVICE);
	D3D12_HEAP_FLAGS d3dHeapFlags = spudgpu_d3d12_get_heap_flags();
	if (desc->usage & SPUDGPU_IMAGE_USAGE_PRESENTABLE)
		d3dHeapFlags |= D3D12_HEAP_FLAG_ALLOW_DISPLAY;
	D3D12_RESOURCE_STATES d3dInitialState =
	    spudgpu_d3d12_get_initial_image_state(desc->usage);

	bool hasOptimizedClearValue =
	    (desc->usage & (SPUDGPU_IMAGE_USAGE_COLOR_ATTACHMENT |
	                    SPUDGPU_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT)) != 0;
	D3D12_CLEAR_VALUE d3dClearValue =
	    spudgpu_d3d12_get_clear_value(&desc->clear_value);
	// D3D12 requires the clear value format to match the resource format exactly.
	d3dClearValue.Format = spudgpu_d3d12_get_dxgi_format(desc->format);

	HRESULT hr = device->_d3d_device->CreateCommittedResource1(
	    &d3dHeapProperties, d3dHeapFlags, &pResult->_d3d_resource_desc,
	    d3dInitialState, hasOptimizedClearValue ? &d3dClearValue : nullptr,
	    nullptr, IID_PPV_ARGS(&pResult->_d3d_resource));
	if (FAILED(hr)) {
		printf("spudgpu: CreateCommittedResource1 failed (%ux%u, fmt=%u, flags=0x%x, heapFlags=0x%x, initialState=0x%x): hr=0x%08lx\n",
		    desc->width, desc->height, (unsigned)pResult->_d3d_resource_desc.Format, (unsigned)pResult->_d3d_resource_desc.Flags,
		    (unsigned)d3dHeapFlags, (unsigned)d3dInitialState, (unsigned long)hr);
		pResult->~spudgpu_image_d3d12();
		free(pResult);
		return SPUDRESULT_API_SPECIFIC_FAILURE;
	}
	pResult->_current_state = d3dInitialState;

	pResult->_d3d_gpu_address = pResult->_d3d_resource->GetGPUVirtualAddress();

#if _DEBUG
	if (spud_debug_name_set(pResult, desc->debug_name) != SPUD_SUCCESS) {
		spudgpu_destroy_image(pResult);
		return SPUDRESULT_OUT_OF_MEMORY;
	}
	pResult->_desc.debug_name = pResult->_debug_name;
#endif

	*out_image = pResult;

	return SPUD_SUCCESS;
}
void spudgpu_destroy_image(spudgpu_image image) {
    if (!image) return;
#if _DEBUG
    free((void *)image->_debug_name);
#endif
    image->~spudgpu_image_d3d12();
    free(image);
}
SPUDRESULT spudgpu_get_image_desc(
	spudgpu_image image,
	spudgpu_image_desc *out_desc) {
    if (!image) return SPUDRESULT_GPU_INVALID_IMAGE;
    if (!out_desc) return SPUDRESULT_NULL_OUTPUT_PARAMETER;
    *out_desc = image->_desc;
    return SPUD_SUCCESS;
}


SPUDRESULT spudgpu_create_image_view(
    spudgpu_image image,
    const spudgpu_image_view_desc *desc,
    spudgpu_image_view *out_image_view) {
	if (!image)
		return SPUDRESULT_GPU_INVALID_IMAGE;
	if (!desc)
		return SPUDRESULT_NULL_DESC;
	if (!out_image_view)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;

	spudgpu_image_view_d3d12 *pResult = (spudgpu_image_view_d3d12 *)calloc(
	    1, sizeof(spudgpu_image_view_d3d12));
	if (!pResult)
		return SPUDRESULT_OUT_OF_MEMORY;
	pResult->_image = image;
	pResult->_desc  = *desc;
	pResult->_desc.parent_image = image;

	DXGI_FORMAT dxgiFmt = spudgpu_d3d12_get_dxgi_format(image->_desc.format);
	const spudgpu_image_view_desc_subresource_range &sr = desc->subresource_range;
	SPUDGPU_IMAGE_USAGE usage = image->_desc.usage;

	if (usage & SPUDGPU_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT) {
		D3D12_DEPTH_STENCIL_VIEW_DESC &dsv = pResult->_d3d_view_desc._dsv;
		dsv.Format = dxgiFmt;
		switch (desc->type) {
		case SPUDGPU_IMAGE_VIEW_TYPE_1D:
			dsv.ViewDimension      = D3D12_DSV_DIMENSION_TEXTURE1D;
			dsv.Texture1D.MipSlice = (UINT)sr.base_mip_level;
			break;
		case SPUDGPU_IMAGE_VIEW_TYPE_1D_ARRAY:
			dsv.ViewDimension                  = D3D12_DSV_DIMENSION_TEXTURE1DARRAY;
			dsv.Texture1DArray.MipSlice        = (UINT)sr.base_mip_level;
			dsv.Texture1DArray.FirstArraySlice = (UINT)sr.base_array_layer;
			dsv.Texture1DArray.ArraySize       = (UINT)sr.array_layer_count;
			break;
		case SPUDGPU_IMAGE_VIEW_TYPE_2D_ARRAY:
			dsv.ViewDimension                  = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
			dsv.Texture2DArray.MipSlice        = (UINT)sr.base_mip_level;
			dsv.Texture2DArray.FirstArraySlice = (UINT)sr.base_array_layer;
			dsv.Texture2DArray.ArraySize       = (UINT)sr.array_layer_count;
			break;
		default:
			dsv.ViewDimension      = D3D12_DSV_DIMENSION_TEXTURE2D;
			dsv.Texture2D.MipSlice = (UINT)sr.base_mip_level;
			break;
		}
	} else if (usage & SPUDGPU_IMAGE_USAGE_COLOR_ATTACHMENT) {
		D3D12_RENDER_TARGET_VIEW_DESC &rtv = pResult->_d3d_view_desc._rtv;
		rtv.Format = dxgiFmt;
		switch (desc->type) {
		case SPUDGPU_IMAGE_VIEW_TYPE_1D:
			rtv.ViewDimension      = D3D12_RTV_DIMENSION_TEXTURE1D;
			rtv.Texture1D.MipSlice = (UINT)sr.base_mip_level;
			break;
		case SPUDGPU_IMAGE_VIEW_TYPE_1D_ARRAY:
			rtv.ViewDimension                  = D3D12_RTV_DIMENSION_TEXTURE1DARRAY;
			rtv.Texture1DArray.MipSlice        = (UINT)sr.base_mip_level;
			rtv.Texture1DArray.FirstArraySlice = (UINT)sr.base_array_layer;
			rtv.Texture1DArray.ArraySize       = (UINT)sr.array_layer_count;
			break;
		case SPUDGPU_IMAGE_VIEW_TYPE_3D:
			rtv.ViewDimension         = D3D12_RTV_DIMENSION_TEXTURE3D;
			rtv.Texture3D.MipSlice    = (UINT)sr.base_mip_level;
			rtv.Texture3D.FirstWSlice = (UINT)sr.base_array_layer;
			rtv.Texture3D.WSize       = (UINT)sr.array_layer_count;
			break;
		case SPUDGPU_IMAGE_VIEW_TYPE_2D_ARRAY:
			rtv.ViewDimension                  = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
			rtv.Texture2DArray.MipSlice        = (UINT)sr.base_mip_level;
			rtv.Texture2DArray.FirstArraySlice = (UINT)sr.base_array_layer;
			rtv.Texture2DArray.ArraySize       = (UINT)sr.array_layer_count;
			break;
		default:
			rtv.ViewDimension      = D3D12_RTV_DIMENSION_TEXTURE2D;
			rtv.Texture2D.MipSlice = (UINT)sr.base_mip_level;
			break;
		}
	} else if (usage & SPUDGPU_IMAGE_USAGE_STORAGE) {
		D3D12_UNORDERED_ACCESS_VIEW_DESC &uav = pResult->_d3d_view_desc._uav;
		uav.Format = dxgiFmt;
		switch (desc->type) {
		case SPUDGPU_IMAGE_VIEW_TYPE_1D:
			uav.ViewDimension      = D3D12_UAV_DIMENSION_TEXTURE1D;
			uav.Texture1D.MipSlice = (UINT)sr.base_mip_level;
			break;
		case SPUDGPU_IMAGE_VIEW_TYPE_1D_ARRAY:
			uav.ViewDimension                  = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
			uav.Texture1DArray.MipSlice        = (UINT)sr.base_mip_level;
			uav.Texture1DArray.FirstArraySlice = (UINT)sr.base_array_layer;
			uav.Texture1DArray.ArraySize       = (UINT)sr.array_layer_count;
			break;
		case SPUDGPU_IMAGE_VIEW_TYPE_3D:
			uav.ViewDimension         = D3D12_UAV_DIMENSION_TEXTURE3D;
			uav.Texture3D.MipSlice    = (UINT)sr.base_mip_level;
			uav.Texture3D.FirstWSlice = (UINT)sr.base_array_layer;
			uav.Texture3D.WSize       = (UINT)sr.array_layer_count;
			break;
		case SPUDGPU_IMAGE_VIEW_TYPE_2D_ARRAY:
			uav.ViewDimension                  = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
			uav.Texture2DArray.MipSlice        = (UINT)sr.base_mip_level;
			uav.Texture2DArray.FirstArraySlice = (UINT)sr.base_array_layer;
			uav.Texture2DArray.ArraySize       = (UINT)sr.array_layer_count;
			break;
		default:
			uav.ViewDimension      = D3D12_UAV_DIMENSION_TEXTURE2D;
			uav.Texture2D.MipSlice = (UINT)sr.base_mip_level;
			break;
		}
	} else {
		D3D12_SHADER_RESOURCE_VIEW_DESC &srv = pResult->_d3d_view_desc._srv;
		srv.Format                  = dxgiFmt;
		srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		switch (desc->type) {
		case SPUDGPU_IMAGE_VIEW_TYPE_1D:
			srv.ViewDimension                 = D3D12_SRV_DIMENSION_TEXTURE1D;
			srv.Texture1D.MostDetailedMip     = (UINT)sr.base_mip_level;
			srv.Texture1D.MipLevels           = (UINT)sr.mip_level_count;
			srv.Texture1D.ResourceMinLODClamp = 0.0f;
			break;
		case SPUDGPU_IMAGE_VIEW_TYPE_1D_ARRAY:
			srv.ViewDimension                      = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
			srv.Texture1DArray.MostDetailedMip     = (UINT)sr.base_mip_level;
			srv.Texture1DArray.MipLevels           = (UINT)sr.mip_level_count;
			srv.Texture1DArray.FirstArraySlice     = (UINT)sr.base_array_layer;
			srv.Texture1DArray.ArraySize           = (UINT)sr.array_layer_count;
			srv.Texture1DArray.ResourceMinLODClamp = 0.0f;
			break;
		case SPUDGPU_IMAGE_VIEW_TYPE_3D:
			srv.ViewDimension                 = D3D12_SRV_DIMENSION_TEXTURE3D;
			srv.Texture3D.MostDetailedMip     = (UINT)sr.base_mip_level;
			srv.Texture3D.MipLevels           = (UINT)sr.mip_level_count;
			srv.Texture3D.ResourceMinLODClamp = 0.0f;
			break;
		case SPUDGPU_IMAGE_VIEW_TYPE_CUBE:
			srv.ViewDimension                   = D3D12_SRV_DIMENSION_TEXTURECUBE;
			srv.TextureCube.MostDetailedMip     = (UINT)sr.base_mip_level;
			srv.TextureCube.MipLevels           = (UINT)sr.mip_level_count;
			srv.TextureCube.ResourceMinLODClamp = 0.0f;
			break;
		case SPUDGPU_IMAGE_VIEW_TYPE_2D_ARRAY:
			srv.ViewDimension                       = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
			srv.Texture2DArray.MostDetailedMip      = (UINT)sr.base_mip_level;
			srv.Texture2DArray.MipLevels            = (UINT)sr.mip_level_count;
			srv.Texture2DArray.FirstArraySlice      = (UINT)sr.base_array_layer;
			srv.Texture2DArray.ArraySize            = (UINT)sr.array_layer_count;
			srv.Texture2DArray.PlaneSlice           = 0;
			srv.Texture2DArray.ResourceMinLODClamp  = 0.0f;
			break;
		case SPUDGPU_IMAGE_VIEW_TYPE_CUBE_ARRAY:
			srv.ViewDimension                          = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
			srv.TextureCubeArray.MostDetailedMip       = (UINT)sr.base_mip_level;
			srv.TextureCubeArray.MipLevels             = (UINT)sr.mip_level_count;
			srv.TextureCubeArray.First2DArrayFace      = (UINT)sr.base_array_layer;
			srv.TextureCubeArray.NumCubes              = (UINT)(sr.array_layer_count / 6);
			srv.TextureCubeArray.ResourceMinLODClamp   = 0.0f;
			break;
		default:
			srv.ViewDimension                  = D3D12_SRV_DIMENSION_TEXTURE2D;
			srv.Texture2D.MostDetailedMip      = (UINT)sr.base_mip_level;
			srv.Texture2D.MipLevels            = (UINT)sr.mip_level_count;
			srv.Texture2D.PlaneSlice           = 0;
			srv.Texture2D.ResourceMinLODClamp  = 0.0f;
			break;
		}
	}

	*out_image_view = pResult;
    return SPUD_SUCCESS;
}
void spudgpu_destroy_image_view(spudgpu_image_view image_view) {
	if (!image_view)
		return;
#if _DEBUG
	free((void *)image_view->_debug_name);
#endif
	free(image_view);
}
SPUDRESULT spudgpu_get_image_view_desc(
    spudgpu_image_view image_view, spudgpu_image_view_desc *out_desc) {
	if (!image_view)
		return SPUDRESULT_GPU_INVALID_IMAGE_VIEW;
	if (!out_desc)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	*out_desc = image_view->_desc;
	return SPUD_SUCCESS;
}
}

#endif // SPUDGPU_COMPILE_D3D12_API
