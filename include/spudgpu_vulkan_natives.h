//
// Created by Nathan on 5/23/2026.
//

/**
 * @file spudgpu_vulkan_natives.h
 * @brief Escape hatch: the raw Vulkan handles behind SpudGPU's opaque
 * objects.
 *
 * Use only for interop with third-party Vulkan libraries (e.g. ImGui,
 * RenderDoc). SpudGPU has no visibility into Vulkan calls made with these
 * handles. Declared only when SPUDGPU_COMPILE_VULKAN_API is set.
 *
 * Every accessor returns a handle SpudGPU still owns: it stays valid for as
 * long as the SpudGPU object it came from, and the caller never destroys it.
 * Each returns NULL (VK_NULL_HANDLE, or the value its own doc names) when
 * given a NULL object.
 */

#ifndef SPUDLIB_SPUDGPU_VULKAN_NATIVES_H
#define SPUDLIB_SPUDGPU_VULKAN_NATIVES_H

#if SPUDGPU_COMPILE_VULKAN_API

#include <vulkan/vulkan.h>
#include <spudgpu.h>

#ifdef __cplusplus
extern "C" {

#endif

/**
 * @brief Returns the VkInstance behind @p instance.
 *
 * @param[in] instance The SpudGPU instance.
 *
 * @return Its VkInstance, or NULL if @p instance is NULL.
 */
VkInstance spudgpu_get_vk_instance(spudgpu_instance instance);

/**
 * @brief Returns the VkPhysicalDevice behind @p adapter.
 *
 * An adapter is a VkPhysicalDevice.
 *
 * @param[in] adapter The adapter, as spudgpu_enumerate_adapters() gave it.
 *
 * @return Its VkPhysicalDevice, or NULL if @p adapter is NULL.
 */
VkPhysicalDevice spudgpu_get_vk_physical_device(spudgpu_adapter adapter);

/**
 * @brief Returns the VkDevice behind @p device.
 *
 * @param[in] device The SpudGPU device.
 *
 * @return Its VkDevice, or NULL if @p device is NULL.
 */
VkDevice spudgpu_get_vk_device(spudgpu_device device);

/**
 * @brief Returns the VkQueue behind @p queue.
 *
 * The caller synchronizes a queue (see spudgpu_submit_command_lists()), and
 * a use of the VkQueue itself - a submit, present or wait by an ImGui or
 * OpenXR backend, say - is one more use of the same queue under the same
 * rule.
 *
 * @param[in] queue The SpudGPU command queue.
 *
 * @return Its VkQueue, or NULL if @p queue is NULL.
 *
 * @see spudgpu_get_vk_queue_family_index()
 */
VkQueue spudgpu_get_vk_queue(spudgpu_command_queue queue);

/**
 * @brief Returns the Vulkan queue family @p queue was created from.
 *
 * @param[in] queue The SpudGPU command queue.
 *
 * @return The queue family index, or UINT32_MAX if @p queue is NULL.
 */
uint32_t spudgpu_get_vk_queue_family_index(spudgpu_command_queue queue);

/**
 * @brief Returns the VkCommandBuffer behind @p cmd.
 *
 * @param[in] cmd The SpudGPU command list.
 *
 * @return Its VkCommandBuffer, or NULL if @p cmd is NULL.
 */
VkCommandBuffer spudgpu_get_vk_command_buffer(spudgpu_command_list cmd);

/**
 * @brief Returns the VkImageView behind @p view.
 *
 * @param[in] view The SpudGPU image view.
 *
 * @return Its VkImageView, or VK_NULL_HANDLE if @p view is NULL.
 */
VkImageView spudgpu_get_vk_image_view(spudgpu_image_view view);

/**
 * @brief Returns the VkDeviceMemory a heap is.
 *
 * The one allocation a heap is, which its placed buffers and images are
 * bound to at their offsets. A heap of a mappable kind keeps this memory
 * mapped for as long as it exists, so it is never mapped again with
 * vkMapMemory.
 *
 * @param[in] heap The SpudGPU heap.
 *
 * @return Its VkDeviceMemory, or VK_NULL_HANDLE if @p heap is NULL.
 *
 * @see spudgpu_create_heap()
 */
VkDeviceMemory spudgpu_get_vk_heap_memory(spudgpu_heap heap);

/**
 * @brief Returns the VkFormat of @p swap_chain's images.
 *
 * @param[in] swap_chain The SpudGPU swap chain.
 *
 * @return Its images' VkFormat, or VK_FORMAT_UNDEFINED if @p swap_chain is
 *         NULL.
 */
VkFormat spudgpu_get_vk_swap_chain_format(spudgpu_swap_chain swap_chain);

#ifdef __cplusplus
}
#endif

#endif // SPUDGPU_COMPILE_VULKAN_API

#endif //SPUDLIB_SPUDGPU_VULKAN_NATIVES_H
