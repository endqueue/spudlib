
#ifndef SPUDGPU_D3D12_NATIVES_H
#define SPUDGPU_D3D12_NATIVES_H

#if SPUDGPU_COMPILE_D3D12_API

#include <d3d12.h>
#include <dxgi1_6.h>
#include <spudgpu.h>

#if __cplusplus
extern "C" {
#endif

IDXGIFactory7 *spudgpu_get_dxgi_factory(spudgpu_instance instance);

IDXGIAdapter4 *spudgpu_get_dxgi_adapter(spudgpu_device device);
ID3D12Device14 *spudgpu_get_d3d12_device(spudgpu_device device);

ID3D12CommandQueue *
spudgpu_get_d3d12_command_queue(spudgpu_command_queue queue);

ID3D12CommandAllocator *
spudgpu_get_d3d12_command_allocator(spudgpu_command_allocator allocator);

ID3D12GraphicsCommandList10 *
spudgpu_get_d3d12_graphics_command_list(spudgpu_command_list cmd);

ID3D12Resource2 *spudgpu_get_d3d12_buffer(spudgpu_buffer buffer);
/* Frozen: a fact of the D3D12 API. It never changes; a new need gets a new type. */
typedef union SPUDGPU_D3D12_BUFFER_VIEW {
	D3D12_VERTEX_BUFFER_VIEW _vb;
	D3D12_INDEX_BUFFER_VIEW _ib;
	D3D12_CONSTANT_BUFFER_VIEW_DESC _cb;
	D3D12_STREAM_OUTPUT_BUFFER_VIEW _so;
} SPUDGPU_D3D12_BUFFER_VIEW;
bool spudgpu_get_d3d12_buffer_view(
    spudgpu_buffer_view view,
    SPUDGPU_D3D12_BUFFER_VIEW *out);

ID3D12Resource2 *spudgpu_get_d3d12_image(spudgpu_image image);
/* Frozen: a fact of the D3D12 API. It never changes; a new need gets a new type. */
typedef union SPUDGPU_D3D12_IMAGE_VIEW {
	D3D12_SHADER_RESOURCE_VIEW_DESC _srv;
	D3D12_RENDER_TARGET_VIEW_DESC _rtv;
	D3D12_DEPTH_STENCIL_VIEW_DESC _dsv;
	D3D12_UNORDERED_ACCESS_VIEW_DESC _uav;
} SPUDGPU_D3D12_IMAGE_VIEW;
bool spudgpu_get_d3d12_image_view(
    spudgpu_image_view view,
    SPUDGPU_D3D12_IMAGE_VIEW *out);

ID3D12Heap *spudgpu_get_d3d12_heap(spudgpu_heap heap);

ID3D12RootSignature *spudgpu_get_d3d12_root_signature(spudgpu_shader_pipeline pipeline);
ID3D12PipelineState *spudgpu_get_d3d12_pipeline_state(spudgpu_shader_pipeline pipeline);

ID3D12Fence1 *spudgpu_get_d3d12_fence(spudgpu_fence fence);
ID3D12Fence1 *spudgpu_get_d3d12_fence_from_semaphore(spudgpu_semaphore semaphore);

#if __cplusplus
}
#endif

#endif // SPUDGPU_COMPILE_D3D12_API

#endif // SPUDGPU_D3D12_NATIVES_H
