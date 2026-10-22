# SpudGPU adapters and devices TODO

Written on 2026-10-21; the header section agreed and implemented the same
day, in `spudgpu.h` and all three backends.

**State (2026-10-21):**

- Metal: built on macOS (Apple M5 Pro), Debug and Release.
  `tests/spudgpu_device_test` (new) passes, plain and under the `-asan` and
  `-tsan` presets; the seven samples the Metal build has ran 120 frames each
  in a Debug build, with the live-object check on.
- D3D12: syntax-checked with mingw-w64 GCC against DirectX-Headers 1.616,
  Debug and Release, every file clean but for mingw's own ABI difference
  in the `GetDesc`-style getters. Not compiled by MSVC, not run.
- Vulkan: syntax-checked with mingw-w64 GCC against the Khronos headers, as
  a Windows build, Debug and Release, clean. Not compiled on Linux, not run.
- The samples: all nine syntax-checked as a Vulkan build;
  SpudGPUBundles and SpudGPUDynamicIndexing (not in the Metal build) not
  otherwise compiled.
- Every other caller (`../SPUDGPU_DEVICE_MIGRATION.md`) is not converted
  yet, and does not build against this `spudgpu.h`.

The rest of this file is the design as agreed, kept as the record of why.

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
  one rule. It also hides mistakes: a caller that uses a feature without
  checking for it works on the machine it was written on, where the GPU
  happens to have it, and fails only on a user's GPU that lacks it. With
  only what was asked for switched on, a forgotten request fails on the
  developer's own machine.
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

Every call this replaces is used outside SpudLib, except
`spudgpu_get_command_queue` and `spudgpu_get_max_queue_count`, which
nothing calls. Every caller takes `devices[0]` or an index it was given,
and the graphics queue. Which callers use what, what each has to change and
in what order is in the workspace's `../SPUDGPU_DEVICE_MIGRATION.md`; this
file names none, as SpudLib names no caller.

## The proposal

The rules, then the header section.

- **Listing keeps nothing on an adapter.** The instance lists its adapters
  when it is created (`vkEnumeratePhysicalDevices`, `EnumAdapters1`,
  `MTLCopyAllDevices`) and never changes the list, so every adapter call may
  be made from any thread with no lock. The list is in the API's order,
  unsorted: preferring one GPU is the caller's. D3D12 is the one backend
  that has to create something to list: its features are only answered by a
  device (`CheckFeatureSupport`), so `spudgpu_create_instance` creates a
  probe device on each adapter in turn, reads it and releases it before the
  next (see "D3D12's probe device" below). The header says so, so that a
  discrete GPU waking at instance creation has its reason written down.
- **An adapter is listed only if a device can be created on it.** Vulkan:
  the physical device's `apiVersion` is 1.3 or later (unchecked today: a
  1.2 device is listed and fails at `vkCreateDevice`). D3D12: the probe
  device at feature level 12_0 is created. Metal: Metal 3. Software
  adapters (WARP, lavapipe, SwiftShader) are listed and marked; filtering
  them is the caller's. (D3D12 skips WARP today: a choice made for the
  caller, which goes.)
- **The list never changes, and nothing reports a GPU added or removed.**
  A GPU plugged in after the instance was made is not in its list; one
  unplugged leaves an adapter no device can be created on. A new instance
  lists the system as it is then. A polled call that says the list is out
  of date is a later item (see "Later, not in this change").
- **An adapter names its GPU.** `adapter_id` is equal for two adapters, in
  any instances of any processes, only if they are the same GPU; the
  reverse does not hold (a restart, a driver restart or a replug may give
  the GPU a new one), so the caller never stores one. The Windows LUID is a
  separate field, for interop that names an adapter by it. A choice saved
  across runs is matched by the hardware IDs and the description: the
  caller's.
- **Supported is the adapter's, enabled is the device's.** An adapter reports
  `supported_features`; a device has exactly its `required_features`, on
  every backend. Using a feature the device wasn't created with is refused
  with the result it gets today when the device lacks it (such as
  `SPUDRESULT_GPU_TIMESTAMPS_NOT_SUPPORTED`), or with
  `SPUDRESULT_GPU_FEATURE_NOT_SUPPORTED` where there is none (a swap chain);
  a command a device can't record (a mesh dispatch, a depth bounds call)
  records nothing and asserts in a debug build. This is the header's usual
  answer to APIs that differ underneath: Vulkan needs features switched on,
  D3D12 and Metal don't, and the narrowest rule all three keep is "ask
  first".
