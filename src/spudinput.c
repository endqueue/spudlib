#include "input/spudinputshared.h"
#include "spudallocator.h"
#include "spudstructbase.h"

#include <stdlib.h>
#include <string.h>

/*
 * SpudInput's front end: the public API, the same on every platform.
 *
 * Every argument check is here, in one order, and so is the table of
 * gamepads: which are connected, their ids, and what each call reports
 * about them. A backend (src/input/backends/) only finds devices and reads
 * them into the records. See src/input/spudinputshared.h.
 */

/* Every button bit spudinput.h names. */
#define SPUDINPUT_ALL_BUTTONS ((SPUDINPUT_GAMEPAD_BUTTONS)((SPUDINPUT_GAMEPAD_BUTTON_TOUCH_SURFACE_1 << 1) - 1))

/* ---- The backends ---- */

/* Each native API built in is compiled here, in this translation unit, so
 * that its functions are inlined into the switches below. See
 * src/input/spudinputshared.h. */
#define SPUDINPUT_FRONT_END 1
#if SPUDINPUT_COMPILE_GAMECONTROLLER
#include "input/backends/apple/spudinputgamecontroller.m"
#endif
#if SPUDINPUT_COMPILE_GAMEINPUT
#include "input/backends/windows/spudinputgameinput.c"
#endif
#if SPUDINPUT_COMPILE_EVDEV
#include "input/backends/linux/spudinputevdev.c"
#endif

/* ---- Dispatch ---- */

/* Whether this build can bind an instance to `api`. */
static bool spudinput_has_api(SPUDINPUT_NATIVE_API api) {
	switch (api) {
#if SPUDINPUT_COMPILE_GAMECONTROLLER
	case SPUDINPUT_NATIVE_API_GAMECONTROLLER:
		return true;
#endif
#if SPUDINPUT_COMPILE_GAMEINPUT
	case SPUDINPUT_NATIVE_API_GAMEINPUT:
		return true;
#endif
#if SPUDINPUT_COMPILE_EVDEV
	case SPUDINPUT_NATIVE_API_EVDEV:
		return true;
#endif
	default:
		return false;
	}
}

/* One per verb of spudinputshared.h, each a switch on the instance's API.
 * Only an instance of an API built in exists, so `default` is never
 * reached; it returns what an unknown API would get. */

static SPUDRESULT spudinput_backend_create(spudinput_instance instance) {
	switch (instance->desc.native_api) {
#if SPUDINPUT_COMPILE_GAMECONTROLLER
	case SPUDINPUT_NATIVE_API_GAMECONTROLLER:
		return spudinput_gamecontroller_create(instance);
#endif
#if SPUDINPUT_COMPILE_GAMEINPUT
	case SPUDINPUT_NATIVE_API_GAMEINPUT:
		return spudinput_gameinput_create(instance);
#endif
#if SPUDINPUT_COMPILE_EVDEV
	case SPUDINPUT_NATIVE_API_EVDEV:
		return spudinput_evdev_create(instance);
#endif
	default:
		return SPUDRESULT_INVALID_API;
	}
}

static void spudinput_backend_destroy(spudinput_instance instance) {
	switch (instance->desc.native_api) {
#if SPUDINPUT_COMPILE_GAMECONTROLLER
	case SPUDINPUT_NATIVE_API_GAMECONTROLLER:
		spudinput_gamecontroller_destroy(instance);
		return;
#endif
#if SPUDINPUT_COMPILE_GAMEINPUT
	case SPUDINPUT_NATIVE_API_GAMEINPUT:
		spudinput_gameinput_destroy(instance);
		return;
#endif
#if SPUDINPUT_COMPILE_EVDEV
	case SPUDINPUT_NATIVE_API_EVDEV:
		spudinput_evdev_destroy(instance);
		return;
#endif
	default:
		return;
	}
}

static SPUDRESULT spudinput_backend_poll(spudinput_instance instance) {
	switch (instance->desc.native_api) {
#if SPUDINPUT_COMPILE_GAMECONTROLLER
	case SPUDINPUT_NATIVE_API_GAMECONTROLLER:
		return spudinput_gamecontroller_poll(instance);
#endif
#if SPUDINPUT_COMPILE_GAMEINPUT
	case SPUDINPUT_NATIVE_API_GAMEINPUT:
		return spudinput_gameinput_poll(instance);
#endif
#if SPUDINPUT_COMPILE_EVDEV
	case SPUDINPUT_NATIVE_API_EVDEV:
		return spudinput_evdev_poll(instance);
#endif
	default:
		return SPUDRESULT_INVALID_API;
	}
}

