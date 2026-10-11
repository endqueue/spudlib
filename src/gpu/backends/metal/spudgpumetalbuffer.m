//
// SpudGPU Metal backend - buffers and buffer views.
// Implemented and verified against real Apple hardware.
//

#if SPUDGPU_COMPILE_METAL_API

#include "spudgpu.h"
#include "spudcore.h"
#include "spudgpumetal.h"
#include <Metal/Metal.h>

// The storage a buffer of [kind] gets: what each SPUDGPU_MEMORY_KIND
// documents in spudgpu.h. Only two storage modes are used - Private for the
// one kind that is never mapped, Shared for the rest - and the CPU cache mode
// separates the kind the CPU only writes from the kinds it reads.
// MTLStorageModeManaged is not used: it keeps a second copy to synchronize,
// which no kind asks for. spudgpu_create_buffer has already refused a value
// that is not a kind.
static MTLResourceOptions spudgpumetal___internal_buffer_resource_options(SPUDGPU_MEMORY_KIND memory_kind) {
	switch (memory_kind) {
	case SPUDGPU_MEMORY_KIND_UPLOAD:
		return MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined;
	case SPUDGPU_MEMORY_KIND_READBACK:
	case SPUDGPU_MEMORY_KIND_DEVICE_MAPPABLE:
		return MTLResourceStorageModeShared | MTLResourceCPUCacheModeDefaultCache;
	default:
		return MTLResourceStorageModePrivate;
	}
}

SPUDRESULT spudgpu_create_buffer(
    spudgpu_device device,
    const spudgpu_buffer_desc *desc,
    spudgpu_buffer *out_buffer) {
	if (!device)
		return SPUDRESULT_GPU_INVALID_DEVICE;
	if (!desc)
		return SPUDRESULT_NULL_DESC;
	if (!out_buffer)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	if (desc->size == 0)
		return SPUDRESULT_GPU_ZERO_BUFFER_SIZE;
	if (desc->usage == SPUDGPU_BUFFER_USAGE_NONE)
		return SPUDRESULT_GPU_INVALID_BUFFER_USAGE;

	spudgpu_device_metal *device_metal = (spudgpu_device_metal *)device;
	SPUDRESULT sr                      = SPUD_SUCCESS;

	// SPUDGPU_MEMORY_KIND's contract (see spudgpu.h), the same checks in the
	// same order on every backend.
	if (desc->memory_kind < SPUDGPU_MEMORY_KIND_DEVICE || desc->memory_kind > SPUDGPU_MEMORY_KIND_DEVICE_MAPPABLE)
		return SPUDRESULT_GPU_INVALID_MEMORY_KIND;
	if (desc->memory_kind == SPUDGPU_MEMORY_KIND_UPLOAD &&
	    (desc->usage & (SPUDGPU_BUFFER_USAGE_STORAGE | SPUDGPU_BUFFER_USAGE_TRANSFER_DST |
	                    SPUDGPU_BUFFER_USAGE_RAYTRACING_ACCELERATION_STRUCTURE)))
		return SPUDRESULT_GPU_INVALID_BUFFER_USAGE;
	if (desc->memory_kind == SPUDGPU_MEMORY_KIND_READBACK && desc->usage != SPUDGPU_BUFFER_USAGE_TRANSFER_DST)
		return SPUDRESULT_GPU_INVALID_BUFFER_USAGE;
	if (desc->memory_kind == SPUDGPU_MEMORY_KIND_DEVICE_MAPPABLE && !device_metal->_properties.device_mappable_memory)
		return SPUDRESULT_GPU_MEMORY_KIND_NOT_SUPPORTED;

	spudgpu_buffer_metal *buffer_metal = (spudgpu_buffer_metal *)calloc(1, sizeof(spudgpu_buffer_metal));
	if (!buffer_metal) {
		sr = SPUDRESULT_GENERAL_FAILURE;
		goto failedattempt;
	}
	buffer_metal->_parent_device = device_metal;
	buffer_metal->_desc          = *desc;

	buffer_metal->_buffer_mtl = [device_metal->_device_mtl
	    newBufferWithLength:desc->size
	                 options:spudgpumetal___internal_buffer_resource_options(desc->memory_kind)];
	if (!buffer_metal->_buffer_mtl) {
		sr = SPUDRESULT_API_SPECIFIC_FAILURE;
		goto failedattempt;
	}

	// Every spudgpu_buffer gets a real GPU virtual address after creation
	// regardless of backend (see spudgpu_buffer_desc::gpu_address_location),
	// mirroring D3D12's always-available GetGPUVirtualAddress - Metal's
	// equivalent is -[MTLBuffer gpuAddress], valid for any storage mode.
	buffer_metal->_desc.gpu_address_location = buffer_metal->_buffer_mtl.gpuAddress;

#if _DEBUG
	if (spud_debug_name_set(buffer_metal, desc->debug_name) != SPUD_SUCCESS) {
		spudgpu_destroy_buffer((spudgpu_buffer)buffer_metal);
		return SPUDRESULT_OUT_OF_MEMORY;
	}
	buffer_metal->_desc.debug_name = buffer_metal->_debug_name;
#endif

	*out_buffer = (spudgpu_buffer)buffer_metal;

	return sr;

failedattempt:
	free(buffer_metal);
	return sr;
}
void spudgpu_destroy_buffer(spudgpu_buffer buffer) {
	spudgpu_buffer_metal *buffer_metal = (spudgpu_buffer_metal *)buffer;
	if (buffer_metal) {
		if (buffer_metal->_buffer_mtl) {
			[buffer_metal->_buffer_mtl release];
		}
#if _DEBUG
		free((void *)buffer_metal->_debug_name);
#endif
		free(buffer_metal);
	}
}
SPUDRESULT spudgpu_get_buffer_desc(
    spudgpu_buffer buffer,
    spudgpu_buffer_desc *out_desc) {
	spudgpu_buffer_metal *buffer_metal = (spudgpu_buffer_metal *)buffer;
	if (!buffer_metal)
		return SPUDRESULT_GPU_INVALID_BUFFER;
	if (!out_desc)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	*out_desc = buffer_metal->_desc;
	return SPUD_SUCCESS;
}

