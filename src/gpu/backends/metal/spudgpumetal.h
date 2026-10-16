//
// SpudGPU Metal backend - internal opaque type definitions.
//

#if SPUDGPU_COMPILE_METAL_API

#ifndef SPUDLIB_SPUDGPUMETAL_H
#define SPUDLIB_SPUDGPUMETAL_H

#include "spudgpu.h"
#include <Metal/Metal.h>
#include <stdatomic.h>
#include <QuartzCore/QuartzCore.h>

// Metal command queues aren't partitioned into families the way Vulkan/D3D12
// queues are (see the natives-header asymmetries in ../../../CLAUDE.md) - any
// id<MTLCommandQueue> can encode render, compute, or blit work. These three
// arrays exist purely to keep spudgpu_get_command_queue's (type, index) shape
// symmetric with the other backends; each slot is still just a generic Metal
// queue.
#define SPUD_METAL_COMMAND_QUEUE_COUNT_PER_FAMILY 4

// Metal's setViewports:count:/setScissorRects:count: always set from slot 0
// with no first_viewport-style offset (unlike Vulkan's vkCmdSetViewport),
// so spudgpu_cmd_set_viewports/set_scissor_rects patch a persistent
// per-command-list array up to this many slots and resend the whole thing -
// see spudgpu_command_list_metal below. 16 matches Metal's real multi-
// viewport/vertex-amplification hardware limit.
#define SPUDGPU_METAL_MAX_VIEWPORTS 16

// Metal has no persistent push-constant-block state the way Vulkan/D3D12 do -
// -setVertexBytes:/-setFragmentBytes: replaces the entire bound region at an
// index each call, so a caller updating one sub-range at a time needs the
// full block re-sent every time. This is the scratch buffer size backing
// that; comfortably above Vulkan's guaranteed minimum maxPushConstantsSize
// (128 bytes).
#define SPUDGPU_METAL_PUSH_CONSTANTS_SIZE 256

// The Metal Shading Language side of whatever cross-compiles this project's
// shaders must bind its push-constant struct at this fixed buffer/argument
// index (e.g. `[[buffer(30)]]`) for spudgpu_cmd_push_constants below to
// reach it - Metal has no other channel for push-constant-style data.
#define SPUDGPU_METAL_PUSH_CONSTANTS_BUFFER_INDEX 30

// Metal shares one buffer-argument-index namespace across vertex-attribute
// buffers and everything else bound via -setVertexBuffer:/-setFragmentBuffer:
// - unlike Vulkan/D3D12, which keep vertex bindings and descriptor sets in
// separate namespaces. Descriptor sets are pinned to the SPUDGPU_MAX_DESCRIPTOR_SET_LAYOUTS
// indices directly below the push-constant buffer (see
// spudgpumetal___internal_cross_compile_spirv_to_msl in spudgpumetalshader.m,
// which pins SPIR-V descriptor set N's whole argument buffer to MSL
// buffer(SPUDGPU_METAL_DESCRIPTOR_SET_BUFFER_INDEX_BASE + N) via
// SPVC_MSL_ARGUMENT_BUFFER_BINDING), leaving indices below this free for
// vertex attribute buffers.
#define SPUDGPU_METAL_DESCRIPTOR_SET_BUFFER_INDEX_BASE \
	(SPUDGPU_METAL_PUSH_CONSTANTS_BUFFER_INDEX - SPUDGPU_MAX_DESCRIPTOR_SET_LAYOUTS)

typedef struct spudgpu_instance_t {
#if _DEBUG
	const char *_debug_name;
#endif
	// Metal has no top-level instance/factory object (unlike VkInstance or
	// IDXGIFactory) - devices come straight from MTLCopyAllDevices(). This
	// struct exists purely so spudgpu_instance can still own and clean up
	// every spudgpu_device handed out by spudgpu_enumerate_devices, matching
	// the ownership contract the Vulkan/D3D12 backends use (there is no
	// public spudgpu_destroy_device).
	// The array spudgpu_enumerate_devices hands out, made by its first call
	// and freed with the instance.
	spudgpu_device *_devices;
	uint32_t _devices_count;
} spudgpu_instance_metal;

