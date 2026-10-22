//
// SpudGPU Metal backend - timestamp sets: an MTLCounterSampleBuffer of the
// timestamp counter set, sampled at a pass's stage boundaries
// (spudgpumetalrenderpass.m) and resolved on the CPU. Run on an Apple M5 Pro
// by a caller's GPU tests (2026-10-16).
//

#if SPUDGPU_COMPILE_METAL_API

#include "spudgpu.h"
#include "spudcore.h"
#include "spudgpumetal.h"
#include <Metal/Metal.h>
#include <stdio.h>
#include <stdlib.h>

id<MTLCounterSet> spudgpumetal___internal_timestamp_counter_set(id<MTLDevice> device) {
	// counterSets is autoreleased, and a caller in plain C may have no pool,
	// so the lookup drains a pool of its own. The array and its counter sets
	// are made for the call and freed with that pool, so the set found is
	// retained before it drains.
	id<MTLCounterSet> found = nil;
	@autoreleasepool {
		for (id<MTLCounterSet> counter_set in device.counterSets)
			if (!found && [counter_set.name isEqualToString:MTLCommonCounterSetTimestamp])
				found = [counter_set retain];
	}
	return found;
}

SPUDRESULT spudgpu_create_timestamp_set(
    spudgpu_device device,
    spudgpu_command_queue queue,
    uint32_t count,
    spudgpu_timestamp_set *out_set) {
	if (!device)
		return SPUDRESULT_GPU_INVALID_DEVICE;
	if (!queue)
		return SPUDRESULT_GPU_INVALID_COMMAND_QUEUE;
	if (count == 0)
		return SPUDRESULT_ZERO_SIZE;
	if (!out_set)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	spudgpu_device_metal *device_metal = (spudgpu_device_metal *)device;
	if (!(device_metal->_features & SPUDGPU_FEATURE_PASS_TIMESTAMPS))
		return SPUDRESULT_GPU_TIMESTAMPS_NOT_SUPPORTED;

	spudgpu_timestamp_set_metal *set = SPUD_NEW_ZEROED(&device_metal->head.allocator, spudgpu_timestamp_set_metal);
	if (!set)
		return SPUDRESULT_OUT_OF_MEMORY;
	set->head.allocator = device_metal->head.allocator;

	MTLCounterSampleBufferDescriptor *buffer_desc = [[MTLCounterSampleBufferDescriptor alloc] init];
	id<MTLCounterSet> counter_set                 = spudgpumetal___internal_timestamp_counter_set(device_metal->_device_mtl);
	buffer_desc.counterSet                        = counter_set;
	[counter_set release];
	buffer_desc.storageMode                       = MTLStorageModeShared;
	buffer_desc.sampleCount                       = count;
	NSError *error                                = nil;
	id<MTLCounterSampleBuffer> buffer = [device_metal->_device_mtl newCounterSampleBufferWithDescriptor:buffer_desc error:&error];
	[buffer_desc release];
	if (!buffer) {
		printf("spudgpu: MTLCounterSampleBuffer creation failed: %s\n", error ? error.localizedDescription.UTF8String : "(unknown)");
		SPUD_DELETE(&device_metal->head.allocator, spudgpu_timestamp_set_metal, set);
		return SPUDRESULT_API_SPECIFIC_FAILURE;
	}
	set->_sample_buffer_mtl = buffer;
	set->_device_mtl        = [device_metal->_device_mtl retain];
	set->_parent_device     = device_metal;
	set->_count             = count;
	[device_metal->_device_mtl sampleTimestamps:&set->_cpu_origin gpuTimestamp:&set->_gpu_origin];
	SPUDGPU___LIVE_ADD(&device_metal->_live, SPUDGPU___OBJECT_TIMESTAMP_SET);
	*out_set = (spudgpu_timestamp_set)set;
	return SPUD_SUCCESS;
}

