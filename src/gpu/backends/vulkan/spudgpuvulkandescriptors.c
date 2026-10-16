
#if SPUDGPU_COMPILE_VULKAN_API

#include <vulkan/vulkan.h>
#include "spudgpu.h"
#include "spudgpuvulkan.h"
#include <stdlib.h>
#include <string.h>

#if __cplusplus
extern "C" {

#endif

static VkDescriptorType spudgpuvulkan___descriptor_type_internal(
    SPUDGPU_DESCRIPTOR_TYPE type) {
    switch (type) {
        case SPUDGPU_DESCRIPTOR_TYPE_UNIFORM_BUFFER: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case SPUDGPU_DESCRIPTOR_TYPE_STORAGE_BUFFER: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case SPUDGPU_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        case SPUDGPU_DESCRIPTOR_TYPE_SAMPLED_IMAGE: return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        case SPUDGPU_DESCRIPTOR_TYPE_SAMPLER: return VK_DESCRIPTOR_TYPE_SAMPLER;
        case SPUDGPU_DESCRIPTOR_TYPE_STORAGE_IMAGE: return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        default: return VK_DESCRIPTOR_TYPE_MAX_ENUM;
    }
}

static VkFilter spudgpuvulkan___filter_internal(SPUDGPU_FILTER filter) {
    return filter == SPUDGPU_FILTER_LINEAR ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
}

static VkSamplerMipmapMode spudgpuvulkan___mipmap_mode_internal(SPUDGPU_FILTER filter) {
    return filter == SPUDGPU_FILTER_LINEAR ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
}

static VkSamplerAddressMode spudgpuvulkan___address_mode_internal(SPUDGPU_ADDRESS_MODE mode) {
    switch (mode) {
        case SPUDGPU_ADDRESS_MODE_MIRRORED_REPEAT: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case SPUDGPU_ADDRESS_MODE_CLAMP_TO_EDGE: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case SPUDGPU_ADDRESS_MODE_CLAMP_TO_BORDER: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        case SPUDGPU_ADDRESS_MODE_REPEAT:
        default: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }
}

SPUDRESULT spudgpu_create_sampler(
    spudgpu_device device,
    const spudgpu_sampler_desc *desc,
    spudgpu_sampler *out_sampler) {
    if (!device) return SPUDRESULT_GPU_INVALID_DEVICE;
    if (!desc) return SPUDRESULT_NULL_DESC;
    if (!out_sampler) return SPUDRESULT_NULL_OUTPUT_PARAMETER;

    VkSamplerCreateInfo info = {0};
    info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    info.magFilter = spudgpuvulkan___filter_internal(desc->mag_filter);
    info.minFilter = spudgpuvulkan___filter_internal(desc->min_filter);
    info.mipmapMode = spudgpuvulkan___mipmap_mode_internal(desc->mipmap_filter);
    info.addressModeU = spudgpuvulkan___address_mode_internal(desc->address_mode_u);
    info.addressModeV = spudgpuvulkan___address_mode_internal(desc->address_mode_v);
    info.addressModeW = spudgpuvulkan___address_mode_internal(desc->address_mode_w);
    info.mipLodBias = desc->mip_lod_bias;
    info.minLod = desc->min_lod;
    info.maxLod = desc->max_lod;
    info.anisotropyEnable = desc->max_anisotropy > 1.0f ? VK_TRUE : VK_FALSE;
    info.maxAnisotropy = desc->max_anisotropy;
    info.compareEnable = VK_FALSE;
    info.compareOp = VK_COMPARE_OP_ALWAYS;
    info.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;

    spudgpu_sampler_vulkan *pResult = calloc(1, sizeof(spudgpu_sampler_vulkan));
    pResult->_device = *device;

    if (vkCreateSampler(device->_logical_device_vk, &info, NULL, &pResult->_sampler_vk) != VK_SUCCESS) {
        free(pResult);
        return SPUDRESULT_API_SPECIFIC_FAILURE;
    }

#if _DEBUG
    if (spud_debug_name_set(pResult, desc->debug_name) != SPUD_SUCCESS) {
        spudgpu_destroy_sampler(pResult);
        return SPUDRESULT_OUT_OF_MEMORY;
    }
#endif

    *out_sampler = pResult;
    return SPUD_SUCCESS;
}

void spudgpu_destroy_sampler(spudgpu_sampler sampler) {
    if (!sampler) return;
    vkDestroySampler(sampler->_device._logical_device_vk, sampler->_sampler_vk, NULL);
#if _DEBUG
    free((void *)sampler->_debug_name);
#endif
    free(sampler);
}

SPUDRESULT spudgpu_create_descriptor_set_layout(
    spudgpu_device device,
    const spudgpu_descriptor_set_layout_desc *desc,
	spudgpu_descriptor_set_layout *out_layout) {
    if (!device) return SPUDRESULT_GPU_INVALID_DEVICE;
    if (!desc) return SPUDRESULT_NULL_DESC;
    if (!out_layout) return SPUDRESULT_NULL_OUTPUT_PARAMETER;
    if (desc->binding_count > SPUDGPU_MAX_DESCRIPTOR_BINDINGS_PER_SET) return SPUDRESULT_GPU_TOO_MANY_DESCRIPTOR_BINDINGS;

    spudgpu_descriptor_set_layout_vulkan result = {0};
    result._device = *device;
    result._desc = *desc;

    VkDevice vk_device = result._device._logical_device_vk;

    // Translate each binding entry to VkDescriptorSetLayoutBinding.
    VkDescriptorSetLayoutBinding vk_bindings[SPUDGPU_MAX_DESCRIPTOR_BINDINGS_PER_SET] = {0};
    for (uint32_t i = 0; i < desc->binding_count; i++) {
        const spudgpu_descriptor_binding_desc *b = &desc->bindings[i];

        VkDescriptorType vk_type = spudgpuvulkan___descriptor_type_internal(b->descriptor_type);
        if (vk_type == VK_DESCRIPTOR_TYPE_MAX_ENUM) return SPUDRESULT_GPU_CANNOT_RESOLVE_API_SPECIFIC_DESCRIPTOR_TYPE; // Unknown type — bail early

        vk_bindings[i].binding = b->binding;
        vk_bindings[i].descriptorType = vk_type;
        vk_bindings[i].descriptorCount = b->count;
        // Translated: SpudGPU's stage bits are not Vulkan's (FRAGMENT is
        // 1 << 1, which is VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT).
        vk_bindings[i].stageFlags = spudgpuvulkan___shader_stages(b->stage_flags);
        // Immutable samplers aren't supported yet — see the "no static/immutable
        // sampler support" gap in ../../../CLAUDE.md. Every sampler goes through
        // the dynamic, descriptor-bound spudgpu_sampler path today.
        vk_bindings[i].pImmutableSamplers = NULL;
    }

    VkDescriptorSetLayoutCreateInfo layoutInfo = {0};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = desc->binding_count;
    layoutInfo.pBindings = vk_bindings;

    if (vkCreateDescriptorSetLayout(
        vk_device, &layoutInfo, NULL,
        &result._layout_vk) != VK_SUCCESS) {
        return SPUDRESULT_API_SPECIFIC_FAILURE;
    }

#if _DEBUG
    // If VK_EXT_debug_utils is loaded, tag the layout object for RenderDoc / validation layers.
    if (desc->debug_name) {
        VkDebugUtilsObjectNameInfoEXT nameInfo = {0};
        nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        nameInfo.objectType = VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT;
        nameInfo.objectHandle = (uint64_t) result._layout_vk;
        nameInfo.pObjectName = desc->debug_name;
        // Best-effort: ignore the return value — not all drivers load the extension.
        PFN_vkSetDebugUtilsObjectNameEXT fn =
                (PFN_vkSetDebugUtilsObjectNameEXT) vkGetDeviceProcAddr(
                    vk_device, "vkSetDebugUtilsObjectNameEXT");
        if (fn) fn(vk_device, &nameInfo);
    }
#endif

    spudgpu_descriptor_set_layout_vulkan *pResult =
            malloc(sizeof(spudgpu_descriptor_set_layout_vulkan));
    memcpy(pResult, &result, sizeof(spudgpu_descriptor_set_layout_vulkan));
#if _DEBUG
    if (spud_debug_name_set(pResult, desc->debug_name) != SPUD_SUCCESS) {
        spudgpu_destroy_descriptor_set_layout(pResult);
        return SPUDRESULT_OUT_OF_MEMORY;
    }
    pResult->_desc.debug_name = pResult->_debug_name;
#endif

    *out_layout = pResult;
    return SPUD_SUCCESS;
}

void spudgpu_destroy_descriptor_set_layout(
    spudgpu_descriptor_set_layout layout) {
    if (!layout) return;
    vkDestroyDescriptorSetLayout(
        layout->_device._logical_device_vk,
        layout->_layout_vk,
        NULL);
#if _DEBUG
    free((void *)layout->_debug_name);
#endif
    free(layout);
}

SPUDRESULT spudgpu_create_descriptor_pool(
    spudgpu_device device,
    const spudgpu_descriptor_pool_desc *desc,
	spudgpu_descriptor_pool *out_pool) {
    if (!device) return SPUDRESULT_GPU_INVALID_DEVICE;
    if (!desc) return SPUDRESULT_NULL_DESC;
    if (!out_pool) return SPUDRESULT_NULL_OUTPUT_PARAMETER;
    if (desc->pool_size_count > SPUDGPU_MAX_DESCRIPTOR_POOL_SIZES) return SPUDRESULT_GPU_TOO_MANY_DESCRIPTOR_POOLS;

    spudgpu_descriptor_pool_vulkan result = {0};
    result._device = *device;
    result._desc = *desc;

    VkDevice vk_device = result._device._logical_device_vk;

    VkDescriptorPoolSize vk_pool_sizes[SPUDGPU_MAX_DESCRIPTOR_POOL_SIZES] = {0};
    for (uint32_t i = 0; i < desc->pool_size_count; i++) {
        VkDescriptorType vk_type =
                spudgpuvulkan___descriptor_type_internal(desc->pool_sizes[i].descriptor_type);
        if (vk_type == VK_DESCRIPTOR_TYPE_MAX_ENUM) return SPUDRESULT_GPU_CANNOT_RESOLVE_API_SPECIFIC_DESCRIPTOR_TYPE;

        vk_pool_sizes[i].type = vk_type;
        vk_pool_sizes[i].descriptorCount = desc->pool_sizes[i].count;
    }

    VkDescriptorPoolCreateInfo poolInfo = {0};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    // FREE_DESCRIPTOR_SET_BIT is not set: individual free is not supported,
    // which is the intended usage — reset the whole pool each frame instead.
    poolInfo.maxSets = desc->max_sets;
    poolInfo.poolSizeCount = desc->pool_size_count;
    poolInfo.pPoolSizes = vk_pool_sizes;

    if (vkCreateDescriptorPool(
        vk_device, &poolInfo, NULL,
        &result._pool_vk) != VK_SUCCESS) {
        return SPUDRESULT_API_SPECIFIC_FAILURE;
    }

#if _DEBUG
    if (desc->debug_name) {
        VkDebugUtilsObjectNameInfoEXT nameInfo = {0};
        nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        nameInfo.objectType = VK_OBJECT_TYPE_DESCRIPTOR_POOL;
        nameInfo.objectHandle = (uint64_t) result._pool_vk;
        nameInfo.pObjectName = desc->debug_name;
        SPUDRESULT sr = SPUD_SUCCESS;
        PFN_vkSetDebugUtilsObjectNameEXT fn =
                (PFN_vkSetDebugUtilsObjectNameEXT) vkGetDeviceProcAddr(
                    vk_device, "vkSetDebugUtilsObjectNameEXT");
		if (fn)
			if (fn(vk_device, &nameInfo))
				sr = SPUDRESULT_API_SPECIFIC_FAILURE;
		if (sr != SPUD_SUCCESS)
			return sr;
	}
#endif

    spudgpu_descriptor_pool_vulkan *pResult = malloc(sizeof(spudgpu_descriptor_pool_vulkan));
    memcpy(pResult, &result, sizeof(spudgpu_descriptor_pool_vulkan));
#if _DEBUG
    if (spud_debug_name_set(pResult, desc->debug_name) != SPUD_SUCCESS) {
        spudgpu_destroy_descriptor_pool(pResult);
        return SPUDRESULT_OUT_OF_MEMORY;
    }
    pResult->_desc.debug_name = pResult->_debug_name;
#endif

    *out_pool = pResult;
    return SPUD_SUCCESS;
}

void spudgpu_reset_descriptor_pool(
    spudgpu_descriptor_pool pool) {
    if (!pool) return;
    // Bulk-free all sets allocated from this pool. The 'flags' parameter is reserved and must be 0.
	vkResetDescriptorPool(pool->_device._logical_device_vk, pool->_pool_vk, 0);
}

void spudgpu_destroy_descriptor_pool(
    spudgpu_descriptor_pool pool) {
    if (!pool) return;
    // vkDestroyDescriptorPool implicitly frees all descriptor sets allocated from it.
    vkDestroyDescriptorPool(
        pool->_device._logical_device_vk,
        pool->_pool_vk,
        NULL);
#if _DEBUG
    free((void *)pool->_debug_name);
#endif
    free(pool);
}

SPUDRESULT spudgpu_create_descriptor_sets(
    spudgpu_device device,
    const spudgpu_descriptor_set_desc *desc,
    spudgpu_descriptor_set *out_sets) {
    if (!device) return SPUDRESULT_GPU_INVALID_DEVICE;
    if (!desc) return SPUDRESULT_NULL_DESC;
    if (!out_sets) return SPUDRESULT_NULL_OUTPUT_PARAMETER;
    if (!desc->pool) return SPUDRESULT_GPU_INVALID_DESCRIPTOR_POOL;
	if (!desc->set_count)
		return SPUDRESULT_GPU_ZERO_DESCRIPTOR_SET_LAYOUTS;
	if (desc->set_count > SPUDGPU_MAX_DESCRIPTOR_SET_LAYOUTS)
		return SPUDRESULT_GPU_TOO_MANY_DESCRIPTOR_SET_LAYOUTS;

    VkDevice vk_device = desc->pool->_device._logical_device_vk;

    // Gather the raw VkDescriptorSetLayout handles from the typed wrappers.
    VkDescriptorSetLayout vk_layouts[SPUDGPU_MAX_DESCRIPTOR_SET_LAYOUTS] = {0};
    for (uint32_t i = 0; i < desc->set_count; i++) {
        if (!desc->set_layouts[i]) return SPUDRESULT_GPU_INVALID_DESCRIPTOR_SET_LAYOUT;
        spudgpu_descriptor_set_layout_vulkan *vkLayout =
                (spudgpu_descriptor_set_layout_vulkan *) desc->set_layouts[i];
        vk_layouts[i] = vkLayout->_layout_vk;
    }

    // Allocate all sets in a single driver call.
    VkDescriptorSet vk_sets[SPUDGPU_MAX_DESCRIPTOR_SET_LAYOUTS] = {0};

    VkDescriptorSetAllocateInfo allocInfo = {0};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = desc->pool->_pool_vk;
    allocInfo.descriptorSetCount = desc->set_count;
    allocInfo.pSetLayouts = vk_layouts;

    if (vkAllocateDescriptorSets(vk_device, &allocInfo, vk_sets) != VK_SUCCESS) {
        return SPUDRESULT_API_SPECIFIC_FAILURE;
    }

    // Wrap each raw VkDescriptorSet in a heap-allocated SpudGPU handle.
    for (uint32_t i = 0; i < desc->set_count; i++) {
        spudgpu_descriptor_set_vulkan *pSet =
                malloc(sizeof(spudgpu_descriptor_set_vulkan));
        if (!pSet) {
            // Partial allocation — caller's out_sets is partially populated.
            // The pool reset at the next frame will reclaim the Vulkan-side sets.
            return SPUDRESULT_GPU_INTERNAL_DESCRIPTOR_SET_ALLOCATION_FAIL;
        }
#if _DEBUG
        pSet->_debug_name = NULL;
#endif
        pSet->_pool = *desc->pool;
        pSet->_set_vk = vk_sets[i];
        out_sets[i] = (spudgpu_descriptor_set) pSet;
    }

    return SPUD_SUCCESS;
}


void spudgpu_update_descriptor_sets(
    spudgpu_device device,
    const spudgpu_write_descriptor_set *writes,
    uint32_t write_count) {
    if (!device || !writes || !write_count) return;

    // We translate each SpudGPU write into a VkWriteDescriptorSet on the stack.
    // For larger write counts a heap allocation would be safer, but the per-frame
    // update pattern rarely exceeds a few dozen writes.
    VkWriteDescriptorSet vk_writes[64] = {0};
    VkDescriptorBufferInfo vk_buf_infos[64] = {0};
    VkDescriptorImageInfo vk_img_infos[64] = {0};

    uint32_t clamped = write_count < 64 ? write_count : 64;

    for (uint32_t i = 0; i < clamped; i++) {
        const spudgpu_write_descriptor_set *w = &writes[i];
        if (!w->dst_set) continue;

        spudgpu_descriptor_set_vulkan *vkSet =
                (spudgpu_descriptor_set_vulkan *) w->dst_set;

        VkDescriptorType vk_type =
                spudgpuvulkan___descriptor_type_internal(w->descriptor_type);
        if (vk_type == VK_DESCRIPTOR_TYPE_MAX_ENUM) continue;

        vk_writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        vk_writes[i].dstSet = vkSet->_set_vk;
        vk_writes[i].dstBinding = w->dst_binding;
        vk_writes[i].dstArrayElement = w->dst_array_element;
        vk_writes[i].descriptorCount = w->descriptor_count;
        vk_writes[i].descriptorType = vk_type;

        if (w->buffer_info) {
            spudgpu_buffer_vulkan *vkBuf =
                    (spudgpu_buffer_vulkan *) w->buffer_info->buffer;
            vk_buf_infos[i].buffer = vkBuf->_buffer_vk;
            vk_buf_infos[i].offset = w->buffer_info->offset;
            // A range of 0 from the caller means "bind the whole buffer".
            vk_buf_infos[i].range = w->buffer_info->range
                                        ? w->buffer_info->range
                                        : VK_WHOLE_SIZE;
            vk_writes[i].pBufferInfo = &vk_buf_infos[i];
        }

        if (w->image_info) {
            spudgpu_image_view_vulkan *vkView =
                    (spudgpu_image_view_vulkan *) w->image_info->image_view;
            vk_img_infos[i].imageView = vkView->_image_view_vk;
            // A SpudGPU layout, translated: the two enums' values differ
            // (SHADER_READ_ONLY is 4, VkImageLayout 4 is a depth layout).
            vk_img_infos[i].imageLayout = spud_layout_to_vk(w->image_info->image_layout);
            // Non-NULL only for COMBINED_IMAGE_SAMPLER writes.
            vk_img_infos[i].sampler = w->sampler
                                           ? ((spudgpu_sampler_vulkan *) w->sampler)->_sampler_vk
                                           : VK_NULL_HANDLE;
            vk_writes[i].pImageInfo = &vk_img_infos[i];
        } else if (w->sampler) {
            // Pure SAMPLER descriptor — no image_info involved.
            vk_img_infos[i].sampler = ((spudgpu_sampler_vulkan *) w->sampler)->_sampler_vk;
            vk_writes[i].pImageInfo = &vk_img_infos[i];
        }
    }

    vkUpdateDescriptorSets(device->_logical_device_vk, clamped, vk_writes, 0, NULL);
}

void spudgpu_cmd_bind_descriptor_sets(
    spudgpu_command_list cmd,
    spudgpu_shader_pipeline pipeline,
    uint32_t first_set,
    const spudgpu_descriptor_set *sets,
    uint32_t set_count) {
    if (!cmd || !pipeline || !sets || !set_count) return;
    if (set_count > SPUDGPU_MAX_DESCRIPTOR_SET_LAYOUTS) return;
    if (first_set > SPUDGPU_MAX_DESCRIPTOR_SET_LAYOUTS - set_count) return;

    // The command list's internal VkCommandBuffer lives at the front of the struct,
    // matching the pattern used in spudgpuvulkanbuffer.c / the command list implementation.
    VkCommandBuffer vk_cmd = cmd->_command_buffer_vk;

    // Unwrap each SpudGPU handle to its raw VkDescriptorSet.
    VkDescriptorSet vk_sets[SPUDGPU_MAX_DESCRIPTOR_SET_LAYOUTS] = {0};
    for (uint32_t i = 0; i < set_count; i++) {
        if (!sets[i]) return;
        spudgpu_descriptor_set_vulkan *vkSet =
                (spudgpu_descriptor_set_vulkan *) sets[i];
        vk_sets[i] = vkSet->_set_vk;
    }

    vkCmdBindDescriptorSets(
        vk_cmd,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipeline->_pipeline_layout_vk,
        first_set,
        set_count,
        vk_sets,
        0, // No dynamic offsets
        NULL);
}

void spudgpu_cmd_bind_descriptor_sets_compute(
    spudgpu_command_list cmd,
    spudgpu_compute_pipeline pipeline,
    uint32_t first_set,
    const spudgpu_descriptor_set *sets,
    uint32_t set_count) {
    if (!cmd || !pipeline || !sets || !set_count) return;
    if (set_count > SPUDGPU_MAX_DESCRIPTOR_SET_LAYOUTS) return;
    if (first_set > SPUDGPU_MAX_DESCRIPTOR_SET_LAYOUTS - set_count) return;

    VkDescriptorSet vk_sets[SPUDGPU_MAX_DESCRIPTOR_SET_LAYOUTS] = {0};
    for (uint32_t i = 0; i < set_count; i++) {
        if (!sets[i]) return;
        spudgpu_descriptor_set_vulkan *vkSet =
                (spudgpu_descriptor_set_vulkan *) sets[i];
        vk_sets[i] = vkSet->_set_vk;
    }

    vkCmdBindDescriptorSets(
        cmd->_command_buffer_vk,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        pipeline->_pipeline_layout_vk,
        first_set,
        set_count,
        vk_sets,
        0,
        NULL);
}


// ============================================================================
//  Bindless / Descriptor Indexing
// ============================================================================

#define SPUDGPU_BINDLESS_SAMPLED_IMAGE_BINDING 0
#define SPUDGPU_BINDLESS_STORAGE_IMAGE_BINDING 1
#define SPUDGPU_BINDLESS_STORAGE_BUFFER_BINDING 2

// Called once, eagerly, from spudgpuvulkan___initialize_vk_logical_device_internal
// right after device creation — every other struct in this backend embeds
// spudgpu_device_vulkan BY VALUE (a snapshot copy taken at creation time), so
// _bindless must already be populated before anything can copy the device,
// or later snapshots would carry a stale NULL forever.
SPUDRESULT spudgpuvulkan___ensure_bindless_state(spudgpu_device device) {
    if (device->_bindless) return SPUD_SUCCESS;

    spudgpu_bindless_state_vulkan *state = calloc(1, sizeof(spudgpu_bindless_state_vulkan));
    if (!state) return SPUDRESULT_API_SPECIFIC_FAILURE;

    VkDescriptorSetLayoutBinding bindings[3] = {0};
    bindings[SPUDGPU_BINDLESS_SAMPLED_IMAGE_BINDING].binding = SPUDGPU_BINDLESS_SAMPLED_IMAGE_BINDING;
    bindings[SPUDGPU_BINDLESS_SAMPLED_IMAGE_BINDING].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    bindings[SPUDGPU_BINDLESS_SAMPLED_IMAGE_BINDING].descriptorCount = SPUDGPU_BINDLESS_MAX_SLOTS_PER_CLASS;
    bindings[SPUDGPU_BINDLESS_SAMPLED_IMAGE_BINDING].stageFlags = VK_SHADER_STAGE_ALL;

    bindings[SPUDGPU_BINDLESS_STORAGE_IMAGE_BINDING].binding = SPUDGPU_BINDLESS_STORAGE_IMAGE_BINDING;
    bindings[SPUDGPU_BINDLESS_STORAGE_IMAGE_BINDING].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[SPUDGPU_BINDLESS_STORAGE_IMAGE_BINDING].descriptorCount = SPUDGPU_BINDLESS_MAX_SLOTS_PER_CLASS;
    bindings[SPUDGPU_BINDLESS_STORAGE_IMAGE_BINDING].stageFlags = VK_SHADER_STAGE_ALL;

    bindings[SPUDGPU_BINDLESS_STORAGE_BUFFER_BINDING].binding = SPUDGPU_BINDLESS_STORAGE_BUFFER_BINDING;
    bindings[SPUDGPU_BINDLESS_STORAGE_BUFFER_BINDING].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[SPUDGPU_BINDLESS_STORAGE_BUFFER_BINDING].descriptorCount = SPUDGPU_BINDLESS_MAX_SLOTS_PER_CLASS;
    bindings[SPUDGPU_BINDLESS_STORAGE_BUFFER_BINDING].stageFlags = VK_SHADER_STAGE_ALL;

    VkDescriptorBindingFlags bindingFlags[3];
    for (uint32_t i = 0; i < 3; i++)
        bindingFlags[i] = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
                           VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo = {0};
    bindingFlagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    bindingFlagsInfo.bindingCount = 3;
    bindingFlagsInfo.pBindingFlags = bindingFlags;

    VkDescriptorSetLayoutCreateInfo layoutInfo = {0};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.pNext = &bindingFlagsInfo;
    layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    layoutInfo.bindingCount = 3;
    layoutInfo.pBindings = bindings;

    spudgpu_descriptor_set_layout_vulkan *layoutWrapper =
            calloc(1, sizeof(spudgpu_descriptor_set_layout_vulkan));
    if (!layoutWrapper) {
        free(state);
        return SPUDRESULT_API_SPECIFIC_FAILURE;
    }
    layoutWrapper->_device = *device;

    if (vkCreateDescriptorSetLayout(
            device->_logical_device_vk, &layoutInfo, NULL,
            &layoutWrapper->_layout_vk) != VK_SUCCESS) {
        free(layoutWrapper);
        free(state);
        return SPUDRESULT_API_SPECIFIC_FAILURE;
    }

    VkDescriptorPoolSize poolSizes[3] = {
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, SPUDGPU_BINDLESS_MAX_SLOTS_PER_CLASS},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, SPUDGPU_BINDLESS_MAX_SLOTS_PER_CLASS},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, SPUDGPU_BINDLESS_MAX_SLOTS_PER_CLASS},
    };

    VkDescriptorPoolCreateInfo poolInfo = {0};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 3;
    poolInfo.pPoolSizes = poolSizes;

    if (vkCreateDescriptorPool(device->_logical_device_vk, &poolInfo, NULL, &state->pool_vk) != VK_SUCCESS) {
        vkDestroyDescriptorSetLayout(device->_logical_device_vk, layoutWrapper->_layout_vk, NULL);
        free(layoutWrapper);
        free(state);
        return SPUDRESULT_API_SPECIFIC_FAILURE;
    }

    VkDescriptorSetAllocateInfo allocInfo = {0};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = state->pool_vk;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layoutWrapper->_layout_vk;

    if (vkAllocateDescriptorSets(device->_logical_device_vk, &allocInfo, &state->set_vk) != VK_SUCCESS) {
        vkDestroyDescriptorPool(device->_logical_device_vk, state->pool_vk, NULL);
        vkDestroyDescriptorSetLayout(device->_logical_device_vk, layoutWrapper->_layout_vk, NULL);
        free(layoutWrapper);
        free(state);
        return SPUDRESULT_API_SPECIFIC_FAILURE;
    }

    state->layout = (spudgpu_descriptor_set_layout) layoutWrapper;
    device->_bindless = state;
    return SPUD_SUCCESS;
}