typedef struct spudgpu_device_t {
#if _DEBUG
	const char *_debug_name;
#endif
	id<MTLDevice> _device_mtl;
	SPUDGPU_DEVICE_PROPERTIES _properties;
	spudgpu_command_queue _cmd_queues_direct[SPUD_METAL_COMMAND_QUEUE_COUNT_PER_FAMILY];
	spudgpu_command_queue _cmd_queues_copy[SPUD_METAL_COMMAND_QUEUE_COUNT_PER_FAMILY];
	spudgpu_command_queue _cmd_queues_compute[SPUD_METAL_COMMAND_QUEUE_COUNT_PER_FAMILY];

	// Set once at device enumeration (spudgpumetalcontext.m) from
	// [_device_mtl supportsFamily:MTLGPUFamilyApple10] - unlike mesh shading
	// (unconditionally true on every Metal 3 device this backend targets),
	// depth bounds testing is a genuinely new (macOS/iOS 26+) primitive that
	// only the newest Apple GPU family exposes. Mirrors
	// spudgpu_depth_bounds_capabilities (see SPUDGPU_EXT_DEPTH_BOUNDS_TEST in
	// spudgpu.h).
	bool _depth_bounds_test_supported;
} spudgpu_device_metal;

typedef struct spudgpu_buffer_t {
#if _DEBUG
	const char *_debug_name;
#endif
	id<MTLBuffer> _buffer_mtl;
	spudgpu_device_metal * _parent_device;
	spudgpu_buffer_desc _desc;
	// NULL for a buffer made on the device; the heap it was placed in
	// otherwise (spudgpu_create_placed_buffer).
	spudgpu_heap _heap;
} spudgpu_buffer_metal;

typedef struct spudgpu_buffer_view_t {
#if _DEBUG
	const char *_debug_name;
#endif
	// The parent buffer lives in _desc.parent_buffer (set from the
	// authoritative `buffer` parameter at creation, not necessarily whatever
	// the caller's desc.parent_buffer was - see spudgpu_create_buffer_view)
	// - no separate redundant pointer field here.
	spudgpu_buffer_view_desc _desc;
} spudgpu_buffer_view_metal;

typedef struct spudgpu_image_t {
#if _DEBUG
	const char *_debug_name;
#endif
	id<MTLTexture> _texture_mtl;
	spudgpu_device_metal *_parent_device;
	spudgpu_image_desc _desc;
	// NULL for an image made on the device; the heap it was placed in
	// otherwise (spudgpu_create_placed_image).
	spudgpu_heap _heap;
} spudgpu_image_metal;

typedef struct spudgpu_heap_t {
#if _DEBUG
	const char *_debug_name;
	// Buffers and images placed in the heap and not yet destroyed.
	// spudgpu_destroy_heap asserts it is 0. Atomic, because resources in
	// one heap may be placed and destroyed on several threads at once.
	_Atomic uint64_t _placed_count;
#endif
	// MTLHeapTypePlacement, hazard tracked (see spudgpumetalheap.m).
	id<MTLHeap> _heap_mtl;
	// The resource options every buffer placed in the heap is made with:
	// the heap's storage and CPU cache modes, and its hazard tracking.
	MTLResourceOptions _resource_options_mtl;
	spudgpu_device_metal *_parent_device;
	spudgpu_heap_desc _desc;
} spudgpu_heap_metal;

// Shared by spudgpu_create_buffer / spudgpu_create_image and the heap calls
// (spudgpumetalheap.m), so a buffer or image is checked and described the
// same way whether it is made on the device or placed in a heap.

// spudgpu_create_buffer's desc checks, in its order, after its argument
// checks: size, usage, memory kind, usage against kind, device.
SPUDRESULT spudgpumetal___check_buffer_desc(
    const SPUDGPU_DEVICE_PROPERTIES *properties,
    const spudgpu_buffer_desc *desc);
// The storage and CPU cache modes a buffer of [memory_kind] is made with.
MTLResourceOptions spudgpumetal___buffer_resource_options(SPUDGPU_MEMORY_KIND memory_kind);
// spudgpu_create_image's desc checks, in its order, after its argument
// checks.
SPUDRESULT spudgpumetal___check_image_desc(const spudgpu_image_desc *desc);
// A texture descriptor for an image of [desc], which the caller releases.
// [desc] has passed spudgpumetal___check_image_desc.
MTLTextureDescriptor *spudgpumetal___new_texture_descriptor(const spudgpu_image_desc *desc);