static void spudinput_backend_release_gamepad(
    spudinput_instance instance,
    spudinput_gamepad_record *record) {
	switch (instance->desc.native_api) {
#if SPUDINPUT_COMPILE_GAMECONTROLLER
	case SPUDINPUT_NATIVE_API_GAMECONTROLLER:
		spudinput_gamecontroller_release_gamepad(instance, record);
		return;
#endif
#if SPUDINPUT_COMPILE_GAMEINPUT
	case SPUDINPUT_NATIVE_API_GAMEINPUT:
		spudinput_gameinput_release_gamepad(instance, record);
		return;
#endif
#if SPUDINPUT_COMPILE_EVDEV
	case SPUDINPUT_NATIVE_API_EVDEV:
		spudinput_evdev_release_gamepad(instance, record);
		return;
#endif
	default:
		return;
	}
}

static SPUDRESULT spudinput_backend_set_rumble(
    spudinput_instance instance,
    spudinput_gamepad_record *record,
    const SPUDINPUT_GAMEPAD_RUMBLE *rumble) {
	switch (instance->desc.native_api) {
#if SPUDINPUT_COMPILE_GAMECONTROLLER
	case SPUDINPUT_NATIVE_API_GAMECONTROLLER:
		return spudinput_gamecontroller_set_rumble(instance, record, rumble);
#endif
#if SPUDINPUT_COMPILE_GAMEINPUT
	case SPUDINPUT_NATIVE_API_GAMEINPUT:
		return spudinput_gameinput_set_rumble(instance, record, rumble);
#endif
#if SPUDINPUT_COMPILE_EVDEV
	case SPUDINPUT_NATIVE_API_EVDEV:
		return spudinput_evdev_set_rumble(instance, record, rumble);
#endif
	default:
		return SPUDRESULT_INVALID_API;
	}
}

static SPUDRESULT spudinput_backend_set_motion_enabled(
    spudinput_instance instance,
    spudinput_gamepad_record *record,
    bool enabled) {
	switch (instance->desc.native_api) {
#if SPUDINPUT_COMPILE_GAMECONTROLLER
	case SPUDINPUT_NATIVE_API_GAMECONTROLLER:
		return spudinput_gamecontroller_set_motion_enabled(instance, record, enabled);
#endif
#if SPUDINPUT_COMPILE_GAMEINPUT
	case SPUDINPUT_NATIVE_API_GAMEINPUT:
		return spudinput_gameinput_set_motion_enabled(instance, record, enabled);
#endif
#if SPUDINPUT_COMPILE_EVDEV
	case SPUDINPUT_NATIVE_API_EVDEV:
		return spudinput_evdev_set_motion_enabled(instance, record, enabled);
#endif
	default:
		return SPUDRESULT_INVALID_API;
	}
}

static SPUDRESULT spudinput_backend_set_light_color(
    spudinput_instance instance,
    spudinput_gamepad_record *record,
    float red,
    float green,
    float blue) {
	switch (instance->desc.native_api) {
#if SPUDINPUT_COMPILE_GAMECONTROLLER
	case SPUDINPUT_NATIVE_API_GAMECONTROLLER:
		return spudinput_gamecontroller_set_light_color(instance, record, red, green, blue);
#endif
#if SPUDINPUT_COMPILE_GAMEINPUT
	case SPUDINPUT_NATIVE_API_GAMEINPUT:
		return spudinput_gameinput_set_light_color(instance, record, red, green, blue);
#endif
#if SPUDINPUT_COMPILE_EVDEV
	case SPUDINPUT_NATIVE_API_EVDEV:
		return spudinput_evdev_set_light_color(instance, record, red, green, blue);
#endif
	default:
		return SPUDRESULT_INVALID_API;
	}
}

