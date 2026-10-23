/*
 * spudperf_test — SpudPerf's monotonic clocks and the waits on them.
 *
 * Needs no hardware and nobody watching, so unlike its neighbours it is a
 * ctest. What it can't check is how late a wait may return: that is the
 * operating system's scheduler's, and a loaded machine is later than an idle
 * one. So lateness is printed, and only a wait that returns early, or is
 * late by so much that it can't have waited on the right clock, fails.
 *
 * Exits 0 when every check passes, 1 when one fails.
 */

#include <spudperf.h>

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int failures = 0;

#define CHECK(cond)                                                              \
	do {                                                                         \
		if (!(cond)) {                                                           \
			fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
			++failures;                                                          \
		}                                                                        \
	} while (0)

#define NS_PER_MS 1000000ULL

/* A wait this late was not a wait for the time asked for: on another clock,
 * or in another unit. Far beyond what a busy machine adds. */
#define ABSURDLY_LATE_NS (2000 * NS_PER_MS)

static void test_clock_does_not_go_back(void) {
	uint64_t previous = spudperf_get_monotonic_time_ns();
	int went_back     = 0;
	for (int i = 0; i < 100000; ++i) {
		const uint64_t now = spudperf_get_monotonic_time_ns();
		if (now < previous)
			++went_back;
		previous = now;
	}
	CHECK(went_back == 0);
}

/* Waits for a point `ahead_ns` from now and returns how late it came back,
 * checking that it was not early. */
static uint64_t wait_ahead(uint64_t ahead_ns) {
	const uint64_t deadline = spudperf_get_monotonic_time_ns() + ahead_ns;
	spudperf_sleep_until_monotonic_ns(deadline);
	const uint64_t after = spudperf_get_monotonic_time_ns();

	CHECK(after >= deadline);
	if (after < deadline)
		return 0;

	const uint64_t late = after - deadline;
	CHECK(late < ABSURDLY_LATE_NS);
	return late;
}

static void test_waits_until_the_time(void) {
	const uint64_t waits_ns[] = {1 * NS_PER_MS, 5 * NS_PER_MS, 20 * NS_PER_MS, 100 * NS_PER_MS, 250000ULL /* a quarter of a millisecond */};
	for (int i = 0; i < 5; ++i) {
		const uint64_t late = wait_ahead(waits_ns[i]);
		printf("spudperf_test: a wait of %" PRIu64 " us came back %" PRIu64 " us after its time\n", waits_ns[i] / 1000, late / 1000);
	}
}

/* A time already reached is not waited for. */
static void test_past_times_return_at_once(void) {
	const uint64_t before = spudperf_get_monotonic_time_ns();
	spudperf_sleep_until_monotonic_ns(0);
	spudperf_sleep_until_monotonic_ns(before);
	if (before > 1000 * NS_PER_MS)
		spudperf_sleep_until_monotonic_ns(before - 1000 * NS_PER_MS);
	const uint64_t after = spudperf_get_monotonic_time_ns();

	/* Three calls that should each be a clock read or two. */
	CHECK(after - before < 100 * NS_PER_MS);
}

/* Waiting for one point after another, each worked out from the first and
 * not from when the last wait came back, stays on time however late any one
 * wait was: the lateness does not add up. */
static void test_points_in_time_do_not_drift(void) {
	enum { STEPS = 30 };
	const uint64_t step_ns = 4 * NS_PER_MS;
	const uint64_t start   = spudperf_get_monotonic_time_ns();

	int early = 0;
	for (int i = 1; i <= STEPS; ++i) {
		const uint64_t deadline = start + step_ns * (uint64_t)i;
		spudperf_sleep_until_monotonic_ns(deadline);
		if (spudperf_get_monotonic_time_ns() < deadline)
			++early;
	}
	const uint64_t total = spudperf_get_monotonic_time_ns() - start;

	CHECK(early == 0);
	CHECK(total >= step_ns * STEPS);
	CHECK(total < step_ns * STEPS + ABSURDLY_LATE_NS);
	printf("spudperf_test: %d waits %" PRIu64 " us apart took %" PRIu64 " us, %" PRIu64 " us over\n", (int)STEPS, step_ns / 1000, total / 1000,
	       (total - step_ns * STEPS) / 1000);
}

/* The same on the millisecond clock, which is its own clock: the times are
 * readings of spudperf_get_current_time_milliseconds(). */
