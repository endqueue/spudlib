/*
 * spudgpu_objects_test — SpudGPU calls nothing else in the workspace makes.
 *
 * On a device of the adapter asked for (0 by default), with nothing drawn:
 * a format looked up by name; a fence signalled from the CPU and waited on; a semaphore made
 * and destroyed; a buffer's desc and GPU address read back; a buffer view's
 * desc; a heap's desc; and a descriptor pool reset and allocated from again.
 * Each with its argument checks. Every block of the instance goes through a
 * counting allocator, checked at the end.
 *
 * Then, on a second device of the same adapter made with
 * SPUDGPU_FEATURE_PRESENT where it has it: the present scaling checks of
 * spudgpu_get_surface_present_scalings and spudgpu_create_swap_chain that
 * come before a surface is read, given a surface handle that is only a
 * head (not_a_surface). On Metal, where a surface is a CAMetalLayer and needs no
 * window, also the query's answer, a swap chain of each scaling, the gravity
 * each sets on the layer (a flipped layer's included) and the scaling read
 * back from the swap chain's desc.
 *
 * Then, on a third device with the optional features the adapter has of
 * those a frame can use, a frame drawn into an image and read back: every
 * draw call, the descriptor, push constant, copy, blit, barrier, clear,
 * placed-resource and timestamp calls, a mesh shader and the depth bounds
 * where the device has them, each call's work checked by the pixels it left
 * (record_frame, below, says which cell is whose). Then a compute shader
 * doubling a buffer, and its pipeline's desc read back. The shaders are
 * SPIR-V compiled from tests/shaders/ into tests/spudgpu_test_shaders.h.
 *
 * And, on the device made with SPUDGPU_FEATURE_PRESENT, three frames
 * presented through a swap chain on a real surface that nobody sees: a
 * hidden window on Windows, a CAMetalLayer with no window on Metal; on
 * Vulkan on Windows also a surface made by
 * spudgpu_create_surface_from_callback with a callback that calls
 * vkCreateWin32SurfaceKHR. Linux has no surface without a display here, and
 * skips the frames.
 *
 * Not here, because they can't run on every backend: the storage-buffer and
 * storage-image bindless calls and spudgpu_cmd_bind_bindless_resources_compute
 * (SPUDGPU_EXT_BINDLESS, 0 on Metal).
 *
 *   spudgpu_objects_test [--adapter N]
 *
 * --adapter picks the adapter, by its index in spudgpu_enumerate_adapters();
 * 0 by default. Exits 0 when every check passes, 1 when one fails, 77
 * (skipped) with no adapter.
 */

#include <spudgpu.h>

#include "../src/spudallocator.h"
#include "spudgpu_test_shaders.h"
#include "spudtest_allocator.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#endif

#if SPUDGPU_COMPILE_METAL_API
#include "spudgpu_test_metal_layer.h"
#endif

#define SKIPPED 77

static int failures = 0;

#define CHECK(cond)                                                              \
	do {                                                                         \
		if (!(cond)) {                                                           \
			fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
			++failures;                                                          \
		}                                                                        \
	} while (0)

static void test_format_from_string(void) {
	CHECK(spudgpu_format_from_string(NULL) == SPUDGPU_FORMAT_UNKNOWN);
	CHECK(spudgpu_format_from_string("") == SPUDGPU_FORMAT_UNKNOWN);
	CHECK(spudgpu_format_from_string("not a format") == SPUDGPU_FORMAT_UNKNOWN);
	/* A name is the enumerator's without its SPUDGPU_FORMAT_ prefix. */
	CHECK(spudgpu_format_from_string("R8G8B8A8_UNORM") == SPUDGPU_FORMAT_R8G8B8A8_UNORM);
	CHECK(spudgpu_format_from_string("B8G8R8A8_UNORM") == SPUDGPU_FORMAT_B8G8R8A8_UNORM);
	CHECK(spudgpu_format_from_string("D32_FLOAT") == SPUDGPU_FORMAT_D32_FLOAT);
	CHECK(spudgpu_format_from_string("SPUDGPU_FORMAT_R8G8B8A8_UNORM") == SPUDGPU_FORMAT_UNKNOWN);
	CHECK(spudgpu_format_from_string("r8g8b8a8_unorm") == SPUDGPU_FORMAT_UNKNOWN);
}

static void test_fence_signal(spudgpu_device device) {
	spudgpu_fence fence = NULL;
	CHECK(spudgpu_create_fence(device, SPUDGPU_FENCE_FLAG_NONE, 3, &fence) == SPUD_SUCCESS);
	if (!fence)
		return;
	CHECK(spudgpu_get_fence_value(fence) == 3);
	CHECK(spudgpu_signal_fence(NULL, fence, 4) == SPUDRESULT_GPU_INVALID_DEVICE);
	CHECK(spudgpu_signal_fence(device, NULL, 4) == SPUDRESULT_GPU_INVALID_FENCE);
	CHECK(spudgpu_signal_fence(device, fence, 7) == SPUD_SUCCESS);
	CHECK(spudgpu_get_fence_value(fence) == 7);
	/* A wait for the signalled value returns at once; a higher one times out. */
	uint64_t value = 7;
	CHECK(spudgpu_wait_for_fences(device, &fence, &value, 1, true, 0) == SPUD_SUCCESS);
	value = 8;
	CHECK(spudgpu_wait_for_fences(device, &fence, &value, 1, true, 0) == SPUDRESULT_GPU_FENCE_WAIT_TIMED_OUT);
	CHECK(spudgpu_signal_fence(device, fence, 8) == SPUD_SUCCESS);
	CHECK(spudgpu_wait_for_fences(device, &fence, &value, 1, true, 1000000000ull) == SPUD_SUCCESS);
	spudgpu_destroy_fence(fence);
}

static void test_semaphore(spudgpu_device device) {
	spudgpu_semaphore semaphore = NULL;
	CHECK(spudgpu_create_semaphore(NULL, &semaphore) == SPUDRESULT_GPU_INVALID_DEVICE);
	CHECK(spudgpu_create_semaphore(device, NULL) == SPUDRESULT_NULL_OUTPUT_PARAMETER);
	CHECK(spudgpu_create_semaphore(device, &semaphore) == SPUD_SUCCESS);
	CHECK(semaphore != NULL);
	spudgpu_destroy_semaphore(semaphore);
	spudgpu_destroy_semaphore(NULL);
}

static void test_buffer_and_view(spudgpu_device device) {
	char name[32];
	strcpy(name, "objects buffer");
	spudgpu_buffer_desc desc;
	memset(&desc, 0, sizeof(desc));
	desc.base.type   = SPUD_STRUCT_TYPE_GPU_BUFFER_DESC;
	desc.usage       = SPUDGPU_BUFFER_USAGE_UNIFORM | SPUDGPU_BUFFER_USAGE_TRANSFER_SRC;
	desc.memory_kind = SPUDGPU_MEMORY_KIND_UPLOAD;
	desc.size        = 4096;
	desc.debug_name  = name;
	spudgpu_buffer buffer = NULL;
	CHECK(spudgpu_create_buffer(device, &desc, &buffer) == SPUD_SUCCESS);
	if (!buffer)
		return;
	strcpy(name, "changed");

	spudgpu_buffer_desc read;
	memset(&read, 0xA5, sizeof(read));
	CHECK(spudgpu_get_buffer_desc(NULL, &read) == SPUDRESULT_GPU_INVALID_BUFFER);
	CHECK(spudgpu_get_buffer_desc(buffer, NULL) == SPUDRESULT_NULL_OUTPUT_PARAMETER);
	CHECK(spudgpu_get_buffer_desc(buffer, &read) == SPUD_SUCCESS);
	CHECK(read.base.type == SPUD_STRUCT_TYPE_GPU_BUFFER_DESC);
	CHECK(read.usage == desc.usage);
	CHECK(read.memory_kind == desc.memory_kind);
	CHECK(read.size == desc.size);
	/* The buffer's own copy of its name, not the caller's string. */
	CHECK(read.debug_name != NULL && read.debug_name != name && strcmp(read.debug_name, "objects buffer") == 0);

	uint64_t address = 0;
	CHECK(spudgpu_get_buffer_gpu_address(NULL, &address) == SPUDRESULT_GPU_INVALID_BUFFER);
	CHECK(spudgpu_get_buffer_gpu_address(buffer, NULL) == SPUDRESULT_NULL_OUTPUT_PARAMETER);
	CHECK(spudgpu_get_buffer_gpu_address(buffer, &address) == SPUD_SUCCESS);
	CHECK(address != 0);
	uint64_t again = 0;
	CHECK(spudgpu_get_buffer_gpu_address(buffer, &again) == SPUD_SUCCESS);
	CHECK(again == address);

	spudgpu_buffer_view_desc view_desc;
	memset(&view_desc, 0, sizeof(view_desc));
	view_desc.base.type                 = SPUD_STRUCT_TYPE_GPU_BUFFER_VIEW_DESC;
	view_desc.parent_buffer             = buffer;
	view_desc.offset_from_parent_buffer = 256;
	view_desc.stride                    = 16;
	view_desc.size                      = 512;
	spudgpu_buffer_view view            = NULL;
	CHECK(spudgpu_create_buffer_view(buffer, &view_desc, &view) == SPUD_SUCCESS);
	if (view) {
		spudgpu_buffer_view_desc view_read;
		memset(&view_read, 0xA5, sizeof(view_read));
		CHECK(spudgpu_get_buffer_view_desc(NULL, &view_read) == SPUDRESULT_GPU_INVALID_BUFFER_VIEW);
		CHECK(spudgpu_get_buffer_view_desc(view, NULL) == SPUDRESULT_NULL_OUTPUT_PARAMETER);
		CHECK(spudgpu_get_buffer_view_desc(view, &view_read) == SPUD_SUCCESS);
		CHECK(view_read.parent_buffer == buffer);
		CHECK(view_read.offset_from_parent_buffer == 256);
		CHECK(view_read.stride == 16);
		CHECK(view_read.size == 512);
		spudgpu_destroy_buffer_view(view);
	}
	spudgpu_destroy_buffer(buffer);
}

static void test_heap_desc(spudgpu_device device) {
	spudgpu_heap_desc desc;
	memset(&desc, 0, sizeof(desc));
	desc.base.type      = SPUD_STRUCT_TYPE_GPU_HEAP_DESC;
	desc.memory_kind    = SPUDGPU_MEMORY_KIND_DEVICE;
	desc.resource_class = SPUDGPU_HEAP_RESOURCE_CLASS_BUFFERS;
	desc.size           = SPUDGPU_HEAP_SIZE_GRANULARITY * 4;
	desc.debug_name     = "objects heap";
	spudgpu_heap heap   = NULL;
	CHECK(spudgpu_create_heap(device, &desc, &heap) == SPUD_SUCCESS);
	if (!heap)
		return;
	spudgpu_heap_desc read;
	memset(&read, 0xA5, sizeof(read));
	CHECK(spudgpu_get_heap_desc(NULL, &read) == SPUDRESULT_GPU_INVALID_HEAP);
	CHECK(spudgpu_get_heap_desc(heap, NULL) == SPUDRESULT_NULL_OUTPUT_PARAMETER);
	CHECK(spudgpu_get_heap_desc(heap, &read) == SPUD_SUCCESS);
	CHECK(read.memory_kind == desc.memory_kind);
	CHECK(read.resource_class == desc.resource_class);
	CHECK(read.size == desc.size);
	CHECK(read.debug_name != NULL && strcmp(read.debug_name, "objects heap") == 0);
	spudgpu_destroy_heap(heap);
}

static void test_descriptor_pool_reset(spudgpu_device device) {
	spudgpu_descriptor_set_layout_desc layout_desc;
	memset(&layout_desc, 0, sizeof(layout_desc));
	layout_desc.base.type                   = SPUD_STRUCT_TYPE_GPU_DESCRIPTOR_SET_LAYOUT_DESC;
	layout_desc.bindings[0].binding         = 0;
	layout_desc.bindings[0].descriptor_type = SPUDGPU_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	layout_desc.bindings[0].count           = 1;
	layout_desc.bindings[0].stage_flags     = SPUDGPU_SHADER_STAGE_VERTEX | SPUDGPU_SHADER_STAGE_FRAGMENT;
	layout_desc.binding_count               = 1;
	spudgpu_descriptor_set_layout layout    = NULL;
	CHECK(spudgpu_create_descriptor_set_layout(device, &layout_desc, &layout) == SPUD_SUCCESS);
	if (!layout)
		return;

	spudgpu_descriptor_pool_desc pool_desc;
	memset(&pool_desc, 0, sizeof(pool_desc));
	pool_desc.base.type                     = SPUD_STRUCT_TYPE_GPU_DESCRIPTOR_POOL_DESC;
	pool_desc.max_sets                      = 2;
	pool_desc.pool_sizes[0].descriptor_type = SPUDGPU_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	pool_desc.pool_sizes[0].count           = 2;
	pool_desc.pool_size_count               = 1;
	spudgpu_descriptor_pool pool            = NULL;
	CHECK(spudgpu_create_descriptor_pool(device, &pool_desc, &pool) == SPUD_SUCCESS);
	if (pool) {
		spudgpu_descriptor_set_desc set_desc;
		memset(&set_desc, 0, sizeof(set_desc));
		set_desc.base.type      = SPUD_STRUCT_TYPE_GPU_DESCRIPTOR_SET_DESC;
		set_desc.pool           = pool;
		set_desc.set_layouts[0] = layout;
		set_desc.set_layouts[1] = layout;
		set_desc.set_count      = 2;
		/* The pool's whole capacity, three times over: a reset gives it all
		 * back each time. */
		for (int round = 0; round < 3; ++round) {
			spudgpu_descriptor_set sets[2] = {NULL, NULL};
			CHECK(spudgpu_create_descriptor_sets(device, &set_desc, sets) == SPUD_SUCCESS);
			CHECK(sets[0] != NULL && sets[1] != NULL && sets[0] != sets[1]);
			spudgpu_reset_descriptor_pool(pool);
		}
		spudgpu_reset_descriptor_pool(NULL);
		spudgpu_destroy_descriptor_pool(pool);
	}
	spudgpu_destroy_descriptor_set_layout(layout);
}

