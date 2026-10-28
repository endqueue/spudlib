#include "spudmemory.h"
#include "memory/spudmemoryshared.h"

#include <stdbool.h>
#include <stdint.h>

/* Whether [value] is a multiple of the page size, which is a power of two. */
static bool spudmem_page_multiple(uint64_t value) { return (value & ((uint64_t)spudmem_backend_page_size() - 1)) == 0; }

uint32_t spudmem_get_page_size(void) { return spudmem_backend_page_size(); }

uint32_t spudmem_get_reserve_granularity(void) { return spudmem_backend_reserve_granularity(); }

SPUDRESULT spudmem_reserve(
    uint64_t size,
    void **out_address) {
	if (out_address)
		*out_address = NULL;
	if (size == 0)
		return SPUDRESULT_ZERO_SIZE;
	if (!spudmem_page_multiple(size))
		return SPUDRESULT_SPUDMEM_NOT_PAGE_ALIGNED;
	if (!out_address)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	return spudmem_backend_reserve(size, out_address);
}

SPUDRESULT spudmem_commit(
    void *address,
    uint64_t size) {
	if (!address)
		return SPUDRESULT_SPUDMEM_NULL_ADDRESS;
	if (!spudmem_page_multiple((uint64_t)(uintptr_t)address))
		return SPUDRESULT_SPUDMEM_NOT_PAGE_ALIGNED;
	if (size == 0)
		return SPUDRESULT_ZERO_SIZE;
	if (!spudmem_page_multiple(size))
		return SPUDRESULT_SPUDMEM_NOT_PAGE_ALIGNED;
	return spudmem_backend_commit(address, size);
}

SPUDRESULT spudmem_decommit(
    void *address,
    uint64_t size) {
	if (!address)
		return SPUDRESULT_SPUDMEM_NULL_ADDRESS;
	if (!spudmem_page_multiple((uint64_t)(uintptr_t)address))
		return SPUDRESULT_SPUDMEM_NOT_PAGE_ALIGNED;
	if (size == 0)
		return SPUDRESULT_ZERO_SIZE;
	if (!spudmem_page_multiple(size))
		return SPUDRESULT_SPUDMEM_NOT_PAGE_ALIGNED;
	return spudmem_backend_decommit(address, size);
}

SPUDRESULT spudmem_release(
    void *address,
    uint64_t size) {
	if (!address)
		return SPUDRESULT_SPUDMEM_NULL_ADDRESS;
	if (!spudmem_page_multiple((uint64_t)(uintptr_t)address))
		return SPUDRESULT_SPUDMEM_NOT_PAGE_ALIGNED;
	if (size == 0)
		return SPUDRESULT_ZERO_SIZE;
	if (!spudmem_page_multiple(size))
		return SPUDRESULT_SPUDMEM_NOT_PAGE_ALIGNED;
	return spudmem_backend_release(address, size);
}
