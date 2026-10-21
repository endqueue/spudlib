//
// Created by Nathan on 5/23/2026.
//

#ifndef SPUDLIB_SPUDGPU_VULKAN_NATIVES_H
#define SPUDLIB_SPUDGPU_VULKAN_NATIVES_H

#if SPUDGPU_COMPILE_VULKAN_API

#include <vulkan/vulkan.h>
#include <spudgpu.h>

#ifdef __cplusplus
extern "C" {

#endif

/* Escape hatch: returns raw Vulkan handles from SpudGPU opaque objects.
 * Use only for interop with third-party Vulkan libraries (e.g. ImGui, RenderDoc).
 * SpudGPU has no visibility into Vulkan calls made with these handles.
 */

VkInstance spudgpu_get_vk_instance(spudgpu_instance instance);

VkPhysicalDevice spudgpu_get_vk_physical_device(spudgpu_device device);

VkDevice spudgpu_get_vk_device(spudgpu_device device);

/* A VkQueue is externally synchronized, and SpudGPU's own submits, presents
 * and waits on it hold the queue's lock. A caller that submits, presents or
 * waits on the VkQueue itself (an ImGui or OpenXR backend, say) does it
 * between spudgpu_vk_queue_lock() and spudgpu_vk_queue_unlock() on the same
 * spudgpu_command_queue, and calls no SpudGPU function on that queue in
 * between: the lock is not recursive. */
VkQueue spudgpu_get_vk_queue(spudgpu_command_queue queue);

/* Takes the queue's lock, waiting while another thread holds it. NULL does
 * nothing. */
void spudgpu_vk_queue_lock(spudgpu_command_queue queue);

/* Releases the lock spudgpu_vk_queue_lock() took, on the same thread. NULL
 * does nothing. */
void spudgpu_vk_queue_unlock(spudgpu_command_queue queue);

uint32_t spudgpu_get_vk_queue_family_index(spudgpu_command_queue queue);

VkCommandBuffer spudgpu_get_vk_command_buffer(spudgpu_command_list cmd);

VkImageView spudgpu_get_vk_image_view(spudgpu_image_view view);

/* The one allocation a heap is, which its placed buffers and images are
 * bound to at their offsets. A heap of a mappable kind keeps this memory
 * mapped for as long as it exists, so it is never mapped again with
 * vkMapMemory. */
VkDeviceMemory spudgpu_get_vk_heap_memory(spudgpu_heap heap);

VkFormat spudgpu_get_vk_swap_chain_format(spudgpu_swap_chain swap_chain);

#ifdef __cplusplus
}
#endif

#endif // SPUDGPU_COMPILE_VULKAN_API

#endif //SPUDLIB_SPUDGPU_VULKAN_NATIVES_H