// A swap chain desc with every field but the scaling set to what any
// backend takes; the queue is the device's first direct one.
static spudgpu_swap_chain_desc swap_chain_desc_for(spudgpu_device device, spudgpu_surface surface, SPUDGPU_PRESENT_SCALING scaling) {
	spudgpu_swap_chain_desc desc;
	memset(&desc, 0, sizeof(desc));
	desc.base.type = SPUD_STRUCT_TYPE_GPU_SWAP_CHAIN_DESC;
	desc.surface   = surface;
	spudgpu_get_command_queue(device, SPUDGPU_COMMAND_LIST_TYPE_DIRECT, 0, &desc.queue);
	desc.width           = 64;
	desc.height          = 64;
	desc.buffer_count    = 1;
	desc.format          = SPUDGPU_FORMAT_B8G8R8A8_UNORM;
	desc.present_mode    = SPUDGPU_PRESENT_MODE_FIFO;
	desc.present_scaling = scaling;
	desc.fullscreen_mode = SPUDGPU_FULLSCREEN_MODE_WINDOWED;
	return desc;
}

// A surface SpudGPU never made, for the checks that come before a surface
// is read: a handle's head and nothing else, marked live so that a Debug
// build's check for a destroyed handle (src/spudstale.h) lets it through to
// them.
static spudgpu_surface not_a_surface(void) {
	static spud_object_head head;
	SPUD_BIRTH(&head);
	return (spudgpu_surface)(void *)&head;
}

// A device made without SPUDGPU_FEATURE_PRESENT: both calls refuse it, after
// their argument checks.
static void test_present_scaling_without_present(spudgpu_device device) {
	spudgpu_surface surface = not_a_surface();
	uint32_t scalings       = 0;
	CHECK(spudgpu_get_surface_present_scalings(device, surface, &scalings) == SPUDRESULT_GPU_FEATURE_NOT_SUPPORTED);
	spudgpu_swap_chain_desc desc = swap_chain_desc_for(device, surface, SPUDGPU_PRESENT_SCALING_NONE);
	spudgpu_swap_chain swap_chain = NULL;
	// The feature is checked before the scaling.
	CHECK(spudgpu_create_swap_chain(device, &desc, &swap_chain) == SPUDRESULT_GPU_FEATURE_NOT_SUPPORTED);
	CHECK(swap_chain == NULL);
}

// [device] has SPUDGPU_FEATURE_PRESENT. Every check here comes before the
// surface is read, so the surface handle is a pointer to nothing SpudGPU
// knows but a live head (not_a_surface).
static void test_present_scaling_checks(spudgpu_device device) {
	spudgpu_surface surface = not_a_surface();
	uint32_t scalings       = 0xdeadu;
	CHECK(spudgpu_get_surface_present_scalings(NULL, surface, &scalings) == SPUDRESULT_GPU_INVALID_DEVICE);
	CHECK(spudgpu_get_surface_present_scalings(device, NULL, &scalings) == SPUDRESULT_GPU_INVALID_SURFACE);
	CHECK(spudgpu_get_surface_present_scalings(device, surface, NULL) == SPUDRESULT_NULL_OUTPUT_PARAMETER);
	CHECK(spudgpu_get_surface_present_scalings(device, NULL, NULL) == SPUDRESULT_GPU_INVALID_SURFACE);
	CHECK(scalings == 0xdeadu);

	spudgpu_swap_chain swap_chain = NULL;
	spudgpu_swap_chain_desc desc  = swap_chain_desc_for(device, surface, SPUDGPU_PRESENT_SCALING_NONE);
	CHECK(desc.queue != NULL);
	CHECK(spudgpu_create_swap_chain(NULL, &desc, &swap_chain) == SPUDRESULT_GPU_INVALID_DEVICE);
	// A zeroed desc has not chosen.
	CHECK(spudgpu_create_swap_chain(device, &desc, &swap_chain) == SPUDRESULT_GPU_INVALID_PRESENT_SCALING);
	// Exactly one value, and only a value the enum has.
	desc.present_scaling = SPUDGPU_PRESENT_SCALING_ONE_TO_ONE | SPUDGPU_PRESENT_SCALING_STRETCH;
	CHECK(spudgpu_create_swap_chain(device, &desc, &swap_chain) == SPUDRESULT_GPU_INVALID_PRESENT_SCALING);
	desc.present_scaling = SPUDGPU_PRESENT_SCALING_ASPECT_FIT << 1;
	CHECK(spudgpu_create_swap_chain(device, &desc, &swap_chain) == SPUDRESULT_GPU_INVALID_PRESENT_SCALING);
	desc.present_scaling = 0xffffffffu;
	CHECK(spudgpu_create_swap_chain(device, &desc, &swap_chain) == SPUDRESULT_GPU_INVALID_PRESENT_SCALING);
	// The checks before it come first: the output, the surface, the queue.
	desc.present_scaling = SPUDGPU_PRESENT_SCALING_NONE;
	CHECK(spudgpu_create_swap_chain(device, &desc, NULL) == SPUDRESULT_NULL_OUTPUT_PARAMETER);
	desc.surface = NULL;
	CHECK(spudgpu_create_swap_chain(device, &desc, &swap_chain) == SPUDRESULT_GPU_INVALID_SURFACE);
	desc.surface = surface;
	desc.queue   = NULL;
	CHECK(spudgpu_create_swap_chain(device, &desc, &swap_chain) == SPUDRESULT_GPU_INVALID_COMMAND_QUEUE);
	CHECK(swap_chain == NULL);

	CHECK(strcmp(spudresult_str(SPUDRESULT_GPU_INVALID_PRESENT_SCALING), "SPUDRESULT_GPU_INVALID_PRESENT_SCALING") == 0);
	CHECK(strcmp(spudresult_str(SPUDRESULT_GPU_PRESENT_SCALING_NOT_SUPPORTED), "SPUDRESULT_GPU_PRESENT_SCALING_NOT_SUPPORTED") == 0);
}

#if SPUDGPU_COMPILE_METAL_API
// A swap chain of [scaling] on a layer, which sets the layer's gravity to
// [gravity] and reads its scaling back.
static void check_metal_swap_chain(spudgpu_device device, spudgpu_surface surface, void *layer, SPUDGPU_PRESENT_SCALING scaling, const char *gravity) {
	spudgpu_test_metal_layer_set_gravity_other(layer);
	spudgpu_swap_chain_desc desc  = swap_chain_desc_for(device, surface, scaling);
	spudgpu_swap_chain swap_chain = NULL;
	CHECK(spudgpu_create_swap_chain(device, &desc, &swap_chain) == SPUD_SUCCESS);
	CHECK(swap_chain != NULL);
	CHECK(strcmp(spudgpu_test_metal_layer_gravity(layer), gravity) == 0);
	spudgpu_swap_chain_desc read;
	memset(&read, 0, sizeof(read));
	CHECK(spudgpu_get_swap_chain_desc(swap_chain, &read) == SPUD_SUCCESS);
	CHECK(read.present_scaling == scaling);
	spudgpu_destroy_swap_chain(swap_chain);
}

static void test_present_scaling_metal(spudgpu_instance instance, spudgpu_device device) {
	for (int flipped = 0; flipped < 2; ++flipped) {
		void *layer             = spudgpu_test_metal_layer_create(flipped != 0);
		spudgpu_surface surface = NULL;
		CHECK(spudgpu_create_surface(instance, layer, NULL, &surface) == SPUD_SUCCESS);
		if (surface) {
			uint32_t scalings = 0;
			CHECK(spudgpu_get_surface_present_scalings(device, surface, &scalings) == SPUD_SUCCESS);
			CHECK(scalings == (SPUDGPU_PRESENT_SCALING_ONE_TO_ONE | SPUDGPU_PRESENT_SCALING_STRETCH | SPUDGPU_PRESENT_SCALING_ASPECT_FIT));
			// Top left on screen: a flipped layer's contents put the y axis
			// the other way up.
			check_metal_swap_chain(device, surface, layer, SPUDGPU_PRESENT_SCALING_ONE_TO_ONE, flipped ? "bottomLeft" : "topLeft");
			check_metal_swap_chain(device, surface, layer, SPUDGPU_PRESENT_SCALING_STRETCH, "resize");
			check_metal_swap_chain(device, surface, layer, SPUDGPU_PRESENT_SCALING_ASPECT_FIT, "resizeAspect");
			spudgpu_destroy_surface(surface);
		}
		spudgpu_test_metal_layer_destroy(layer);
	}
}
#endif

// ---- A frame drawn and read back ------------------------------------------
//
// One pass into a 64 x 64 image seen as an 8 x 8 grid of cells, each draw
// call covering cells of its own in a colour of its own, then the image
// copied to a second one by a blit and both read back, so that what each
// call drew is checked by its pixels. The frame takes the SpudGPU calls
// nothing else in this test runs: shader modules and pipelines, a uniform
// buffer, a texture and a sampler through a descriptor set, push constants,
// vertex, index and indirect buffers, every draw call, a colour and a depth
// clear before the pass, the depth bounds and a mesh shader where the device
// has them, timestamps where it has them, buffer and image copies and
// barriers, a placed buffer and a placed image, debug labels, a plain submit
// and a wait for the queue. A compute shader doubles a buffer after it.

#define DRAW_GRID 8
#define DRAW_CELL 8
#define DRAW_SIZE (DRAW_GRID * DRAW_CELL)

typedef struct draw_push {
	float color[4];
	// x: depth; y: how much of the texture to add; zw: the mesh shader's
	// first cell.
	float params[4];
} draw_push;

typedef struct cell {
	float x, y;
} cell;

static spudgpu_shader_module make_module(spudgpu_device device, const uint32_t *words, size_t size, SPUDGPU_SHADER_STAGE stage, const char *name) {
	spudgpu_shader_module_desc desc;
	memset(&desc, 0, sizeof(desc));
	desc.base.type               = SPUD_STRUCT_TYPE_GPU_SHADER_MODULE_DESC;
	desc.stage                   = stage;
	desc.spirv_code              = words;
	desc.spirv_size              = size;
	desc.debug_name              = name;
	spudgpu_shader_module module = NULL;
	CHECK(spudgpu_create_shader_module(device, &desc, &module) == SPUD_SUCCESS);
	return module;
}

static spudgpu_buffer make_buffer(spudgpu_device device, SPUDGPU_MEMORY_KIND kind, SPUDGPU_BUFFER_USAGE usage, uint64_t size, const void *contents) {
	spudgpu_buffer_desc desc;
	memset(&desc, 0, sizeof(desc));
	desc.base.type        = SPUD_STRUCT_TYPE_GPU_BUFFER_DESC;
	desc.usage            = usage;
	desc.memory_kind      = kind;
	desc.size             = size;
	spudgpu_buffer buffer = NULL;
	CHECK(spudgpu_create_buffer(device, &desc, &buffer) == SPUD_SUCCESS);
	if (buffer && contents) {
		void *mapped = NULL;
		CHECK(spudgpu_map_buffer(buffer, 0, size, &mapped) == SPUD_SUCCESS);
		if (mapped) {
			memcpy(mapped, contents, (size_t)size);
			CHECK(spudgpu_flush_buffer(buffer, 0, size) == SPUD_SUCCESS);
			spudgpu_unmap_buffer(buffer);
		}
	}
	return buffer;
}

static spudgpu_buffer_view make_view(spudgpu_buffer buffer, uint64_t offset, uint64_t stride, uint64_t size) {
	spudgpu_buffer_view_desc desc;
	memset(&desc, 0, sizeof(desc));
	desc.base.type                 = SPUD_STRUCT_TYPE_GPU_BUFFER_VIEW_DESC;
	desc.parent_buffer             = buffer;
	desc.offset_from_parent_buffer = offset;
	desc.stride                    = stride;
	desc.size                      = size;
	spudgpu_buffer_view view       = NULL;
	if (buffer)
		CHECK(spudgpu_create_buffer_view(buffer, &desc, &view) == SPUD_SUCCESS);
	return view;
}

