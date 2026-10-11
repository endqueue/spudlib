
#if SPUDGPU_COMPILE_VULKAN_API

#include <vulkan/vulkan.h>
#include "spudgpu.h"
#include "spudgpuvulkan.h"
#include <stdlib.h>
#include <string.h>

uint32_t spudgpuvulkan___find_memory_type_internal(
    VkPhysicalDevice physicalDevice,
    uint32_t typeFilter,
    VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return (uint32_t) -1;
    //throw std::runtime_error("SpudGPU Vulkan: failed to find suitable memory type!");
}

void spudgpuvulkan___buffer_usage_flags_internal(
    uint32_t __spud_gpu_buffer_usage,
    VkBufferUsageFlags *output) {
    VkBufferUsageFlags *const outputValue = output;
    if (__spud_gpu_buffer_usage & SPUDGPU_BUFFER_USAGE_VERTEX)
        *outputValue |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if (__spud_gpu_buffer_usage & SPUDGPU_BUFFER_USAGE_INDEX)
        *outputValue |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if (__spud_gpu_buffer_usage & SPUDGPU_BUFFER_USAGE_UNIFORM)
        *outputValue |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if (__spud_gpu_buffer_usage & SPUDGPU_BUFFER_USAGE_STORAGE)
        *outputValue |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if (__spud_gpu_buffer_usage & SPUDGPU_BUFFER_USAGE_TRANSFER_SRC)
        *outputValue |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if (__spud_gpu_buffer_usage & SPUDGPU_BUFFER_USAGE_TRANSFER_DST)
        *outputValue |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (__spud_gpu_buffer_usage & SPUDGPU_BUFFER_USAGE_INDIRECT)
        *outputValue |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
}

// The first memory type in typeFilter that has every bit of [properties], and
// that type's own flags. UINT32_MAX if there is none.
static uint32_t spudgpuvulkan___internal_find_memory_type_with_flags(
    const VkPhysicalDeviceMemoryProperties *memProperties,
    uint32_t typeFilter,
    VkMemoryPropertyFlags properties,
    VkMemoryPropertyFlags *out_flags) {
    for (uint32_t i = 0; i < memProperties->memoryTypeCount; i++) {
        if (!(typeFilter & (1u << i))) continue;
        if ((memProperties->memoryTypes[i].propertyFlags & properties) != properties) continue;
        *out_flags = memProperties->memoryTypes[i].propertyFlags;
        return i;
    }
    return UINT32_MAX;
}

// The memory type a buffer of [kind] is allocated from: the properties each
// SPUDGPU_MEMORY_KIND documents in spudgpu.h. Nothing here asks for
// HOST_COHERENT: the caller flushes and invalidates whatever it was given.
static uint32_t spudgpuvulkan___internal_buffer_memory_type(
    VkPhysicalDevice physicalDevice,
    uint32_t typeFilter,
    SPUDGPU_MEMORY_KIND kind,
    VkMemoryPropertyFlags *out_flags) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    switch (kind) {
    case SPUDGPU_MEMORY_KIND_DEVICE:
        return spudgpuvulkan___internal_find_memory_type_with_flags(
            &memProperties, typeFilter,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, out_flags);
    case SPUDGPU_MEMORY_KIND_UPLOAD:
        return spudgpuvulkan___internal_find_memory_type_with_flags(
            &memProperties, typeFilter,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, out_flags);
    case SPUDGPU_MEMORY_KIND_READBACK: {
        // HOST_CACHED is what makes CPU reads fast, and no device has to
        // offer it: without it the contract still holds on plain
        // HOST_VISIBLE memory.
        uint32_t index = spudgpuvulkan___internal_find_memory_type_with_flags(
            &memProperties, typeFilter,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT, out_flags);
        if (index != UINT32_MAX) return index;
        return spudgpuvulkan___internal_find_memory_type_with_flags(
            &memProperties, typeFilter,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, out_flags);
    }
    case SPUDGPU_MEMORY_KIND_DEVICE_MAPPABLE:
        return spudgpuvulkan___internal_find_memory_type_with_flags(
            &memProperties, typeFilter,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, out_flags);
    default:
        return UINT32_MAX;
    }
}

