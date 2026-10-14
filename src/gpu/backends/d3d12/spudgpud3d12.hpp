
#if SPUDGPU_COMPILE_D3D12_API

#include "d3dx12.h"
#include "spudgpu.h"
#include "spudgpu_d3d12_natives.h"
#include <array>
#include <atomic>
#include <cstdlib>
#include <dxgi1_6.h>
#include <new>
#include <vector>
#include <wrl/client.h>

#define D3D12_GPU_VIRTUAL_ADDRESS_NULL ((D3D12_GPU_VIRTUAL_ADDRESS)0)
#define D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN ((D3D12_GPU_VIRTUAL_ADDRESS) - 1)
#define SPUD_D3D12_COMMAND_QUEUE_COUNT_PER_FAMILY 4


inline D3D12_COMMAND_LIST_TYPE
spudgpu_d3d12_get_command_list_type(SPUDGPU_COMMAND_LIST_TYPE t) {
	switch (t) {
	case SPUDGPU_COMMAND_LIST_TYPE_DIRECT:
		return D3D12_COMMAND_LIST_TYPE_DIRECT;
	case SPUDGPU_COMMAND_LIST_TYPE_COPY:
		return D3D12_COMMAND_LIST_TYPE_COPY;
	case SPUDGPU_COMMAND_LIST_TYPE_COMPUTE:
		return D3D12_COMMAND_LIST_TYPE_COMPUTE;
	case SPUDGPU_COMMAND_LIST_TYPE_BUNDLE:
		return D3D12_COMMAND_LIST_TYPE_BUNDLE;
	default:
		return D3D12_COMMAND_LIST_TYPE_NONE;
	}
}

// The heap flags of every buffer and image. spudgpu.h declares a new
// resource's contents undefined until written, so D3D12 is never asked to
// zero one.
inline D3D12_HEAP_FLAGS spudgpu_d3d12_get_heap_flags() {
	return D3D12_HEAP_FLAG_CREATE_NOT_ZEROED;
}
inline D3D12_RESOURCE_FLAGS spudgpu_d3d12_get_buffer_resource_flags(
    SPUDGPU_BUFFER_USAGE usage) {
	D3D12_RESOURCE_FLAGS result = D3D12_RESOURCE_FLAG_NONE;
	if (usage & SPUDGPU_BUFFER_USAGE_STORAGE)
		result |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	if (usage & SPUDGPU_BUFFER_USAGE_RAYTRACING_ACCELERATION_STRUCTURE)
		result |= D3D12_RESOURCE_FLAG_RAYTRACING_ACCELERATION_STRUCTURE;
	return result;
}
inline D3D12_RESOURCE_FLAGS spudgpu_d3d12_get_image_resource_flags(
    SPUDGPU_IMAGE_USAGE usage) {
	D3D12_RESOURCE_FLAGS result = D3D12_RESOURCE_FLAG_NONE;
	if (usage & SPUDGPU_IMAGE_USAGE_COLOR_ATTACHMENT)
		result |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	if (usage & SPUDGPU_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT)
		result |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	if (usage & SPUDGPU_IMAGE_USAGE_STORAGE)
		result |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	return result;
}
inline DXGI_FORMAT spudgpu_d3d12_get_dxgi_format(SPUDGPU_FORMAT fmt) {
	return static_cast<DXGI_FORMAT>(fmt);
}

