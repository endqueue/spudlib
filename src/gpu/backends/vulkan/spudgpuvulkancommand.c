
#if SPUDGPU_COMPILE_VULKAN_API

#include "spudgpuvulkan.h"
#include "stdio.h"
#include "stdlib.h"
#include <assert.h>
#include "../../../spudstructbase.h"

#if __cplusplus
extern "C" {



#endif

// The device's own queues, made with it (spudgpu_create_device): the same
// kind and index always hand out the same one, as on D3D12 and Metal, so the
// caller can tell two uses of one VkQueue apart from uses of two.
SPUDRESULT spudgpu_get_command_queue(
    spudgpu_device device,
    SPUDGPU_COMMAND_LIST_TYPE type,
    uint32_t index,
    spudgpu_command_queue *out_queue) {
    if (!device) return SPUDRESULT_GPU_INVALID_DEVICE;
    struct spudgpu_command_queue_t **queues = NULL;
    uint32_t count = 0;
    switch (type) {
    case SPUDGPU_COMMAND_LIST_TYPE_DIRECT:
        queues = device->_cmd_queues_direct;
        count  = device->_cmd_queue_count_direct;
        break;
    case SPUDGPU_COMMAND_LIST_TYPE_COMPUTE:
        queues = device->_cmd_queues_compute;
        count  = device->_cmd_queue_count_compute;
        break;
    case SPUDGPU_COMMAND_LIST_TYPE_COPY:
        queues = device->_cmd_queues_copy;
        count  = device->_cmd_queue_count_copy;
        break;
    default:
        // Bundles aren't submitted to a queue, on any backend.
        return SPUDRESULT_GPU_INVALID_COMMAND_LIST_TYPE;
    }
    if (index >= count) return SPUDRESULT_INDEX_OUT_OF_RANGE;
    if (!out_queue) return SPUDRESULT_NULL_OUTPUT_PARAMETER;
    *out_queue = queues[index];
    return SPUD_SUCCESS;
}

SPUDRESULT spudgpu_submit_command_lists(
    spudgpu_command_queue queue,
    spudgpu_command_list *cmd_lists,
    uint32_t cmd_list_count) {
    if (!queue) return SPUDRESULT_GPU_INVALID_COMMAND_QUEUE;
    if (!cmd_lists) return SPUDRESULT_GPU_INVALID_COMMAND_LIST;
    if (cmd_list_count == 0) return SPUDRESULT_ZERO_SIZE;
    
    const SPUD_ALLOCATOR *allocator = &queue->head.allocator;
    VkCommandBuffer *buffers = SPUD_NEW_ARRAY(allocator, VkCommandBuffer, cmd_list_count);
    if (!buffers) return SPUDRESULT_OUT_OF_MEMORY;
    for (uint32_t i = 0; i < cmd_list_count; i++) {
        spudgpu_command_list_vulkan *cl = (spudgpu_command_list_vulkan *) cmd_lists[i];
        buffers[i] = cl->_command_buffer_vk;
    }

    VkSubmitInfo submit = {0};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = cmd_list_count;
    submit.pCommandBuffers = buffers;

    spudgpuvulkan___queue_enter(queue);
    VkResult r = vkQueueSubmit(queue->_queue_vk, 1, &submit, VK_NULL_HANDLE);
    spudgpuvulkan___queue_leave(queue);
    if (r != VK_SUCCESS) {
        printf("spudgpu: vkQueueSubmit failed (%d)\n", r);
        SPUD_DELETE_ARRAY(allocator, VkCommandBuffer, buffers, cmd_list_count);
        return SPUDRESULT_API_SPECIFIC_FAILURE;
    }
    SPUD_DELETE_ARRAY(allocator, VkCommandBuffer, buffers, cmd_list_count);
    return SPUD_SUCCESS;
}

// spudgpu_queue_submit with only the command lists and the swap chain set.
SPUDRESULT spudgpu_submit_command_lists_synced(
    spudgpu_command_queue queue,
    spudgpu_command_list *cmd_lists,
    uint32_t cmd_list_count,
    spudgpu_swap_chain swap_chain) {
    if (!queue) return SPUDRESULT_GPU_INVALID_COMMAND_QUEUE;
    if (!cmd_lists) return SPUDRESULT_GPU_INVALID_COMMAND_LIST;
    if (cmd_list_count == 0) return SPUDRESULT_ZERO_SIZE;
    if (!swap_chain) return SPUDRESULT_GPU_INVALID_SWAP_CHAIN;

    spudgpu_submit_desc desc = {0};
    desc.base.type = SPUD_STRUCT_TYPE_GPU_SUBMIT_DESC;
    desc.cmd_lists      = cmd_lists;
    desc.cmd_list_count = cmd_list_count;
    desc.swap_chain     = swap_chain;
    return spudgpu_queue_submit(queue, &desc);
}

SPUDRESULT spudgpu_create_command_allocator(
    spudgpu_device device,
	const spudgpu_command_allocator_desc *desc,
	spudgpu_command_allocator *out_allocator) {
    if (!device) return SPUDRESULT_GPU_INVALID_DEVICE;
    if (!desc) return SPUDRESULT_NULL_DESC;
    const SPUDRESULT base_sr = spud_struct_check(&desc->base, SPUD_STRUCT_TYPE_GPU_COMMAND_ALLOCATOR_DESC);
    if (base_sr != SPUD_SUCCESS) return base_sr;
    if (!out_allocator) return SPUDRESULT_NULL_OUTPUT_PARAMETER;

    // The lists of an allocator are submitted to a queue of its kind, a
    // bundle's run inside a direct list: the device must have one, and the
    // pool is of that kind's family (Vulkan allows a pool only of a family
    // the device has queues of).
    uint32_t queues = 0;
    switch (desc->type) {
    case SPUDGPU_COMMAND_LIST_TYPE_DIRECT:
    case SPUDGPU_COMMAND_LIST_TYPE_BUNDLE: queues = device->_cmd_queue_count_direct; break;
    case SPUDGPU_COMMAND_LIST_TYPE_COMPUTE: queues = device->_cmd_queue_count_compute; break;
    case SPUDGPU_COMMAND_LIST_TYPE_COPY: queues = device->_cmd_queue_count_copy; break;
    default: break;
    }
    if (queues == 0) return SPUDRESULT_GPU_INVALID_COMMAND_LIST_TYPE;
    const SPUD_ALLOCATOR *allocator = &device->head.allocator;
    const uint32_t graphics_family  = spudgpuvulkan___family_of(device->_adapter, desc->type);

    VkCommandPoolCreateInfo pool_info = {0};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = graphics_family;
    // RESET_COMMAND_BUFFER_BIT lets you reset individual command buffers
    // instead of the whole pool — useful for multi-threaded recording.
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    spudgpu_command_allocator_vulkan *alloc = SPUD_NEW_ZEROED(allocator, spudgpu_command_allocator_vulkan);
    if (!alloc) return SPUDRESULT_OUT_OF_MEMORY;
    alloc->head.allocator = device->head.allocator;
    alloc->_device = *device;
    alloc->_queue_family_index = graphics_family;
    alloc->_type = desc->type;

    VkResult r = vkCreateCommandPool(
        device->_logical_device_vk, &pool_info, NULL, &alloc->_command_pool_vk);
    if (r != VK_SUCCESS) {
        printf("spudgpu: vkCreateCommandPool failed (%d)\n", r);
        SPUD_DELETE(allocator, spudgpu_command_allocator_vulkan, alloc);
        return SPUDRESULT_API_SPECIFIC_FAILURE;
    }

    SPUDGPU___LIVE_ADD(&device->_shared->live, SPUDGPU___OBJECT_COMMAND_ALLOCATOR);
    *out_allocator = alloc;
    return SPUD_SUCCESS;
}

SPUDRESULT spudgpu_reset_command_allocator(spudgpu_command_allocator allocator) {
    if (!allocator) return SPUDRESULT_GPU_INVALID_COMMAND_ALLOCATOR;
#if _DEBUG
    // spudgpu.h: no list of the allocator is open when it is reset.
    assert(!allocator->_open_list && "spudgpu_reset_command_allocator: a list of this allocator is still open");
#endif
    VkResult r = vkResetCommandPool(
        allocator->_device._logical_device_vk, allocator->_command_pool_vk, 0);
    return (r == VK_SUCCESS) ? SPUD_SUCCESS : SPUDRESULT_API_SPECIFIC_FAILURE;
}

void spudgpu_destroy_command_allocator(spudgpu_command_allocator allocator) {
    if (!allocator) return;
    vkDestroyCommandPool(allocator->_device._logical_device_vk, allocator->_command_pool_vk, NULL);
    SPUDGPU___LIVE_REMOVE(&allocator->_device._shared->live, SPUDGPU___OBJECT_COMMAND_ALLOCATOR);
    const SPUD_ALLOCATOR memory_allocator = allocator->head.allocator;
    spud_string_free(&memory_allocator, allocator->head.debug_name);
    SPUD_DELETE(&memory_allocator, spudgpu_command_allocator_vulkan, allocator);
}

SPUDRESULT spudgpu_create_command_list(
    spudgpu_command_allocator allocator, spudgpu_command_list *out_cmd_list) {
    if (!allocator) return SPUDRESULT_GPU_INVALID_COMMAND_ALLOCATOR;
    if (!out_cmd_list) return SPUDRESULT_NULL_OUTPUT_PARAMETER;
    
    VkCommandBufferAllocateInfo alloc_info = {0};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = allocator->_command_pool_vk;
    alloc_info.level = (allocator->_type == SPUDGPU_COMMAND_LIST_TYPE_BUNDLE)
        ? VK_COMMAND_BUFFER_LEVEL_SECONDARY
        : VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;

    spudgpu_command_list_vulkan *cl = SPUD_NEW_ZEROED(&allocator->head.allocator, spudgpu_command_list_vulkan);
    if (!cl) return SPUDRESULT_OUT_OF_MEMORY;
    cl->head.allocator = allocator->head.allocator;
    cl->_allocator = *allocator;
#if _DEBUG
    cl->_debug_allocator = allocator;
#endif

    VkResult r = vkAllocateCommandBuffers(
        allocator->_device._logical_device_vk, &alloc_info, &cl->_command_buffer_vk);
    if (r != VK_SUCCESS) {
        printf("spudgpu: vkAllocateCommandBuffers failed (%d)\n", r);
        SPUD_DELETE(&allocator->head.allocator, spudgpu_command_list_vulkan, cl);
        return SPUDRESULT_API_SPECIFIC_FAILURE;
    }

    SPUDGPU___LIVE_ADD(&allocator->_device._shared->live, SPUDGPU___OBJECT_COMMAND_LIST);
	*out_cmd_list = cl;
    return SPUD_SUCCESS;
}

void spudgpu_destroy_command_list(spudgpu_command_list cmd) {
    if (!cmd) return;
    vkFreeCommandBuffers(
        cmd->_allocator._device._logical_device_vk,
        cmd->_allocator._command_pool_vk,
        1, &cmd->_command_buffer_vk);
#if _DEBUG
    // A list destroyed open is not open any more.
    spudgpuvulkan___debug_list_closed(cmd);
#endif
    SPUDGPU___LIVE_REMOVE(&cmd->_allocator._device._shared->live, SPUDGPU___OBJECT_COMMAND_LIST);
    const SPUD_ALLOCATOR allocator = cmd->head.allocator;
    spud_string_free(&allocator, cmd->head.debug_name);
    SPUD_DELETE(&allocator, spudgpu_command_list_vulkan, cmd);
}

#if _DEBUG
void spudgpuvulkan___debug_list_opened(spudgpu_command_list cmd) {
    // spudgpu.h: one list of an allocator open at a time. Vulkan would
    // allow this; D3D12 would refuse it.
    assert(!cmd->_debug_allocator->_open_list && "spudgpu_begin_command_list: a list of this allocator is already open; end it first");
    cmd->_debug_allocator->_open_list = cmd;
}

void spudgpuvulkan___debug_list_closed(spudgpu_command_list cmd) {
    if (cmd->_debug_allocator->_open_list == cmd)
        cmd->_debug_allocator->_open_list = NULL;
}
#endif

void spudgpu_begin_command_list(spudgpu_command_list cmd) {
    if (!cmd) return;
#if _DEBUG
    spudgpuvulkan___debug_list_opened(cmd);
#endif

    // Reset so this buffer can be re-recorded this frame.
    vkResetCommandBuffer(cmd->_command_buffer_vk, 0);

    // ONE_TIME_SUBMIT_BIT is a hint that this buffer is re-recorded each frame.
    // Remove it if you plan to record once and submit many times.
    VkCommandBufferBeginInfo begin = {0};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd->_command_buffer_vk, &begin);
}