typedef struct spudgpu_image_view_t {
#if _DEBUG
	const char *_debug_name;
#endif
	// Metal represents a texture view as another id<MTLTexture> (see
	// spudgpu_get_mtl_texture_view in spudgpu_metal_natives.h) rather than a
	// distinct view object - this is that view texture, not the parent's.
	id<MTLTexture> _texture_view_mtl;
	spudgpu_image_metal *_parent_image;
	spudgpu_image_view_desc _desc;
} spudgpu_image_view_metal;

typedef struct spudgpu_command_queue_t {
#if _DEBUG
	const char *_debug_name;
#endif
	id<MTLCommandQueue> _command_queue_mtl;
	spudgpu_device_metal *_parent_device;
} spudgpu_command_queue_metal;

typedef struct spudgpu_command_allocator_t {
#if _DEBUG
	const char *_debug_name;
#endif
	// No native Metal handle: command buffers are requested fresh from the
	// queue each time (see spudgpu_metal_natives.h) rather than reset from a
	// persistent allocator/pool object the way Vulkan/D3D12 do. _parent_queue
	// is that queue - resolved once from _desc.type at creation time (a
	// borrowed reference into the device's cached queue families, not owned
	// here) - and is what every spudgpu_create_command_list/
	// spudgpu_begin_command_list call for lists made from this allocator
	// pulls a fresh id<MTLCommandBuffer> from.
	spudgpu_device_metal *_parent_device;
	spudgpu_command_queue_metal *_parent_queue;
	spudgpu_command_allocator_desc _desc;
#if _DEBUG
	// The list of this allocator that is open for recording, or NULL. One
	// at a time on every backend (spudgpu_begin_command_list): Metal has no
	// allocator to contend for, D3D12 does, and a debug build asserts so the
	// rule is caught here too. Not atomic: an allocator is used from one
	// thread at a time.
	spudgpu_command_list _open_list;
#endif
} spudgpu_command_allocator_metal;