// The heap a buffer of [kind] is created in: what each SPUDGPU_MEMORY_KIND
// documents in spudgpu.h. spudgpu_create_buffer has already refused a value
// that is not a kind.
inline D3D12_HEAP_PROPERTIES
spudgpu_d3d12_get_heap_properties_from_memory_kind(SPUDGPU_MEMORY_KIND kind) {
	D3D12_HEAP_PROPERTIES props = {};
	switch (kind) {
	case SPUDGPU_MEMORY_KIND_UPLOAD:
		props.Type = D3D12_HEAP_TYPE_UPLOAD;
		break;
	case SPUDGPU_MEMORY_KIND_READBACK:
		props.Type = D3D12_HEAP_TYPE_READBACK;
		break;
	case SPUDGPU_MEMORY_KIND_DEVICE_MAPPABLE:
		// A custom heap is held to neither the upload heap's rule (always
		// GENERIC_READ, no unordered access) nor the readback heap's (always
		// COPY_DEST). L0 is the only pool a UMA adapter has, and
		// spudgpu_create_buffer only gets here on one.
		props.Type                 = D3D12_HEAP_TYPE_CUSTOM;
		props.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
		props.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
		break;
	default:
		props.Type = D3D12_HEAP_TYPE_DEFAULT;
		break;
	}
	return props;
}
// An upload heap's resources are GENERIC_READ and a readback heap's are
// COPY_DEST for as long as they exist; D3D12 allows no other state for them.
inline D3D12_RESOURCE_STATES
spudgpu_d3d12_get_initial_buffer_state(SPUDGPU_MEMORY_KIND kind) {
	if (kind == SPUDGPU_MEMORY_KIND_UPLOAD)
		return D3D12_RESOURCE_STATE_GENERIC_READ;
	if (kind == SPUDGPU_MEMORY_KIND_READBACK)
		return D3D12_RESOURCE_STATE_COPY_DEST;
	return D3D12_RESOURCE_STATE_COMMON;
}
inline D3D12_RESOURCE_STATES
spudgpu_d3d12_get_initial_image_state(SPUDGPU_IMAGE_USAGE usage) {
	if (usage & SPUDGPU_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT)
		return D3D12_RESOURCE_STATE_DEPTH_WRITE;
	if (usage & SPUDGPU_IMAGE_USAGE_COLOR_ATTACHMENT)
		return D3D12_RESOURCE_STATE_RENDER_TARGET;
	if (usage & SPUDGPU_IMAGE_USAGE_TRANSFER_DST)
		return D3D12_RESOURCE_STATE_COPY_DEST;
	if (usage & SPUDGPU_IMAGE_USAGE_STORAGE)
		return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	return D3D12_RESOURCE_STATE_COMMON;
}

typedef struct spudgpu_instance_t {
#if _DEBUG
	const char *_debug_name;
#endif
	const char *application_name;
	uint32_t application_version;
	const char *engine_name;
	uint32_t engine_version;
	Microsoft::WRL::ComPtr<IDXGIFactory7> _dxgi_factory;
	spudgpu_device *_gpu_devices;
	uint32_t _gpu_device_count;
	bool _gpu_devices_enumerated;
} spudgpu_instance_d3d12;

#define SPUDGPU_BINDLESS_MAX_SLOTS_PER_CLASS 4096

// Forward declaration -- spudgpu_bindless_state_d3d12 below stores a pointer
// to this before its full definition (with the rest of the descriptor-set
// types) appears further down this file.
typedef struct spudgpu_descriptor_set_layout_t spudgpu_descriptor_set_layout_d3d12;

// Lazily created the first time bindless capabilities/registration is used on
// a device. Safe to lazy-init here (unlike the Vulkan backend): every D3D12
// struct above stores spudgpu_device_d3d12* by pointer, never by value, so
// there is no stale-snapshot risk from initializing this after other objects
// already exist.
typedef struct spudgpu_bindless_state_d3d12 {
	// One shader-visible CBV_SRV_UAV heap, 3 fixed-size ranges back-to-back:
	// [0, MAX) sampled images, [MAX, 2*MAX) storage images,
	// [2*MAX, 3*MAX) storage buffers. spudgpu_d3d12_build_root_signature
	// (spudgpud3d12shader.cpp) already builds exactly this shape from
	// layout->_desc.bindings[] with no changes needed — see `layout` below.
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap;
	uint32_t increment = 0;

	// Sentinel spudgpu_descriptor_set_layout_d3d12 carrying only the 3
	// bindings' shape (binding_count/bindings[]) needed to generate the
	// matching root-signature ranges when a caller includes this handle in
	// their own descriptor_set_layouts[]. Its _slots[]/_cbv_srv_uav_count
	// bookkeeping (used by the classic pool/set allocator) is left zeroed —
	// bindless never goes through spudgpu_create_descriptor_sets.
	spudgpu_descriptor_set_layout_d3d12 *layout = nullptr;

	uint32_t sampled_image_next_unused  = 0;
	uint32_t storage_image_next_unused  = 0;
	uint32_t storage_buffer_next_unused = 0;

	// Stacks of freed indices, reused before drawing from *_next_unused.
	std::vector<uint32_t> sampled_image_free_stack;
	std::vector<uint32_t> storage_image_free_stack;
	std::vector<uint32_t> storage_buffer_free_stack;
} spudgpu_bindless_state_d3d12;

