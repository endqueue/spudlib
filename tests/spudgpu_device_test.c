/*
 * spudgpu_device_test — SpudGPU's adapters and devices.
 *
 * Lists the adapters, reads the first one's properties (chained structs
 * included), and creates and destroys devices on it: the argument checks of
 * every call, a device's queues by kind, features asked for and not, the
 * descriptor budget, one device per adapter, and a device made again after
 * one is destroyed. Everything goes through a checking allocator, which must
 * end with nothing left.
 *
 * Needs a GPU but no window, nobody watching and nothing drawn, so it is a
 * ctest; with no adapter it exits 77, which ctest reports as skipped.
 *
 * Exits 0 when every check passes, 1 when one fails.
 */

#include <spudgpu.h>

#include "spudtest_allocator.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond)                                                              \
	do {                                                                         \
		if (!(cond)) {                                                           \
			fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
			++failures;                                                          \
		}                                                                        \
	} while (0)

#define SKIPPED 77

static SPUDGPU_ADAPTER_PROPERTIES properties_of(spudgpu_adapter adapter) {
	SPUDGPU_ADAPTER_PROPERTIES properties;
	memset(&properties, 0, sizeof(properties));
	properties.base.type = SPUD_STRUCT_TYPE_GPU_ADAPTER_PROPERTIES;
	CHECK(spudgpu_get_adapter_properties(adapter, &properties) == SPUD_SUCCESS);
	return properties;
}

static spudgpu_device_desc device_desc(void) {
	spudgpu_device_desc desc;
	memset(&desc, 0, sizeof(desc));
	desc.base.type          = SPUD_STRUCT_TYPE_GPU_DEVICE_DESC;
	desc.direct_queue_count = 1;
	desc.max_descriptors    = 16;
	desc.max_samplers       = 4;
	desc.debug_name         = "spudgpu_device_test";
	return desc;
}

static void test_enumerate(spudgpu_instance instance) {
	const spudgpu_adapter *adapters = NULL;
	uint32_t count                  = 0;
	CHECK(spudgpu_enumerate_adapters(NULL, &adapters, &count) == SPUDRESULT_GPU_INVALID_INSTANCE);
	CHECK(spudgpu_enumerate_adapters(instance, NULL, &count) == SPUDRESULT_NULL_OUTPUT_PARAMETER);
	CHECK(spudgpu_enumerate_adapters(instance, &adapters, NULL) == SPUDRESULT_NULL_OUTPUT_PARAMETER);

	// The list is read once: every call hands out the same array.
	const spudgpu_adapter *again = NULL;
	uint32_t again_count         = 0;
	CHECK(spudgpu_enumerate_adapters(instance, &adapters, &count) == SPUD_SUCCESS);
	CHECK(spudgpu_enumerate_adapters(instance, &again, &again_count) == SPUD_SUCCESS);
	CHECK(adapters == again);
	CHECK(count == again_count);
}

