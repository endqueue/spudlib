#ifndef SPUDMEMORY_H
#define SPUDMEMORY_H

/**
 * @file spudmemory.h
 * @brief SpudMemory: the platform's virtual memory, a page at a time.
 *
 * Address space is reserved, then pages of it are committed (given memory
 * the caller can read and write), decommitted (their memory given back, the
 * address space kept) and the whole reservation released. That is all: how
 * much to reserve, when to commit, and how the pages are divided up are the
 * caller's.
 *
 * Backends: VirtualAlloc/VirtualFree on Windows; mmap/mprotect/munmap on
 * Linux and every Apple system.
 *
 * Rules that hold on every backend:
 *
 * - Every size, and every address passed in, is a multiple of the page size
 *   (smem_get_page_size()). Nothing is rounded: a size or address that isn't
 *   one is refused with SPUDRESULT_SMEM_NOT_PAGE_ALIGNED.
 * - A committed page reads zero until it is written, the first time and
 *   every time it is committed again after a decommit. Committing a page
 *   that is already committed leaves its contents as they are.
 * - Reading or writing a page that isn't committed is undefined (it
 *   faults on every backend today).
 * - The pages committed and decommitted are within one reservation, and a
 *   release gives back the whole of one: the address and size
 *   smem_reserve() returned. Anything else is undefined (Windows can't
 *   release part of a reservation).
 *
 * No call holds state, so any may be called from any thread. Two threads
 * changing the same pages at once is the caller's to prevent.
 */

#include <spudcore.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#define SMEM_KiB(n) ((uint64_t)(n) << 10)
#define SMEM_MiB(n) ((uint64_t)(n) << 20)
#define SMEM_GiB(n) ((uint64_t)(n) << 30)

/**
 * @brief The size of a page, in bytes: what every size and address the
 * other calls take is a multiple of. A power of two; the same for the life
 * of the process. 4 KiB on most x86-64 systems, 16 KiB on Apple Silicon.
 */
uint32_t smem_get_page_size(void);

/**
 * @brief Reserves address space, none of it committed.
 *
 * @param[in]  size        Bytes to reserve: a multiple of the page size.
 * @param[out] out_address Receives the start of the reservation, a
 *                         multiple of the page size; NULL on failure.
 *
 * @retval SPUD_SUCCESS The space is reserved.
 * @retval SPUDRESULT_ZERO_SIZE `size` is 0.
 * @retval SPUDRESULT_SMEM_NOT_PAGE_ALIGNED `size` is not a multiple of the
 *         page size.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_address` is NULL.
 * @retval SPUDRESULT_OUT_OF_MEMORY The system has no address space or
 *         memory left for it.
 * @retval SPUDRESULT_API_SPECIFIC_FAILURE The platform refused for another
 *         reason.
 */
SPUDRESULT smem_reserve(
    uint64_t size,
    void **out_address);

/**
 * @brief Commits pages of a reservation, so that they can be read and
 * written.
 *
 * @param[in] address The first page: a multiple of the page size.
 * @param[in] size    Bytes to commit: a multiple of the page size.
 *
 * @retval SPUD_SUCCESS The pages are committed.
 * @retval SPUDRESULT_SMEM_NULL_ADDRESS `address` is NULL.
 * @retval SPUDRESULT_SMEM_NOT_PAGE_ALIGNED `address` or `size` is not a
 *         multiple of the page size.
 * @retval SPUDRESULT_ZERO_SIZE `size` is 0.
 * @retval SPUDRESULT_OUT_OF_MEMORY The system has no memory left for them.
 * @retval SPUDRESULT_API_SPECIFIC_FAILURE The platform refused for another
 *         reason.
 */
SPUDRESULT smem_commit(
    void *address,
    uint64_t size);

/**
 * @brief Decommits pages of a reservation: their memory goes back to the
 * system, and the address space stays reserved. Pages that aren't committed
 * may be in the range.
 *
 * @param[in] address The first page: a multiple of the page size.
 * @param[in] size    Bytes to decommit: a multiple of the page size.
 *
 * @retval SPUD_SUCCESS The pages are decommitted.
 * @retval SPUDRESULT_SMEM_NULL_ADDRESS `address` is NULL.
 * @retval SPUDRESULT_SMEM_NOT_PAGE_ALIGNED `address` or `size` is not a
 *         multiple of the page size.
 * @retval SPUDRESULT_ZERO_SIZE `size` is 0.
 * @retval SPUDRESULT_API_SPECIFIC_FAILURE The platform refused.
 */
SPUDRESULT smem_decommit(
    void *address,
    uint64_t size);

/**
 * @brief Releases a whole reservation, committed pages and all.
 *
 * @param[in] address The start smem_reserve() returned.
 * @param[in] size    The size it was reserved with.
 *
 * @retval SPUD_SUCCESS The reservation is gone.
 * @retval SPUDRESULT_SMEM_NULL_ADDRESS `address` is NULL.
 * @retval SPUDRESULT_SMEM_NOT_PAGE_ALIGNED `address` or `size` is not a
 *         multiple of the page size.
 * @retval SPUDRESULT_ZERO_SIZE `size` is 0.
 * @retval SPUDRESULT_API_SPECIFIC_FAILURE The platform refused.
 */
SPUDRESULT smem_release(
    void *address,
    uint64_t size);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // SPUDMEMORY_H
