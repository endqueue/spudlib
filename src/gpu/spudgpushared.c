#include "spudgpu.h"
#include "spudgpushared.h"
#include "../spudallocator.h"
#include "../spudstructbase.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#if __cplusplus
extern "C" {
#endif

SPUDRESULT spudgpu___internal_check_instance_desc(
    const spudgpu_instance_desc *desc,
    spudgpu_instance *out_instance) {
	if (!desc)
		return SPUDRESULT_NULL_DESC;
	const SPUDRESULT base_result = spud_struct_check(&desc->base, SPUD_STRUCT_TYPE_GPU_INSTANCE_DESC);
	if (base_result != SPUD_SUCCESS)
		return base_result;
	if (!out_instance)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	*out_instance = NULL;
	return spud_allocator_check(desc->allocator);
}

bool spudgpu___internal_blend_attachments_equal(
    const spudgpu_blend_attachment_desc *a,
    const spudgpu_blend_attachment_desc *b) {
	if (a->blend_enable != b->blend_enable)
		return false;
	if (!a->blend_enable)
		return true;
	return a->src_color_blend_factor == b->src_color_blend_factor && a->dst_color_blend_factor == b->dst_color_blend_factor &&
	       a->color_blend_op == b->color_blend_op && a->src_alpha_blend_factor == b->src_alpha_blend_factor &&
	       a->dst_alpha_blend_factor == b->dst_alpha_blend_factor && a->alpha_blend_op == b->alpha_blend_op;
}

SPUDRESULT spudgpu___internal_check_pipeline_outputs(
    const spudgpu_shader_pipeline_desc *desc,
    SPUDGPU_FEATURES features) {
	if (desc->color_attachment_count > SPUDGPU_MAX_COLOR_ATTACHMENTS)
		return SPUDRESULT_GPU_TOO_MANY_COLOR_ATTACHMENTS;
	if (desc->depth_bias_clamp != 0.0f && !(features & SPUDGPU_FEATURE_DEPTH_BIAS_CLAMP))
		return SPUDRESULT_GPU_DEPTH_BIAS_CLAMP_NOT_SUPPORTED;
	if (!(features & SPUDGPU_FEATURE_INDEPENDENT_BLEND))
		for (uint32_t i = 1; i < desc->color_attachment_count; ++i)
			if (!spudgpu___internal_blend_attachments_equal(&desc->blend_attachments[0], &desc->blend_attachments[i]))
				return SPUDRESULT_GPU_INDEPENDENT_BLEND_NOT_SUPPORTED;
	return SPUD_SUCCESS;
}

// ---------------------------------------------------------------------------
// Adapters and devices
// ---------------------------------------------------------------------------

SPUDRESULT spudgpu___internal_get_adapter_properties(
    const spudgpu___adapter_facts *facts,
    SPUDGPU_ADAPTER_PROPERTIES *out_properties) {
	if (!out_properties)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	if (out_properties->base.type != SPUD_STRUCT_TYPE_GPU_ADAPTER_PROPERTIES)
		return SPUDRESULT_STRUCT_WRONG_TYPE;

	// The whole chain is checked before anything is written: a call that
	// fails leaves every struct of it as it was.
	bool seen_mesh     = false;
	bool seen_bindless = false;
	for (const SPUD_STRUCT_BASE_OUT *link = out_properties->base.next; link; link = link->next) {
		switch (link->type) {
#if SPUDGPU_EXT_MESH_SHADING
		case SPUD_STRUCT_TYPE_GPU_MESH_SHADING_LIMITS:
			if (seen_mesh)
				return SPUDRESULT_STRUCT_DUPLICATE_IN_CHAIN;
			seen_mesh = true;
			break;
#endif
#if SPUDGPU_EXT_BINDLESS
		case SPUD_STRUCT_TYPE_GPU_BINDLESS_LIMITS:
			if (seen_bindless)
				return SPUDRESULT_STRUCT_DUPLICATE_IN_CHAIN;
			seen_bindless = true;
			break;
#endif
		default:
			return SPUDRESULT_STRUCT_UNKNOWN_IN_CHAIN;
		}
	}
	(void)seen_mesh;
	(void)seen_bindless;

	// Every field but the base is SpudGPU's to write; the base, chain
	// included, stays the caller's.
	const SPUD_STRUCT_BASE_OUT base = out_properties->base;
	*out_properties                 = facts->properties;
	out_properties->base            = base;

	for (SPUD_STRUCT_BASE_OUT *link = out_properties->base.next; link; link = link->next) {
#if SPUDGPU_EXT_MESH_SHADING
		if (link->type == SPUD_STRUCT_TYPE_GPU_MESH_SHADING_LIMITS) {
			SPUDGPU_MESH_SHADING_LIMITS *mesh   = (SPUDGPU_MESH_SHADING_LIMITS *)link;
			mesh->max_mesh_output_vertices       = facts->mesh_max_output_vertices;
			mesh->max_mesh_output_primitives     = facts->mesh_max_output_primitives;
			mesh->max_mesh_workgroup_invocations = facts->mesh_max_workgroup_invocations;
		}
#endif
#if SPUDGPU_EXT_BINDLESS
		if (link->type == SPUD_STRUCT_TYPE_GPU_BINDLESS_LIMITS) {
			SPUDGPU_BINDLESS_LIMITS *bindless = (SPUDGPU_BINDLESS_LIMITS *)link;
			bindless->max_sampled_images      = facts->bindless_max_sampled_images;
			bindless->max_storage_images      = facts->bindless_max_storage_images;
			bindless->max_storage_buffers     = facts->bindless_max_storage_buffers;
		}
#endif
	}
	return SPUD_SUCCESS;
}

SPUDRESULT spudgpu___internal_check_swap_chain_desc(
    SPUDGPU_FEATURES features,
    const spudgpu_swap_chain_desc *desc,
    const spudgpu_swap_chain *out_swap_chain) {
	if (!desc)
		return SPUDRESULT_NULL_DESC;
	const SPUDRESULT base_sr = spud_struct_check(&desc->base, SPUD_STRUCT_TYPE_GPU_SWAP_CHAIN_DESC);
	if (base_sr != SPUD_SUCCESS)
		return base_sr;
	if (!out_swap_chain)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	if (!desc->surface)
		return SPUDRESULT_GPU_INVALID_SURFACE;
	if (!desc->queue)
		return SPUDRESULT_GPU_INVALID_COMMAND_QUEUE;
	if (!(features & SPUDGPU_FEATURE_PRESENT))
		return SPUDRESULT_GPU_FEATURE_NOT_SUPPORTED;
	switch (desc->present_scaling) {
	case SPUDGPU_PRESENT_SCALING_ONE_TO_ONE:
	case SPUDGPU_PRESENT_SCALING_STRETCH:
	case SPUDGPU_PRESENT_SCALING_ASPECT_FIT:
		return SPUD_SUCCESS;
	default:
		return SPUDRESULT_GPU_INVALID_PRESENT_SCALING;
	}
}

SPUDRESULT spudgpu___internal_check_surface_present_scalings(
    SPUDGPU_FEATURES features,
    const void *surface,
    const uint32_t *out_scalings) {
	if (!surface)
		return SPUDRESULT_GPU_INVALID_SURFACE;
	// The surface's struct is the backend's; its head is first in every one.
	if (SPUD_STALE_HEAD((const spud_object_head *)surface, "spudgpu_get_surface_present_scalings", "surface"))
		return SPUDRESULT_GPU_INVALID_SURFACE;
	if (!out_scalings)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	if (!(features & SPUDGPU_FEATURE_PRESENT))
		return SPUDRESULT_GPU_FEATURE_NOT_SUPPORTED;
	return SPUD_SUCCESS;
}

uint32_t spudgpu___internal_queue_count(
    const spudgpu_device_desc *desc,
    SPUDGPU_COMMAND_LIST_TYPE type) {
	switch (type) {
	case SPUDGPU_COMMAND_LIST_TYPE_DIRECT:
		return desc->direct_queue_count;
	case SPUDGPU_COMMAND_LIST_TYPE_COMPUTE:
		return desc->compute_queue_count;
	case SPUDGPU_COMMAND_LIST_TYPE_COPY:
		return desc->copy_queue_count;
	default:
		return 0;
	}
}

SPUDRESULT spudgpu___internal_check_device_desc(
    const spudgpu___adapter_facts *facts,
    const spudgpu_device_desc *desc,
    spudgpu_device *out_device) {
	if (!desc)
		return SPUDRESULT_NULL_DESC;
	const SPUDRESULT base_result = spud_struct_check(&desc->base, SPUD_STRUCT_TYPE_GPU_DEVICE_DESC);
	if (base_result != SPUD_SUCCESS)
		return base_result;
	if (!out_device)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	*out_device = NULL;

	const SPUDGPU_ADAPTER_PROPERTIES *properties = &facts->properties;
	if (desc->required_features & ~properties->supported_features)
		return SPUDRESULT_GPU_FEATURE_NOT_SUPPORTED;
	if (desc->direct_queue_count == 0 && desc->compute_queue_count == 0 && desc->copy_queue_count == 0)
		return SPUDRESULT_ZERO_SIZE;
	for (uint32_t i = 0; i < properties->queue_pool_count; ++i) {
		const SPUDGPU_QUEUE_POOL *pool = &properties->queue_pools[i];
		// 64 bits: three counts near UINT32_MAX don't wrap into a small sum.
		uint64_t asked = 0;
		if (pool->kinds & SPUDGPU_QUEUE_KIND_DIRECT)
			asked += desc->direct_queue_count;
		if (pool->kinds & SPUDGPU_QUEUE_KIND_COMPUTE)
			asked += desc->compute_queue_count;
		if (pool->kinds & SPUDGPU_QUEUE_KIND_COPY)
			asked += desc->copy_queue_count;
		if (asked > pool->max_queues)
			return SPUDRESULT_GPU_TOO_MANY_QUEUES;
	}
	if (desc->max_descriptors > properties->max_descriptors)
		return SPUDRESULT_GPU_DESCRIPTOR_BUDGET_TOO_LARGE;
	if (desc->max_samplers > properties->max_samplers)
		return SPUDRESULT_GPU_DESCRIPTOR_BUDGET_TOO_LARGE;
	if (desc->required_features & SPUDGPU_FEATURE_BINDLESS) {
		const uint64_t table = (uint64_t)facts->bindless_max_sampled_images + facts->bindless_max_storage_images +
		                       facts->bindless_max_storage_buffers;
		if (table > desc->max_descriptors)
			return SPUDRESULT_GPU_OUT_OF_DESCRIPTORS;
	}
	return SPUD_SUCCESS;
}

void spudgpu___internal_pool_descriptor_counts(
    const spudgpu_descriptor_pool_desc *desc,
    uint64_t *out_descriptors,
    uint64_t *out_samplers) {
	uint64_t descriptors = 0;
	uint64_t samplers    = 0;
	for (uint32_t i = 0; i < desc->pool_size_count && i < SPUDGPU_MAX_DESCRIPTOR_POOL_SIZES; ++i) {
		const spudgpu_descriptor_pool_size *size = &desc->pool_sizes[i];
		if (size->descriptor_type == SPUDGPU_DESCRIPTOR_TYPE_SAMPLER) {
			samplers += size->count;
		} else {
			descriptors += size->count;
			// The image's descriptor and the sampler's, which D3D12 keeps in
			// two heaps and every backend counts the same way.
			if (size->descriptor_type == SPUDGPU_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
				samplers += size->count;
		}
	}
	*out_descriptors = descriptors;
	*out_samplers    = samplers;
}

bool spudgpu___internal_budget_init(
    spudgpu___descriptor_budget *budget,
    uint32_t max_descriptors,
    uint32_t max_samplers) {
	if (!spud_mutex_init(&budget->lock))
		return false;
	budget->max_descriptors  = max_descriptors;
	budget->max_samplers     = max_samplers;
	budget->used_descriptors = 0;
	budget->used_samplers    = 0;
	return true;
}

void spudgpu___internal_budget_destroy(spudgpu___descriptor_budget *budget) {
	spud_mutex_destroy(&budget->lock);
}

SPUDRESULT spudgpu___internal_budget_take(
    spudgpu___descriptor_budget *budget,
    uint64_t descriptors,
    uint64_t samplers) {
	SPUDRESULT result = SPUD_SUCCESS;
	spud_mutex_lock(&budget->lock);
	if (descriptors > budget->max_descriptors - budget->used_descriptors ||
	    samplers > budget->max_samplers - budget->used_samplers) {
		result = SPUDRESULT_GPU_OUT_OF_DESCRIPTORS;
	} else {
		budget->used_descriptors += descriptors;
		budget->used_samplers += samplers;
	}
	spud_mutex_unlock(&budget->lock);
	return result;
}

void spudgpu___internal_budget_give_back(
    spudgpu___descriptor_budget *budget,
    uint64_t descriptors,
    uint64_t samplers) {
	spud_mutex_lock(&budget->lock);
	assert(budget->used_descriptors >= descriptors);
	assert(budget->used_samplers >= samplers);
	budget->used_descriptors -= descriptors;
	budget->used_samplers -= samplers;
	spud_mutex_unlock(&budget->lock);
}

// ---------------------------------------------------------------------------
// Live objects (debug builds)
// ---------------------------------------------------------------------------

#if _DEBUG
static const char *spudgpu___object_kind_name(SPUDGPU___OBJECT_KIND kind) {
	switch (kind) {
	case SPUDGPU___OBJECT_BUFFER:
		return "buffer";
	case SPUDGPU___OBJECT_BUFFER_VIEW:
		return "buffer view";
	case SPUDGPU___OBJECT_IMAGE:
		return "image";
	case SPUDGPU___OBJECT_IMAGE_VIEW:
		return "image view";
	case SPUDGPU___OBJECT_SAMPLER:
		return "sampler";
	case SPUDGPU___OBJECT_HEAP:
		return "heap";
	case SPUDGPU___OBJECT_SHADER_MODULE:
		return "shader module";
	case SPUDGPU___OBJECT_SHADER_PIPELINE:
		return "shader pipeline";
	case SPUDGPU___OBJECT_COMPUTE_PIPELINE:
		return "compute pipeline";
	case SPUDGPU___OBJECT_DESCRIPTOR_SET_LAYOUT:
		return "descriptor set layout";
	case SPUDGPU___OBJECT_DESCRIPTOR_POOL:
		return "descriptor pool";
	case SPUDGPU___OBJECT_COMMAND_ALLOCATOR:
		return "command allocator";
	case SPUDGPU___OBJECT_COMMAND_LIST:
		return "command list";
	case SPUDGPU___OBJECT_FENCE:
		return "fence";
	case SPUDGPU___OBJECT_SEMAPHORE:
		return "semaphore";
	case SPUDGPU___OBJECT_SWAP_CHAIN:
		return "swap chain";
	case SPUDGPU___OBJECT_TIMESTAMP_SET:
		return "timestamp set";
	default:
		return "object";
	}
}

bool spudgpu___internal_live_init(spudgpu___live_objects *live) {
	memset(live->counts, 0, sizeof(live->counts));
	return spud_mutex_init(&live->lock);
}

void spudgpu___internal_live_destroy(spudgpu___live_objects *live) {
	spud_mutex_destroy(&live->lock);
}

void spudgpu___internal_live_add(spudgpu___live_objects *live, SPUDGPU___OBJECT_KIND kind) {
	spud_mutex_lock(&live->lock);
	++live->counts[kind];
	spud_mutex_unlock(&live->lock);
}

void spudgpu___internal_live_remove(spudgpu___live_objects *live, SPUDGPU___OBJECT_KIND kind) {
	spud_mutex_lock(&live->lock);
	assert(live->counts[kind] > 0 && "spudgpu: an object was destroyed that its device never counted");
	if (live->counts[kind] > 0)
		--live->counts[kind];
	spud_mutex_unlock(&live->lock);
}

void spudgpu___internal_live_check_none(spudgpu___live_objects *live, const char *device_name) {
	spud_mutex_lock(&live->lock);
	uint64_t total = 0;
	for (int kind = 0; kind < SPUDGPU___OBJECT_KIND_COUNT; ++kind)
		total += live->counts[kind];
	if (total > 0) {
		printf("spudgpu: device \"%s\" destroyed with", device_name ? device_name : "");
		bool first = true;
		for (int kind = 0; kind < SPUDGPU___OBJECT_KIND_COUNT; ++kind) {
			if (live->counts[kind] == 0)
				continue;
			printf("%s %llu %s%s", first ? "" : ",", (unsigned long long)live->counts[kind],
			    spudgpu___object_kind_name((SPUDGPU___OBJECT_KIND)kind), live->counts[kind] == 1 ? "" : "s");
			first = false;
		}
		printf(" alive\n");
	}
	spud_mutex_unlock(&live->lock);
	assert(total == 0 && "spudgpu: a device was destroyed with objects made from it still alive");
}
#endif

#if __cplusplus
}
#endif
