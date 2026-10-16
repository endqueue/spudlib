//
// Created by nathanmoore on 5/18/26.
//

#if SPUDGPU_COMPILE_VULKAN_API

#ifndef SPUDLIB_SPUDGPUVULKAN_H
#define SPUDLIB_SPUDGPUVULKAN_H

#include "spudgpu.h"
#include <vulkan/vulkan.h>
#include <stdatomic.h>

typedef struct spudgpu_instance_t {
#if _DEBUG
    const char *_debug_name;
#endif
    VkInstance _instance_vk;
    // Effectively this -- spudgpu_device_t *_device_pointer_array[device_count];
    // We need to keep track of newly allocated spudgpu_device's to prevent memory leaks.
    uint64_t *_devices_pointer_array;
    uint32_t _devices_count;
} spudgpu_instance_vulkan;

#define SPUDGPU_BINDLESS_MAX_SLOTS_PER_CLASS 4096

// Lazily created the first time bindless capabilities/registration is used on
// a device — most devices that never touch the bindless API pay nothing.
typedef struct spudgpu_bindless_state_vulkan {
    spudgpu_descriptor_set_layout layout; // Wraps the single global layout.
    VkDescriptorPool pool_vk;
    VkDescriptorSet set_vk;

    uint32_t sampled_image_next_unused;
    uint32_t storage_image_next_unused;
    uint32_t storage_buffer_next_unused;

    // Stack of freed indices, reused before drawing from *_next_unused.
    uint32_t sampled_image_free_stack[SPUDGPU_BINDLESS_MAX_SLOTS_PER_CLASS];
    uint32_t sampled_image_free_count;
    uint32_t storage_image_free_stack[SPUDGPU_BINDLESS_MAX_SLOTS_PER_CLASS];
    uint32_t storage_image_free_count;
    uint32_t storage_buffer_free_stack[SPUDGPU_BINDLESS_MAX_SLOTS_PER_CLASS];
    uint32_t storage_buffer_free_count;
} spudgpu_bindless_state_vulkan;

typedef struct spudgpu_device_t {
#if _DEBUG
    const char *_debug_name;
#endif
    VkDevice _logical_device_vk;
    VkPhysicalDevice _physical_device_vk;
    spudgpu_instance_vulkan _instance;
    SPUDGPU_DEVICE_PROPERTIES _properties;
    VkPhysicalDeviceProperties _properties_vk;
    VkPhysicalDeviceFeatures _features_vk;
    uint32_t _graphics_queue_family_index;

    // Set once at device creation (spudgpuvulkancontext.c) after checking
    // both VK_EXT_mesh_shader's presence in vkEnumerateDeviceExtensionProperties
    // *and* querying VkPhysicalDeviceMeshShaderFeaturesEXT::meshShader via
    // vkGetPhysicalDeviceFeatures2 - the extension being enumerable doesn't
    // by itself guarantee the feature bit is supported. Mirrors
    // spudgpu_mesh_shading_capabilities (see SPUDGPU_EXT_MESH_SHADING in
    // spudgpu.h).
    bool _mesh_shading_supported;
    uint32_t _mesh_shading_max_output_vertices;
    uint32_t _mesh_shading_max_output_primitives;
    uint32_t _mesh_shading_max_workgroup_invocations;

    // Set once at device creation (spudgpuvulkancontext.c) from
    // VkPhysicalDeviceFeatures::depthBounds - queried before the logical
    // device exists (unlike _properties_vk/_features_vk below, which are
    // only populated after), since it must also decide whether to request
    // the depthBounds feature bit when creating the device. Mirrors
    // spudgpu_depth_bounds_capabilities (see SPUDGPU_EXT_DEPTH_BOUNDS_TEST
    // in spudgpu.h).
    bool _depth_bounds_test_supported;
    // NULL when _mesh_shading_supported is false. Loaded via
    // vkGetDeviceProcAddr since VK_EXT_mesh_shader's entry points don't come
    // statically linked the way VK_KHR_swapchain's do (the only other
    // device extension this backend has ever enabled).
    PFN_vkCmdDrawMeshTasksEXT _vkCmdDrawMeshTasksEXT;
    // NULL when the instance has no VK_EXT_debug_utils
    // (spudgpu_cmd_begin_debug_label records nothing then).
    PFN_vkCmdBeginDebugUtilsLabelEXT _vkCmdBeginDebugUtilsLabelEXT;
    PFN_vkCmdEndDebugUtilsLabelEXT _vkCmdEndDebugUtilsLabelEXT;

    // Lazily allocated by the first spudgpu_get_bindless_capabilities /
    // spudgpu_bindless_register_* / spudgpu_get_bindless_descriptor_set_layout
    // call. NULL until then. Owned by this device; never copied by value —
    // every struct above embeds spudgpu_device_vulkan by value, so mutating
    // bindless state must go through the live spudgpu_device pointer, not a
    // snapshot copy.
    spudgpu_bindless_state_vulkan *_bindless;
} spudgpu_device_vulkan;

