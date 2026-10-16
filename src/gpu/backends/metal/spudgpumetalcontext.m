
#if SPUDGPU_COMPILE_METAL_API

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "spudgpumetal.h"

SPUDRESULT spudgpu_create_instance(
    const char *application_name,
    uint32_t application_version,
    const char *engine_name,
    uint32_t engine_version,
    spudgpu_instance *out_instance) {
	// Metal has no application/engine name or instance-level version info to
	// hand anywhere (unlike VkApplicationInfo) - accepted for API symmetry
	// with the Vulkan/D3D12 backends and otherwise unused.
	(void) application_name;
	(void) application_version;
	(void) engine_name;
	(void) engine_version;

	if (!out_instance) return SPUDRESULT_NULL_OUTPUT_PARAMETER;

	spudgpu_instance_metal *instance = calloc(1, sizeof(spudgpu_instance_metal));
	if (!instance) return SPUDRESULT_OUT_OF_MEMORY;

	*out_instance = instance;
	return SPUD_SUCCESS;
}

static void spudgpumetal___internal_destroy_command_queue_family(
    spudgpu_command_queue *family) {
	for (uint32_t i = 0; i < SPUD_METAL_COMMAND_QUEUE_COUNT_PER_FAMILY; i++) {
		spudgpu_command_queue_metal *queue = (spudgpu_command_queue_metal *) family[i];
		if (!queue) continue;
		if (queue->_command_queue_mtl) {
			[queue->_command_queue_mtl release];
		}
#if _DEBUG
		free((void *)queue->_debug_name);
#endif
		free(queue);
	}
}

SPUDRESULT spudgpu_destroy_instance(spudgpu_instance instance) {
	if (!instance) return SPUD_SUCCESS;

	// instance owns every spudgpu_device handed out by spudgpu_enumerate_devices
	// (there is no public spudgpu_destroy_device) - mirrors the Vulkan backend.
	for (uint32_t i = 0; i < instance->_devices_count; i++) {
		spudgpu_device_metal *device = (spudgpu_device_metal *) instance->_devices[i];
		if (!device) continue;
		spudgpumetal___internal_destroy_command_queue_family(device->_cmd_queues_direct);
		spudgpumetal___internal_destroy_command_queue_family(device->_cmd_queues_copy);
		spudgpumetal___internal_destroy_command_queue_family(device->_cmd_queues_compute);
		[device->_device_mtl release];
#if _DEBUG
		free((void *)device->_debug_name);
#endif
		free(device);
	}
	free(instance->_devices);
#if _DEBUG
	free((void *)instance->_debug_name);
#endif
	free(instance);
	return SPUD_SUCCESS;
}

static void spudgpumetal___internal_make_device_properties(
    spudgpu_device_metal *device) {
	if (!device) return;

	const char *name = [device->_device_mtl.name UTF8String];
	size_t nameLen = strnlen(name, sizeof(device->_properties.description) - 1);
	memcpy(device->_properties.description, name, nameLen);
	device->_properties.description[nameLen] = '\0';

	// Metal has no PCI-style vendor/device ID accessor the way Vulkan/D3D12
	// do (MTLDevice only exposes -registryID, an unrelated IOKit registry
	// entry ID) - every Metal device is an Apple GPU on the platforms
	// spudlib targets, so vendor_id is hardcoded rather than left unset.
	device->_properties.vendor_id = SPUDGPU_VENDOR_APPLE;
	device->_properties.device_id = 0;
	device->_properties.subSys_id = 0;
	device->_properties.revision  = 0;

	// Apple Silicon GPUs share unified memory with the CPU, so "dedicated
	// video memory" doesn't apply the way it does to a discrete Vulkan/D3D12
	// card - report the working-set budget under whichever bucket matches
	// this device's actual memory model instead of guessing a split.
	uint64_t workingSet = device->_device_mtl.recommendedMaxWorkingSetSize;
	bool unified = device->_device_mtl.hasUnifiedMemory;
	device->_properties.dedicated_video_memory  = unified ? 0 : workingSet;
	device->_properties.dedicated_system_memory = 0;
	device->_properties.shared_system_memory    = unified ? workingSet : 0;
	device->_properties.unified_memory          = unified;
	// SPUDGPU_MEMORY_KIND_DEVICE_MAPPABLE is Shared storage, which is the
	// device's own memory only where that memory is the system's.
	device->_properties.device_mappable_memory  = unified;
	// MTLRenderPassDescriptor.renderTargetArrayLength, by Metal's feature
	// set tables.
	device->_properties.layered_rendering =
	    [device->_device_mtl supportsFamily:MTLGPUFamilyApple5] || [device->_device_mtl supportsFamily:MTLGPUFamilyMac2];
	// A heap of MTLStorageModeShared, which every mappable kind is, can be
	// made on an Apple GPU and not on any other. Each MTLGPUFamilyApple
	// family includes the ones before it, so Apple1 is every Apple GPU.
	device->_properties.mappable_heaps = [device->_device_mtl supportsFamily:MTLGPUFamilyApple1];
	// setDepthBias:slopeScale:clamp: takes a clamp, and each colour
	// attachment of a render pipeline descriptor blends on its own, on
	// every device.
	device->_properties.depth_bias_clamp  = true;
	device->_properties.independent_blend = true;
	// MTLTextureTypeCubeArray, on every Apple Silicon and Mac2 GPU.
	device->_properties.cube_array_views = true;
	// spudgpu_cmd_draw_indirect loops one indirect draw per entry, so any
	// count is recorded.
	device->_properties.max_draw_indirect_count = UINT32_MAX;
	// A pass's timestamps are samples of the timestamp counter set at its
	// stage boundaries.
	id<MTLCounterSet> timestamp_counter_set = spudgpumetal___internal_timestamp_counter_set(device->_device_mtl);
	device->_properties.pass_timestamps =
	    timestamp_counter_set != nil && [device->_device_mtl supportsCounterSampling:MTLCounterSamplingPointAtStageBoundary];
	[timestamp_counter_set release];
}