typedef struct spudgpu_command_list_t {
#if _DEBUG
	const char *_debug_name;
#endif
	id<MTLCommandBuffer> _command_buffer_mtl;
	spudgpu_command_allocator_metal *_parent_allocator;

	// Set by spudgpu_cmd_begin_rendering, cleared by spudgpu_cmd_end_rendering
	// (spudgpumetalrenderpass.m, not yet implemented) - all of
	// spudgpu_cmd_set_viewports/set_scissor_rects/set_vertex_buffers/
	// set_index_buffer/draw*/bind_pipeline/push_constants below encode
	// against this. Metal has no command-buffer-level draw/state API the
	// way Vulkan/D3D12 do - everything is a method on the render encoder
	// itself, which only exists between begin/end_rendering.
	id<MTLRenderCommandEncoder> _active_render_encoder;
	// What spudgpu_cmd_bind_pipeline last set on _active_render_encoder,
	// cleared when a render encoder is made. Not retained. Metal's
	// validation refuses setting a state the encoder already has, and two
	// pipelines can share one depth-stencil state, since Metal returns one
	// object for equal descriptors.
	id<MTLRenderPipelineState> _encoder_pipeline_state_mtl;
	id<MTLDepthStencilState> _encoder_depth_stencil_state_mtl;
	// Likewise the rasterizer state; meaningful only while
	// _encoder_rasterizer_set is true, which a new encoder clears.
	bool _encoder_rasterizer_set;
	MTLCullMode _encoder_cull_mode;
	MTLWinding _encoder_winding;
	MTLTriangleFillMode _encoder_fill_mode;
	float _encoder_depth_bias[3];

	// Persistent viewport/scissor state - see SPUDGPU_METAL_MAX_VIEWPORTS.
	SPUDGPU_VIEWPORT _viewports[SPUDGPU_METAL_MAX_VIEWPORTS];
	uint32_t _viewport_count; // High-water mark of slots ever set, not a "count set this call".
	SPUDGPU_SCISSOR_RECT _scissor_rects[SPUDGPU_METAL_MAX_VIEWPORTS];
	uint32_t _scissor_rect_count;

	// Borrowed (not owned) - the currently bound pipeline/index buffer view,
	// needed because Metal's draw calls take primitive type and index
	// buffer/type as direct arguments rather than consuming previously-set
	// encoder state the way Vulkan/D3D12 do. spudgpu_shader_pipeline_metal
	// isn't defined yet at this point in the file (it's declared below) -
	// the struct tag is forward-referenced here, which is fine for a
	// pointer to an as-yet-incomplete type.
	struct spudgpu_shader_pipeline_t *_bound_pipeline;
	spudgpu_buffer_view_metal *_bound_index_buffer_view;

	// Compute counterpart to _active_render_encoder above. Metal disallows
	// two live encoders of different kinds on one command buffer at once, so
	// spudgpu_cmd_bind_compute_pipeline/spudgpu_cmd_dispatch (spudgpumetalcommand.m)
	// end _active_render_encoder before opening this, and spudgpu_cmd_begin_rendering
	// (spudgpumetalrenderpass.m) ends this before opening a render encoder -
	// see spudgpumetal___internal_end_active_compute_encoder. Set by the
	// first of those two calls per compute pass, cleared by
	// spudgpumetal___internal_end_active_compute_encoder (called from
	// spudgpu_cmd_begin_rendering and spudgpu_end_command_list).
	id<MTLComputeCommandEncoder> _active_compute_encoder;
	struct spudgpu_compute_pipeline_t *_bound_compute_pipeline;

	// Blit counterpart to _active_render_encoder/_active_compute_encoder
	// above, backing spudgpu_cmd_copy_buffer/_copy_buffer_to_image/
	// _copy_image_to_buffer (spudgpumetalrenderpass.m) via an
	// MTLBlitCommandEncoder. Same "Metal disallows two live encoders of
	// different kinds on one command buffer at once" rule applies - opening
	// this ends any active render/compute encoder first, and
	// spudgpu_cmd_begin_rendering/spudgpumetal___internal_ensure_compute_encoder
	// end this before opening theirs. Cleared by
	// spudgpumetal___internal_end_active_blit_encoder (called from
	// spudgpu_end_command_list and from those two encoder-open paths).
	id<MTLBlitCommandEncoder> _active_blit_encoder;

	// Push-constant scratch block - see SPUDGPU_METAL_PUSH_CONSTANTS_SIZE.
	uint8_t _push_constants_scratch[SPUDGPU_METAL_PUSH_CONSTANTS_SIZE];
	uint32_t _push_constants_extent; // High-water mark of bytes ever written.

	// The same for spudgpu_cmd_push_constants_compute, kept apart so a
	// compute push never rewrites what the render encoder was last sent.
	uint8_t _compute_push_constants_scratch[SPUDGPU_METAL_PUSH_CONSTANTS_SIZE];
	uint32_t _compute_push_constants_extent;
} spudgpu_command_list_metal;

typedef struct spudgpu_timestamp_set_t {
#if _DEBUG
	const char *_debug_name;
#endif
	// Of the device's timestamp counter set, in shared storage so the CPU
	// can resolve it. A pass samples it at its stage boundaries
	// (spudgpumetalrenderpass.m).
	id<MTLCounterSampleBuffer> _sample_buffer_mtl;
	id<MTLDevice> _device_mtl;
	uint32_t _count;
	// sampleTimestamps:gpuTimestamp: at creation: the CPU and GPU clocks at
	// one moment, which spudgpu_get_timestamps sets against a second pair.
	MTLTimestamp _cpu_origin;
	MTLTimestamp _gpu_origin;
} spudgpu_timestamp_set_metal;

// Defined in spudgpumetalimage.m - shared so shader pipeline attachment
// formats agree exactly with what spudgpu_create_image produces for the
// same SPUDGPU_FORMAT.
MTLPixelFormat spudgpumetal___internal_image_pixel_format(SPUDGPU_FORMAT format);

// Defined in spudgpumetalcommand.m - ends and releases cmd_list_metal's
// _active_compute_encoder if one is active, else a no-op. Shared with
// spudgpu_cmd_begin_rendering (spudgpumetalrenderpass.m), which must end any
// active compute encoder before opening a render encoder (Metal disallows
// two live encoders of different kinds on one command buffer at once) - see
// _active_compute_encoder's comment above.
void spudgpumetal___internal_end_active_compute_encoder(spudgpu_command_list_metal *cmd_list_metal);

