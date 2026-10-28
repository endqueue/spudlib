/*
 * spudmemory_test — SpudMemory's virtual memory calls.
 *
 * Every argument check of each call, and the rules spudmemory.h states for
 * every backend: committed pages read zero, a second commit keeps what a
 * page holds, a decommit and commit again reads zero, a decommit may cover
 * pages that aren't committed, and a reservation can be far larger than
 * what is committed in it. Needs nothing but the machine.
 *
 * Exits 0 when every check passes, 1 when one fails.
 */

#include <spudmemory.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond)                                                              \
	do {                                                                         \
		if (!(cond)) {                                                           \
			fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
			++failures;                                                          \
		}                                                                        \
	} while (0)

#define CHECK_RESULT(call, expected)                                                                                         \
	do {                                                                                                                     \
		const SPUDRESULT r_ = (call);                                                                                        \
		if (r_ != (expected)) {                                                                                              \
			fprintf(stderr, "%s:%d: %s returned %s, expected %s\n", __FILE__, __LINE__, #call, spudresult_str(r_), #expected); \
			++failures;                                                                                                      \
		}                                                                                                                    \
	} while (0)

/* Whether [size] bytes at [p] are all [value]. */
static int all_bytes_are(
    const uint8_t *p,
    uint64_t size,
    uint8_t value) {
	for (uint64_t i = 0; i < size; ++i) {
		if (p[i] != value)
			return 0;
	}
	return 1;
}

static void test_page_size(void) {
	const uint32_t page = spudmem_get_page_size();
	CHECK(page != 0);
	CHECK((page & (page - 1)) == 0);
	CHECK(page == spudmem_get_page_size());
	printf("spudmemory_test: page size %u\n", (unsigned)page);
}

/* A power of two, a page multiple, and where a reservation starts. */
static void test_reserve_granularity(void) {
	const uint32_t page        = spudmem_get_page_size();
	const uint32_t granularity = spudmem_get_reserve_granularity();
	CHECK(granularity != 0);
	CHECK((granularity & (granularity - 1)) == 0);
	CHECK(granularity % page == 0);
	CHECK(granularity == spudmem_get_reserve_granularity());
	void *address = NULL;
	CHECK_RESULT(spudmem_reserve(page, &address), SPUD_SUCCESS);
	CHECK(address != NULL);
	CHECK((uint64_t)(uintptr_t)address % granularity == 0);
	if (address)
		CHECK_RESULT(spudmem_release(address, page), SPUD_SUCCESS);
	printf("spudmemory_test: reserve granularity %u\n", (unsigned)granularity);
}

static void test_reserve_checks(void) {
	const uint64_t page = spudmem_get_page_size();
	void *address       = (void *)&address;

	CHECK_RESULT(spudmem_reserve(0, &address), SPUDRESULT_ZERO_SIZE);
	CHECK(address == NULL);
	address = (void *)&address;
	CHECK_RESULT(spudmem_reserve(page + 1, &address), SPUDRESULT_SPUDMEM_NOT_PAGE_ALIGNED);
	CHECK(address == NULL);
	CHECK_RESULT(spudmem_reserve(page - 1, &address), SPUDRESULT_SPUDMEM_NOT_PAGE_ALIGNED);
	CHECK_RESULT(spudmem_reserve(page, NULL), SPUDRESULT_NULL_OUTPUT_PARAMETER);
	/* Checked in parameter order: the size before the output. */
	CHECK_RESULT(spudmem_reserve(0, NULL), SPUDRESULT_ZERO_SIZE);
}

/* The same four checks, in the same order, for each call that takes an
 * address and a size. */
static void test_range_checks(
    const char *name,
    SPUDRESULT (*call)(void *, uint64_t),
    uint8_t *base,
    uint64_t page) {
	printf("spudmemory_test: %s's checks\n", name);
	CHECK_RESULT(call(NULL, page), SPUDRESULT_SPUDMEM_NULL_ADDRESS);
	CHECK_RESULT(call(base + 1, page), SPUDRESULT_SPUDMEM_NOT_PAGE_ALIGNED);
	CHECK_RESULT(call(base, 0), SPUDRESULT_ZERO_SIZE);
	CHECK_RESULT(call(base, page + 1), SPUDRESULT_SPUDMEM_NOT_PAGE_ALIGNED);
	CHECK_RESULT(call(NULL, 0), SPUDRESULT_SPUDMEM_NULL_ADDRESS);
	CHECK_RESULT(call(base + 1, 0), SPUDRESULT_SPUDMEM_NOT_PAGE_ALIGNED);
}

static void test_reserve_commit_release(void) {
	const uint64_t page = spudmem_get_page_size();
	/* A gigabyte of address space and a few pages of memory in it. */
	const uint64_t reserved = SPUDMEM_GiB(1);
	void *address           = NULL;
	CHECK_RESULT(spudmem_reserve(reserved, &address), SPUD_SUCCESS);
	if (!address)
		return;
	CHECK(((uintptr_t)address & (page - 1)) == 0);
	uint8_t *base = (uint8_t *)address;

	test_range_checks("spudmem_commit", spudmem_commit, base, page);
	test_range_checks("spudmem_decommit", spudmem_decommit, base, page);

	/* Committed pages read zero, and can be written. */
	CHECK_RESULT(spudmem_commit(base, 4 * page), SPUD_SUCCESS);
	CHECK(all_bytes_are(base, 4 * page, 0));
	memset(base, 0xA5, 4 * page);
	CHECK(all_bytes_are(base, 4 * page, 0xA5));

	/* A second commit over committed pages, and one more, keeps what they
	 * hold and zeroes only the new page. */
	CHECK_RESULT(spudmem_commit(base, 5 * page), SPUD_SUCCESS);
	CHECK(all_bytes_are(base, 4 * page, 0xA5));
	CHECK(all_bytes_are(base + 4 * page, page, 0));

	/* Decommitted and committed again, a page reads zero; its neighbours
	 * are untouched. */
	CHECK_RESULT(spudmem_decommit(base + page, page), SPUD_SUCCESS);
	CHECK_RESULT(spudmem_commit(base + page, page), SPUD_SUCCESS);
	CHECK(all_bytes_are(base + page, page, 0));
	CHECK(all_bytes_are(base, page, 0xA5));
	CHECK(all_bytes_are(base + 2 * page, 2 * page, 0xA5));

	/* A decommit may cover pages that were never committed. */
	CHECK_RESULT(spudmem_decommit(base, 64 * page), SPUD_SUCCESS);
	CHECK_RESULT(spudmem_commit(base, page), SPUD_SUCCESS);
	CHECK(all_bytes_are(base, page, 0));

	/* Pages far into the reservation, and its last one. */
	uint8_t *middle = base + reserved / 2;
	uint8_t *last   = base + reserved - page;
	CHECK_RESULT(spudmem_commit(middle, page), SPUD_SUCCESS);
	CHECK_RESULT(spudmem_commit(last, page), SPUD_SUCCESS);
	middle[0]        = 1;
	last[page - 1]   = 2;
	CHECK(middle[0] == 1);
	CHECK(last[page - 1] == 2);

	test_range_checks("spudmem_release", spudmem_release, base, page);
	CHECK_RESULT(spudmem_release(base, reserved), SPUD_SUCCESS);
}

/* A reservation larger than this machine's memory is address space only. */
static void test_large_reservation(void) {
	const uint64_t reserved = SPUDMEM_GiB(64);
	void *address           = NULL;
	const SPUDRESULT r      = spudmem_reserve(reserved, &address);
	if (r != SPUD_SUCCESS) {
		/* A 32-bit process, or a system that limits address space, can
		 * refuse; that is an answer, not a failure of the call. */
		printf("spudmemory_test: 64 GiB not reserved: %s\n", spudresult_str(r));
		CHECK(r == SPUDRESULT_OUT_OF_MEMORY);
		return;
	}
	const uint64_t page = spudmem_get_page_size();
	CHECK_RESULT(spudmem_commit(address, page), SPUD_SUCCESS);
	((uint8_t *)address)[0] = 7;
	CHECK(((uint8_t *)address)[0] == 7);
	CHECK_RESULT(spudmem_release(address, reserved), SPUD_SUCCESS);
}

int main(void) {
	test_page_size();
	test_reserve_granularity();
	test_reserve_checks();
	test_reserve_commit_release();
	test_large_reservation();
	if (failures) {
		fprintf(stderr, "spudmemory_test: %d failure(s)\n", failures);
		return 1;
	}
	printf("spudmemory_test: all passed\n");
	return 0;
}