static void test_adapter_properties(spudgpu_adapter adapter) {
	SPUDGPU_ADAPTER_PROPERTIES properties;
	memset(&properties, 0, sizeof(properties));
	properties.base.type = SPUD_STRUCT_TYPE_GPU_ADAPTER_PROPERTIES;
	CHECK(spudgpu_get_adapter_properties(NULL, &properties) == SPUDRESULT_GPU_INVALID_ADAPTER);
	CHECK(spudgpu_get_adapter_properties(adapter, NULL) == SPUDRESULT_NULL_OUTPUT_PARAMETER);

	properties.base.type = SPUD_STRUCT_TYPE_GPU_DEVICE_DESC;
	CHECK(spudgpu_get_adapter_properties(adapter, &properties) == SPUDRESULT_STRUCT_WRONG_TYPE);
	properties.base.type = SPUD_STRUCT_TYPE_GPU_ADAPTER_PROPERTIES;

	// A struct the call doesn't take there, anywhere in the chain, refuses
	// the whole call and writes nothing.
	SPUD_STRUCT_BASE_OUT stranger = {SPUD_STRUCT_TYPE_GPU_PLACEMENT_REQUIREMENTS, NULL};
	properties.base.next          = &stranger;
	properties.description[0]     = '\x7f';
	CHECK(spudgpu_get_adapter_properties(adapter, &properties) == SPUDRESULT_STRUCT_UNKNOWN_IN_CHAIN);
	CHECK(properties.description[0] == '\x7f');
	properties.base.next = NULL;

#if SPUDGPU_EXT_MESH_SHADING
	SPUDGPU_MESH_SHADING_LIMITS mesh_a, mesh_b;
	memset(&mesh_a, 0, sizeof(mesh_a));
	memset(&mesh_b, 0, sizeof(mesh_b));
	mesh_a.base.type     = SPUD_STRUCT_TYPE_GPU_MESH_SHADING_LIMITS;
	mesh_b.base.type     = SPUD_STRUCT_TYPE_GPU_MESH_SHADING_LIMITS;
	mesh_a.base.next     = &mesh_b.base;
	properties.base.next = &mesh_a.base;
	CHECK(spudgpu_get_adapter_properties(adapter, &properties) == SPUDRESULT_STRUCT_DUPLICATE_IN_CHAIN);

	// One limits struct: filled in, and the chain left as the caller built it.
	mesh_a.base.next = NULL;
	CHECK(spudgpu_get_adapter_properties(adapter, &properties) == SPUD_SUCCESS);
	CHECK(properties.base.type == SPUD_STRUCT_TYPE_GPU_ADAPTER_PROPERTIES);
	CHECK(properties.base.next == &mesh_a.base);
	CHECK(mesh_a.base.type == SPUD_STRUCT_TYPE_GPU_MESH_SHADING_LIMITS);
	CHECK(mesh_a.base.next == NULL);
	if (properties.supported_features & SPUDGPU_FEATURE_MESH_SHADING) {
		CHECK(mesh_a.max_mesh_output_vertices > 0);
		CHECK(mesh_a.max_mesh_workgroup_invocations > 0);
	} else {
		CHECK(mesh_a.max_mesh_output_vertices == 0);
	}
	properties.base.next = NULL;
#endif

	properties = properties_of(adapter);
	printf("adapter: %s, vendor 0x%04x, type %u, features 0x%llx, %u queue pools\n", properties.description,
	    (unsigned)properties.vendor_id, (unsigned)properties.adapter_type,
	    (unsigned long long)properties.supported_features, (unsigned)properties.queue_pool_count);
	CHECK(properties.description[0] != '\0');
	CHECK(properties.adapter_type <= SPUDGPU_ADAPTER_TYPE_SOFTWARE);

	// Some byte of the id says which GPU.
	bool id_nonzero = false;
	for (size_t i = 0; i < sizeof(properties.adapter_id); ++i)
		id_nonzero = id_nonzero || properties.adapter_id[i] != 0;
	CHECK(id_nonzero);

	// Every kind is in exactly one pool, and no pool is empty.
	CHECK(properties.queue_pool_count >= 1 && properties.queue_pool_count <= 3);
	SPUDGPU_QUEUE_KINDS seen = 0;
	for (uint32_t i = 0; i < properties.queue_pool_count && i < 3; ++i) {
		CHECK(properties.queue_pools[i].kinds != 0);
		CHECK((seen & properties.queue_pools[i].kinds) == 0);
		CHECK(properties.queue_pools[i].max_queues > 0);
		seen |= properties.queue_pools[i].kinds;
	}
	CHECK(seen == (SPUDGPU_QUEUE_KIND_DIRECT | SPUDGPU_QUEUE_KIND_COMPUTE | SPUDGPU_QUEUE_KIND_COPY));

	CHECK(properties.uniform_buffer_offset_alignment > 0);
	CHECK((properties.uniform_buffer_offset_alignment & (properties.uniform_buffer_offset_alignment - 1)) == 0);
	CHECK(properties.storage_buffer_offset_alignment > 0);
	CHECK(properties.max_draw_indirect_count >= 1);
	CHECK(properties.max_descriptors > 0);
	CHECK(properties.max_samplers > 0);
#if !SPUDGPU_EXT_BINDLESS
	CHECK(!(properties.supported_features & SPUDGPU_FEATURE_BINDLESS));
#endif
	// A device-mappable kind needs memory that is the system's.
	if (properties.supported_features & SPUDGPU_FEATURE_DEVICE_MAPPABLE_MEMORY)
		CHECK(properties.unified_memory);

	bool supported = true;
	CHECK(spudgpu_adapter_supports_surface(NULL, NULL, &supported) == SPUDRESULT_GPU_INVALID_ADAPTER);
	CHECK(spudgpu_adapter_supports_surface(adapter, NULL, &supported) == SPUDRESULT_GPU_INVALID_SURFACE);
}