// Defined in spudgpumetalrenderpass.m - ends and releases cmd_list_metal's
// _active_blit_encoder if one is active, else a no-op. Shared with
// spudgpu_cmd_begin_rendering (spudgpumetalrenderpass.m) and
// spudgpumetal___internal_ensure_compute_encoder (spudgpumetalcommand.m),
// both of which must end any active blit encoder before opening their own -
// see _active_blit_encoder's comment above.
void spudgpumetal___internal_end_active_blit_encoder(spudgpu_command_list_metal *cmd_list_metal);

// The device's timestamp counter set (MTLCommonCounterSetTimestamp),
// retained, which the caller releases; or nil if it has none. Defined in
// spudgpumetalquery.m.
id<MTLCounterSet> spudgpumetal___internal_timestamp_counter_set(id<MTLDevice> device);

typedef struct spudgpu_shader_module_t {
#if _DEBUG
	const char *_debug_name;
#endif
	spudgpu_device_metal *_parent_device;
	spudgpu_shader_module_desc _desc;
	// SPIRV-Cross-generated MSL source is compiled into this once at module
	// creation. _function_mtl is looked up eagerly too, always under the
	// name "main0" - SPIRV-Cross's MSL backend always renames a SPIR-V
	// module's entry point to this when the module has exactly one (the
	// expected shape for this project's per-stage-compiled shaders), since
	// MSL reserves the literal name "main". This means
	// spudgpu_shader_pipeline_desc's *_entry_point strings are accepted for
	// cross-backend API symmetry but not consulted on this backend - see
	// spudgpu_create_shader_module.
	id<MTLLibrary> _library_mtl;
	id<MTLFunction> _function_mtl;
	// SPUDGPU_SHADER_STAGE_COMPUTE and _MESH only: the GLSL shader's declared
	// local_size_x/y/z (SPIR-V's LocalSize execution mode), reflected out of
	// the SPIR-V at cross-compile time - see
	// spudgpumetal___internal_cross_compile_spirv_to_msl in
	// spudgpumetalshader.m. Unlike Vulkan/D3D12, where the driver reads this
	// straight out of the bound pipeline at dispatch time, Metal's
	// -dispatchThreadgroups:threadsPerThreadgroup:/-drawMeshThreadgroups:
	// threadsPerObjectThreadgroup:threadsPerMeshThreadgroup: require the caller to
	// supply it explicitly every call - spudgpu_create_compute_pipeline
	// copies these onto spudgpu_compute_pipeline_metal below so
	// spudgpu_cmd_dispatch (spudgpumetalcommand.m) never needs the module
	// directly. Zero (never set) for vertex/fragment modules.
	uint32_t _local_size_x, _local_size_y, _local_size_z;
} spudgpu_shader_module_metal;

typedef struct spudgpu_shader_pipeline_t {
#if _DEBUG
	const char *_debug_name;
#endif
	id<MTLRenderPipelineState> _render_pipeline_state_mtl;
	// Vulkan/D3D12 bake depth/stencil test config into the same monolithic
	// pipeline object as everything else; Metal keeps it as a wholly
	// separate native object, bound alongside (not instead of) the render
	// pipeline state - see spudgpu_cmd_bind_pipeline in spudgpumetalcommand.m.
	id<MTLDepthStencilState> _depth_stencil_state_mtl;
	spudgpu_device_metal *_parent_device;
	spudgpu_shader_pipeline_desc _desc;
	// True when this pipeline was built from a MTLMeshRenderPipelineDescriptor
	// (desc.mesh_module set) rather than a classic MTLRenderPipelineDescriptor
	// - _render_pipeline_state_mtl/_depth_stencil_state_mtl are reused as-is
	// either way (newRenderPipelineStateWithMeshDescriptor:... returns the
	// same id<MTLRenderPipelineState> type), but binding buffers/bytes to a
	// mesh pipeline's render encoder goes through -setMeshBuffer:/
	// -setMeshBytes: instead of -setVertexBuffer:/-setVertexBytes: (see
	// spudgpu_cmd_bind_descriptor_sets/spudgpu_cmd_push_constants), so
	// callers need to know which shape is bound.
	bool _is_mesh_pipeline;
	// Copied from mesh_module at pipeline-creation time when _is_mesh_pipeline
	// - see spudgpu_shader_module_metal::_local_size_x/y/z above.
	uint32_t _mesh_local_size_x, _mesh_local_size_y, _mesh_local_size_z;
} spudgpu_shader_pipeline_metal;

