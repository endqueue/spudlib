/*
 * spudgpu_objects_test — SpudGPU calls nothing else in the workspace makes.
 *
 * On a device of adapter 0, with no window and nothing drawn: a format looked
 * up by name; a fence signalled from the CPU and waited on; a semaphore made
 * and destroyed; a buffer's desc and GPU address read back; a buffer view's
 * desc; a heap's desc; and a descriptor pool reset and allocated from again.
 * Each with its argument checks. Every block of the instance goes through a
 * counting allocator, checked at the end.
 *
 * Not here, because they can't run without a window or on every backend:
 * spudgpu_create_surface_from_callback (a window), and the storage-buffer and
 * storage-image bindless calls and spudgpu_cmd_bind_bindless_resources_compute
 * (SPUDGPU_EXT_BINDLESS, 0 on Metal). spudgpu_get_compute_pipeline_desc needs
 * a compute shader, which this test has none of.
 *
 * Exits 0 when every check passes, 1 when one fails, 77 (skipped) with no
 * adapter.
 */

#include <spudgpu.h>

#include "spudtest_allocator.h"

#include <stdio.h>
#include <string.h>

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

int main(void) {
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

	spudgpu_device_desc device_desc;
	memset(&device_desc, 0, sizeof(device_desc));
	device_desc.base.type          = SPUD_STRUCT_TYPE_GPU_DEVICE_DESC;
	device_desc.direct_queue_count = 1;
	device_desc.max_descriptors    = 16;
	device_desc.max_samplers       = 4;
	device_desc.debug_name         = "spudgpu_objects_test";
	spudgpu_device device          = NULL;
	CHECK(spudgpu_create_device(adapters[0], &device_desc, &device) == SPUD_SUCCESS);
	if (device) {
		test_fence_signal(device);
		test_semaphore(device);
		test_buffer_and_view(device);
		test_heap_desc(device);
		test_descriptor_pool_reset(device);
		spudgpu_destroy_device(device);
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