typedef struct spudgpu_device_t {
#if _DEBUG
	const char *_debug_name;
#endif
	spudgpu_instance_d3d12 *_instance;
	Microsoft::WRL::ComPtr<IDXGIAdapter4> _dxgi_adapter;
	Microsoft::WRL::ComPtr<ID3D12Device14> _d3d_device;
	SPUDGPU_DEVICE_PROPERTIES _properties;
	std::array<spudgpu_command_queue, SPUD_D3D12_COMMAND_QUEUE_COUNT_PER_FAMILY>
	    _cmd_queues_direct;
	std::array<spudgpu_command_queue, SPUD_D3D12_COMMAND_QUEUE_COUNT_PER_FAMILY>
	    _cmd_queues_copy;
	std::array<spudgpu_command_queue, SPUD_D3D12_COMMAND_QUEUE_COUNT_PER_FAMILY>
	    _cmd_queues_compute;

	// Lazily allocated by the first spudgpu_get_bindless_capabilities /
	// spudgpu_bindless_register_* / spudgpu_get_bindless_descriptor_set_layout
	// call. nullptr until then; this device struct is value-initialised
	// when it is constructed in place (see spudgpud3d12context.cpp), so it
	// starts nullptr for free. Released with the device.
	spudgpu_bindless_state_d3d12 *_bindless;

	// Lazily created by the first spudgpu_cmd_draw_indirect /
	// _indexed_indirect call on this device. Both are root-signature-less
	// (nullptr) command signatures describing only a single DRAW/DRAW_INDEXED
	// argument — the layout is fixed by spudgpu_draw_indirect_args /
	// spudgpu_draw_indexed_indirect_args, never per-caller, so one signature
	// per device covers every indirect draw ever issued against it. Both
	// ComPtrs start empty (nullptr) from the device's constructor.
	Microsoft::WRL::ComPtr<ID3D12CommandSignature> _draw_indirect_command_signature;
	Microsoft::WRL::ComPtr<ID3D12CommandSignature> _draw_indexed_indirect_command_signature;
} spudgpu_device_d3d12;

typedef struct spudgpu_command_queue_t {
#if _DEBUG
	const char *_debug_name;
#endif
	spudgpu_device_d3d12 *_device;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> _d3d_cmd_queue;
} spudgpu_command_queue_d3d12;

typedef struct spudgpu_command_allocator_t {
#if _DEBUG
	const char *_debug_name;
#endif
	spudgpu_device_d3d12 *_device;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> _d3d_cmd_allocator;
	D3D12_COMMAND_LIST_TYPE _d3d_cmd_list_type;
} spudgpu_command_allocator_d3d12;

typedef struct spudgpu_command_list_t {
#if _DEBUG
	const char *_debug_name;
#endif
	spudgpu_command_allocator_d3d12 *_allocator;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList10> _d3d_cmd_list;

	// Persistent RTV/DSV heaps reused across spudgpu_cmd_begin_rendering
	// calls instead of allocating a new descriptor heap every render pass.
	// _rtv_heap grows (and is recreated) if a pass needs more color
	// attachments than it currently holds; _dsv_heap only ever needs one
	// descriptor and is created once on first use.
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _rtv_heap;
	UINT _rtv_heap_capacity;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _dsv_heap;

	// Set by spudgpu_cmd_begin_rendering, read by spudgpu_cmd_end_rendering:
	// true if BeginRenderPass/EndRenderPass was used for this pass, false if
	// the classic OMSetRenderTargets/Clear*View path was used instead (see
	// spudgpu_cmd_begin_rendering's will_execute_bundles handling) -- the two
	// don't mix, so end_rendering must match whichever begin_rendering chose.
	bool _using_render_pass;
} spudgpu_command_list_d3d12;

typedef struct spudgpu_buffer_t {
#if _DEBUG
	const char *_debug_name;
#endif
	spudgpu_device_d3d12 *_device;
	spudgpu_buffer_desc _desc;
	Microsoft::WRL::ComPtr<ID3D12Resource2> _d3d_resource;
	D3D12_RESOURCE_DESC _d3d_resource_desc;
	D3D12_GPU_VIRTUAL_ADDRESS _d3d_gpu_address;
	// nullptr for a committed buffer; the heap it was placed in otherwise
	// (spudgpu_create_placed_buffer).
	spudgpu_heap _heap;
} spudgpu_buffer_d3d12;

typedef struct spudgpu_buffer_view_t {
#if _DEBUG
	const char *_debug_name;
#endif
	spudgpu_buffer_d3d12 *_buffer;
	spudgpu_buffer_view_desc _desc;
	SPUDGPU_D3D12_BUFFER_VIEW _d3d_view;
} spudgpu_buffer_view_d3d12;