typedef struct spudgpu_compute_pipeline_t {
#if _DEBUG
	const char *_debug_name;
#endif
	id<MTLComputePipelineState> _compute_pipeline_state_mtl;
	spudgpu_device_metal *_parent_device;
	spudgpu_compute_pipeline_desc _desc;
	// Copied from the compute module at creation time - see
	// spudgpu_shader_module_metal::_local_size_x/y/z above.
	uint32_t _local_size_x, _local_size_y, _local_size_z;
} spudgpu_compute_pipeline_metal;

typedef struct spudgpu_fence_t {
#if _DEBUG
	const char *_debug_name;
#endif
	// Fences map to id<MTLSharedEvent>, not MTLFence - see
	// spudgpu_metal_natives.h for why (MTLFence can't be CPU-waited).
	id<MTLSharedEvent> _shared_event_mtl;
	spudgpu_device_metal *_parent_device;
	SPUDGPU_FENCE_FLAGS _flags;
	// Used only by a swap chain's own in-flight fence, which counts its
	// submissions itself (spudgpu_queue_submit with a swap chain). A fence
	// the caller created has no counter here: the value a submission signals
	// is the caller's (spudgpu_submit_desc::signal_fence_value), and the
	// value reached is the shared event's signaledValue.
	uint64_t _signal_value;
} spudgpu_fence_metal;

typedef struct spudgpu_semaphore_t {
#if _DEBUG
	const char *_debug_name;
#endif
	// Semaphores map to plain id<MTLEvent>, not id<MTLSharedEvent> - unlike
	// spudgpu_fence, spudgpu_semaphore's public API never exposes a CPU-side
	// wait/signal/read, only GPU wait/signal across queue submissions, which
	// is all MTLEvent (MTLSharedEvent's CPU-visibility-free base class) does.
	id<MTLEvent> _event_mtl;
	spudgpu_device_metal *_parent_device;
	uint64_t _signal_value;
} spudgpu_semaphore_metal;

typedef struct spudgpu_sampler_t {
#if _DEBUG
	const char *_debug_name;
#endif
	id<MTLSamplerState> _sampler_state_mtl;
} spudgpu_sampler_metal;

typedef struct spudgpu_descriptor_set_layout_t {
#if _DEBUG
	const char *_debug_name;
#endif
	spudgpu_device_metal *_parent_device;
	spudgpu_descriptor_set_layout_desc _desc;
	// Metal has no VkDescriptorSetLayout-equivalent schema object independent
	// of a concrete backing buffer - the schema lives entirely in this
	// MTLArgumentEncoder, built once here from the binding list and reused
	// (redirected via -setArgumentBuffer:offset:) to encode every descriptor
	// set allocated from this layout, per Apple's documented pattern for
	// writing into many argument buffer instances from one shared encoder.
	id<MTLArgumentEncoder> _argument_encoder_mtl;
} spudgpu_descriptor_set_layout_metal;

typedef struct spudgpu_descriptor_pool_t {
#if _DEBUG
	const char *_debug_name;
#endif
	spudgpu_device_metal *_parent_device;
	spudgpu_descriptor_pool_desc _desc;
	// Metal needs no real backing arena the way Vulkan's VkDescriptorPool
	// does - each descriptor set below owns its own small MTLBuffer sized to
	// its layout's encodedLength. This only enforces the caller's declared
	// _desc.max_sets cap (see spudgpu_create_descriptor_sets), matching
	// vkAllocateDescriptorSets' out-of-pool-memory failure without an actual
	// pooled allocator behind it.
	uint32_t _allocated_set_count;
	// Owned array of every live set allocated from this pool (length
	// _desc.max_sets) - purely so spudgpu_reset_descriptor_pool/
	// spudgpu_destroy_descriptor_pool can free each set's MTLBuffer in bulk,
	// matching those calls' "free all sets allocated from this pool"
	// contract instead of leaking GPU memory every reset.
	struct spudgpu_descriptor_set_t **_tracked_sets_mtl;
} spudgpu_descriptor_pool_metal;

// Upper bound on distinct underlying resources (buffers/textures) tracked
// per descriptor set for residency (see spudgpu_cmd_bind_descriptor_sets in
// spudgpumetalcommand.m) - anything reached only indirectly through an
// argument buffer must be marked resident with -useResource:usage: on every
// encoder that uses it, unlike a directly-bound resource. Sized well above
// SPUDGPU_MAX_DESCRIPTOR_BINDINGS_PER_SET to leave room for small resource
// arrays within a single binding.
#define SPUDGPU_METAL_MAX_TRACKED_RESOURCES_PER_SET 32