static SPUDRESULT spudinput_backend_set_player_indicator(
    spudinput_instance instance,
    spudinput_gamepad_record *record,
    uint32_t player) {
	switch (instance->desc.native_api) {
#if SPUDINPUT_COMPILE_GAMECONTROLLER
	case SPUDINPUT_NATIVE_API_GAMECONTROLLER:
		return spudinput_gamecontroller_set_player_indicator(instance, record, player);
#endif
#if SPUDINPUT_COMPILE_GAMEINPUT
	case SPUDINPUT_NATIVE_API_GAMEINPUT:
		return spudinput_gameinput_set_player_indicator(instance, record, player);
#endif
#if SPUDINPUT_COMPILE_EVDEV
	case SPUDINPUT_NATIVE_API_EVDEV:
		return spudinput_evdev_set_player_indicator(instance, record, player);
#endif
	default:
		return SPUDRESULT_INVALID_API;
	}
}

static void spudinput_backend_get_button_label(
    spudinput_instance instance,
    spudinput_gamepad_record *record,
    SPUDINPUT_GAMEPAD_BUTTONS button,
    char *out_label,
    uint32_t label_size) {
	switch (instance->desc.native_api) {
#if SPUDINPUT_COMPILE_GAMECONTROLLER
	case SPUDINPUT_NATIVE_API_GAMECONTROLLER:
		spudinput_gamecontroller_get_button_label(instance, record, button, out_label, label_size);
		return;
#endif
#if SPUDINPUT_COMPILE_GAMEINPUT
	case SPUDINPUT_NATIVE_API_GAMEINPUT:
		spudinput_gameinput_get_button_label(instance, record, button, out_label, label_size);
		return;
#endif
#if SPUDINPUT_COMPILE_EVDEV
	case SPUDINPUT_NATIVE_API_EVDEV:
		spudinput_evdev_get_button_label(instance, record, button, out_label, label_size);
		return;
#endif
	default:
		return;
	}
}

static void spudinput_backend_get_axis_label(
    spudinput_instance instance,
    spudinput_gamepad_record *record,
    SPUDINPUT_GAMEPAD_AXIS axis,
    char *out_label,
    uint32_t label_size) {
	switch (instance->desc.native_api) {
#if SPUDINPUT_COMPILE_GAMECONTROLLER
	case SPUDINPUT_NATIVE_API_GAMECONTROLLER:
		spudinput_gamecontroller_get_axis_label(instance, record, axis, out_label, label_size);
		return;
#endif
#if SPUDINPUT_COMPILE_GAMEINPUT
	case SPUDINPUT_NATIVE_API_GAMEINPUT:
		spudinput_gameinput_get_axis_label(instance, record, axis, out_label, label_size);
		return;
#endif
#if SPUDINPUT_COMPILE_EVDEV
	case SPUDINPUT_NATIVE_API_EVDEV:
		spudinput_evdev_get_axis_label(instance, record, axis, out_label, label_size);
		return;
#endif
	default:
		return;
	}
}

/* ---- Shared with the backends ---- */

SPUDRESULT spudinput_shared_reserve(
    spudinput_instance instance,
    uint32_t extra) {
	const uint64_t needed = (uint64_t)instance->record_count + extra;
	if (needed <= instance->record_capacity)
		return SPUD_SUCCESS;
	if (needed > UINT32_MAX)
		return SPUDRESULT_OUT_OF_MEMORY;

	/* Both arrays or neither, so that both stay `record_capacity` long,
	 * which is the count each is freed with. */
	const SPUD_ALLOCATOR *allocator = &instance->head.allocator;
	spudinput_gamepad_id *ids       = SPUD_NEW_ARRAY(allocator, spudinput_gamepad_id, (size_t)needed);
	if (!ids)
		return SPUDRESULT_OUT_OF_MEMORY;
	spudinput_gamepad_record *records = SPUD_NEW_ARRAY(allocator, spudinput_gamepad_record, (size_t)needed);
	if (!records) {
		SPUD_DELETE_ARRAY(allocator, spudinput_gamepad_id, ids, (size_t)needed);
		return SPUDRESULT_OUT_OF_MEMORY;
	}

	if (instance->id_count)
		memcpy(ids, instance->ids, (size_t)instance->id_count * sizeof(spudinput_gamepad_id));
	if (instance->record_count)
		memcpy(records, instance->records, (size_t)instance->record_count * sizeof(spudinput_gamepad_record));
	SPUD_DELETE_ARRAY(allocator, spudinput_gamepad_id, instance->ids, instance->record_capacity);
	SPUD_DELETE_ARRAY(allocator, spudinput_gamepad_record, instance->records, instance->record_capacity);
	instance->ids             = ids;
	instance->records         = records;
	instance->record_capacity = (uint32_t)needed;
	return SPUD_SUCCESS;
}

