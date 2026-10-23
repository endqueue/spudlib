#ifndef SPUDGPUSHARED_H
#define SPUDGPUSHARED_H

/* Internal header - never included outside spudlib/src/gpu/. What every
 * backend shares: format facts the public header doesn't expose, and the
 * checks of a call that are the same on every backend, written once so that
 * all three return the same result for the same condition. */

#include "spudgpu.h"
#include "../spudthread.h"

#if __cplusplus
extern "C" {
#endif

/*
 * The checks every backend's create makes of its desc, in this order, before
 * anything else: SPUDRESULT_NULL_DESC, the struct base's,
 * SPUDRESULT_NULL_OUTPUT_PARAMETER (and *out set to NULL from there on), then
 * the allocator's (SPUDRESULT_NULL_ALLOCATOR, SPUDRESULT_INVALID_ALLOCATOR).
 */
SPUDRESULT spudgpu___internal_check_instance_desc(
    const spudgpu_instance_desc *desc,
    spudgpu_instance *out_instance);

/* The size of one texel block of [fmt] in bytes: a texel for an uncompressed
 * format, a 4x4 block for a block-compressed one (BC1-BC7). 0 for a format
 * whose texels aren't a whole number of bytes or have no known size. */
uint32_t spudgpu___internal_format_block_bytes(SPUDGPU_FORMAT fmt);

/* The checks of spudgpu_create_shader_pipeline() on its color attachments
 * and its depth bias, in this order, after the shader stage checks:
 * SPUDRESULT_GPU_TOO_MANY_COLOR_ATTACHMENTS,
 * SPUDRESULT_GPU_DEPTH_BIAS_CLAMP_NOT_SUPPORTED,
 * SPUDRESULT_GPU_INDEPENDENT_BLEND_NOT_SUPPORTED. [features] are the
 * device's. */
SPUDRESULT spudgpu___internal_check_pipeline_outputs(
    const spudgpu_shader_pipeline_desc *desc,
    SPUDGPU_FEATURES features);

/* Whether blend attachment [a] and [b] blend the same way. Two that don't
 * blend are the same whatever their factors. */
bool spudgpu___internal_blend_attachments_equal(
    const spudgpu_blend_attachment_desc *a,
    const spudgpu_blend_attachment_desc *b);

/* ---------------------------------------------------------------------------
 * Adapters and devices
 * ------------------------------------------------------------------------ */

/* What every backend keeps of an adapter: its properties as
 * spudgpu_get_adapter_properties() hands them out (base set to its own type,
 * no chain), and the limits of the chained structs, which this build may not
 * declare. Filled in once, when the instance is created, and never changed,
 * so it is read from any thread with no lock. */
typedef struct spudgpu___adapter_facts {
	SPUDGPU_ADAPTER_PROPERTIES properties;
	uint32_t mesh_max_output_vertices;
	uint32_t mesh_max_output_primitives;
	uint32_t mesh_max_workgroup_invocations;
	uint32_t bindless_max_sampled_images;
	uint32_t bindless_max_storage_images;
	uint32_t bindless_max_storage_buffers;
} spudgpu___adapter_facts;

/* spudgpu_get_adapter_properties() after its adapter check, the same on
 * every backend: SPUDRESULT_NULL_OUTPUT_PARAMETER, the base's type, then the
 * chain (SPUDRESULT_STRUCT_UNKNOWN_IN_CHAIN, _DUPLICATE_IN_CHAIN), all before
 * anything is written; then every struct of the chain is filled in. */
SPUDRESULT spudgpu___internal_get_adapter_properties(
    const spudgpu___adapter_facts *facts,
    SPUDGPU_ADAPTER_PROPERTIES *out_properties);

/* The checks of spudgpu_create_device() after its adapter check, in the
 * header's order: SPUDRESULT_NULL_DESC, the base's,
 * SPUDRESULT_NULL_OUTPUT_PARAMETER (and *out_device NULL from there on),
 * SPUDRESULT_GPU_FEATURE_NOT_SUPPORTED, SPUDRESULT_ZERO_SIZE,
 * SPUDRESULT_GPU_TOO_MANY_QUEUES, SPUDRESULT_GPU_DESCRIPTOR_BUDGET_TOO_LARGE,
 * SPUDRESULT_GPU_OUT_OF_DESCRIPTORS (the bindless table past the budget).
 * The backend then claims the adapter (SPUDRESULT_GPU_ADAPTER_IN_USE). */
SPUDRESULT spudgpu___internal_check_device_desc(
    const spudgpu___adapter_facts *facts,
    const spudgpu_device_desc *desc,
    spudgpu_device *out_device);

/* ---------------------------------------------------------------------------
 * Swap chains
 * ------------------------------------------------------------------------ */

/* The checks of spudgpu_create_swap_chain() after its device check, in the
 * header's order, none of which reads the surface: SPUDRESULT_NULL_DESC, the
 * base's, SPUDRESULT_NULL_OUTPUT_PARAMETER, SPUDRESULT_GPU_INVALID_SURFACE,
 * SPUDRESULT_GPU_INVALID_COMMAND_QUEUE, SPUDRESULT_GPU_FEATURE_NOT_SUPPORTED
 * (no SPUDGPU_FEATURE_PRESENT in the device's [features]),
 * SPUDRESULT_GPU_INVALID_PRESENT_SCALING. The backend's own checks follow,
 * then SPUDRESULT_GPU_PRESENT_SCALING_NOT_SUPPORTED, the first that reads
 * the surface. */
SPUDRESULT spudgpu___internal_check_swap_chain_desc(
    SPUDGPU_FEATURES features,
    const spudgpu_swap_chain_desc *desc,
    const spudgpu_swap_chain *out_swap_chain);

/* The checks of spudgpu_get_surface_present_scalings() after its device
 * check: SPUDRESULT_GPU_INVALID_SURFACE, SPUDRESULT_NULL_OUTPUT_PARAMETER,
 * SPUDRESULT_GPU_FEATURE_NOT_SUPPORTED. */
SPUDRESULT spudgpu___internal_check_surface_present_scalings(
    SPUDGPU_FEATURES features,
    const void *surface,
    const uint32_t *out_scalings);

/* How many queues of [type] [desc] asks for; 0 for BUNDLE or a value that
 * is not a type. */
uint32_t spudgpu___internal_queue_count(
    const spudgpu_device_desc *desc,
    SPUDGPU_COMMAND_LIST_TYPE type);

/* The descriptors and samplers a pool of [desc] takes out of its device's
 * budget (spudgpu_device_desc::max_descriptors). [desc] has passed the
 * pool's own checks. */
void spudgpu___internal_pool_descriptor_counts(
    const spudgpu_descriptor_pool_desc *desc,
    uint64_t *out_descriptors,
    uint64_t *out_samplers);

/* A device's descriptor budget, as Vulkan and Metal keep it (D3D12 keeps it
 * as its shader-visible heaps). Pools are made and destroyed on any thread,
 * and the budget is the device's, not the caller's, so it has its own lock.
 * Lives where it is never copied. */
typedef struct spudgpu___descriptor_budget {
	spud_mutex lock;
	uint64_t max_descriptors;
	uint64_t max_samplers;
	uint64_t used_descriptors;
	uint64_t used_samplers;
} spudgpu___descriptor_budget;

/* False when the lock can't be made (SPUDRESULT_GENERAL_FAILURE). */
bool spudgpu___internal_budget_init(
    spudgpu___descriptor_budget *budget,
    uint32_t max_descriptors,
    uint32_t max_samplers);
void spudgpu___internal_budget_destroy(spudgpu___descriptor_budget *budget);
/* Takes both or neither: SPUDRESULT_GPU_OUT_OF_DESCRIPTORS when either
 * doesn't fit in what is left. */
SPUDRESULT spudgpu___internal_budget_take(
    spudgpu___descriptor_budget *budget,
    uint64_t descriptors,
    uint64_t samplers);
void spudgpu___internal_budget_give_back(
    spudgpu___descriptor_budget *budget,
    uint64_t descriptors,
    uint64_t samplers);

/* ---------------------------------------------------------------------------
 * Live objects (debug builds)
 *
 * A debug build counts, per device, the objects of each kind made from it
 * and not yet destroyed, and spudgpu_destroy_device() reports any left and
 * asserts. Release builds have no counter: the macros below compile to
 * nothing there, and the counter members are under #if _DEBUG.
 * ------------------------------------------------------------------------ */

typedef enum SPUDGPU___OBJECT_KIND {
	SPUDGPU___OBJECT_BUFFER = 0,
	SPUDGPU___OBJECT_BUFFER_VIEW,
	SPUDGPU___OBJECT_IMAGE,
	SPUDGPU___OBJECT_IMAGE_VIEW,
	SPUDGPU___OBJECT_SAMPLER,
	SPUDGPU___OBJECT_HEAP,
	SPUDGPU___OBJECT_SHADER_MODULE,
	SPUDGPU___OBJECT_SHADER_PIPELINE,
	SPUDGPU___OBJECT_COMPUTE_PIPELINE,
	SPUDGPU___OBJECT_DESCRIPTOR_SET_LAYOUT,
	SPUDGPU___OBJECT_DESCRIPTOR_POOL,
	SPUDGPU___OBJECT_COMMAND_ALLOCATOR,
	SPUDGPU___OBJECT_COMMAND_LIST,
	SPUDGPU___OBJECT_FENCE,
	SPUDGPU___OBJECT_SEMAPHORE,
	SPUDGPU___OBJECT_SWAP_CHAIN,
	SPUDGPU___OBJECT_TIMESTAMP_SET,
	SPUDGPU___OBJECT_KIND_COUNT
} SPUDGPU___OBJECT_KIND;

#if _DEBUG
typedef struct spudgpu___live_objects {
	spud_mutex lock;
	uint64_t counts[SPUDGPU___OBJECT_KIND_COUNT];
} spudgpu___live_objects;

bool spudgpu___internal_live_init(spudgpu___live_objects *live);
void spudgpu___internal_live_destroy(spudgpu___live_objects *live);
void spudgpu___internal_live_add(spudgpu___live_objects *live, SPUDGPU___OBJECT_KIND kind);
void spudgpu___internal_live_remove(spudgpu___live_objects *live, SPUDGPU___OBJECT_KIND kind);
/* Prints every kind still alive on the device named [device_name] (or
 * unnamed) and asserts none is. */
void spudgpu___internal_live_check_none(spudgpu___live_objects *live, const char *device_name);

#define SPUDGPU___LIVE_ADD(live, kind) spudgpu___internal_live_add((live), (kind))
#define SPUDGPU___LIVE_REMOVE(live, kind) spudgpu___internal_live_remove((live), (kind))
#else
#define SPUDGPU___LIVE_ADD(live, kind) ((void)0)
#define SPUDGPU___LIVE_REMOVE(live, kind) ((void)0)
#endif

#if __cplusplus
}
#endif

#endif // SPUDGPUSHARED_H
