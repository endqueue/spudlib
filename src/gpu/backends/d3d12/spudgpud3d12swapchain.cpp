
#if SPUDGPU_COMPILE_D3D12_API

#include "spudgpud3d12.hpp"
#include "../../../spudstructbase.h"

// Releases a swap chain and its back buffer arrays. Either array may still
// be null, which is how a swap chain that failed part-way through creation
// arrives here.
static void spudgpu_d3d12___release_swap_chain(spudgpu_swap_chain_d3d12 *swap_chain) {
	if (swap_chain->_back_buffer_images) {
		for (uint32_t i = 0; i < swap_chain->_desc.buffer_count; ++i) {
			free((void *)swap_chain->_back_buffer_images[i]._debug_name);
			swap_chain->_back_buffer_images[i].~spudgpu_image_d3d12();
		}
		free(swap_chain->_back_buffer_images);
	}
	if (swap_chain->_back_buffer_image_views) {
		for (uint32_t i = 0; i < swap_chain->_desc.buffer_count; ++i)
			free((void *)swap_chain->_back_buffer_image_views[i]._debug_name);
	}
	free(swap_chain->_back_buffer_image_views);
	free((void *)swap_chain->_debug_name);
	swap_chain->~spudgpu_swap_chain_d3d12();
	free(swap_chain);
}

