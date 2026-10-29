#ifndef SPUDALLOCATOR_H
#define SPUDALLOCATOR_H

// How SpudLib takes memory from the caller's allocator (SPUD_ALLOCATOR,
// spudcore.h), written once so that every module and backend allocates,
// frees and checks it the same way.
//
// Every handle's first member is a spud_object_head: the handle's debug name,
// then a copy of the allocator its instance was made with. A handle frees
// through its own copy, its name included, so no destroy call takes an
// allocator, and a handle that outlasts its instance (a SpudNet resolver whose
// lookup is still out) still has it. A pointer to a handle converts to a pointer to its head
// (C11 6.7.2.1p15), which is how spud_debug_name_set reaches both from a
// void *.
//
// SPUD_ALLOCATOR's contract says SpudLib never asks for 0 bytes. These
// functions keep it by asking for 1 byte where the size is 0, at allocation
// and at free alike, so a block of a count that may be 0 is allocated and
// freed with the same count and needs no case of its own.

#include "spudcore.h"
#include <assert.h>
#include <eqstd.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#if defined(__cplusplus)
#define SPUD_ALIGNOF(T) alignof(T)
#else
#define SPUD_ALIGNOF(T) _Alignof(T)
#endif

// The alignment of max_align_t: what a block that may hold any scalar type is
// allocated and freed with. MSVC's C compiler has no max_align_t (its
// <stddef.h> declares one for C++ only, as double), so there it is double's,
// the same value MSVC's C++ gives.
#if defined(_MSC_VER) && !defined(__clang__) && !defined(__cplusplus)
#define SPUD_MAX_ALIGN _Alignof(double)
#else
#define SPUD_MAX_ALIGN SPUD_ALIGNOF(max_align_t)
#endif

typedef struct spud_object_head {
	// NULL, or a copy made through [allocator].
	const char *debug_name;
	// The allocator the handle was made with and frees through: a copy of
	// its instance's.
	SPUD_ALLOCATOR allocator;
} spud_object_head;

// The check a create call makes of a desc's allocator, right after the
// desc's struct check.
static inline SPUDRESULT spud_allocator_check(const SPUD_ALLOCATOR *allocator) {
	if (!allocator)
		return SPUDRESULT_NULL_ALLOCATOR;
	if (!allocator->allocate || !allocator->free)
		return SPUDRESULT_INVALID_ALLOCATOR;
	return SPUD_SUCCESS;
}

// A block of [size] bytes (1 if [size] is 0), contents undefined, or NULL.
static inline void *spud_allocate(const SPUD_ALLOCATOR *allocator, size_t size, size_t alignment) {
	assert(alignment && !(alignment & (alignment - 1)));
	return allocator->allocate(allocator->user_data, size ? size : 1, alignment);
}

// spud_allocate, zeroed: what calloc was.
static inline void *spud_allocate_zeroed(const SPUD_ALLOCATOR *allocator, size_t size, size_t alignment) {
	void *memory = spud_allocate(allocator, size, alignment);
	if (memory)
		memset(memory, 0, size);
	return memory;
}

// Frees a block spud_allocate gave, with the same [size] and [alignment].
// NULL is nothing to free.
static inline void spud_free(const SPUD_ALLOCATOR *allocator, const void *memory, size_t size, size_t alignment) {
	if (memory)
		allocator->free(allocator->user_data, (void *)memory, size ? size : 1, alignment);
}

// [count] elements of [size] bytes, or NULL, also when the product would
// overflow.
static inline void *spud_allocate_array(const SPUD_ALLOCATOR *allocator, size_t count, size_t size, size_t alignment, bool zeroed) {
	if (size && count > SIZE_MAX / size)
		return NULL;
	return zeroed ? spud_allocate_zeroed(allocator, count * size, alignment) : spud_allocate(allocator, count * size, alignment);
}

static inline void spud_free_array(const SPUD_ALLOCATOR *allocator, const void *memory, size_t count, size_t size, size_t alignment) {
	spud_free(allocator, memory, count * size, alignment);
}

