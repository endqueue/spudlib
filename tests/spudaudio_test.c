/*
 * spudaudio_test — SpudAudio without a sound.
 *
 * Every call but the two that change what is heard or how a device is set:
 * spudaudio_stream_start() is never called, and nor is
 * spudaudio_set_device_sample_rate() but with arguments it refuses. That
 * covers each call's argument checks; the instance, enumeration and device
 * properties in both directions; timing caps; a probe of the default output
 * device's own format, following its suggestions; a stream created stopped,
 * its configuration, latency and status read, and destroyed; and setting
 * and clearing the device-event callback. Every block goes through a
 * counting allocator, checked at the end. spudaudio_sine is the test that
 * plays something.
 *
 * With no output device, the parts that need one are skipped and the rest
 * still run. A build with no native audio API checks that the stub refuses.
 *
 * Exits 0 when every check passes, 1 when one fails.
 */

#include <spudaudio.h>

#include "spudtest_allocator.h"

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

#if SPUDAUDIO_COMPILE_COREAUDIO
static const SPUDAUDIO_NATIVE_API g_api        = SPUDAUDIO_NATIVE_API_COREAUDIO;
static const SPUDAUDIO_THREAD_PRIORITY g_thread = SPUDAUDIO_THREAD_PRIORITY_PLATFORM;
#elif SPUDAUDIO_COMPILE_WASAPI
static const SPUDAUDIO_NATIVE_API g_api        = SPUDAUDIO_NATIVE_API_WASAPI;
static const SPUDAUDIO_THREAD_PRIORITY g_thread = SPUDAUDIO_THREAD_PRIORITY_NORMAL;
#elif SPUDAUDIO_COMPILE_ALSA
static const SPUDAUDIO_NATIVE_API g_api        = SPUDAUDIO_NATIVE_API_ALSA;
static const SPUDAUDIO_THREAD_PRIORITY g_thread = SPUDAUDIO_THREAD_PRIORITY_NORMAL;
#else
static const SPUDAUDIO_NATIVE_API g_api        = SPUDAUDIO_NATIVE_API_NONE;
static const SPUDAUDIO_THREAD_PRIORITY g_thread = SPUDAUDIO_THREAD_PRIORITY_NORMAL;
#endif

/* Never called: no stream is started. */
static void silent_callback(
    spudaudio_stream stream,
    void *frames,
    uint32_t frame_count,
    const SPUDAUDIO_CALLBACK_INFO *info,
    void *user_data) {
	(void)stream;
	(void)frames;
	(void)frame_count;
	(void)info;
	*(int *)user_data = 1;
}

static void device_event_callback(
    SPUDAUDIO_DEVICE_EVENT event,
    const char *persistent_id,
    SPUDAUDIO_DIRECTION direction,
    SPUDAUDIO_DEFAULT_ROLES roles,
    void *user_data) {
	(void)event;
	(void)persistent_id;
	(void)direction;
	(void)roles;
	(void)user_data;
}

static void test_sample_format_sizes(void) {
	CHECK(spudaudio_sample_format_byte_size(SPUDAUDIO_SAMPLE_FORMAT_UNKNOWN) == 0);
	CHECK(spudaudio_sample_format_byte_size(SPUDAUDIO_SAMPLE_FORMAT_S16) == 2);
	CHECK(spudaudio_sample_format_byte_size(SPUDAUDIO_SAMPLE_FORMAT_S24_PACKED) == 3);
	CHECK(spudaudio_sample_format_byte_size(SPUDAUDIO_SAMPLE_FORMAT_S24_32_LSB) == 4);
	CHECK(spudaudio_sample_format_byte_size(SPUDAUDIO_SAMPLE_FORMAT_S32) == 4);
	CHECK(spudaudio_sample_format_byte_size(SPUDAUDIO_SAMPLE_FORMAT_F32) == 4);
	CHECK(spudaudio_sample_format_byte_size((SPUDAUDIO_SAMPLE_FORMAT)99) == 0);
}

