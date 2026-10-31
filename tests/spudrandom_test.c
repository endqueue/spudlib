/*
 * spudrandom_test - SpudRandom: the operating system's secure random bytes.
 *
 * Randomness can't be proved by a test; what is checked is the contract:
 * every byte asked for is written (a long fill, past each platform's piece,
 * leaves no run of its old contents), two fills differ, nothing past the
 * size is touched, and the argument checks. Exits 0 when every check
 * passes, 1 when one fails.
 */

#include <spudrandom.h>

#include <stdio.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond)                                                                                                                                            \
	do {                                                                                                                                                       \
		if (!(cond)) {                                                                                                                                         \
			fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond);                                                                           \
			++failures;                                                                                                                                        \
		}                                                                                                                                                      \
	} while (0)

int main(void) {
	CHECK(spudrandom_fill(NULL, 0) == SPUD_SUCCESS);
	CHECK(spudrandom_fill(NULL, 16) == SPUDRESULT_NULL_OUTPUT_PARAMETER);

	/* 4,096 bytes and a guard: no run of 64 zero bytes is left of what was
	 * there (a chance of 2^-512 for each), and the guard is untouched. */
	static unsigned char first[4096 + 1];
	static unsigned char second[4096 + 1];
	memset(first, 0, sizeof(first));
	memset(second, 0, sizeof(second));
	first[4096]  = 0xa5;
	second[4096] = 0xa5;
	CHECK(spudrandom_fill(first, 4096) == SPUD_SUCCESS);
	CHECK(spudrandom_fill(second, 4096) == SPUD_SUCCESS);
	CHECK(first[4096] == 0xa5);
	CHECK(second[4096] == 0xa5);
	for (size_t at = 0; at + 64 <= 4096; at += 64) {
		size_t zeros = 0;
		for (size_t i = 0; i < 64; ++i)
			zeros += first[at + i] == 0;
		CHECK(zeros < 64);
	}
	CHECK(memcmp(first, second, 4096) != 0);

	/* One byte, a size no platform's piece divides into. */
	unsigned char one[2] = {0, 0x5a};
	CHECK(spudrandom_fill(one, 1) == SPUD_SUCCESS);
	CHECK(one[1] == 0x5a);

	if (failures) {
		fprintf(stderr, "spudrandom_test: %d failure(s)\n", failures);
		return 1;
	}
	printf("spudrandom_test: all passed\n");
	return 0;
}
