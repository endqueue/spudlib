
#if SPUDGPU_COMPILE_D3D12_API

#include "spudgpu_d3d12_natives.h"
#include "spudgpud3d12.hpp"

extern "C" {

IDXGIFactory7 *spudgpu_get_dxgi_factory(spudgpu_instance instance) {
    return instance ? instance->_dxgi_factory.Get() : nullptr;
}

IDXGIAdapter4 *spudgpu_get_dxgi_adapter(spudgpu_device device) {
    return device ? device->_dxgi_adapter.Get() : nullptr;
}
ID3D12Device14 *spudgpu_get_d3d12_device(spudgpu_device device) {
    return device ? device->_d3d_device.Get() : nullptr;
}

ID3D12CommandQueue *
spudgpu_get_d3d12_command_queue(spudgpu_command_queue queue) {
    return queue ? queue->_d3d_cmd_queue.Get() : nullptr;
}

ID3D12CommandAllocator *
spudgpu_get_d3d12_command_allocator(spudgpu_command_allocator allocator) {
    return allocator ? allocator->_d3d_cmd_allocator.Get() : nullptr;
}

ID3D12GraphicsCommandList10 *
spudgpu_get_d3d12_graphics_command_list(spudgpu_command_list cmd) {
    return cmd ? cmd->_d3d_cmd_list.Get() : nullptr;
}

ID3D12Resource2 *spudgpu_get_d3d12_buffer(spudgpu_buffer buffer) {
    return buffer ? buffer->_d3d_resource.Get() : nullptr;
}
bool spudgpu_get_d3d12_buffer_view(
    spudgpu_buffer_view view,
    SPUDGPU_D3D12_BUFFER_VIEW *out) {
	if (view && out) {
		*out = view->_d3d_view;
		return true;
	} else
		return false;
}

ID3D12Heap *spudgpu_get_d3d12_heap(spudgpu_heap heap) {
    return heap ? heap->_d3d_heap.Get() : nullptr;
}
ID3D12Resource2 *spudgpu_get_d3d12_image(spudgpu_image image) {
    return image ? image->_d3d_resource.Get() : nullptr;
}
bool spudgpu_get_d3d12_image_view(
    spudgpu_image_view view, SPUDGPU_D3D12_IMAGE_VIEW *out) {
	if (view && out) {
		*out = view->_d3d_view_desc;
		return true;
	} else
		return false;
}

ID3D12RootSignature *spudgpu_get_d3d12_root_signature(spudgpu_shader_pipeline pipeline) {
    return pipeline ? pipeline->_d3d_root_signature.Get() : nullptr;
}
ID3D12PipelineState *spudgpu_get_d3d12_pipeline_state(spudgpu_shader_pipeline pipeline) {
    return pipeline ? pipeline->_d3d_pipeline_state.Get() : nullptr;
}

ID3D12Fence1 *spudgpu_get_d3d12_fence(spudgpu_fence fence) {
    return fence ? fence->_d3d_fence.Get() : nullptr;
}
ID3D12Fence1 *spudgpu_get_d3d12_fence_from_semaphore(spudgpu_semaphore semaphore) {
    return semaphore ? semaphore->_d3d_fence.Get() : nullptr;
}

} // extern "C"

#endif // SPUDGPU_COMPILE_D3D12_API