spudinput_gamepad_record *spudinput_shared_add(
    spudinput_instance instance,
    void *native) {
	spudinput_gamepad_record *record = &instance->records[instance->record_count++];
	memset(record, 0, sizeof(*record));
	record->id     = instance->next_id++;
	record->native = native;
	record->seen   = true;
	return record;
}

void spudinput_shared_copy_utf8(
    char *destination,
    uint32_t size,
    const char *source) {
	destination[0] = '\0';
	if (!source)
		return;

	const size_t length = strlen(source);
	size_t count        = length;
	if (count > (size_t)size - 1) {
		count = (size_t)size - 1;
		/* Back to the start of the character the cut falls inside: the
		 * bytes after a character's first all begin with the bits 10. */
		while (count > 0 && ((unsigned char)source[count] & 0xC0) == 0x80)
			--count;
	}
	memcpy(destination, source, count);
	destination[count] = '\0';
}

/* The record of a gamepad that is connected as of the last poll, or NULL.
 * One a failed poll added is not found: its id is not in the list yet. */
static spudinput_gamepad_record *spudinput_find_gamepad(
    spudinput_instance instance,
    spudinput_gamepad_id gamepad) {
	if (gamepad == SPUDINPUT_GAMEPAD_ID_NONE)
		return NULL;

	for (uint32_t i = 0; i < instance->id_count; ++i) {
		if (instance->ids[i] != gamepad)
			continue;

		/* The list is in the records' order, but a failed poll may have
		 * added records since it was written. */
		for (uint32_t j = 0; j < instance->record_count; ++j) {
			if (instance->records[j].id == gamepad)
				return &instance->records[j];
		}
		return NULL;
	}
	return NULL;
}

/* Whether a level, or a colour's part, is a number from 0 to 1. */
static bool spudinput_is_unit(float value) { return value >= 0.0f && value <= 1.0f; }

/* ---- Instance ---- */

SPUDRESULT spudinput_instance_create(
    const SPUDINPUT_INSTANCE_DESC *desc,
    spudinput_instance *out_instance) {
	if (!desc)
		return SPUDRESULT_NULL_DESC;
	const SPUDRESULT base_result = spud_struct_check(&desc->base, SPUD_STRUCT_TYPE_INPUT_INSTANCE_DESC);
	if (base_result != SPUD_SUCCESS)
		return base_result;
	if (!out_instance)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	*out_instance = NULL;
	const SPUDRESULT allocator_result = spud_allocator_check(desc->allocator);
	if (allocator_result != SPUD_SUCCESS)
		return allocator_result;
	if (!spudinput_has_api(desc->native_api))
		return SPUDRESULT_INVALID_API;

	spudinput_instance instance = SPUD_NEW_ZEROED(desc->allocator, struct spudinput_instance_t);
	if (!instance)
		return SPUDRESULT_OUT_OF_MEMORY;
	instance->head.allocator = *desc->allocator;
	SPUD_BIRTH(instance);
	instance->desc      = *desc;
	/* The caller's chain and allocator struct needn't outlive this call;
	 * nothing is chained yet. */
	instance->desc.base.next = NULL;
	instance->desc.allocator = &instance->head.allocator;
	instance->next_id        = 1;

	const SPUDRESULT result = spudinput_backend_create(instance);
	if (result != SPUD_SUCCESS) {
		const SPUD_ALLOCATOR allocator = instance->head.allocator;
		SPUD_DELETE(&allocator, struct spudinput_instance_t, instance);
		return result;
	}

	*out_instance = instance;
	return SPUD_SUCCESS;
}

void spudinput_instance_destroy(spudinput_instance instance) {
	if (!instance)
		return;
	if (SPUD_STALE(instance, "spudinput_instance_destroy", "instance"))
		return;

	SPUD_KILL(instance);
	const SPUD_ALLOCATOR allocator = instance->head.allocator;
	for (uint32_t i = 0; i < instance->record_count; ++i) {
		spudinput_backend_release_gamepad(instance, &instance->records[i]);
		SPUD_DELETE_ARRAY(&allocator, SPUDINPUT_GAMEPAD_MOTION_SAMPLE, instance->records[i].motion_samples, instance->desc.motion_sample_capacity);
	}
	spudinput_backend_destroy(instance);

	SPUD_DELETE_ARRAY(&allocator, spudinput_gamepad_record, instance->records, instance->record_capacity);
	SPUD_DELETE_ARRAY(&allocator, spudinput_gamepad_id, instance->ids, instance->record_capacity);
	SPUD_DELETE(&allocator, struct spudinput_instance_t, instance);
}