// Counterpart of spudgpuvulkan___ensure_bindless_state, called by
// spudgpu_destroy_instance before it destroys the device.
void spudgpuvulkan___destroy_bindless_state(spudgpu_device device) {
    if (!device || !device->_bindless) return;
    spudgpu_bindless_state_vulkan *state = device->_bindless;

    // Destroying the pool frees the set allocated from it.
    vkDestroyDescriptorPool(device->_logical_device_vk, state->pool_vk, NULL);
    spudgpu_descriptor_set_layout_vulkan *layoutWrapper = (spudgpu_descriptor_set_layout_vulkan *) state->layout;
    if (layoutWrapper) {
        vkDestroyDescriptorSetLayout(device->_logical_device_vk, layoutWrapper->_layout_vk, NULL);
#if _DEBUG
        free((void *)layoutWrapper->_debug_name);
#endif
        free(layoutWrapper);
    }
    free(state);
    device->_bindless = NULL;
}

static SPUDRESULT spudgpuvulkan___bindless_alloc_index(
        uint32_t *next_unused,
        uint32_t *free_stack,
        uint32_t *free_count,
        uint32_t *out_index) {
    if (*free_count > 0) {
        *out_index = free_stack[--(*free_count)];
        return SPUD_SUCCESS;
    }
    if (*next_unused >= SPUDGPU_BINDLESS_MAX_SLOTS_PER_CLASS)
        return SPUDRESULT_GPU_BINDLESS_OUT_OF_SLOTS;
    *out_index = (*next_unused)++;
    return SPUD_SUCCESS;
}

