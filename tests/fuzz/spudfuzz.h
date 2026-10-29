/*
 * spudfuzz.h - what SpudNet's fuzz targets share.
 *
 * Each target in this folder defines LLVMFuzzerTestOneInput over one of
 * SpudNet's internal parsers of bytes that arrive from the network (or from
 * the caller, for the proxy URL), and checks what the parser must keep
 * whatever the input: no out-of-bounds read, no leak and no wrong free
 * through the caller's allocator, and the answers a second, plainer reading
 * of the same bytes gives. A failed check prints and aborts, which libFuzzer
 * reports as a crash and the replay test (spudfuzz_replay.c) as a failure.
 *
 * Built two ways (CMakeLists.txt): with -fsanitize=fuzzer under
 * SPUDLIB_BUILD_FUZZERS, and with spudfuzz_replay.c's main as a ctest that
 * feeds it every file of its seed corpus and its regression folder.
 */

#ifndef SPUDFUZZ_H
#define SPUDFUZZ_H

#include "../spudtest_allocator.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define SPUDFUZZ_CHECK(cond)                                                          \
	do {                                                                              \
		if (!(cond)) {                                                                \
			fprintf(stderr, "%s:%d: SPUDFUZZ_CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
			abort();                                                                  \
		}                                                                             \
	} while (0)

/* An input read a piece at a time; what is asked for past its end is 0. */
typedef struct spudfuzz_reader {
	const uint8_t *data;
	size_t         size;
} spudfuzz_reader;

static uint8_t spudfuzz_byte(spudfuzz_reader *reader) {
	if (reader->size == 0)
		return 0;
	uint8_t byte = reader->data[0];
	reader->data++;
	reader->size--;
	return byte;
}

/* A tracking allocator that refuses every allocation after `refuse_after`
 * of them, or none for SIZE_MAX: the out-of-memory paths are fuzzed too. */
static SPUD_ALLOCATOR spudfuzz_allocator(spudtest_allocator *tracker, size_t refuse_after) {
	SPUD_ALLOCATOR allocator = spudtest_allocator_make(tracker);
	tracker->refuse_after    = refuse_after;
	return allocator;
}

/* A copy of `size` bytes and a terminator, in a block of exactly that size,
 * so a read past the terminator is AddressSanitizer's to see. The caller
 * frees it with free(). */
static char *spudfuzz_terminated(const uint8_t *data, size_t size) {
	char *copy = (char *)malloc(size + 1);
	if (!copy)
		abort();
	if (size)
		memcpy(copy, data, size);
	copy[size] = '\0';
	return copy;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

#endif // SPUDFUZZ_H