static void test_create_instance_checks(const SPUD_ALLOCATOR *allocator) {
	spudaudio_instance instance = NULL;
	SPUDAUDIO_INSTANCE_DESC desc;
	memset(&desc, 0, sizeof(desc));

	CHECK_RESULT(spudaudio_create_instance(NULL, &instance), SPUDRESULT_NULL_DESC);
	/* A zeroed desc is refused for its base, then one with only its base
	 * for its allocator. */
	CHECK_RESULT(spudaudio_create_instance(&desc, &instance), SPUDRESULT_STRUCT_WRONG_TYPE);
	desc.base.type = SPUD_STRUCT_TYPE_AUDIO_INSTANCE_DESC;
	desc.base.next = (const SPUD_STRUCT_BASE *)&desc;
	CHECK_RESULT(spudaudio_create_instance(&desc, &instance), SPUDRESULT_STRUCT_UNKNOWN_IN_CHAIN);
	desc.base.next = NULL;
	CHECK_RESULT(spudaudio_create_instance(&desc, NULL), SPUDRESULT_NULL_OUTPUT_PARAMETER);
	CHECK_RESULT(spudaudio_create_instance(&desc, &instance), SPUDRESULT_NULL_ALLOCATOR);
	SPUD_ALLOCATOR missing = *allocator;
	missing.free           = NULL;
	desc.allocator         = &missing;
	CHECK_RESULT(spudaudio_create_instance(&desc, &instance), SPUDRESULT_INVALID_ALLOCATOR);
	desc.allocator  = allocator;
	desc.native_api = SPUDAUDIO_NATIVE_API_NONE;
	CHECK(spudaudio_create_instance(&desc, &instance) != SPUD_SUCCESS);
	CHECK(instance == NULL);
	desc.native_api = (SPUDAUDIO_NATIVE_API)99;
	CHECK(spudaudio_create_instance(&desc, &instance) != SPUD_SUCCESS);
	CHECK(instance == NULL);

	CHECK_RESULT(spudaudio_destroy_instance(NULL), SPUDRESULT_SAUD_INVALID_INSTANCE);
	CHECK(spudaudio_get_native_api(NULL) == SPUDAUDIO_NATIVE_API_NONE);
}

/* The calls that take a device, a stream or an instance, given none. */
static void test_null_handles(void) {
	spudaudio_device *devices = NULL;
	uint32_t count            = 0;
	CHECK_RESULT(spudaudio_enumerate_devices(NULL, SPUDAUDIO_DIRECTION_OUTPUT, &devices, &count), SPUDRESULT_SAUD_INVALID_INSTANCE);

	SPUDAUDIO_DEVICE_PROPERTIES props = {.base = {SPUD_STRUCT_TYPE_AUDIO_DEVICE_PROPERTIES, NULL}};
	CHECK_RESULT(spudaudio_get_device_properties(NULL, &props), SPUDRESULT_SAUD_INVALID_DEVICE);
	CHECK_RESULT(spudaudio_set_device_sample_rate(NULL, 48000), SPUDRESULT_SAUD_INVALID_DEVICE);

	SPUDAUDIO_FORMAT format       = {0};
	SPUDAUDIO_TIMING_CAPS caps     = {.base = {SPUD_STRUCT_TYPE_AUDIO_TIMING_CAPS, NULL}};
	CHECK_RESULT(spudaudio_get_timing_caps(NULL, &format, SPUDAUDIO_STREAM_FLAG_NONE, &caps), SPUDRESULT_SAUD_INVALID_DEVICE);

	SPUDAUDIO_STREAM_CONFIG config = {.base = {SPUD_STRUCT_TYPE_AUDIO_STREAM_CONFIG, NULL}};
	SPUDAUDIO_STREAM_STATUS status;
	uint32_t latency = 0;
	CHECK_RESULT(spudaudio_stream_start(NULL), SPUDRESULT_SAUD_INVALID_STREAM);
	CHECK_RESULT(spudaudio_stream_stop(NULL), SPUDRESULT_SAUD_INVALID_STREAM);
	CHECK_RESULT(spudaudio_stream_get_config(NULL, &config), SPUDRESULT_SAUD_INVALID_STREAM);
	CHECK_RESULT(spudaudio_stream_get_latency_frames(NULL, &latency), SPUDRESULT_SAUD_INVALID_STREAM);
	CHECK_RESULT(spudaudio_stream_get_status(NULL, &status), SPUDRESULT_SAUD_INVALID_STREAM);
	spudaudio_destroy_stream(NULL);

	CHECK_RESULT(spudaudio_set_device_event_callback(NULL, device_event_callback, NULL), SPUDRESULT_SAUD_INVALID_INSTANCE);
}

/* Each direction's devices and their properties. Returns the output device
 * holding the general default role, else the first, else NULL. */