SPUDRESULT spudgpu_get_bindless_capabilities(
        spudgpu_device device,
        spudgpu_bindless_capabilities *out_caps) {
    if (!device) return SPUDRESULT_GPU_INVALID_DEVICE;
    if (!out_caps) return SPUDRESULT_NULL_OUTPUT_PARAMETER;

    memset(out_caps, 0, sizeof(*out_caps));
    // Reflects whether the global layout/pool/set actually got created —
    // eagerly attempted once at device creation (see
    // spudgpuvulkancontext.c), so this is normally already known-good by the
    // time any caller queries it, not a fresh attempt every call.
    out_caps->supported = spudgpuvulkan___ensure_bindless_state(device) == SPUD_SUCCESS;
    if (!out_caps->supported) return SPUD_SUCCESS;
    out_caps->max_sampled_images = SPUDGPU_BINDLESS_MAX_SLOTS_PER_CLASS;
    out_caps->max_storage_images = SPUDGPU_BINDLESS_MAX_SLOTS_PER_CLASS;
    out_caps->max_storage_buffers = SPUDGPU_BINDLESS_MAX_SLOTS_PER_CLASS;
    return SPUD_SUCCESS;
}

spudgpu_descriptor_set_layout spudgpu_get_bindless_descriptor_set_layout(spudgpu_device device) {
    if (!device) return NULL;
    if (spudgpuvulkan___ensure_bindless_state(device) != SPUD_SUCCESS) return NULL;
    return device->_bindless->layout;
}