// [offset, offset + size) of a buffer as a range vkFlushMappedMemoryRanges
// and vkInvalidateMappedMemoryRanges accept: widened to multiples of
// nonCoherentAtomSize, and stopped at the end of the allocation. The whole
// allocation is what spudgpu_map_buffer maps, so the widened range is always
// inside the mapping.
static VkMappedMemoryRange spudgpuvulkan___internal_buffer_memory_range(
    spudgpu_buffer buffer,
    uint64_t offset,
    uint64_t size) {
    const VkDeviceSize atom = buffer->_device._properties_vk.limits.nonCoherentAtomSize;
    VkDeviceSize begin = (VkDeviceSize) offset;
    VkDeviceSize end = (VkDeviceSize) (offset + size);
    begin -= begin % atom;
    if (end % atom) end += atom - end % atom;
    if (end > buffer->_memory_size_vk) end = buffer->_memory_size_vk;

    VkMappedMemoryRange range = {0};
    range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    range.pNext = NULL;
    range.memory = buffer->_memory_vk;
    range.offset = begin;
    range.size = end - begin;
    return range;
}

#if __cplusplus
extern "C" {
#endif

SPUDRESULT spudgpu_create_buffer(
    spudgpu_device device,
    const spudgpu_buffer_desc *desc,
    spudgpu_buffer *out_buffer) {
    if (!device) return SPUDRESULT_GPU_INVALID_DEVICE;
    if (!desc) return SPUDRESULT_NULL_DESC;
    if (!out_buffer) return SPUDRESULT_NULL_OUTPUT_PARAMETER;

    // Validate desc
    {
        /*if (desc.gpu_address_location == 0)
            throw std::runtime_error(
                "gpu_buffer_vulkan::gpu_buffer_vulkan: invalid GPU address location: " + std::to_string(
                    desc.gpu_address_location));*/

        if (desc->size == 0)
            //throw std::runtime_error(
            //   "SpudGPU create_buffer: invalid size: " + std::to_string(desc->size));
            return SPUDRESULT_GPU_ZERO_BUFFER_SIZE;

        if (desc->usage == 0)
            //throw std::runtime_error(
            //  "SpudGPU create_buffer: invalid usage: SPUDGPU_BUFFER_USAGE_NONE");
            return SPUDRESULT_GPU_INVALID_BUFFER_USAGE;

        // SPUDGPU_MEMORY_KIND's contract (see spudgpu.h), the same checks
        // in the same order on every backend.
        if (desc->memory_kind < SPUDGPU_MEMORY_KIND_DEVICE || desc->memory_kind > SPUDGPU_MEMORY_KIND_DEVICE_MAPPABLE)
            return SPUDRESULT_GPU_INVALID_MEMORY_KIND;
        if (desc->memory_kind == SPUDGPU_MEMORY_KIND_UPLOAD &&
            (desc->usage & (SPUDGPU_BUFFER_USAGE_STORAGE | SPUDGPU_BUFFER_USAGE_TRANSFER_DST |
                            SPUDGPU_BUFFER_USAGE_RAYTRACING_ACCELERATION_STRUCTURE)))
            return SPUDRESULT_GPU_INVALID_BUFFER_USAGE;
        if (desc->memory_kind == SPUDGPU_MEMORY_KIND_READBACK && desc->usage != SPUDGPU_BUFFER_USAGE_TRANSFER_DST)
            return SPUDRESULT_GPU_INVALID_BUFFER_USAGE;
        if (desc->memory_kind == SPUDGPU_MEMORY_KIND_DEVICE_MAPPABLE && !device->_properties.device_mappable_memory)
            return SPUDRESULT_GPU_MEMORY_KIND_NOT_SUPPORTED;
    }

    // Create the result struct
    spudgpu_buffer_vulkan result = {0};
    result._device = *device;
    result._desc = *desc;

    // Get native Vulkan device handles
    VkDevice vk_device = result._device._logical_device_vk;
    VkPhysicalDevice vk_physical_device = result._device._physical_device_vk;

    VkBufferCreateInfo bufferInfo = {0};
    bufferInfo.pNext = NULL;
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = desc->size;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    // Covert SPUDGPU_BUFFER_USAGE to native Vulkan usage
    spudgpuvulkan___buffer_usage_flags_internal(desc->usage, &bufferInfo.usage);

    // Every spudgpu_buffer gets a valid desc.gpu_address_location after
    // creation (see spudgpu.h), mirroring D3D12's always-available
    // GetGPUVirtualAddress — so every buffer needs this usage bit.
    bufferInfo.usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    // Create buffer
    if (vkCreateBuffer(vk_device, &bufferInfo, NULL, &result._buffer_vk) != VK_SUCCESS) {
        //throw std::runtime_error("SpudGPU Vulkan: failed to create buffer!");
        return SPUDRESULT_API_SPECIFIC_FAILURE;
    }

    // Allocate memory
    {
        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(vk_device, result._buffer_vk, &memRequirements);

        // Fails when the buffer can use no memory type that has the kind's
        // properties.
        const uint32_t memoryTypeIndex = spudgpuvulkan___internal_buffer_memory_type(
            vk_physical_device, memRequirements.memoryTypeBits,
            desc->memory_kind, &result._memory_properties_vk);
        if (memoryTypeIndex == UINT32_MAX) {
            vkDestroyBuffer(vk_device, result._buffer_vk, NULL);
            return SPUDRESULT_API_SPECIFIC_FAILURE;
        }
        result._memory_size_vk = memRequirements.size;

        // Required whenever the buffer was created with
        // VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT (see bufferInfo.usage
        // above) — vkGetBufferDeviceAddress below requires the backing
        // memory to have been allocated with this flag set.
        VkMemoryAllocateFlagsInfo allocFlagsInfo = {0};
        allocFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
        allocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

        VkMemoryAllocateInfo allocInfo = {0};
        allocInfo.pNext = &allocFlagsInfo;
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = memoryTypeIndex;

        if (vkAllocateMemory(vk_device, &allocInfo, NULL, &result._memory_vk) != VK_SUCCESS) {
            //throw std::runtime_error("SpudGPU Vulkan: failed to allocate buffer memory!");
            vkDestroyBuffer(vk_device, result._buffer_vk, NULL);
            return SPUDRESULT_API_SPECIFIC_FAILURE;
        }

        if (vkBindBufferMemory(vk_device, result._buffer_vk, result._memory_vk, 0) != VK_SUCCESS) {
            vkDestroyBuffer(vk_device, result._buffer_vk, NULL);
            vkFreeMemory(vk_device, result._memory_vk, NULL);
            return SPUDRESULT_API_SPECIFIC_FAILURE;
        }
    }

    // Retrieve the GPU Address point for gpu_buffer_desc
    {
        VkBufferDeviceAddressInfoKHR info = {0};
        info.pNext = NULL;
        info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        info.buffer = result._buffer_vk;
        result._desc.gpu_address_location = vkGetBufferDeviceAddress(vk_device, &info);
    }

    // If all successful, return a memcpy'ed heap pointer
    spudgpu_buffer_vulkan *pResult = malloc(sizeof(spudgpu_buffer_vulkan));
    memcpy(pResult, &result, sizeof(spudgpu_buffer_vulkan));
#if _DEBUG
    if (spud_debug_name_set(pResult, desc->debug_name) != SPUD_SUCCESS) {
        spudgpu_destroy_buffer(pResult);
        return SPUDRESULT_OUT_OF_MEMORY;
    }
    pResult->_desc.debug_name = pResult->_debug_name;
#endif

    *out_buffer = pResult;
    return SPUD_SUCCESS;
}

SPUDRESULT spudgpu_get_buffer_desc(
    spudgpu_buffer buffer,
    spudgpu_buffer_desc *out_desc) {
    if (!buffer) return SPUDRESULT_GPU_INVALID_BUFFER;
    if (!out_desc) return SPUDRESULT_NULL_OUTPUT_PARAMETER;
    *out_desc = buffer->_desc;
    return SPUD_SUCCESS;
}

void spudgpu_destroy_buffer(spudgpu_buffer buffer) {
    if (!buffer) return;
    vkDestroyBuffer(buffer->_device._logical_device_vk, buffer->_buffer_vk, NULL);
    vkFreeMemory(buffer->_device._logical_device_vk, buffer->_memory_vk, NULL);
#if _DEBUG
    free((void *)buffer->_debug_name);
#endif
    free(buffer);
}

SPUDRESULT spudgpu_create_buffer_view(
    spudgpu_buffer buffer,
    const spudgpu_buffer_view_desc *desc,
    spudgpu_buffer_view *out_view) {
    if (!buffer) return SPUDRESULT_GPU_INVALID_BUFFER;
    if (!desc) return SPUDRESULT_NULL_DESC;
    if (!out_view) return SPUDRESULT_NULL_OUTPUT_PARAMETER;

	// Validate desc
	if (desc->size == 0)
		return SPUDRESULT_GPU_ZERO_BUFFER_SIZE;

	// Validate that the view range fits within the parent buffer
	if (desc->offset_from_parent_buffer + desc->size > buffer->_desc.size)
		return SPUDRESULT_GPU_BUFFER_OR_IMAGE_VIEW_RANGE_OUT_OF_SCOPE;

	spudgpu_buffer_view_vulkan result = {0};
    result._desc = *desc;
    result._desc.parent_buffer = buffer;
    result._buffer_view_vk = VK_NULL_HANDLE; // Only needed for texel buffers

    /*
     * Vulkan buffer views are not needed for anything other than a texel buffer.
     * This is because buffer handles + offsets are passed directly into Vulkan commands.
     * Unlike D3D12 which has explicit D3D12*_VIEW handles.
     */

    // If all successful, return a memcpy'ed heap pointer
    spudgpu_buffer_view pResult = malloc(sizeof(spudgpu_buffer_view_vulkan));
    memcpy(pResult, &result, sizeof(spudgpu_buffer_view_vulkan));
    *out_view = pResult;
    return SPUD_SUCCESS;
}

SPUDRESULT spudgpu_get_buffer_view_desc(
    spudgpu_buffer_view buffer_view,
    spudgpu_buffer_view_desc *out_desc) {
    if (!buffer_view) return SPUDRESULT_GPU_INVALID_BUFFER_VIEW;
    if (!out_desc) return SPUDRESULT_NULL_OUTPUT_PARAMETER;
    *out_desc = buffer_view->_desc;
    return SPUD_SUCCESS;
}

void spudgpu_destroy_buffer_view(spudgpu_buffer_view buffer_view) {
    if (!buffer_view) return;
	vkDestroyBufferView(
	    buffer_view->_desc.parent_buffer->_device._logical_device_vk,
	    buffer_view->_buffer_view_vk, NULL);
#if _DEBUG
	free((void *)buffer_view->_debug_name);
#endif
	free(buffer_view);
}

SPUDRESULT spudgpu_map_buffer(
    spudgpu_buffer buffer,
    uint64_t offset,
    uint64_t size,
    void **ppData) {
    if (!buffer) return SPUDRESULT_GPU_INVALID_BUFFER;
    //if (!size) return SPUDRESULT_GPU_ZERO_BUFFER_SIZE;
    if (offset + size > buffer->_desc.size) return SPUDRESULT_GPU_MAP_OUT_OF_RANGE;
    if (!ppData) return SPUDRESULT_NULL_OUTPUT_PARAMETER;

    if (buffer->_desc.memory_kind == SPUDGPU_MEMORY_KIND_DEVICE)
        return SPUDRESULT_GPU_INVALID_MEMORY_KIND;

    // The whole allocation is mapped, whatever range was asked for: a flush
    // or invalidate range has to be widened to nonCoherentAtomSize (see
    // spudgpuvulkan___internal_buffer_memory_range) and still lie inside
    // the mapping.
    void *mapped = NULL;
    if (vkMapMemory(
        buffer->_device._logical_device_vk,
        buffer->_memory_vk,
        0,
        VK_WHOLE_SIZE,
        0, // flags — reserved, must be 0
        &mapped) != VK_SUCCESS)
        return SPUDRESULT_API_SPECIFIC_FAILURE;

    *ppData = (uint8_t *) mapped + offset;
	return SPUD_SUCCESS;
}

void spudgpu_unmap_buffer(spudgpu_buffer buffer) {
    if (!buffer) return;
    vkUnmapMemory(
        buffer->_device._logical_device_vk,
        buffer->_memory_vk);
}

SPUDRESULT spudgpu_flush_buffer(
    spudgpu_buffer buffer,
    uint64_t offset,
    uint64_t size) {
    if (!buffer) return SPUDRESULT_GPU_INVALID_BUFFER;
    if (buffer->_desc.memory_kind != SPUDGPU_MEMORY_KIND_UPLOAD &&
        buffer->_desc.memory_kind != SPUDGPU_MEMORY_KIND_DEVICE_MAPPABLE)
        return SPUDRESULT_GPU_INVALID_MEMORY_KIND;
    if (offset + size > buffer->_desc.size) return SPUDRESULT_GPU_MAP_OUT_OF_RANGE;

    // Coherent memory has nothing to flush.
    if (buffer->_memory_properties_vk & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
        return SPUD_SUCCESS;

    const VkMappedMemoryRange range = spudgpuvulkan___internal_buffer_memory_range(
        buffer, offset, (size == 0) ? buffer->_desc.size - offset : size);
    if (vkFlushMappedMemoryRanges(
        buffer->_device._logical_device_vk,
        1, &range) != VK_SUCCESS)
        return SPUDRESULT_API_SPECIFIC_FAILURE;

    return SPUD_SUCCESS;
}

SPUDRESULT spudgpu_invalidate_buffer(
    spudgpu_buffer buffer,
    uint64_t offset,
    uint64_t size) {
    if (!buffer) return SPUDRESULT_GPU_INVALID_BUFFER;
    if (buffer->_desc.memory_kind != SPUDGPU_MEMORY_KIND_READBACK &&
        buffer->_desc.memory_kind != SPUDGPU_MEMORY_KIND_DEVICE_MAPPABLE)
        return SPUDRESULT_GPU_INVALID_MEMORY_KIND;
    if (offset + size > buffer->_desc.size) return SPUDRESULT_GPU_MAP_OUT_OF_RANGE;

    // Coherent memory has nothing to invalidate.
    if (buffer->_memory_properties_vk & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
        return SPUD_SUCCESS;

    const VkMappedMemoryRange range = spudgpuvulkan___internal_buffer_memory_range(
        buffer, offset, (size == 0) ? buffer->_desc.size - offset : size);
    if (vkInvalidateMappedMemoryRanges(
        buffer->_device._logical_device_vk,
        1, &range) != VK_SUCCESS)
        return SPUDRESULT_API_SPECIFIC_FAILURE;

    return SPUD_SUCCESS;
}

#if __cplusplus
}
#endif

#endif // SPUDGPU_COMPILE_VULKAN_API