static spudaudio_device test_enumerate(spudaudio_instance instance) {
	spudaudio_device *devices = NULL;
	uint32_t count            = 0;
	CHECK_RESULT(spudaudio_enumerate_devices(instance, SPUDAUDIO_DIRECTION_OUTPUT, NULL, &count), SPUDRESULT_NULL_OUTPUT_PARAMETER);
	CHECK_RESULT(spudaudio_enumerate_devices(instance, SPUDAUDIO_DIRECTION_OUTPUT, &devices, NULL), SPUDRESULT_NULL_OUTPUT_PARAMETER);
	CHECK_RESULT(spudaudio_enumerate_devices(instance, (SPUDAUDIO_DIRECTION)99, &devices, &count), SPUDRESULT_DESC_INVALID_PARAMETERS);

	spudaudio_device chosen = NULL;
	const SPUDAUDIO_DIRECTION directions[2] = {SPUDAUDIO_DIRECTION_OUTPUT, SPUDAUDIO_DIRECTION_INPUT};
	for (int d = 0; d < 2; ++d) {
		CHECK_RESULT(spudaudio_enumerate_devices(instance, directions[d], &devices, &count), SPUD_SUCCESS);
		printf("spudaudio_test: %u %s device(s)\n", (unsigned)count, d == 0 ? "output" : "input");
		for (uint32_t i = 0; i < count; ++i) {
			SPUDAUDIO_DEVICE_PROPERTIES props;
			memset(&props, 0, sizeof(props));
			props.base.type = SPUD_STRUCT_TYPE_AUDIO_DEVICE_PROPERTIES;
			CHECK_RESULT(spudaudio_get_device_properties(devices[i], &props), SPUD_SUCCESS);
			CHECK(props.direction == directions[d]);
			CHECK(memchr(props.name, '\0', sizeof(props.name)) != NULL);
			CHECK(props.persistent_id[0] != '\0');
			printf("spudaudio_test:   %s\n", props.name);
			if (d == 0 && !chosen)
				chosen = devices[i];
			if (d == 0 && (props.default_roles & SPUDAUDIO_DEFAULT_ROLE_GENERAL))
				chosen = devices[i];
		}
		/* The same endpoint keeps its handle across enumerations. */
		spudaudio_device first = count ? devices[0] : NULL;
		uint32_t again         = 0;
		CHECK_RESULT(spudaudio_enumerate_devices(instance, directions[d], &devices, &again), SPUD_SUCCESS);
		if (first && again)
			CHECK(devices[0] == first);
	}

	if (chosen) {
		SPUDAUDIO_DEVICE_PROPERTIES props;
		memset(&props, 0, sizeof(props));
		CHECK_RESULT(spudaudio_get_device_properties(chosen, NULL), SPUDRESULT_NULL_OUTPUT_PARAMETER);
		CHECK_RESULT(spudaudio_get_device_properties(chosen, &props), SPUDRESULT_STRUCT_WRONG_TYPE);
		/* A rate of 0 is refused before anything is set. */
		CHECK_RESULT(spudaudio_set_device_sample_rate(chosen, 0), SPUDRESULT_DESC_INVALID_PARAMETERS);
	}
	return chosen;
}

/* A probe of [device]'s own format (or float stereo where it reports none),
 * following its suggestions, then a stream created stopped. */