SPUDRESULT spudgpu_bindless_register_sampled_image(
        spudgpu_device device,
        spudgpu_image_view view,
        uint32_t *out_index) {
    if (!device) return SPUDRESULT_GPU_INVALID_DEVICE;
    if (!view) return SPUDRESULT_GPU_INVALID_IMAGE_VIEW;
    if (!out_index) return SPUDRESULT_NULL_OUTPUT_PARAMETER;

    SPUDRESULT sr = spudgpuvulkan___ensure_bindless_state(device);
    if (sr != SPUD_SUCCESS) return SPUDRESULT_GPU_EXT_BINDLESS_DESCRIPTOR_INDEXING_NOT_SUPPORTED;

    spudgpu_bindless_state_vulkan *state = device->_bindless;
    sr = spudgpuvulkan___bindless_alloc_index(
            &state->sampled_image_next_unused,
            state->sampled_image_free_stack,
            &state->sampled_image_free_count,
            out_index);
    if (sr != SPUD_SUCCESS) return sr;

    spudgpu_image_view_vulkan *vkView = (spudgpu_image_view_vulkan *) view;
    VkDescriptorImageInfo imageInfo = {0};
    imageInfo.imageView = vkView->_image_view_vk;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet write = {0};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = state->set_vk;
    write.dstBinding = SPUDGPU_BINDLESS_SAMPLED_IMAGE_BINDING;
    write.dstArrayElement = *out_index;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    write.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(device->_logical_device_vk, 1, &write, 0, NULL);
    return SPUD_SUCCESS;
}