typedef struct spudgpu_descriptor_set_t {
#if _DEBUG
	const char *_debug_name;
#endif
	spudgpu_descriptor_pool_metal *_parent_pool;
	spudgpu_descriptor_set_layout_metal *_layout;
	// The argument buffer itself, written into by spudgpu_update_descriptor_sets
	// via _layout's shared MTLArgumentEncoder.
	id<MTLBuffer> _argument_buffer_mtl;
	// See SPUDGPU_METAL_MAX_TRACKED_RESOURCES_PER_SET above. One entry per
	// argument index that has been written: writing an index again replaces
	// its entry, so a set never marks resident what it no longer references.
	// Not retained - as with the argument buffer's own reference, a resource
	// must outlive every bind of a set that still references it.
	uint32_t _tracked_argument_indices[SPUDGPU_METAL_MAX_TRACKED_RESOURCES_PER_SET];
	id<MTLResource> _tracked_resources_mtl[SPUDGPU_METAL_MAX_TRACKED_RESOURCES_PER_SET];
	MTLResourceUsage _tracked_resource_usages_mtl[SPUDGPU_METAL_MAX_TRACKED_RESOURCES_PER_SET];
	uint32_t _tracked_resource_count;
} spudgpu_descriptor_set_metal;

typedef struct spudgpu_surface_t {
#if _DEBUG
	const char *_debug_name;
#endif
	spudgpu_instance_metal *_parent_instance;
	void *_window_handle;
	void *_display_handle;
	// Attached to _window_handle's view by spudgpu_create_surface. NULL until
	// that's implemented - spudgpu_create_swap_chain checks for this and
	// fails rather than assuming it exists.
	CAMetalLayer *_metal_layer;
} spudgpu_surface_metal;

typedef struct spudgpu_swap_chain_t {
#if _DEBUG
	const char *_debug_name;
#endif
	spudgpu_device_metal *_parent_device;
	spudgpu_swap_chain_desc _desc;
	// Borrowed from _desc.surface->_metal_layer - not owned here.
	CAMetalLayer *_metal_layer;
	// From _desc.queue - see spudgpu_swap_chain_desc::queue. Borrowed, not
	// owned (the device's cached queue families outlive this swap chain).
	spudgpu_command_queue_metal *_present_queue;
	// The drawable currently held between acquire and present. Metal command
	// buffers/drawables are single-use, unlike Vulkan/D3D12's stable N-image
	// array - there is exactly one of these at a time, not one per
	// _desc.buffer_count slot (buffer_count is forced to 1 on this backend;
	// see spudgpu_create_swap_chain).
	id<CAMetalDrawable> _current_drawable;
	// Single-element arrays (buffer_count == 1 on Metal) so
	// spudgpu_get_swap_chain_image_view's existing index-0 lookup keeps
	// working unchanged. Repointed at _current_drawable.texture on every
	// acquire.
	spudgpu_image_metal *_back_buffer_images;
	spudgpu_image_view_metal *_back_buffer_image_views;
	// Signaled once at creation and never touched again - see
	// spudgpu_create_swap_chain. Kept only so
	// spudgpu_swap_chain_get_image_available_semaphore returns something
	// valid for callers following the Vulkan-shaped pattern; Metal's own
	// acquire/present path never waits on it, since nextDrawable already
	// blocks until the image is genuinely available before acquire returns.
	spudgpu_semaphore_metal *_image_available_semaphore;
	// Genuinely signaled per-frame by spudgpu_queue_submit/
	// spudgpu_submit_command_lists_synced when a caller includes it, but
	// Metal's own spudgpu_swap_chain_present doesn't consume it either -
	// present ordering comes from committing to _present_queue after the
	// render work, the same guarantee spudgpu_queue_wait_idle relies on
	// (mirrors the D3D12 backend, which doesn't consume it in present() at
	// all). Kept for API symmetry with Vulkan, which does consume it.
	spudgpu_semaphore_metal *_render_finished_semaphore;
	spudgpu_fence_metal *_in_flight_fence;
} spudgpu_swap_chain_metal;

#endif // SPUDLIB_SPUDGPUMETAL_H

#endif // SPUDGPU_COMPILE_METAL_API