SPUDINPUT_NATIVE_API spudinput_instance_get_native_api(spudinput_instance instance) {
	if (!instance)
		return SPUDINPUT_NATIVE_API_NONE;
	if (SPUD_STALE(instance, "spudinput_instance_get_native_api", "instance"))
		return SPUDINPUT_NATIVE_API_NONE;

	return instance->desc.native_api;
}

SPUDRESULT spudinput_instance_poll(spudinput_instance instance) {
	if (!instance)
		return SPUDRESULT_SPUDINPUT_INVALID_INSTANCE;
	if (SPUD_STALE(instance, "spudinput_instance_poll", "instance"))
		return SPUDRESULT_SPUDINPUT_INVALID_INSTANCE;

	for (uint32_t i = 0; i < instance->record_count; ++i)
		instance->records[i].seen = false;

	const SPUDRESULT result = spudinput_backend_poll(instance);
	if (result != SPUD_SUCCESS)
		return result;

	/* What was not seen has gone. The rest move down, keeping their
	 * order. */
	uint32_t kept = 0;
	for (uint32_t i = 0; i < instance->record_count; ++i) {
		spudinput_gamepad_record *record = &instance->records[i];
		if (!record->seen) {
			spudinput_backend_release_gamepad(instance, record);
			SPUD_DELETE_ARRAY(&instance->head.allocator, SPUDINPUT_GAMEPAD_MOTION_SAMPLE, record->motion_samples, instance->desc.motion_sample_capacity);
			continue;
		}

		if (kept != i)
			instance->records[kept] = *record;
		++kept;
	}
	instance->record_count = kept;

	/* There is room: the id array is as long as the record array. */
	for (uint32_t i = 0; i < kept; ++i)
		instance->ids[i] = instance->records[i].id;
	instance->id_count = kept;

	return SPUD_SUCCESS;
}

/* ---- Gamepads ---- */

SPUDRESULT spudinput_instance_get_gamepads(
    spudinput_instance instance,
    const spudinput_gamepad_id **out_gamepads,
    uint32_t *out_gamepad_count) {
	if (!instance)
		return SPUDRESULT_SPUDINPUT_INVALID_INSTANCE;
	if (SPUD_STALE(instance, "spudinput_instance_get_gamepads", "instance"))
		return SPUDRESULT_SPUDINPUT_INVALID_INSTANCE;
	if (!out_gamepads || !out_gamepad_count)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;

	*out_gamepads      = instance->id_count ? instance->ids : NULL;
	*out_gamepad_count = instance->id_count;
	return SPUD_SUCCESS;
}

SPUDRESULT spudinput_gamepad_get_properties(
    spudinput_instance instance,
    spudinput_gamepad_id gamepad,
    SPUDINPUT_GAMEPAD_PROPERTIES *out_properties) {
	if (!instance)
		return SPUDRESULT_SPUDINPUT_INVALID_INSTANCE;
	if (SPUD_STALE(instance, "spudinput_gamepad_get_properties", "instance"))
		return SPUDRESULT_SPUDINPUT_INVALID_INSTANCE;

	const spudinput_gamepad_record *record = spudinput_find_gamepad(instance, gamepad);
	if (!record)
		return SPUDRESULT_SPUDINPUT_GAMEPAD_NOT_CONNECTED;
	if (!out_properties)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	const SPUDRESULT base_result = spud_struct_out_check(&out_properties->base, SPUD_STRUCT_TYPE_INPUT_GAMEPAD_PROPERTIES);
	if (base_result != SPUD_SUCCESS)
		return base_result;

	*out_properties = record->properties;
	/* The record's copy has no base of its own; the check above leaves the
	 * caller's as this one. */
	out_properties->base = spud_struct_base_out(SPUD_STRUCT_TYPE_INPUT_GAMEPAD_PROPERTIES);
	return SPUD_SUCCESS;
}