static void test_millisecond_waits(void) {
	const uint64_t waits_ms[] = {1, 5, 20, 100};
	for (int i = 0; i < 4; ++i) {
		const uint64_t deadline = spudperf_get_current_time_milliseconds() + waits_ms[i];
		spudperf_sleep_until_monotonic_ms(deadline);
		const uint64_t after = spudperf_get_current_time_milliseconds();

		CHECK(after >= deadline);
		CHECK(after - deadline < ABSURDLY_LATE_NS / NS_PER_MS);
		printf("spudperf_test: a wait of %" PRIu64 " ms came back %" PRIu64 " ms after its time\n", waits_ms[i], after >= deadline ? after - deadline : 0);
	}

	/* Times already reached. */
	const uint64_t before_ns = spudperf_get_monotonic_time_ns();
	const uint64_t now_ms    = spudperf_get_current_time_milliseconds();
	spudperf_sleep_until_monotonic_ms(0);
	spudperf_sleep_until_monotonic_ms(now_ms);
	if (now_ms > 1000)
		spudperf_sleep_until_monotonic_ms(now_ms - 1000);
	CHECK(spudperf_get_monotonic_time_ns() - before_ns < 100 * NS_PER_MS);

	/* Points 4 ms apart counted from the first: no drift, and never early. */
	enum { STEPS = 30 };
	const uint64_t start = spudperf_get_current_time_milliseconds();
	int early            = 0;
	for (int i = 1; i <= STEPS; ++i) {
		const uint64_t deadline = start + 4 * (uint64_t)i;
		spudperf_sleep_until_monotonic_ms(deadline);
		if (spudperf_get_current_time_milliseconds() < deadline)
			++early;
	}
	const uint64_t total = spudperf_get_current_time_milliseconds() - start;
	CHECK(early == 0);
	CHECK(total >= 4 * STEPS);
	CHECK(total < 4 * STEPS + ABSURDLY_LATE_NS / NS_PER_MS);
	printf("spudperf_test: %d waits 4 ms apart took %" PRIu64 " ms\n", (int)STEPS, total);
}

/* The CPU's counter only goes up, and moves over a busy loop. */
static void test_clock_cycles(void) {
	uint64_t previous   = spudperf_catch_current_clock_cycle();
	const uint64_t first = previous;
	int went_back       = 0;
	volatile uint64_t sink = 0;
	for (int i = 0; i < 100000; ++i) {
		sink += (uint64_t)i;
		const uint64_t now = spudperf_catch_current_clock_cycle();
		if (now < previous)
			went_back = 1;
		previous = now;
	}
	CHECK(went_back == 0);
	CHECK(previous > first);
}

/* Memory in use is reported, the peak is never below it, and a large block
 * written whole raises both. Each output may be NULL. */
static void test_ram_usage(void) {
	uint64_t current = 0;
	uint64_t peak    = 0;
	spudperf_get_ram_usage(&current, &peak);
	CHECK(current > 0);
	CHECK(peak >= current);
	spudperf_get_ram_usage(NULL, NULL);
	uint64_t only = 0;
	spudperf_get_ram_usage(&only, NULL);
	CHECK(only > 0);
	only = 0;
	spudperf_get_ram_usage(NULL, &only);
	CHECK(only >= current / 2);

	const size_t size = 64u << 20;
	unsigned char *block = (unsigned char *)malloc(size);
	CHECK(block != NULL);
	if (!block)
		return;
	memset(block, 1, size);
	uint64_t after      = 0;
	uint64_t peak_after = 0;
	spudperf_get_ram_usage(&after, &peak_after);
	CHECK(after >= current + size / 2);
	CHECK(peak_after >= after);
	CHECK(peak_after >= peak);
	printf("spudperf_test: %llu MiB in use, %llu MiB with a 64 MiB block written\n", (unsigned long long)(current >> 20),
	    (unsigned long long)(after >> 20));
	free(block);
}

int main(void) {
	test_clock_does_not_go_back();
	test_waits_until_the_time();
	test_past_times_return_at_once();
	test_points_in_time_do_not_drift();
	test_millisecond_waits();
	test_clock_cycles();
	test_ram_usage();

	if (failures) {
		fprintf(stderr, "spudperf_test: %d failure(s)\n", failures);
		return 1;
	}
	printf("spudperf_test: all passed\n");
	return 0;
}