- **Vulkan's base set is switched on for every device, and nothing else
  but what is asked for.** SpudGPU itself relies on `dynamicRendering`,
  `synchronization2` (barriers), `timelineSemaphore` (`spudgpu_fence`) and
  `bufferDeviceAddress` (every buffer has an address); Vulkan 1.3 requires
  all four, so the version check covers them. Everything else, the
  descriptor-indexing bits included (today switched on unconditionally,
  though only the bindless code uses update-after-bind and partially
  bound), is switched on only by the feature bit that needs it.
- **The caller asks for queues.** By kind and count, up to what the adapter
  reports. The device makes them all at creation and owns them;
  `spudgpu_get_command_queue` hands them out (and is written for Vulkan).
- **Queues come from pools, and the adapter lists them.** Vulkan makes each
  kind from one family, by the rule in `spudgpu_device_desc`, and kinds
  that fall back to one family share its `queueCount` (Intel Iris Xe: one
  family of one queue for all three kinds). Three separate maxima would
  hide that, so the adapter lists pools: the kinds each serves and the most
  queues it holds, and the desc's counts for a pool's kinds add up to at
  most that. D3D12 and Metal report three pools, one kind each, with no
  limit. Each pool says what the API reports about where its queues run:
  Vulkan knows the graphics family from a family without graphics; D3D12
  and Metal say nothing, which is reported as unknown rather than left to
  look like three separate families. No API promises that a queue kind has
  hardware of its own, or runs at the same time as graphics, and the header
  promises nothing either. (Settled 2026-10-21.)
- **SpudGPU sets no queue priority.** Vulkan gets the same value for every
  queue, which in its relative scheme is no preference; D3D12 gets
  `NORMAL`, its own no preference. The order work on different queues runs
  in is the driver's. Choosing a priority is a later feature bit (see
  "Later, not in this change"), since Metal has none.
- **One device per adapter at a time.** D3D12 returns the same device to a
  second `D3D12CreateDevice` on one adapter, and Metal has one `MTLDevice`
  per GPU; Vulkan would allow more, but the narrowest rule is one. A flag on
  the adapter, set atomically, is the only state this needs, and it is
  SpudGPU's to guard (the adapter is shared, hidden state).
- **One device per GPU in the process, across instances: the caller's
  rule.** Two instances list two adapters for one GPU, and on D3D12 a
  device made through each would be one `ID3D12Device` under two handles.
  SpudGPU can't see across instances without process-wide state, which it
  doesn't hold, so the caller keeps this, by `adapter_id`. It matters most
  in a recovery: the lost device is destroyed before a device on the same
  GPU is made in a new instance.
- **Nothing is made later, except what is keyed by a value the caller
  passes at a call or belongs to an object the caller synchronizes.**
  Settled 2026-10-21, against what engines do (Unreal's D3D12 RHI and
  NVRHI set up their descriptor heaps and bindless state at device
  creation):
  - The bindless table is made by `spudgpu_create_device` when
    `SPUDGPU_FEATURE_BINDLESS` is asked for, and not otherwise, on every
    backend. The pointer is set once and is null exactly when the feature
    wasn't asked for. `_devices_lock`, `_bindless_create_mutex` and D3D12's
    atomic `_bindless` go with the lazy state they guarded; the table keeps
    its own lock, since registering is done on any thread.
  - D3D12's shader-visible heaps (`_csv_heap`, `_sampler_heap`) are made by
    `spudgpu_create_device`, sized by the descriptor budget below, and not
    made at all for a budget of 0. A failure to make them fails the create
    call, not someone's first pool. `_shader_heap_mutex` stays, guarding the
    free lists only.
  - D3D12's indirect command signatures for tightly packed arguments
    (stride = the size of `spudgpu_draw_indirect_args`, and of
    `spudgpu_draw_indexed_indirect_args`) are made by
    `spudgpu_create_device`, so the common case takes no lock. A signature
    for any other stride is still made by the first draw that uses it,
    under `_indirect_signature_mutex`: it is keyed by the caller's stride,
    and making others ahead would be SpudGPU choosing strides. (Engines
    whose APIs fix the stride make every signature at init; a translator
    that takes a stride caches, as vkd3d-proton does the other way round.)
  - A command list's `_rtv_heap` and `_dsv_heap` stay lazy: the caller
    synchronizes the list, so this costs no lock and hides nothing shared.
    Engines write RTVs and DSVs once, at view creation, into a device-level
    CPU heap; that would need a shared allocator and its lock, and is not
    part of this change.
