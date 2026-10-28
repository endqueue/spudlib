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
 *   (spudmem_get_page_size()). Nothing is rounded: a size or address that isn't
 *   one is refused with SPUDRESULT_SPUDMEM_NOT_PAGE_ALIGNED.
 * - A committed page reads zero until it is written, the first time and
 *   every time it is committed again after a decommit. Committing a page
 *   that is already committed leaves its contents as they are.
 * - Reading or writing a page that isn't committed is undefined (it
 *   faults on every backend today).
 * - The pages committed and decommitted are within one reservation, and a
 *   release gives back the whole of one: the address and size
 *   spudmem_reserve() returned. Anything else is undefined (Windows can't
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

/** @brief @p n kibibytes, in bytes, as a uint64_t. */
#define SPUDMEM_KiB(n) ((uint64_t)(n) << 10)
/** @brief @p n mebibytes, in bytes, as a uint64_t. */
#define SPUDMEM_MiB(n) ((uint64_t)(n) << 20)
/** @brief @p n gibibytes, in bytes, as a uint64_t. */
#define SPUDMEM_GiB(n) ((uint64_t)(n) << 30)

/**
 * @brief The size of a page, in bytes: what every size and address the
 * other calls take is a multiple of. A power of two; the same for the life
 * of the process. 4 KiB on most x86-64 systems, 16 KiB on Apple Silicon.
 * @return The page size in bytes.
 */
uint32_t spudmem_get_page_size(void);

/**
 * @brief The reservation granularity, in bytes: where the system starts a
 * reservation, and so the size below which a reservation strands address
 * space no other reservation can use. A power of two and a multiple of the
 * page size; the same for the life of the process. 64 KiB on Windows
 * (`dwAllocationGranularity`); the page size on Linux and every Apple
 * system.
 *
 * spudmem_reserve() still takes any multiple of the page size: a caller
 * that reserves many small ranges reads this to size them, and nothing is
 * rounded for it.
 *
 * @return The reservation granularity in bytes.
 */
uint32_t spudmem_get_reserve_granularity(void);

/**
 * @brief Reserves address space, none of it committed.
 *
 * @param[in]  size        Bytes to reserve: a multiple of the page size.
 * @param[out] out_address Receives the start of the reservation, a
 *                         multiple of the reservation granularity
 *                         (spudmem_get_reserve_granularity()); NULL on
 *                         failure.
 *
 * @retval SPUD_SUCCESS The space is reserved.
 * @retval SPUDRESULT_ZERO_SIZE `size` is 0.
 * @retval SPUDRESULT_SPUDMEM_NOT_PAGE_ALIGNED `size` is not a multiple of the
 *         page size.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_address` is NULL.
 * @retval SPUDRESULT_OUT_OF_MEMORY The system has no address space or
 *         memory left for it.
 * @retval SPUDRESULT_API_SPECIFIC_FAILURE The platform refused for another
 *         reason.
 */
SPUDRESULT spudmem_reserve(
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
 * @retval SPUDRESULT_SPUDMEM_NULL_ADDRESS `address` is NULL.
 * @retval SPUDRESULT_SPUDMEM_NOT_PAGE_ALIGNED `address` or `size` is not a
 *         multiple of the page size.
 * @retval SPUDRESULT_ZERO_SIZE `size` is 0.
 * @retval SPUDRESULT_OUT_OF_MEMORY The system has no memory left for them.
 * @retval SPUDRESULT_API_SPECIFIC_FAILURE The platform refused for another
 *         reason.
 */
SPUDRESULT spudmem_commit(
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
 * @retval SPUDRESULT_SPUDMEM_NULL_ADDRESS `address` is NULL.
 * @retval SPUDRESULT_SPUDMEM_NOT_PAGE_ALIGNED `address` or `size` is not a
 *         multiple of the page size.
 * @retval SPUDRESULT_ZERO_SIZE `size` is 0.
 * @retval SPUDRESULT_API_SPECIFIC_FAILURE The platform refused.
 */
SPUDRESULT spudmem_decommit(
    void *address,
    uint64_t size);

/**
 * @brief Releases a whole reservation, committed pages and all.
 *
 * @param[in] address The start spudmem_reserve() returned.
 * @param[in] size    The size it was reserved with.
 *
 * @retval SPUD_SUCCESS The reservation is gone.
 * @retval SPUDRESULT_SPUDMEM_NULL_ADDRESS `address` is NULL.
 * @retval SPUDRESULT_SPUDMEM_NOT_PAGE_ALIGNED `address` or `size` is not a
 *         multiple of the page size.
 * @retval SPUDRESULT_ZERO_SIZE `size` is 0.
 * @retval SPUDRESULT_API_SPECIFIC_FAILURE The platform refused.
 */
SPUDRESULT spudmem_release(
    void *address,
    uint64_t size);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // SPUDMEMORY_H