static void test_device_desc_checks(spudgpu_adapter adapter) {
	const SPUDGPU_ADAPTER_PROPERTIES properties = properties_of(adapter);
	spudgpu_device device                       = (spudgpu_device)(uintptr_t)1;
	spudgpu_device_desc desc                    = device_desc();

	CHECK(spudgpu_create_device(NULL, &desc, &device) == SPUDRESULT_GPU_INVALID_ADAPTER);
	CHECK(spudgpu_create_device(adapter, NULL, &device) == SPUDRESULT_NULL_DESC);
	desc.base.type = SPUD_STRUCT_TYPE_GPU_INSTANCE_DESC;
	CHECK(spudgpu_create_device(adapter, &desc, &device) == SPUDRESULT_STRUCT_WRONG_TYPE);
	desc = device_desc();
	CHECK(spudgpu_create_device(adapter, &desc, NULL) == SPUDRESULT_NULL_OUTPUT_PARAMETER);

	// Refused from here: the output is NULL.
	device                 = (spudgpu_device)(uintptr_t)1;
	desc.required_features = UINT64_C(1) << 63;
	CHECK(spudgpu_create_device(adapter, &desc, &device) == SPUDRESULT_GPU_FEATURE_NOT_SUPPORTED);
	CHECK(device == NULL);
	desc = device_desc();

	desc.direct_queue_count = 0;
	CHECK(spudgpu_create_device(adapter, &desc, &device) == SPUDRESULT_ZERO_SIZE);
	desc = device_desc();

	for (uint32_t i = 0; i < properties.queue_pool_count; ++i) {
		const SPUDGPU_QUEUE_POOL *pool = &properties.queue_pools[i];
		if (pool->max_queues == UINT32_MAX)
			continue;
		spudgpu_device_desc too_many = device_desc();
		too_many.direct_queue_count  = 0;
		if (pool->kinds & SPUDGPU_QUEUE_KIND_DIRECT)
			too_many.direct_queue_count = pool->max_queues + 1;
		else if (pool->kinds & SPUDGPU_QUEUE_KIND_COMPUTE)
			too_many.compute_queue_count = pool->max_queues + 1;
		else
			too_many.copy_queue_count = pool->max_queues + 1;
		CHECK(spudgpu_create_device(adapter, &too_many, &device) == SPUDRESULT_GPU_TOO_MANY_QUEUES);
	}

	if (properties.max_descriptors < UINT32_MAX) {
		desc.max_descriptors = properties.max_descriptors + 1;
		CHECK(spudgpu_create_device(adapter, &desc, &device) == SPUDRESULT_GPU_DESCRIPTOR_BUDGET_TOO_LARGE);
		desc = device_desc();
	}
	if (properties.max_samplers < UINT32_MAX) {
		desc.max_samplers = properties.max_samplers + 1;
		CHECK(spudgpu_create_device(adapter, &desc, &device) == SPUDRESULT_GPU_DESCRIPTOR_BUDGET_TOO_LARGE);
		desc = device_desc();
	}

#if SPUDGPU_EXT_BINDLESS
	if (properties.supported_features & SPUDGPU_FEATURE_BINDLESS) {
		// The table comes out of the budget, which 16 doesn't hold.
		desc.required_features = SPUDGPU_FEATURE_BINDLESS;
		CHECK(spudgpu_create_device(adapter, &desc, &device) == SPUDRESULT_GPU_OUT_OF_DESCRIPTORS);
		desc = device_desc();
	}
#endif
}

