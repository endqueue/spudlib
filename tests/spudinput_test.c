/*
 * spudinput_test — SpudInput's front end, and whatever gamepads are there.
 *
 * It needs no gamepad: with none connected it checks everything that can be
 * checked without one (what each call returns for a bad argument and for a
 * gamepad that isn't there, and that a poll with nothing to find succeeds),
 * so it is a ctest. With one or more connected it also reads each, checks
 * that what it reports agrees with itself, and prints it, which is as far as
 * a test with nobody holding the gamepad can go. It moves no motor and sets
 * no light.
 *
 * Exits 0 when every check passes, 1 when one fails.
 */

#include <spudinput.h>

/* The front end's own helpers, for the one that has no public call of its
 * own to be reached through. */
#include "../src/input/spudinputshared.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#if SPUDINPUT_COMPILE_GAMECONTROLLER
#include <CoreFoundation/CoreFoundation.h>
#endif

static int failures = 0;

#define CHECK(cond)                                                              \
	do {                                                                         \
		if (!(cond)) {                                                           \
			fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
			++failures;                                                          \
		}                                                                        \
	} while (0)

/* The native API this build has, or NONE. */
static SPUDINPUT_NATIVE_API native_api(void) {
#if SPUDINPUT_COMPILE_GAMECONTROLLER
	return SPUDINPUT_NATIVE_API_GAMECONTROLLER;
#elif SPUDINPUT_COMPILE_GAMEINPUT
	return SPUDINPUT_NATIVE_API_GAMEINPUT;
#elif SPUDINPUT_COMPILE_XINPUT
	return SPUDINPUT_NATIVE_API_XINPUT;
#elif SPUDINPUT_COMPILE_EVDEV
	return SPUDINPUT_NATIVE_API_EVDEV;
#else
	return SPUDINPUT_NATIVE_API_NONE;
#endif
}

/* Lets the platform notice devices for a moment, where it needs the caller
 * to: GameController's arrive only while the main run loop runs. */
static void let_devices_arrive(void) {
#if SPUDINPUT_COMPILE_GAMECONTROLLER
	CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.5, false);
#endif
}

static void test_copy_utf8(void) {
	char out[8];

	spudinput_shared_copy_utf8(out, sizeof(out), "pad");
	CHECK(strcmp(out, "pad") == 0);
	spudinput_shared_copy_utf8(out, sizeof(out), NULL);
	CHECK(strcmp(out, "") == 0);
	spudinput_shared_copy_utf8(out, sizeof(out), "");
	CHECK(strcmp(out, "") == 0);

	/* Exactly fits, and one too many. */
	spudinput_shared_copy_utf8(out, sizeof(out), "1234567");
	CHECK(strcmp(out, "1234567") == 0);
	spudinput_shared_copy_utf8(out, sizeof(out), "12345678");
	CHECK(strcmp(out, "1234567") == 0);

	/* Room for the terminator alone. */
	spudinput_shared_copy_utf8(out, 1, "pad");
	CHECK(strcmp(out, "") == 0);

	/* A cut that would fall inside a character falls before it instead:
	 * U+00E9 is two bytes, U+20AC three, U+1F3AE four. */
	spudinput_shared_copy_utf8(out, sizeof(out), "123456\xC3\xA9");
	CHECK(strcmp(out, "123456") == 0);
	spudinput_shared_copy_utf8(out, sizeof(out), "12345\xE2\x82\xAC");
	CHECK(strcmp(out, "12345") == 0);
	spudinput_shared_copy_utf8(out, sizeof(out), "1234\xF0\x9F\x8E\xAE");
	CHECK(strcmp(out, "1234") == 0);
	/* And one that fits whole is kept whole. */
	spudinput_shared_copy_utf8(out, sizeof(out), "12345\xC3\xA9");
	CHECK(strcmp(out, "12345\xC3\xA9") == 0);
	spudinput_shared_copy_utf8(out, sizeof(out), "123\xF0\x9F\x8E\xAE");
	CHECK(strcmp(out, "123\xF0\x9F\x8E\xAE") == 0);
}