- **The caller sets the device's descriptor budget.** The shader-visible
  heaps' size was SpudGPU's choice (a million CBV/SRV/UAV descriptors and
  2048 samplers, made in full on first use), and Unreal and NVRHI both
  leave it to the application. A D3D12 heap size on the desc would be a
  field only one API acts on, so it is a rule all three keep instead: the
  desc's `max_descriptors` and `max_samplers` bound what the device's
  descriptor pools and bindless table hold at once, the adapter reports
  the most allowed, D3D12 sizes its heaps from the budget, and Vulkan and
  Metal count against it. A pool that doesn't fit is refused on every
  backend with `SPUDRESULT_GPU_OUT_OF_DESCRIPTORS`. Today only D3D12 refuses
  a pool when the samplers run out (with
  `SPUDRESULT_API_SPECIFIC_FAILURE`); this makes it one rule.
- **A debug build counts what is alive on a device.** Destroying a device
  with objects still made from it is the caller's mistake, and the
  graphics APIs report it unevenly (Vulkan's validation layer does; D3D12
  and Metal mostly don't). So a debug build keeps, per device, an atomic
  count of live objects for each kind of handle made from it (buffers,
  images, views, samplers, heaps, shader modules, pipelines, layouts,
  descriptor pools, command allocators and lists, fences, semaphores,
  swap chains, timestamp sets): each create call adds one when it
  succeeds, each destroy call takes one away. Atomic, because objects are
  made and destroyed on any thread. `spudgpu_destroy_device` prints every
  kind whose count isn't 0 (`spudgpu: device "name" destroyed with 3
  buffers, 1 pipeline alive`) and asserts. What has no destroy call of
  its own (descriptor sets, a swap chain's images and views) is counted
  through its owner, not separately; the queues are the device's own and
  not counted. A release build has no counter. This is the heaps' rule (a
  debug build counts live placements, and `spudgpu_destroy_heap` asserts
  the count is 0) applied to the device. The counters are in the backends'
  internal structs only; no public struct changes between builds.
- **A device can be destroyed and made again, and that is the recovery
  from a lost one.** Destroy everything made from the device, then the
  device; create a device on the same adapter. If that fails, for any
  reason, destroy the instance, make a new one, and find the GPU there by
  `adapter_id`, else by the hardware IDs, else choose again. No result
  tells "the adapter is gone" from another failure: D3D12 can't say so
  reliably at `D3D12CreateDevice`, and the caller does the same either
  way. This is WebGPU's rule (an adapter is spent; `requestAdapter` again)
  and DXGI's (`IsCurrent()`, a new factory, enumerate again). It covers a
  hang or reset (TDR), a driver update or restart, and an unplugged GPU.

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
 * (SPUDGPU_EXT_BINDLESS). The table is made with the
 * device, and only when this is asked for. */
#define SPUDGPU_FEATURE_BINDLESS (UINT64_C(1) << 8)
/** Mesh-shader pipelines and spudgpu_cmd_dispatch_mesh()
 * (SPUDGPU_EXT_MESH_SHADING). */
#define SPUDGPU_FEATURE_MESH_SHADING (UINT64_C(1) << 9)
/** The depth bounds test (SPUDGPU_EXT_DEPTH_BOUNDS_TEST). */
#define SPUDGPU_FEATURE_DEPTH_BOUNDS_TEST (UINT64_C(1) << 10)
/** spudgpu_create_swap_chain(), and spudgpu_adapter_supports_surface()
 * answering true; refused with SPUDRESULT_GPU_FEATURE_NOT_SUPPORTED.
 * Vulkan: VK_KHR_swapchain. D3D12 and Metal: always supported. A device that
 * never presents (tests, an offline render) need not ask, and a GPU with no
 * display (an NVIDIA card in TCC mode) is still listed for compute. */
#define SPUDGPU_FEATURE_PRESENT (UINT64_C(1) << 11)

/** The kinds of queue, as bits, for SPUDGPU_QUEUE_POOL. */
typedef uint32_t SPUDGPU_QUEUE_KINDS;
#define SPUDGPU_QUEUE_KIND_DIRECT (1u << 0)
#define SPUDGPU_QUEUE_KIND_COMPUTE (1u << 1)
#define SPUDGPU_QUEUE_KIND_COPY (1u << 2)

/**
 * @brief What the graphics API says about where a pool's queues run.
 *
 * No value promises that work on the pool runs at the same time as
 * graphics work: no API promises that.
 */
typedef uint32_t SPUDGPU_QUEUE_POOL_ENGINE;
enum {
	/** The API doesn't say. D3D12 and Metal: every pool. */
	SPUDGPU_QUEUE_POOL_ENGINE_UNKNOWN = 0,
	/** The family graphics work runs on (Vulkan: a family with
	 * VK_QUEUE_GRAPHICS_BIT). Always the pool with DIRECT in it. */
	SPUDGPU_QUEUE_POOL_ENGINE_GRAPHICS = 1,
	/** A family the driver presents as separate from graphics (Vulkan: a
	 * family without VK_QUEUE_GRAPHICS_BIT). */
	SPUDGPU_QUEUE_POOL_ENGINE_SEPARATE = 2
};

/* Frozen: one pool of queues, as SPUDGPU_ADAPTER_PROPERTIES lists them. */
typedef struct SPUDGPU_QUEUE_POOL {
	/** The kinds made from this pool; every kind is in exactly one pool. */
	SPUDGPU_QUEUE_KINDS kinds;
	/** The most queues of those kinds, together, one device may have: the
	 * desc's counts for them add up to at most this. Vulkan: the family's
	 * `queueCount`. D3D12 and Metal: UINT32_MAX. */
	uint32_t max_queues;
	/** What the API says about where this pool's queues run. */
	SPUDGPU_QUEUE_POOL_ENGINE engine;
} SPUDGPU_QUEUE_POOL;

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
	/** Names this adapter's GPU: two adapters with equal ids, from any
	 * instances in any processes, are the same GPU. The reverse does not
	 * hold: a GPU may get a new id when the system restarts, when its driver
	 * is restarted (updated, reinstalled, or reset by the OS), or when it is
	 * plugged in again. Never store one. Opaque: compare all 16 bytes.
	 * Vulkan: `VkPhysicalDeviceIDProperties::deviceUUID`. D3D12:
	 * `DXGI_ADAPTER_DESC3::AdapterLuid`, then zeros. Metal: `registryID`,
	 * then zeros. */
	uint8_t adapter_id[16];
	/** The adapter's Windows LUID, for interop that names an adapter by one
	 * (DXGI, OpenXR's D3D12 binding). Valid only when #luid_valid is true.
	 * D3D12: `AdapterLuid`. Vulkan: `deviceLUID`. Metal: never valid. */
	uint8_t luid[8];
	/** D3D12: always true. Vulkan: `deviceLUIDValid`. Metal: false. */
	bool luid_valid;
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
	/** The most a device's spudgpu_device_desc::max_descriptors may be.
	 * D3D12: 1,000,000, the shader-visible heap's limit below resource binding
	 * tier 3, kept on tier 3 too. Vulkan: UINT32_MAX (it has no device-wide
	 * limit). Metal: to be settled from its argument buffer limits when
	 * written. */
	uint32_t max_descriptors;
	/** The most a device's spudgpu_device_desc::max_samplers may be. D3D12:
	 * 2048 (`D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE`). Vulkan:
	 * UINT32_MAX. Metal: to be settled from `maxArgumentBufferSamplerCount`
	 * when written. */
	uint32_t max_samplers;

	/** The pools a device's queues come from, #queue_pool_count of them
	 * (1 to 3). Vulkan: one per family the rule in spudgpu_device_desc
	 * picks. D3D12 and Metal: three, one kind each. */
	SPUDGPU_QUEUE_POOL queue_pools[3];
	/** The number of entries of #queue_pools in use. */
	uint32_t queue_pool_count;
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
 * A GPU added after the instance was created is not listed, and one removed
 * stays listed with no device creatable on it; a new instance lists the
 * system as it is then. Listing keeps nothing on an adapter. D3D12 answers
 * an adapter's features only through a device, so on D3D12
 * spudgpu_create_instance() creates a device on each adapter in turn and
 * releases it before the next: a discrete GPU may wake briefly then.
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
 * queues are made from. D3D12 and Metal: always true. False on every backend
 * for an adapter without SPUDGPU_FEATURE_PRESENT.
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
	 * How many queues of each kind the device has. For each of the
	 * adapter's SPUDGPU_ADAPTER_PROPERTIES::queue_pools, the counts of the
	 * kinds it serves add up to at most its `max_queues`. Their sum is at
	 * least 1. Vulkan makes each kind from one queue family: direct queues
	 * from the first family with graphics; compute queues from the first
	 * family with compute and without graphics, else the direct family; copy
	 * queues from the first family with transfer only, else the compute
	 * family. That rule is what the adapter's pools report.
	 *
	 * SpudGPU sets no queue priority: Vulkan gets the same value for every
	 * queue, D3D12 `NORMAL`. The order work on different queues runs in is
	 * the driver's.
	 */
	uint32_t direct_queue_count;
	/** As #direct_queue_count, for SPUDGPU_COMMAND_LIST_TYPE_COMPUTE. */
	uint32_t compute_queue_count;
	/** As #direct_queue_count, for SPUDGPU_COMMAND_LIST_TYPE_COPY. */
	uint32_t copy_queue_count;
	/**
	 * The most descriptors other than samplers (sampled and storage images,
	 * uniform and storage buffers, texel buffer views) the device's
	 * descriptor pools and bindless table hold at once, together. At most
	 * the adapter's SPUDGPU_ADAPTER_PROPERTIES::max_descriptors. A combined
	 * image sampler counts one here and one in #max_samplers. A pool that
	 * would go past it is refused with SPUDRESULT_GPU_OUT_OF_DESCRIPTORS,
	 * on every backend. 0 is a device with no descriptors: every pool is
	 * refused. With SPUDGPU_FEATURE_BINDLESS, the table's size
	 * (SPUDGPU_BINDLESS_LIMITS, all three parts) comes out of it.
	 *
	 * D3D12: the size of the shader-visible CBV/SRV/UAV heap, made with the
	 * device. Vulkan and Metal: a count kept by the device.
	 */
	uint32_t max_descriptors;
	/** As #max_descriptors, for samplers, against
	 * SPUDGPU_ADAPTER_PROPERTIES::max_samplers. D3D12: the size of the
	 * shader-visible sampler heap. */
	uint32_t max_samplers;
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
 * The rule is one device per GPU in the process, across instances too, and
 * that part is the caller's: two instances list two adapters for one GPU,
 * which SpudGPU does not compare. On D3D12 a device made through each would
 * be one native device under two handles. Compare
 * SPUDGPU_ADAPTER_PROPERTIES::adapter_id.
 *
 * Vulkan: `vkCreateDevice` with the base set SpudGPU relies on
 * (`dynamicRendering`, `synchronization2`, `timelineSemaphore`,
 * `bufferDeviceAddress`, all required by Vulkan 1.3) and the extensions and
 * feature bits `required_features` needs, and nothing else. D3D12:
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
 * @retval SPUDRESULT_GPU_TOO_MANY_QUEUES The counts of the kinds one of
 *         the adapter's pools serves add up to more than its `max_queues`.
 * @retval SPUDRESULT_GPU_DESCRIPTOR_BUDGET_TOO_LARGE `desc->max_descriptors`
 *         or `desc->max_samplers` is above the adapter's limit.
 * @retval SPUDRESULT_GPU_OUT_OF_DESCRIPTORS SPUDGPU_FEATURE_BINDLESS is asked
 *         for and the bindless table doesn't fit in the budget.
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
 * Also the way back from SPUDRESULT_GPU_DEVICE_LOST:
 *
 * 1. Destroy everything made from the lost device, then the device.
 * 2. Create a device on the same adapter.
 * 3. If that fails, whatever the result, destroy the instance and create a
 *    new one; find the GPU there by SPUDGPU_ADAPTER_PROPERTIES::adapter_id,
 *    else by its hardware IDs, else choose again. A driver update or an
 *    unplugged GPU leaves the old adapter unusable; a hang or reset usually
 *    does not.
 *
 * @warning Every object created from the device must be destroyed first, and
 * every submission to its queues must have finished (or the device be lost).
 * No other thread may be using it. A debug build checks the first: it prints
 * the kinds of object still alive, with their counts, and asserts.
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

### Build, GPU, device: EXT macros, feature bits and properties

Settled 2026-10-21. Three questions, each with its own answer in the
header, side by side:

| | Question | Known | If no |
|---|---|---|---|
| `SPUDGPU_EXT_*` macro | Does the backend in this build implement it? | at compile time | its functions and types don't exist: a compile error |
| bit in the adapter's `supported_features` | Does this GPU support it? | when the instance lists its adapters | the bit can't be asked for |
| bit in the device's features | Did the caller ask for it? | when the device is created | the call is refused |
| property | A fact about the GPU | when the instance lists its adapters | nothing is refused; the caller reads it |

**A feature or a property.** A capability that some call refuses without it
is a feature: a `SPUDGPU_FEATURE_*` bit the caller asks for when creating the
device. A fact that makes no call refuse is a property: a field of
`SPUDGPU_ADAPTER_PROPERTIES` the caller reads. So `DEVICE_MAPPABLE_MEMORY`
and `MAPPABLE_HEAPS` are bits although no API switches them on (a buffer or
heap of that kind is refused without them), and `unified_memory` is a
property (it only says which path is faster). A limit that only matters
with a feature is a property in that feature's chained limits struct, and
counts only on a device created with the feature. What SpudGPU itself
relies on (Vulkan's base set) is neither: it is required of an adapter.
Only the capabilities SpudGPU already tracks are bits, plus PRESENT; a new
one is sorted by this rule when it is added.

**Three cases for a capability:**

- *Only the backend varies*: an EXT macro, no bit. Bundles: Metal doesn't
  implement them, and every Vulkan and D3D12 GPU has them.
- *Only the GPU varies*: a bit, no EXT macro. Timestamps, layered
  rendering, PRESENT, depth bias clamp.
- *Both vary*: an EXT macro and a bit. Bindless: Metal doesn't implement
  it, and Vulkan and D3D12 GPUs differ (D3D12 needs binding tier 3).

**How they connect in the header:**

- The feature bits are always defined, never inside an `#if`: they are
  numbers. A bit whose EXT macro is 0 is never in any adapter's
  `supported_features`, so portable caller code asking for it compiles on
  every backend and asks for nothing there.
- An EXT group's functions and its limits struct stay inside its `#if`, as
  the capability structs are now. Their `SPUD_STRUCT_TYPE` values are in
  `spudcore.h` unconditionally, as every struct type is.
- An EXT's result keeps its number and widens: `SPUDRESULT_GPU_EXT_<NAME>_NOT_SUPPORTED`
  means the device wasn't created with the feature, which covers a GPU
  that lacks it, since that one can't be asked.
- Every EXT macro's comment says what it means and what it doesn't: "1
  when the backend compiled into this build implements <name>, 0
  otherwise. It says nothing about any GPU: whether one has it is
  SPUDGPU_FEATURE_<NAME> in its adapter's supported_features, and whether
  a device may use it is that bit in the device's features." (Every EXT
  macro, SpudGPU's and the other modules', gets its own version of the
  first sentence; the second only where a feature bit pairs with it.) The
  `EXT` prefix stays: it is the convention across SpudLib, and callers'
  own rules may name it.