typedef struct spudgpu_command_queue_t {
#if _DEBUG
    const char *_debug_name;
#endif
    VkQueue _queue_vk;
    uint32_t _queue_family_index;
} spudgpu_command_queue_vulkan;

typedef struct spudgpu_command_allocator_t {
#if _DEBUG
    const char *_debug_name;
#endif
    VkCommandPool _command_pool_vk;
    spudgpu_device_vulkan _device;
    uint32_t _queue_family_index;
    // SPUDGPU_COMMAND_LIST_TYPE_BUNDLE allocators hand out
    // VK_COMMAND_BUFFER_LEVEL_SECONDARY buffers (see spudgpu_create_command_list);
    // every other type hands out PRIMARY.
    SPUDGPU_COMMAND_LIST_TYPE _type;
} spudgpu_command_allocator_vulkan;

typedef struct spudgpu_command_list_t {
#if _DEBUG
    const char *_debug_name;
#endif
    VkCommandBuffer _command_buffer_vk;
    spudgpu_command_allocator_vulkan _allocator;

    // Set by spudgpu_cmd_begin_rendering, cleared by spudgpu_cmd_end_rendering
    // (spudgpuvulkanrenderpass.c). Lets spudgpu_cmd_clear_color_attachment/
    // depth_attachment take the cheap vkCmdClearAttachments fast path — valid
    // only against attachments already bound in the currently-active
    // rendering instance, addressed by index, not by view identity — instead
    // of nesting a redundant begin/end pair, which would be an outright
    // Vulkan validation error regardless of cost: vkCmdBeginRendering cannot
    // be called while a rendering instance is already active on this command
    // buffer.
    bool _rendering_active;
    // VkRenderingInfo::layerCount of the pass being recorded.
    uint32_t _bound_layer_count;
    VkImageView _bound_color_attachments[SPUDGPU_MAX_COLOR_ATTACHMENTS];
    uint32_t _bound_color_attachment_count;
    VkImageView _bound_depth_attachment_view; // VK_NULL_HANDLE if none bound
    // The timestamp the open pass writes at spudgpu_cmd_end_rendering
    // (spudgpu_rendering_begin_desc::timestamp_set); VK_NULL_HANDLE for none.
    VkQueryPool _end_timestamp_pool_vk;
    uint32_t _end_timestamp_index;
} spudgpu_command_list_vulkan;

typedef struct spudgpu_timestamp_set_t {
#if _DEBUG
    const char *_debug_name;
#endif
    VkQueryPool _query_pool_vk;
    VkDevice _logical_device_vk;
    uint32_t _count;
    // VkPhysicalDeviceLimits::timestampPeriod: nanoseconds a tick.
    float _period_ns;
} spudgpu_timestamp_set_vulkan;

typedef struct spudgpu_buffer_t {
#if _DEBUG
    const char *_debug_name;
#endif
    VkBuffer _buffer_vk;
    // The buffer's own allocation, or its heap's when _heap is set: the
    // buffer is bound at _memory_offset_vk of it (0 for its own) and covers
    // _memory_size_vk bytes from there, which may be more than _desc.size.
    // Those and the property flags of the memory type are what
    // spudgpu_flush_buffer/spudgpu_invalidate_buffer need to form a range
    // Vulkan accepts.
    VkDeviceMemory _memory_vk;
    VkDeviceSize _memory_offset_vk;
    VkDeviceSize _memory_size_vk;
    VkMemoryPropertyFlags _memory_properties_vk;
    spudgpu_buffer_desc _desc;
    spudgpu_device_vulkan _device;
    // NULL for a buffer with its own allocation; the heap it was placed in
    // otherwise (spudgpu_create_placed_buffer), whose memory it never frees.
    spudgpu_heap _heap;
} spudgpu_buffer_vulkan;