void spudgpu_destroy_timestamp_set(spudgpu_timestamp_set set) {
	if (!set)
		return;
	spudgpu_timestamp_set_metal *set_metal = (spudgpu_timestamp_set_metal *)set;
	SPUDGPU___LIVE_REMOVE(&set_metal->_parent_device->_live, SPUDGPU___OBJECT_TIMESTAMP_SET);
	[set_metal->_sample_buffer_mtl release];
	[set_metal->_device_mtl release];
	const SPUD_ALLOCATOR allocator = set_metal->head.allocator;
	spud_string_free(&allocator, set_metal->head.debug_name);
	SPUD_DELETE(&allocator, spudgpu_timestamp_set_metal, set_metal);
}

void spudgpu_cmd_reset_timestamps(
    spudgpu_command_list cmd,
    spudgpu_timestamp_set set,
    uint32_t first,
    uint32_t count) {
	// A sample overwrites what the entry held: nothing to reset.
	(void)cmd;
	(void)set;
	(void)first;
	(void)count;
}

void spudgpu_cmd_resolve_timestamps(
    spudgpu_command_list cmd,
    spudgpu_timestamp_set set,
    uint32_t first,
    uint32_t count) {
	// Resolved on the CPU from shared storage (spudgpu_get_timestamps).
	(void)cmd;
	(void)set;
	(void)first;
	(void)count;
}

SPUDRESULT spudgpu_get_timestamps(
    spudgpu_timestamp_set set,
    uint32_t first,
    uint32_t count,
    uint64_t *out_ns) {
	if (!set)
		return SPUDRESULT_GPU_INVALID_TIMESTAMP_SET;
	if (count == 0)
		return SPUDRESULT_ZERO_SIZE;
	spudgpu_timestamp_set_metal *set_metal = (spudgpu_timestamp_set_metal *)set;
	if (first >= set_metal->_count || count > set_metal->_count - first)
		return SPUDRESULT_INDEX_OUT_OF_RANGE;
	if (!out_ns)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;

	// The GPU's ticks are set against the CPU's nanoseconds by two pairs of
	// samples of both clocks, the creation's and this one, as Apple's
	// "Converting GPU Timestamps into CPU Time" does. A GPU clock that hasn't
	// moved since creation gives no rate, and is taken as nanoseconds.
	MTLTimestamp cpu_now = 0, gpu_now = 0;
	[set_metal->_device_mtl sampleTimestamps:&cpu_now gpuTimestamp:&gpu_now];
	double ns_per_tick = 1.0;
	if (gpu_now > set_metal->_gpu_origin && cpu_now > set_metal->_cpu_origin)
		ns_per_tick = (double)(cpu_now - set_metal->_cpu_origin) / (double)(gpu_now - set_metal->_gpu_origin);

	SPUDRESULT sr = SPUD_SUCCESS;
	// -resolveCounterRange: returns an autoreleased NSData, and a caller in
	// plain C may have no pool of its own.
	@autoreleasepool {
		NSData *data = [set_metal->_sample_buffer_mtl resolveCounterRange:NSMakeRange(first, count)];
		if (!data || data.length < (NSUInteger)count * sizeof(MTLCounterResultTimestamp)) {
			sr = SPUDRESULT_API_SPECIFIC_FAILURE;
		} else {
			const MTLCounterResultTimestamp *results = (const MTLCounterResultTimestamp *)data.bytes;
			for (uint32_t i = 0; i < count; ++i) {
				const uint64_t ticks = results[i].timestamp;
				// An entry no pass sampled is undefined (spudgpu.h); Metal
				// marks it, and it is given as 0.
				if (ticks == MTLCounterErrorValue) {
					out_ns[i] = 0;
					continue;
				}
				out_ns[i] = (uint64_t)((double)(int64_t)(ticks - set_metal->_gpu_origin) * ns_per_tick) + set_metal->_cpu_origin;
			}
		}
	}
	return sr;
}

#endif // SPUDGPU_COMPILE_METAL_API