typedef struct spudgpu_image_t {
#if _DEBUG
	const char *_debug_name;
#endif
	spudgpu_device_d3d12 *_device;
	spudgpu_image_desc _desc;
	Microsoft::WRL::ComPtr<ID3D12Resource2> _d3d_resource;
	D3D12_RESOURCE_DESC _d3d_resource_desc;
	D3D12_GPU_VIRTUAL_ADDRESS _d3d_gpu_address;

	// The resource's actual current D3D12 state, kept in sync by every
	// spudgpu_cmd_image_barrier* call (spudgpud3d12renderpass.cpp). Needed
	// because SPUDGPU_IMAGE_LAYOUT_UNDEFINED (the Vulkan "don't care what it
	// was" convention this API follows) has no real D3D12 equivalent --
	// D3D12's strict before/after state tracking means the barrier's
	// declared "before" state has to match whatever the resource is
	// actually in, which for a freshly created image is its usage-implied
	// initial state (not always D3D12_RESOURCE_STATE_COMMON -- see
	// spudgpu_d3d12_get_initial_image_state) and for a recycled resource
	// (a swap chain back buffer transitioning every frame) is whatever the
	// last real transition left it at, not the creation-time value. Set at
	// creation (spudgpu_create_image / swap chain back buffer wrapping) and
	// updated after every barrier -- never re-derived from a static
	// usage-based guess.
	D3D12_RESOURCE_STATES _current_state;
	// Empty while every subresource is in _current_state, which is nearly
	// always. Once spudgpu_cmd_image_barrier_subresource has moved part of
	// the image somewhere else, it holds the state of each (mip, layer),
	// at index mip + layer * mip_levels, and _current_state is not read
	// until the states agree again and this is emptied. A single state for
	// the whole image can't describe mip N in one layout and mip N + 1 in
	// another, which is what that call exists for.
	std::vector<D3D12_RESOURCE_STATES> _subresource_states;
	// nullptr for a committed image; the heap it was placed in otherwise
	// (spudgpu_create_placed_image).
	spudgpu_heap _heap;
} spudgpu_image_d3d12;

typedef struct spudgpu_heap_t {
#if _DEBUG
	const char *_debug_name;
	// Buffers and images placed in the heap and not yet destroyed.
	// spudgpu_destroy_heap asserts it is 0. Atomic, because resources in
	// one heap may be placed and destroyed on several threads at once.
	std::atomic<uint64_t> _placed_count;
#endif
	spudgpu_device_d3d12 *_device;
	spudgpu_heap_desc _desc;
	Microsoft::WRL::ComPtr<ID3D12Heap> _d3d_heap;
} spudgpu_heap_d3d12;

// Shared by spudgpu_create_buffer / spudgpu_create_image and the heap calls
// (spudgpud3d12heap.cpp), so a buffer or image is checked and described the
// same way whether it is committed or placed.

// spudgpu_create_buffer's desc checks, in its order, after its argument
// checks: size, usage, memory kind, usage against kind, device.
SPUDRESULT spudgpu_d3d12_check_buffer_desc(
    const SPUDGPU_DEVICE_PROPERTIES *properties,
    const spudgpu_buffer_desc *desc);
CD3DX12_RESOURCE_DESC spudgpu_d3d12_create_resource_desc_from_buffer(
    const spudgpu_buffer_desc *desc);
// spudgpu_create_image's desc checks, in its order, after its argument
// checks.
SPUDRESULT spudgpu_d3d12_check_image_desc(const spudgpu_image_desc *desc);
SPUDRESULT spudgpu_d3d12_create_resource_desc_from_image(
    const spudgpu_image_desc *desc,
    D3D12_RESOURCE_DESC *out_d3d_resource_desc);
// The optimized clear value an image of [desc] is created with, or false if
// it has none (it is not an attachment).
bool spudgpu_d3d12_get_image_clear_value(
    const spudgpu_image_desc *desc,
    D3D12_CLEAR_VALUE *out_clear_value);

typedef struct spudgpu_image_view_t {
#if _DEBUG
	const char *_debug_name;
#endif
	spudgpu_image_d3d12 *_image;
	spudgpu_image_view_desc _desc;
	// What a render pass reads (_rtv, _dsv), and what
	// spudgpu_get_d3d12_image_view hands out: one desc, the first of depth
	// stencil, render target, unordered access and shader resource that the
	// image's usage allows.
	SPUDGPU_D3D12_IMAGE_VIEW _d3d_view_desc;
	// What a descriptor write reads, whatever else the image is used for.
	D3D12_SHADER_RESOURCE_VIEW_DESC _d3d_srv;
	D3D12_UNORDERED_ACCESS_VIEW_DESC _d3d_uav;
} spudgpu_image_view_d3d12;