typedef struct spudgpu_buffer_view_t {
#if _DEBUG
    const char *_debug_name;
#endif
    VkBufferView _buffer_view_vk;
    spudgpu_buffer_view_desc _desc;
} spudgpu_buffer_view_vulkan;

typedef struct spudgpu_image_t {
#if _DEBUG
    const char *_debug_name;
#endif
    VkImage _image_vk;
    // The image's own allocation, or its heap's when _heap is set.
    VkDeviceMemory _memory_vk;
    VkFormat _format_vk;
    spudgpu_image_desc _desc;
    spudgpu_device_vulkan _device;
    // NULL for an image with its own allocation; the heap it was placed in
    // otherwise (spudgpu_create_placed_image), whose memory it never frees.
    spudgpu_heap _heap;
} spudgpu_image_vulkan;

typedef struct spudgpu_heap_t {
#if _DEBUG
    const char *_debug_name;
    // Buffers and images placed in the heap and not yet destroyed.
    // spudgpu_destroy_heap asserts it is 0. Atomic, because resources in
    // one heap may be placed and destroyed on several threads at once.
    _Atomic uint64_t _placed_count;
#endif
    VkDeviceMemory _memory_vk;
    // The memory type the heap was allocated from (see spudgpuvulkanheap.c
    // for how it is chosen), and its property flags.
    uint32_t _memory_type_index_vk;
    VkMemoryPropertyFlags _memory_properties_vk;
    // The whole allocation, mapped from creation to destruction, for a heap
    // of a mappable kind; NULL for SPUDGPU_MEMORY_KIND_DEVICE. Vulkan allows
    // one mapping of a VkDeviceMemory at a time, so the heap maps it once
    // and its buffers' spudgpu_map_buffer hands out pointers into it.
    void *_mapped;
    spudgpu_heap_desc _desc;
    spudgpu_device_vulkan _device;
} spudgpu_heap_vulkan;

typedef struct spudgpu_image_view_t {
#if _DEBUG
    const char *_debug_name;
#endif
    VkImageView _image_view_vk;
    spudgpu_image_view_desc _desc;
} spudgpu_image_view_vulkan;

typedef struct spudgpu_surface_t {
#if _DEBUG
    const char *_debug_name;
#endif
    VkSurfaceKHR _surface_vk;
    spudgpu_instance_vulkan _instance;
} spudgpu_surface_vulkan;

typedef struct spudgpu_fence_t {
#if _DEBUG
    const char *_debug_name;
#endif
    // A timeline semaphore, not a VkFence: spudgpu_fence is a counter that
    // only goes up, which is what a timeline semaphore is and a VkFence
    // (one bit, reset by hand) is not.
    VkSemaphore _semaphore_vk;
    VkDevice _device_vk;
} spudgpu_fence_vulkan;

typedef struct spudgpu_semaphore_t {
#if _DEBUG
    const char *_debug_name;
#endif
    VkSemaphore _semaphore_vk;
    VkDevice _device_vk;
} spudgpu_semaphore_vulkan;

typedef struct spudgpu_swap_chain_t {
#if _DEBUG
    const char *_debug_name;
#endif
    VkSwapchainKHR _swapchain_vk;
    spudgpu_swap_chain_desc _desc;
    spudgpu_device_vulkan _device;
    VkQueue _present_queue_vk; // From desc.queue, set at creation - see spudgpu_swap_chain_desc::queue.
    VkSurfaceKHR _surface_vk;
    VkFormat _format_vk;
    VkExtent2D _extent_vk;
    VkImage *_swapchain_images_vk;
    uint32_t _swapchain_images_count;
    spudgpu_image_view_vulkan *_swapchain_image_views_vk;
    uint32_t _swapchain_image_views_count;

    spudgpu_semaphore_vulkan *_image_available_semaphores; // One per frame-in-flight
    spudgpu_semaphore_vulkan *_render_finished_semaphores; // One per frame-in-flight
    // One per frame-in-flight. Plain VkFences, internal to the swap chain:
    // signaled by a submission made with it, waited on and reset by acquire.
    VkFence *_in_flight_fences_vk;
    uint32_t _max_frames_in_flight;
    uint32_t _current_frame; // Cycles 0..max_frames_in_flight-1
    uint32_t _current_image_index; // Set by acquire, read by present
} spudgpu_swap_chain_vulkan;