SPUDRESULT spudgpu_create_buffer_view(
    spudgpu_buffer buffer,
    const spudgpu_buffer_view_desc *desc,
    spudgpu_buffer_view *out_buffer_view) {
	if (!buffer)
		return SPUDRESULT_GPU_INVALID_BUFFER;
	if (!desc)
		return SPUDRESULT_NULL_DESC;
	if (!out_buffer_view)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	if (desc->size == 0)
		return SPUDRESULT_GPU_ZERO_BUFFER_SIZE;

	spudgpu_buffer_metal *parent_buffer_metal = (spudgpu_buffer_metal *)buffer;
	if (desc->offset_from_parent_buffer + desc->size > parent_buffer_metal->_desc.size)
		return SPUDRESULT_GPU_BUFFER_OR_IMAGE_VIEW_RANGE_OUT_OF_SCOPE;

	SPUDRESULT sr = SPUD_SUCCESS;

	spudgpu_buffer_view_metal *result = (spudgpu_buffer_view_metal *)calloc(1, sizeof(spudgpu_buffer_view_metal));
	if (!result) {
		sr = SPUDRESULT_GENERAL_FAILURE;
		goto failedattempt;
	}
	result->_desc               = *desc;
	result->_desc.parent_buffer = buffer; // Authoritative - see spudgpu_buffer_view_metal.

	*out_buffer_view = (spudgpu_buffer_view)result;

	return sr;

failedattempt:
	return sr;
}