- **Bindless is renamed so one name runs through it.** "Descriptor
  indexing" is Vulkan's word (D3D12 says binding tier 3, Metal argument
  buffers), and the bit and limits say "bindless":
  `SPUDGPU_EXT_BINDLESS_DESCRIPTOR_INDEXING` becomes `SPUDGPU_EXT_BINDLESS`,
  and `SPUDRESULT_GPU_EXT_BINDLESS_DESCRIPTOR_INDEXING_NOT_SUPPORTED`
  becomes `SPUDRESULT_GPU_EXT_BINDLESS_NOT_SUPPORTED`, same value (325).
  Mesh shading and the depth bounds test already share one name across
  macro, bit and result.
- `SPUDGPU_EXT_MESH_SHADING` and `SPUDGPU_EXT_DEPTH_BOUNDS_TEST` are 1 on
  every backend, so they gate nothing today; both are capabilities of the
  second case in practice. They stay, as markers for a backend that may
  lack them (an OpenGL one, see `CLAUDE.md`); dropping them would be its
  own cleanup.

**The four cases, as a caller writes them:**

1. Only the backend varies (bundles): a compile-time choice, nothing asked
   for.

   ```c
   #if SPUDGPU_EXT_BUNDLES
   	spudgpu_cmd_execute_bundle(cmd, static_scene_bundle);   /* recorded once */
   #else
   	record_static_scene(cmd);                               /* recorded every frame */
   #endif
   ```