static void test_create_arguments(void) {
	SPUDINPUT_INSTANCE_DESC desc;
	memset(&desc, 0, sizeof(desc));
	spudinput_instance instance = (spudinput_instance)(uintptr_t)1;

	CHECK(spudinput_instance_create(NULL, &instance) == SPUDRESULT_NULL_DESC);
	CHECK(spudinput_instance_create(&desc, NULL) == SPUDRESULT_NULL_OUTPUT_PARAMETER);

	/* A zeroed desc has not chosen an API. */
	CHECK(spudinput_instance_create(&desc, &instance) == SPUDRESULT_INVALID_API);
	CHECK(instance == NULL);

	/* One no build has, and every one this build doesn't. */
	desc.native_api = 99;
	CHECK(spudinput_instance_create(&desc, &instance) == SPUDRESULT_INVALID_API);
	for (SPUDINPUT_NATIVE_API api = SPUDINPUT_NATIVE_API_GAMECONTROLLER; api <= SPUDINPUT_NATIVE_API_EVDEV; ++api) {
		if (api == native_api())
			continue;
		desc.native_api = api;
		instance        = (spudinput_instance)(uintptr_t)1;
		CHECK(spudinput_instance_create(&desc, &instance) == SPUDRESULT_INVALID_API);
		CHECK(instance == NULL);
	}

	spudinput_instance_destroy(NULL);
}

static void test_null_instance(void) {
	const spudinput_gamepad_id *ids = NULL;
	uint32_t count                  = 0;
	SPUDINPUT_GAMEPAD_PROPERTIES properties;
	SPUDINPUT_GAMEPAD_STATE state;
	SPUDINPUT_GAMEPAD_BATTERY battery;
	SPUDINPUT_GAMEPAD_TOUCH_STATE touch;
	const SPUDINPUT_GAMEPAD_MOTION_SAMPLE *samples = NULL;
	uint32_t dropped                               = 0;
	SPUDINPUT_GAMEPAD_RUMBLE rumble;
	memset(&rumble, 0, sizeof(rumble));
	char label[16];

	const SPUDRESULT invalid = SPUDRESULT_SPUDINPUT_INVALID_INSTANCE;
	CHECK(spudinput_instance_get_native_api(NULL) == SPUDINPUT_NATIVE_API_NONE);
	CHECK(spudinput_instance_poll(NULL) == invalid);
	CHECK(spudinput_instance_get_gamepads(NULL, &ids, &count) == invalid);
	CHECK(spudinput_gamepad_get_properties(NULL, 1, &properties) == invalid);
	CHECK(spudinput_gamepad_get_state(NULL, 1, &state) == invalid);
	CHECK(spudinput_gamepad_set_rumble(NULL, 1, &rumble) == invalid);
	CHECK(spudinput_gamepad_get_battery(NULL, 1, &battery) == invalid);
	CHECK(spudinput_gamepad_set_motion_enabled(NULL, 1, true) == invalid);
	CHECK(spudinput_gamepad_get_motion(NULL, 1, &samples, &count, &dropped) == invalid);
	CHECK(spudinput_gamepad_get_touch(NULL, 1, 0, &touch) == invalid);
	CHECK(spudinput_gamepad_set_light_color(NULL, 1, 0.0f, 0.0f, 0.0f) == invalid);
	CHECK(spudinput_gamepad_set_player_indicator(NULL, 1, 0) == invalid);
	CHECK(spudinput_gamepad_get_button_label(NULL, 1, SPUDINPUT_GAMEPAD_BUTTON_SOUTH, label, sizeof(label)) == invalid);
	CHECK(spudinput_gamepad_get_axis_label(NULL, 1, SPUDINPUT_GAMEPAD_AXIS_LEFT_STICK_X, label, sizeof(label)) == invalid);
}