void spudgpu_destroy_buffer_view(spudgpu_buffer_view buffer_view) {
	if (!buffer_view)
		return;
#if _DEBUG
	free((void *)buffer_view->_debug_name);
#endif
	free(buffer_view);
}
SPUDRESULT spudgpu_get_buffer_view_desc(
    spudgpu_buffer_view buffer_view,
    spudgpu_buffer_view_desc *out_desc) {
	if (!buffer_view)
		return SPUDRESULT_GPU_INVALID_BUFFER_VIEW;
	if (!out_desc)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	*out_desc = ((spudgpu_buffer_view_metal *)buffer_view)->_desc;
	return SPUD_SUCCESS;
}

SPUDRESULT spudgpu_map_buffer(
    spudgpu_buffer buffer,
    uint64_t offset,
    uint64_t size,
    void **ppData) {
	if (!buffer)
		return SPUDRESULT_GPU_INVALID_BUFFER;

	spudgpu_buffer_metal *buffer_metal = (spudgpu_buffer_metal *)buffer;
	if (offset + size > buffer_metal->_desc.size)
		return SPUDRESULT_GPU_MAP_OUT_OF_RANGE;
	if (!ppData)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;

	// Only a Shared-storage buffer has a CPU-visible pointer at all, which
	// is every kind but DEVICE - see
	// spudgpumetal___internal_buffer_resource_options.
	if (buffer_metal->_desc.memory_kind == SPUDGPU_MEMORY_KIND_DEVICE)
		return SPUDRESULT_GPU_INVALID_MEMORY_KIND;

	uint64_t map_size = (size == 0) ? buffer_metal->_desc.size : size;
	if (offset + map_size > buffer_metal->_desc.size)
		return SPUDRESULT_GPU_MAP_OUT_OF_RANGE;

	// Metal has no map/unmap operation to perform - a Shared-storage
	// buffer's CPU pointer is always available via -contents; "mapping" is
	// just handing back contents + offset.
	*ppData = (uint8_t *)buffer_metal->_buffer_mtl.contents + offset;

	return SPUD_SUCCESS;
}

void spudgpu_unmap_buffer(spudgpu_buffer buffer) {
	// Nothing to unmap - see spudgpu_map_buffer.
	if (!buffer)
		return;
}

// A Shared-storage MTLBuffer is one piece of memory the CPU and the GPU both
// see, so there is nothing to flush out of or invalidate - only a Managed
// buffer has a second copy, and no kind uses one (see
// spudgpumetal___internal_buffer_resource_options). Both calls make the
// checks every backend makes and then have nothing to do.
SPUDRESULT spudgpu_flush_buffer(
    spudgpu_buffer buffer,
    uint64_t offset,
    uint64_t size) {
	if (!buffer)
		return SPUDRESULT_GPU_INVALID_BUFFER;

	spudgpu_buffer_metal *buffer_metal = (spudgpu_buffer_metal *)buffer;
	if (buffer_metal->_desc.memory_kind != SPUDGPU_MEMORY_KIND_UPLOAD &&
	    buffer_metal->_desc.memory_kind != SPUDGPU_MEMORY_KIND_DEVICE_MAPPABLE)
		return SPUDRESULT_GPU_INVALID_MEMORY_KIND;
	if (offset + size > buffer_metal->_desc.size)
		return SPUDRESULT_GPU_MAP_OUT_OF_RANGE;
	return SPUD_SUCCESS;
}

SPUDRESULT spudgpu_invalidate_buffer(
    spudgpu_buffer buffer,
    uint64_t offset,
    uint64_t size) {
	if (!buffer)
		return SPUDRESULT_GPU_INVALID_BUFFER;

	spudgpu_buffer_metal *buffer_metal = (spudgpu_buffer_metal *)buffer;
	if (buffer_metal->_desc.memory_kind != SPUDGPU_MEMORY_KIND_READBACK &&
	    buffer_metal->_desc.memory_kind != SPUDGPU_MEMORY_KIND_DEVICE_MAPPABLE)
		return SPUDRESULT_GPU_INVALID_MEMORY_KIND;
	if (offset + size > buffer_metal->_desc.size)
		return SPUDRESULT_GPU_MAP_OUT_OF_RANGE;
	return SPUD_SUCCESS;
}


#endif // SPUDGPU_COMPILE_METAL_API