SPUDRESULT spudinput_gamepad_get_state(
    spudinput_instance instance,
    spudinput_gamepad_id gamepad,
    SPUDINPUT_GAMEPAD_STATE *out_state) {
	if (!instance)
		return SPUDRESULT_SPUDINPUT_INVALID_INSTANCE;
	if (SPUD_STALE(instance, "spudinput_gamepad_get_state", "instance"))
		return SPUDRESULT_SPUDINPUT_INVALID_INSTANCE;

	const spudinput_gamepad_record *record = spudinput_find_gamepad(instance, gamepad);
	if (!record)
		return SPUDRESULT_SPUDINPUT_GAMEPAD_NOT_CONNECTED;
	if (!out_state)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;

	*out_state = record->state;
	return SPUD_SUCCESS;
}

/* ---- Rumble ---- */

SPUDRESULT spudinput_gamepad_set_rumble(
    spudinput_instance instance,
    spudinput_gamepad_id gamepad,
    const SPUDINPUT_GAMEPAD_RUMBLE *rumble) {
	if (!instance)
		return SPUDRESULT_SPUDINPUT_INVALID_INSTANCE;
	if (SPUD_STALE(instance, "spudinput_gamepad_set_rumble", "instance"))
		return SPUDRESULT_SPUDINPUT_INVALID_INSTANCE;

	spudinput_gamepad_record *record = spudinput_find_gamepad(instance, gamepad);
	if (!record)
		return SPUDRESULT_SPUDINPUT_GAMEPAD_NOT_CONNECTED;
	if (!rumble)
		return SPUDRESULT_NULL_DESC;
	/* Written so that a level that is not a number fails as well. */
	if (!spudinput_is_unit(rumble->low_frequency) || !spudinput_is_unit(rumble->high_frequency) || !spudinput_is_unit(rumble->left_trigger) ||
	    !spudinput_is_unit(rumble->right_trigger))
		return SPUDRESULT_DESC_INVALID_PARAMETERS;

	const SPUDINPUT_GAMEPAD_FEATURES features = record->properties.features;
	if ((rumble->low_frequency > 0.0f && !(features & SPUDINPUT_GAMEPAD_FEATURE_RUMBLE_LOW_FREQUENCY)) ||
	    (rumble->high_frequency > 0.0f && !(features & SPUDINPUT_GAMEPAD_FEATURE_RUMBLE_HIGH_FREQUENCY)) ||
	    (rumble->left_trigger > 0.0f && !(features & SPUDINPUT_GAMEPAD_FEATURE_RUMBLE_LEFT_TRIGGER)) ||
	    (rumble->right_trigger > 0.0f && !(features & SPUDINPUT_GAMEPAD_FEATURE_RUMBLE_RIGHT_TRIGGER)))
		return SPUDRESULT_SPUDINPUT_FEATURE_NOT_PRESENT;

	return spudinput_backend_set_rumble(instance, record, rumble);
}

/* ---- Battery ---- */

SPUDRESULT spudinput_gamepad_get_battery(
    spudinput_instance instance,
    spudinput_gamepad_id gamepad,
    SPUDINPUT_GAMEPAD_BATTERY *out_battery) {
	if (!instance)
		return SPUDRESULT_SPUDINPUT_INVALID_INSTANCE;
	if (SPUD_STALE(instance, "spudinput_gamepad_get_battery", "instance"))
		return SPUDRESULT_SPUDINPUT_INVALID_INSTANCE;

	const spudinput_gamepad_record *record = spudinput_find_gamepad(instance, gamepad);
	if (!record)
		return SPUDRESULT_SPUDINPUT_GAMEPAD_NOT_CONNECTED;
	if (!(record->properties.features & SPUDINPUT_GAMEPAD_FEATURE_BATTERY))
		return SPUDRESULT_SPUDINPUT_FEATURE_NOT_PRESENT;
	if (!out_battery)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;

	*out_battery = record->battery;
	return SPUD_SUCCESS;
}

/* ---- Motion sensors ---- */

#define SPUDINPUT_MOTION_FEATURES (SPUDINPUT_GAMEPAD_FEATURE_ACCELEROMETER | SPUDINPUT_GAMEPAD_FEATURE_GYROSCOPE)

