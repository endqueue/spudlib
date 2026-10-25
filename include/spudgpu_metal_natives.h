/**
 * @file spudgpu_metal_natives.h
 * @brief Escape hatch: the raw Metal objects behind SpudGPU's opaque
 * objects.
 *
 * Use only for interop with third-party Metal libraries (e.g. ImGui,
 * MetalFX). SpudGPU has no visibility into Metal calls made with these
 * handles. Declared only when SPUDGPU_COMPILE_METAL_API is set.
 *
 * Every accessor returns an object SpudGPU still owns: it stays valid for as
 * long as the SpudGPU object it came from. Each returns nil (or the value
 * its own doc names) when given a NULL object.
 *
 * Metal's object model doesn't line up 1:1 with the Vulkan/D3D12 natives
 * headers in a few places - see the notes below rather than assuming a
 * missing accessor here is an oversight:
 *
 * - No spudgpu_get_mtl_instance(): Metal has no top-level instance/factory
 *   object (unlike VkInstance or IDXGIFactory). Devices are enumerated
 *   directly via MTLCopyAllDevices(), so spudgpu_instance has no native
 *   Metal handle to hand back.
 * - No spudgpu_get_mtl_queue_family_index(): Metal command queues aren't
 *   partitioned into families the way Vulkan queues are.
 * - No native accessor for spudgpu_command_allocator: Metal has no
 *   persistent allocator/pool object to reset between frames - command
 *   buffers are requested fresh from the queue each time.
 * - No spudgpu_get_mtl_root_signature(): Metal has no runtime
 *   root-signature / pipeline-layout object - binding indices come from
 *   [[buffer(n)]] / [[texture(n)]] attributes baked into the shader source
 *   itself.
 */

#ifndef SPUDLIB_SPUDGPU_METAL_NATIVES_H
#define SPUDLIB_SPUDGPU_METAL_NATIVES_H

#if SPUDGPU_COMPILE_METAL_API

#include <Metal/Metal.h>
#include <spudgpu.h>