typedef struct spudgpu_shader_module_t {
#if _DEBUG
    const char *_debug_name;
#endif
    VkShaderModule _shader_module_vk;
    VkShaderStageFlagBits _stage_vk;
    spudgpu_device_vulkan _device;
} spudgpu_shader_module_vulkan;


typedef struct spudgpu_shader_pipeline_t {
#if _DEBUG
    const char *_debug_name;
#endif
    VkPipeline _pipeline_vk;
    VkPipelineLayout _pipeline_layout_vk;
    spudgpu_device_vulkan _device;
    spudgpu_shader_pipeline_desc _desc;
} spudgpu_shader_pipeline_vulkan;

typedef struct spudgpu_compute_pipeline_t {
#if _DEBUG
    const char *_debug_name;
#endif
    VkPipeline _pipeline_vk;
    VkPipelineLayout _pipeline_layout_vk;
    spudgpu_device_vulkan _device;
    spudgpu_compute_pipeline_desc _desc;
} spudgpu_compute_pipeline_vulkan;

typedef struct spudgpu_sampler_t {
#if _DEBUG
    const char *_debug_name;
#endif
    VkSampler _sampler_vk;
    spudgpu_device_vulkan _device;
} spudgpu_sampler_vulkan;

typedef struct spudgpu_descriptor_set_layout_t {
#if _DEBUG
    const char *_debug_name;
#endif
    VkDescriptorSetLayout _layout_vk;
    spudgpu_device_vulkan _device;
    spudgpu_descriptor_set_layout_desc _desc;
} spudgpu_descriptor_set_layout_vulkan;

typedef struct spudgpu_descriptor_pool_t {
#if _DEBUG
    const char *_debug_name;
#endif
    VkDescriptorPool _pool_vk;
    spudgpu_device_vulkan _device;
    spudgpu_descriptor_pool_desc _desc;
} spudgpu_descriptor_pool_vulkan;

typedef struct spudgpu_descriptor_set_t {
#if _DEBUG
    const char *_debug_name;
#endif
    VkDescriptorSet _set_vk;
    spudgpu_descriptor_pool_vulkan _pool;
} spudgpu_descriptor_set_vulkan;

VkFormat convert_spud_to_vulkan_format(SPUDGPU_FORMAT format);

// Shared by spudgpu_create_buffer / spudgpu_create_image and the heap calls
// (spudgpuvulkanheap.c), so a buffer or image is checked and described the
// same way whether it has its own allocation or is placed in a heap.

// spudgpu_create_buffer's desc checks, in its order, after its argument
// checks: size, usage, memory kind, usage against kind, device.
SPUDRESULT spudgpuvulkan___check_buffer_desc(
    const SPUDGPU_DEVICE_PROPERTIES *properties,
    const spudgpu_buffer_desc *desc);
// The VkBufferCreateInfo of a buffer of [desc].
void spudgpuvulkan___buffer_create_info(
    const spudgpu_buffer_desc *desc,
    VkBufferCreateInfo *out_info);
// The first memory type in [typeFilter] with the properties [kind]
// documents in spudgpu.h, and that type's flags. UINT32_MAX if none.
uint32_t spudgpuvulkan___memory_type_for_kind(
    VkPhysicalDevice physicalDevice,
    uint32_t typeFilter,
    SPUDGPU_MEMORY_KIND kind,
    VkMemoryPropertyFlags *out_flags);
// spudgpu_create_image's desc checks, in its order, after its argument
// checks.
SPUDRESULT spudgpuvulkan___check_image_desc(const spudgpu_image_desc *desc);
// The VkImageCreateInfo of an image of [desc].
void spudgpuvulkan___image_create_info(
    const spudgpu_image_desc *desc,
    VkImageCreateInfo *out_info);

// Defined in spudgpuvulkandescriptors.c. Idempotent — safe to call more than
// once, but must be called at least once before the device's first
// by-value copy (command allocator/buffer/image/pipeline creation, ...) so
// every later snapshot observes a populated _bindless pointer rather than a
// stale NULL from before lazy initialization.
SPUDRESULT spudgpuvulkan___ensure_bindless_state(spudgpu_device device);
void spudgpuvulkan___destroy_bindless_state(spudgpu_device device);

#endif //SPUDLIB_SPUDGPUVULKAN_H

#endif //SPUDGPU_COMPILE_VULKAN_API