static spudgpu_image make_image(spudgpu_device device, spudgpu_heap heap, SPUDGPU_IMAGE_USAGE usage, SPUDGPU_FORMAT format, uint32_t width, uint32_t height,
                                const char *name) {
	spudgpu_image_desc desc;
	memset(&desc, 0, sizeof(desc));
	desc.base.type          = SPUD_STRUCT_TYPE_GPU_IMAGE_DESC;
	desc.usage              = usage;
	desc.type               = SPUDGPU_IMAGE_TYPE_2D;
	desc.format             = format;
	desc.clear_value.format = format;
	// The values the frame clears the attachments to, which D3D12 clears
	// fastest when told at creation.
	if (format == SPUDGPU_FORMAT_D32_FLOAT) {
		desc.clear_value.depth_stencil.depth = 0.25f;
	} else {
		desc.clear_value.color[0] = 0.4f;
		desc.clear_value.color[1] = 0.4f;
		desc.clear_value.color[2] = 0.4f;
		desc.clear_value.color[3] = 1.0f;
	}
	desc.width          = width;
	desc.height         = height;
	desc.depth          = 1;
	desc.array_layers   = 1;
	desc.mip_levels     = 1;
	desc.debug_name     = name;
	spudgpu_image image = NULL;
	if (heap) {
		spudgpu_placement_requirements needs;
		memset(&needs, 0, sizeof(needs));
		needs.base.type = SPUD_STRUCT_TYPE_GPU_PLACEMENT_REQUIREMENTS;
		CHECK(spudgpu_get_image_placement_requirements(heap, &desc, &needs) == SPUD_SUCCESS);
		CHECK(needs.size >= (uint64_t)width * height * 4 && needs.alignment > 0);
		CHECK(spudgpu_create_placed_image(heap, 0, &desc, &image) == SPUD_SUCCESS);
	} else {
		CHECK(spudgpu_create_image(device, &desc, &image) == SPUD_SUCCESS);
	}
	return image;
}

static spudgpu_image_view make_image_view(spudgpu_image image, uint64_t aspect) {
	spudgpu_image_view_desc desc;
	memset(&desc, 0, sizeof(desc));
	desc.base.type                           = SPUD_STRUCT_TYPE_GPU_IMAGE_VIEW_DESC;
	desc.parent_image                        = image;
	desc.type                                = SPUDGPU_IMAGE_VIEW_TYPE_2D;
	desc.subresource_range.aspect_mask       = aspect;
	desc.subresource_range.mip_level_count   = 1;
	desc.subresource_range.array_layer_count = 1;
	spudgpu_image_view view                  = NULL;
	if (image)
		CHECK(spudgpu_create_image_view(image, &desc, &view) == SPUD_SUCCESS);
	return view;
}

static spudgpu_heap make_heap(spudgpu_device device, SPUDGPU_HEAP_RESOURCE_CLASS resource_class, const char *name) {
	spudgpu_heap_desc desc;
	memset(&desc, 0, sizeof(desc));
	desc.base.type      = SPUD_STRUCT_TYPE_GPU_HEAP_DESC;
	desc.memory_kind    = SPUDGPU_MEMORY_KIND_DEVICE;
	desc.resource_class = resource_class;
	desc.size           = SPUDGPU_HEAP_SIZE_GRANULARITY * 16;
	desc.debug_name     = name;
	spudgpu_heap heap   = NULL;
	CHECK(spudgpu_create_heap(device, &desc, &heap) == SPUD_SUCCESS);
	return heap;
}

// A graphics pipeline over the test's shaders: [mesh] in place of the
// vertex shader where given; depth as asked.
static spudgpu_shader_pipeline make_pipeline(spudgpu_device device, spudgpu_shader_module vertex, spudgpu_shader_module mesh, spudgpu_shader_module fragment,
                                             spudgpu_descriptor_set_layout layout, bool depth_test, SPUDGPU_COMPARE_OP compare, bool depth_bounds,
                                             const char *name) {
	spudgpu_shader_pipeline_desc desc;
	memset(&desc, 0, sizeof(desc));
	desc.base.type       = SPUD_STRUCT_TYPE_GPU_SHADER_PIPELINE_DESC;
	desc.debug_name      = name;
	desc.fragment_module = fragment;
	if (mesh) {
		desc.mesh_module = mesh;
	} else {
		desc.vertex_module                   = vertex;
		desc.vertex_attributes[0].location   = 0;
		desc.vertex_attributes[0].binding    = 0;
		desc.vertex_attributes[0].format     = SPUDGPU_FORMAT_R32G32_FLOAT;
		desc.vertex_attributes[1].location   = 1;
		desc.vertex_attributes[1].binding    = 1;
		desc.vertex_attributes[1].format     = SPUDGPU_FORMAT_R32G32_FLOAT;
		desc.vertex_attribute_count          = 2;
		desc.vertex_bindings[0].binding      = 0;
		desc.vertex_bindings[0].stride       = sizeof(cell);
		desc.vertex_bindings[1].binding      = 1;
		desc.vertex_bindings[1].stride       = sizeof(cell);
		desc.vertex_bindings[1].per_instance = true;
		desc.vertex_binding_count            = 2;
	}
	desc.primitive_topology          = SPUDGPU_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	desc.cull_mode                   = SPUDGPU_CULL_MODE_NONE;
	desc.depth_test_enable           = depth_test;
	desc.depth_write_enable          = depth_test && !depth_bounds;
	desc.depth_compare_op            = compare;
	desc.depth_bounds_test_enable    = depth_bounds;
	desc.color_attachment_formats[0] = SPUDGPU_FORMAT_R8G8B8A8_UNORM;
	desc.color_attachment_count      = 1;
	desc.depth_format                = SPUDGPU_FORMAT_D32_FLOAT;
	desc.descriptor_set_layouts[0]   = layout;
	desc.descriptor_set_layout_count = 1;
	desc.push_constant_ranges[0].stage_flags = (mesh ? SPUDGPU_SHADER_STAGE_MESH : SPUDGPU_SHADER_STAGE_VERTEX) | SPUDGPU_SHADER_STAGE_FRAGMENT;
	desc.push_constant_ranges[0].size        = sizeof(draw_push);
	desc.push_constant_range_count           = 1;
	spudgpu_shader_pipeline pipeline         = NULL;
	CHECK(spudgpu_create_shader_pipeline(device, &desc, &pipeline) == SPUD_SUCCESS);
	return pipeline;
}

static void push_draw(spudgpu_command_list cmd, spudgpu_shader_pipeline pipeline, float r, float g, float b, float a, float depth, float texture) {
	draw_push push = {{r, g, b, a}, {depth, texture, 0.0f, 0.0f}};
	spudgpu_cmd_push_constants(cmd, pipeline, 0, sizeof(push), &push);
}

// Each byte of a pixel, to within 2.
static bool pixel_is(const uint8_t *pixels, uint32_t row_pitch, uint32_t x, uint32_t y, int r, int g, int b, int a) {
	const uint8_t *p  = pixels + (size_t)y * row_pitch + (size_t)x * 4;
	const int want[4] = {r, g, b, a};
	for (int i = 0; i < 4; ++i) {
		const int d = (int)p[i] - want[i];
		if (d < -2 || d > 2) {
			fprintf(stderr, "  pixel (%u, %u) is (%u, %u, %u, %u), expected (%d, %d, %d, %d)\n", x, y, p[0], p[1], p[2], p[3], r, g, b, a);
			return false;
		}
	}
	return true;
}

// The middle of cell (cx, cy) is (r, g, b, a).
static bool cell_is(const uint8_t *pixels, uint32_t row_pitch, uint32_t cx, uint32_t cy, int r, int g, int b, int a) {
	return pixel_is(pixels, row_pitch, cx * DRAW_CELL + DRAW_CELL / 2, cy * DRAW_CELL + DRAW_CELL / 2, r, g, b, a);
}

// Records with [record], submits with spudgpu_submit_command_lists() and
// waits for the queue to be idle.
typedef void (*record_fn)(spudgpu_command_list cmd, void *context);
static void run_once(spudgpu_command_queue queue, spudgpu_command_allocator allocator, spudgpu_command_list cmd, record_fn record, void *context) {
	CHECK(spudgpu_reset_command_allocator(allocator) == SPUD_SUCCESS);
	spudgpu_begin_command_list(cmd);
	record(cmd, context);
	spudgpu_end_command_list(cmd);
	CHECK(spudgpu_submit_command_lists(queue, &cmd, 1) == SPUD_SUCCESS);
	spudgpu_queue_wait_idle(queue);
}

typedef struct draw_frame {
	SPUDGPU_FEATURES features;
	spudgpu_shader_pipeline plain, depth_tested, depth_bounded, meshed;
	spudgpu_descriptor_set set;
	spudgpu_image target, copy, depth, texture;
	spudgpu_image_view target_view, depth_view;
	spudgpu_buffer vertex_upload, vertices, texel_upload, indirect, readback_target, readback_copy;
	spudgpu_buffer_view corners, indices, instances[8];
	uint64_t texel_row_pitch, row_pitch;
	spudgpu_timestamp_set timestamps;
} draw_frame;

// The vertices to the device buffer, the texels to the texture: before the
// frame, each copy between its barriers.
static void record_uploads(spudgpu_command_list cmd, void *context) {
	draw_frame *f = (draw_frame *)context;
	spudgpu_cmd_begin_debug_label(cmd, "uploads");
	spudgpu_buffer_barrier to_copy;
	memset(&to_copy, 0, sizeof(to_copy));
	to_copy.base.type    = SPUD_STRUCT_TYPE_GPU_BUFFER_BARRIER;
	to_copy.buffer       = f->vertices;
	to_copy.state_before = SPUDGPU_RESOURCE_STATE_COMMON;
	to_copy.state_after  = SPUDGPU_RESOURCE_STATE_COPY_DEST;
	spudgpu_cmd_pipeline_barrier(cmd, &to_copy, 1, NULL, 0);
	// In two copies, so that the offsets count: the first six corners, then
	// the four after them.
	spudgpu_cmd_copy_buffer(cmd, f->vertex_upload, f->vertices, 0, 0, 6 * sizeof(cell));
	spudgpu_cmd_copy_buffer(cmd, f->vertex_upload, f->vertices, 6 * sizeof(cell), 6 * sizeof(cell), 4 * sizeof(cell));
	spudgpu_buffer_barrier to_vertices = to_copy;
	to_vertices.state_before           = SPUDGPU_RESOURCE_STATE_COPY_DEST;
	to_vertices.state_after            = SPUDGPU_RESOURCE_STATE_VERTEX_BUFFER;
	spudgpu_cmd_pipeline_barrier(cmd, &to_vertices, 1, NULL, 0);

	spudgpu_cmd_image_barrier(cmd, f->texture, SPUDGPU_IMAGE_LAYOUT_UNDEFINED, SPUDGPU_IMAGE_LAYOUT_TRANSFER_DST);
	spudgpu_image_buffer_copy_desc copy;
	memset(&copy, 0, sizeof(copy));
	copy.buffer_row_length = (uint32_t)(f->texel_row_pitch / 4);
	copy.array_layer_count = 1;
	copy.width             = 2;
	copy.height            = 2;
	copy.depth             = 1;
	spudgpu_cmd_copy_buffer_to_image(cmd, f->texel_upload, f->texture, &copy);
	spudgpu_cmd_image_barrier(cmd, f->texture, SPUDGPU_IMAGE_LAYOUT_TRANSFER_DST, SPUDGPU_IMAGE_LAYOUT_SHADER_READ_ONLY);
	spudgpu_cmd_end_debug_label(cmd);
}