2. Only the GPU varies (timestamps): no `#if`; asked for, then checked.

   ```c
   SPUDGPU_ADAPTER_PROPERTIES props = {.base.type = SPUD_STRUCT_TYPE_GPU_ADAPTER_PROPERTIES};
   spudgpu_get_adapter_properties(adapter, &props);

   spudgpu_device_desc desc = {.base.type = SPUD_STRUCT_TYPE_GPU_DEVICE_DESC};
   desc.required_features = props.supported_features & SPUDGPU_FEATURE_PASS_TIMESTAMPS;
   /* ... queues, budget ... */
   spudgpu_create_device(adapter, &desc, &device);

   if (spudgpu_get_device_features(device) & SPUDGPU_FEATURE_PASS_TIMESTAMPS)
   	spudgpu_create_timestamp_set(device, queue, 2 * pass_count, &timestamps);
   else
   	timestamps = NULL;   /* "not available" */
   ```

   Without the bit asked for, `spudgpu_create_timestamp_set` is refused even
   on a GPU that has timestamps: a forgotten request fails on the machine it
   was written on, not on a user's GPU that lacks the feature.

3. Both vary (bindless): asking compiles everywhere; using it is inside the
   `#if`.

   ```c
   /* On Metal, supported_features never has the bit: this asks for nothing. */
   desc.required_features = props.supported_features & SPUDGPU_FEATURE_BINDLESS;
   spudgpu_create_device(adapter, &desc, &device);

   bool bindless = false;
   #if SPUDGPU_EXT_BINDLESS
   bindless = (spudgpu_get_device_features(device) & SPUDGPU_FEATURE_BINDLESS) != 0;
   #endif

   if (bindless) {
   #if SPUDGPU_EXT_BINDLESS
   	uint32_t index;
   	spudgpu_bindless_register_sampled_image(device, albedo_view, &index);
   	material.albedo_index = index;
   #endif
   } else {
   	write_material_descriptor_set(device, &material, albedo_view);   /* every backend */
   }
   ```

   Its limits are `SPUDGPU_BINDLESS_LIMITS`, chained onto the adapter
   properties, inside the same `#if`.

