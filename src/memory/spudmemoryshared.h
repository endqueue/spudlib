#ifndef SPUDMEMORYSHARED_H
#define SPUDMEMORYSHARED_H

/*
 * What a SpudMemory backend provides. src/spudmemory.c is the front end:
 * every argument check is there, once, and a backend is called only with
 * arguments that passed them (a non-null, page-aligned address and a
 * non-zero, page-multiple size).
 */

#include "spudmemory.h"

#ifdef __cplusplus
extern "C" {
#endif

uint32_t spudmem_backend_page_size(void);
uint32_t spudmem_backend_reserve_granularity(void);
SPUDRESULT spudmem_backend_reserve(
    uint64_t size,
    void **out_address);
SPUDRESULT spudmem_backend_commit(
    void *address,
    uint64_t size);
SPUDRESULT spudmem_backend_decommit(
    void *address,
    uint64_t size);
SPUDRESULT spudmem_backend_release(
    void *address,
    uint64_t size);

#ifdef __cplusplus
}
#endif

#endif // SPUDMEMORYSHARED_H