static void test_queues(spudgpu_device device) {
	spudgpu_command_queue queue = NULL;
	spudgpu_command_queue again = NULL;
	CHECK(spudgpu_get_command_queue(NULL, SPUDGPU_COMMAND_LIST_TYPE_DIRECT, 0, &queue) == SPUDRESULT_GPU_INVALID_DEVICE);
	CHECK(spudgpu_get_command_queue(device, SPUDGPU_COMMAND_LIST_TYPE_BUNDLE, 0, &queue) ==
	      SPUDRESULT_GPU_INVALID_COMMAND_LIST_TYPE);
	CHECK(spudgpu_get_command_queue(device, 99, 0, &queue) == SPUDRESULT_GPU_INVALID_COMMAND_LIST_TYPE);
	CHECK(spudgpu_get_command_queue(device, SPUDGPU_COMMAND_LIST_TYPE_DIRECT, 0, NULL) == SPUDRESULT_NULL_OUTPUT_PARAMETER);

	// Two direct queues, one compute, no copy queue.
	CHECK(spudgpu_get_command_queue(device, SPUDGPU_COMMAND_LIST_TYPE_DIRECT, 0, &queue) == SPUD_SUCCESS);
	CHECK(queue != NULL);
	CHECK(spudgpu_get_command_queue(device, SPUDGPU_COMMAND_LIST_TYPE_DIRECT, 0, &again) == SPUD_SUCCESS);
	CHECK(queue == again);
	CHECK(spudgpu_get_command_queue(device, SPUDGPU_COMMAND_LIST_TYPE_DIRECT, 1, &again) == SPUD_SUCCESS);
	CHECK(again != NULL && again != queue);
	CHECK(spudgpu_get_command_queue(device, SPUDGPU_COMMAND_LIST_TYPE_DIRECT, 2, &again) == SPUDRESULT_INDEX_OUT_OF_RANGE);
	CHECK(spudgpu_get_command_queue(device, SPUDGPU_COMMAND_LIST_TYPE_COMPUTE, 0, &again) == SPUD_SUCCESS);
	CHECK(again != NULL);
	CHECK(spudgpu_get_command_queue(device, SPUDGPU_COMMAND_LIST_TYPE_COPY, 0, &again) == SPUDRESULT_INDEX_OUT_OF_RANGE);

	// An allocator of a kind the device has no queue of is refused; a bundle
	// allocator needs a direct queue.
	spudgpu_command_allocator_desc allocator_desc;
	memset(&allocator_desc, 0, sizeof(allocator_desc));
	allocator_desc.base.type = SPUD_STRUCT_TYPE_GPU_COMMAND_ALLOCATOR_DESC;
	allocator_desc.type      = SPUDGPU_COMMAND_LIST_TYPE_COPY;
	spudgpu_command_allocator allocator = NULL;
	CHECK(spudgpu_create_command_allocator(device, &allocator_desc, &allocator) == SPUDRESULT_GPU_INVALID_COMMAND_LIST_TYPE);
	allocator_desc.type = SPUDGPU_COMMAND_LIST_TYPE_DIRECT;
	CHECK(spudgpu_create_command_allocator(device, &allocator_desc, &allocator) == SPUD_SUCCESS);
	spudgpu_command_list list = NULL;
	CHECK(spudgpu_create_command_list(allocator, &list) == SPUD_SUCCESS);
	spudgpu_destroy_command_list(list);
	spudgpu_destroy_command_allocator(allocator);
}