// What realloc was, for an array whose count the caller keeps: a block of
// [new_count] elements holding the first min(old, new) of [memory]'s, the old
// block freed. On failure, NULL, and [memory] is untouched and still the
// caller's. [memory] may be NULL with an [old_count] of 0. Elements past the
// old count are undefined.
static inline void *spud_reallocate_array(
    const SPUD_ALLOCATOR *allocator,
    void *memory,
    size_t old_count,
    size_t new_count,
    size_t size,
    size_t alignment) {
	void *moved = spud_allocate_array(allocator, new_count, size, alignment, false);
	if (!moved)
		return NULL;
	if (memory) {
		memcpy(moved, memory, (old_count < new_count ? old_count : new_count) * size);
		spud_free_array(allocator, memory, old_count, size, alignment);
	}
	return moved;
}

// A copy of [string] made through [allocator], or NULL if it can't be made.
// [string] is not NULL.
static inline char *spud_string_copy(const SPUD_ALLOCATOR *allocator, const char *string) {
	size_t size = strlen(string) + 1;
	char *copy  = (char *)spud_allocate(allocator, size, 1);
	if (copy)
		memcpy(copy, string, size);
	return copy;
}

// Frees a copy spud_string_copy made. NULL is nothing to free.
static inline void spud_string_free(const SPUD_ALLOCATOR *allocator, const char *string) {
	if (string)
		spud_free(allocator, string, strlen(string) + 1, 1);
}

// [allocator] as eqstd takes one, for an EQ_ARRAY (or an EQ_STRING) whose
// memory is the caller's. SPUD_ALLOCATOR's three fields are EQ_ALLOCATOR's
// first three, with the same function types, and eqstd grows a block by
// allocating, copying and freeing when `resize` is NULL, as
// spud_reallocate_array does. An array keeps a pointer to its EQ_ALLOCATOR,
// so the result is stored beside the array, in the object that owns both
// (never a local that the array outlives). eqstd keeps SPUD_ALLOCATOR's
// contract: it never asks for 0 bytes, and frees each block with the size and
// alignment it was allocated with (EQ_ARRAY_ALIGNMENT of the element's size
// for an array, a multiple of the element's alignment).
static inline EQ_ALLOCATOR spud_eq_allocator(const SPUD_ALLOCATOR *allocator) {
	EQ_ALLOCATOR eq = {allocator->user_data, allocator->allocate, allocator->free, NULL};
	return eq;
}

// One element of type T, or an array of [n], made and freed through [a].
#define SPUD_NEW(a, T) ((T *)spud_allocate((a), sizeof(T), SPUD_ALIGNOF(T)))
#define SPUD_NEW_ZEROED(a, T) ((T *)spud_allocate_zeroed((a), sizeof(T), SPUD_ALIGNOF(T)))
#define SPUD_DELETE(a, T, p) spud_free((a), (p), sizeof(T), SPUD_ALIGNOF(T))
#define SPUD_NEW_ARRAY(a, T, n) ((T *)spud_allocate_array((a), (n), sizeof(T), SPUD_ALIGNOF(T), false))
#define SPUD_NEW_ARRAY_ZEROED(a, T, n) ((T *)spud_allocate_array((a), (n), sizeof(T), SPUD_ALIGNOF(T), true))
#define SPUD_DELETE_ARRAY(a, T, p, n) spud_free_array((a), (p), (n), sizeof(T), SPUD_ALIGNOF(T))
#define SPUD_RESIZE_ARRAY(a, T, p, old_n, new_n) \
	((T *)spud_reallocate_array((a), (p), (old_n), (new_n), sizeof(T), SPUD_ALIGNOF(T)))

// Gives [head] a debug name: a copy of [name] made through its allocator,
// freeing the one it had. NULL removes the name. [name] may be the head's
// own current name. SPUDRESULT_OUT_OF_MEMORY leaves the old name in place.
static inline SPUDRESULT spud_object_name_set(spud_object_head *head, const char *name) {
	char *copy = NULL;
	if (name) {
		copy = spud_string_copy(&head->allocator, name);
		if (!copy)
			return SPUDRESULT_OUT_OF_MEMORY;
	}
	spud_string_free(&head->allocator, head->debug_name);
	head->debug_name = copy;
	return SPUD_SUCCESS;
}

#endif // SPUDALLOCATOR_H
