//
// SpudGPU Metal backend - fences (id<MTLSharedEvent>) and semaphores
// (plain id<MTLEvent> - spudgpu_semaphore never exposes a CPU-side accessor,
// so it doesn't need MTLSharedEvent's CPU-visibility on top). Implemented and
// verified against real Apple hardware.
//

#if SPUDGPU_COMPILE_METAL_API

#include "spudgpu.h"
#include "spudcore.h"
#include "spudgpumetal.h"
#include <Metal/Metal.h>
#include <dispatch/dispatch.h>
#include <time.h>

static uint64_t spudgpumetal___internal_now_ms(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

SPUDRESULT spudgpu_create_fence(
    spudgpu_device device,
    SPUDGPU_FENCE_FLAGS flags,
    uint64_t initial_value,
    spudgpu_fence *out_fence) {
	if (!device)
		return SPUDRESULT_GPU_INVALID_DEVICE;
	if (!out_fence)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;

	SPUDRESULT sr = SPUD_SUCCESS;

	spudgpu_fence_metal *fence_metal = SPUD_NEW_ZEROED(&device->head.allocator, spudgpu_fence_metal);
	if (!fence_metal) {
		sr = SPUDRESULT_OUT_OF_MEMORY;
		goto failedattempt;
	}
	fence_metal->head.allocator = device->head.allocator;
	fence_metal->_parent_device = (spudgpu_device_metal *)device;
	fence_metal->_flags         = flags;

	// SPUDGPU_FENCE_FLAG_SHARED needs no special creation-time flag on Metal
	// the way D3D12_FENCE_FLAG_SHARED does - any MTLSharedEvent can hand out
	// a cross-process MTLSharedEventHandle via -newSharedEventHandle on
	// demand. flags is kept on the struct for API symmetry and future
	// natives-header use.
	fence_metal->_shared_event_mtl = [fence_metal->_parent_device->_device_mtl newSharedEvent];
	if (!fence_metal->_shared_event_mtl) {
		sr = SPUDRESULT_API_SPECIFIC_FAILURE;
		goto failedattempt;
	}
	fence_metal->_shared_event_mtl.signaledValue = initial_value;

	*out_fence = (spudgpu_fence)fence_metal;

	return sr;

failedattempt:
	SPUD_DELETE(&device->head.allocator, spudgpu_fence_metal, fence_metal);
	return sr;
}

void spudgpu_destroy_fence(spudgpu_fence fence) {
	spudgpu_fence_metal *fence_metal = (spudgpu_fence_metal *)fence;
	if (fence_metal) {
		if (fence_metal->_shared_event_mtl) {
			[fence_metal->_shared_event_mtl release];
		}
		const SPUD_ALLOCATOR allocator = fence_metal->head.allocator;
		spud_string_free(&allocator, fence_metal->head.debug_name);
		SPUD_DELETE(&allocator, spudgpu_fence_metal, fence_metal);
	}
}

uint64_t spudgpu_get_fence_value(spudgpu_fence fence) {
	spudgpu_fence_metal *fence_metal = (spudgpu_fence_metal *)fence;
	if (!fence_metal)
		return 0;
	if (!fence_metal->_shared_event_mtl)
		return 0;
	// The value the GPU (or a prior spudgpu_signal_fence call) has actually
	// reached, not one a submission still running has been asked to signal.
	return fence_metal->_shared_event_mtl.signaledValue;
}

SPUDRESULT spudgpu_signal_fence(
    spudgpu_device device,
    spudgpu_fence fence,
    uint64_t value) {
	if (!device)
		return SPUDRESULT_GPU_INVALID_DEVICE;
	if (!fence)
		return SPUDRESULT_GPU_INVALID_FENCE;

	spudgpu_fence_metal *fence_metal = (spudgpu_fence_metal *)fence;
	fence_metal->_shared_event_mtl.signaledValue = value;

	return SPUD_SUCCESS;
}

SPUDRESULT spudgpu_wait_for_fences(
    spudgpu_device device,
    spudgpu_fence *fences,
    const uint64_t *values,
    uint32_t fence_count,
    bool wait_all,
    uint64_t timeout_ns) {
	if (!device)
		return SPUDRESULT_GPU_INVALID_DEVICE;
	if (!fences)
		return SPUDRESULT_GPU_INVALID_FENCE;
	if (!values)
		return SPUDRESULT_NULL_OBJECT;
	if (fence_count == 0)
		return SPUDRESULT_ZERO_SIZE;
	if (fence_count > 64)
		return SPUDRESULT_DESC_INVALID_PARAMETERS;
	for (uint32_t i = 0; i < fence_count; i++) {
		if (!fences[i])
			return SPUDRESULT_GPU_INVALID_FENCE;
	}

	// Rounded up, so a wait shorter than a millisecond still waits.
	uint64_t timeout_ms = (timeout_ns == UINT64_MAX) ? UINT64_MAX : timeout_ns / 1000000ULL + (timeout_ns % 1000000ULL ? 1 : 0);

	if (wait_all) {
		// MTLSharedEvent only exposes a single-event blocking wait
		// (-waitUntilSignaledValue:timeoutMS:), unlike vkWaitSemaphores/
		// WaitForMultipleObjects - wait_all is composed by waiting on each
		// event in turn against a shared remaining-time budget, which is
		// equivalent to waiting on all of them at once since the call can't
		// return success before every one of them is satisfied anyway.
		uint64_t remaining_ms = timeout_ms;
		for (uint32_t i = 0; i < fence_count; i++) {
			spudgpu_fence_metal *fence_metal = (spudgpu_fence_metal *)fences[i];

			// Already there: no wait, and none of the budget spent.
			if (fence_metal->_shared_event_mtl.signaledValue >= values[i])
				continue;

			uint64_t started_ms = spudgpumetal___internal_now_ms();
			BOOL signaled = [fence_metal->_shared_event_mtl
			    waitUntilSignaledValue:values[i]
			                 timeoutMS:remaining_ms];
			if (!signaled)
				return SPUDRESULT_GPU_FENCE_WAIT_TIMED_OUT;

			if (timeout_ms != UINT64_MAX) {
				uint64_t elapsed_ms = spudgpumetal___internal_now_ms() - started_ms;
				remaining_ms         = (elapsed_ms >= remaining_ms) ? 0 : remaining_ms - elapsed_ms;
			}
		}
		return SPUD_SUCCESS;
	}

	// wait_all == false: block until ANY one fence reaches its value. One
	// already there settles it without a listener.
	for (uint32_t i = 0; i < fence_count; i++) {
		spudgpu_fence_metal *fence_metal = (spudgpu_fence_metal *)fences[i];
		if (fence_metal->_shared_event_mtl.signaledValue >= values[i])
			return SPUD_SUCCESS;
	}

	// There is no native any-of-N wait either, so this is composed from a
	// per-fence async listener that releases one shared dispatch semaphore
	// as soon as the first of them fires.
	dispatch_queue_t listener_queue =
	    dispatch_queue_create("spudgpu.metal.wait_for_fences", DISPATCH_QUEUE_SERIAL);
	MTLSharedEventListener *listener =
	    [[MTLSharedEventListener alloc] initWithDispatchQueue:listener_queue];
	dispatch_semaphore_t any_signaled = dispatch_semaphore_create(0);

	for (uint32_t i = 0; i < fence_count; i++) {
		spudgpu_fence_metal *fence_metal = (spudgpu_fence_metal *)fences[i];
		[fence_metal->_shared_event_mtl
		    notifyListener:listener
		            atValue:values[i]
		              block:^(id<MTLSharedEvent> event, uint64_t value) {
			              dispatch_semaphore_signal(any_signaled);
		              }];
	}

	dispatch_time_t deadline = (timeout_ns == UINT64_MAX)
	                               ? DISPATCH_TIME_FOREVER
	                               : dispatch_time(DISPATCH_TIME_NOW, (int64_t)timeout_ns);
	long wait_result = dispatch_semaphore_wait(any_signaled, deadline);

	[any_signaled release];
	[listener release];
	[listener_queue release];

	return (wait_result == 0) ? SPUD_SUCCESS : SPUDRESULT_GPU_FENCE_WAIT_TIMED_OUT;
}

SPUDRESULT spudgpu_create_semaphore(
    spudgpu_device device,
    spudgpu_semaphore *out_semaphore) {
	if (!device)
		return SPUDRESULT_GPU_INVALID_DEVICE;
	if (!out_semaphore)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;

	SPUDRESULT sr = SPUD_SUCCESS;

	spudgpu_semaphore_metal *semaphore_metal = SPUD_NEW_ZEROED(&device->head.allocator, spudgpu_semaphore_metal);
	if (!semaphore_metal) {
		sr = SPUDRESULT_GENERAL_FAILURE;
		goto failedattempt;
	}
	semaphore_metal->head.allocator = device->head.allocator;
	semaphore_metal->_parent_device = (spudgpu_device_metal *)device;

	semaphore_metal->_event_mtl = [semaphore_metal->_parent_device->_device_mtl newEvent];
	if (!semaphore_metal->_event_mtl) {
		sr = SPUDRESULT_API_SPECIFIC_FAILURE;
		goto failedattempt;
	}

	*out_semaphore = (spudgpu_semaphore)semaphore_metal;

	return sr;

failedattempt:
	SPUD_DELETE(&device->head.allocator, spudgpu_semaphore_metal, semaphore_metal);
	return sr;
}

void spudgpu_destroy_semaphore(spudgpu_semaphore semaphore) {
	spudgpu_semaphore_metal *semaphore_metal = (spudgpu_semaphore_metal *)semaphore;
	if (semaphore_metal) {
		if (semaphore_metal->_event_mtl) {
			[semaphore_metal->_event_mtl release];
		}
		const SPUD_ALLOCATOR allocator = semaphore_metal->head.allocator;
		spud_string_free(&allocator, semaphore_metal->head.debug_name);
		SPUD_DELETE(&allocator, spudgpu_semaphore_metal, semaphore_metal);
	}
}

#endif // SPUDGPU_COMPILE_METAL_API
