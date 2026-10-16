
#if SPUDGPU_COMPILE_D3D12_API

#include "spudgpud3d12.hpp"
#include <cstring>

extern "C" {

spudgpu_command_queue spudgpu_get_graphics_queue(spudgpu_device device) {
	if (!device)
		return nullptr;
	return device->_cmd_queues_direct[0];
}

uint32_t spudgpu_get_max_queue_count(
    spudgpu_device device, SPUDGPU_COMMAND_LIST_TYPE type) {
	if (!device)
		return 0;
	// Bundles aren't submitted to a queue directly, so they have no queues.
	if (type != SPUDGPU_COMMAND_LIST_TYPE_DIRECT && type != SPUDGPU_COMMAND_LIST_TYPE_COPY &&
	    type != SPUDGPU_COMMAND_LIST_TYPE_COMPUTE)
		return 0;
	return SPUD_D3D12_COMMAND_QUEUE_COUNT_PER_FAMILY;
}

SPUDRESULT spudgpu_get_command_queue(
    spudgpu_device device,
    SPUDGPU_COMMAND_LIST_TYPE type,
    uint32_t index,
    spudgpu_command_queue *out_queue) {
	if (!device)
		return SPUDRESULT_GPU_INVALID_DEVICE;
	if (type != SPUDGPU_COMMAND_LIST_TYPE_DIRECT && type != SPUDGPU_COMMAND_LIST_TYPE_COPY &&
	    type != SPUDGPU_COMMAND_LIST_TYPE_COMPUTE)
		return SPUDRESULT_GPU_INVALID_COMMAND_LIST_TYPE;
	if (index >= SPUD_D3D12_COMMAND_QUEUE_COUNT_PER_FAMILY)
		return SPUDRESULT_INDEX_OUT_OF_RANGE;
	if (!out_queue)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	switch (type) {
	case SPUDGPU_COMMAND_LIST_TYPE_DIRECT:
		*out_queue = device->_cmd_queues_direct[index];
		break;
	case SPUDGPU_COMMAND_LIST_TYPE_COPY:
		*out_queue = device->_cmd_queues_copy[index];
		break;
	case SPUDGPU_COMMAND_LIST_TYPE_COMPUTE:
		*out_queue = device->_cmd_queues_compute[index];
		break;
	default:
		return SPUDRESULT_GPU_INVALID_COMMAND_LIST_TYPE;
	}
	return SPUD_SUCCESS;
}

SPUDRESULT spudgpu_submit_command_lists(
    spudgpu_command_queue queue,
    spudgpu_command_list *cmd_lists,
    uint32_t cmd_list_count) {
	if (!queue)
		return SPUDRESULT_GPU_INVALID_COMMAND_QUEUE;
	if (!cmd_lists)
		return SPUDRESULT_GPU_INVALID_COMMAND_LIST;
	if (cmd_list_count == 0)
		return SPUDRESULT_ZERO_SIZE;
	ID3D12CommandList **d3dLists =
	    (ID3D12CommandList **)malloc(sizeof(ID3D12CommandList *) * cmd_list_count);
	if (!d3dLists)
		return SPUDRESULT_OUT_OF_MEMORY;
	for (uint32_t i = 0; i < cmd_list_count; ++i)
		d3dLists[i] = cmd_lists[i]->_d3d_cmd_list.Get();
	queue->_d3d_cmd_queue->ExecuteCommandLists(
	    static_cast<UINT>(cmd_list_count), d3dLists);
	free(d3dLists);
	return SPUD_SUCCESS;
}

SPUDRESULT spudgpu_submit_command_lists_synced(
    spudgpu_command_queue queue,
    spudgpu_command_list *cmd_lists,
    uint32_t cmd_list_count,
    spudgpu_swap_chain swap_chain) {
	if (!queue)
		return SPUDRESULT_GPU_INVALID_COMMAND_QUEUE;
	if (!cmd_lists)
		return SPUDRESULT_GPU_INVALID_COMMAND_LIST;
	if (cmd_list_count == 0)
		return SPUDRESULT_ZERO_SIZE;
	if (!swap_chain)
		return SPUDRESULT_GPU_INVALID_SWAP_CHAIN;

	// spudgpu_queue_submit with only the command lists and the swap chain set.
	spudgpu_submit_desc desc = {};
	desc.cmd_lists           = cmd_lists;
	desc.cmd_list_count      = cmd_list_count;
	desc.swap_chain          = swap_chain;
	return spudgpu_queue_submit(queue, &desc);
}

SPUDRESULT spudgpu_create_command_allocator(
    spudgpu_device device,
    const spudgpu_command_allocator_desc *desc,
    spudgpu_command_allocator *out_allocator) {
	if (!device)
		return SPUDRESULT_GPU_INVALID_DEVICE;
	if (!desc)
		return SPUDRESULT_NULL_DESC;
	if (!out_allocator)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	spudgpu_command_allocator_d3d12 *pResult =
	    (spudgpu_command_allocator_d3d12 *)malloc(
	        sizeof(spudgpu_command_allocator_d3d12));
	if (!pResult)
		return SPUDRESULT_OUT_OF_MEMORY;
	pResult          = new (pResult) spudgpu_command_allocator_d3d12();
	pResult->_device = device;
    pResult->_d3d_cmd_list_type = spudgpu_d3d12_get_command_list_type(desc->type);
	if (FAILED(device->_d3d_device->CreateCommandAllocator(
	        pResult->_d3d_cmd_list_type,
	        IID_PPV_ARGS(&pResult->_d3d_cmd_allocator)))) {
		pResult->~spudgpu_command_allocator_d3d12();
		free(pResult);
		return SPUDRESULT_API_SPECIFIC_FAILURE;
	}
	*out_allocator = pResult;
	return SPUD_SUCCESS;
}

void spudgpu_destroy_command_allocator(spudgpu_command_allocator allocator) {
	if (!allocator)
		return;
#if _DEBUG
	free((void *)allocator->_debug_name);
#endif
	allocator->~spudgpu_command_allocator_d3d12();
	free(allocator);
}

SPUDRESULT
spudgpu_reset_command_allocator(spudgpu_command_allocator allocator) {
	if (!allocator)
		return SPUDRESULT_GPU_INVALID_COMMAND_ALLOCATOR;
	if (FAILED(allocator->_d3d_cmd_allocator->Reset()))
		return SPUDRESULT_API_SPECIFIC_FAILURE;
	return SPUD_SUCCESS;
}

SPUDRESULT spudgpu_create_command_list(
    spudgpu_command_allocator allocator, spudgpu_command_list *out_cmd_list) {
	if (!allocator)
		return SPUDRESULT_GPU_INVALID_COMMAND_ALLOCATOR;
    if (!out_cmd_list)
        return SPUDRESULT_NULL_OUTPUT_PARAMETER;
    spudgpu_command_list_d3d12 *pResult = (spudgpu_command_list_d3d12 *) malloc(sizeof(spudgpu_command_list_d3d12));
    if (!pResult)
        return SPUDRESULT_OUT_OF_MEMORY;
    pResult = new (pResult) spudgpu_command_list_d3d12();
    pResult->_allocator = allocator;
    if (FAILED(pResult->_allocator->_device->_d3d_device->CreateCommandList1(
        1, allocator->_d3d_cmd_list_type, D3D12_COMMAND_LIST_FLAG_NONE,
        IID_PPV_ARGS(&pResult->_d3d_cmd_list)
    ))) {
        pResult->~spudgpu_command_list_d3d12();
        free(pResult);
        return SPUDRESULT_API_SPECIFIC_FAILURE;
    }
	*out_cmd_list = pResult;
    return SPUD_SUCCESS;
}
void spudgpu_destroy_command_list(spudgpu_command_list cmd) {
    if (!cmd) return;
#if _DEBUG
    free((void *)cmd->_debug_name);
#endif
    cmd->~spudgpu_command_list_d3d12();
    free(cmd);
}
void spudgpu_begin_command_list(spudgpu_command_list cmd)
{
	if (!cmd) return;
	cmd->_d3d_cmd_list->Reset(
	    cmd->_allocator->_d3d_cmd_allocator.Get(), nullptr);
}
void spudgpu_end_command_list(spudgpu_command_list cmd)
{
	if (!cmd) return;
	cmd->_d3d_cmd_list->Close();
}

// PIX's encoding of an event's text: metadata 1 is a null-terminated ANSI
// string (PIX_EVENT_ANSI_VERSION in pix3.h), which PIX and the debug layer
// read without WinPixEventRuntime.
static const UINT spudgpu_d3d12_pix_event_ansi = 1;

void spudgpu_cmd_begin_debug_label(spudgpu_command_list cmd, const char *label) {
	if (!cmd) return;
	const char *text = label ? label : "";
	cmd->_d3d_cmd_list->BeginEvent(spudgpu_d3d12_pix_event_ansi, text, (UINT)(strlen(text) + 1));
}

void spudgpu_cmd_end_debug_label(spudgpu_command_list cmd) {
	if (!cmd) return;
	cmd->_d3d_cmd_list->EndEvent();
}
void spudgpu_cmd_set_viewports(
    spudgpu_command_list cmd,
    uint32_t first_viewport,
    uint32_t viewport_count,
    const SPUDGPU_VIEWPORT *viewports) {
	if (!cmd || !viewports || !viewport_count)
		return;
	//D3D12_VIEWPORT *d3dViewports;
	cmd->_d3d_cmd_list->RSSetViewports(
	    static_cast<UINT>(viewport_count),
	    reinterpret_cast<D3D12_VIEWPORT *>(
	        const_cast<SPUDGPU_VIEWPORT *>(viewports)));
}
void spudgpu_cmd_set_scissor_rects(
    spudgpu_command_list cmd,
    uint32_t first_scissor_rect,
    uint32_t scissor_rect_count,
    const SPUDGPU_SCISSOR_RECT *scissor_rects) {
	if (!cmd || !scissor_rects || !scissor_rect_count)
		return;
	// SPUDGPU_SCISSOR_RECT is {x, y, width, height} floats; D3D12_RECT is
	// {left, top, right, bottom} LONGs -- not layout-compatible, needs a
	// real per-rect conversion rather than a reinterpret_cast.
	D3D12_RECT *d3dRects = (D3D12_RECT *)malloc(
	    sizeof(D3D12_RECT) * scissor_rect_count);
	if (!d3dRects)
		return;
	for (uint32_t i = 0; i < scissor_rect_count; ++i) {
		d3dRects[i].left   = (LONG)scissor_rects[i].x;
		d3dRects[i].top    = (LONG)scissor_rects[i].y;
		d3dRects[i].right  = (LONG)(scissor_rects[i].x + scissor_rects[i].width);
		d3dRects[i].bottom = (LONG)(scissor_rects[i].y + scissor_rects[i].height);
	}
	cmd->_d3d_cmd_list->RSSetScissorRects(
	    static_cast<UINT>(scissor_rect_count), d3dRects);
	free(d3dRects);
}
void spudgpu_cmd_set_vertex_buffers(
    spudgpu_command_list cmd,
    uint32_t start_slot,
    uint32_t view_count,
    spudgpu_buffer_view *buffer_views) {
	if (!cmd || !view_count || !buffer_views)
		return;
	D3D12_VERTEX_BUFFER_VIEW *d3dVbViews = (D3D12_VERTEX_BUFFER_VIEW *)malloc(
	    sizeof(D3D12_VERTEX_BUFFER_VIEW) * view_count);
	if (!d3dVbViews)
		return;
	for (uint32_t i = 0; i < view_count; ++i)
		d3dVbViews[i] = buffer_views[i]->_d3d_view._vb;
	cmd->_d3d_cmd_list->IASetVertexBuffers(
		static_cast<UINT>(start_slot),
		static_cast<UINT>(view_count),
		d3dVbViews
	);
	free(d3dVbViews);
}
void spudgpu_cmd_set_index_buffer(
    spudgpu_command_list cmd,
    spudgpu_buffer_view buffer_view) {
	if (!cmd || !buffer_view)
		return;
	cmd->_d3d_cmd_list->IASetIndexBuffer(&buffer_view->_d3d_view._ib);
}
void spudgpu_cmd_draw(
    spudgpu_command_list cmd,
    uint32_t vertex_count,
    uint32_t start_vertex_location) {
	if (!cmd)
		return;
	cmd->_d3d_cmd_list->DrawInstanced(
	    vertex_count, 1, start_vertex_location, 0);
}
void spudgpu_cmd_draw_indexed(
    spudgpu_command_list cmd,
    uint32_t index_count,
    uint32_t start_index_location,
    int32_t base_vertex_location) {
	if (!cmd)
		return;
	cmd->_d3d_cmd_list->DrawIndexedInstanced(
		index_count, 1, start_index_location, base_vertex_location, 0);
}
void spudgpu_cmd_draw_instanced(
    spudgpu_command_list cmd,
    uint32_t vertex_count_per_instance,
    uint32_t instance_count,
    uint32_t start_vertex_location,
    uint32_t start_instance_location) {
	if (!cmd)
		return;
	cmd->_d3d_cmd_list->DrawInstanced(
		vertex_count_per_instance,
		instance_count,
		start_vertex_location,
		start_instance_location);
}
void spudgpu_cmd_draw_indexed_instanced(
    spudgpu_command_list cmd,
    uint32_t index_count_per_instance,
    uint32_t instance_count,
    uint32_t start_index_location,
    int32_t base_vertex_location,
    uint32_t start_instance_location) {
	if (!cmd)
		return;
	cmd->_d3d_cmd_list->DrawIndexedInstanced(
		index_count_per_instance,
		instance_count,
		start_index_location,
		base_vertex_location,
		start_instance_location);
}
/*
void spudgpu_cmd_bind_pipeline(
    spudgpu_command_list cmd,
    spudgpu_shader_pipeline pipeline) {
	if (!cmd || !pipeline)
		return;
	ID3D12GraphicsCommandList *cmdList = cmd->_d3d_cmd_list.Get();
	cmdList->SetPipelineState(pipeline->_d3d_pipeline_state.Get());
	cmdList->SetGraphicsRootSignature(pipeline->_d3d_root_signature.Get());
	cmdList->IASetPrimitiveTopology(pipeline->_d3d_primitive_topology);
}
void spudgpu_cmd_push_constants(
    spudgpu_command_list cmd,
    spudgpu_shader_pipeline pipeline,
    uint32_t offset,
    uint32_t size,
    const void *data) {
	if (!cmd || !pipeline || !data || size == 0)
		return;
	cmd->_d3d_cmd_list->SetGraphicsRoot32BitConstants(
	    0,
	    size / sizeof(uint32_t),
	    data,
	    offset / sizeof(uint32_t));
}*/

void spudgpu_cmd_dispatch(
    spudgpu_command_list cmd,
    uint32_t group_count_x,
    uint32_t group_count_y,
    uint32_t group_count_z) {
	if (!cmd)
		return;
	cmd->_d3d_cmd_list->Dispatch(group_count_x, group_count_y, group_count_z);
}

#if SPUDGPU_EXT_MESH_SHADING
void spudgpu_cmd_dispatch_mesh(
    spudgpu_command_list cmd,
    uint32_t group_count_x,
    uint32_t group_count_y,
    uint32_t group_count_z) {
	if (!cmd)
		return;
	// DispatchMesh is declared on ID3D12GraphicsCommandList6 - _d3d_cmd_list
	// is stored as the newer ID3D12GraphicsCommandList10 (spudgpud3d12.hpp),
	// which already inherits it, so no QueryInterface is needed here.
	cmd->_d3d_cmd_list->DispatchMesh(group_count_x, group_count_y, group_count_z);
}
#endif

#if SPUDGPU_EXT_DEPTH_BOUNDS_TEST
void spudgpu_cmd_set_depth_bounds(
    spudgpu_command_list cmd,
    float min_depth_bounds,
    float max_depth_bounds) {
	if (!cmd)
		return;
	// OMSetDepthBounds is declared on ID3D12GraphicsCommandList1 - _d3d_cmd_list
	// is stored as the newer ID3D12GraphicsCommandList10 (spudgpud3d12.hpp),
	// which already inherits it, so no QueryInterface is needed here.
	cmd->_d3d_cmd_list->OMSetDepthBounds(min_depth_bounds, max_depth_bounds);
}
#endif

// Lazily creates (and caches on the device) the root-signature-less command
// signature used by spudgpu_cmd_draw_indirect / _indexed_indirect. Mechanical
// translation, not a policy choice: D3D12 structurally requires this object
// for any ExecuteIndirect call at all, and since SpudGPU's indirect-draw
// argument layout never varies per caller (see spudgpu_draw_indirect_args /
// spudgpu_draw_indexed_indirect_args in spudgpu.h), one signature per device
// per draw type covers every call site.
static ID3D12CommandSignature *spudgpu_d3d12___get_draw_indirect_signature(
    spudgpu_device_d3d12 *device, bool indexed) {
	Microsoft::WRL::ComPtr<ID3D12CommandSignature> &cached =
	    indexed ? device->_draw_indexed_indirect_command_signature
	            : device->_draw_indirect_command_signature;
	if (cached)
		return cached.Get();

	D3D12_INDIRECT_ARGUMENT_DESC argument_desc = {};
	argument_desc.Type = indexed ? D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED
	                              : D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;

	D3D12_COMMAND_SIGNATURE_DESC signature_desc = {};
	signature_desc.pArgumentDescs = &argument_desc;
	signature_desc.NumArgumentDescs = 1;
	signature_desc.ByteStride = indexed
	                                ? sizeof(spudgpu_draw_indexed_indirect_args)
	                                : sizeof(spudgpu_draw_indirect_args);

	// rootSignature is nullptr: only valid (and required) when every argument
	// in the signature is DRAW/DRAW_INDEXED/DISPATCH — no root-parameter
	// update arguments, which SpudGPU's indirect draw never uses.
	if (FAILED(device->_d3d_device->CreateCommandSignature(
	        &signature_desc, nullptr, IID_PPV_ARGS(&cached))))
		return nullptr;
	return cached.Get();
}

void spudgpu_cmd_draw_indirect(
    spudgpu_command_list cmd,
    spudgpu_buffer buffer,
    uint64_t offset,
    uint32_t draw_count,
    uint32_t stride) {
	if (!cmd || !buffer || draw_count == 0)
		return;
	ID3D12CommandSignature *signature =
	    spudgpu_d3d12___get_draw_indirect_signature(cmd->_allocator->_device, false);
	if (!signature)
		return;
	cmd->_d3d_cmd_list->ExecuteIndirect(
	    signature, draw_count, buffer->_d3d_resource.Get(), offset, nullptr, 0);
}

void spudgpu_cmd_draw_indexed_indirect(
    spudgpu_command_list cmd,
    spudgpu_buffer buffer,
    uint64_t offset,
    uint32_t draw_count,
    uint32_t stride) {
	if (!cmd || !buffer || draw_count == 0)
		return;
	ID3D12CommandSignature *signature =
	    spudgpu_d3d12___get_draw_indirect_signature(cmd->_allocator->_device, true);
	if (!signature)
		return;
	cmd->_d3d_cmd_list->ExecuteIndirect(
	    signature, draw_count, buffer->_d3d_resource.Get(), offset, nullptr, 0);
}

// ---------------------------------------------------------------------------
// spudgpu_cmd_pipeline_barrier — SPUDGPU_RESOURCE_STATE -> D3D12_RESOURCE_STATES.
// Distinct from the image-only spudgpu_cmd_image_barrier mechanism (which
// uses SPUDGPU_IMAGE_LAYOUT and already works for swap chain/render target
// transitions) — this is the more general resource-state model that also
// covers buffers.
// ---------------------------------------------------------------------------

static D3D12_RESOURCE_STATES spudgpu_d3d12___resource_state(SPUDGPU_RESOURCE_STATE state) {
	switch (state) {
	case SPUDGPU_RESOURCE_STATE_VERTEX_BUFFER:
		return D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
	case SPUDGPU_RESOURCE_STATE_INDEX_BUFFER:
		return D3D12_RESOURCE_STATE_INDEX_BUFFER;
	case SPUDGPU_RESOURCE_STATE_RENDER_TARGET:
		return D3D12_RESOURCE_STATE_RENDER_TARGET;
	case SPUDGPU_RESOURCE_STATE_DEPTH_WRITE:
		return D3D12_RESOURCE_STATE_DEPTH_WRITE;
	case SPUDGPU_RESOURCE_STATE_SHADER_RESOURCE:
		return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	case SPUDGPU_RESOURCE_STATE_UNORDERED_ACCESS:
		return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	case SPUDGPU_RESOURCE_STATE_INDIRECT_ARGUMENT:
		return D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
	case SPUDGPU_RESOURCE_STATE_PRESENT:
		return D3D12_RESOURCE_STATE_PRESENT;
	case SPUDGPU_RESOURCE_STATE_COPY_SOURCE:
		return D3D12_RESOURCE_STATE_COPY_SOURCE;
	case SPUDGPU_RESOURCE_STATE_COPY_DEST:
		return D3D12_RESOURCE_STATE_COPY_DEST;
	case SPUDGPU_RESOURCE_STATE_COMMON:
	default:
		return D3D12_RESOURCE_STATE_COMMON;
	}
}

void spudgpu_cmd_pipeline_barrier(
    spudgpu_command_list cmd,
    const spudgpu_buffer_barrier *buffer_barriers,
    uint32_t buffer_barrier_count,
    const spudgpu_image_barrier *image_barriers,
    uint32_t image_barrier_count) {
	if (!cmd)
		return;
	if (buffer_barrier_count > 0 && !buffer_barriers)
		return;
	if (image_barrier_count > 0 && !image_barriers)
		return;
	uint32_t total = buffer_barrier_count + image_barrier_count;
	if (total == 0)
		return;

	std::vector<D3D12_RESOURCE_BARRIER> barriers;
	barriers.reserve(total);

	for (uint32_t i = 0; i < buffer_barrier_count; i++) {
		const spudgpu_buffer_barrier &b = buffer_barriers[i];
		// UNORDERED_ACCESS to UNORDERED_ACCESS is not a transition and not
		// nothing: it is the header's "writes before are complete before
		// accesses after", which D3D12 has its own barrier type for.
		if (b.state_before == SPUDGPU_RESOURCE_STATE_UNORDERED_ACCESS &&
		    b.state_after == SPUDGPU_RESOURCE_STATE_UNORDERED_ACCESS) {
			barriers.push_back(CD3DX12_RESOURCE_BARRIER::UAV(b.buffer->_d3d_resource.Get()));
			continue;
		}
		D3D12_RESOURCE_STATES before = spudgpu_d3d12___resource_state(b.state_before);
		D3D12_RESOURCE_STATES after = spudgpu_d3d12___resource_state(b.state_after);
		// Any other pair of equal states does nothing, as the header says.
		if (before == after)
			continue;
		barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
		    b.buffer->_d3d_resource.Get(), before, after));
	}
	for (uint32_t i = 0; i < image_barrier_count; i++) {
		const spudgpu_image_barrier &b = image_barriers[i];
		// As for a buffer, above.
		if (b.state_before == SPUDGPU_RESOURCE_STATE_UNORDERED_ACCESS &&
		    b.state_after == SPUDGPU_RESOURCE_STATE_UNORDERED_ACCESS) {
			barriers.push_back(CD3DX12_RESOURCE_BARRIER::UAV(b.image->_d3d_resource.Get()));
			continue;
		}
		D3D12_RESOURCE_STATES before = spudgpu_d3d12___resource_state(b.state_before);
		D3D12_RESOURCE_STATES after = spudgpu_d3d12___resource_state(b.state_after);
		if (before == after)
			continue;
		barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
		    b.image->_d3d_resource.Get(), before, after));
	}

	if (!barriers.empty())
		cmd->_d3d_cmd_list->ResourceBarrier(
		    static_cast<UINT>(barriers.size()), barriers.data());
}
SPUDRESULT spudgpu_queue_submit(
    spudgpu_command_queue queue, const spudgpu_submit_desc *desc) {
	if (!queue)
		return SPUDRESULT_GPU_INVALID_COMMAND_QUEUE;
	if (!desc)
		return SPUDRESULT_NULL_DESC;
	if (!desc->cmd_lists)
		return SPUDRESULT_GPU_INVALID_COMMAND_LIST;
	if (desc->cmd_list_count == 0)
		return SPUDRESULT_ZERO_SIZE;
	// Allocated before the waits are queued, so a failed allocation leaves
	// the queue untouched.
	ID3D12CommandList **d3dLists =
	    (ID3D12CommandList **)malloc(sizeof(ID3D12CommandList *) * desc->cmd_list_count);
	if (!d3dLists)
		return SPUDRESULT_OUT_OF_MEMORY;
	for (uint32_t i = 0; i < desc->wait_semaphore_count; ++i) {
		spudgpu_semaphore sem = desc->wait_semaphores[i];
		queue->_d3d_cmd_queue->Wait(sem->_d3d_fence.Get(), sem->_signal_value);
	}
	for (uint32_t i = 0; i < desc->cmd_list_count; ++i)
		d3dLists[i] = desc->cmd_lists[i]->_d3d_cmd_list.Get();
	queue->_d3d_cmd_queue->ExecuteCommandLists(
	    static_cast<UINT>(desc->cmd_list_count), d3dLists);
	free(d3dLists);

	// Everything below is queued behind the command lists, so it is
	// signaled once they have run.
	SPUDRESULT result = SPUD_SUCCESS;
	for (uint32_t i = 0; i < desc->signal_semaphore_count; ++i) {
		spudgpu_semaphore sem = desc->signal_semaphores[i];
		if (FAILED(queue->_d3d_cmd_queue->Signal(sem->_d3d_fence.Get(), ++sem->_signal_value)))
			result = SPUDRESULT_API_SPECIFIC_FAILURE;
	}
	if (desc->swap_chain) {
		// See spudgpu_swap_chain_d3d12 (spudgpud3d12.hpp) for why this is one
		// monotonic value per submit rather than Vulkan/Metal's
		// per-back-buffer bookkeeping, and
		// spudgpu_swap_chain_acquire_next_image (spudgpud3d12swapchain.cpp)
		// for where it gets waited on.
		uint64_t value = ++desc->swap_chain->_frame_fence_next_value;
		if (FAILED(queue->_d3d_cmd_queue->Signal(desc->swap_chain->_frame_fence.Get(), value)))
			result = SPUDRESULT_API_SPECIFIC_FAILURE;
	}
	if (desc->signal_fence) {
		if (FAILED(queue->_d3d_cmd_queue->Signal(
		        desc->signal_fence->_d3d_fence.Get(), desc->signal_fence_value)))
			result = SPUDRESULT_API_SPECIFIC_FAILURE;
	}
	return result;
}
void spudgpu_queue_wait_idle(spudgpu_command_queue queue) {
	if (!queue)
		return;
	Microsoft::WRL::ComPtr<ID3D12Fence> fence;
	if (FAILED(queue->_device->_d3d_device->CreateFence(
	        0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))))
		return;
	queue->_d3d_cmd_queue->Signal(fence.Get(), 1);
	if (fence->GetCompletedValue() < 1) {
		HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
		fence->SetEventOnCompletion(1, event);
		WaitForSingleObject(event, INFINITE);
		CloseHandle(event);
	}
}

#if SPUDGPU_EXT_BUNDLES
void spudgpu_begin_bundle_command_list(
    spudgpu_command_list bundle, const spudgpu_bundle_inheritance_desc *desc) {
	// D3D12 bundles need no attachment info up front - they inherit whatever
	// render targets are bound on the direct list that executes them.
	if (!bundle || !desc)
		return;
	spudgpu_begin_command_list(bundle);
}

void spudgpu_cmd_execute_bundle(
    spudgpu_command_list cmd, spudgpu_command_list bundle) {
	if (!cmd || !bundle)
		return;
	cmd->_d3d_cmd_list->ExecuteBundle(bundle->_d3d_cmd_list.Get());
}
#endif // SPUDGPU_EXT_BUNDLES
}

#endif // SPUDGPU_COMPILE_D3D12_API
