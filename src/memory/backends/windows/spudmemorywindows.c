#if SPUDLIB_PLATFORM_WIN32

#include "../../spudmemoryshared.h"

#include <Windows.h>
#include <stdint.h>

uint32_t smem_backend_page_size(void) {
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	return (uint32_t)info.dwPageSize;
}

/* The two errors that mean the system has no memory or address space left. */
static SPUDRESULT smem_windows_failure(void) {
	const DWORD error = GetLastError();
	if (error == ERROR_NOT_ENOUGH_MEMORY || error == ERROR_COMMITMENT_LIMIT)
		return SPUDRESULT_OUT_OF_MEMORY;
	return SPUDRESULT_API_SPECIFIC_FAILURE;
}

SPUDRESULT smem_backend_reserve(
    uint64_t size,
    void **out_address) {
	if (size > (uint64_t)SIZE_MAX)
		return SPUDRESULT_OUT_OF_MEMORY;
	void *address = VirtualAlloc(NULL, (SIZE_T)size, MEM_RESERVE, PAGE_NOACCESS);
	if (!address)
		return smem_windows_failure();
	*out_address = address;
	return SPUD_SUCCESS;
}

SPUDRESULT smem_backend_commit(
    void *address,
    uint64_t size) {
	/* Pages committed for the first time, or again after a decommit, are
	 * zero; already committed ones keep their contents. */
	if (!VirtualAlloc(address, (SIZE_T)size, MEM_COMMIT, PAGE_READWRITE))
		return smem_windows_failure();
	return SPUD_SUCCESS;
}

SPUDRESULT smem_backend_decommit(
    void *address,
    uint64_t size) {
	if (!VirtualFree(address, (SIZE_T)size, MEM_DECOMMIT))
		return SPUDRESULT_API_SPECIFIC_FAILURE;
	return SPUD_SUCCESS;
}

SPUDRESULT smem_backend_release(
    void *address,
    uint64_t size) {
	/* MEM_RELEASE takes the reservation's base and a size of 0; the size
	 * was checked by the front end, and the header's rule makes it the
	 * whole reservation's. */
	(void)size;
	if (!VirtualFree(address, 0, MEM_RELEASE))
		return SPUDRESULT_API_SPECIFIC_FAILURE;
	return SPUD_SUCCESS;
}

#endif // SPUDLIB_PLATFORM_WIN32