static spudgpu_descriptor_pool make_pool(spudgpu_device device, SPUDGPU_DESCRIPTOR_TYPE type, uint32_t count, SPUDRESULT *out_result) {
	spudgpu_descriptor_pool_desc desc;
	memset(&desc, 0, sizeof(desc));
	desc.base.type                  = SPUD_STRUCT_TYPE_GPU_DESCRIPTOR_POOL_DESC;
	desc.max_sets                   = 1;
	desc.pool_size_count            = 1;
	desc.pool_sizes[0].descriptor_type = type;
	desc.pool_sizes[0].count        = count;
	spudgpu_descriptor_pool pool    = NULL;
	*out_result                     = spudgpu_create_descriptor_pool(device, &desc, &pool);
	return pool;
}

static void test_budget(spudgpu_device device) {
	// The device's budget is 16 descriptors and 4 samplers.
	SPUDRESULT result;
	spudgpu_descriptor_pool first = make_pool(device, SPUDGPU_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 12, &result);
	CHECK(result == SPUD_SUCCESS);
	make_pool(device, SPUDGPU_DESCRIPTOR_TYPE_STORAGE_BUFFER, 5, &result);
	CHECK(result == SPUDRESULT_GPU_OUT_OF_DESCRIPTORS);
	spudgpu_descriptor_pool second = make_pool(device, SPUDGPU_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4, &result);
	CHECK(result == SPUD_SUCCESS);
	make_pool(device, SPUDGPU_DESCRIPTOR_TYPE_SAMPLER, 5, &result);
	CHECK(result == SPUDRESULT_GPU_OUT_OF_DESCRIPTORS);
	spudgpu_descriptor_pool samplers = make_pool(device, SPUDGPU_DESCRIPTOR_TYPE_SAMPLER, 4, &result);
	CHECK(result == SPUD_SUCCESS);

	// What a destroyed pool held is given back.
	spudgpu_destroy_descriptor_pool(first);
	spudgpu_descriptor_pool third = make_pool(device, SPUDGPU_DESCRIPTOR_TYPE_STORAGE_BUFFER, 12, &result);
	CHECK(result == SPUD_SUCCESS);

	spudgpu_destroy_descriptor_pool(third);
	spudgpu_destroy_descriptor_pool(samplers);
	spudgpu_destroy_descriptor_pool(second);
}

static void test_features(spudgpu_adapter adapter, spudgpu_device device) {
	const SPUDGPU_ADAPTER_PROPERTIES properties = properties_of(adapter);
	CHECK(spudgpu_get_device_adapter(device) == adapter);
	CHECK(spudgpu_get_device_adapter(NULL) == NULL);
	CHECK(spudgpu_get_device_features(NULL) == 0);

	// The device has exactly what it was created with: here, nothing. A
	// feature the adapter has is still refused.
	CHECK(spudgpu_get_device_features(device) == 0);
	if (properties.supported_features & SPUDGPU_FEATURE_PASS_TIMESTAMPS) {
		spudgpu_command_queue queue = NULL;
		CHECK(spudgpu_get_command_queue(device, SPUDGPU_COMMAND_LIST_TYPE_DIRECT, 0, &queue) == SPUD_SUCCESS);
		spudgpu_timestamp_set set = NULL;
		CHECK(spudgpu_create_timestamp_set(device, queue, 2, &set) == SPUDRESULT_GPU_TIMESTAMPS_NOT_SUPPORTED);
	}
}

static void test_timestamps_when_asked(spudgpu_adapter adapter) {
	const SPUDGPU_ADAPTER_PROPERTIES properties = properties_of(adapter);
	if (!(properties.supported_features & SPUDGPU_FEATURE_PASS_TIMESTAMPS))
		return;
	spudgpu_device_desc desc = device_desc();
	desc.required_features   = SPUDGPU_FEATURE_PASS_TIMESTAMPS;
	spudgpu_device device    = NULL;
	CHECK(spudgpu_create_device(adapter, &desc, &device) == SPUD_SUCCESS);
	if (!device)
		return;
	CHECK(spudgpu_get_device_features(device) == SPUDGPU_FEATURE_PASS_TIMESTAMPS);
	spudgpu_command_queue queue = NULL;
	CHECK(spudgpu_get_command_queue(device, SPUDGPU_COMMAND_LIST_TYPE_DIRECT, 0, &queue) == SPUD_SUCCESS);
	spudgpu_timestamp_set set = NULL;
	CHECK(spudgpu_create_timestamp_set(device, queue, 2, &set) == SPUD_SUCCESS);
	spudgpu_destroy_timestamp_set(set);
	spudgpu_destroy_device(device);
}

