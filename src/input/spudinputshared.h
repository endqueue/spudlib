#ifndef SPUDLIB_SPUDINPUTSHARED_H
#define SPUDLIB_SPUDINPUTSHARED_H

/*
 * SpudInput: what the front end (src/spudinput.c) and the backends share.
 *
 * The front end is the public API: every argument check, the table of
 * gamepads and their ids, and what each call reports. A backend finds
 * devices and reads them, through the spudinput_backend_* functions at the
 * end of this file, and writes what it reads into the front end's records.
 * So a rule about a call - what it checks, in what order, what it returns -
 * is written once, in the front end, and is the same on every platform.
 *
 * Nothing outside src/ includes this.
 */

#include "spudinput.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/*
 * One connected gamepad: everything the public calls report about it, as of
 * the last poll. A backend fills it during spudinput_backend_poll() and
 * nowhere else, so that nothing a caller reads changes between two polls.
 */
typedef struct spudinput_gamepad_record {
	spudinput_gamepad_id id;
	/* The backend's own object for the device. Not null. It is also what
	 * tells one device from another: see spudinput_shared_find(). */
	void *native;
	/* Cleared by the front end before a poll and set by the backend for
	 * each device still there; what is left clear has gone. */
	bool seen;
	/* As last set with spudinput_gamepad_set_motion_enabled(). */
	bool motion_enabled;

	SPUDINPUT_GAMEPAD_PROPERTIES properties;
	SPUDINPUT_GAMEPAD_STATE state;
	SPUDINPUT_GAMEPAD_BATTERY battery;
	SPUDINPUT_GAMEPAD_TOUCH_STATE touch[SPUDINPUT_GAMEPAD_TOUCH_SURFACE_MAX];

	/* The samples of the last poll, oldest first. Room for the instance's
	 * motion_sample_capacity, allocated by the front end the first time
	 * the sensors are switched on; null until then. */
	SPUDINPUT_GAMEPAD_MOTION_SAMPLE *motion_samples;
	uint32_t motion_sample_count;
	uint32_t motion_dropped_count;
} spudinput_gamepad_record;

struct spudinput_instance_t {
	SPUDINPUT_INSTANCE_DESC desc;

	/* The connected gamepads, in the order they were first seen. */
	spudinput_gamepad_record *records;
	uint32_t record_count;
	/* Of `records` and of `ids`. */
	uint32_t record_capacity;
	/* The ids of `records`, as spudinput_instance_get_gamepads() hands
	 * them out. Rewritten by each poll that succeeds. */
	spudinput_gamepad_id *ids;
	uint32_t id_count;
	/* The next id to give out. Starts at 1 and only goes up. */
	spudinput_gamepad_id next_id;

	/* The backend's own state for the instance, or null. */
	void *backend;
};

/* ---- For a backend's poll ---- */

/* Makes room for `extra` gamepads beyond those there are. A backend calls
 * it once, with the number of devices it is about to report, before it
 * changes anything, so that a poll that runs out of memory has changed
 * nothing. Returns SPUDRESULT_OUT_OF_MEMORY or SPUD_SUCCESS. */
SPUDRESULT spudinput_shared_reserve(
    spudinput_instance instance,
    uint32_t extra);

/* A record for a new device: zeroed, with a new id and `native` in it, and
 * marked seen. There must be room (spudinput_shared_reserve). Its id is
 * not handed out by spudinput_instance_get_gamepads() until the poll
 * succeeds. */
spudinput_gamepad_record *spudinput_shared_add(
    spudinput_instance instance,
    void *native);

/* Copies a null-terminated UTF-8 string into `size` bytes, terminator
 * included, cutting it at a whole character if it doesn't fit. A null
 * `source` gives an empty string. `size` is at least 1. */
void spudinput_shared_copy_utf8(
    char *destination,
    uint32_t size,
    const char *source);

/* ---- What a backend provides ---- */

/*
 * Exactly one set of these is compiled in: a backend's, or the front end's
 * own (in spudinput.c) where no native API is built, which has no API to
 * accept and so is never asked for anything else.
 *
 * Arguments have been checked by the front end: instance and record are
 * real, the record's gamepad has the feature the call is about, and every
 * number is in range.
 */

/* Whether this build can bind an instance to `api`. */
bool spudinput_backend_has_api(SPUDINPUT_NATIVE_API api);

/* Starts the platform's API for a new instance whose desc is in place.
 * Returns SPUDRESULT_SPUDINPUT_UNSUPPORTED for a read_in_background the API
 * doesn't offer. On failure the instance is freed by the front end and
 * spudinput_backend_destroy() is not called. */
SPUDRESULT spudinput_backend_create(spudinput_instance instance);

/* Ends it. Every gamepad has already been released. */
void spudinput_backend_destroy(spudinput_instance instance);

/*
 * Reads the platform: for each device still there, finds its record or
 * adds one (filling its properties), writes its state, battery, touch and
 * motion samples, and marks it seen. The front end then releases what was
 * not seen and rewrites the id list.
 *
 * On failure the front end changes nothing more, so a backend allocates
 * what it needs before it writes to any record.
 */
SPUDRESULT spudinput_backend_poll(spudinput_instance instance);

/* The device has gone, or the instance is going: stops its motors and its
 * sensors and frees `record->native`. */
void spudinput_backend_release_gamepad(
    spudinput_instance instance,
    spudinput_gamepad_record *record);

SPUDRESULT spudinput_backend_set_rumble(
    spudinput_instance instance,
    spudinput_gamepad_record *record,
    const SPUDINPUT_GAMEPAD_RUMBLE *rumble);

/* `enabled` differs from record->motion_enabled, which the front end sets
 * afterwards, and record->motion_samples is allocated. */
SPUDRESULT spudinput_backend_set_motion_enabled(
    spudinput_instance instance,
    spudinput_gamepad_record *record,
    bool enabled);

SPUDRESULT spudinput_backend_set_light_color(
    spudinput_instance instance,
    spudinput_gamepad_record *record,
    float red,
    float green,
    float blue);

SPUDRESULT spudinput_backend_set_player_indicator(
    spudinput_instance instance,
    spudinput_gamepad_record *record,
    uint32_t player);

/* `out_label` holds an empty string on entry; left so where the platform
 * has no label. `button` is exactly one bit. */
void spudinput_backend_get_button_label(
    spudinput_instance instance,
    spudinput_gamepad_record *record,
    SPUDINPUT_GAMEPAD_BUTTONS button,
    char *out_label,
    uint32_t label_size);

void spudinput_backend_get_axis_label(
    spudinput_instance instance,
    spudinput_gamepad_record *record,
    SPUDINPUT_GAMEPAD_AXIS axis,
    char *out_label,
    uint32_t label_size);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // SPUDLIB_SPUDINPUTSHARED_H
