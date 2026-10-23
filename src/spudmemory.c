#include "spudmemory.h"
#include "memory/spudmemoryshared.h"

#include <stdbool.h>
#include <stdint.h>

/* Whether [value] is a multiple of the page size, which is a power of two. */
static bool smem_page_multiple(uint64_t value) { return (value & ((uint64_t)smem_backend_page_size() - 1)) == 0; }

uint32_t smem_get_page_size(void) { return smem_backend_page_size(); }

SPUDRESULT smem_reserve(
    uint64_t size,
    void **out_address) {
	if (out_address)
		*out_address = NULL;
	if (size == 0)
		return SPUDRESULT_ZERO_SIZE;
	if (!smem_page_multiple(size))
		return SPUDRESULT_SMEM_NOT_PAGE_ALIGNED;
	if (!out_address)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	return smem_backend_reserve(size, out_address);
}

SPUDRESULT smem_commit(
    void *address,
    uint64_t size) {
	if (!address)
		return SPUDRESULT_SMEM_NULL_ADDRESS;
	if (!smem_page_multiple((uint64_t)(uintptr_t)address))
		return SPUDRESULT_SMEM_NOT_PAGE_ALIGNED;
	if (size == 0)
		return SPUDRESULT_ZERO_SIZE;
	if (!smem_page_multiple(size))
		return SPUDRESULT_SMEM_NOT_PAGE_ALIGNED;
	return smem_backend_commit(address, size);
}

SPUDRESULT smem_decommit(
    void *address,
    uint64_t size) {
	if (!address)
		return SPUDRESULT_SMEM_NULL_ADDRESS;
	if (!smem_page_multiple((uint64_t)(uintptr_t)address))
		return SPUDRESULT_SMEM_NOT_PAGE_ALIGNED;
	if (size == 0)
		return SPUDRESULT_ZERO_SIZE;
	if (!smem_page_multiple(size))
		return SPUDRESULT_SMEM_NOT_PAGE_ALIGNED;
	return smem_backend_decommit(address, size);
}

SPUDRESULT smem_release(
    void *address,
    uint64_t size) {
	if (!address)
		return SPUDRESULT_SMEM_NULL_ADDRESS;
	if (!smem_page_multiple((uint64_t)(uintptr_t)address))
		return SPUDRESULT_SMEM_NOT_PAGE_ALIGNED;
	if (size == 0)
		return SPUDRESULT_ZERO_SIZE;
	if (!smem_page_multiple(size))
		return SPUDRESULT_SMEM_NOT_PAGE_ALIGNED;
	return smem_backend_release(address, size);
}