#ifdef __cplusplus
extern "C" {

#endif

/**
 * @brief Returns the id<MTLDevice> behind @p device.
 *
 * @param[in] device The SpudGPU device.
 *
 * @return Its id<MTLDevice>, or nil if @p device is NULL.
 *
 * @see spudgpu_get_mtl_adapter_device()
 */
id<MTLDevice> spudgpu_get_mtl_device(spudgpu_device device);

/**
 * @brief Returns the id<MTLDevice> behind @p adapter.
 *
 * No physical/logical device split: id<MTLDevice> already covers both
 * roles Vulkan splits into VkPhysicalDevice/VkDevice, so an adapter and the
 * device created on it hand back the same id<MTLDevice>.
 *
 * @param[in] adapter The adapter, as spudgpu_enumerate_adapters() gave it.
 *
 * @return Its id<MTLDevice>, or nil if @p adapter is NULL.
 */
id<MTLDevice> spudgpu_get_mtl_adapter_device(spudgpu_adapter adapter);

/**
 * @brief Returns the id<MTLCommandQueue> behind @p queue.
 *
 * The caller synchronizes a queue (see spudgpu_submit_command_lists()), and
 * a use of the id<MTLCommandQueue> itself is one more use of the same queue
 * under the same rule.
 *
 * @param[in] queue The SpudGPU command queue.
 *
 * @return Its id<MTLCommandQueue>, or nil if @p queue is NULL.
 */
id<MTLCommandQueue> spudgpu_get_mtl_command_queue(spudgpu_command_queue queue);

/**
 * @brief Returns the id<MTLCommandBuffer> behind @p cmd.
 *
 * @param[in] cmd The SpudGPU command list.
 *
 * @return Its id<MTLCommandBuffer>, or nil if @p cmd is NULL.
 */
id<MTLCommandBuffer> spudgpu_get_mtl_command_buffer(spudgpu_command_list cmd);

/**
 * @brief Returns the id<MTLBuffer> behind @p buffer.
 *
 * @param[in] buffer The SpudGPU buffer.
 *
 * @return Its id<MTLBuffer>, or nil if @p buffer is NULL.
 */
id<MTLBuffer> spudgpu_get_mtl_buffer(spudgpu_buffer buffer);

/**
 * @brief What a SpudGPU buffer view is on Metal, as filled in by
 * spudgpu_get_mtl_buffer_view().
 *
 * Metal has no separate buffer-view object (Vulkan's VkBufferView, D3D12's
 * `*_BUFFER_VIEW` descs) - buffers are bound directly with an offset/stride
 * at bind time. This struct carries the same information out of SpudGPU's
 * own view abstraction instead of a native handle.
 */
/* Frozen: a fact of the Metal API. It never changes; a new need gets a new type. */
typedef struct SPUDGPU_MTL_BUFFER_VIEW {
	/** The parent buffer's id<MTLBuffer>. */
	id<MTLBuffer> buffer;
	/** Byte offset of the view in #buffer. */
	NSUInteger offset;
	/** Size of the view in bytes. */
	NSUInteger length;
	/** Byte stride of one element, as the view was created with. */
	NSUInteger stride;
} SPUDGPU_MTL_BUFFER_VIEW;
/**
 * @brief Fills @p out with the buffer, offset, length and stride of @p view.
 *
 * @param[in]  view The SpudGPU buffer view.
 * @param[out] out  Receives the view.
 *
 * @return true if @p out was written; false if @p view or @p out is NULL or
 *         the view has no parent buffer.
 */
bool spudgpu_get_mtl_buffer_view(
    spudgpu_buffer_view view,
    SPUDGPU_MTL_BUFFER_VIEW *out);

/**
 * @brief Returns the id<MTLTexture> behind @p image.
 *
 * @param[in] image The SpudGPU image.
 *
 * @return Its id<MTLTexture>, or nil if @p image is NULL.
 *
 * @see spudgpu_get_mtl_texture_view()
 */
id<MTLTexture> spudgpu_get_mtl_texture(spudgpu_image image);

/**
 * @brief Returns the id<MTLTexture> behind the image view @p view.
 *
 * Metal represents a "view of a texture" (reinterpreted format/mip/array
 * range, via -newTextureViewWithPixelFormat:...) as another
 * id<MTLTexture>, not a distinct lightweight descriptor type the way
 * Vulkan's VkImageView or D3D12's `*_VIEW_DESC` structs are - so this
 * returns the same native type as spudgpu_get_mtl_texture(), just for the
 * view object rather than the base image.
 *
 * @param[in] view The SpudGPU image view.
 *
 * @return The view's id<MTLTexture>, or nil if @p view is NULL.
 */
id<MTLTexture> spudgpu_get_mtl_texture_view(spudgpu_image_view view);

/**
 * @brief Returns the id<MTLHeap> behind @p heap.
 *
 * A placement heap (MTLHeapTypePlacement), hazard tracked: see
 * spudgpu_create_heap().
 *
 * @param[in] heap The SpudGPU heap.
 *
 * @return Its id<MTLHeap>, or nil if @p heap is NULL.
 */
id<MTLHeap> spudgpu_get_mtl_heap(spudgpu_heap heap);

/**
 * @brief Returns the id<MTLRenderPipelineState> behind @p pipeline.
 *
 * @param[in] pipeline The SpudGPU shader pipeline.
 *
 * @return Its id<MTLRenderPipelineState>, or nil if @p pipeline is NULL.
 */
id<MTLRenderPipelineState>
spudgpu_get_mtl_render_pipeline_state(spudgpu_shader_pipeline pipeline);

/**
 * @brief Returns the id<MTLSharedEvent> behind @p fence.
 *
 * Neither fence nor semaphore comes back as MTLFence: plain MTLFence only
 * tracks hazards between encoders within a single command buffer/queue and
 * can't be waited on from the CPU or across queues, so it can't stand in for
 * either Vulkan/D3D12 concept - that role belongs to a pipeline barrier, not
 * a fence or semaphore.
 *
 * spudgpu_fence needs CPU wait/signal/read (spudgpu_wait_for_fences(),
 * spudgpu_signal_fence(), spudgpu_get_fence_value()) plus cross-process
 * sharing (SPUDGPU_FENCE_FLAG_SHARED), so it comes back as
 * id<MTLSharedEvent>.
 *
 * @param[in] fence The SpudGPU fence.
 *
 * @return Its id<MTLSharedEvent>, or nil if @p fence is NULL.
 *
 * @see spudgpu_get_mtl_event_from_semaphore()
 */
id<MTLSharedEvent> spudgpu_get_mtl_shared_event(spudgpu_fence fence);
/**
 * @brief Returns the id<MTLEvent> behind @p semaphore.
 *
 * spudgpu_semaphore's public API never exposes a CPU-side accessor - only
 * GPU wait/signal across queue submissions - so it comes back as plain
 * id<MTLEvent>, MTLSharedEvent's CPU-visibility-free base class, instead of
 * id<MTLSharedEvent> (see spudgpu_get_mtl_shared_event()).
 *
 * @param[in] semaphore The SpudGPU semaphore.
 *
 * @return Its id<MTLEvent>, or nil if @p semaphore is NULL.
 */
id<MTLEvent> spudgpu_get_mtl_event_from_semaphore(spudgpu_semaphore semaphore);

/**
 * @brief Returns the MTLPixelFormat of @p swap_chain's images.
 *
 * The CAMetalLayer's pixelFormat, which spudgpu_create_swap_chain() set
 * from the swap chain's SPUDGPU_FORMAT.
 *
 * @param[in] swap_chain The SpudGPU swap chain.
 *
 * @return The pixel format, or MTLPixelFormatInvalid if @p swap_chain is
 *         NULL.
 */
MTLPixelFormat spudgpu_get_mtl_swap_chain_format(spudgpu_swap_chain swap_chain);

#ifdef __cplusplus
}
#endif

#endif // SPUDGPU_COMPILE_METAL_API

#endif //SPUDLIB_SPUDGPU_METAL_NATIVES_H