void spudgpu_end_command_list(spudgpu_command_list cmd) {
    if (!cmd) return;
    vkEndCommandBuffer(cmd->_command_buffer_vk);
#if _DEBUG
    spudgpuvulkan___debug_list_closed(cmd);
#endif
}

void spudgpu_cmd_begin_debug_label(spudgpu_command_list cmd, const char *label) {
    if (!cmd) return;
    PFN_vkCmdBeginDebugUtilsLabelEXT begin = cmd->_allocator._device._vkCmdBeginDebugUtilsLabelEXT;
    if (!begin) return;
    VkDebugUtilsLabelEXT info = {0};
    info.sType      = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    info.pLabelName = label ? label : "";
    begin(cmd->_command_buffer_vk, &info);
}

void spudgpu_cmd_end_debug_label(spudgpu_command_list cmd) {
    if (!cmd) return;
    PFN_vkCmdEndDebugUtilsLabelEXT end = cmd->_allocator._device._vkCmdEndDebugUtilsLabelEXT;
    if (!end) return;
    end(cmd->_command_buffer_vk);
}

void spudgpu_cmd_set_viewports(
    spudgpu_command_list cmd,
    uint32_t first_viewport,
    uint32_t viewport_count,
    const SPUDGPU_VIEWPORT *viewports) {
    if (!cmd || !viewports || viewport_count == 0) return;

    VkViewport *vk_viewports = SPUD_NEW_ARRAY_ZEROED(&cmd->head.allocator, VkViewport, viewport_count);
    if (!vk_viewports) return;
    for (uint32_t i = 0; i < viewport_count; i++) {
        vk_viewports[i].x = viewports[i].x;
        vk_viewports[i].y = viewports[i].y;
        vk_viewports[i].width = viewports[i].width;
        vk_viewports[i].height = viewports[i].height;
        vk_viewports[i].minDepth = viewports[i].minDepth;
        vk_viewports[i].maxDepth = viewports[i].maxDepth;
    }

    vkCmdSetViewport(cmd->_command_buffer_vk, first_viewport, viewport_count, vk_viewports);

    SPUD_DELETE_ARRAY(&cmd->head.allocator, VkViewport, vk_viewports, viewport_count);
}