/* Every call that takes a gamepad, given one that is not there. */
static void test_absent_gamepad(
    spudinput_instance instance,
    spudinput_gamepad_id gamepad) {
	SPUDINPUT_GAMEPAD_PROPERTIES properties;
	SPUDINPUT_GAMEPAD_STATE state;
	SPUDINPUT_GAMEPAD_BATTERY battery;
	SPUDINPUT_GAMEPAD_TOUCH_STATE touch;
	const SPUDINPUT_GAMEPAD_MOTION_SAMPLE *samples = NULL;
	uint32_t count                                 = 0;
	uint32_t dropped                               = 0;
	SPUDINPUT_GAMEPAD_RUMBLE rumble;
	memset(&rumble, 0, sizeof(rumble));
	char label[16];

	const SPUDRESULT gone = SPUDRESULT_SPUDINPUT_GAMEPAD_NOT_CONNECTED;
	CHECK(spudinput_gamepad_get_properties(instance, gamepad, &properties) == gone);
	CHECK(spudinput_gamepad_get_state(instance, gamepad, &state) == gone);
	CHECK(spudinput_gamepad_set_rumble(instance, gamepad, &rumble) == gone);
	CHECK(spudinput_gamepad_get_battery(instance, gamepad, &battery) == gone);
	CHECK(spudinput_gamepad_set_motion_enabled(instance, gamepad, true) == gone);
	CHECK(spudinput_gamepad_get_motion(instance, gamepad, &samples, &count, &dropped) == gone);
	CHECK(spudinput_gamepad_get_touch(instance, gamepad, 0, &touch) == gone);
	CHECK(spudinput_gamepad_set_light_color(instance, gamepad, 0.0f, 0.0f, 0.0f) == gone);
	CHECK(spudinput_gamepad_set_player_indicator(instance, gamepad, 0) == gone);
	CHECK(spudinput_gamepad_get_button_label(instance, gamepad, SPUDINPUT_GAMEPAD_BUTTON_SOUTH, label, sizeof(label)) == gone);
	CHECK(spudinput_gamepad_get_axis_label(instance, gamepad, SPUDINPUT_GAMEPAD_AXIS_LEFT_STICK_X, label, sizeof(label)) == gone);

	/* That the gamepad is not there comes before anything else that is
	 * wrong with the call. */
	CHECK(spudinput_gamepad_get_properties(instance, gamepad, NULL) == gone);
	CHECK(spudinput_gamepad_set_rumble(instance, gamepad, NULL) == gone);
	CHECK(spudinput_gamepad_get_button_label(instance, gamepad, 0, NULL, 0) == gone);
}

/* One connected gamepad: what it says of itself has to agree with itself.
 * Nothing here needs anyone to touch it. */