extern "C" {

SPUDRESULT spudgpu_create_swap_chain(
    spudgpu_device device,
    const spudgpu_swap_chain_desc *desc,
    spudgpu_swap_chain *out_swap_chain) {
	if (!device)
		return SPUDRESULT_GPU_INVALID_DEVICE;
	if (!desc)
		return SPUDRESULT_NULL_DESC;
	const SPUDRESULT base_sr = spud_struct_check(&desc->base, SPUD_STRUCT_TYPE_GPU_SWAP_CHAIN_DESC);
	if (base_sr != SPUD_SUCCESS)
		return base_sr;
	if (!out_swap_chain)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	if (!desc->surface)
		return SPUDRESULT_GPU_INVALID_SURFACE;
	if (!desc->queue)
		return SPUDRESULT_GPU_INVALID_COMMAND_QUEUE;

	spudgpu_swap_chain_d3d12 *pResult =
	    (spudgpu_swap_chain_d3d12 *)malloc(sizeof(spudgpu_swap_chain_d3d12));
	if (!pResult)
		return SPUDRESULT_OUT_OF_MEMORY;
	pResult          = new (pResult) spudgpu_swap_chain_d3d12();
	pResult->_device = device;
	pResult->_desc   = *desc;

	HWND hwnd = desc->surface->_hwnd;
	// The DXGI swap chain is bound to this specific queue for its lifetime -
	// spudgpu_swap_chain_desc::queue, not a hardcoded "the graphics queue"
	// guess, since DXGI can't rebind a swap chain to a different queue later.
	ID3D12CommandQueue *cmdQueue = desc->queue->_d3d_cmd_queue.Get();

	DXGI_SWAP_CHAIN_DESC1 scDesc = {};
	scDesc.Width                 = desc->width;
	scDesc.Height                = desc->height;
	scDesc.Format                = spudgpu_d3d12_get_dxgi_format(desc->format);
	scDesc.Stereo                = FALSE;
	scDesc.SampleDesc            = {1, 0};
	scDesc.BufferUsage           = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	// Copy source/destination needs no DXGI flag; D3D12 back buffers allow
	// CopyTextureRegion either way.
	if (desc->usage & SPUDGPU_IMAGE_USAGE_SAMPLED)
		scDesc.BufferUsage |= DXGI_USAGE_SHADER_INPUT;
	if (desc->usage & SPUDGPU_IMAGE_USAGE_STORAGE)
		scDesc.BufferUsage |= DXGI_USAGE_UNORDERED_ACCESS;
	scDesc.BufferCount           = desc->buffer_count;
	scDesc.Scaling               = DXGI_SCALING_STRETCH;
	scDesc.SwapEffect            = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	scDesc.AlphaMode             = DXGI_ALPHA_MODE_UNSPECIFIED;
	scDesc.Flags                 = 0;

	bool isExclusiveFullscreen =
	    (desc->fullscreen_mode == SPUDGPU_FULLSCREEN_MODE_FULLSCREEN);
	DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsDesc = {};
	fsDesc.RefreshRate                     = {0, 1};
	fsDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	fsDesc.Scaling          = DXGI_MODE_SCALING_UNSPECIFIED;
	fsDesc.Windowed         = FALSE;

	Microsoft::WRL::ComPtr<IDXGISwapChain1> dxgiSwapChain1;
	HRESULT hr = device->_instance->_dxgi_factory->CreateSwapChainForHwnd(
	    cmdQueue, hwnd, &scDesc, isExclusiveFullscreen ? &fsDesc : nullptr,
	    nullptr, &dxgiSwapChain1);
	if (FAILED(hr)) {
		spudgpu_d3d12___release_swap_chain(pResult);
		return SPUDRESULT_API_SPECIFIC_FAILURE;
	}
	hr = dxgiSwapChain1.As(&pResult->_dxgi_swap_chain);
	if (FAILED(hr)) {
		spudgpu_d3d12___release_swap_chain(pResult);
		return SPUDRESULT_API_SPECIFIC_FAILURE;
	}

	// The image holds a ComPtr, so each element is constructed in place; the
	// image view is plain and zeroed memory is its initial state. Every
	// image is constructed before the first GetBuffer so that a failure
	// below always finds buffer_count constructed images to release.
	spudgpu_image_d3d12 *images = (spudgpu_image_d3d12 *)malloc(
	    sizeof(spudgpu_image_d3d12) * desc->buffer_count);
	if (!images) {
		spudgpu_d3d12___release_swap_chain(pResult);
		return SPUDRESULT_OUT_OF_MEMORY;
	}
	for (uint32_t i = 0; i < desc->buffer_count; ++i)
		new (&images[i]) spudgpu_image_d3d12();
	pResult->_back_buffer_images = images;

	pResult->_back_buffer_image_views = (spudgpu_image_view_d3d12 *)calloc(
	    desc->buffer_count, sizeof(spudgpu_image_view_d3d12));
	if (!pResult->_back_buffer_image_views) {
		spudgpu_d3d12___release_swap_chain(pResult);
		return SPUDRESULT_OUT_OF_MEMORY;
	}

	DXGI_FORMAT fmt = spudgpu_d3d12_get_dxgi_format(desc->format);

	for (uint32_t i = 0; i < desc->buffer_count; ++i) {
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		hr = pResult->_dxgi_swap_chain->GetBuffer(i, IID_PPV_ARGS(&resource));
		if (FAILED(hr)) {
			spudgpu_d3d12___release_swap_chain(pResult);
			return SPUDRESULT_API_SPECIFIC_FAILURE;
		}

		spudgpu_image_d3d12 &img = pResult->_back_buffer_images[i];
		hr = resource.As(&img._d3d_resource);
		if (FAILED(hr)) {
			spudgpu_d3d12___release_swap_chain(pResult);
			return SPUDRESULT_API_SPECIFIC_FAILURE;
		}
		img._device            = device;
		img._desc.format       = desc->format;
		img._desc.width        = desc->width;
		img._desc.height       = desc->height;
		img._desc.depth        = 1;
		img._desc.mip_levels   = 1;
		img._desc.array_layers = 1;
		img._desc.type         = SPUDGPU_IMAGE_TYPE_2D;
		img._desc.usage        = SPUDGPU_IMAGE_USAGE_COLOR_ATTACHMENT | desc->usage;
		img._d3d_resource_desc = img._d3d_resource->GetDesc();
		// Swap chain back buffers start life in PRESENT (== COMMON, 0x0), not
		// the RENDER_TARGET state SPUDGPU_IMAGE_USAGE_COLOR_ATTACHMENT would
		// otherwise imply for a freshly spudgpu_create_image'd resource --
		// they're DXGI-provided, not created via CreateCommittedResource1, so
		// spudgpu_d3d12_get_initial_image_state's usage-based guess doesn't
		// apply here.
		img._current_state     = D3D12_RESOURCE_STATE_PRESENT;

		spudgpu_image_view_d3d12 &view = pResult->_back_buffer_image_views[i];
		view._image                    = &img;
		view._desc.type                = SPUDGPU_IMAGE_VIEW_TYPE_2D;
		view._desc.parent_image        = &img;
		// view._desc.format                             = desc->format;
		view._d3d_view_desc._rtv.Format        = fmt;
		view._d3d_view_desc._rtv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		view._d3d_view_desc._rtv.Texture2D.MipSlice   = 0;
		view._d3d_view_desc._rtv.Texture2D.PlaneSlice = 0;
	}

	if (FAILED(device->_d3d_device->CreateFence(
	        0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&pResult->_frame_fence)))) {
		spudgpu_d3d12___release_swap_chain(pResult);
		return SPUDRESULT_API_SPECIFIC_FAILURE;
	}
	pResult->_frame_fence_next_value = 0;

	*out_swap_chain = pResult;
	return SPUD_SUCCESS;
}

