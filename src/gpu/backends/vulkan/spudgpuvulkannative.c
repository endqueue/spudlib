#if SPUDGPU_COMPILE_VULKAN_API

#include "spudgpuvulkan.h"
#include <stdint.h>

#if __cplusplus
extern "C" {

#endif

VkInstance spudgpu_get_vk_instance(spudgpu_instance instance) {
    return instance ? instance->_instance_vk : NULL;
}

VkPhysicalDevice spudgpu_get_vk_physical_device(spudgpu_device device) {
    return device ? device->_physical_device_vk : NULL;
}

VkDevice spudgpu_get_vk_device(spudgpu_device device) {
    return device ? device->_logical_device_vk : NULL;
}

VkQueue spudgpu_get_vk_queue(spudgpu_command_queue queue) {
    return queue ? queue->_queue_vk : NULL;
}

uint32_t spudgpu_get_vk_queue_family_index(spudgpu_command_queue queue) {
    return queue ? queue->_queue_family_index : UINT32_MAX;
}

VkCommandBuffer spudgpu_get_vk_command_buffer(spudgpu_command_list cmd) {
    return cmd ? cmd->_command_buffer_vk : NULL;
}

VkImageView spudgpu_get_vk_image_view(spudgpu_image_view view) {
    if (!view) return NULL;
    // VkImageView is a non-dispatchable handle (uint64_t in the Vulkan spec).
    // The reinterpret through uintptr_t is lossless on 64-bit targets.
    return ((spudgpu_image_view_vulkan *) view)->_image_view_vk;
}

VkDeviceMemory spudgpu_get_vk_heap_memory(spudgpu_heap heap) {
    return heap ? heap->_memory_vk : VK_NULL_HANDLE;
}

VkFormat spudgpu_get_vk_swap_chain_format(spudgpu_swap_chain swap_chain) {
    return swap_chain ? swap_chain->_format_vk : VK_FORMAT_UNDEFINED;
}

#if __cplusplus
}
#endif

#endif // SPUDGPU_COMPILE_VULKAN_API