int main(void) {
	spudtest_allocator tracker;
	SPUD_ALLOCATOR allocator = spudtest_allocator_make(&tracker);

	spudgpu_instance_desc instance_desc;
	memset(&instance_desc, 0, sizeof(instance_desc));
	instance_desc.base.type        = SPUD_STRUCT_TYPE_GPU_INSTANCE_DESC;
	instance_desc.allocator        = &allocator;
	instance_desc.application_name = "spudgpu_device_test";
	instance_desc.engine_name      = "spudgpu_device_test";
	spudgpu_instance instance      = NULL;
	if (spudgpu_create_instance(&instance_desc, &instance) != SPUD_SUCCESS) {
		fprintf(stderr, "spudgpu_device_test: no instance\n");
		return 1;
	}

	test_enumerate(instance);
	const spudgpu_adapter *adapters = NULL;
	uint32_t adapter_count          = 0;
	spudgpu_enumerate_adapters(instance, &adapters, &adapter_count);
	if (adapter_count == 0) {
		printf("spudgpu_device_test: no adapter, skipped\n");
		spudgpu_destroy_instance(instance);
		return SKIPPED;
	}
	const spudgpu_adapter adapter = adapters[0];

	test_adapter_properties(adapter);
	test_device_desc_checks(adapter);

	// A device of two direct queues and one compute queue, no copy queue,
	// no features, a budget of 16 descriptors and 4 samplers.
	const SPUDGPU_ADAPTER_PROPERTIES properties = properties_of(adapter);
	spudgpu_device_desc desc                    = device_desc();
	desc.direct_queue_count                     = 2;
	desc.compute_queue_count                    = 1;
	// Where compute shares the direct queues' pool, the pool must hold three.
	bool fits = true;
	for (uint32_t i = 0; i < properties.queue_pool_count; ++i) {
		const SPUDGPU_QUEUE_POOL *pool = &properties.queue_pools[i];
		uint64_t asked                 = 0;
		if (pool->kinds & SPUDGPU_QUEUE_KIND_DIRECT)
			asked += 2;
		if (pool->kinds & SPUDGPU_QUEUE_KIND_COMPUTE)
			asked += 1;
		fits = fits && asked <= pool->max_queues;
	}
	if (!fits) {
		printf("spudgpu_device_test: the adapter's pools can't hold 2 direct and 1 compute queue; skipped the queue tests\n");
		desc.direct_queue_count  = 1;
		desc.compute_queue_count = 0;
	}

	spudgpu_device device = NULL;
	CHECK(spudgpu_create_device(adapter, &desc, &device) == SPUD_SUCCESS);
	if (device) {
		// One device per adapter at a time.
		spudgpu_device second = (spudgpu_device)(uintptr_t)1;
		CHECK(spudgpu_create_device(adapter, &desc, &second) == SPUDRESULT_GPU_ADAPTER_IN_USE);
		CHECK(second == NULL);

		if (fits)
			test_queues(device);
		test_budget(device);
		test_features(adapter, device);
		spudgpu_destroy_device(device);
	}
	spudgpu_destroy_device(NULL);

	// The adapter is free again: a device is made on it as before, which is
	// also the way back from a lost device.
	test_timestamps_when_asked(adapter);
	device = NULL;
	CHECK(spudgpu_create_device(adapter, &desc, &device) == SPUD_SUCCESS);
	spudgpu_destroy_device(device);

	CHECK(spudgpu_destroy_instance(instance) == SPUD_SUCCESS);
	CHECK(spudtest_allocator_clean(&tracker));

	if (failures)
		fprintf(stderr, "spudgpu_device_test: %d checks failed\n", failures);
	else
		printf("spudgpu_device_test: all checks passed\n");
	return failures ? 1 : 0;
}