4. A property (`unified_memory`): no `#if`, no bit, nothing asked for.

   ```c
   SPUDGPU_ADAPTER_PROPERTIES props = {.base.type = SPUD_STRUCT_TYPE_GPU_ADAPTER_PROPERTIES};
   spudgpu_get_adapter_properties(spudgpu_get_device_adapter(device), &props);

   if (props.unified_memory)
   	write_vertices_directly(upload_buffer, vertices);           /* one copy */
   else
   	stage_then_copy(cmd, staging, device_buffer, vertices);     /* CPU, staging, GPU */
   ```

New `SPUDRESULT`s, in the unused 211-219:
`SPUDRESULT_GPU_INVALID_ADAPTER` (211), `_FEATURE_NOT_SUPPORTED` (212),
`_TOO_MANY_QUEUES` (213), `_ADAPTER_IN_USE` (214), `_DEVICE_LOST` (215,
for the step below), `_OUT_OF_DESCRIPTORS` (216: a pool, or the bindless
table, past the device's budget, on every backend) and
`_DESCRIPTOR_BUDGET_TOO_LARGE` (217). New `SPUD_STRUCT_TYPE`s:
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
| Vulkan | `VkPhysicalDevice`, properties and features read at instance creation | `vkCreateDevice`: the queue families the desc's counts need, the base set, and only the extensions and feature bits `required_features` names (`VK_KHR_swapchain` for PRESENT, the descriptor-indexing bits for BINDLESS); then the queues, and the bindless table if asked | each bit maps to the `VkPhysicalDeviceFeatures*` member or extension it needs today |
| D3D12 | `IDXGIAdapter4`, with `DXGI_ADAPTER_DESC3` and the `CheckFeatureSupport` answers read through a probe device, released before the next adapter's | `D3D12CreateDevice` at 12_0, the queues, the debug callback, the shader-visible heaps sized by the budget, the tightly packed indirect signatures, and the bindless range if asked | nothing to switch on; the device checks its bits |
| Metal | `id<MTLDevice>` from `MTLCopyAllDevices` | the queues, and the bindless argument-buffer state if asked | nothing to switch on; the device checks its bits |

#### D3D12's probe device

Settled 2026-10-21. `DXGI_ADAPTER_DESC3` gives the name, IDs, memory sizes,
LUID and the software flag with no device, but every answer that becomes
`supported_features` (`OPTIONS`, `OPTIONS2`, `OPTIONS7`, the resource
binding tier, `ARCHITECTURE1`) is a `CheckFeatureSupport` call, a device
method; no device-free source has them. Probing on the first properties
call instead would bring back lazy state and a lock and save nothing, since
a caller choosing a GPU asks about all of them; keeping the probe for
`spudgpu_create_device` would hold a device on every adapter, the first
complaint above. So:

1. `spudgpu_create_instance`, per adapter in `EnumAdapters1` order:
   `GetDesc3`; `D3D12CreateDevice` at 12_0 into a local `ComPtr` (failure:
   the adapter isn't listed); the `CheckFeatureSupport` calls; release.
   One probe at a time, never all at once. The separate
   `D3D12CreateDevice(..., nullptr)` test goes: the probe answers it.
2. The probe is fully released before `spudgpu_create_device` can run.
   D3D12 hands back the one live device per adapter, so a probe still held
   would become the caller's device, with what it carried.
3. The debug callback is registered by `spudgpu_create_device` only, never
   on the probe.
4. The debug layer is switched on before the first probe, in
   `spudgpu_create_instance`, where it is now (until the later item on
   validation moves the choice to the caller).
5. Software adapters (WARP) are listed, as SPUDGPU_ADAPTER_TYPE_SOFTWARE.
6. The header says what listing costs on D3D12 (in
   spudgpu_enumerate_adapters()'s comment above).

A device made twice on the adapter the caller uses is the price, once per
run or per recovery. Measure instance creation on the Iris Xe and the RTX
A2000 when the D3D12 work is done.

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

- **`spudgpu_get_graphics_queue`.** Sixteen caller files use it. Removing it
  is the consistent answer (it is `(DIRECT, 0)`), but it could stay as a
  one-line wrapper for one release.
- **A migration shim.** A `spudgpu_enumerate_devices` kept for a while, that
  creates a device on adapter 0 with every supported feature, would let the
  callers move one at a time. Proposed: no shim; change the callers in the
  same change, as the version policy allows before 1.0.0.

## Later, not in this change

- **A polled "the adapters changed" call.** Recovery by a new instance is
  the common practice (WebGPU, DXGI's guidance, Chromium) and most engines
  stop there. Notification is for a caller that wants a GPU attached while
  it runs, or must let go of one being ejected. The shape, so it is decided:

  ```c
  /* Whether the system's GPUs differ from those this instance listed when
   * it was created. True changes nothing in the instance: the caller makes
   * a new one when it chooses, and finds what changed by adapter_id. */
  SPUDRESULT spudgpu_instance_adapters_changed(spudgpu_instance instance,
                                               bool *out_changed);
  ```

  D3D12: `IDXGIFactory1::IsCurrent()`. Metal: an atomic flag the
  `MTLCopyAllDevicesWithObserver` block sets (Metal runs it on its own
  queue; SpudGPU starts no thread). Vulkan has nothing; the OS does: a
  udev monitor on Linux, `CM_Register_Notification` with
  `GUID_DEVINTERFACE_DISPLAY_ADAPTER` on Windows, read at each call, as
  SpudInput reads inotify at each poll. A waitable form (an fd or event for
  a wait set) only if a caller needs it. Build it when a caller asks.
- **Queue priority, an optional feature bit.** Vulkan (a relative float
  per queue, at least two distinct levels) and D3D12 (`NORMAL`, `HIGH`)
  have a priority within the device, both hints; Metal has none. So it is
  a capability, not a rule: `SPUDGPU_FEATURE_QUEUE_PRIORITY`, supported on
  Vulkan and D3D12 and never on Metal, and with it a struct chained onto
  `spudgpu_device_desc` giving each queue `NORMAL` or `HIGH` (two levels,
  what every API with priorities guarantees). The chained struct without
  the bit is refused. A bit and not an `SPUDGPU_EXT_*` macro: it is a desc
  field, and a public struct holds no `#if`. Priority against other
  processes (`VK_KHR_global_priority`, D3D12's `GLOBAL_REALTIME`), which
  may need privileges, is a second bit with its own refusal for them. Build
  when a caller asks; one whose uploads on a copy queue shouldn't slow its
  frame is the likely first.
- **Validation is the caller's choice.** Both backends switch it on by
  themselves in a debug build: the D3D12 debug layer
  (`spudgpud3d12context.cpp`, `EnableDebugLayer` in
  `spudgpu_create_instance`) and `VK_LAYER_KHRONOS_validation`
  (`spudgpuvulkancontext.c`). That is a decision made for the caller, and on
  D3D12 a process-wide one: switching the layer on removes every D3D12
  device the process already has. It becomes a field of
  `spudgpu_instance_desc` (off, on, on with GPU-based validation), with no
  default, in every build. Its own change.

## Breaks

Every caller: listing devices and taking the graphics queue both change,
and so does reading properties and capabilities, and the Vulkan and D3D12
natives that take a device. Every caller that presents asks for
SPUDGPU_FEATURE_PRESENT, and every caller sets a descriptor budget. A
shader outside bindless that uses `nonuniformEXT` is outside Vulkan's rules
once the descriptor-indexing bits are off. The callers change in the same
change and run their tests (`../SPUDGPU_DEVICE_MIGRATION.md`). All three
backends change: build `macos-metal`, `linux-vulkan` and `windows-d3d12`.

## Done when

The steps every item of `MATURITY_TODO.md` ends with:

- build on macOS and run the tests, plain and under the `-asan` and `-tsan`
  presets;
- build every caller the change breaks, in the same change, and run its
  tests, on all three backends (`../SPUDGPU_DEVICE_MIGRATION.md` lists
  them);
- `./check.sh` from the workspace;
- after the spudlib commit is pushed, `./check.sh --update-locks`, and commit
  the locks with the repositories that use them;
- each changed repository's `.git/COMMIT_DRAFT` rewritten to describe its
  change;
- `CLAUDE.md`: the threading entry under "Known gaps" shortened (the device
  array and the lazy bindless table are gone); the "lazily created member"
  exception names `_rtv_heap`, `_dsv_heap` and the indirect signatures of
  other strides only; the known gap on D3D12's shader-visible heaps says
  they are made with the device, sized by the caller's budget, and that a
  pool past it is refused on every backend;
- `CLAUDE.md`, SpudGPU conventions: the `SPUDGPU_EXT_*` paragraph says an
  EXT pairs with a `SPUDGPU_FEATURE_*` bit where support varies by GPU,
  and its result is returned when the device wasn't created with the bit;
  the mesh shading paragraph and the `SPUDGPU_LEGACY_*` paragraph point to
  the feature bits instead of `spudgpu_get_mesh_shading_capabilities` and
  `spudgpu_bindless_capabilities::supported`; the bindless names follow the
  rename.