void spudgpu_bindless_unregister_sampled_image(spudgpu_device device, uint32_t index) {
    if (!device || !device->_bindless) return;
    if (index == SPUDGPU_BINDLESS_INVALID_INDEX) return;
    spudgpu_bindless_state_vulkan *state = device->_bindless;
    if (state->sampled_image_free_count < SPUDGPU_BINDLESS_MAX_SLOTS_PER_CLASS)
        state->sampled_image_free_stack[state->sampled_image_free_count++] = index;
}

SPUDRESULT spudgpu_bindless_register_storage_image(
        spudgpu_device device,
        spudgpu_image_view view,
        uint32_t *out_index) {
    if (!device) return SPUDRESULT_GPU_INVALID_DEVICE;
    if (!view) return SPUDRESULT_GPU_INVALID_IMAGE_VIEW;
    if (!out_index) return SPUDRESULT_NULL_OUTPUT_PARAMETER;

    SPUDRESULT sr = spudgpuvulkan___ensure_bindless_state(device);
    if (sr != SPUD_SUCCESS) return SPUDRESULT_GPU_EXT_BINDLESS_DESCRIPTOR_INDEXING_NOT_SUPPORTED;

    spudgpu_bindless_state_vulkan *state = device->_bindless;
    sr = spudgpuvulkan___bindless_alloc_index(
            &state->storage_image_next_unused,
            state->storage_image_free_stack,
            &state->storage_image_free_count,
            out_index);
    if (sr != SPUD_SUCCESS) return sr;

    spudgpu_image_view_vulkan *vkView = (spudgpu_image_view_vulkan *) view;
    VkDescriptorImageInfo imageInfo = {0};
    imageInfo.imageView = vkView->_image_view_vk;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkWriteDescriptorSet write = {0};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = state->set_vk;
    write.dstBinding = SPUDGPU_BINDLESS_STORAGE_IMAGE_BINDING;
    write.dstArrayElement = *out_index;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    write.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(device->_logical_device_vk, 1, &write, 0, NULL);
    return SPUD_SUCCESS;
}