SPUDRESULT spudinput_gamepad_set_motion_enabled(
    spudinput_instance instance,
    spudinput_gamepad_id gamepad,
    bool enabled) {
	if (!instance)
		return SPUDRESULT_SPUDINPUT_INVALID_INSTANCE;
	if (SPUD_STALE(instance, "spudinput_gamepad_set_motion_enabled", "instance"))
		return SPUDRESULT_SPUDINPUT_INVALID_INSTANCE;

	spudinput_gamepad_record *record = spudinput_find_gamepad(instance, gamepad);
	if (!record)
		return SPUDRESULT_SPUDINPUT_GAMEPAD_NOT_CONNECTED;
	if (!(record->properties.features & SPUDINPUT_MOTION_FEATURES))
		return SPUDRESULT_SPUDINPUT_FEATURE_NOT_PRESENT;
	if (enabled && instance->desc.motion_sample_capacity == 0)
		return SPUDRESULT_ZERO_SIZE;
	if (enabled == record->motion_enabled)
		return SPUD_SUCCESS;

	if (enabled && !record->motion_samples) {
		record->motion_samples =
		    SPUD_NEW_ARRAY_ZEROED(&instance->head.allocator, SPUDINPUT_GAMEPAD_MOTION_SAMPLE, instance->desc.motion_sample_capacity);
		if (!record->motion_samples)
			return SPUDRESULT_OUT_OF_MEMORY;
	}

	const SPUDRESULT result = spudinput_backend_set_motion_enabled(instance, record, enabled);
	if (result != SPUD_SUCCESS)
		return result;

	record->motion_enabled = enabled;
	if (!enabled) {
		/* Off: nothing is handed out from here on. */
		record->motion_sample_count  = 0;
		record->motion_dropped_count = 0;
	}
	return SPUD_SUCCESS;
}

SPUDRESULT spudinput_gamepad_get_motion(
    spudinput_instance instance,
    spudinput_gamepad_id gamepad,
    const SPUDINPUT_GAMEPAD_MOTION_SAMPLE **out_samples,
    uint32_t *out_sample_count,
    uint32_t *out_dropped_count) {
	if (!instance)
		return SPUDRESULT_SPUDINPUT_INVALID_INSTANCE;
	if (SPUD_STALE(instance, "spudinput_gamepad_get_motion", "instance"))
		return SPUDRESULT_SPUDINPUT_INVALID_INSTANCE;

	const spudinput_gamepad_record *record = spudinput_find_gamepad(instance, gamepad);
	if (!record)
		return SPUDRESULT_SPUDINPUT_GAMEPAD_NOT_CONNECTED;
	if (!(record->properties.features & SPUDINPUT_MOTION_FEATURES))
		return SPUDRESULT_SPUDINPUT_FEATURE_NOT_PRESENT;
	if (!out_samples || !out_sample_count || !out_dropped_count)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;

	*out_samples       = record->motion_sample_count ? record->motion_samples : NULL;
	*out_sample_count  = record->motion_sample_count;
	*out_dropped_count = record->motion_dropped_count;
	return SPUD_SUCCESS;
}

/* ---- Touch surfaces ---- */

SPUDRESULT spudinput_gamepad_get_touch(
    spudinput_instance instance,
    spudinput_gamepad_id gamepad,
    uint32_t surface,
    SPUDINPUT_GAMEPAD_TOUCH_STATE *out_touch) {
	if (!instance)
		return SPUDRESULT_SPUDINPUT_INVALID_INSTANCE;
	if (SPUD_STALE(instance, "spudinput_gamepad_get_touch", "instance"))
		return SPUDRESULT_SPUDINPUT_INVALID_INSTANCE;

	const spudinput_gamepad_record *record = spudinput_find_gamepad(instance, gamepad);
	if (!record)
		return SPUDRESULT_SPUDINPUT_GAMEPAD_NOT_CONNECTED;
	if (surface >= record->properties.touch_surface_count)
		return SPUDRESULT_INDEX_OUT_OF_RANGE;
	if (!out_touch)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;

	*out_touch = record->touch[surface];
	return SPUD_SUCCESS;
}

/* ---- Lights ---- */

SPUDRESULT spudinput_gamepad_set_light_color(
    spudinput_instance instance,
    spudinput_gamepad_id gamepad,
    float red,
    float green,
    float blue) {
	if (!instance)
		return SPUDRESULT_SPUDINPUT_INVALID_INSTANCE;
	if (SPUD_STALE(instance, "spudinput_gamepad_set_light_color", "instance"))
		return SPUDRESULT_SPUDINPUT_INVALID_INSTANCE;

	spudinput_gamepad_record *record = spudinput_find_gamepad(instance, gamepad);
	if (!record)
		return SPUDRESULT_SPUDINPUT_GAMEPAD_NOT_CONNECTED;
	if (!(record->properties.features & SPUDINPUT_GAMEPAD_FEATURE_LIGHT_COLOR))
		return SPUDRESULT_SPUDINPUT_FEATURE_NOT_PRESENT;
	if (!spudinput_is_unit(red) || !spudinput_is_unit(green) || !spudinput_is_unit(blue))
		return SPUDRESULT_DESC_INVALID_PARAMETERS;

	return spudinput_backend_set_light_color(instance, record, red, green, blue);
}