void spudgpu_destroy_swap_chain(spudgpu_swap_chain swap_chain) {
	if (!swap_chain)
		return;
	spudgpu_d3d12___release_swap_chain(swap_chain);
}

SPUDRESULT spudgpu_get_swap_chain_desc(
    spudgpu_swap_chain swap_chain, spudgpu_swap_chain_desc *out_desc) {
	if (!swap_chain)
		return SPUDRESULT_GPU_INVALID_SWAP_CHAIN;
	if (!out_desc)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	*out_desc = swap_chain->_desc;
	out_desc->base = spud_struct_base(SPUD_STRUCT_TYPE_GPU_SWAP_CHAIN_DESC);
	return SPUD_SUCCESS;
}

uint32_t spudgpu_swap_chain_acquire_next_image(spudgpu_swap_chain swap_chain) {
	if (!swap_chain)
		return SPUD_UINT32_MAX;

	// The in-flight-fence half of spudgpu_submit_command_lists_synced's
	// contract (see spudgpu_swap_chain_d3d12 in spudgpud3d12.hpp) -- block
	// until the previously submitted frame's GPU work has completed before
	// handing back a buffer index the caller is about to reuse a shared
	// command allocator/list against.
	uint64_t pending = swap_chain->_frame_fence_next_value;
	if (pending != 0 && swap_chain->_frame_fence->GetCompletedValue() < pending) {
		HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
		if (event) {
			if (SUCCEEDED(swap_chain->_frame_fence->SetEventOnCompletion(pending, event)))
				WaitForSingleObject(event, INFINITE);
			CloseHandle(event);
		}
	}

	return swap_chain->_dxgi_swap_chain->GetCurrentBackBufferIndex();
}

void spudgpu_swap_chain_present(spudgpu_swap_chain swap_chain) {
	if (!swap_chain)
		return;
	UINT syncInterval =
	    (swap_chain->_desc.present_mode == SPUDGPU_PRESENT_MODE_IMMEDIATE) ? 0
	                                                                       : 1;
	swap_chain->_dxgi_swap_chain->Present(syncInterval, 0);
}

spudgpu_image_view spudgpu_get_swap_chain_image_view(
    spudgpu_swap_chain swap_chain, uint32_t image_index) {
	if (!swap_chain || image_index >= swap_chain->_desc.buffer_count)
		return nullptr;
	return &swap_chain->_back_buffer_image_views[image_index];
}
}

#endif // SPUDGPU_COMPILE_D3D12_API