static void test_stream(spudaudio_device device) {
	SPUDAUDIO_DEVICE_PROPERTIES props;
	memset(&props, 0, sizeof(props));
	props.base.type = SPUD_STRUCT_TYPE_AUDIO_DEVICE_PROPERTIES;
	CHECK_RESULT(spudaudio_get_device_properties(device, &props), SPUD_SUCCESS);

	SPUDAUDIO_STREAM_DESC desc;
	memset(&desc, 0, sizeof(desc));
	desc.base.type = SPUD_STRUCT_TYPE_AUDIO_STREAM_DESC;
	desc.device    = device;
	if (props.native_format.sample_format != SPUDAUDIO_SAMPLE_FORMAT_UNKNOWN) {
		desc.format = props.native_format;
	} else {
		desc.format.sample_format = SPUDAUDIO_SAMPLE_FORMAT_F32;
		desc.format.sample_rate   = 48000;
		desc.format.channel_count = 2;
		desc.format.interleaved   = true;
	}
	desc.flags           = SPUDAUDIO_STREAM_FLAG_NONE;
	desc.thread.priority = g_thread;

	SPUDAUDIO_TIMING_CAPS caps;
	memset(&caps, 0, sizeof(caps));
	caps.base.type = SPUD_STRUCT_TYPE_AUDIO_TIMING_CAPS;
	CHECK_RESULT(spudaudio_get_timing_caps(device, NULL, desc.flags, &caps), SPUDRESULT_NULL_DESC);
	CHECK_RESULT(spudaudio_get_timing_caps(device, &desc.format, desc.flags, NULL), SPUDRESULT_NULL_OUTPUT_PARAMETER);
	SPUDRESULT r = spudaudio_get_timing_caps(device, &desc.format, desc.flags, &caps);
	if (r == SPUD_SUCCESS) {
		CHECK(caps.min_period_frames <= caps.max_period_frames);
		CHECK(caps.min_buffer_frames <= caps.max_buffer_frames);
		CHECK(caps.period_granularity_frames >= 1);
		desc.period_frames = caps.default_period_frames ? caps.default_period_frames : caps.min_period_frames;
		if (caps.max_buffer_frames) {
			desc.buffer_frames = desc.period_frames * 2;
			if (desc.buffer_frames < caps.min_buffer_frames)
				desc.buffer_frames = caps.min_buffer_frames;
			if (desc.buffer_frames > caps.max_buffer_frames)
				desc.buffer_frames = caps.max_buffer_frames;
		}
	} else {
		printf("spudaudio_test: timing caps: %s\n", spudresult_str(r));
		desc.period_frames = desc.format.sample_rate / 100;
		desc.buffer_frames = g_api == SPUDAUDIO_NATIVE_API_ALSA ? desc.period_frames * 2 : 0;
	}

	/* The probe's checks. */
	SPUDAUDIO_STREAM_CONFIG config;
	memset(&config, 0, sizeof(config));
	config.base.type = SPUD_STRUCT_TYPE_AUDIO_STREAM_CONFIG;
	CHECK_RESULT(spudaudio_probe_stream(&desc, NULL), SPUDRESULT_NULL_OUTPUT_PARAMETER);
	CHECK_RESULT(spudaudio_probe_stream(NULL, &config), SPUDRESULT_NULL_DESC);
	SPUDAUDIO_STREAM_DESC bad = desc;
	bad.device                = NULL;
	CHECK_RESULT(spudaudio_probe_stream(&bad, &config), SPUDRESULT_SAUD_INVALID_DEVICE);
	bad                 = desc;
	bad.base.type       = SPUD_STRUCT_TYPE_AUDIO_INSTANCE_DESC;
	CHECK_RESULT(spudaudio_probe_stream(&bad, &config), SPUDRESULT_STRUCT_WRONG_TYPE);
	bad                 = desc;
	bad.thread.priority = SPUDAUDIO_THREAD_PRIORITY_UNSPECIFIED;
	CHECK(spudaudio_probe_stream(&bad, &config) != SPUD_SUCCESS);
	bad                 = desc;
	bad.period_frames   = 0;
	CHECK_RESULT(spudaudio_probe_stream(&bad, &config), SPUDRESULT_SAUD_PERIOD_OUT_OF_RANGE);
	SPUDAUDIO_STREAM_CONFIG wrong;
	memset(&wrong, 0, sizeof(wrong));
	CHECK_RESULT(spudaudio_probe_stream(&desc, &wrong), SPUDRESULT_STRUCT_WRONG_TYPE);

	/* Follow the suggestions until a configuration is accepted. */
	int accepted = 0;
	for (int round = 0; round < 8 && !accepted; ++round) {
		memset(&config, 0, sizeof(config));
		config.base.type = SPUD_STRUCT_TYPE_AUDIO_STREAM_CONFIG;
		r                = spudaudio_probe_stream(&desc, &config);
		if (r == SPUD_SUCCESS)
			accepted = 1;
		else if (r == SPUDRESULT_SAUD_FORMAT_NOT_SUPPORTED && config.format.sample_format != SPUDAUDIO_SAMPLE_FORMAT_UNKNOWN)
			desc.format = config.format;
		else if (r == SPUDRESULT_SAUD_PERIOD_OUT_OF_RANGE && config.period_frames)
			desc.period_frames = config.period_frames;
		else if (r == SPUDRESULT_SAUD_BUFFER_OUT_OF_RANGE)
			desc.buffer_frames = config.buffer_frames;
		else
			break;
	}
	if (!accepted) {
		printf("spudaudio_test: no configuration accepted (%s); stream not created\n", spudresult_str(r));
		CHECK(accepted);
		return;
	}
	CHECK(config.period_frames == desc.period_frames);
	CHECK(config.max_callback_frames >= config.period_frames);

	/* Create checks: the callback is required to create, not to probe. */
	int called              = 0;
	spudaudio_stream stream = NULL;
	CHECK_RESULT(spudaudio_create_stream(&desc, NULL), SPUDRESULT_NULL_OUTPUT_PARAMETER);
	CHECK_RESULT(spudaudio_create_stream(&desc, &stream), SPUDRESULT_DESC_INVALID_PARAMETERS);
	CHECK(stream == NULL);
	desc.callback  = silent_callback;
	desc.user_data = &called;
	CHECK_RESULT(spudaudio_create_stream(&desc, &stream), SPUD_SUCCESS);
	if (!stream)
		return;

	SPUDAUDIO_STREAM_CONFIG granted;
	memset(&granted, 0, sizeof(granted));
	granted.base.type = SPUD_STRUCT_TYPE_AUDIO_STREAM_CONFIG;
	CHECK_RESULT(spudaudio_stream_get_config(stream, NULL), SPUDRESULT_NULL_OUTPUT_PARAMETER);
	CHECK_RESULT(spudaudio_stream_get_config(stream, &granted), SPUD_SUCCESS);
	/* Creation grants what the probe said it would. */
	CHECK(granted.period_frames == config.period_frames);
	CHECK(granted.buffer_frames == config.buffer_frames);
	CHECK(granted.max_callback_frames >= granted.period_frames);
	CHECK(granted.format.sample_rate == config.format.sample_rate);
	CHECK(granted.format.channel_count == config.format.channel_count);

	uint32_t latency = UINT32_MAX;
	CHECK_RESULT(spudaudio_stream_get_latency_frames(stream, NULL), SPUDRESULT_NULL_OUTPUT_PARAMETER);
	r = spudaudio_stream_get_latency_frames(stream, &latency);
	CHECK(r == SPUD_SUCCESS);

	SPUDAUDIO_STREAM_STATUS status;
	memset(&status, 0xFF, sizeof(status));
	CHECK_RESULT(spudaudio_stream_get_status(stream, NULL), SPUDRESULT_NULL_OUTPUT_PARAMETER);
	CHECK_RESULT(spudaudio_stream_get_status(stream, &status), SPUD_SUCCESS);
	CHECK(status.state == SPUDAUDIO_STREAM_STATE_STOPPED);
	CHECK(status.error == SPUD_SUCCESS);

	/* Stopping a stream that never started is allowed. */
	CHECK_RESULT(spudaudio_stream_stop(stream), SPUD_SUCCESS);
	spudaudio_destroy_stream(stream);
	CHECK(called == 0);
	printf("spudaudio_test: stream of %u Hz, %u channel(s), period %u, buffer %u, created and destroyed stopped\n",
	    (unsigned)granted.format.sample_rate, (unsigned)granted.format.channel_count, (unsigned)granted.period_frames,
	    (unsigned)granted.buffer_frames);
}