SPUDRESULT spudinput_gamepad_set_player_indicator(
    spudinput_instance instance,
    spudinput_gamepad_id gamepad,
    uint32_t player) {
	if (!instance)
		return SPUDRESULT_SPUDINPUT_INVALID_INSTANCE;
	if (SPUD_STALE(instance, "spudinput_gamepad_set_player_indicator", "instance"))
		return SPUDRESULT_SPUDINPUT_INVALID_INSTANCE;

	spudinput_gamepad_record *record = spudinput_find_gamepad(instance, gamepad);
	if (!record)
		return SPUDRESULT_SPUDINPUT_GAMEPAD_NOT_CONNECTED;
	if (!(record->properties.features & SPUDINPUT_GAMEPAD_FEATURE_PLAYER_INDICATOR))
		return SPUDRESULT_SPUDINPUT_FEATURE_NOT_PRESENT;
	if (player > record->properties.player_indicator_max)
		return SPUDRESULT_INDEX_OUT_OF_RANGE;

	return spudinput_backend_set_player_indicator(instance, record, player);
}

/* ---- Labels ---- */

SPUDRESULT spudinput_gamepad_get_button_label(
    spudinput_instance instance,
    spudinput_gamepad_id gamepad,
    SPUDINPUT_GAMEPAD_BUTTONS button,
    char *out_label,
    uint32_t label_size) {
	if (!instance)
		return SPUDRESULT_SPUDINPUT_INVALID_INSTANCE;
	if (SPUD_STALE(instance, "spudinput_gamepad_get_button_label", "instance"))
		return SPUDRESULT_SPUDINPUT_INVALID_INSTANCE;

	spudinput_gamepad_record *record = spudinput_find_gamepad(instance, gamepad);
	if (!record)
		return SPUDRESULT_SPUDINPUT_GAMEPAD_NOT_CONNECTED;
	if (!(record->properties.features & SPUDINPUT_GAMEPAD_FEATURE_LABELS))
		return SPUDRESULT_SPUDINPUT_FEATURE_NOT_PRESENT;
	/* No bit, a bit this header doesn't name, or more than one. */
	if (button == 0 || (button & ~SPUDINPUT_ALL_BUTTONS) || (button & (button - 1)))
		return SPUDRESULT_DESC_INVALID_PARAMETERS;
	if (!out_label)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	if (label_size == 0)
		return SPUDRESULT_ZERO_SIZE;

	out_label[0] = '\0';
	spudinput_backend_get_button_label(instance, record, button, out_label, label_size);
	return SPUD_SUCCESS;
}

SPUDRESULT spudinput_gamepad_get_axis_label(
    spudinput_instance instance,
    spudinput_gamepad_id gamepad,
    SPUDINPUT_GAMEPAD_AXIS axis,
    char *out_label,
    uint32_t label_size) {
	if (!instance)
		return SPUDRESULT_SPUDINPUT_INVALID_INSTANCE;
	if (SPUD_STALE(instance, "spudinput_gamepad_get_axis_label", "instance"))
		return SPUDRESULT_SPUDINPUT_INVALID_INSTANCE;

	spudinput_gamepad_record *record = spudinput_find_gamepad(instance, gamepad);
	if (!record)
		return SPUDRESULT_SPUDINPUT_GAMEPAD_NOT_CONNECTED;
	if (!(record->properties.features & SPUDINPUT_GAMEPAD_FEATURE_LABELS))
		return SPUDRESULT_SPUDINPUT_FEATURE_NOT_PRESENT;
	if (axis >= SPUDINPUT_GAMEPAD_AXIS_COUNT)
		return SPUDRESULT_INDEX_OUT_OF_RANGE;
	if (!out_label)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	if (label_size == 0)
		return SPUDRESULT_ZERO_SIZE;

	out_label[0] = '\0';
	spudinput_backend_get_axis_label(instance, record, axis, out_label, label_size);
	return SPUD_SUCCESS;
}
