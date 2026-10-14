#include "input/spudinputshared.h"

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

/* ---- With no native API built in ---- */

#if !(SPUDINPUT_COMPILE_GAMECONTROLLER || SPUDINPUT_COMPILE_GAMEINPUT || SPUDINPUT_COMPILE_XINPUT || SPUDINPUT_COMPILE_EVDEV)

/* There is no API to bind an instance to, so no instance is ever made and
 * none of the rest is ever reached. */

bool spudinput_backend_has_api(SPUDINPUT_NATIVE_API api) {
	(void)api;
	return false;
}

SPUDRESULT spudinput_backend_create(spudinput_instance instance) {
	(void)instance;
	return SPUDRESULT_NOT_IMPLEMENTED_YET;
}

void spudinput_backend_destroy(spudinput_instance instance) { (void)instance; }

SPUDRESULT spudinput_backend_poll(spudinput_instance instance) {
	(void)instance;
	return SPUDRESULT_NOT_IMPLEMENTED_YET;
}

void spudinput_backend_release_gamepad(
    spudinput_instance instance,
    spudinput_gamepad_record *record) {
	(void)instance;
	(void)record;
}

SPUDRESULT spudinput_backend_set_rumble(
    spudinput_instance instance,
    spudinput_gamepad_record *record,
    const SPUDINPUT_GAMEPAD_RUMBLE *rumble) {
	(void)instance;
	(void)record;
	(void)rumble;
	return SPUDRESULT_NOT_IMPLEMENTED_YET;
}

SPUDRESULT spudinput_backend_set_motion_enabled(
    spudinput_instance instance,
    spudinput_gamepad_record *record,
    bool enabled) {
	(void)instance;
	(void)record;
	(void)enabled;
	return SPUDRESULT_NOT_IMPLEMENTED_YET;
}

SPUDRESULT spudinput_backend_set_light_color(
    spudinput_instance instance,
    spudinput_gamepad_record *record,
    float red,
    float green,
    float blue) {
	(void)instance;
	(void)record;
	(void)red;
	(void)green;
	(void)blue;
	return SPUDRESULT_NOT_IMPLEMENTED_YET;
}

SPUDRESULT spudinput_backend_set_player_indicator(
    spudinput_instance instance,
    spudinput_gamepad_record *record,
    uint32_t player) {
	(void)instance;
	(void)record;
	(void)player;
	return SPUDRESULT_NOT_IMPLEMENTED_YET;
}

void spudinput_backend_get_button_label(
    spudinput_instance instance,
    spudinput_gamepad_record *record,
    SPUDINPUT_GAMEPAD_BUTTONS button,
    char *out_label,
    uint32_t label_size) {
	(void)instance;
	(void)record;
	(void)button;
	(void)out_label;
	(void)label_size;
}

void spudinput_backend_get_axis_label(
    spudinput_instance instance,
    spudinput_gamepad_record *record,
    SPUDINPUT_GAMEPAD_AXIS axis,
    char *out_label,
    uint32_t label_size) {
	(void)instance;
	(void)record;
	(void)axis;
	(void)out_label;
	(void)label_size;
}

#endif

/* ---- Shared with the backends ---- */

SPUDRESULT spudinput_shared_reserve(
    spudinput_instance instance,
    uint32_t extra) {
	const uint64_t needed = (uint64_t)instance->record_count + extra;
	if (needed <= instance->record_capacity)
		return SPUD_SUCCESS;
	if (needed > UINT32_MAX)
		return SPUDRESULT_OUT_OF_MEMORY;

	/* Both arrays or neither: the ids first, since having more room for
	 * ids than for records does no harm if the second allocation fails. */
	spudinput_gamepad_id *ids = (spudinput_gamepad_id *)realloc(instance->ids, (size_t)needed * sizeof(spudinput_gamepad_id));
	if (!ids)
		return SPUDRESULT_OUT_OF_MEMORY;
	instance->ids = ids;

	spudinput_gamepad_record *records = (spudinput_gamepad_record *)realloc(instance->records, (size_t)needed * sizeof(spudinput_gamepad_record));
	if (!records)
		return SPUDRESULT_OUT_OF_MEMORY;
	instance->records = records;

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
	if (!out_instance)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	*out_instance = NULL;
	if (desc->native_api == SPUDINPUT_NATIVE_API_NONE || !spudinput_backend_has_api(desc->native_api))
		return SPUDRESULT_INVALID_API;

	spudinput_instance instance = (spudinput_instance)calloc(1, sizeof(struct spudinput_instance_t));
	if (!instance)
		return SPUDRESULT_OUT_OF_MEMORY;
	instance->desc    = *desc;
	instance->next_id = 1;

	const SPUDRESULT result = spudinput_backend_create(instance);
	if (result != SPUD_SUCCESS) {
		free(instance);
		return result;
	}

	*out_instance = instance;
	return SPUD_SUCCESS;
}

void spudinput_instance_destroy(spudinput_instance instance) {
	if (!instance)
		return;

	for (uint32_t i = 0; i < instance->record_count; ++i) {
		spudinput_backend_release_gamepad(instance, &instance->records[i]);
		free(instance->records[i].motion_samples);
	}
	spudinput_backend_destroy(instance);

	free(instance->records);
	free(instance->ids);
	free(instance);
}

SPUDINPUT_NATIVE_API spudinput_instance_get_native_api(spudinput_instance instance) {
	if (!instance)
		return SPUDINPUT_NATIVE_API_NONE;

	return instance->desc.native_api;
}

SPUDRESULT spudinput_instance_poll(spudinput_instance instance) {
	if (!instance)
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
			free(record->motion_samples);
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

	const spudinput_gamepad_record *record = spudinput_find_gamepad(instance, gamepad);
	if (!record)
		return SPUDRESULT_SPUDINPUT_GAMEPAD_NOT_CONNECTED;
	if (!out_properties)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;

	*out_properties = record->properties;
	return SPUD_SUCCESS;
}

SPUDRESULT spudinput_gamepad_get_state(
    spudinput_instance instance,
    spudinput_gamepad_id gamepad,
    SPUDINPUT_GAMEPAD_STATE *out_state) {
	if (!instance)
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
		    (SPUDINPUT_GAMEPAD_MOTION_SAMPLE *)calloc(instance->desc.motion_sample_capacity, sizeof(SPUDINPUT_GAMEPAD_MOTION_SAMPLE));
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