static void record_frame(spudgpu_command_list cmd, void *context) {
	draw_frame *f = (draw_frame *)context;
	if (f->timestamps)
		spudgpu_cmd_reset_timestamps(cmd, f->timestamps, 0, 2);
	spudgpu_cmd_begin_debug_label(cmd, "frame");
	spudgpu_cmd_image_barrier_view(cmd, f->target_view, SPUDGPU_IMAGE_LAYOUT_UNDEFINED, SPUDGPU_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	spudgpu_cmd_image_barrier_view(cmd, f->depth_view, SPUDGPU_IMAGE_LAYOUT_UNDEFINED, SPUDGPU_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
	// Both attachments cleared before the pass, which loads them: what no
	// draw covers is grey only if the colour clear ran, and the depth tests
	// below pass and fail against the depth clear's 0.25.
	spudgpu_cmd_clear_color_attachment(cmd, f->target_view, 0.4f, 0.4f, 0.4f, 1.0f, DRAW_SIZE, DRAW_SIZE);
	spudgpu_cmd_clear_depth_attachment(cmd, f->depth_view, true, false, 0.25f, 0, DRAW_SIZE, DRAW_SIZE);

	spudgpu_rendering_begin_desc pass;
	memset(&pass, 0, sizeof(pass));
	pass.base.type                         = SPUD_STRUCT_TYPE_GPU_RENDERING_BEGIN_DESC;
	pass.color_attachments[0].image_view   = f->target_view;
	pass.color_attachments[0].load_op      = SPUDGPU_LOAD_OP_LOAD;
	pass.color_attachments[0].store_op     = SPUDGPU_STORE_OP_STORE;
	pass.color_attachment_count            = 1;
	pass.depth_attachment.image_view       = f->depth_view;
	pass.depth_attachment.depth_load_op    = SPUDGPU_LOAD_OP_LOAD;
	pass.depth_attachment.depth_store_op   = SPUDGPU_STORE_OP_STORE;
	pass.depth_attachment.stencil_load_op  = SPUDGPU_LOAD_OP_DONT_CARE;
	pass.depth_attachment.stencil_store_op = SPUDGPU_STORE_OP_DONT_CARE;
	pass.width                             = DRAW_SIZE;
	pass.height                            = DRAW_SIZE;
	if (f->timestamps) {
		pass.timestamp_set         = f->timestamps;
		pass.timestamp_begin_index = 0;
		pass.timestamp_end_index   = 1;
	}
	spudgpu_cmd_begin_rendering(cmd, &pass);

	SPUDGPU_VIEWPORT viewport = {0.0f, 0.0f, (float)DRAW_SIZE, (float)DRAW_SIZE, 0.0f, 1.0f};
	spudgpu_cmd_set_viewports(cmd, 0, 1, &viewport);
	SPUDGPU_SCISSOR_RECT scissor = {0.0f, 0.0f, (float)DRAW_SIZE, (float)DRAW_SIZE};
	spudgpu_cmd_set_scissor_rects(cmd, 0, 1, &scissor);

	spudgpu_cmd_bind_pipeline(cmd, f->plain);
	spudgpu_cmd_bind_descriptor_sets(cmd, f->plain, 0, &f->set, 1);
	spudgpu_cmd_set_index_buffer(cmd, f->indices);
	spudgpu_buffer_view views[2] = {f->corners, f->instances[0]};

	// Row 0, each call in its own colour.
	// (0, 0): a plain draw of six corners.
	spudgpu_cmd_set_vertex_buffers(cmd, 0, 2, views);
	push_draw(cmd, f->plain, 1.0f, 0.0f, 0.0f, 1.0f, 0.5f, 0.0f);
	spudgpu_cmd_draw(cmd, 6, 0);
	// (1, 0) and (2, 0): two instances from the second.
	views[1] = f->instances[1];
	spudgpu_cmd_set_vertex_buffers(cmd, 0, 2, views);
	push_draw(cmd, f->plain, 0.0f, 1.0f, 0.0f, 1.0f, 0.5f, 0.0f);
	spudgpu_cmd_draw_instanced(cmd, 6, 2, 0, 1);
	// (3, 0): indexed, the four corners after the first six.
	views[1] = f->instances[2];
	spudgpu_cmd_set_vertex_buffers(cmd, 0, 2, views);
	push_draw(cmd, f->plain, 1.0f, 1.0f, 0.0f, 1.0f, 0.5f, 0.0f);
	spudgpu_cmd_draw_indexed(cmd, 6, 0, 6);
	// (4, 0) and (5, 0): indexed and instanced, from the second six indices
	// and the second instance.
	views[1] = f->instances[3];
	spudgpu_cmd_set_vertex_buffers(cmd, 0, 2, views);
	push_draw(cmd, f->plain, 1.0f, 0.0f, 1.0f, 1.0f, 0.5f, 0.0f);
	spudgpu_cmd_draw_indexed_instanced(cmd, 6, 2, 6, 6, 1);
	// (6, 0), then (6, 0) and (7, 0): two indirect draws, 32 bytes apart;
	// in one call where the device draws several.
	views[1] = f->instances[4];
	spudgpu_cmd_set_vertex_buffers(cmd, 0, 2, views);
	push_draw(cmd, f->plain, 0.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.0f);
	const uint32_t draws = (f->features & SPUDGPU_FEATURE_MULTI_DRAW_INDIRECT) ? 2 : 1;
	spudgpu_cmd_draw_indirect(cmd, f->indirect, 0, draws, 32);
	if (draws == 1)
		spudgpu_cmd_draw_indirect(cmd, f->indirect, 32, 1, 32);
	// Row 1. (0, 1), then (0, 1) and (1, 1): two indexed indirect draws,
	// 20 bytes apart.
	views[1] = f->instances[5];
	spudgpu_cmd_set_vertex_buffers(cmd, 0, 2, views);
	push_draw(cmd, f->plain, 0.6f, 0.2f, 0.0f, 1.0f, 0.5f, 0.0f);
	spudgpu_cmd_draw_indexed_indirect(cmd, f->indirect, 64, draws, 20);
	if (draws == 1)
		spudgpu_cmd_draw_indexed_indirect(cmd, f->indirect, 84, 1, 20);
	// (2, 1): the texture alone, one texel a quarter of the cell.
	views[1] = f->instances[6];
	spudgpu_cmd_set_vertex_buffers(cmd, 0, 2, views);
	push_draw(cmd, f->plain, 0.0f, 0.0f, 0.0f, 0.0f, 0.5f, 1.0f);
	spudgpu_cmd_draw(cmd, 6, 0);

	// Depth, less than the cleared 0.25 to pass: (3, 1) at 0.2 passes and
	// writes 0.2, (4, 1) at 0.5 fails, (5, 1) at 0.1 passes.
	spudgpu_cmd_bind_pipeline(cmd, f->depth_tested);
	spudgpu_cmd_bind_descriptor_sets(cmd, f->depth_tested, 0, &f->set, 1);
	views[1] = f->instances[7];
	spudgpu_cmd_set_vertex_buffers(cmd, 0, 2, views);
	push_draw(cmd, f->depth_tested, 0.2f, 0.2f, 0.8f, 1.0f, 0.2f, 0.0f);
	spudgpu_cmd_draw_instanced(cmd, 6, 1, 0, 0);
	push_draw(cmd, f->depth_tested, 0.2f, 0.2f, 0.8f, 1.0f, 0.5f, 0.0f);
	spudgpu_cmd_draw_instanced(cmd, 6, 1, 0, 1);
	push_draw(cmd, f->depth_tested, 0.8f, 0.2f, 0.2f, 1.0f, 0.1f, 0.0f);
	spudgpu_cmd_draw_instanced(cmd, 6, 1, 0, 2);

	// Depth bounds of 0.15 to 0.22 over (3, 1), whose depth is 0.2, and
	// (6, 1), whose depth is 0.25: only the first is drawn.
	if (f->depth_bounded) {
		spudgpu_cmd_bind_pipeline(cmd, f->depth_bounded);
		spudgpu_cmd_bind_descriptor_sets(cmd, f->depth_bounded, 0, &f->set, 1);
		spudgpu_cmd_set_depth_bounds(cmd, 0.15f, 0.22f);
		push_draw(cmd, f->depth_bounded, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f);
		spudgpu_cmd_draw_instanced(cmd, 6, 2, 0, 3);
	}
	// Row 2. (0, 2) and (1, 2): a mesh shader's two workgroups.
	if (f->meshed) {
		spudgpu_cmd_bind_pipeline(cmd, f->meshed);
		spudgpu_cmd_bind_descriptor_sets(cmd, f->meshed, 0, &f->set, 1);
		draw_push push = {{1.0f, 0.6f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 2.0f}};
		spudgpu_cmd_push_constants(cmd, f->meshed, 0, sizeof(push), &push);
		spudgpu_cmd_dispatch_mesh(cmd, 2, 1, 1);
	}
	spudgpu_cmd_end_rendering(cmd);
	if (f->timestamps)
		spudgpu_cmd_resolve_timestamps(cmd, f->timestamps, 0, 2);

	// The target to the copy by a blit, then both to buffers.
	spudgpu_cmd_image_barrier_view(cmd, f->target_view, SPUDGPU_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, SPUDGPU_IMAGE_LAYOUT_TRANSFER_SRC);
	spudgpu_image_view_desc_subresource_range whole;
	memset(&whole, 0, sizeof(whole));
	whole.aspect_mask       = SPUDGPU_IMAGE_ASPECT_COLOR;
	whole.mip_level_count   = 1;
	whole.array_layer_count = 1;
	spudgpu_cmd_image_barrier_subresource(cmd, f->copy, &whole, SPUDGPU_IMAGE_LAYOUT_UNDEFINED, SPUDGPU_IMAGE_LAYOUT_TRANSFER_DST);
	spudgpu_image_blit_desc blit;
	memset(&blit, 0, sizeof(blit));
	blit.src_array_layer_count = 1;
	blit.src_x1                = DRAW_SIZE;
	blit.src_y1                = DRAW_SIZE;
	blit.src_z1                = 1;
	blit.dst_array_layer_count = 1;
	blit.dst_x1                = DRAW_SIZE;
	blit.dst_y1                = DRAW_SIZE;
	blit.dst_z1                = 1;
	blit.filter                = SPUDGPU_FILTER_NEAREST;
	spudgpu_cmd_blit_image(cmd, f->target, f->copy, &blit);
	spudgpu_cmd_image_barrier_subresource(cmd, f->copy, &whole, SPUDGPU_IMAGE_LAYOUT_TRANSFER_DST, SPUDGPU_IMAGE_LAYOUT_TRANSFER_SRC);
	spudgpu_image_buffer_copy_desc copy;
	memset(&copy, 0, sizeof(copy));
	copy.buffer_row_length = (uint32_t)(f->row_pitch / 4);
	copy.array_layer_count = 1;
	copy.width             = DRAW_SIZE;
	copy.height            = DRAW_SIZE;
	copy.depth             = 1;
	spudgpu_cmd_copy_image_to_buffer(cmd, f->target, f->readback_target, &copy);
	spudgpu_cmd_copy_image_to_buffer(cmd, f->copy, f->readback_copy, &copy);
	spudgpu_cmd_end_debug_label(cmd);
}

static void check_frame(const draw_frame *f, const uint8_t *pixels, const char *which) {
	const uint32_t pitch = (uint32_t)f->row_pitch;
	const int tint       = 51; // the uniform buffer's blue, added to every draw
	printf("spudgpu_objects_test: checking the %s\n", which);
	CHECK(cell_is(pixels, pitch, 0, 0, 255, 0, tint, 255));   // draw
	CHECK(cell_is(pixels, pitch, 1, 0, 0, 255, tint, 255));   // draw_instanced, first instance 1
	CHECK(cell_is(pixels, pitch, 2, 0, 0, 255, tint, 255));
	CHECK(cell_is(pixels, pitch, 3, 0, 255, 255, tint, 255)); // draw_indexed, base vertex 6
	CHECK(cell_is(pixels, pitch, 4, 0, 255, 0, 255, 255));    // draw_indexed_instanced
	CHECK(cell_is(pixels, pitch, 5, 0, 255, 0, 255, 255));
	CHECK(cell_is(pixels, pitch, 6, 0, 0, 255, 255, 255));    // draw_indirect, both draws
	CHECK(cell_is(pixels, pitch, 7, 0, 0, 255, 255, 255));
	CHECK(cell_is(pixels, pitch, 0, 1, 153, 51, tint, 255));  // draw_indexed_indirect, both draws
	CHECK(cell_is(pixels, pitch, 1, 1, 153, 51, tint, 255));
	// The texture: red, green above; blue, white below; nearest texels.
	CHECK(pixel_is(pixels, pitch, 2 * DRAW_CELL + 2, 1 * DRAW_CELL + 2, 255, 0, tint, 255));
	CHECK(pixel_is(pixels, pitch, 2 * DRAW_CELL + 6, 1 * DRAW_CELL + 2, 0, 255, tint, 255));
	CHECK(pixel_is(pixels, pitch, 2 * DRAW_CELL + 2, 1 * DRAW_CELL + 6, 0, 0, 255, 255));
	CHECK(pixel_is(pixels, pitch, 2 * DRAW_CELL + 6, 1 * DRAW_CELL + 6, 255, 255, 255, 255));
	// Depth: (3, 1) drawn, then drawn over within the bounds where they are
	// tested; (4, 1) not drawn; (5, 1) drawn; (6, 1) outside the bounds.
	if (f->depth_bounded)
		CHECK(cell_is(pixels, pitch, 3, 1, 255, 255, 255, 255));
	else
		CHECK(cell_is(pixels, pitch, 3, 1, 51, 51, 255, 255));
	CHECK(cell_is(pixels, pitch, 4, 1, 102, 102, 102, 255));
	CHECK(cell_is(pixels, pitch, 5, 1, 204, 51, 102, 255));
	CHECK(cell_is(pixels, pitch, 6, 1, 102, 102, 102, 255));
	if (f->meshed) {
		CHECK(cell_is(pixels, pitch, 0, 2, 255, 153, tint, 255));
		CHECK(cell_is(pixels, pitch, 1, 2, 255, 153, tint, 255));
	}
	// What nothing drew: the grey of the clear before the pass.
	CHECK(cell_is(pixels, pitch, 7, 7, 102, 102, 102, 255));
	CHECK(cell_is(pixels, pitch, 2, 2, 102, 102, 102, 255));
}

static void read_back(const draw_frame *f, spudgpu_buffer buffer, uint64_t size, const char *which) {
	void *mapped = NULL;
	CHECK(spudgpu_map_buffer(buffer, 0, size, &mapped) == SPUD_SUCCESS);
	if (!mapped)
		return;
	CHECK(spudgpu_invalidate_buffer(buffer, 0, size) == SPUD_SUCCESS);
	check_frame(f, (const uint8_t *)mapped, which);
	spudgpu_unmap_buffer(buffer);
}

static void test_draw_frame(spudgpu_device device, spudgpu_command_queue queue, spudgpu_command_allocator allocator, spudgpu_command_list cmd) {
	draw_frame f;
	memset(&f, 0, sizeof(f));
	f.features = spudgpu_get_device_features(device);

	// Shaders, a set of a tint, a texture and a sampler, and the pipelines.
	spudgpu_shader_module vertex =
	    make_module(device, spudgpu_test_objects_draw_vert, sizeof(spudgpu_test_objects_draw_vert), SPUDGPU_SHADER_STAGE_VERTEX, "draw vertex");
	spudgpu_shader_module fragment =
	    make_module(device, spudgpu_test_objects_draw_frag, sizeof(spudgpu_test_objects_draw_frag), SPUDGPU_SHADER_STAGE_FRAGMENT, "draw fragment");
	spudgpu_shader_module mesh = NULL;
	if (f.features & SPUDGPU_FEATURE_MESH_SHADING)
		mesh = make_module(device, spudgpu_test_objects_draw_mesh, sizeof(spudgpu_test_objects_draw_mesh), SPUDGPU_SHADER_STAGE_MESH, "draw mesh");
	else
		printf("spudgpu_objects_test: the device has no mesh shading, spudgpu_cmd_dispatch_mesh not run\n");
	spudgpu_descriptor_set_layout_desc layout_desc;
	memset(&layout_desc, 0, sizeof(layout_desc));
	layout_desc.base.type                   = SPUD_STRUCT_TYPE_GPU_DESCRIPTOR_SET_LAYOUT_DESC;
	layout_desc.bindings[0].binding         = 0;
	layout_desc.bindings[0].descriptor_type = SPUDGPU_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	layout_desc.bindings[0].count           = 1;
	layout_desc.bindings[0].stage_flags     = SPUDGPU_SHADER_STAGE_FRAGMENT;
	layout_desc.bindings[1].binding         = 1;
	layout_desc.bindings[1].descriptor_type = SPUDGPU_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	layout_desc.bindings[1].count           = 1;
	layout_desc.bindings[1].stage_flags     = SPUDGPU_SHADER_STAGE_FRAGMENT;
	layout_desc.bindings[2].binding         = 2;
	layout_desc.bindings[2].descriptor_type = SPUDGPU_DESCRIPTOR_TYPE_SAMPLER;
	layout_desc.bindings[2].count           = 1;
	layout_desc.bindings[2].stage_flags     = SPUDGPU_SHADER_STAGE_FRAGMENT;
	layout_desc.binding_count               = 3;
	spudgpu_descriptor_set_layout layout    = NULL;
	CHECK(spudgpu_create_descriptor_set_layout(device, &layout_desc, &layout) == SPUD_SUCCESS);
	if (vertex && fragment && layout) {
		f.plain        = make_pipeline(device, vertex, NULL, fragment, layout, false, SPUDGPU_COMPARE_OP_ALWAYS, false, "draw plain");
		f.depth_tested = make_pipeline(device, vertex, NULL, fragment, layout, true, SPUDGPU_COMPARE_OP_LESS, false, "draw depth");
		if (f.features & SPUDGPU_FEATURE_DEPTH_BOUNDS_TEST)
			f.depth_bounded = make_pipeline(device, vertex, NULL, fragment, layout, true, SPUDGPU_COMPARE_OP_ALWAYS, true, "draw bounds");
		else
			printf("spudgpu_objects_test: the device has no depth bounds test, spudgpu_cmd_set_depth_bounds not run\n");
		if (mesh)
			f.meshed = make_pipeline(device, NULL, mesh, fragment, layout, false, SPUDGPU_COMPARE_OP_ALWAYS, false, "draw meshed");
	}

	spudgpu_descriptor_pool_desc pool_desc;
	memset(&pool_desc, 0, sizeof(pool_desc));
	pool_desc.base.type                     = SPUD_STRUCT_TYPE_GPU_DESCRIPTOR_POOL_DESC;
	pool_desc.max_sets                      = 1;
	pool_desc.pool_sizes[0].descriptor_type = SPUDGPU_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	pool_desc.pool_sizes[0].count           = 1;
	pool_desc.pool_sizes[1].descriptor_type = SPUDGPU_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	pool_desc.pool_sizes[1].count           = 1;
	pool_desc.pool_sizes[2].descriptor_type = SPUDGPU_DESCRIPTOR_TYPE_SAMPLER;
	pool_desc.pool_sizes[2].count           = 1;
	pool_desc.pool_size_count               = 3;
	spudgpu_descriptor_pool pool            = NULL;
	CHECK(spudgpu_create_descriptor_pool(device, &pool_desc, &pool) == SPUD_SUCCESS);
	if (pool && layout) {
		spudgpu_descriptor_set_desc set_desc;
		memset(&set_desc, 0, sizeof(set_desc));
		set_desc.base.type      = SPUD_STRUCT_TYPE_GPU_DESCRIPTOR_SET_DESC;
		set_desc.pool           = pool;
		set_desc.set_layouts[0] = layout;
		set_desc.set_count      = 1;
		CHECK(spudgpu_create_descriptor_sets(device, &set_desc, &f.set) == SPUD_SUCCESS);
	}

	// The corners: a unit quad as six (two triangles), then as four for the
	// index buffer's two triangles; uploaded to a buffer placed in a heap.
	static const cell corners[10] = {{0, 0}, {1, 0}, {0, 1}, {0, 1}, {1, 0}, {1, 1}, {0, 0}, {1, 0}, {0, 1}, {1, 1}};
	f.vertex_upload               = make_buffer(device, SPUDGPU_MEMORY_KIND_UPLOAD, SPUDGPU_BUFFER_USAGE_TRANSFER_SRC, sizeof(corners), corners);
	spudgpu_heap buffer_heap      = make_heap(device, SPUDGPU_HEAP_RESOURCE_CLASS_BUFFERS, "draw buffers");
	if (buffer_heap) {
		spudgpu_buffer_desc desc;
		memset(&desc, 0, sizeof(desc));
		desc.base.type   = SPUD_STRUCT_TYPE_GPU_BUFFER_DESC;
		desc.usage       = SPUDGPU_BUFFER_USAGE_VERTEX | SPUDGPU_BUFFER_USAGE_TRANSFER_DST;
		desc.memory_kind = SPUDGPU_MEMORY_KIND_DEVICE;
		desc.size        = sizeof(corners);
		spudgpu_placement_requirements needs;
		memset(&needs, 0, sizeof(needs));
		needs.base.type = SPUD_STRUCT_TYPE_GPU_PLACEMENT_REQUIREMENTS;
		CHECK(spudgpu_get_buffer_placement_requirements(buffer_heap, &desc, &needs) == SPUD_SUCCESS);
		CHECK(needs.size >= sizeof(corners) && needs.alignment > 0);
		// Past the heap's start, at the alignment asked for.
		CHECK(spudgpu_create_placed_buffer(buffer_heap, needs.alignment, &desc, &f.vertices) == SPUD_SUCCESS);
	}
	f.corners = make_view(f.vertices, 0, sizeof(cell), sizeof(corners));
	// The instances: one list per draw call, a cell each. (-1, -1) is off
	// the target: what a draw that ignored its first instance would draw.
	static const cell instances[8][3] = {
	    {{0, 0}},
	    {{-1, -1}, {1, 0}, {2, 0}},
	    {{3, 0}},
	    {{-1, -1}, {4, 0}, {5, 0}},
	    {{6, 0}, {7, 0}},
	    {{0, 1}, {1, 1}},
	    {{2, 1}},
	    {{3, 1}, {4, 1}, {5, 1}},
	};
	// After the last list: the depth bounds' two cells.
	static const cell bounded[3] = {{3, 1}, {6, 1}};
	cell instance_data[8 * 3 + 3];
	memcpy(instance_data, instances, sizeof(instances));
	memcpy(instance_data + 8 * 3, bounded, sizeof(bounded));
	spudgpu_buffer instance_buffer = make_buffer(device, SPUDGPU_MEMORY_KIND_UPLOAD, SPUDGPU_BUFFER_USAGE_VERTEX, sizeof(instance_data), instance_data);
	for (int i = 0; i < 8; ++i)
		f.instances[i] = make_view(instance_buffer, (uint64_t)i * 3 * sizeof(cell), sizeof(cell), (i == 7 ? 6 : 3) * sizeof(cell));
	static const uint16_t indices[12] = {0, 1, 2, 2, 1, 3, 0, 1, 2, 2, 1, 3};
	spudgpu_buffer index_buffer       = make_buffer(device, SPUDGPU_MEMORY_KIND_UPLOAD, SPUDGPU_BUFFER_USAGE_INDEX, sizeof(indices), indices);
	f.indices                         = make_view(index_buffer, 0, sizeof(uint16_t), sizeof(indices));
	// Indirect arguments: two draws 32 bytes apart at 0, two indexed draws
	// 20 bytes apart at 64.
	uint8_t indirect[104];
	memset(indirect, 0, sizeof(indirect));
	const spudgpu_draw_indirect_args draws[2]                 = {{6, 1, 0, 0}, {6, 2, 0, 0}};
	const spudgpu_draw_indexed_indirect_args indexed_draws[2] = {{6, 1, 0, 6, 0}, {6, 2, 6, 6, 0}};
	memcpy(indirect, &draws[0], sizeof(draws[0]));
	memcpy(indirect + 32, &draws[1], sizeof(draws[1]));
	memcpy(indirect + 64, &indexed_draws[0], sizeof(indexed_draws[0]));
	memcpy(indirect + 84, &indexed_draws[1], sizeof(indexed_draws[1]));
	f.indirect = make_buffer(device, SPUDGPU_MEMORY_KIND_UPLOAD, SPUDGPU_BUFFER_USAGE_INDIRECT, sizeof(indirect), indirect);
	// The tint, written through a mapping of part of the buffer.
	const float tint[4]    = {0.0f, 0.0f, 0.2f, 0.0f};
	spudgpu_buffer uniform = make_buffer(device, SPUDGPU_MEMORY_KIND_UPLOAD, SPUDGPU_BUFFER_USAGE_UNIFORM, 256, NULL);
	if (uniform) {
		void *mapped = NULL;
		CHECK(spudgpu_map_buffer(uniform, 0, 256, &mapped) == SPUD_SUCCESS);
		if (mapped) {
			memcpy(mapped, tint, sizeof(tint));
			CHECK(spudgpu_flush_buffer(uniform, 0, sizeof(tint)) == SPUD_SUCCESS);
			spudgpu_unmap_buffer(uniform);
		}
	}

	// The images: the target placed in a heap, a copy, depth, and a 2 x 2
	// texture with its sampler.
	spudgpu_heap image_heap = make_heap(device, SPUDGPU_HEAP_RESOURCE_CLASS_ATTACHMENT_IMAGES, "draw attachments");
	if (image_heap)
		f.target = make_image(device, image_heap, SPUDGPU_IMAGE_USAGE_COLOR_ATTACHMENT | SPUDGPU_IMAGE_USAGE_TRANSFER_SRC, SPUDGPU_FORMAT_R8G8B8A8_UNORM, DRAW_SIZE,
		                      DRAW_SIZE, "draw target");
	f.copy = make_image(device, NULL, SPUDGPU_IMAGE_USAGE_TRANSFER_DST | SPUDGPU_IMAGE_USAGE_TRANSFER_SRC, SPUDGPU_FORMAT_R8G8B8A8_UNORM, DRAW_SIZE, DRAW_SIZE,
	                    "draw copy");
	f.depth   = make_image(device, NULL, SPUDGPU_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT, SPUDGPU_FORMAT_D32_FLOAT, DRAW_SIZE, DRAW_SIZE, "draw depth");
	f.texture = make_image(device, NULL, SPUDGPU_IMAGE_USAGE_SAMPLED | SPUDGPU_IMAGE_USAGE_TRANSFER_DST, SPUDGPU_FORMAT_R8G8B8A8_UNORM, 2, 2, "draw texture");
	f.target_view                   = make_image_view(f.target, SPUDGPU_IMAGE_ASPECT_COLOR);
	f.depth_view                    = make_image_view(f.depth, SPUDGPU_IMAGE_ASPECT_DEPTH);
	spudgpu_image_view texture_view = make_image_view(f.texture, SPUDGPU_IMAGE_ASPECT_COLOR);
	if (f.target && f.target_view) {
		spudgpu_image_desc read;
		memset(&read, 0xA5, sizeof(read));
		CHECK(spudgpu_get_image_desc(NULL, &read) == SPUDRESULT_GPU_INVALID_IMAGE);
		CHECK(spudgpu_get_image_desc(f.target, NULL) == SPUDRESULT_NULL_OUTPUT_PARAMETER);
		CHECK(spudgpu_get_image_desc(f.target, &read) == SPUD_SUCCESS);
		CHECK(read.width == DRAW_SIZE && read.height == DRAW_SIZE && read.format == SPUDGPU_FORMAT_R8G8B8A8_UNORM && read.mip_levels == 1);
		CHECK(read.debug_name != NULL && strcmp(read.debug_name, "draw target") == 0);
		spudgpu_image_view_desc view_read;
		memset(&view_read, 0xA5, sizeof(view_read));
		CHECK(spudgpu_get_image_view_desc(NULL, &view_read) == SPUDRESULT_GPU_INVALID_IMAGE_VIEW);
		CHECK(spudgpu_get_image_view_desc(f.target_view, NULL) == SPUDRESULT_NULL_OUTPUT_PARAMETER);
		CHECK(spudgpu_get_image_view_desc(f.target_view, &view_read) == SPUD_SUCCESS);
		CHECK(view_read.parent_image == f.target && view_read.type == SPUDGPU_IMAGE_VIEW_TYPE_2D);
		CHECK(view_read.subresource_range.aspect_mask == SPUDGPU_IMAGE_ASPECT_COLOR && view_read.subresource_range.mip_level_count == 1);
	}
	spudgpu_sampler_desc sampler_desc;
	memset(&sampler_desc, 0, sizeof(sampler_desc));
	sampler_desc.base.type      = SPUD_STRUCT_TYPE_GPU_SAMPLER_DESC;
	sampler_desc.mag_filter     = SPUDGPU_FILTER_NEAREST;
	sampler_desc.min_filter     = SPUDGPU_FILTER_NEAREST;
	sampler_desc.mipmap_filter  = SPUDGPU_FILTER_NEAREST;
	sampler_desc.address_mode_u = SPUDGPU_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler_desc.address_mode_v = SPUDGPU_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler_desc.address_mode_w = SPUDGPU_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler_desc.max_lod        = 0.0f;
	sampler_desc.max_anisotropy = 1.0f;
	sampler_desc.debug_name     = "draw sampler";
	spudgpu_sampler sampler     = NULL;
	CHECK(spudgpu_create_sampler(device, &sampler_desc, &sampler) == SPUD_SUCCESS);

	// The texels, each row at the pitch a copy wants.
	uint64_t texel_total = 0;
	spudgpu_get_image_buffer_copy_size(f.texture, 0, &f.texel_row_pitch, &texel_total);
	CHECK(f.texel_row_pitch >= 8 && texel_total >= f.texel_row_pitch + 8);
	uint64_t alignment = 0;
	CHECK(spudgpu_get_image_buffer_copy_offset_alignment(NULL, &alignment) == SPUDRESULT_GPU_INVALID_IMAGE);
	if (f.texture) {
		CHECK(spudgpu_get_image_buffer_copy_offset_alignment(f.texture, NULL) == SPUDRESULT_NULL_OUTPUT_PARAMETER);
		CHECK(spudgpu_get_image_buffer_copy_offset_alignment(f.texture, &alignment) == SPUD_SUCCESS);
		CHECK(alignment >= 4 && alignment % 4 == 0);
	}
	uint8_t texels[2048];
	memset(texels, 0, sizeof(texels));
	if (texel_total <= sizeof(texels)) {
		const uint8_t top[8]    = {255, 0, 0, 255, 0, 255, 0, 255};
		const uint8_t bottom[8] = {0, 0, 255, 255, 255, 255, 255, 255};
		memcpy(texels, top, 8);
		memcpy(texels + f.texel_row_pitch, bottom, 8);
		f.texel_upload = make_buffer(device, SPUDGPU_MEMORY_KIND_UPLOAD, SPUDGPU_BUFFER_USAGE_TRANSFER_SRC, texel_total, texels);
	}
	uint64_t total = 0;
	spudgpu_get_image_buffer_copy_size(f.target, 0, &f.row_pitch, &total);
	CHECK(f.row_pitch >= DRAW_SIZE * 4 && total >= f.row_pitch * (DRAW_SIZE - 1) + DRAW_SIZE * 4);
	f.readback_target = make_buffer(device, SPUDGPU_MEMORY_KIND_READBACK, SPUDGPU_BUFFER_USAGE_TRANSFER_DST, total, NULL);
	f.readback_copy   = make_buffer(device, SPUDGPU_MEMORY_KIND_READBACK, SPUDGPU_BUFFER_USAGE_TRANSFER_DST, total, NULL);

	if (f.set && uniform && texture_view && sampler) {
		spudgpu_descriptor_buffer_info uniform_info = {uniform, 0, 256};
		spudgpu_descriptor_image_info texture_info  = {texture_view, SPUDGPU_IMAGE_LAYOUT_SHADER_READ_ONLY};
		spudgpu_write_descriptor_set writes[3];
		memset(writes, 0, sizeof(writes));
		for (int i = 0; i < 3; ++i) {
			writes[i].base.type        = SPUD_STRUCT_TYPE_GPU_WRITE_DESCRIPTOR_SET;
			writes[i].dst_set          = f.set;
			writes[i].dst_binding      = (uint32_t)i;
			writes[i].descriptor_count = 1;
		}
		writes[0].descriptor_type = SPUDGPU_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writes[0].buffer_info     = &uniform_info;
		writes[1].descriptor_type = SPUDGPU_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		writes[1].image_info      = &texture_info;
		writes[2].descriptor_type = SPUDGPU_DESCRIPTOR_TYPE_SAMPLER;
		writes[2].sampler         = sampler;
		spudgpu_update_descriptor_sets(device, writes, 3);
	}

	if (f.features & SPUDGPU_FEATURE_PASS_TIMESTAMPS)
		CHECK(spudgpu_create_timestamp_set(device, queue, 2, &f.timestamps) == SPUD_SUCCESS);
	else
		printf("spudgpu_objects_test: the device has no pass timestamps, they are not run\n");

	const bool ready = f.plain && f.depth_tested && f.set && f.vertex_upload && f.vertices && f.corners && f.indices && f.indirect && f.texel_upload &&
	                   f.target_view && f.depth_view && f.copy && f.readback_target && f.readback_copy && instance_buffer && f.instances[7];
	CHECK(ready);
	if (ready) {
		run_once(queue, allocator, cmd, record_uploads, &f);
		run_once(queue, allocator, cmd, record_frame, &f);
		read_back(&f, f.readback_target, total, "target");
		read_back(&f, f.readback_copy, total, "blitted copy");
		if (f.timestamps) {
			uint64_t ns[2] = {0, 0};
			CHECK(spudgpu_get_timestamps(f.timestamps, 0, 2, ns) == SPUD_SUCCESS);
			CHECK(ns[1] >= ns[0]);
			printf("spudgpu_objects_test: the pass took %llu ns on the GPU\n", (unsigned long long)(ns[1] - ns[0]));
		}
	}

	spudgpu_destroy_timestamp_set(f.timestamps);
	spudgpu_destroy_sampler(sampler);
	spudgpu_destroy_image_view(texture_view);
	spudgpu_destroy_image_view(f.depth_view);
	spudgpu_destroy_image_view(f.target_view);
	spudgpu_destroy_image(f.texture);
	spudgpu_destroy_image(f.depth);
	spudgpu_destroy_image(f.copy);
	spudgpu_destroy_image(f.target);
	spudgpu_destroy_heap(image_heap);
	spudgpu_destroy_buffer(f.readback_copy);
	spudgpu_destroy_buffer(f.readback_target);
	spudgpu_destroy_buffer(f.texel_upload);
	spudgpu_destroy_buffer(uniform);
	spudgpu_destroy_buffer(f.indirect);
	spudgpu_destroy_buffer_view(f.indices);
	spudgpu_destroy_buffer(index_buffer);
	for (int i = 0; i < 8; ++i)
		spudgpu_destroy_buffer_view(f.instances[i]);
	spudgpu_destroy_buffer(instance_buffer);
	spudgpu_destroy_buffer_view(f.corners);
	spudgpu_destroy_buffer(f.vertices);
	spudgpu_destroy_heap(buffer_heap);
	spudgpu_destroy_buffer(f.vertex_upload);
	spudgpu_destroy_descriptor_pool(pool);
	spudgpu_destroy_shader_pipeline(f.meshed);
	spudgpu_destroy_shader_pipeline(f.depth_bounded);
	spudgpu_destroy_shader_pipeline(f.depth_tested);
	spudgpu_destroy_shader_pipeline(f.plain);
	spudgpu_destroy_descriptor_set_layout(layout);
	spudgpu_destroy_shader_module(mesh);
	spudgpu_destroy_shader_module(fragment);
	spudgpu_destroy_shader_module(vertex);
}

// ---- Compute -------------------------------------------------------------------

typedef struct compute_run {
	spudgpu_compute_pipeline pipeline;
	spudgpu_descriptor_set set;
	spudgpu_buffer upload, source, destination, readback;
} compute_run;

#define COMPUTE_COUNT 100u
#define COMPUTE_ADD 7u

static void record_compute(spudgpu_command_list cmd, void *context) {
	compute_run *c       = (compute_run *)context;
	const uint64_t bytes = COMPUTE_COUNT * sizeof(uint32_t);
	spudgpu_buffer_barrier barriers[2];
	memset(barriers, 0, sizeof(barriers));
	barriers[0].base.type    = SPUD_STRUCT_TYPE_GPU_BUFFER_BARRIER;
	barriers[0].buffer       = c->source;
	barriers[0].state_before = SPUDGPU_RESOURCE_STATE_COMMON;
	barriers[0].state_after  = SPUDGPU_RESOURCE_STATE_COPY_DEST;
	spudgpu_cmd_pipeline_barrier(cmd, barriers, 1, NULL, 0);
	spudgpu_cmd_copy_buffer(cmd, c->upload, c->source, 0, 0, bytes);
	barriers[0].state_before = SPUDGPU_RESOURCE_STATE_COPY_DEST;
	barriers[0].state_after  = SPUDGPU_RESOURCE_STATE_UNORDERED_ACCESS;
	barriers[1]              = barriers[0];
	barriers[1].buffer       = c->destination;
	barriers[1].state_before = SPUDGPU_RESOURCE_STATE_COMMON;
	spudgpu_cmd_pipeline_barrier(cmd, barriers, 2, NULL, 0);
	spudgpu_cmd_bind_compute_pipeline(cmd, c->pipeline);
	spudgpu_cmd_bind_descriptor_sets_compute(cmd, c->pipeline, 0, &c->set, 1);
	const uint32_t push[2] = {COMPUTE_ADD, COMPUTE_COUNT};
	spudgpu_cmd_push_constants_compute(cmd, c->pipeline, 0, sizeof(push), push);
	spudgpu_cmd_dispatch(cmd, (COMPUTE_COUNT + 63) / 64, 1, 1);
	barriers[0].buffer       = c->destination;
	barriers[0].state_before = SPUDGPU_RESOURCE_STATE_UNORDERED_ACCESS;
	barriers[0].state_after  = SPUDGPU_RESOURCE_STATE_COPY_SOURCE;
	spudgpu_cmd_pipeline_barrier(cmd, barriers, 1, NULL, 0);
	spudgpu_cmd_copy_buffer(cmd, c->destination, c->readback, 0, 0, bytes);
}

static void test_compute(spudgpu_device device, spudgpu_command_queue queue, spudgpu_command_allocator allocator, spudgpu_command_list cmd) {
	compute_run c;
	memset(&c, 0, sizeof(c));
	spudgpu_shader_module module =
	    make_module(device, spudgpu_test_objects_double_comp, sizeof(spudgpu_test_objects_double_comp), SPUDGPU_SHADER_STAGE_COMPUTE, "double");
	spudgpu_descriptor_set_layout_desc layout_desc;
	memset(&layout_desc, 0, sizeof(layout_desc));
	layout_desc.base.type = SPUD_STRUCT_TYPE_GPU_DESCRIPTOR_SET_LAYOUT_DESC;
	for (uint32_t i = 0; i < 2; ++i) {
		layout_desc.bindings[i].binding         = i;
		layout_desc.bindings[i].descriptor_type = SPUDGPU_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		layout_desc.bindings[i].count           = 1;
		layout_desc.bindings[i].stage_flags     = SPUDGPU_SHADER_STAGE_COMPUTE;
	}
	layout_desc.binding_count            = 2;
	spudgpu_descriptor_set_layout layout = NULL;
	CHECK(spudgpu_create_descriptor_set_layout(device, &layout_desc, &layout) == SPUD_SUCCESS);

	spudgpu_compute_pipeline_desc desc;
	memset(&desc, 0, sizeof(desc));
	desc.base.type                           = SPUD_STRUCT_TYPE_GPU_COMPUTE_PIPELINE_DESC;
	desc.compute_module                      = module;
	desc.descriptor_set_layouts[0]           = layout;
	desc.descriptor_set_layout_count         = 1;
	desc.push_constant_ranges[0].stage_flags = SPUDGPU_SHADER_STAGE_COMPUTE;
	desc.push_constant_ranges[0].size        = 2 * sizeof(uint32_t);
	desc.push_constant_range_count           = 1;
	desc.debug_name                          = "double";
	if (module && layout)
		CHECK(spudgpu_create_compute_pipeline(device, &desc, &c.pipeline) == SPUD_SUCCESS);
	if (c.pipeline) {
		spudgpu_compute_pipeline_desc read;
		memset(&read, 0xA5, sizeof(read));
		CHECK(spudgpu_get_compute_pipeline_desc(NULL, &read) == SPUDRESULT_GPU_INVALID_COMPUTE_PIPELINE);
		CHECK(spudgpu_get_compute_pipeline_desc(c.pipeline, NULL) == SPUDRESULT_NULL_OUTPUT_PARAMETER);
		CHECK(spudgpu_get_compute_pipeline_desc(c.pipeline, &read) == SPUD_SUCCESS);
		CHECK(read.base.type == SPUD_STRUCT_TYPE_GPU_COMPUTE_PIPELINE_DESC);
		CHECK(read.compute_module == module);
		CHECK(read.descriptor_set_layout_count == 1 && read.descriptor_set_layouts[0] == (void *)layout);
		CHECK(read.push_constant_range_count == 1 && read.push_constant_ranges[0].size == 8 &&
		      read.push_constant_ranges[0].stage_flags == SPUDGPU_SHADER_STAGE_COMPUTE);
		CHECK(read.debug_name != NULL && strcmp(read.debug_name, "double") == 0);
	}

	spudgpu_descriptor_pool_desc pool_desc;
	memset(&pool_desc, 0, sizeof(pool_desc));
	pool_desc.base.type                     = SPUD_STRUCT_TYPE_GPU_DESCRIPTOR_POOL_DESC;
	pool_desc.max_sets                      = 1;
	pool_desc.pool_sizes[0].descriptor_type = SPUDGPU_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	pool_desc.pool_sizes[0].count           = 2;
	pool_desc.pool_size_count               = 1;
	spudgpu_descriptor_pool pool            = NULL;
	CHECK(spudgpu_create_descriptor_pool(device, &pool_desc, &pool) == SPUD_SUCCESS);
	if (pool && layout) {
		spudgpu_descriptor_set_desc set_desc;
		memset(&set_desc, 0, sizeof(set_desc));
		set_desc.base.type      = SPUD_STRUCT_TYPE_GPU_DESCRIPTOR_SET_DESC;
		set_desc.pool           = pool;
		set_desc.set_layouts[0] = layout;
		set_desc.set_count      = 1;
		CHECK(spudgpu_create_descriptor_sets(device, &set_desc, &c.set) == SPUD_SUCCESS);
	}
	uint32_t values[COMPUTE_COUNT];
	for (uint32_t i = 0; i < COMPUTE_COUNT; ++i)
		values[i] = i * 3u + 1u;
	const uint64_t bytes = sizeof(values);
	c.upload             = make_buffer(device, SPUDGPU_MEMORY_KIND_UPLOAD, SPUDGPU_BUFFER_USAGE_TRANSFER_SRC, bytes, values);
	c.source             = make_buffer(device, SPUDGPU_MEMORY_KIND_DEVICE, SPUDGPU_BUFFER_USAGE_STORAGE | SPUDGPU_BUFFER_USAGE_TRANSFER_DST, bytes, NULL);
	c.destination        = make_buffer(device, SPUDGPU_MEMORY_KIND_DEVICE, SPUDGPU_BUFFER_USAGE_STORAGE | SPUDGPU_BUFFER_USAGE_TRANSFER_SRC, bytes, NULL);
	c.readback           = make_buffer(device, SPUDGPU_MEMORY_KIND_READBACK, SPUDGPU_BUFFER_USAGE_TRANSFER_DST, bytes, NULL);
	if (c.pipeline && c.set && c.upload && c.source && c.destination && c.readback) {
		spudgpu_descriptor_buffer_info infos[2] = {{c.source, 0, bytes}, {c.destination, 0, bytes}};
		spudgpu_write_descriptor_set writes[2];
		memset(writes, 0, sizeof(writes));
		for (int i = 0; i < 2; ++i) {
			writes[i].base.type        = SPUD_STRUCT_TYPE_GPU_WRITE_DESCRIPTOR_SET;
			writes[i].dst_set          = c.set;
			writes[i].dst_binding      = (uint32_t)i;
			writes[i].descriptor_count = 1;
			writes[i].descriptor_type  = SPUDGPU_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			writes[i].buffer_info      = &infos[i];
		}
		spudgpu_update_descriptor_sets(device, writes, 2);
		// Submitted with spudgpu_queue_submit(), signalling a fence to 5,
		// and waited for through the fence rather than the queue.
		spudgpu_fence fence = NULL;
		CHECK(spudgpu_create_fence(device, SPUDGPU_FENCE_FLAG_NONE, 0, &fence) == SPUD_SUCCESS);
		CHECK(spudgpu_reset_command_allocator(allocator) == SPUD_SUCCESS);
		spudgpu_begin_command_list(cmd);
		record_compute(cmd, &c);
		spudgpu_end_command_list(cmd);
		spudgpu_submit_desc submit;
		memset(&submit, 0, sizeof(submit));
		submit.base.type          = SPUD_STRUCT_TYPE_GPU_SUBMIT_DESC;
		submit.cmd_lists          = &cmd;
		submit.cmd_list_count     = 1;
		submit.signal_fence       = fence;
		submit.signal_fence_value = 5;
		CHECK(spudgpu_queue_submit(NULL, &submit) == SPUDRESULT_GPU_INVALID_COMMAND_QUEUE);
		CHECK(spudgpu_queue_submit(queue, &submit) == SPUD_SUCCESS);
		if (fence) {
			uint64_t value = 5;
			CHECK(spudgpu_wait_for_fences(device, &fence, &value, 1, true, 10000000000ull) == SPUD_SUCCESS);
			CHECK(spudgpu_get_fence_value(fence) == 5);
		}
		spudgpu_destroy_fence(fence);
		void *mapped = NULL;
		CHECK(spudgpu_map_buffer(c.readback, 0, bytes, &mapped) == SPUD_SUCCESS);
		if (mapped) {
			CHECK(spudgpu_invalidate_buffer(c.readback, 0, bytes) == SPUD_SUCCESS);
			const uint32_t *out = (const uint32_t *)mapped;
			uint32_t wrong      = 0;
			for (uint32_t i = 0; i < COMPUTE_COUNT; ++i)
				wrong += out[i] != values[i] * 2u + COMPUTE_ADD;
			CHECK(wrong == 0);
			printf("spudgpu_objects_test: the compute shader doubled %u values, %u wrong\n", COMPUTE_COUNT, wrong);
			spudgpu_unmap_buffer(c.readback);
		}
	}
	spudgpu_destroy_buffer(c.readback);
	spudgpu_destroy_buffer(c.destination);
	spudgpu_destroy_buffer(c.source);
	spudgpu_destroy_buffer(c.upload);
	spudgpu_destroy_descriptor_pool(pool);
	spudgpu_destroy_compute_pipeline(c.pipeline);
	spudgpu_destroy_descriptor_set_layout(layout);
	spudgpu_destroy_shader_module(module);
}

// A device with the features the frame can use that [adapter] has, and the
// frame and the compute shader on it.
static void test_drawing(spudgpu_adapter adapter, SPUDGPU_FEATURES supported) {
	spudgpu_device_desc desc;
	memset(&desc, 0, sizeof(desc));
	desc.base.type          = SPUD_STRUCT_TYPE_GPU_DEVICE_DESC;
	desc.required_features  = supported & (SPUDGPU_FEATURE_PASS_TIMESTAMPS | SPUDGPU_FEATURE_DEPTH_BOUNDS_TEST | SPUDGPU_FEATURE_MESH_SHADING |
                                          SPUDGPU_FEATURE_MULTI_DRAW_INDIRECT);
	desc.direct_queue_count = 1;
	desc.max_descriptors    = 16;
	desc.max_samplers       = 4;
	desc.debug_name         = "spudgpu_objects_test drawing";
	spudgpu_device device   = NULL;
	CHECK(spudgpu_create_device(adapter, &desc, &device) == SPUD_SUCCESS);
	if (!device)
		return;
	CHECK(spudgpu_get_device_features(device) == desc.required_features);
	spudgpu_command_queue queue = NULL;
	CHECK(spudgpu_get_command_queue(device, SPUDGPU_COMMAND_LIST_TYPE_DIRECT, 0, &queue) == SPUD_SUCCESS);
	spudgpu_command_allocator_desc allocator_desc;
	memset(&allocator_desc, 0, sizeof(allocator_desc));
	allocator_desc.base.type            = SPUD_STRUCT_TYPE_GPU_COMMAND_ALLOCATOR_DESC;
	allocator_desc.type                 = SPUDGPU_COMMAND_LIST_TYPE_DIRECT;
	spudgpu_command_allocator allocator = NULL;
	CHECK(spudgpu_create_command_allocator(device, &allocator_desc, &allocator) == SPUD_SUCCESS);
	spudgpu_command_list cmd = NULL;
	if (allocator)
		CHECK(spudgpu_create_command_list(allocator, &cmd) == SPUD_SUCCESS);
	if (queue && cmd) {
		test_draw_frame(device, queue, allocator, cmd);
		test_compute(device, queue, allocator, cmd);
	}
	spudgpu_destroy_command_list(cmd);
	spudgpu_destroy_command_allocator(allocator);
	spudgpu_destroy_device(device);
}

// ---- A swap chain's frames -----------------------------------------------------

#if defined(_WIN32)
static LRESULT CALLBACK hidden_window_proc(HWND window, UINT message, WPARAM w, LPARAM l) {
	return DefWindowProcW(window, message, w, l);
}

// A window that is never shown: a surface needs one, nobody needs to see it.
static HWND hidden_window_create(void) {
	WNDCLASSEXW window_class;
	memset(&window_class, 0, sizeof(window_class));
	window_class.cbSize        = sizeof(window_class);
	window_class.lpfnWndProc   = hidden_window_proc;
	window_class.hInstance     = GetModuleHandleW(NULL);
	window_class.lpszClassName = L"spudgpu_objects_test";
	RegisterClassExW(&window_class);
	RECT rect = {0, 0, 64, 64};
	AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
	return CreateWindowExW(0, L"spudgpu_objects_test", L"spudgpu_objects_test", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left,
	                       rect.bottom - rect.top, NULL, NULL, GetModuleHandleW(NULL), NULL);
}

static void hidden_window_destroy(HWND window) {
	DestroyWindow(window);
	MSG message;
	while (PeekMessageW(&message, NULL, 0, 0, PM_REMOVE)) {
		TranslateMessage(&message);
		DispatchMessageW(&message);
	}
	UnregisterClassW(L"spudgpu_objects_test", GetModuleHandleW(NULL));
}
#endif

static bool callback_refuses(void *vk_instance, void *user_data, void *out_surface) {
	(void)vk_instance;
	(void)out_surface;
	++*(int *)user_data;
	return false;
}

#if SPUDGPU_COMPILE_VULKAN_API && defined(_WIN32)
// What the callback needs of Vulkan, declared here so that the test needs no
// Vulkan headers: vkCreateWin32SurfaceKHR, found through the loader.
typedef struct test_win32_surface_info {
	uint32_t type; // VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR
	const void *next;
	uint32_t flags;
	HINSTANCE instance;
	HWND window;
} test_win32_surface_info;
typedef void (*test_vk_void_fn)(void);
typedef test_vk_void_fn (*test_get_instance_proc_fn)(void *instance, const char *name);
typedef int32_t (*test_create_win32_surface_fn)(void *instance, const test_win32_surface_info *info, const void *allocator, uint64_t *surface);

typedef struct callback_window {
	HWND window;
	int calls;
	void *instance_seen;
} callback_window;

static bool callback_makes_win32_surface(void *vk_instance, void *user_data, void *out_surface) {
	callback_window *w = (callback_window *)user_data;
	++w->calls;
	w->instance_seen = vk_instance;
	HMODULE loader   = GetModuleHandleW(L"vulkan-1.dll");
	if (!loader)
		return false;
	test_get_instance_proc_fn get = (test_get_instance_proc_fn)(void *)GetProcAddress(loader, "vkGetInstanceProcAddr");
	if (!get)
		return false;
	test_create_win32_surface_fn create = (test_create_win32_surface_fn)(void *)get(vk_instance, "vkCreateWin32SurfaceKHR");
	if (!create)
		return false;
	test_win32_surface_info info = {1000009000u, NULL, 0, GetModuleHandleW(NULL), w->window};
	return create(vk_instance, &info, NULL, (uint64_t *)out_surface) == 0;
}
#endif

// Three frames into a swap chain on [surface]: acquired, cleared, submitted
// with spudgpu_submit_command_lists_synced() and presented.
static void present_frames(spudgpu_device device, spudgpu_surface surface, const char *what) {
	spudgpu_swap_chain_desc desc = swap_chain_desc_for(device, surface, SPUDGPU_PRESENT_SCALING_ONE_TO_ONE);
#if !SPUDGPU_COMPILE_METAL_API
	desc.buffer_count = 2;
#endif
	spudgpu_swap_chain swap_chain = NULL;
	CHECK(spudgpu_create_swap_chain(device, &desc, &swap_chain) == SPUD_SUCCESS);
	if (!swap_chain)
		return;
	spudgpu_swap_chain_desc created;
	memset(&created, 0, sizeof(created));
	created.base.type = SPUD_STRUCT_TYPE_GPU_SWAP_CHAIN_DESC;
	CHECK(spudgpu_get_swap_chain_desc(swap_chain, &created) == SPUD_SUCCESS);

	spudgpu_semaphore available = spudgpu_swap_chain_get_image_available_semaphore(swap_chain);
	spudgpu_semaphore finished  = spudgpu_swap_chain_get_render_finished_semaphore(swap_chain);
	CHECK(spudgpu_swap_chain_get_image_available_semaphore(NULL) == NULL);
	CHECK(spudgpu_swap_chain_get_render_finished_semaphore(NULL) == NULL);
#if SPUDGPU_COMPILE_D3D12_API
	// Not implemented by D3D12, which synchronizes a present by a fence of
	// its own: NULL, as spudgpu.h says.
	CHECK(available == NULL && finished == NULL);
#elif SPUDGPU_COMPILE_VULKAN_API
	CHECK(available != NULL && finished != NULL && available != finished);
	CHECK(spudgpu_swap_chain_get_image_available_semaphore(swap_chain) == available);
	CHECK(spudgpu_swap_chain_get_render_finished_semaphore(swap_chain) == finished);
#else
	(void)available;
	(void)finished;
#endif

	spudgpu_command_allocator_desc allocator_desc;
	memset(&allocator_desc, 0, sizeof(allocator_desc));
	allocator_desc.base.type            = SPUD_STRUCT_TYPE_GPU_COMMAND_ALLOCATOR_DESC;
	allocator_desc.type                 = SPUDGPU_COMMAND_LIST_TYPE_DIRECT;
	spudgpu_command_allocator allocator = NULL;
	CHECK(spudgpu_create_command_allocator(device, &allocator_desc, &allocator) == SPUD_SUCCESS);
	spudgpu_command_list cmd = NULL;
	if (allocator)
		CHECK(spudgpu_create_command_list(allocator, &cmd) == SPUD_SUCCESS);
	CHECK(spudgpu_get_swap_chain_image_view(NULL, 0) == NULL);
	CHECK(spudgpu_get_swap_chain_image_view(swap_chain, created.buffer_count) == NULL);
	int presented = 0;
	for (int frame = 0; cmd && frame < 3; ++frame) {
		const uint32_t index = spudgpu_swap_chain_acquire_next_image(swap_chain);
		CHECK(index < created.buffer_count);
		spudgpu_image_view view = spudgpu_get_swap_chain_image_view(swap_chain, index);
		CHECK(view != NULL);
		if (!view)
			break;
		CHECK(spudgpu_reset_command_allocator(allocator) == SPUD_SUCCESS);
		spudgpu_begin_command_list(cmd);
		spudgpu_cmd_image_barrier_view(cmd, view, SPUDGPU_IMAGE_LAYOUT_UNDEFINED, SPUDGPU_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		spudgpu_rendering_begin_desc pass;
		memset(&pass, 0, sizeof(pass));
		pass.base.type                       = SPUD_STRUCT_TYPE_GPU_RENDERING_BEGIN_DESC;
		pass.color_attachments[0].image_view = view;
		pass.color_attachments[0].load_op    = SPUDGPU_LOAD_OP_CLEAR;
		pass.color_attachments[0].store_op   = SPUDGPU_STORE_OP_STORE;
		pass.color_attachment_count          = 1;
		pass.width                           = created.width;
		pass.height                          = created.height;
		spudgpu_cmd_begin_rendering(cmd, &pass);
		spudgpu_cmd_end_rendering(cmd);
		spudgpu_cmd_image_barrier_view(cmd, view, SPUDGPU_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, SPUDGPU_IMAGE_LAYOUT_PRESENT_SRC);
		spudgpu_end_command_list(cmd);
		CHECK(spudgpu_submit_command_lists_synced(NULL, &cmd, 1, swap_chain) == SPUDRESULT_GPU_INVALID_COMMAND_QUEUE);
		CHECK(spudgpu_submit_command_lists_synced(desc.queue, &cmd, 1, NULL) == SPUDRESULT_GPU_INVALID_SWAP_CHAIN);
		CHECK(spudgpu_submit_command_lists_synced(desc.queue, &cmd, 1, swap_chain) == SPUD_SUCCESS);
		spudgpu_swap_chain_present(swap_chain);
		++presented;
	}
	CHECK(presented == 3);
	printf("spudgpu_objects_test: %d frames presented to %s\n", presented, what);
	spudgpu_queue_wait_idle(desc.queue);
	spudgpu_destroy_command_list(cmd);
	spudgpu_destroy_command_allocator(allocator);
	spudgpu_destroy_swap_chain(swap_chain);
}

// A swap chain's frames on a real surface, where this test can make one
// without showing anything; and spudgpu_create_surface_from_callback().
static void test_swap_chain_frames(spudgpu_instance instance, spudgpu_device device) {
	int refusals = 0;
	CHECK(spudgpu_create_surface_from_callback(NULL, &refusals, callback_refuses) == NULL);
	CHECK(spudgpu_create_surface_from_callback(instance, &refusals, NULL) == NULL);
	CHECK(spudgpu_create_surface_from_callback(instance, &refusals, callback_refuses) == NULL);
#if SPUDGPU_COMPILE_VULKAN_API
	CHECK(refusals == 1); // called once, and its refusal is the call's
#else
	CHECK(refusals == 0); // not implemented: the callback is never called
#endif
#if defined(_WIN32)
	HWND window = hidden_window_create();
	CHECK(window != NULL);
	if (!window)
		return;
	spudgpu_surface surface = NULL;
	CHECK(spudgpu_create_surface(instance, window, NULL, &surface) == SPUD_SUCCESS);
	if (surface) {
		present_frames(device, surface, "a hidden window");
		spudgpu_destroy_surface(surface);
	}
#if SPUDGPU_COMPILE_VULKAN_API
	callback_window w             = {window, 0, NULL};
	spudgpu_surface from_callback = spudgpu_create_surface_from_callback(instance, &w, callback_makes_win32_surface);
	CHECK(from_callback != NULL);
	CHECK(w.calls == 1 && w.instance_seen != NULL);
	if (from_callback) {
		present_frames(device, from_callback, "a surface the callback made");
		spudgpu_destroy_surface(from_callback);
	}
#endif
	hidden_window_destroy(window);
#elif SPUDGPU_COMPILE_METAL_API
	void *layer             = spudgpu_test_metal_layer_create(false);
	spudgpu_surface surface = NULL;
	CHECK(spudgpu_create_surface(instance, layer, NULL, &surface) == SPUD_SUCCESS);
	if (surface) {
		present_frames(device, surface, "a CAMetalLayer");
		spudgpu_destroy_surface(surface);
	}
	spudgpu_test_metal_layer_destroy(layer);
#else
	(void)device;
	printf("spudgpu_objects_test: no surface without a display here, no swap chain frames\n");
#endif
}

static void test_queries(spudgpu_instance instance) {
	CHECK(spudgpu_get_native_gpu_api(NULL) == SPUDGPU_NATIVE_API_NONE);
#if SPUDGPU_COMPILE_VULKAN_API
	CHECK(spudgpu_get_native_gpu_api(instance) == SPUDGPU_NATIVE_API_VULKAN);
#elif SPUDGPU_COMPILE_D3D12_API
	CHECK(spudgpu_get_native_gpu_api(instance) == SPUDGPU_NATIVE_API_D3D12);
#elif SPUDGPU_COMPILE_METAL_API
	CHECK(spudgpu_get_native_gpu_api(instance) == SPUDGPU_NATIVE_API_METAL);
#endif
	CHECK(spudgpu_format_bit_count(SPUDGPU_FORMAT_UNKNOWN) == 0);
	CHECK(spudgpu_format_bit_count(SPUDGPU_FORMAT_R8G8B8A8_UNORM) == 32);
	CHECK(spudgpu_format_bit_count(SPUDGPU_FORMAT_B8G8R8A8_UNORM) == 32);
	CHECK(spudgpu_format_bit_count(SPUDGPU_FORMAT_R32G32_FLOAT) == 64);
	CHECK(spudgpu_format_bit_count(SPUDGPU_FORMAT_R32G32B32A32_FLOAT) == 128);
	CHECK(spudgpu_format_bit_count(SPUDGPU_FORMAT_R16_UINT) == 16);
	CHECK(spudgpu_format_bit_count(SPUDGPU_FORMAT_D32_FLOAT) == 32);
}

int main(int argc, char **argv) {
	uint32_t adapter_index = 0;
	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "--adapter") == 0 && i + 1 < argc) {
			adapter_index = (uint32_t)strtoul(argv[++i], NULL, 10);
		} else {
			fprintf(stderr, "usage: spudgpu_objects_test [--adapter N]\n");
			return 2;
		}
	}
	test_format_from_string();

	spudtest_allocator tracker;
	SPUD_ALLOCATOR allocator = spudtest_allocator_make(&tracker);
	spudgpu_instance_desc instance_desc;
	memset(&instance_desc, 0, sizeof(instance_desc));
	instance_desc.base.type        = SPUD_STRUCT_TYPE_GPU_INSTANCE_DESC;
	instance_desc.allocator        = &allocator;
	instance_desc.application_name = "spudgpu_objects_test";
	instance_desc.engine_name      = "spudgpu_objects_test";
	spudgpu_instance instance      = NULL;
	if (spudgpu_create_instance(&instance_desc, &instance) != SPUD_SUCCESS) {
		fprintf(stderr, "spudgpu_objects_test: no instance\n");
		return 1;
	}
	const spudgpu_adapter *adapters = NULL;
	uint32_t adapter_count          = 0;
	spudgpu_enumerate_adapters(instance, &adapters, &adapter_count);
	if (adapter_count == 0) {
		printf("spudgpu_objects_test: no adapter, skipped\n");
		spudgpu_destroy_instance(instance);
		return SKIPPED;
	}
	if (adapter_index >= adapter_count) {
		fprintf(stderr, "spudgpu_objects_test: no adapter %u (%u listed)\n", adapter_index, adapter_count);
		spudgpu_destroy_instance(instance);
		return 2;
	}
	const spudgpu_adapter adapter = adapters[adapter_index];
	test_queries(instance);

	spudgpu_device_desc device_desc;
	memset(&device_desc, 0, sizeof(device_desc));
	device_desc.base.type          = SPUD_STRUCT_TYPE_GPU_DEVICE_DESC;
	device_desc.direct_queue_count = 1;
	device_desc.max_descriptors    = 16;
	device_desc.max_samplers       = 4;
	device_desc.debug_name         = "spudgpu_objects_test";
	spudgpu_device device          = NULL;
	CHECK(spudgpu_create_device(adapter, &device_desc, &device) == SPUD_SUCCESS);
	if (device) {
		test_fence_signal(device);
		test_semaphore(device);
		test_buffer_and_view(device);
		test_heap_desc(device);
		test_descriptor_pool_reset(device);
		test_present_scaling_without_present(device);
		spudgpu_destroy_device(device);
	}

	SPUDGPU_ADAPTER_PROPERTIES properties;
	memset(&properties, 0, sizeof(properties));
	properties.base.type = SPUD_STRUCT_TYPE_GPU_ADAPTER_PROPERTIES;
	CHECK(spudgpu_get_adapter_properties(adapter, &properties) == SPUD_SUCCESS);
	printf("spudgpu_objects_test: adapter %u: %s\n", adapter_index, properties.description);
	test_drawing(adapter, properties.supported_features);
	if (properties.supported_features & SPUDGPU_FEATURE_PRESENT) {
		device_desc.required_features = SPUDGPU_FEATURE_PRESENT;
		device_desc.debug_name        = "spudgpu_objects_test present";
		spudgpu_device present_device = NULL;
		CHECK(spudgpu_create_device(adapter, &device_desc, &present_device) == SPUD_SUCCESS);
		if (present_device) {
			test_present_scaling_checks(present_device);
#if SPUDGPU_COMPILE_METAL_API
			test_present_scaling_metal(instance, present_device);
#endif
			test_swap_chain_frames(instance, present_device);
			spudgpu_destroy_device(present_device);
		}
	} else {
		printf("spudgpu_objects_test: the adapter can't present, present scaling checks and swap chain frames skipped\n");
	}
	spudgpu_destroy_instance(instance);
	CHECK(spudtest_allocator_clean(&tracker));

	if (failures) {
		fprintf(stderr, "spudgpu_objects_test: %d failure(s)\n", failures);
		return 1;
	}
	printf("spudgpu_objects_test: all passed\n");
	return 0;
}
