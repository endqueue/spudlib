# SpudGPU adapters and devices TODO

Written on 2026-10-21. A proposal, not started: agree on the header section
below before any backend code changes, as `SPUDFILES_TODO.md` asks for
SpudFiles.

`spudgpu_enumerate_devices` both lists the GPUs and creates a device on every
one of them. Every graphics API SpudGPU translates keeps those apart (Vulkan's
`vkEnumeratePhysicalDevices` / `vkCreateDevice`, DXGI's `EnumAdapters1` /
`D3D12CreateDevice`, WebGPU's `requestAdapter` / `requestDevice`), and this
splits them the same way: an **adapter** says what a GPU is and can do and
costs nothing to list; a **device** is created on one, by the caller, with the
features and queues the caller asks for.

## What is wrong now

- **A device on every GPU, made for the caller.** The first
  `spudgpu_enumerate_devices` creates a logical device (and on Vulkan the
  bindless table) on every GPU the API lists. On a laptop with an integrated
  and a discrete GPU that powers up the discrete one, and on Vulkan each
  device costs driver memory and threads, whichever one the caller then uses.
- **Features switched on by SpudGPU, not the caller.** Vulkan enables
  features at `vkCreateDevice`, and the backend enables everything it finds
  (`shaderOutputLayer`, `depthBiasClamp`, `independentBlend`,
  `multiDrawIndirect`, `imageCubeArray`, depth bounds, mesh shading,
  descriptor indexing). That is a decision made for the caller, against the
  one rule, and some cost something just by being on.
- **No destroy, so no recovery.** Devices belong to the instance and live as
  long as it does. A lost device (a TDR, a driver reset, an eGPU unplugged)
  can only be replaced by destroying the whole instance.
- **Queues fixed by SpudGPU.** Vulkan makes one graphics queue; D3D12 four of
  each kind (`SPUD_D3D12_COMMAND_QUEUE_COUNT_PER_FAMILY`). The caller can't
  ask for a compute or copy queue on Vulkan at all
  (`spudgpu_get_command_queue` and `spudgpu_get_max_queue_count` are not
  written there).
- **Lazy state that had to be locked.** The device array is made by the first
  enumerate call, which is why `_devices_lock` exists on Vulkan and the
  D3D12 and Metal first calls still race. D3D12's bindless table is made by
  the first bindless call, which is why `_bindless_create_mutex` exists.
- **Capabilities asked of the wrong object.** `spudgpu_get_*_capabilities`
  and most of `SPUDGPU_DEVICE_PROPERTIES` describe the hardware, but are
  asked of a device; once a device exists the question that matters is what
  it was created with.

## What the callers use

As of 2026-10-21, `grep -rlE "\bspudgpu_<name>\("` over the workspace, by
repository (files):

| Call | Callers |
|---|---|
| `spudgpu_enumerate_devices` | spudgpusamples 9, latran 3, libdemo 2, apricottests 1, trellis 1 |
| `spudgpu_get_graphics_queue` | the same 16 files |
| `spudgpu_get_device_properties` | spudgpusamples 9, ApricotFields 1, apricottests 1, libdemo 1, trellis 1 |
| `spudgpu_get_mesh_shading_capabilities` | ApricotFields 2, spudgpusamples 1 |
| `spudgpu_get_bindless_capabilities`, `_depth_bounds_capabilities` | spudgpusamples 1 each |
| `spudgpu_get_vk_physical_device`, `_vk_device`, `_d3d12_device` | libdemo 1 each |
| `spudgpu_get_command_queue`, `_max_queue_count` | none |

Every caller takes `devices[0]` or an index it was given, and the graphics
queue. Aprend reads `unified_memory`, `device_mappable_memory`,
`layered_rendering`, `max_draw_indirect_count`, `pass_timestamps` and the
two offset alignments. Run the grep again before changing anything.

## The proposal

The rules, then the header section.

- **Listing creates nothing.** The instance lists its adapters when it is
  created (`vkEnumeratePhysicalDevices`, `EnumAdapters1`,
  `MTLCopyAllDevices`, each cheap) and never changes the list, so every
  adapter call may be made from any thread with no lock. The list is in the
  API's order, unsorted: preferring one GPU is the caller's.
- **An adapter is listed only if a device can be created on it.** Vulkan:
  1.3 with dynamic rendering and timeline semaphores. D3D12:
  `D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0, ..., nullptr)`, which
  tests without creating. Metal: Metal 3. Software adapters are listed and
  marked; filtering them is the caller's.
- **Supported is the adapter's, enabled is the device's.** An adapter reports
  `supported_features`; a device has exactly its `required_features`, on
  every backend. Using a feature the device wasn't created with is refused
  with the result it gets today when the device lacks it, and a command a
  device can't record (a mesh dispatch, a depth bounds call) records nothing
  and asserts in a debug build. This is the header's usual answer to APIs
  that differ underneath: Vulkan needs features switched on, D3D12 and Metal
  don't, and the narrowest rule all three keep is "ask first".
- **The caller asks for queues.** By kind and count, up to what the adapter
  reports. The device makes them all at creation and owns them;
  `spudgpu_get_command_queue` hands them out (and is written for Vulkan).
- **One device per adapter at a time.** D3D12 returns the same device to a
  second `D3D12CreateDevice` on one adapter, and Metal has one `MTLDevice`
  per GPU; Vulkan would allow more, but the narrowest rule is one. A flag on
  the adapter, set atomically, is the only state this needs, and it is
  SpudGPU's to guard (the adapter is shared, hidden state).
- **Nothing is made later.** The bindless table is made by
  `spudgpu_create_device` when `SPUDGPU_FEATURE_BINDLESS` is asked for, and
  not otherwise, on every backend. `_devices_lock` and
  `_bindless_create_mutex` go away with the lazy state they guarded.
- **A device can be destroyed and made again.** `spudgpu_destroy_device`, and
  then `spudgpu_create_device` on the same adapter, is also the recovery from
  a lost device.

### The header section

To replace `spudgpu.h` from `spudgpu_enumerate_devices` through
`spudgpu_get_command_queue`, keeping the vendor IDs and
`spudgpu_get_native_gpu_api`. It compiles as C11, C23 and C++20 against the
current `spudgpu.h` (2026-10-21).

```c
/** Opaque handle to a GPU the graphics API lists: what it is and what it can
 * do. Owned by the instance; listing one creates nothing on it. */
typedef struct spudgpu_adapter_t *spudgpu_adapter;

/**
 * @brief What kind of GPU an adapter is, as the graphics API reports it.
 */
typedef uint32_t SPUDGPU_ADAPTER_TYPE;
enum {
	/** The API reports no kind, or one not listed here. */
	SPUDGPU_ADAPTER_TYPE_OTHER = 0,
	/** A GPU sharing the system's memory and package (Vulkan
	 * `INTEGRATED_GPU`; D3D12 `UMA`; Metal `hasUnifiedMemory` and not
	 * `isRemovable`). */
	SPUDGPU_ADAPTER_TYPE_INTEGRATED = 1,
	/** A GPU with memory of its own (Vulkan `DISCRETE_GPU`; D3D12 not
	 * `UMA`; Metal not `hasUnifiedMemory`). */
	SPUDGPU_ADAPTER_TYPE_DISCRETE = 2,
	/** A GPU a hypervisor presents (Vulkan `VIRTUAL_GPU`). */
	SPUDGPU_ADAPTER_TYPE_VIRTUAL = 3,
	/** A renderer that runs on the CPU (Vulkan `CPU`, such as lavapipe or
	 * SwiftShader; D3D12 `DXGI_ADAPTER_FLAG3_SOFTWARE`, WARP). Listed like
	 * any other: whether to use one is the caller's. */
	SPUDGPU_ADAPTER_TYPE_SOFTWARE = 4
};

/**
 * @brief Capabilities an adapter may or may not have, which a device has only
 * when it was created with them.
 *
 * A bit set in SPUDGPU_ADAPTER_PROPERTIES::supported_features says the
 * adapter has it. A device has exactly the features in its
 * spudgpu_device_desc::required_features, on every backend: using one the
 * device was not created with is refused with that feature's result, as if
 * the adapter lacked it, even where the graphics API underneath would allow
 * it. Vulkan switches most of these on when its device is created, and some
 * cost something just by being on; D3D12 and Metal have nothing to switch on,
 * and hold the device to the same rule.
 *
 * A feature behind an SPUDGPU_EXT_* macro that is 0 in this build is never
 * supported. New features are new bits; a bit's meaning never changes.
 * The bits are macros, not an enum: before C23 an enumerator can't go past
 * an int, and the set may grow past 32.
 */
typedef uint64_t SPUDGPU_FEATURES;

/** spudgpu_rendering_begin_desc::layer_count above 0. Was
 * SPUDGPU_DEVICE_PROPERTIES::layered_rendering. */
#define SPUDGPU_FEATURE_LAYERED_RENDERING (UINT64_C(1) << 0)
/** spudgpu_shader_pipeline_desc::depth_bias_clamp other than 0;
 * refused with SPUDRESULT_GPU_DEPTH_BIAS_CLAMP_NOT_SUPPORTED. */
#define SPUDGPU_FEATURE_DEPTH_BIAS_CLAMP (UINT64_C(1) << 1)
/** Color attachments that blend differently from each other; refused
 * with SPUDRESULT_GPU_INDEPENDENT_BLEND_NOT_SUPPORTED. */
#define SPUDGPU_FEATURE_INDEPENDENT_BLEND (UINT64_C(1) << 2)
/** An indirect draw of more than one entry per call, up to
 * SPUDGPU_ADAPTER_PROPERTIES::max_draw_indirect_count. Without it, 1. */
#define SPUDGPU_FEATURE_MULTI_DRAW_INDIRECT (UINT64_C(1) << 3)
/** SPUDGPU_IMAGE_VIEW_TYPE_CUBE_ARRAY views, and shaders that declare a
 * cube array. */
#define SPUDGPU_FEATURE_CUBE_ARRAY_VIEWS (UINT64_C(1) << 4)
/** spudgpu_create_timestamp_set(); refused with
 * SPUDRESULT_GPU_TIMESTAMPS_NOT_SUPPORTED. */
#define SPUDGPU_FEATURE_PASS_TIMESTAMPS (UINT64_C(1) << 5)
/** Buffers of SPUDGPU_MEMORY_KIND_DEVICE_MAPPABLE; refused with
 * SPUDRESULT_GPU_MEMORY_KIND_NOT_SUPPORTED. */
#define SPUDGPU_FEATURE_DEVICE_MAPPABLE_MEMORY (UINT64_C(1) << 6)
/** Heaps of a mappable SPUDGPU_MEMORY_KIND. Was
 * SPUDGPU_DEVICE_PROPERTIES::mappable_heaps. */
#define SPUDGPU_FEATURE_MAPPABLE_HEAPS (UINT64_C(1) << 7)
/** The device's bindless table and every spudgpu_bindless_* call
 * (SPUDGPU_EXT_BINDLESS_DESCRIPTOR_INDEXING). The table is made with the
 * device, and only when this is asked for. */
#define SPUDGPU_FEATURE_BINDLESS (UINT64_C(1) << 8)
/** Mesh-shader pipelines and spudgpu_cmd_dispatch_mesh()
 * (SPUDGPU_EXT_MESH_SHADING). */
#define SPUDGPU_FEATURE_MESH_SHADING (UINT64_C(1) << 9)
/** The depth bounds test (SPUDGPU_EXT_DEPTH_BOUNDS_TEST). */
#define SPUDGPU_FEATURE_DEPTH_BOUNDS_TEST (UINT64_C(1) << 10)

/**
 * @brief What an adapter is and what it can do, as the graphics API reports
 * it.
 *
 * Facts only: nothing here is chosen. Limits that only matter with a feature
 * are chained onto `base.next` (SPUDGPU_MESH_SHADING_LIMITS,
 * SPUDGPU_BINDLESS_LIMITS) and filled in only when the adapter supports it.
 */
typedef struct SPUDGPU_ADAPTER_PROPERTIES {
	/** `type` is SPUD_STRUCT_TYPE_GPU_ADAPTER_PROPERTIES; `next` may chain
	 * the limits structs. The caller builds the chain and sets each type;
	 * SpudGPU fills in the rest. See SPUD_STRUCT_BASE. */
	SPUD_STRUCT_BASE_OUT base;
	/** The adapter's name, null-terminated. */
	char description[128];
	/** PCI vendor ID, e.g. SPUDGPU_VENDOR_NVIDIA. */
	uint32_t vendor_id;
	/** PCI device ID. */
	uint32_t device_id;
	/** PCI subsystem ID. D3D12 only; 0 on Vulkan and Metal. */
	uint32_t subsystem_id;
	/** PCI revision. D3D12 only; 0 on Vulkan and Metal. */
	uint32_t revision;
	/** What kind of GPU it is. */
	SPUDGPU_ADAPTER_TYPE adapter_type;
	/** Every SPUDGPU_FEATURE_* the adapter has. */
	SPUDGPU_FEATURES supported_features;

	/** As SPUDGPU_DEVICE_PROPERTIES::dedicated_video_memory today. */
	uint64_t dedicated_video_memory;
	/** As SPUDGPU_DEVICE_PROPERTIES::dedicated_system_memory today. */
	uint64_t dedicated_system_memory;
	/** As SPUDGPU_DEVICE_PROPERTIES::shared_system_memory today. */
	uint64_t shared_system_memory;
	/** As SPUDGPU_DEVICE_PROPERTIES::unified_memory today. A fact about the
	 * memory, not a feature: nothing is switched on by it. */
	bool unified_memory;

	/** As SPUDGPU_DEVICE_PROPERTIES::uniform_buffer_offset_alignment today. */
	uint32_t uniform_buffer_offset_alignment;
	/** As SPUDGPU_DEVICE_PROPERTIES::storage_buffer_offset_alignment today. */
	uint32_t storage_buffer_offset_alignment;
	/** The most entries one indirect draw takes on a device created with
	 * SPUDGPU_FEATURE_MULTI_DRAW_INDIRECT; 1 without it, whatever this says.
	 * Vulkan: `VkPhysicalDeviceLimits::maxDrawIndirectCount`. D3D12 and
	 * Metal: UINT32_MAX. */
	uint32_t max_draw_indirect_count;

	/**
	 * The most queues of each kind one device on this adapter may be created
	 * with (spudgpu_device_desc). Vulkan: the `queueCount` of the family each
	 * kind is made from (see spudgpu_device_desc); families shared between
	 * kinds share their count. D3D12 and Metal have no limit: UINT32_MAX.
	 */
	uint32_t max_direct_queues;
	/** As #max_direct_queues, for SPUDGPU_COMMAND_LIST_TYPE_COMPUTE. */
	uint32_t max_compute_queues;
	/** As #max_direct_queues, for SPUDGPU_COMMAND_LIST_TYPE_COPY. */
	uint32_t max_copy_queues;
} SPUDGPU_ADAPTER_PROPERTIES;

/** @brief Mesh shading's limits, chained onto SPUDGPU_ADAPTER_PROPERTIES.
 * All 0 when the adapter lacks SPUDGPU_FEATURE_MESH_SHADING. Replaces
 * spudgpu_mesh_shading_capabilities. */
typedef struct SPUDGPU_MESH_SHADING_LIMITS {
	/** `type` is SPUD_STRUCT_TYPE_GPU_MESH_SHADING_LIMITS. */
	SPUD_STRUCT_BASE_OUT base;
	/** Most vertices one mesh shader workgroup may emit. */
	uint32_t max_mesh_output_vertices;
	/** Most primitives one mesh shader workgroup may emit. */
	uint32_t max_mesh_output_primitives;
	/** Most invocations in one mesh shader workgroup. */
	uint32_t max_mesh_workgroup_invocations;
} SPUDGPU_MESH_SHADING_LIMITS;

/** @brief The bindless table's size, chained onto
 * SPUDGPU_ADAPTER_PROPERTIES. All 0 when the adapter lacks
 * SPUDGPU_FEATURE_BINDLESS. Replaces spudgpu_bindless_capabilities. */
typedef struct SPUDGPU_BINDLESS_LIMITS {
	/** `type` is SPUD_STRUCT_TYPE_GPU_BINDLESS_LIMITS. */
	SPUD_STRUCT_BASE_OUT base;
	/** Number of slots in the sampled image table. */
	uint32_t max_sampled_images;
	/** Number of slots in the storage image table. */
	uint32_t max_storage_images;
	/** Number of slots in the storage buffer table. */
	uint32_t max_storage_buffers;
} SPUDGPU_BINDLESS_LIMITS;

/**
 * @brief Lists the adapters an instance can create devices on.
 *
 * The list is read when the instance is created and never changes, so this
 * may be called from any thread. It is in the order the graphics API gives
 * (Vulkan's loader, DXGI's `EnumAdapters1`, `MTLCopyAllDevices`): SpudGPU
 * neither sorts nor filters it by preference. An adapter is listed only if a
 * device can be created on it; software adapters are listed, with
 * SPUDGPU_ADAPTER_TYPE_SOFTWARE. No adapter is a count of 0, not a failure.
 *
 * The adapters and the array belong to the instance and are released by
 * spudgpu_destroy_instance(); the caller frees neither.
 *
 * @param[in]  instance           Instance to list adapters of.
 * @param[out] out_adapters       Receives a pointer to the instance's array.
 * @param[out] out_adapters_count Receives the number of adapters in it.
 *
 * @retval SPUD_SUCCESS Both outputs were written.
 * @retval SPUDRESULT_GPU_INVALID_INSTANCE `instance` is NULL.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_adapters` or
 *         `out_adapters_count` is NULL.
 */
SPUDRESULT spudgpu_enumerate_adapters(
    spudgpu_instance instance,
    const spudgpu_adapter **out_adapters,
    uint32_t *out_adapters_count);

/**
 * @brief Reads what an adapter is and can do.
 *
 * May be called from any thread. Creates nothing on the adapter.
 *
 * @param[in]  adapter        Adapter to query.
 * @param[out] out_properties Receives its properties, and those of every
 *                            struct chained onto it.
 *
 * @retval SPUD_SUCCESS `out_properties` and its chain were filled in.
 * @retval SPUDRESULT_GPU_INVALID_ADAPTER `adapter` is NULL.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_properties` is NULL.
 * @retval SPUDRESULT_STRUCT_WRONG_TYPE `out_properties->base.type` is not
 *         SPUD_STRUCT_TYPE_GPU_ADAPTER_PROPERTIES.
 * @retval SPUDRESULT_STRUCT_UNKNOWN_IN_CHAIN A struct in the chain is not
 *         one of the limits structs.
 */
SPUDRESULT spudgpu_get_adapter_properties(
    spudgpu_adapter adapter,
    SPUDGPU_ADAPTER_PROPERTIES *out_properties);

/**
 * @brief Reports whether a device on an adapter can present to a surface.
 *
 * Vulkan: `vkGetPhysicalDeviceSurfaceSupportKHR` for the family direct
 * queues are made from. D3D12 and Metal: always true.
 *
 * @param[in]  adapter       Adapter to ask about.
 * @param[in]  surface       Surface to present to.
 * @param[out] out_supported Receives the answer. False is an answer, not a
 *                           failure.
 *
 * @retval SPUD_SUCCESS `out_supported` was written.
 * @retval SPUDRESULT_GPU_INVALID_ADAPTER `adapter` is NULL.
 * @retval SPUDRESULT_GPU_INVALID_SURFACE `surface` is NULL.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_supported` is NULL.
 * @return SPUDRESULT_API_SPECIFIC_FAILURE if the graphics API fails.
 */
SPUDRESULT spudgpu_adapter_supports_surface(
    spudgpu_adapter adapter,
    spudgpu_surface surface,
    bool *out_supported);

/**
 * @brief What the caller decides when creating a device. No field has a
 * default: a zeroed desc asks for no features and no queues, which is
 * refused.
 */
typedef struct spudgpu_device_desc {
	/** `type` is SPUD_STRUCT_TYPE_GPU_DEVICE_DESC; `next` is NULL (nothing
	 * is chained onto this struct yet). Set by the caller. See
	 * SPUD_STRUCT_BASE. */
	SPUD_STRUCT_BASE base;
	/** The features the device has (SPUDGPU_FEATURE_*), each of which the
	 * adapter must support. Anything not asked for here the device lacks. */
	SPUDGPU_FEATURES required_features;
	/**
	 * How many queues of each kind the device has, each at most the
	 * adapter's SPUDGPU_ADAPTER_PROPERTIES::max_*_queues. Their sum is at
	 * least 1. Vulkan makes each kind from one queue family: direct queues
	 * from the first family with graphics; compute queues from the first
	 * family with compute and without graphics, else the direct family; copy
	 * queues from the first family with transfer only, else the compute
	 * family. Kinds that share a family share its `queueCount`.
	 */
	uint32_t direct_queue_count;
	/** As #direct_queue_count, for SPUDGPU_COMMAND_LIST_TYPE_COMPUTE. */
	uint32_t compute_queue_count;
	/** As #direct_queue_count, for SPUDGPU_COMMAND_LIST_TYPE_COPY. */
	uint32_t copy_queue_count;
	/** Name for debuggers and validation messages, or NULL. Copied. */
	const char *debug_name;
} spudgpu_device_desc;

/**
 * @brief Creates a device on an adapter, with the features and queues the
 * caller asks for.
 *
 * One device per adapter at a time, on every backend: D3D12 hands back the
 * same device to a second `D3D12CreateDevice` on one adapter, and Metal has
 * one `MTLDevice` per GPU, so a second device would not be a separate one
 * there. Two threads creating on one adapter at once get one device and
 * SPUDRESULT_GPU_ADAPTER_IN_USE.
 *
 * Vulkan: `vkCreateDevice` with the extensions and feature bits
 * `required_features` needs, and nothing else switched on. D3D12:
 * `D3D12CreateDevice` at feature level 12_0, and the queues. Metal: the
 * queues on the adapter's `MTLDevice`.
 *
 * @param[in]  adapter    Adapter to create the device on.
 * @param[in]  desc       What the caller decides.
 * @param[out] out_device Receives the new device on success; NULL on
 *                        failure.
 *
 * @retval SPUD_SUCCESS The device was created.
 * @retval SPUDRESULT_GPU_INVALID_ADAPTER `adapter` is NULL.
 * @retval SPUDRESULT_NULL_DESC `desc` is NULL.
 * @retval SPUDRESULT_STRUCT_WRONG_TYPE `desc->base.type` is not
 *         SPUD_STRUCT_TYPE_GPU_DEVICE_DESC.
 * @retval SPUDRESULT_STRUCT_UNKNOWN_IN_CHAIN `desc->base.next` is not NULL.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_device` is NULL.
 * @retval SPUDRESULT_GPU_FEATURE_NOT_SUPPORTED A bit of
 *         `desc->required_features` is not in the adapter's
 *         `supported_features`.
 * @retval SPUDRESULT_ZERO_SIZE The three queue counts are all 0.
 * @retval SPUDRESULT_GPU_TOO_MANY_QUEUES A queue count is above the
 *         adapter's limit for its kind.
 * @retval SPUDRESULT_GPU_ADAPTER_IN_USE The adapter already has a device.
 * @return Another SPUDRESULT if the backend fails, such as
 *         SPUDRESULT_OUT_OF_MEMORY or SPUDRESULT_API_SPECIFIC_FAILURE.
 *
 * @see spudgpu_destroy_device()
 */
SPUDRESULT spudgpu_create_device(
    spudgpu_adapter adapter,
    const spudgpu_device_desc *desc,
    spudgpu_device *out_device);

/**
 * @brief Destroys a device and its queues, and frees its adapter for
 * another.
 *
 * Also the way back from SPUDRESULT_GPU_DEVICE_LOST: destroy the lost device
 * and everything made from it, and create a new one on the same adapter.
 *
 * @warning Every object created from the device must be destroyed first, and
 * every submission to its queues must have finished (or the device be lost).
 * No other thread may be using it.
 *
 * @param[in] device Device to destroy. NULL does nothing.
 */
void spudgpu_destroy_device(spudgpu_device device);

/**
 * @brief Returns the adapter a device was created on.
 *
 * @param[in] device Device to query.
 *
 * @return The adapter, or NULL if `device` is NULL.
 */
spudgpu_adapter spudgpu_get_device_adapter(spudgpu_device device);

/**
 * @brief Returns the features a device was created with: its
 * spudgpu_device_desc::required_features.
 *
 * @param[in] device Device to query.
 *
 * @return The features, or 0 if `device` is NULL.
 */
SPUDGPU_FEATURES spudgpu_get_device_features(spudgpu_device device);

/**
 * @brief Returns one of the queues a device was created with.
 *
 * The device owns the queue; it is not destroyed by the caller. The same
 * kind and index always give the same queue. Replaces
 * spudgpu_get_graphics_queue(), which is (DIRECT, 0).
 *
 * @param[in]  device    Device to get the queue from.
 * @param[in]  type      Kind of queue: DIRECT, COMPUTE or COPY.
 * @param[in]  index     Which queue of that kind, below the count the device
 *                       was created with.
 * @param[out] out_queue Receives the queue on success.
 *
 * @retval SPUD_SUCCESS `out_queue` was written.
 * @retval SPUDRESULT_GPU_INVALID_DEVICE `device` is NULL.
 * @retval SPUDRESULT_GPU_INVALID_COMMAND_LIST_TYPE `type` is BUNDLE, or not a
 *         kind of command list.
 * @retval SPUDRESULT_INDEX_OUT_OF_RANGE `index` is not below the count the
 *         device was created with for `type`.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_queue` is NULL.
 */
SPUDRESULT spudgpu_get_command_queue(
    spudgpu_device device,
    SPUDGPU_COMMAND_LIST_TYPE type,
    uint32_t index,
    spudgpu_command_queue *out_queue);
```

### What goes, and what replaces it

| Now | Then |
|---|---|
| `spudgpu_enumerate_devices` | `spudgpu_enumerate_adapters`, then `spudgpu_create_device` |
| `SPUDGPU_DEVICE_PROPERTIES`, `spudgpu_get_device_properties` | `SPUDGPU_ADAPTER_PROPERTIES`, `spudgpu_get_adapter_properties(spudgpu_get_device_adapter(device), ...)` |
| its `layered_rendering`, `depth_bias_clamp`, `independent_blend`, `pass_timestamps`, `cube_array_views`, `device_mappable_memory`, `mappable_heaps` | `SPUDGPU_FEATURE_*` bits |
| its `subSys_id` | `subsystem_id` |
| `spudgpu_get_bindless_capabilities`, `spudgpu_bindless_capabilities` | `SPUDGPU_FEATURE_BINDLESS` and `SPUDGPU_BINDLESS_LIMITS` |
| `spudgpu_get_mesh_shading_capabilities`, `spudgpu_mesh_shading_capabilities` | `SPUDGPU_FEATURE_MESH_SHADING` and `SPUDGPU_MESH_SHADING_LIMITS` |
| `spudgpu_get_depth_bounds_capabilities`, `spudgpu_depth_bounds_capabilities` | `SPUDGPU_FEATURE_DEPTH_BOUNDS_TEST` |
| `spudgpu_get_graphics_queue` | `spudgpu_get_command_queue(device, SPUDGPU_COMMAND_LIST_TYPE_DIRECT, 0, &queue)` |
| `spudgpu_get_max_queue_count` | the count the device was created with |
| `spudgpu_get_vk_physical_device(device)` | `spudgpu_get_vk_physical_device(adapter)`; D3D12 gains `spudgpu_get_dxgi_adapter(adapter)`, Metal `spudgpu_get_metal_device(adapter)` |

The `SPUDGPU_EXT_*` macros stay as they are: they say what a build can do at
all; the feature bits say what one adapter and one device can.

New `SPUDRESULT`s, in the unused 211-215:
`SPUDRESULT_GPU_INVALID_ADAPTER` (211), `_FEATURE_NOT_SUPPORTED` (212),
`_TOO_MANY_QUEUES` (213), `_ADAPTER_IN_USE` (214) and `_DEVICE_LOST` (215,
for the step below). New `SPUD_STRUCT_TYPE`s:
`_GPU_ADAPTER_PROPERTIES` (1026), `_GPU_DEVICE_DESC` (1027),
`_GPU_MESH_SHADING_LIMITS` (1028), `_GPU_BINDLESS_LIMITS` (1029). The
retired types (1000, 1017, 1020, 1021) are never reused.
`SPUDRESULT_GPU_DEVICE_ENUMERATION_FAILURE` becomes what
`spudgpu_create_instance` returns when the API can't list its GPUs; no
adapter is a count of 0, not a failure.

### What a caller looks like

```c
const spudgpu_adapter *adapters;
uint32_t adapter_count;
spudgpu_enumerate_adapters(instance, &adapters, &adapter_count);
if (adapter_count == 0) /* no GPU: the caller's to handle */;

SPUDGPU_MESH_SHADING_LIMITS mesh = {.base.type = SPUD_STRUCT_TYPE_GPU_MESH_SHADING_LIMITS};
SPUDGPU_ADAPTER_PROPERTIES props = {.base.type = SPUD_STRUCT_TYPE_GPU_ADAPTER_PROPERTIES,
                                    .base.next = &mesh.base};
spudgpu_get_adapter_properties(adapters[0], &props);

spudgpu_device_desc desc = {.base.type = SPUD_STRUCT_TYPE_GPU_DEVICE_DESC};
desc.required_features = props.supported_features &
    (SPUDGPU_FEATURE_MESH_SHADING | SPUDGPU_FEATURE_PASS_TIMESTAMPS);
desc.direct_queue_count = 1;

spudgpu_device device;
spudgpu_create_device(adapters[0], &desc, &device);
spudgpu_command_queue queue;
spudgpu_get_command_queue(device, SPUDGPU_COMMAND_LIST_TYPE_DIRECT, 0, &queue);
/* ... */
spudgpu_destroy_device(device);
spudgpu_destroy_instance(instance);
```

### Each backend

| | Adapter | `spudgpu_create_device` | Features |
|---|---|---|---|
| Vulkan | `VkPhysicalDevice`, properties and features read at instance creation | `vkCreateDevice`: the queue families the desc's counts need, `VK_KHR_swapchain`, and only the extensions and feature bits `required_features` names; then the queues, and the bindless table if asked | each bit maps to the `VkPhysicalDeviceFeatures*` member or extension it needs today |
| D3D12 | `IDXGIAdapter4`, with `DXGI_ADAPTER_DESC3` and the `CheckFeatureSupport` answers read through a probe device released at once, or kept for the first device | `D3D12CreateDevice` at 12_0, the queues, the debug callback, and the shader-visible heaps and bindless range if asked | nothing to switch on; the device checks its bits |
| Metal | `id<MTLDevice>` from `MTLCopyAllDevices` | the queues, and the bindless argument-buffer state if asked | nothing to switch on; the device checks its bits |

D3D12's properties need a device to ask (`CheckFeatureSupport` is a device
method): the adapter makes one when the instance is created, reads what it
needs, and releases it. That creates and drops a device per D3D12 adapter
once, at instance creation; see the open questions.

### Device loss, a step after

`SPUDRESULT_GPU_DEVICE_LOST` is only useful if every call that can see it
returns it. Today `spudgpu_swap_chain_present` and `spudgpu_queue_wait_idle`
return nothing and `spudgpu_swap_chain_acquire_next_image` returns an index.
After this change: those return a `SPUDRESULT` (acquire gives its index
through an output), and submit, present, acquire, wait-idle and
`spudgpu_wait_for_fences` return `SPUDRESULT_GPU_DEVICE_LOST` when the API
says so (`VK_ERROR_DEVICE_LOST`, `DXGI_ERROR_DEVICE_REMOVED` /
`_RESET`, `MTLCommandBufferErrorDeviceRemoved`). A separate change, since it
touches the frame loop of every caller.

## Open questions

- **D3D12's probe device.** `D3D12CreateDevice` returns the process's one
  device per adapter, so a probe device kept alive becomes the caller's
  device later, and one released is made again at `spudgpu_create_device`.
  Keeping it costs nothing a caller who uses that adapter wasn't going to
  pay, and something for one who doesn't. Release it, unless creating it
  twice turns out to be slow.
- **Vulkan's queue families.** The desc's comment picks a family per kind by
  rule (dedicated first, then shared). That is mechanism with no choice in
  it, but it hides families from the caller. The alternative, families in
  the adapter's properties and the caller naming one per kind, is Vulkan's
  own shape and has no D3D12 or Metal meaning. Proposed: the rule.
- **`spudgpu_get_graphics_queue`.** Sixteen caller files use it. Removing it
  is the consistent answer (it is `(DIRECT, 0)`), but it could stay as a
  one-line wrapper for one release.
- **Features no caller has asked for yet.** Only the capabilities SpudGPU
  already tracks are bits. Anything Vulkan 1.3 core guarantees
  (dynamic rendering, timeline semaphores, descriptor indexing's base) is
  required of an adapter, not a feature.
- **A migration shim.** A `spudgpu_enumerate_devices` kept for a while, that
  creates a device on adapter 0 with every supported feature, would let the
  callers move one at a time. Proposed: no shim; change the callers in the
  same change, as the version policy allows before 1.0.0.

## Breaks

Every caller in the table above: 16 files call `spudgpu_enumerate_devices`
and `spudgpu_get_graphics_queue`, Aprend reads the properties and the mesh
shading capability, and `libdemo` the natives. Change them in the same
change, and build and run each one's tests. All three backends change: build
`macos-metal`, `linux-vulkan` and `windows-d3d12`.

## Done when

The steps every item of `MATURITY_TODO.md` ends with:

- build on macOS and run the tests, plain and under the `-asan` and `-tsan`
  presets;
- build every caller the change breaks, in the same change, and run its
  tests (`aprend_gpu_tests` on all three backends, the samples);
- `./check.sh` from the workspace;
- after the spudlib commit is pushed, `./check.sh --update-locks`, and commit
  the locks with the repositories that use them;
- each changed repository's `.git/COMMIT_DRAFT` rewritten to describe its
  change;
- `CLAUDE.md`: the threading entry under "Known gaps" shortened (the device
  array and the lazy bindless table are gone), and the "lazily created
  member" exception no longer naming `_bindless`.