void spudgpu_cmd_set_scissor_rects(
    spudgpu_command_list cmd,
    uint32_t first_scissor_rect,
    uint32_t scissor_rect_count,
    const SPUDGPU_SCISSOR_RECT *scissor_rects) {
    if (!cmd || !scissor_rects || scissor_rect_count == 0) return;

    VkRect2D *vk_scissors = SPUD_NEW_ARRAY_ZEROED(&cmd->head.allocator, VkRect2D, scissor_rect_count);
    if (!vk_scissors) return;
    for (uint32_t i = 0; i < scissor_rect_count; i++) {
        vk_scissors[i].offset.x = (int32_t) scissor_rects[i].x;
        vk_scissors[i].offset.y = (int32_t) scissor_rects[i].y;
        vk_scissors[i].extent.width = (uint32_t) scissor_rects[i].width;
        vk_scissors[i].extent.height = (uint32_t) scissor_rects[i].height;
    }

    vkCmdSetScissor(cmd->_command_buffer_vk, first_scissor_rect, scissor_rect_count, vk_scissors);
    
    SPUD_DELETE_ARRAY(&cmd->head.allocator, VkRect2D, vk_scissors, scissor_rect_count);
}

void spudgpu_cmd_set_vertex_buffers(
    spudgpu_command_list cmd,
    uint32_t start_slot,
    uint32_t view_count,
    spudgpu_buffer_view *buffer_views) {
    if (!cmd || !buffer_views || view_count == 0) return;

    // Both arrays or neither.
    const SPUD_ALLOCATOR *allocator = &cmd->head.allocator;
    VkBuffer *vk_buffers = SPUD_NEW_ARRAY_ZEROED(allocator, VkBuffer, view_count);
    if (!vk_buffers) return;
    VkDeviceSize *vk_offsets = SPUD_NEW_ARRAY_ZEROED(allocator, VkDeviceSize, view_count);
    if (!vk_offsets) {
        SPUD_DELETE_ARRAY(allocator, VkBuffer, vk_buffers, view_count);
        return;
    }

    for (uint32_t i = 0; i < view_count; i++) {
        spudgpu_buffer_view_vulkan *bv = (spudgpu_buffer_view_vulkan *) buffer_views[i];
        vk_buffers[i] = bv->_desc.parent_buffer->_buffer_vk;
        vk_offsets[i] = (VkDeviceSize) bv->_desc.offset_from_parent_buffer;
    }

    vkCmdBindVertexBuffers(
        cmd->_command_buffer_vk,
        start_slot,
        view_count,
        vk_buffers,
        vk_offsets);

    SPUD_DELETE_ARRAY(allocator, VkBuffer, vk_buffers, view_count);
    SPUD_DELETE_ARRAY(allocator, VkDeviceSize, vk_offsets, view_count);
}