int main(void) {
	test_sample_format_sizes();

	spudtest_allocator tracker;
	SPUD_ALLOCATOR allocator = spudtest_allocator_make(&tracker);
	test_create_instance_checks(&allocator);
	test_null_handles();

	SPUDAUDIO_INSTANCE_DESC desc;
	memset(&desc, 0, sizeof(desc));
	desc.base.type              = SPUD_STRUCT_TYPE_AUDIO_INSTANCE_DESC;
	desc.allocator              = &allocator;
	desc.native_api             = g_api;
	spudaudio_instance instance = NULL;
	const SPUDRESULT r          = spudaudio_create_instance(&desc, &instance);
	if (g_api == SPUDAUDIO_NATIVE_API_NONE) {
		/* A build with no native audio API: the stub refuses. */
		CHECK(r != SPUD_SUCCESS);
		CHECK(instance == NULL);
	} else {
		CHECK_RESULT(r, SPUD_SUCCESS);
	}
	if (instance) {
		CHECK(spudaudio_get_native_api(instance) == g_api);
		spudaudio_device device = test_enumerate(instance);
		if (device)
			test_stream(device);
		else
			printf("spudaudio_test: no output device; streams not tested\n");

		CHECK_RESULT(spudaudio_set_device_event_callback(instance, device_event_callback, NULL), SPUD_SUCCESS);
		/* Setting it again replaces it; NULL clears it. */
		CHECK_RESULT(spudaudio_set_device_event_callback(instance, device_event_callback, &tracker), SPUD_SUCCESS);
		CHECK_RESULT(spudaudio_set_device_event_callback(instance, NULL, NULL), SPUD_SUCCESS);
		CHECK_RESULT(spudaudio_set_device_event_callback(instance, device_event_callback, NULL), SPUD_SUCCESS);
		/* Destroying the instance clears it. */
		CHECK_RESULT(spudaudio_destroy_instance(instance), SPUD_SUCCESS);
	}
	CHECK(spudtest_allocator_clean(&tracker));

	if (failures) {
		fprintf(stderr, "spudaudio_test: %d failure(s)\n", failures);
		return 1;
	}
	printf("spudaudio_test: all passed\n");
	return 0;
}