static void test_connected_gamepad(
    spudinput_instance instance,
    spudinput_gamepad_id gamepad) {
	SPUDINPUT_GAMEPAD_PROPERTIES properties;
	memset(&properties, 0xff, sizeof(properties));
	CHECK(spudinput_gamepad_get_properties(instance, gamepad, &properties) == SPUD_SUCCESS);
	CHECK(spudinput_gamepad_get_properties(instance, gamepad, NULL) == SPUDRESULT_NULL_OUTPUT_PARAMETER);

	/* The name is terminated inside its array. */
	CHECK(memchr(properties.name, '\0', SPUDINPUT_GAMEPAD_NAME_SIZE) != NULL);
	CHECK(properties.touch_surface_count <= SPUDINPUT_GAMEPAD_TOUCH_SURFACE_MAX);
	for (uint32_t i = 0; i < properties.touch_surface_count; ++i)
		CHECK(properties.touch_surfaces[i].contact_max <= SPUDINPUT_GAMEPAD_TOUCH_CONTACT_MAX);
	if (!(properties.features & SPUDINPUT_GAMEPAD_FEATURE_PLAYER_INDICATOR))
		CHECK(properties.player_indicator_max == 0);

	printf("  gamepad %" PRIu64 ": \"%s\" vendor %04x product %04x\n", gamepad, properties.name, properties.vendor_id, properties.product_id);
	printf(
	    "    buttons %05x axes %02x analog triggers %d features %03x touch surfaces %u player indicator max %u\n", properties.buttons, properties.axes,
	    (int)properties.analog_triggers, properties.features, properties.touch_surface_count, properties.player_indicator_max);

	SPUDINPUT_GAMEPAD_STATE state;
	SPUDINPUT_GAMEPAD_STATE again;
	CHECK(spudinput_gamepad_get_state(instance, gamepad, &state) == SPUD_SUCCESS);
	CHECK(spudinput_gamepad_get_state(instance, gamepad, NULL) == SPUDRESULT_NULL_OUTPUT_PARAMETER);
	/* Reading changes nothing. */
	CHECK(spudinput_gamepad_get_state(instance, gamepad, &again) == SPUD_SUCCESS);
	CHECK(memcmp(&state, &again, sizeof(state)) == 0);

	/* Nothing it doesn't have is down, and each axis is in its range. */
	CHECK((state.buttons & ~properties.buttons) == 0);
	CHECK((state.buttons_went_down & ~properties.buttons) == 0);
	CHECK((state.buttons_went_up & ~properties.buttons) == 0);
	for (uint32_t axis = 0; axis < SPUDINPUT_GAMEPAD_AXIS_COUNT; ++axis) {
		const float low = axis < SPUDINPUT_GAMEPAD_AXIS_LEFT_TRIGGER ? -1.0f : 0.0f;
		CHECK(state.axes[axis] >= low);
		CHECK(state.axes[axis] <= 1.0f);
		if (!(properties.axes & (1u << axis)))
			CHECK(state.axes[axis] == 0.0f);
	}
	printf(
	    "    state: buttons %05x axes %+.3f %+.3f %+.3f %+.3f %.3f %.3f\n", state.buttons, state.axes[0], state.axes[1], state.axes[2], state.axes[3],
	    state.axes[4], state.axes[5]);

	/* Rumble: the checks that come before any motor is touched. All four
	 * at 0 is asked of every gamepad, and moves nothing. */
	SPUDINPUT_GAMEPAD_RUMBLE rumble;
	memset(&rumble, 0, sizeof(rumble));
	CHECK(spudinput_gamepad_set_rumble(instance, gamepad, NULL) == SPUDRESULT_NULL_DESC);
	CHECK(spudinput_gamepad_set_rumble(instance, gamepad, &rumble) == SPUD_SUCCESS);
	rumble.low_frequency = -0.1f;
	CHECK(spudinput_gamepad_set_rumble(instance, gamepad, &rumble) == SPUDRESULT_DESC_INVALID_PARAMETERS);
	rumble.low_frequency = 1.5f;
	CHECK(spudinput_gamepad_set_rumble(instance, gamepad, &rumble) == SPUDRESULT_DESC_INVALID_PARAMETERS);
	rumble.low_frequency = NAN;
	CHECK(spudinput_gamepad_set_rumble(instance, gamepad, &rumble) == SPUDRESULT_DESC_INVALID_PARAMETERS);
	rumble.low_frequency = 0.0f;
	rumble.right_trigger = NAN;
	CHECK(spudinput_gamepad_set_rumble(instance, gamepad, &rumble) == SPUDRESULT_DESC_INVALID_PARAMETERS);
	rumble.right_trigger = 0.0f;
	if (!(properties.features & SPUDINPUT_GAMEPAD_FEATURE_RUMBLE_LEFT_TRIGGER)) {
		rumble.left_trigger = 0.5f;
		CHECK(spudinput_gamepad_set_rumble(instance, gamepad, &rumble) == SPUDRESULT_SPUDINPUT_FEATURE_NOT_PRESENT);
		rumble.left_trigger = 0.0f;
	}

	SPUDINPUT_GAMEPAD_BATTERY battery;
	if (properties.features & SPUDINPUT_GAMEPAD_FEATURE_BATTERY) {
		CHECK(spudinput_gamepad_get_battery(instance, gamepad, &battery) == SPUD_SUCCESS);
		CHECK(spudinput_gamepad_get_battery(instance, gamepad, NULL) == SPUDRESULT_NULL_OUTPUT_PARAMETER);
		CHECK(battery.state <= SPUDINPUT_BATTERY_STATE_FULL);
		CHECK(battery.level >= 0.0f);
		CHECK(battery.level <= 1.0f);
		if (!battery.level_known)
			CHECK(battery.level == 0.0f);
		printf("    battery: state %u level %.2f known %d steps %u\n", battery.state, battery.level, (int)battery.level_known, battery.level_steps);
	} else {
		CHECK(spudinput_gamepad_get_battery(instance, gamepad, &battery) == SPUDRESULT_SPUDINPUT_FEATURE_NOT_PRESENT);
	}

	/* Motion: the instance was made keeping no samples, so the sensors
	 * can't be switched on, and nothing is handed out. */
	const SPUDINPUT_GAMEPAD_MOTION_SAMPLE *samples = NULL;
	uint32_t sample_count                          = 7;
	uint32_t dropped_count                         = 7;
	if (properties.features & (SPUDINPUT_GAMEPAD_FEATURE_ACCELEROMETER | SPUDINPUT_GAMEPAD_FEATURE_GYROSCOPE)) {
		CHECK(spudinput_gamepad_set_motion_enabled(instance, gamepad, true) == SPUDRESULT_ZERO_SIZE);
		CHECK(spudinput_gamepad_set_motion_enabled(instance, gamepad, false) == SPUD_SUCCESS);
		CHECK(spudinput_gamepad_get_motion(instance, gamepad, &samples, &sample_count, &dropped_count) == SPUD_SUCCESS);
		CHECK(samples == NULL);
		CHECK(sample_count == 0);
		CHECK(dropped_count == 0);
		CHECK(spudinput_gamepad_get_motion(instance, gamepad, NULL, &sample_count, &dropped_count) == SPUDRESULT_NULL_OUTPUT_PARAMETER);
		CHECK(spudinput_gamepad_get_motion(instance, gamepad, &samples, NULL, &dropped_count) == SPUDRESULT_NULL_OUTPUT_PARAMETER);
		CHECK(spudinput_gamepad_get_motion(instance, gamepad, &samples, &sample_count, NULL) == SPUDRESULT_NULL_OUTPUT_PARAMETER);
	} else {
		const SPUDRESULT absent = SPUDRESULT_SPUDINPUT_FEATURE_NOT_PRESENT;
		CHECK(spudinput_gamepad_set_motion_enabled(instance, gamepad, true) == absent);
		CHECK(spudinput_gamepad_get_motion(instance, gamepad, &samples, &sample_count, &dropped_count) == absent);
	}

	SPUDINPUT_GAMEPAD_TOUCH_STATE touch;
	for (uint32_t surface = 0; surface < properties.touch_surface_count; ++surface) {
		CHECK(spudinput_gamepad_get_touch(instance, gamepad, surface, &touch) == SPUD_SUCCESS);
		CHECK(spudinput_gamepad_get_touch(instance, gamepad, surface, NULL) == SPUDRESULT_NULL_OUTPUT_PARAMETER);
		CHECK(touch.contact_count <= properties.touch_surfaces[surface].contact_max);
		for (uint32_t i = 0; i < touch.contact_count && i < SPUDINPUT_GAMEPAD_TOUCH_CONTACT_MAX; ++i) {
			CHECK(touch.contacts[i].x >= 0.0f);
			CHECK(touch.contacts[i].x <= 1.0f);
			CHECK(touch.contacts[i].y >= 0.0f);
			CHECK(touch.contacts[i].y <= 1.0f);
		}
	}
	CHECK(spudinput_gamepad_get_touch(instance, gamepad, properties.touch_surface_count, &touch) == SPUDRESULT_INDEX_OUT_OF_RANGE);

	/* Lights: only what is refused. Nothing is lit or put out. */
	if (properties.features & SPUDINPUT_GAMEPAD_FEATURE_LIGHT_COLOR) {
		CHECK(spudinput_gamepad_set_light_color(instance, gamepad, 1.5f, 0.0f, 0.0f) == SPUDRESULT_DESC_INVALID_PARAMETERS);
		CHECK(spudinput_gamepad_set_light_color(instance, gamepad, 0.0f, -0.5f, 0.0f) == SPUDRESULT_DESC_INVALID_PARAMETERS);
		CHECK(spudinput_gamepad_set_light_color(instance, gamepad, 0.0f, 0.0f, NAN) == SPUDRESULT_DESC_INVALID_PARAMETERS);
	} else {
		CHECK(spudinput_gamepad_set_light_color(instance, gamepad, 0.0f, 0.0f, 0.0f) == SPUDRESULT_SPUDINPUT_FEATURE_NOT_PRESENT);
	}
	if (properties.features & SPUDINPUT_GAMEPAD_FEATURE_PLAYER_INDICATOR) {
		CHECK(spudinput_gamepad_set_player_indicator(instance, gamepad, properties.player_indicator_max + 1) == SPUDRESULT_INDEX_OUT_OF_RANGE);
	} else {
		CHECK(spudinput_gamepad_set_player_indicator(instance, gamepad, 0) == SPUDRESULT_SPUDINPUT_FEATURE_NOT_PRESENT);
	}

	char label[64];
	if (properties.features & SPUDINPUT_GAMEPAD_FEATURE_LABELS) {
		const SPUDINPUT_GAMEPAD_BUTTONS south = SPUDINPUT_GAMEPAD_BUTTON_SOUTH;
		CHECK(spudinput_gamepad_get_button_label(instance, gamepad, 0, label, sizeof(label)) == SPUDRESULT_DESC_INVALID_PARAMETERS);
		CHECK(spudinput_gamepad_get_button_label(instance, gamepad, south | SPUDINPUT_GAMEPAD_BUTTON_EAST, label, sizeof(label)) ==
		      SPUDRESULT_DESC_INVALID_PARAMETERS);
		CHECK(spudinput_gamepad_get_button_label(instance, gamepad, 1u << 30, label, sizeof(label)) == SPUDRESULT_DESC_INVALID_PARAMETERS);
		CHECK(spudinput_gamepad_get_button_label(instance, gamepad, south, NULL, sizeof(label)) == SPUDRESULT_NULL_OUTPUT_PARAMETER);
		CHECK(spudinput_gamepad_get_button_label(instance, gamepad, south, label, 0) == SPUDRESULT_ZERO_SIZE);
		CHECK(spudinput_gamepad_get_axis_label(instance, gamepad, SPUDINPUT_GAMEPAD_AXIS_COUNT, label, sizeof(label)) == SPUDRESULT_INDEX_OUT_OF_RANGE);
		CHECK(spudinput_gamepad_get_axis_label(instance, gamepad, 0, NULL, sizeof(label)) == SPUDRESULT_NULL_OUTPUT_PARAMETER);
		CHECK(spudinput_gamepad_get_axis_label(instance, gamepad, 0, label, 0) == SPUDRESULT_ZERO_SIZE);

		printf("    labels:");
		for (SPUDINPUT_GAMEPAD_BUTTONS bit = 1u; bit <= SPUDINPUT_GAMEPAD_BUTTON_TOUCH_SURFACE_1; bit <<= 1) {
			memset(label, 0x7f, sizeof(label));
			CHECK(spudinput_gamepad_get_button_label(instance, gamepad, bit, label, sizeof(label)) == SPUD_SUCCESS);
			CHECK(memchr(label, '\0', sizeof(label)) != NULL);
			if (label[0])
				printf(" [%s]", label);

			/* Into one byte: the terminator and nothing else. */
			char tiny[1] = {0x7f};
			CHECK(spudinput_gamepad_get_button_label(instance, gamepad, bit, tiny, 1) == SPUD_SUCCESS);
			CHECK(tiny[0] == '\0');
		}
		for (SPUDINPUT_GAMEPAD_AXIS axis = 0; axis < SPUDINPUT_GAMEPAD_AXIS_COUNT; ++axis) {
			CHECK(spudinput_gamepad_get_axis_label(instance, gamepad, axis, label, sizeof(label)) == SPUD_SUCCESS);
			CHECK(memchr(label, '\0', sizeof(label)) != NULL);
			if (label[0])
				printf(" <%s>", label);
		}
		printf("\n");
	} else {
		CHECK(spudinput_gamepad_get_button_label(instance, gamepad, SPUDINPUT_GAMEPAD_BUTTON_SOUTH, label, sizeof(label)) ==
		      SPUDRESULT_SPUDINPUT_FEATURE_NOT_PRESENT);
	}
}