void spudgpu_cmd_set_index_buffer(
    spudgpu_command_list cmd,
    spudgpu_buffer_view buffer_view) {
    if (!cmd || !buffer_view) return;
    spudgpu_buffer_view_vulkan *bv = (spudgpu_buffer_view_vulkan *) buffer_view;
    VkIndexType index_type = bv->_desc.stride == sizeof(uint16_t)
                                 ? VK_INDEX_TYPE_UINT16
                                 : VK_INDEX_TYPE_UINT32;
    vkCmdBindIndexBuffer(
        cmd->_command_buffer_vk,
        bv->_desc.parent_buffer->_buffer_vk,
        (VkDeviceSize) bv->_desc.offset_from_parent_buffer,
        index_type);
}

void spudgpu_cmd_draw(
    spudgpu_command_list cmd,
    uint32_t vertex_count,
    uint32_t start_vertex_location) {
    if (!cmd) return;
    vkCmdDraw(
        cmd->_command_buffer_vk,
        vertex_count,
        1,
        start_vertex_location,
        0);
}

void spudgpu_cmd_bind_pipeline(
    spudgpu_command_list cmd,
    spudgpu_shader_pipeline pipeline) {
    if (!cmd || !pipeline) return;

    vkCmdBindPipeline(
        cmd->_command_buffer_vk,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipeline->_pipeline_vk);
}

void spudgpu_cmd_draw_indexed(
    spudgpu_command_list cmd,
    uint32_t index_count,
    uint32_t start_index_location,
    int32_t base_vertex_location) {
    if (!cmd) return;
    vkCmdDrawIndexed(
        cmd->_command_buffer_vk,
        index_count,
        1,
        start_index_location,
        base_vertex_location,
        0);
}

