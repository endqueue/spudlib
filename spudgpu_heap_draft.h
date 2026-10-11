
/*
 * DRAFT - SpudGPU heaps and the buffers and images placed in them. Designed,
 * not built: not compiled, not included anywhere, no backend has a line of
 * it. Sections move into include/spudgpu.h as each is implemented on all
 * three backends; this file shrinks until it's gone.
 *
 * Why: every spudgpu_create_buffer() and spudgpu_create_image() is a device
 * allocation of its own (vkAllocateMemory, CreateCommittedResource,
 * newBufferWithLength). A heap is one allocation, and the caller places as
 * many buffers and images in it as fit, at offsets it chooses. SpudGPU
 * reports what a resource needs (size, alignment) and creates it where it is
 * told; which heap, which offset, when a heap is made or given back are all
 * the caller's. SpudGPU keeps no record of what is placed where.
 *
 * Asked for by ApricotFields APREND_TODO.md item 35.
 *
 * Open questions for review are at the end of the file.
 */

#ifndef SPUDGPU_HEAP_DRAFT_H
#define SPUDGPU_HEAP_DRAFT_H

#include "spudgpu.h"

#if __cplusplus
extern "C" {
#endif // __cplusplus

/* ---- Results (spudcore.h, in the gap after 239) ---------------------- */

/*
 *	SPUDRESULT_GPU_INVALID_HEAP                  = 240,  NULL heap
 *	SPUDRESULT_GPU_INVALID_HEAP_RESOURCE_CLASS   = 241,  NONE or not a class
 *	SPUDRESULT_GPU_INVALID_HEAP_SIZE             = 242,  not a multiple of
 *	                                                     SPUDGPU_HEAP_SIZE_GRANULARITY
 *	SPUDRESULT_GPU_HEAP_MISMATCH                 = 243,  resource is not of the
 *	                                                     heap's kind or class
 *	SPUDRESULT_GPU_PLACEMENT_MISALIGNED          = 244,  offset not a multiple of
 *	                                                     the reported alignment
 *	SPUDRESULT_GPU_PLACEMENT_OUT_OF_RANGE        = 245,  offset + reported size
 *	                                                     runs past the heap's end
 */

/* ---- Handle ---------------------------------------------------------- */

/** An opaque handle to a heap: one device allocation that buffers and images
 *  are placed in. */
typedef struct spudgpu_heap_t *spudgpu_heap;

/* ---- Device property (added to SPUDGPU_DEVICE_PROPERTIES) ------------ */

/*
 *	/ **
 *	 * Whether a heap can be created with SPUDGPU_MEMORY_KIND_UPLOAD,
 *	 * SPUDGPU_MEMORY_KIND_READBACK or SPUDGPU_MEMORY_KIND_DEVICE_MAPPABLE.
 *	 * False means spudgpu_create_heap() returns
 *	 * SPUDRESULT_GPU_MEMORY_KIND_NOT_SUPPORTED for those kinds, and buffers
 *	 * of them are created with spudgpu_create_buffer() as before.
 *	 *
 *	 * Vulkan and D3D12: always true. Metal: true on a device in
 *	 * MTLGPUFamilyApple*, false on any other (a Mac without Apple silicon
 *	 * makes heaps of MTLStorageModePrivate only).
 *	 *
 *	 * DEVICE_MAPPABLE also needs #device_mappable_memory.
 *	 * /
 *	bool mappable_heaps;
 */

/* ---- Resource classes ------------------------------------------------ */

/**
 * @brief What a heap holds: buffers, or images that are never attachments,
 * or images that are.
 *
 * A heap holds one class and nothing else, on every backend. That is D3D12's
 * resource heap tier 1 (D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS,
 * _ALLOW_ONLY_NON_RT_DS_TEXTURES, _ALLOW_ONLY_RT_DS_TEXTURES) made the rule,
 * and it is acted on by more than one API: Vulkan picks the heap's memory
 * type from it (see "Backends" below), and with buffers and images never in
 * one heap, Vulkan's bufferImageGranularity never applies. Metal could mix
 * them and doesn't.
 *
 * The class is the image's usage, not a choice: an image with
 * SPUDGPU_IMAGE_USAGE_COLOR_ATTACHMENT or
 * SPUDGPU_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT goes in an
 * ATTACHMENT_IMAGES heap, any other in an IMAGES heap.
 */
typedef uint32_t SPUDGPU_HEAP_RESOURCE_CLASS;

enum {
	/** Not a class. spudgpu_create_heap() returns
	 *  SPUDRESULT_GPU_INVALID_HEAP_RESOURCE_CLASS: a zeroed desc has not
	 *  chosen. */
	SPUDGPU_HEAP_RESOURCE_CLASS_NONE = 0,
	/** Buffers, of the heap's memory kind. */
	SPUDGPU_HEAP_RESOURCE_CLASS_BUFFERS = 1,
	/** Images with neither attachment usage. The heap's kind is
	 *  SPUDGPU_MEMORY_KIND_DEVICE. */
	SPUDGPU_HEAP_RESOURCE_CLASS_IMAGES = 2,
	/** Images with a color or depth/stencil attachment usage. The heap's
	 *  kind is SPUDGPU_MEMORY_KIND_DEVICE. */
	SPUDGPU_HEAP_RESOURCE_CLASS_ATTACHMENT_IMAGES = 3
};

/**
 * Every heap's size is a multiple of this. It is D3D12's heap alignment
 * (D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT) made the rule; SpudGPU has no
 * multisampled images, so D3D12's 4 MiB MSAA alignment never applies.
 */
#define SPUDGPU_HEAP_SIZE_GRANULARITY 65536u

/* ---- Heaps ----------------------------------------------------------- */

/**
 * @brief Describes a heap.
 */
typedef struct spudgpu_heap_desc {
	/**
	 * The memory every resource placed in the heap is in, with the same
	 * contract as a buffer's (see SPUDGPU_MEMORY_KIND). An image heap's is
	 * SPUDGPU_MEMORY_KIND_DEVICE: images have no other kind.
	 */
	SPUDGPU_MEMORY_KIND memory_kind;

	/** What the heap holds. @see SPUDGPU_HEAP_RESOURCE_CLASS */
	SPUDGPU_HEAP_RESOURCE_CLASS resource_class;

	/** Bytes. A multiple of SPUDGPU_HEAP_SIZE_GRANULARITY, not 0. */
	uint64_t size;
#if _DEBUG
	/** @brief A string identifier used for diagnostic tracking. */
	const char *debug_name;
#endif
} spudgpu_heap_desc;

/**
 * @brief Allocates a heap.
 *
 * Its contents are undefined, as a buffer's are: placing a resource in it
 * does not clear the bytes the resource covers.
 *
 * A heap of a mappable kind is ready to have its buffers mapped from
 * creation on (see spudgpu_map_buffer() under "Placed resources").
 *
 * @param[in]  device   Device the heap is allocated on.
 * @param[in]  desc     Heap configuration.
 * @param[out] out_heap Receives the new heap on success.
 *
 * @retval SPUD_SUCCESS The heap was created.
 * @retval SPUDRESULT_GPU_INVALID_DEVICE `device` is NULL.
 * @retval SPUDRESULT_NULL_DESC `desc` is NULL.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_heap` is NULL.
 * @retval SPUDRESULT_ZERO_SIZE `desc->size` is 0.
 * @retval SPUDRESULT_GPU_INVALID_HEAP_SIZE `desc->size` is not a multiple
 *         of SPUDGPU_HEAP_SIZE_GRANULARITY.
 * @retval SPUDRESULT_GPU_INVALID_HEAP_RESOURCE_CLASS
 *         `desc->resource_class` is SPUDGPU_HEAP_RESOURCE_CLASS_NONE or not
 *         a class.
 * @retval SPUDRESULT_GPU_INVALID_MEMORY_KIND `desc->memory_kind` is
 *         SPUDGPU_MEMORY_KIND_NONE or not a kind, or the class is an image
 *         class and the kind is not SPUDGPU_MEMORY_KIND_DEVICE.
 * @retval SPUDRESULT_GPU_MEMORY_KIND_NOT_SUPPORTED The kind is mappable and
 *         the device's SPUDGPU_DEVICE_PROPERTIES::mappable_heaps is false,
 *         or it is SPUDGPU_MEMORY_KIND_DEVICE_MAPPABLE and
 *         SPUDGPU_DEVICE_PROPERTIES::device_mappable_memory is false.
 * @return Another SPUDRESULT if the backend fails, such as
 *         SPUDRESULT_OUT_OF_MEMORY or SPUDRESULT_API_SPECIFIC_FAILURE (the
 *         device has no memory left for a heap that size is the usual one).
 *
 * The checks run in the order listed.
 *
 * @see spudgpu_destroy_heap()
 */
SPUDRESULT spudgpu_create_heap(
    spudgpu_device device,
    const spudgpu_heap_desc *desc,
    spudgpu_heap *out_heap);

/**
 * @brief Frees a heap's memory.
 *
 * @warning Every buffer and image placed in the heap must be destroyed
 * first, which means command lists that reference them have finished.
 *
 * @param[in] heap Heap to destroy. NULL does nothing.
 */
void spudgpu_destroy_heap(spudgpu_heap heap);

/**
 * @brief Reads the descriptor a heap was created with.
 *
 * @retval SPUD_SUCCESS `out_desc` was filled in.
 * @retval SPUDRESULT_GPU_INVALID_HEAP `heap` is NULL.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_desc` is NULL.
 */
SPUDRESULT spudgpu_get_heap_desc(
    spudgpu_heap heap,
    spudgpu_heap_desc *out_desc);

/* ---- Placement requirements ------------------------------------------ */

/**
 * @brief What a resource needs from a heap: how many bytes from its offset,
 * and what its offset must be a multiple of.
 */
typedef struct spudgpu_placement_requirements {
	/**
	 * Bytes the resource covers from its offset. Never less than the
	 * buffer's size; often more (see "Backends" for each API's rounding).
	 * Two live resources in one heap never share a byte of these.
	 */
	uint64_t size;
	/** A power of two the offset is a multiple of. */
	uint64_t alignment;
} spudgpu_placement_requirements;

/**
 * @brief Reports what a buffer of `desc` needs to be placed in `heap`.
 *
 * Asks the heap, not the device, because the answer is the heap's memory's:
 * in a heap of a mappable kind, the size and alignment are both multiples of
 * what the device flushes and invalidates in (Vulkan's
 * nonCoherentAtomSize), so a flush or invalidate of one buffer never touches
 * a byte of another.
 *
 * The same `desc` and heap give the same answer for as long as the heap
 * exists. Nothing is allocated.
 *
 * @param[in]  heap             Heap the buffer would be placed in.
 * @param[in]  desc             The buffer, as spudgpu_create_placed_buffer()
 *                              would be given it. `gpu_address_location` is
 *                              not read.
 * @param[out] out_requirements Receives the size and alignment.
 *
 * @retval SPUD_SUCCESS `out_requirements` was filled in.
 * @retval SPUDRESULT_GPU_INVALID_HEAP `heap` is NULL.
 * @retval SPUDRESULT_NULL_DESC `desc` is NULL.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_requirements` is NULL.
 * @retval (every result of spudgpu_create_buffer()'s desc checks, in its
 *         order: size, usage, kind, usage against kind.)
 * @retval SPUDRESULT_GPU_HEAP_MISMATCH The heap is not a
 *         SPUDGPU_HEAP_RESOURCE_CLASS_BUFFERS heap, or `desc->memory_kind`
 *         is not the heap's.
 * @return SPUDRESULT_API_SPECIFIC_FAILURE if the graphics API's query fails.
 */
SPUDRESULT spudgpu_get_buffer_placement_requirements(
    spudgpu_heap heap,
    const spudgpu_buffer_desc *desc,
    spudgpu_placement_requirements *out_requirements);

/**
 * @brief Reports what an image of `desc` needs to be placed in `heap`.
 *
 * @retval SPUD_SUCCESS `out_requirements` was filled in.
 * @retval SPUDRESULT_GPU_INVALID_HEAP `heap` is NULL.
 * @retval SPUDRESULT_NULL_DESC `desc` is NULL.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_requirements` is NULL.
 * @retval (every result of spudgpu_create_image()'s desc checks, in its
 *         order.)
 * @retval SPUDRESULT_GPU_INVALID_IMAGE_USAGE `desc->usage` has
 *         SPUDGPU_IMAGE_USAGE_TRANSIENT_ATTACHMENT or
 *         SPUDGPU_IMAGE_USAGE_PRESENTABLE: a transient attachment's memory
 *         is the tile's or lazily allocated, never a heap's, and a
 *         presentable image's is the swap chain's.
 * @retval SPUDRESULT_GPU_HEAP_MISMATCH The heap's class is not the image's
 *         (see SPUDGPU_HEAP_RESOURCE_CLASS).
 * @return SPUDRESULT_API_SPECIFIC_FAILURE if the graphics API's query fails.
 */
SPUDRESULT spudgpu_get_image_placement_requirements(
    spudgpu_heap heap,
    const spudgpu_image_desc *desc,
    spudgpu_placement_requirements *out_requirements);

/* ---- Placed resources ------------------------------------------------ */

/**
 * @brief Creates a buffer in `heap`'s memory at `offset`.
 *
 * The result is a spudgpu_buffer like any other: views, binding, copies,
 * barriers, spudgpu_get_buffer_desc() and spudgpu_destroy_buffer() are
 * unchanged, and its `gpu_address_location` is valid. Destroying it gives
 * back nothing to the device; the bytes stay the heap's, and once the
 * buffer is destroyed (its command lists finished) another resource may be
 * placed over them. Its contents are undefined until written, as any new
 * buffer's are, whatever was in those bytes before.
 *
 * Mapping keeps its contract: a placed buffer has one mapping at a time and
 * may stay mapped for as long as it exists. Two placed buffers of one heap
 * can be mapped at once.
 *
 * Nothing checks that the bytes are free: placing two live resources over
 * one byte is undefined, on every backend.
 *
 * @param[in]  heap       Heap to place the buffer in.
 * @param[in]  offset     Byte offset in the heap. A multiple of the
 *                        alignment spudgpu_get_buffer_placement_requirements()
 *                        reports for `desc`.
 * @param[in]  desc       Buffer configuration. `memory_kind` is the heap's.
 * @param[out] out_buffer Receives the new buffer on success.
 *
 * @retval SPUD_SUCCESS The buffer was created.
 * @retval (every result of spudgpu_get_buffer_placement_requirements(), in
 *         its order, with `out_buffer` in place of `out_requirements`.)
 * @retval SPUDRESULT_GPU_PLACEMENT_MISALIGNED `offset` is not a multiple of
 *         the reported alignment.
 * @retval SPUDRESULT_GPU_PLACEMENT_OUT_OF_RANGE `offset` plus the reported
 *         size runs past the end of the heap.
 * @return Another SPUDRESULT if the backend fails.
 */
SPUDRESULT spudgpu_create_placed_buffer(
    spudgpu_heap heap,
    uint64_t offset,
    const spudgpu_buffer_desc *desc,
    spudgpu_buffer *out_buffer);

/**
 * @brief Creates an image in `heap`'s memory at `offset`.
 *
 * The result is a spudgpu_image like any other, destroyed with
 * spudgpu_destroy_image(), with the same rules as a placed buffer for what
 * destroying it gives back and what overlapping placements are.
 *
 * An image in a SPUDGPU_HEAP_RESOURCE_CLASS_ATTACHMENT_IMAGES heap has one
 * rule more, on every backend: the first thing done with it writes all of
 * every subresource. That is a rendering pass that clears each of its
 * attachments over the whole image, or copies into all of every
 * subresource. (D3D12 requires a placed render target or depth/stencil
 * resource to be initialized by a clear, a full copy or DiscardResource
 * before it is used otherwise; this is that rule, without the discard,
 * which SpudGPU has no command for. See open question 3.)
 *
 * @param[in]  heap      Heap to place the image in.
 * @param[in]  offset    Byte offset in the heap. A multiple of the
 *                       alignment spudgpu_get_image_placement_requirements()
 *                       reports for `desc`.
 * @param[in]  desc      Image configuration.
 * @param[out] out_image Receives the new image on success.
 *
 * @retval SPUD_SUCCESS The image was created.
 * @retval (every result of spudgpu_get_image_placement_requirements(), in
 *         its order, with `out_image` in place of `out_requirements`.)
 * @retval SPUDRESULT_GPU_PLACEMENT_MISALIGNED `offset` is not a multiple of
 *         the reported alignment.
 * @retval SPUDRESULT_GPU_PLACEMENT_OUT_OF_RANGE `offset` plus the reported
 *         size runs past the end of the heap.
 * @return Another SPUDRESULT if the backend fails.
 */
SPUDRESULT spudgpu_create_placed_image(
    spudgpu_heap heap,
    uint64_t offset,
    const spudgpu_image_desc *desc,
    spudgpu_image *out_image);

/* ---- Native unwrap (each backend's *_natives.h) ---------------------- */

/*
 *	VkDeviceMemory  spudgpu_vulkan_native_heap(spudgpu_heap);
 *	ID3D12Heap *    spudgpu_d3d12_native_heap(spudgpu_heap);
 *	id<MTLHeap>     spudgpu_metal_native_heap(spudgpu_heap);
 */

#if __cplusplus
}
#endif // __cplusplus