static SPUDRESULT spudgpumetal___internal_create_command_queue_family(
    spudgpu_device_metal *device,
    spudgpu_command_queue *out_family) {
	for (uint32_t i = 0; i < SPUD_METAL_COMMAND_QUEUE_COUNT_PER_FAMILY; i++) {
		spudgpu_command_queue_metal *queue = calloc(1, sizeof(spudgpu_command_queue_metal));
		if (!queue) return SPUDRESULT_OUT_OF_MEMORY;
		queue->_command_queue_mtl = [device->_device_mtl newCommandQueue];
		if (!queue->_command_queue_mtl) {
			free(queue);
			return SPUDRESULT_API_SPECIFIC_FAILURE;
		}
		queue->_parent_device = device;
		out_family[i] = (spudgpu_command_queue) queue;
	}
	return SPUD_SUCCESS;
}

static SPUDRESULT spudgpumetal___internal_create_device_command_queues(
    spudgpu_device_metal *device) {
	SPUDRESULT sr = SPUD_SUCCESS;

	sr = spudgpumetal___internal_create_command_queue_family(device, device->_cmd_queues_direct);
	if (sr != SPUD_SUCCESS) return sr;

	sr = spudgpumetal___internal_create_command_queue_family(device, device->_cmd_queues_copy);
	if (sr != SPUD_SUCCESS) return sr;

	sr = spudgpumetal___internal_create_command_queue_family(device, device->_cmd_queues_compute);
	if (sr != SPUD_SUCCESS) return sr;

	return sr;
}

SPUDRESULT spudgpu_enumerate_devices(
    spudgpu_instance instance,
    spudgpu_device **out_devices,
    uint32_t *out_devices_count) {
	if (!instance) return SPUDRESULT_GPU_INVALID_INSTANCE;
	if (!out_devices || !out_devices_count) return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	// The devices are made once; a later call hands out the same array.
	if (instance->_devices) {
		*out_devices       = instance->_devices;
		*out_devices_count = instance->_devices_count;
		return SPUD_SUCCESS;
	}

	NSArray<id<MTLDevice> > *mtlDevices = MTLCopyAllDevices();
	uint32_t deviceCount = (uint32_t) mtlDevices.count;
	if (deviceCount == 0) {
		[mtlDevices release];
		return SPUDRESULT_GPU_DEVICE_ENUMERATION_FAILURE;
	}

	// The array is the instance's, and the caller is handed it: the
	// devices and the array are freed by spudgpu_destroy_instance.
	instance->_devices = calloc(deviceCount, sizeof(spudgpu_device));
	if (!instance->_devices) {
		[mtlDevices release];
		return SPUDRESULT_OUT_OF_MEMORY;
	}
	instance->_devices_count = deviceCount;
	*out_devices             = instance->_devices;
	*out_devices_count       = deviceCount;

	for (uint32_t i = 0; i < deviceCount; i++) {
		spudgpu_device_metal *device = calloc(1, sizeof(spudgpu_device_metal));
		device->_device_mtl = [mtlDevices[i] retain];
#if SPUDGPU_EXT_DEPTH_BOUNDS_TEST
		device->_depth_bounds_test_supported =
		    [device->_device_mtl supportsFamily:MTLGPUFamilyApple10];
#endif
		spudgpumetal___internal_make_device_properties(device);

		SPUDRESULT queue_sr = spudgpumetal___internal_create_device_command_queues(device);
		if (queue_sr != SPUD_SUCCESS) {
			printf("spudgpu: failed to create Metal command queues for device\n");
			[mtlDevices release];
			return queue_sr;
		}

		instance->_devices[i] = (spudgpu_device) device;
	}

	[mtlDevices release];
	return SPUD_SUCCESS;
}

SPUDRESULT spudgpu_get_device_properties(
    spudgpu_device device, SPUDGPU_DEVICE_PROPERTIES *out_properties) {
	if (!device)
		return SPUDRESULT_GPU_INVALID_DEVICE;
	if (!out_properties)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	memcpy(
	    out_properties, &device->_properties,
	    sizeof(SPUDGPU_DEVICE_PROPERTIES));
	return SPUD_SUCCESS;
}

SPUDGPU_NATIVE_API spudgpu_get_native_gpu_api(spudgpu_instance instance) {
	return instance ? SPUDGPU_NATIVE_API_METAL : SPUDGPU_NATIVE_API_NONE;
}

#endif // SPUDGPU_COMPILE_METAL_API