void spudgpu_cmd_draw_instanced(
    spudgpu_command_list cmd,
    uint32_t vertex_count_per_instance,
    uint32_t instance_count,
    uint32_t start_vertex_location,
    uint32_t start_instance_location) {
    if (!cmd) return;
    vkCmdDraw(
        cmd->_command_buffer_vk,
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
    if (!cmd) return;
    vkCmdDrawIndexed(
        cmd->_command_buffer_vk,
        index_count_per_instance,
        instance_count,
        start_index_location,
        base_vertex_location,
        start_instance_location);
}

// One vkQueueSubmit carrying everything the desc asks for:
// - the caller's binary semaphores;
// - with a swap chain: a wait on its image_available semaphore, a signal of
//   its render_finished semaphore, and its in-flight VkFence, which is the
//   one VkFence a submit can take;
// - the caller's fence, which is a timeline semaphore and so goes in the
//   signal semaphore list with its value in a VkTimelineSemaphoreSubmitInfo.
SPUDRESULT spudgpu_queue_submit(spudgpu_command_queue queue, const spudgpu_submit_desc *desc) {
    if (!queue) return SPUDRESULT_GPU_INVALID_COMMAND_QUEUE;
    if (!desc) return SPUDRESULT_NULL_DESC;
    const SPUDRESULT base_sr = spud_struct_check(&desc->base, SPUD_STRUCT_TYPE_GPU_SUBMIT_DESC);
    if (base_sr != SPUD_SUCCESS) return base_sr;
    if (!desc->cmd_lists) return SPUDRESULT_GPU_INVALID_COMMAND_LIST;
    if (desc->cmd_list_count == 0) return SPUDRESULT_ZERO_SIZE;

    spudgpu_swap_chain_vulkan *swap_chain = (spudgpu_swap_chain_vulkan *) desc->swap_chain;
    spudgpu_fence_vulkan *fence           = (spudgpu_fence_vulkan *) desc->signal_fence;

    uint32_t wait_count   = desc->wait_semaphore_count + (swap_chain ? 1 : 0);
    uint32_t signal_count = desc->signal_semaphore_count + (swap_chain ? 1 : 0) + (fence ? 1 : 0);

    // A count may legitimately be 0 (the helpers then ask for 1 byte, at
    // allocation and free alike). The value arrays are zeroed; a binary
    // semaphore's entry is ignored.
    const SPUD_ALLOCATOR *allocator = &queue->head.allocator;
    VkCommandBuffer      *cmd_bufs      = SPUD_NEW_ARRAY_ZEROED(allocator, VkCommandBuffer, desc->cmd_list_count);
    VkSemaphore          *wait_sems     = SPUD_NEW_ARRAY_ZEROED(allocator, VkSemaphore, wait_count);
    VkPipelineStageFlags *wait_stages   = SPUD_NEW_ARRAY_ZEROED(allocator, VkPipelineStageFlags, wait_count);
    uint64_t             *wait_values   = SPUD_NEW_ARRAY_ZEROED(allocator, uint64_t, wait_count);
    VkSemaphore          *signal_sems   = SPUD_NEW_ARRAY_ZEROED(allocator, VkSemaphore, signal_count);
    uint64_t             *signal_values = SPUD_NEW_ARRAY_ZEROED(allocator, uint64_t, signal_count);
    if (!cmd_bufs || !wait_sems || !wait_stages || !wait_values || !signal_sems || !signal_values) {
        SPUD_DELETE_ARRAY(allocator, VkCommandBuffer, cmd_bufs, desc->cmd_list_count);
        SPUD_DELETE_ARRAY(allocator, VkSemaphore, wait_sems, wait_count);
        SPUD_DELETE_ARRAY(allocator, VkPipelineStageFlags, wait_stages, wait_count);
        SPUD_DELETE_ARRAY(allocator, uint64_t, wait_values, wait_count);
        SPUD_DELETE_ARRAY(allocator, VkSemaphore, signal_sems, signal_count);
        SPUD_DELETE_ARRAY(allocator, uint64_t, signal_values, signal_count);
        return SPUDRESULT_OUT_OF_MEMORY;
    }

    for (uint32_t i = 0; i < desc->cmd_list_count; i++) {
        spudgpu_command_list_vulkan *cl = (spudgpu_command_list_vulkan *) desc->cmd_lists[i];
        cmd_bufs[i] = cl->_command_buffer_vk;
    }

    uint32_t wait_index = 0;
    for (uint32_t i = 0; i < desc->wait_semaphore_count; i++) {
        spudgpu_semaphore_vulkan *sem = (spudgpu_semaphore_vulkan *) desc->wait_semaphores[i];
        wait_sems[wait_index]   = sem->_semaphore_vk;
        wait_stages[wait_index] = (VkPipelineStageFlags) desc->wait_stage_masks[i];
        wait_index++;
    }
    uint32_t signal_index = 0;
    for (uint32_t i = 0; i < desc->signal_semaphore_count; i++) {
        spudgpu_semaphore_vulkan *sem = (spudgpu_semaphore_vulkan *) desc->signal_semaphores[i];
        signal_sems[signal_index++] = sem->_semaphore_vk;
    }

    VkFence in_flight_fence = VK_NULL_HANDLE;
    if (swap_chain) {
        uint32_t frame = swap_chain->_current_frame;
        wait_sems[wait_index]   = swap_chain->_image_available_semaphores[frame]._semaphore_vk;
        wait_stages[wait_index] = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        wait_index++;
        // render_finished_semaphores is indexed by swapchain IMAGE index, not
        // by the frame-in-flight slot (see
        // spudgpuvulkan___fences_semaphores_swapchain_creation_internal and
        // spudgpu_swap_chain_present, which waits on this same array using
        // _current_image_index).
        signal_sems[signal_index++] =
            swap_chain->_render_finished_semaphores[swap_chain->_current_image_index]._semaphore_vk;
        in_flight_fence = swap_chain->_in_flight_fences_vk[frame];
    }
    if (fence) {
        signal_sems[signal_index]   = fence->_semaphore_vk;
        signal_values[signal_index] = desc->signal_fence_value;
        signal_index++;
    }

    VkTimelineSemaphoreSubmitInfo timeline = {0};
    timeline.sType                     = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
    timeline.waitSemaphoreValueCount   = wait_count;
    timeline.pWaitSemaphoreValues      = wait_values;
    timeline.signalSemaphoreValueCount = signal_count;
    timeline.pSignalSemaphoreValues    = signal_values;

    VkSubmitInfo submit = {0};
    submit.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.pNext                = fence ? &timeline : NULL;
    submit.waitSemaphoreCount   = wait_count;
    submit.pWaitSemaphores      = wait_sems;
    submit.pWaitDstStageMask    = wait_stages;
    submit.commandBufferCount   = desc->cmd_list_count;
    submit.pCommandBuffers      = cmd_bufs;
    submit.signalSemaphoreCount = signal_count;
    submit.pSignalSemaphores    = signal_sems;

    spudgpuvulkan___queue_enter(queue);
    VkResult r = vkQueueSubmit(queue->_queue_vk, 1, &submit, in_flight_fence);
    spudgpuvulkan___queue_leave(queue);
    if (r != VK_SUCCESS) {
        printf("spudgpu: vkQueueSubmit failed (%d)\n", r);
    }

    SPUD_DELETE_ARRAY(allocator, VkCommandBuffer, cmd_bufs, desc->cmd_list_count);
    SPUD_DELETE_ARRAY(allocator, VkSemaphore, wait_sems, wait_count);
    SPUD_DELETE_ARRAY(allocator, VkPipelineStageFlags, wait_stages, wait_count);
    SPUD_DELETE_ARRAY(allocator, uint64_t, wait_values, wait_count);
    SPUD_DELETE_ARRAY(allocator, VkSemaphore, signal_sems, signal_count);
    SPUD_DELETE_ARRAY(allocator, uint64_t, signal_values, signal_count);
    return r == VK_SUCCESS ? SPUD_SUCCESS : SPUDRESULT_API_SPECIFIC_FAILURE;
}

void spudgpu_queue_wait_idle(spudgpu_command_queue queue) {
    if (!queue) return;
    spudgpuvulkan___queue_enter(queue);
    vkQueueWaitIdle(queue->_queue_vk);
    spudgpuvulkan___queue_leave(queue);
}

void spudgpu_cmd_push_constants(
    spudgpu_command_list    cmd,
    spudgpu_shader_pipeline pipeline,
    uint32_t                offset,
    uint32_t                size,
    const void             *data) {
    if (!cmd || !pipeline || !data || size == 0) return;

    /* Collect stage flags from all push constant ranges that overlap [offset, offset+size). */
    VkShaderStageFlags stages = 0;
    uint32_t range_count = pipeline->_desc.push_constant_range_count;
    for (uint32_t i = 0; i < range_count; i++) {
        const spudgpu_push_constant_range_desc *r = &pipeline->_desc.push_constant_ranges[i];
        // Every stage, through the one translation the pipeline layout was
        // made with: a mesh- or task-only range used to give no stages here,
        // and the push was dropped.
        if (r->offset < offset + size && r->offset + r->size > offset)
            stages |= spudgpuvulkan___shader_stages(r->stage_flags);
    }
    if (!stages) return;

    vkCmdPushConstants(
        cmd->_command_buffer_vk,
        pipeline->_pipeline_layout_vk,
        stages, offset, size, data);
}

void spudgpu_cmd_push_constants_compute(
    spudgpu_command_list     cmd,
    spudgpu_compute_pipeline pipeline,
    uint32_t                 offset,
    uint32_t                 size,
    const void              *data) {
    if (!cmd || !pipeline || !data || size == 0) return;
    if (!pipeline->_desc.push_constant_range_count) return;

    /* Every range of a compute pipeline's layout is created with the compute
     * stage alone (spudgpu_create_compute_pipeline), so there are no stage
     * flags to collect from the ranges. */
    vkCmdPushConstants(
        cmd->_command_buffer_vk,
        pipeline->_pipeline_layout_vk,
        VK_SHADER_STAGE_COMPUTE_BIT, offset, size, data);
}

void spudgpu_cmd_bind_bindless_resources(
    spudgpu_command_list cmd,
    spudgpu_shader_pipeline pipeline,
    uint32_t set_index) {
    if (!cmd || !pipeline) return;
    if (set_index >= SPUDGPU_MAX_DESCRIPTOR_SET_LAYOUTS) return;
    spudgpu_device_vulkan *device = &cmd->_allocator._device;
    if (!device->_bindless) return;

    vkCmdBindDescriptorSets(
        cmd->_command_buffer_vk,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipeline->_pipeline_layout_vk,
        set_index,
        1, &device->_bindless->set_vk,
        0, NULL);
}

void spudgpu_cmd_bind_bindless_resources_compute(
    spudgpu_command_list cmd,
    spudgpu_compute_pipeline pipeline,
    uint32_t set_index) {
    if (!cmd || !pipeline) return;
    if (set_index >= SPUDGPU_MAX_DESCRIPTOR_SET_LAYOUTS) return;
    spudgpu_device_vulkan *device = &cmd->_allocator._device;
    if (!device->_bindless) return;

    vkCmdBindDescriptorSets(
        cmd->_command_buffer_vk,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        pipeline->_pipeline_layout_vk,
        set_index,
        1, &device->_bindless->set_vk,
        0, NULL);
}

void spudgpu_cmd_bind_compute_pipeline(
    spudgpu_command_list cmd,
    spudgpu_compute_pipeline pipeline) {
    if (!cmd || !pipeline) return;

    vkCmdBindPipeline(
        cmd->_command_buffer_vk,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        pipeline->_pipeline_vk);
}

void spudgpu_cmd_dispatch(
    spudgpu_command_list cmd,
    uint32_t group_count_x,
    uint32_t group_count_y,
    uint32_t group_count_z) {
    if (!cmd) return;
    vkCmdDispatch(cmd->_command_buffer_vk, group_count_x, group_count_y, group_count_z);
}

void spudgpu_cmd_draw_indirect(
    spudgpu_command_list cmd,
    spudgpu_buffer buffer,
    uint64_t offset,
    uint32_t draw_count,
    uint32_t stride) {
    if (!cmd || !buffer || draw_count == 0) return;
    if (draw_count > spudgpuvulkan___max_draw_indirect_count(&cmd->_allocator._device)) return;
    if (stride % 4 != 0 || stride < sizeof(spudgpu_draw_indirect_args)) return;
    vkCmdDrawIndirect(
        cmd->_command_buffer_vk,
        buffer->_buffer_vk,
        (VkDeviceSize) offset,
        draw_count,
        stride);
}

void spudgpu_cmd_draw_indexed_indirect(
    spudgpu_command_list cmd,
    spudgpu_buffer buffer,
    uint64_t offset,
    uint32_t draw_count,
    uint32_t stride) {
    if (!cmd || !buffer || draw_count == 0) return;
    if (draw_count > spudgpuvulkan___max_draw_indirect_count(&cmd->_allocator._device)) return;
    if (stride % 4 != 0 || stride < sizeof(spudgpu_draw_indexed_indirect_args)) return;
    vkCmdDrawIndexedIndirect(
        cmd->_command_buffer_vk,
        buffer->_buffer_vk,
        (VkDeviceSize) offset,
        draw_count,
        stride);
}

#if SPUDGPU_EXT_MESH_SHADING
void spudgpu_cmd_dispatch_mesh(
    spudgpu_command_list cmd,
    uint32_t group_count_x,
    uint32_t group_count_y,
    uint32_t group_count_z) {
    if (!cmd) return;
    // Loaded once at device creation via vkGetDeviceProcAddr - see
    // spudgpuvulkancontext.c - since VK_EXT_mesh_shader's entry points
    // don't come statically linked. NULL here means the device this
    // command list's allocator was created against never enabled mesh
    // shading (it was created without SPUDGPU_FEATURE_MESH_SHADING).
    PFN_vkCmdDrawMeshTasksEXT draw_mesh_tasks = cmd->_allocator._device._vkCmdDrawMeshTasksEXT;
    if (!draw_mesh_tasks) return;
    draw_mesh_tasks(cmd->_command_buffer_vk, group_count_x, group_count_y, group_count_z);
}
#endif

#if SPUDGPU_EXT_DEPTH_BOUNDS_TEST
void spudgpu_cmd_set_depth_bounds(
    spudgpu_command_list cmd,
    float min_depth_bounds,
    float max_depth_bounds) {
    if (!cmd) return;
    // Without the feature depthBounds was never switched on for this device,
    // so vkCmdSetDepthBounds would be invalid to call at all.
    const bool enabled = (cmd->_allocator._device._features & SPUDGPU_FEATURE_DEPTH_BOUNDS_TEST) != 0;
    assert(enabled && "spudgpu_cmd_set_depth_bounds: the device was created without SPUDGPU_FEATURE_DEPTH_BOUNDS_TEST");
    if (!enabled) return;
    vkCmdSetDepthBounds(cmd->_command_buffer_vk, min_depth_bounds, max_depth_bounds);
}
#endif

// ---------------------------------------------------------------------------
// spudgpu_cmd_pipeline_barrier — SPUDGPU_RESOURCE_STATE -> VkAccessFlags /
// VkPipelineStageFlags / VkImageLayout. Distinct from the SPUDGPU_IMAGE_LAYOUT
// mechanism spudgpu_cmd_image_barrier already uses (spudgpuvulkanrenderpass.c)
// — that path stays the one used for swap chain/render target transitions;
// this one exists for the more general resource-state model (buffers, plus
// images expressed the same way) declared alongside SPUDGPU_RESOURCE_STATE.
// ---------------------------------------------------------------------------

static VkAccessFlags spudgpu_vk___resource_state_to_access(SPUDGPU_RESOURCE_STATE state) {
    switch (state) {
    case SPUDGPU_RESOURCE_STATE_VERTEX_BUFFER:
        return VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    case SPUDGPU_RESOURCE_STATE_INDEX_BUFFER:
        return VK_ACCESS_INDEX_READ_BIT;
    // Read as well as written (a load op of LOAD, blending, the depth test),
    // as layout_to_access in spudgpuvulkanrenderpass.c.
    case SPUDGPU_RESOURCE_STATE_RENDER_TARGET:
        return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    case SPUDGPU_RESOURCE_STATE_DEPTH_WRITE:
        return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    case SPUDGPU_RESOURCE_STATE_SHADER_RESOURCE:
        return VK_ACCESS_SHADER_READ_BIT;
    case SPUDGPU_RESOURCE_STATE_UNORDERED_ACCESS:
        return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    case SPUDGPU_RESOURCE_STATE_INDIRECT_ARGUMENT:
        return VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    case SPUDGPU_RESOURCE_STATE_COPY_SOURCE:
        return VK_ACCESS_TRANSFER_READ_BIT;
    case SPUDGPU_RESOURCE_STATE_COPY_DEST:
        return VK_ACCESS_TRANSFER_WRITE_BIT;
    case SPUDGPU_RESOURCE_STATE_PRESENT:
    case SPUDGPU_RESOURCE_STATE_COMMON:
    default:
        return 0;
    }
}

static VkPipelineStageFlags spudgpu_vk___resource_state_to_stage(SPUDGPU_RESOURCE_STATE state) {
    switch (state) {
    case SPUDGPU_RESOURCE_STATE_VERTEX_BUFFER:
    case SPUDGPU_RESOURCE_STATE_INDEX_BUFFER:
        return VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
    case SPUDGPU_RESOURCE_STATE_RENDER_TARGET:
        return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    case SPUDGPU_RESOURCE_STATE_DEPTH_WRITE:
        return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    case SPUDGPU_RESOURCE_STATE_SHADER_RESOURCE:
    case SPUDGPU_RESOURCE_STATE_UNORDERED_ACCESS:
        return VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
    case SPUDGPU_RESOURCE_STATE_INDIRECT_ARGUMENT:
        return VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
    case SPUDGPU_RESOURCE_STATE_COPY_SOURCE:
    case SPUDGPU_RESOURCE_STATE_COPY_DEST:
        return VK_PIPELINE_STAGE_TRANSFER_BIT;
    case SPUDGPU_RESOURCE_STATE_PRESENT:
        return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    case SPUDGPU_RESOURCE_STATE_COMMON:
    default:
        return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    }
}

static VkImageLayout spudgpu_vk___resource_state_to_image_layout(SPUDGPU_RESOURCE_STATE state) {
    switch (state) {
    case SPUDGPU_RESOURCE_STATE_RENDER_TARGET:
        return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    case SPUDGPU_RESOURCE_STATE_DEPTH_WRITE:
        return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    case SPUDGPU_RESOURCE_STATE_SHADER_RESOURCE:
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    case SPUDGPU_RESOURCE_STATE_UNORDERED_ACCESS:
        return VK_IMAGE_LAYOUT_GENERAL;
    case SPUDGPU_RESOURCE_STATE_COPY_SOURCE:
        return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    case SPUDGPU_RESOURCE_STATE_COPY_DEST:
        return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    case SPUDGPU_RESOURCE_STATE_PRESENT:
        return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    case SPUDGPU_RESOURCE_STATE_COMMON:
    default:
        return VK_IMAGE_LAYOUT_UNDEFINED;
    }
}

void spudgpu_cmd_pipeline_barrier(
    spudgpu_command_list cmd,
    const spudgpu_buffer_barrier *buffer_barriers,
    uint32_t buffer_barrier_count,
    const spudgpu_image_barrier *image_barriers,
    uint32_t image_barrier_count) {
    if (!cmd) return;
    if (buffer_barrier_count > 0 && !buffer_barriers) return;
    for (uint32_t buffer_barriers_i = 0; buffer_barriers_i < buffer_barrier_count; ++buffer_barriers_i)
        if (spud_struct_check(&buffer_barriers[buffer_barriers_i].base, SPUD_STRUCT_TYPE_GPU_BUFFER_BARRIER) != SPUD_SUCCESS)
            return;
    if (image_barrier_count > 0 && !image_barriers) return;
    for (uint32_t image_barriers_i = 0; image_barriers_i < image_barrier_count; ++image_barriers_i)
        if (spud_struct_check(&image_barriers[image_barriers_i].base, SPUD_STRUCT_TYPE_GPU_IMAGE_BARRIER) != SPUD_SUCCESS)
            return;
    if (buffer_barrier_count == 0 && image_barrier_count == 0) return;

    VkPipelineStageFlags src_stage = 0;
    VkPipelineStageFlags dst_stage = 0;

    // Both arrays or neither; nothing is recorded if either can't be made.
    const SPUD_ALLOCATOR *allocator = &cmd->head.allocator;
    VkBufferMemoryBarrier *vk_buffer_barriers =
        buffer_barrier_count ? SPUD_NEW_ARRAY_ZEROED(allocator, VkBufferMemoryBarrier, buffer_barrier_count) : NULL;
    if (buffer_barrier_count && !vk_buffer_barriers) return;
    VkImageMemoryBarrier *vk_image_barriers =
        image_barrier_count ? SPUD_NEW_ARRAY_ZEROED(allocator, VkImageMemoryBarrier, image_barrier_count) : NULL;
    if (image_barrier_count && !vk_image_barriers) {
        SPUD_DELETE_ARRAY(allocator, VkBufferMemoryBarrier, vk_buffer_barriers, buffer_barrier_count);
        return;
    }
    for (uint32_t i = 0; i < buffer_barrier_count; i++) {
        const spudgpu_buffer_barrier *b = &buffer_barriers[i];
        vk_buffer_barriers[i].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        vk_buffer_barriers[i].srcAccessMask = spudgpu_vk___resource_state_to_access(b->state_before);
        vk_buffer_barriers[i].dstAccessMask = spudgpu_vk___resource_state_to_access(b->state_after);
        vk_buffer_barriers[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        vk_buffer_barriers[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        vk_buffer_barriers[i].buffer = b->buffer->_buffer_vk;
        vk_buffer_barriers[i].offset = 0;
        vk_buffer_barriers[i].size = VK_WHOLE_SIZE;

        src_stage |= spudgpu_vk___resource_state_to_stage(b->state_before);
        dst_stage |= spudgpu_vk___resource_state_to_stage(b->state_after);
    }

    for (uint32_t i = 0; i < image_barrier_count; i++) {
        const spudgpu_image_barrier *b = &image_barriers[i];
        VkImageAspectFlags aspect = (b->state_before == SPUDGPU_RESOURCE_STATE_DEPTH_WRITE ||
                                      b->state_after == SPUDGPU_RESOURCE_STATE_DEPTH_WRITE)
                                         ? VK_IMAGE_ASPECT_DEPTH_BIT
                                         : VK_IMAGE_ASPECT_COLOR_BIT;

        vk_image_barriers[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        vk_image_barriers[i].srcAccessMask = spudgpu_vk___resource_state_to_access(b->state_before);
        vk_image_barriers[i].dstAccessMask = spudgpu_vk___resource_state_to_access(b->state_after);
        vk_image_barriers[i].oldLayout = spudgpu_vk___resource_state_to_image_layout(b->state_before);
        vk_image_barriers[i].newLayout = spudgpu_vk___resource_state_to_image_layout(b->state_after);
        vk_image_barriers[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        vk_image_barriers[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        vk_image_barriers[i].image = b->image->_image_vk;
        vk_image_barriers[i].subresourceRange.aspectMask = aspect;
        vk_image_barriers[i].subresourceRange.baseMipLevel = 0;
        vk_image_barriers[i].subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        vk_image_barriers[i].subresourceRange.baseArrayLayer = 0;
        vk_image_barriers[i].subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

        src_stage |= spudgpu_vk___resource_state_to_stage(b->state_before);
        dst_stage |= spudgpu_vk___resource_state_to_stage(b->state_after);
    }

    if (!src_stage) src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    if (!dst_stage) dst_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

    vkCmdPipelineBarrier(
        cmd->_command_buffer_vk,
        src_stage, dst_stage,
        0,
        0, NULL,
        buffer_barrier_count, vk_buffer_barriers,
        image_barrier_count, vk_image_barriers);

    SPUD_DELETE_ARRAY(allocator, VkBufferMemoryBarrier, vk_buffer_barriers, buffer_barrier_count);
    SPUD_DELETE_ARRAY(allocator, VkImageMemoryBarrier, vk_image_barriers, image_barrier_count);
}

#if __cplusplus
}
#endif

#endif // SPUDGPU_COMPILE_VULKAN_API