void spudgpu_bindless_unregister_storage_image(spudgpu_device device, uint32_t index) {
    if (!device || !device->_bindless) return;
    if (index == SPUDGPU_BINDLESS_INVALID_INDEX) return;
    spudgpu_bindless_state_vulkan *state = device->_bindless;
    if (state->storage_image_free_count < SPUDGPU_BINDLESS_MAX_SLOTS_PER_CLASS)
        state->storage_image_free_stack[state->storage_image_free_count++] = index;
}

SPUDRESULT spudgpu_bindless_register_storage_buffer(
        spudgpu_device device,
        spudgpu_buffer_view view,
        uint32_t *out_index) {
    if (!device) return SPUDRESULT_GPU_INVALID_DEVICE;
    if (!view) return SPUDRESULT_GPU_INVALID_BUFFER_VIEW;
    if (!out_index) return SPUDRESULT_NULL_OUTPUT_PARAMETER;

    SPUDRESULT sr = spudgpuvulkan___ensure_bindless_state(device);
    if (sr != SPUD_SUCCESS) return SPUDRESULT_GPU_EXT_BINDLESS_DESCRIPTOR_INDEXING_NOT_SUPPORTED;

    spudgpu_bindless_state_vulkan *state = device->_bindless;
    sr = spudgpuvulkan___bindless_alloc_index(
            &state->storage_buffer_next_unused,
            state->storage_buffer_free_stack,
            &state->storage_buffer_free_count,
            out_index);
    if (sr != SPUD_SUCCESS) return sr;

    spudgpu_buffer_view_vulkan *vkView = (spudgpu_buffer_view_vulkan *) view;

    // spudgpu_buffer_view has no owning spudgpu_buffer handle on its own —
    // it only wraps a VkBufferView, which typed/texel storage buffers need
    // but raw byte-range SSBO bindings (the common bindless case) don't.
    // Bind the raw buffer range this view describes via its VkBuffer parent.
    spudgpu_buffer_vulkan *vkBuffer =
            (spudgpu_buffer_vulkan *) vkView->_desc.parent_buffer;

    VkDescriptorBufferInfo bufferInfo = {0};
    bufferInfo.buffer = vkBuffer->_buffer_vk;
    bufferInfo.offset = vkView->_desc.offset_from_parent_buffer;
    bufferInfo.range = vkView->_desc.size ? vkView->_desc.size : VK_WHOLE_SIZE;

    VkWriteDescriptorSet write = {0};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = state->set_vk;
    write.dstBinding = SPUDGPU_BINDLESS_STORAGE_BUFFER_BINDING;
    write.dstArrayElement = *out_index;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(device->_logical_device_vk, 1, &write, 0, NULL);
    return SPUD_SUCCESS;
}

void spudgpu_bindless_unregister_storage_buffer(spudgpu_device device, uint32_t index) {
    if (!device || !device->_bindless) return;
    if (index == SPUDGPU_BINDLESS_INVALID_INDEX) return;
    spudgpu_bindless_state_vulkan *state = device->_bindless;
    if (state->storage_buffer_free_count < SPUDGPU_BINDLESS_MAX_SLOTS_PER_CLASS)
        state->storage_buffer_free_stack[state->storage_buffer_free_count++] = index;
}

#if __cplusplus
}
#endif

#endif // SPUDGPU_COMPILE_VULKAN_API