/*
 * ---- Backends ---------------------------------------------------------
 *
 * One new file per backend, the "heap" concern: spudgpuvulkanheap.c,
 * spudgpud3d12heap.cpp, spudgpumetalheap.m, holding the heap calls, both
 * requirement queries and both placed creates. The buffer and image structs
 * gain `_heap` (NULL for a committed resource) and `_heap_offset`; destroy
 * frees device memory only when `_heap` is NULL. A placed create shares its
 * desc checks and API desc building with spudgpu_create_buffer() /
 * spudgpu_create_image() through internal functions the buffer and image
 * files already have or gain, so the checks cannot drift.
 *
 * Vulkan (reference)
 * - Heap: one vkAllocateMemory. The memory type is chosen at creation, from
 *   the kind's properties (the same table spudgpu_create_buffer uses)
 *   intersected with the memoryTypeBits of probes: for BUFFERS, a VkBuffer
 *   with every usage the kind allows (the spec makes a buffer with fewer
 *   usages allow a superset of those types, so every buffer of the kind
 *   fits); for IMAGES, one color image (memoryTypeBits are the same for
 *   every optimal-tiling non-transient color image); for
 *   ATTACHMENT_IMAGES, a color attachment and one image of each
 *   depth/stencil format SpudGPU has (depth formats may differ per format).
 *   Probes are created and destroyed at heap creation; no extension needed.
 *   No type in the intersection is SPUDRESULT_API_SPECIFIC_FAILURE at heap
 *   creation, so a placement never fails for a memory type.
 * - BUFFERS heaps are allocated with VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
 *   as every buffer's memory is today.
 * - A heap of a mappable kind is mapped once, whole, at creation, and
 *   unmapped at destroy: Vulkan allows one mapping of a VkDeviceMemory at a
 *   time, so the buffers can't each map it. spudgpu_map_buffer() on a
 *   placed buffer returns the heap's pointer plus `_heap_offset` plus
 *   `offset`; spudgpu_unmap_buffer() has nothing to undo.
 * - Requirements: vkGetBufferMemoryRequirements / vkGetImageMemoryRequirements
 *   on a VkBuffer / VkImage created and destroyed for the query (or
 *   vkGetDevice*MemoryRequirements where maintenance4 is there - an
 *   optimisation, not a different answer). In a mappable heap, size and
 *   alignment are raised to multiples of nonCoherentAtomSize, which
 *   keeps the flush and invalidate widening inside the buffer's own bytes.
 * - Placed create: vkCreateBuffer / vkCreateImage, then vkBindBufferMemory /
 *   vkBindImageMemory at the offset.
 * - Images in heaps are always DEVICE_LOCAL; TRANSIENT_ATTACHMENT is refused
 *   before Vulkan is asked, so LAZILY_ALLOCATED never comes up.
 *
 * D3D12
 * - Heap: ID3D12Device::CreateHeap, D3D12_HEAP_DESC with the kind's heap
 *   properties (the function spudgpu_create_buffer uses), alignment
 *   D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT, flags
 *   D3D12_HEAP_FLAG_CREATE_NOT_ZEROED plus the class's ALLOW_ONLY_ flag. The
 *   ALLOW_ONLY_ flag is set on resource heap tier 2 too: one rule.
 * - Requirements: GetResourceAllocationInfo on the D3D12_RESOURCE_DESC the
 *   committed path builds (the 256-byte rounding of a UNIFORM-only buffer
 *   included). Buffers come back 64 KiB aligned and 64 KiB rounded: a
 *   placed buffer is 64 KiB at the least, as a committed one is. Small
 *   buffers stay suballocated inside a larger buffer, which is what
 *   Aprend's pools and material arena already do. A texture asks for
 *   D3D12_SMALL_RESOURCE_PLACEMENT_ALIGNMENT (4 KiB) first and takes what
 *   GetResourceAllocationInfo returns.
 * - Placed create: CreatePlacedResource1 (CreatePlacedResource for buffers
 *   is enough), same initial state and optimized clear value as the
 *   committed path.
 * - Map works per placed resource in UPLOAD, READBACK and custom heaps, so
 *   mapping is unchanged.
 * - A destroyed resource's bytes are reused only after its command lists
 *   finished (the destroy rule), so no aliasing barrier is needed between
 *   the old and the new resource.
 *
 * Metal
 * - Heap: MTLHeapDescriptor, type MTLHeapTypePlacement, the kind's storage
 *   and CPU cache mode (the table spudgpu_create_buffer uses),
 *   hazardTrackingMode MTLHazardTrackingModeTracked. A heap defaults to
 *   untracked, and resources inherit the heap's mode; SpudGPU's Metal
 *   barriers are no-ops that rely on tracking (spudgpumetalrenderpass.m),
 *   so the heap must be tracked or every barrier must become real. See open
 *   question 1.
 * - Requirements: heapBufferSizeAndAlignWithLength:options: /
 *   heapTextureSizeAndAlignWithDescriptor: on the device.
 * - Placed create: newBufferWithLength:options:offset: /
 *   newTextureWithDescriptor:offset:.
 * - Residency: resources reached through an argument buffer are already
 *   marked with useResource:usage: per set (spudgpumetaldescriptors.m);
 *   that works for placed resources unchanged. useHeap: is an optimisation
 *   for later.
 *
 * ---- Not in this draft -------------------------------------------------
 *
 * - Aliasing (two resources over the same bytes, used in turn with an
 *   aliasing barrier). Overlap is undefined here. When transient render
 *   targets want it, it is a barrier kind and a discard command, on all
 *   three.
 * - Memory budget queries (VK_EXT_memory_budget, QueryVideoMemoryInfo,
 *   currentAllocatedSize) and D3D12 residency (MakeResident / Evict). A
 *   caller that grows heaps will want the budget; it is a separate call.
 * - D3D12 tight alignment (D3D12_RESOURCE_FLAG_USE_TIGHT_ALIGNMENT), which
 *   would end the 64 KiB floor for buffers where the driver has it. That is
 *   a SPUDGPU_EXT_ or a device property if wanted.
 *
 * ---- Open questions for review -----------------------------------------
 *
 * 1. Metal hazard tracking. Tracked heaps keep today's no-op barriers
 *    correct, but Metal then tracks the heap, not each resource in it, as
 *    far as I know (to verify on hardware): a write to one placed resource
 *    orders against every read of the others in its heap. Correct, and
 *    some false serialization. The alternative is untracked heaps and real
 *    Metal barriers (memoryBarrierWithScope inside an encoder, MTLFence
 *    across encoders), which spudgpu_cmd_pipeline_barrier's shape doesn't
 *    support yet (its own comment says so). Proposed: tracked now, and
 *    untracked is its own item when the barriers are made real.
 *
 * 2. One resource class per heap even on D3D12 tier 2. Proposed yes (one
 *    rule everywhere, no bufferImageGranularity, Vulkan memory-type probes
 *    stay simple). Costs the caller one more heap list.
 *
 * 3. First use of a placed attachment image. Proposed: the "writes all of
 *    every subresource first" rule above, enforced by nothing. The other
 *    choice is a spudgpu_cmd_discard_image() command (DiscardResource on
 *    D3D12, nothing on Vulkan with an UNDEFINED-layout barrier, nothing on
 *    Metal), which aliasing would need anyway.
 *
 * 4. Destroying a heap with resources still placed. Proposed: a debug build
 *    counts live placements in the heap and spudgpu_destroy_heap() with a
 *    nonzero count is reported the way SpudGPU reports other misuse in
 *    debug (to match whatever that is - destroy_buffer has only its
 *    @warning today). Release builds don't count.
 *
 * 5. The requirement queries take the heap rather than the device. That is
 *    what lets Vulkan fold nonCoherentAtomSize into the answer and lets
 *    every backend refuse a class or kind mismatch before any placement.
 *    The caller has to have a heap before it can ask; Aprend's allocator
 *    keeps heaps per (kind, class), so it does.
 */

#endif // SPUDGPU_HEAP_DRAFT_H
