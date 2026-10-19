
#if SPUDGPU_COMPILE_D3D12_API

#include "spudgpud3d12.hpp"

static D3D12_FENCE_FLAGS
spudgpu_d3d12_get_fence_flags(SPUDGPU_FENCE_FLAGS flags) {
	D3D12_FENCE_FLAGS result = D3D12_FENCE_FLAG_NONE;
	switch (flags) {
	case SPUDGPU_FENCE_FLAG_SHARED:
		result |= D3D12_FENCE_FLAG_SHARED;
		break;
	}
	return result;
}

extern "C" {

SPUDRESULT spudgpu_create_fence(
    spudgpu_device device,
    SPUDGPU_FENCE_FLAGS flags,
    uint64_t initial_value,
    spudgpu_fence *out_fence) {
	if (!device)
		return SPUDRESULT_GPU_INVALID_DEVICE;
	if (!out_fence)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;

	spudgpu_fence_d3d12 *pResult =
	    (spudgpu_fence_d3d12 *)malloc(sizeof(spudgpu_fence_d3d12));
	if (!pResult)
		return SPUDRESULT_OUT_OF_MEMORY;
	pResult                       = new (pResult) spudgpu_fence_d3d12();
	pResult->_device              = device;
	pResult->flags                = flags;

	if (FAILED(device->_d3d_device->CreateFence(
	        initial_value, spudgpu_d3d12_get_fence_flags(flags),
	        IID_PPV_ARGS(&pResult->_d3d_fence)))) {
		pResult->~spudgpu_fence_d3d12();
		free(pResult);
		return SPUDRESULT_API_SPECIFIC_FAILURE;
	}

	*out_fence = pResult;

	return SPUD_SUCCESS;
}

void spudgpu_destroy_fence(spudgpu_fence fence) {
	if (!fence)
		return;
	free((void *)fence->_debug_name);
	fence->~spudgpu_fence_d3d12();
	free(fence);
}

uint64_t spudgpu_get_fence_value(spudgpu_fence fence) {
	if (!fence)
		return 0;
	else
		return fence->_d3d_fence->GetCompletedValue();
}
SPUDRESULT spudgpu_signal_fence(
    spudgpu_device device, spudgpu_fence fence, uint64_t value) {
	if (!device)
		return SPUDRESULT_GPU_INVALID_DEVICE;
	if (!fence)
		return SPUDRESULT_GPU_INVALID_FENCE;
	if (FAILED(fence->_d3d_fence->Signal(value)))
		return SPUDRESULT_API_SPECIFIC_FAILURE;
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
	// The header's limit, which is also MAXIMUM_WAIT_OBJECTS.
	if (fence_count > 64)
		return SPUDRESULT_DESC_INVALID_PARAMETERS;
	for (uint32_t i = 0; i < fence_count; i++) {
		if (!fences[i])
			return SPUDRESULT_GPU_INVALID_FENCE;
	}

	HANDLE events[MAXIMUM_WAIT_OBJECTS];
	uint32_t events_created = 0;

	for (uint32_t i = 0; i < fence_count; i++) {
		events[i] = CreateEventW(nullptr, FALSE, FALSE, nullptr);
		if (!events[i]) {
			for (uint32_t j = 0; j < events_created; j++)
				CloseHandle(events[j]);
			return SPUDRESULT_API_SPECIFIC_FAILURE;
		}
		events_created++;
		// Sets the event at once if the fence is already at the value.
		if (FAILED(fences[i]->_d3d_fence->SetEventOnCompletion(values[i], events[i]))) {
			for (uint32_t j = 0; j < events_created; j++)
				CloseHandle(events[j]);
			return SPUDRESULT_API_SPECIFIC_FAILURE;
		}
	}

	// Rounded up, so a wait shorter than a millisecond still waits; a
	// timeout too long for a DWORD waits without limit.
	DWORD timeout_ms = INFINITE;
	if (timeout_ns != UINT64_MAX) {
		uint64_t ms = timeout_ns / 1000000ULL + (timeout_ns % 1000000ULL ? 1 : 0);
		if (ms < INFINITE)
			timeout_ms = (DWORD)ms;
	}
	DWORD result = WaitForMultipleObjects(
	    fence_count, events, wait_all ? TRUE : FALSE, timeout_ms);

	for (uint32_t i = 0; i < fence_count; i++)
		CloseHandle(events[i]);

	if (result == WAIT_TIMEOUT)
		return SPUDRESULT_GPU_FENCE_WAIT_TIMED_OUT;
	if (result == WAIT_FAILED)
		return SPUDRESULT_API_SPECIFIC_FAILURE;
	return SPUD_SUCCESS;
}

SPUDRESULT spudgpu_create_semaphore(
    spudgpu_device device, spudgpu_semaphore *out_semaphore) {
	if (!device)
		return SPUDRESULT_GPU_INVALID_DEVICE;
	if (!out_semaphore)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;

	spudgpu_semaphore_d3d12 *pResult =
	    (spudgpu_semaphore_d3d12 *)malloc(sizeof(spudgpu_semaphore_d3d12));
	if (!pResult)
		return SPUDRESULT_OUT_OF_MEMORY;
	pResult          = new (pResult) spudgpu_semaphore_d3d12();
	pResult->_device = device;

	if (FAILED(device->_d3d_device->CreateFence(
	        0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&pResult->_d3d_fence)))) {
		pResult->~spudgpu_semaphore_d3d12();
		free(pResult);
		return SPUDRESULT_API_SPECIFIC_FAILURE;
	}

	*out_semaphore = pResult;

	return SPUD_SUCCESS;
}

void spudgpu_destroy_semaphore(spudgpu_semaphore semaphore) {
	if (!semaphore)
		return;
	free((void *)semaphore->_debug_name);
	semaphore->~spudgpu_semaphore_d3d12();
	free(semaphore);
}
}

#endif // SPUDGPU_COMPILE_D3D12_API