static void test_instance(void) {
	SPUDINPUT_INSTANCE_DESC desc;
	memset(&desc, 0, sizeof(desc));
	desc.native_api = native_api();
	/* Only while this process is the one in use, and keeping no motion
	 * samples: the test switches no sensor on. */
	desc.read_in_background     = false;
	desc.motion_sample_capacity = 0;

#if SPUDINPUT_COMPILE_XINPUT || SPUDINPUT_COMPILE_EVDEV
	/* These always read; saying otherwise is refused. */
	spudinput_instance refused = (spudinput_instance)(uintptr_t)1;
	CHECK(spudinput_instance_create(&desc, &refused) == SPUDRESULT_SPUDINPUT_UNSUPPORTED);
	CHECK(refused == NULL);
	desc.read_in_background = true;
#endif

	spudinput_instance instance = NULL;
	CHECK(spudinput_instance_create(&desc, &instance) == SPUD_SUCCESS);
	if (!instance)
		return;
	CHECK(spudinput_instance_get_native_api(instance) == native_api());

	const spudinput_gamepad_id *ids = (const spudinput_gamepad_id *)(uintptr_t)1;
	uint32_t count                  = 7;

	/* Before the first poll there is nothing, whatever is plugged in. */
	CHECK(spudinput_instance_get_gamepads(instance, &ids, &count) == SPUD_SUCCESS);
	CHECK(ids == NULL);
	CHECK(count == 0);
	CHECK(spudinput_instance_get_gamepads(instance, NULL, &count) == SPUDRESULT_NULL_OUTPUT_PARAMETER);
	CHECK(spudinput_instance_get_gamepads(instance, &ids, NULL) == SPUDRESULT_NULL_OUTPUT_PARAMETER);
	test_absent_gamepad(instance, SPUDINPUT_GAMEPAD_ID_NONE);
	test_absent_gamepad(instance, 1);

	let_devices_arrive();
	CHECK(spudinput_instance_poll(instance) == SPUD_SUCCESS);
	CHECK(spudinput_instance_get_gamepads(instance, &ids, &count) == SPUD_SUCCESS);
	CHECK((ids == NULL) == (count == 0));
	printf("spudinput_test: %u gamepad(s) connected\n", count);

	/* Ids are given out from 1, each once, in the order first seen. */
	spudinput_gamepad_id highest = SPUDINPUT_GAMEPAD_ID_NONE;
	for (uint32_t i = 0; i < count; ++i) {
		CHECK(ids[i] > highest);
		highest = ids[i];
	}
	test_absent_gamepad(instance, SPUDINPUT_GAMEPAD_ID_NONE);
	test_absent_gamepad(instance, highest + 1);
	test_absent_gamepad(instance, UINT64_MAX);

	/* A copy: the array is the instance's and the next poll may move it. */
	spudinput_gamepad_id seen[16];
	const uint32_t seen_count = count < 16 ? count : 16;
	for (uint32_t i = 0; i < seen_count; ++i)
		seen[i] = ids[i];
	for (uint32_t i = 0; i < seen_count; ++i)
		test_connected_gamepad(instance, seen[i]);

	/* More polls, as a caller makes them: nothing has been unplugged in
	 * these few milliseconds, so the same gamepads keep the same ids. */
	for (int i = 0; i < 20; ++i)
		CHECK(spudinput_instance_poll(instance) == SPUD_SUCCESS);
	CHECK(spudinput_instance_get_gamepads(instance, &ids, &count) == SPUD_SUCCESS);
	CHECK(count >= seen_count);
	for (uint32_t i = 0; i < seen_count && i < count; ++i)
		CHECK(ids[i] == seen[i]);

	/* After the first poll nothing has gone down "since the poll before"
	 * unless someone is pressing: it can't be checked, but the state can
	 * still be read. */
	SPUDINPUT_GAMEPAD_STATE state;
	for (uint32_t i = 0; i < seen_count; ++i)
		CHECK(spudinput_gamepad_get_state(instance, seen[i], &state) == SPUD_SUCCESS);

	spudinput_instance_destroy(instance);

	/* A second instance, made and destroyed with nothing asked of it, and
	 * one that polls: an instance holds no state of another's. */
	CHECK(spudinput_instance_create(&desc, &instance) == SPUD_SUCCESS);
	spudinput_instance_destroy(instance);
	CHECK(spudinput_instance_create(&desc, &instance) == SPUD_SUCCESS);
	if (instance) {
		CHECK(spudinput_instance_poll(instance) == SPUD_SUCCESS);
		CHECK(spudinput_instance_get_gamepads(instance, &ids, &count) == SPUD_SUCCESS);
		/* Its ids start from 1 again. */
		if (count > 0)
			CHECK(ids[0] == 1);
		spudinput_instance_destroy(instance);
	}
}

int main(void) {
	test_copy_utf8();
	test_create_arguments();
	test_null_instance();
	if (native_api() == SPUDINPUT_NATIVE_API_NONE)
		printf("spudinput_test: no native input API in this build; the instance tests are skipped\n");
	else
		test_instance();

	if (failures)
		fprintf(stderr, "spudinput_test: %d failure(s)\n", failures);
	else
		printf("spudinput_test: all passed\n");
	return failures ? 1 : 0;
}
