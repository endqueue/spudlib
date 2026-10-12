//
// SpudGPU Metal backend - implements the escape-hatch accessors declared in
// include/spudgpu_metal_natives.h.
// Not yet implemented: no Apple hardware available to develop/verify against.
// Wired into the spudlib CMake target ahead of time - see CMakeLists.txt.
//

#if SPUDGPU_COMPILE_METAL_API

#include "spudgpu.h"
#include "spudcore.h"
#include "spudgpumetal.h"
#include "spudgpu_metal_natives.h"
#include <Metal/Metal.h>

id<MTLDevice> spudgpu_get_mtl_device(spudgpu_device device) {
	spudgpu_device_metal *device_metal = (spudgpu_device_metal *)device;
	if (!device_metal)
		return nil;
	return device_metal->_device_mtl;
}

id<MTLCommandQueue> spudgpu_get_mtl_command_queue(spudgpu_command_queue queue) {
	spudgpu_command_queue_metal *queue_metal = (spudgpu_command_queue_metal *)queue;
	if (!queue_metal)
		return nil;
	return queue_metal->_command_queue_mtl;
}

id<MTLCommandBuffer> spudgpu_get_mtl_command_buffer(spudgpu_command_list cmd) {
	spudgpu_command_list_metal *cmd_metal = (spudgpu_command_list_metal *)cmd;
	if (!cmd_metal)
		return nil;
	return cmd_metal->_command_buffer_mtl;
}

id<MTLBuffer> spudgpu_get_mtl_buffer(spudgpu_buffer buffer) {
	spudgpu_buffer_metal *buffer_metal = (spudgpu_buffer_metal *)buffer;
	if (!buffer_metal)
		return nil;
	return buffer_metal->_buffer_mtl;
}

bool spudgpu_get_mtl_buffer_view(
    spudgpu_buffer_view view,
    SPUDGPU_MTL_BUFFER_VIEW *out) {
	spudgpu_buffer_view_metal *view_metal = (spudgpu_buffer_view_metal *)view;
	if (!view_metal || !out)
		return false;
	spudgpu_buffer_metal *parent_buffer = (spudgpu_buffer_metal *)view_metal->_desc.parent_buffer;
	if (!parent_buffer)
		return false;

	out->buffer = parent_buffer->_buffer_mtl;
	out->offset = (NSUInteger)view_metal->_desc.offset_from_parent_buffer;
	out->length = (NSUInteger)view_metal->_desc.size;
	out->stride = (NSUInteger)view_metal->_desc.stride;
	return true;
}

id<MTLTexture> spudgpu_get_mtl_texture(spudgpu_image image) {
	spudgpu_image_metal *image_metal = (spudgpu_image_metal *)image;
	if (!image_metal)
		return nil;
	return image_metal->_texture_mtl;
}

id<MTLHeap> spudgpu_get_mtl_heap(spudgpu_heap heap) {
	spudgpu_heap_metal *heap_metal = (spudgpu_heap_metal *)heap;
	if (!heap_metal)
		return nil;
	return heap_metal->_heap_mtl;
}

id<MTLTexture> spudgpu_get_mtl_texture_view(spudgpu_image_view view) {
	spudgpu_image_view_metal *view_metal = (spudgpu_image_view_metal *)view;
	if (!view_metal)
		return nil;
	return view_metal->_texture_view_mtl;
}

id<MTLRenderPipelineState> spudgpu_get_mtl_render_pipeline_state(spudgpu_shader_pipeline pipeline) {
	spudgpu_shader_pipeline_metal *pipeline_metal = (spudgpu_shader_pipeline_metal *)pipeline;
	if (!pipeline_metal)
		return nil;
	return pipeline_metal->_render_pipeline_state_mtl;
}

id<MTLSharedEvent> spudgpu_get_mtl_shared_event(spudgpu_fence fence) {
	spudgpu_fence_metal *fence_metal = (spudgpu_fence_metal *)fence;
	if (!fence_metal)
		return nil;
	return fence_metal->_shared_event_mtl;
}

id<MTLEvent> spudgpu_get_mtl_event_from_semaphore(spudgpu_semaphore semaphore) {
	spudgpu_semaphore_metal *semaphore_metal = (spudgpu_semaphore_metal *)semaphore;
	if (!semaphore_metal)
		return nil;
	return semaphore_metal->_event_mtl;
}

MTLPixelFormat spudgpu_get_mtl_swap_chain_format(spudgpu_swap_chain swap_chain) {
	spudgpu_swap_chain_metal *swap_chain_metal = (spudgpu_swap_chain_metal *)swap_chain;
	if (!swap_chain_metal)
		return MTLPixelFormatInvalid;

	// METAL API CODE
	// Needs a SPUDGPU_FORMAT -> MTLPixelFormat table (mirroring the D3D12
	// backend's spudgpu_d3d12_get_dxgi_format) before this can translate
	// swap_chain_metal->_desc.format for real.

	return MTLPixelFormatInvalid;
}

#endif // SPUDGPU_COMPILE_METAL_API