// A root parameter index that no table or constant block was given.
#define SPUDGPU_D3D12_NO_ROOT_PARAMETER UINT32_MAX

// Where spudgpu_d3d12_build_root_signature (spudgpud3d12shader.cpp) put each
// descriptor set's tables and the push constants among a root signature's
// parameters. A D3D12 descriptor table reads from one heap, so a set with
// both CBV/SRV/UAV and sampler descriptors takes two tables, one per heap;
// a set's index is therefore not its root parameter index, and every call
// that sets a root parameter looks it up here.
typedef struct spudgpu_d3d12_root_layout {
	// Indexed by set number; SPUDGPU_D3D12_NO_ROOT_PARAMETER where the set
	// has no descriptor of that heap.
	uint32_t cbv_srv_uav_param[SPUDGPU_MAX_DESCRIPTOR_SET_LAYOUTS];
	uint32_t sampler_param[SPUDGPU_MAX_DESCRIPTOR_SET_LAYOUTS];
	// SPUDGPU_D3D12_NO_ROOT_PARAMETER with no push constant range.
	uint32_t push_constant_param;
} spudgpu_d3d12_root_layout;

typedef struct spudgpu_shader_module_t {
#if _DEBUG
	const char *_debug_name;
#endif
	spudgpu_device_d3d12 *_device;
	spudgpu_shader_module_desc _desc;
	Microsoft::WRL::ComPtr<ID3DBlob> _d3d_blob;
} spudgpu_shader_module_d3d12;

typedef struct spudgpu_shader_pipeline_t {
#if _DEBUG
	const char *_debug_name;
#endif
	spudgpu_device_d3d12 *_device;
	spudgpu_shader_pipeline_desc _desc;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> _d3d_pipeline_state;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> _d3d_root_signature;
	spudgpu_d3d12_root_layout _root_layout;
	D3D_PRIMITIVE_TOPOLOGY _d3d_primitive_topology;
	// True when built from desc->mesh_module (SPUDGPU_EXT_MESH_SHADING) via
	// the D3D12_PIPELINE_STATE_STREAM_DESC path instead of the classic
	// CreateGraphicsPipelineState path - see spudgpud3d12shader.cpp. Mesh
	// pipelines have no input-assembler stage, so _d3d_primitive_topology
	// is meaningless for them and IASetPrimitiveTopology must be skipped.
	bool _is_mesh_pipeline;
} spudgpu_shader_pipeline_d3d12;

typedef struct spudgpu_compute_pipeline_t {
#if _DEBUG
	const char *_debug_name;
#endif
	spudgpu_device_d3d12 *_device;
	spudgpu_compute_pipeline_desc _desc;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> _d3d_pipeline_state;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> _d3d_root_signature;
	spudgpu_d3d12_root_layout _root_layout;
} spudgpu_compute_pipeline_d3d12;

typedef struct spudgpu_fence_t {
#if _DEBUG
	const char *_debug_name;
#endif
	spudgpu_device_d3d12 *_device;
	SPUDGPU_FENCE_FLAGS flags;
	// The fence's value is the ID3D12Fence's own. Nothing is counted here:
	// the value a submission signals is the caller's
	// (spudgpu_submit_desc::signal_fence_value).
	Microsoft::WRL::ComPtr<ID3D12Fence1> _d3d_fence;
} spudgpu_fence_d3d12;

typedef struct spudgpu_semaphore_t {
#if _DEBUG
	const char *_debug_name;
#endif
	spudgpu_device_d3d12 *_device;
	uint64_t _signal_value;
	Microsoft::WRL::ComPtr<ID3D12Fence1> _d3d_fence;
} spudgpu_semaphore_d3d12;

// Per-binding heap offsets pre-computed at layout creation time.
typedef struct spudgpu_d3d12_binding_slot {
	uint32_t cbv_srv_uav_offset; // from set base in the CBV/SRV/UAV heap
	uint32_t sampler_offset;     // from set base in the sampler heap
	bool     is_sampler;         // true if this binding lives in the sampler heap
} spudgpu_d3d12_binding_slot;

// D3D12 samplers are descriptor-heap entries, not standalone device objects
// the way VkSampler/id<MTLSamplerState> are — this just stores the
// translated desc, written into a sampler heap slot on demand by
// spudgpu_update_descriptor_sets (spudgpud3d12descriptors.cpp).
typedef struct spudgpu_sampler_t {
#if _DEBUG
	const char *_debug_name;
#endif
	D3D12_SAMPLER_DESC _d3d_desc;
} spudgpu_sampler_d3d12;

typedef struct spudgpu_descriptor_set_layout_t {
#if _DEBUG
	const char *_debug_name;
#endif
	spudgpu_device_d3d12 *_device;
	spudgpu_descriptor_set_layout_desc _desc;
	// Offsets into a set's allocated heap range, indexed by binding desc order.
	spudgpu_d3d12_binding_slot _slots[SPUDGPU_MAX_DESCRIPTOR_BINDINGS_PER_SET];
	uint32_t _cbv_srv_uav_count; // CBV/SRV/UAV descriptors needed per set
	uint32_t _sampler_count;     // sampler descriptors needed per set
} spudgpu_descriptor_set_layout_d3d12;

typedef struct spudgpu_descriptor_pool_t {
#if _DEBUG
	const char *_debug_name;
#endif
	spudgpu_device_d3d12 *_device;
	spudgpu_descriptor_pool_desc _desc;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _cbv_srv_uav_heap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _sampler_heap;
	uint32_t _cbv_srv_uav_capacity;
	uint32_t _sampler_capacity;
	uint32_t _cbv_srv_uav_cursor; // next free slot index
	uint32_t _sampler_cursor;
	uint32_t _cbv_srv_uav_increment;
	uint32_t _sampler_increment;
	// Head of the list of every set allocated from this pool, linked through
	// spudgpu_descriptor_set_d3d12::_next. There is no per-set destroy call,
	// so the pool is what releases them, on reset and on destroy.
	struct spudgpu_descriptor_set_t *_sets;
} spudgpu_descriptor_pool_d3d12;

typedef struct spudgpu_descriptor_set_t {
#if _DEBUG
	const char *_debug_name;
#endif
	spudgpu_descriptor_pool_d3d12 *_pool;
	spudgpu_descriptor_set_layout_d3d12 *_layout;
	uint32_t _cbv_srv_uav_base; // base slot index in pool's CBV/SRV/UAV heap
	uint32_t _sampler_base;     // base slot index in pool's sampler heap
	struct spudgpu_descriptor_set_t *_next; // next set in _pool's _sets list
} spudgpu_descriptor_set_d3d12;

typedef struct spudgpu_surface_t {
#if _DEBUG
	const char *_debug_name;
#endif
	HWND _hwnd;
	spudgpu_instance_d3d12 *_instance;
} spudgpu_surface_d3d12;

typedef struct spudgpu_swap_chain_t {
#if _DEBUG
	const char *_debug_name;
#endif
	spudgpu_device_d3d12 *_device;
	spudgpu_swap_chain_desc _desc;
	Microsoft::WRL::ComPtr<IDXGISwapChain4> _dxgi_swap_chain;
	spudgpu_image_d3d12 *_back_buffer_images;
	spudgpu_image_view_d3d12 *_back_buffer_image_views;

	// Backs spudgpu_submit_command_lists_synced's "signals the in-flight
	// fence" half of the Vulkan/Metal contract. The other two thirds of that
	// contract have no D3D12 equivalent to wire up: flip-model DXGI swap
	// chains hand back a buffer index synchronously (no Vulkan-style
	// image_available acquire wait), and Present() is queued on the same
	// command queue the render work was submitted to, so it's already
	// ordered after that work with no separate render_finished signal
	// needed. One monotonically increasing fence value per submit, rather
	// than Vulkan/Metal's per-back-buffer-index tracking, because every
	// sample built against this backend so far (see HelloTriangle) reuses a
	// single command allocator/list across frames instead of one per
	// back-buffer index -- so the only safe wait is against the
	// immediately preceding submit, not the submit that last touched the
	// current back-buffer index specifically (which can be a stale,
	// already-satisfied value that under-waits relative to the shared
	// allocator's real last use). See spudgpu_swap_chain_acquire_next_image
	// (spudgpud3d12swapchain.cpp) for where this gets waited on.
	Microsoft::WRL::ComPtr<ID3D12Fence1> _frame_fence;
	uint64_t _frame_fence_next_value;
} spudgpu_swap_chain_d3d12;

#endif // SPUDGPU_COMPILE_D3D12_API
