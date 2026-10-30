// Only when the GameInput API is compiled in (see spudinput.h).
#if SPUDINPUT_COMPILE_GAMEINPUT

// Part of src/spudinput.c's translation unit, which includes it; never
// compiled on its own (see src/input/spudinputshared.h).
#ifndef SPUDINPUT_FRONT_END
#error "spudinputgameinput.c is included by src/spudinput.c, not compiled on its own."
#endif

/*
 * SpudInput on GameInput: Windows.
 *
 * The GameInput of the Windows SDK (GameInput.h, gameinput.lib, the
 * GameInput.dll the system or its redistributable installs). A device is a
 * gamepad when GameInput presents it as GameInputKindGamepad, and
 * GameInputGamepadState is the whole of the mapping: there is no table of
 * devices in this file.
 *
 * GameInput says which devices come and go only through a device callback.
 * The gamepads connected when the instance is made are listed then, with a
 * blocking enumeration, so the first poll has them. After that, so that
 * nothing runs on a thread SpudInput didn't ask for, the instance makes a
 * dispatcher, which takes GameInput's work off its own threads: the work,
 * and the callbacks with it, then run inside the dispatches each poll
 * makes. The callback only writes down which devices are connected; the
 * poll turns that into records. The list has a lock all the same, taken
 * only outside a dispatch, in case a runtime delivers a callback elsewhere.
 *
 * GameInput keeps a history of each device's readings, so a poll walks
 * every reading since the one it last took, and a press shorter than the
 * time between two polls is still in buttons_went_down and buttons_went_up.
 * When the reading taken last has left the history
 * (GAMEINPUT_E_REFERENCE_READING_TOO_OLD), the poll takes the newest one and
 * what happened in between is reported as the difference.
 *
 * GameInput's gamepad, as this file reads it:
 * - Sticks: -1 to 1, y positive away from the user, as spudinput.h has it.
 * - Triggers: 0 to 1.
 * - Face buttons: A south, B east, X west, Y north. View is BACK, Menu is
 *   START. The guide button is not part of GameInput's gamepad state.
 *
 * Rumble is SetRumbleState with the four motor levels, which GameInput holds
 * until they are set again. A gamepad has the battery feature when GameInput
 * says anything of its battery when it is first seen.
 *
 * Not given: motion sensors, touch surfaces, lights and labels. GameInput's
 * gamepad has no sensors or touch surfaces, no way to set a light, and its
 * labels are an enum rather than the platform's words.
 *
 * The SDK's C macros for IGameInputDispatcher are missing a closing
 * parenthesis, so every method here is called through lpVtbl.
 */

#include "../../spudinputshared.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <GameInput.h>

#include <stdlib.h>
#include <string.h>

/* The most dispatches one poll makes. Each does the work that can be done
 * without waiting; more than a few in a row only happen when devices come
 * and go in a burst, and what is left is done at the next poll. */
#define SPUDINPUT_GAMEINPUT_DISPATCH_MAX 16

/* One gamepad: the device, and the reading of it taken last. */
typedef struct spudinput_gameinput_pad {
	IGameInputDevice *device;
	/* Null until the first reading. */
	IGameInputReading *reading;
} spudinput_gameinput_pad;

typedef EQ_ARRAY(IGameInputDevice *) spudinput_gameinput_device_array;

typedef struct spudinput_gameinput_instance {
	IGameInput *game_input;
	IGameInputDispatcher *dispatcher;
	GameInputCallbackToken device_token;
	/* The instance's allocator, for the device callback, which is given only
	 * this struct. */
	const SPUD_ALLOCATOR *allocator;
	/* eqstd's view of it (spud_eq_allocator), which `connected` points at. */
	EQ_ALLOCATOR eq_allocator;

	/* Guards what follows, which the device callback writes. */
	SRWLOCK lock;
	/* The gamepads connected, as the callback has said, each with a
	 * reference of its own. */
	spudinput_gameinput_device_array connected;
	/* A callback could not write a change down for want of memory: the
	 * list is to be made again from GameInput. */
	bool lost;
} spudinput_gameinput_instance;

/* GameInput's gamepad buttons, each with spudinput.h's. */
static const struct {
	GameInputGamepadButtons native;
	SPUDINPUT_GAMEPAD_BUTTONS button;
} spudinput_gameinput_buttons[] = {
	{GameInputGamepadA, SPUDINPUT_GAMEPAD_BUTTON_SOUTH},
	{GameInputGamepadB, SPUDINPUT_GAMEPAD_BUTTON_EAST},
	{GameInputGamepadX, SPUDINPUT_GAMEPAD_BUTTON_WEST},
	{GameInputGamepadY, SPUDINPUT_GAMEPAD_BUTTON_NORTH},
	{GameInputGamepadLeftShoulder, SPUDINPUT_GAMEPAD_BUTTON_LEFT_SHOULDER},
	{GameInputGamepadRightShoulder, SPUDINPUT_GAMEPAD_BUTTON_RIGHT_SHOULDER},
	{GameInputGamepadLeftThumbstick, SPUDINPUT_GAMEPAD_BUTTON_LEFT_STICK},
	{GameInputGamepadRightThumbstick, SPUDINPUT_GAMEPAD_BUTTON_RIGHT_STICK},
	{GameInputGamepadView, SPUDINPUT_GAMEPAD_BUTTON_BACK},
	{GameInputGamepadMenu, SPUDINPUT_GAMEPAD_BUTTON_START},
	{GameInputGamepadDPadUp, SPUDINPUT_GAMEPAD_BUTTON_DPAD_UP},
	{GameInputGamepadDPadDown, SPUDINPUT_GAMEPAD_BUTTON_DPAD_DOWN},
	{GameInputGamepadDPadLeft, SPUDINPUT_GAMEPAD_BUTTON_DPAD_LEFT},
	{GameInputGamepadDPadRight, SPUDINPUT_GAMEPAD_BUTTON_DPAD_RIGHT},
};

#define SPUDINPUT_GAMEINPUT_BUTTON_COUNT (sizeof(spudinput_gameinput_buttons) / sizeof(spudinput_gameinput_buttons[0]))

static SPUDINPUT_GAMEPAD_BUTTONS spudinput_gameinput_map_buttons(GameInputGamepadButtons native) {
	SPUDINPUT_GAMEPAD_BUTTONS buttons = 0;
	for (size_t i = 0; i < SPUDINPUT_GAMEINPUT_BUTTON_COUNT; ++i) {
		if (native & spudinput_gameinput_buttons[i].native)
			buttons |= spudinput_gameinput_buttons[i].button;
	}
	return buttons;
}

/* ---- Which devices are connected ---- */

static int64_t spudinput_gameinput_find_connected(
    const spudinput_gameinput_instance *backend,
    const IGameInputDevice *device) {
	for (size_t i = 0; i < backend->connected.count; ++i) {
		if (backend->connected.data[i] == device)
			return (int64_t)i;
	}
	return -1;
}

/* Drops every device the list holds. Called with the lock held, or when
 * nothing else can reach the list. */
static void spudinput_gameinput_clear_connected(spudinput_gameinput_instance *backend) {
	for (size_t i = 0; i < backend->connected.count; ++i)
		backend->connected.data[i]->lpVtbl->Release(backend->connected.data[i]);
	eq_array_clear(&backend->connected);
}

/* GameInput says a gamepad came or went. Only the move between connected
 * and not is written down. */
static void CALLBACK spudinput_gameinput_device_callback(
    GameInputCallbackToken token,
    void *context,
    IGameInputDevice *device,
    uint64_t timestamp,
    GameInputDeviceStatus current_status,
    GameInputDeviceStatus previous_status) {
	(void)token;
	(void)timestamp;
	spudinput_gameinput_instance *backend = (spudinput_gameinput_instance *)context;
	const bool is_connected               = (current_status & GameInputDeviceConnected) != 0;
	const bool was_connected              = (previous_status & GameInputDeviceConnected) != 0;
	if (is_connected == was_connected)
		return;

	AcquireSRWLockExclusive(&backend->lock);
	const int64_t index = spudinput_gameinput_find_connected(backend, device);
	if (is_connected && index < 0) {
		if (eq_array_push(&backend->connected, &device) != EQ_SUCCESS) {
			backend->lost = true;
			ReleaseSRWLockExclusive(&backend->lock);
			return;
		}
		device->lpVtbl->AddRef(device);
	} else if (!is_connected && index >= 0) {
		device->lpVtbl->Release(device);
		// An index inside the list, so the removal can't fail.
		(void)eq_array_remove_swap(&backend->connected, (size_t)index);
	}
	ReleaseSRWLockExclusive(&backend->lock);
}

/* Asks GameInput to tell the callback about every gamepad, those connected
 * now included: before this returns with GameInputBlockingEnumeration, from
 * the next dispatch on with GameInputAsyncEnumeration. */
static SPUDRESULT spudinput_gameinput_register(
    spudinput_gameinput_instance *backend,
    GameInputEnumerationKind enumeration) {
	const HRESULT hr = backend->game_input->lpVtbl->RegisterDeviceCallback(backend->game_input,
	    NULL,
	    GameInputKindGamepad,
	    GameInputDeviceConnected,
	    enumeration,
	    backend,
	    spudinput_gameinput_device_callback,
	    &backend->device_token);
	if (FAILED(hr)) {
		backend->device_token = 0;
		return SPUDRESULT_API_SPECIFIC_FAILURE;
	}
	return SPUD_SUCCESS;
}

static void spudinput_gameinput_unregister(spudinput_gameinput_instance *backend) {
	if (!backend->device_token)
		return;
	backend->game_input->lpVtbl->UnregisterCallback(backend->game_input, backend->device_token, UINT64_MAX);
	backend->device_token = 0;
}

/* ---- One gamepad ---- */

static void spudinput_gameinput_stop_rumble(IGameInputDevice *device) {
	const GameInputRumbleParams stopped = {0.0f, 0.0f, 0.0f, 0.0f};
	device->lpVtbl->SetRumbleState(device, &stopped);
}

/* What GameInput says of a gamepad's battery, as spudinput.h has it. */
static SPUDINPUT_GAMEPAD_BATTERY spudinput_gameinput_battery(const GameInputBatteryState *native) {
	SPUDINPUT_GAMEPAD_BATTERY battery = {0};
	switch (native->status) {
	case GameInputBatteryNotPresent:
		battery.state = SPUDINPUT_BATTERY_STATE_NONE;
		return battery;
	case GameInputBatteryDischarging:
		battery.state = SPUDINPUT_BATTERY_STATE_DISCHARGING;
		break;
	/* On power, and neither filling nor emptying. */
	case GameInputBatteryIdle:
		battery.state = SPUDINPUT_BATTERY_STATE_FULL;
		break;
	case GameInputBatteryCharging:
		battery.state = SPUDINPUT_BATTERY_STATE_CHARGING;
		break;
	default:
		battery.state = SPUDINPUT_BATTERY_STATE_UNKNOWN;
		return battery;
	}
	if (native->fullChargeCapacity > 0.0f) {
		float level = native->remainingCapacity / native->fullChargeCapacity;
		if (level < 0.0f)
			level = 0.0f;
		if (level > 1.0f)
			level = 1.0f;
		battery.level_known = true;
		battery.level       = level;
	}
	return battery;
}

static void spudinput_gameinput_fill_properties(
    spudinput_gamepad_record *record,
    IGameInputDevice *device) {
	const GameInputDeviceInfo *info           = device->lpVtbl->GetDeviceInfo(device);
	SPUDINPUT_GAMEPAD_PROPERTIES *properties = &record->properties;

	spudinput_shared_copy_utf8(properties->name, sizeof(properties->name), info->displayName ? info->displayName->data : NULL);
	properties->vendor_id  = info->vendorId;
	properties->product_id = info->productId;
	for (size_t i = 0; i < SPUDINPUT_GAMEINPUT_BUTTON_COUNT; ++i)
		properties->buttons |= spudinput_gameinput_buttons[i].button;
	properties->axes            = (1u << SPUDINPUT_GAMEPAD_AXIS_COUNT) - 1;
	properties->analog_triggers = true;

	if (info->supportedRumbleMotors & GameInputRumbleLowFrequency)
		properties->features |= SPUDINPUT_GAMEPAD_FEATURE_RUMBLE_LOW_FREQUENCY;
	if (info->supportedRumbleMotors & GameInputRumbleHighFrequency)
		properties->features |= SPUDINPUT_GAMEPAD_FEATURE_RUMBLE_HIGH_FREQUENCY;
	if (info->supportedRumbleMotors & GameInputRumbleLeftTrigger)
		properties->features |= SPUDINPUT_GAMEPAD_FEATURE_RUMBLE_LEFT_TRIGGER;
	if (info->supportedRumbleMotors & GameInputRumbleRightTrigger)
		properties->features |= SPUDINPUT_GAMEPAD_FEATURE_RUMBLE_RIGHT_TRIGGER;

	GameInputBatteryState battery;
	device->lpVtbl->GetBatteryState(device, &battery);
	if (battery.status != GameInputBatteryUnknown)
		properties->features |= SPUDINPUT_GAMEPAD_FEATURE_BATTERY;
}

/* Takes one reading into the record's state: the buttons that changed since
 * the state before it, and the axes as they are in it. */
static void spudinput_gameinput_take_reading(
    spudinput_gamepad_record *record,
    IGameInputReading *reading) {
	GameInputGamepadState native;
	if (!reading->lpVtbl->GetGamepadState(reading, &native))
		return;

	SPUDINPUT_GAMEPAD_STATE *state          = &record->state;
	const SPUDINPUT_GAMEPAD_BUTTONS buttons = spudinput_gameinput_map_buttons(native.buttons);
	state->buttons_went_down |= buttons & ~state->buttons;
	state->buttons_went_up |= state->buttons & ~buttons;
	state->buttons = buttons;

	state->axes[SPUDINPUT_GAMEPAD_AXIS_LEFT_STICK_X]  = native.leftThumbstickX;
	state->axes[SPUDINPUT_GAMEPAD_AXIS_LEFT_STICK_Y]  = native.leftThumbstickY;
	state->axes[SPUDINPUT_GAMEPAD_AXIS_RIGHT_STICK_X] = native.rightThumbstickX;
	state->axes[SPUDINPUT_GAMEPAD_AXIS_RIGHT_STICK_Y] = native.rightThumbstickY;
	state->axes[SPUDINPUT_GAMEPAD_AXIS_LEFT_TRIGGER]  = native.leftTrigger;
	state->axes[SPUDINPUT_GAMEPAD_AXIS_RIGHT_TRIGGER] = native.rightTrigger;
}

/* Every reading of the gamepad since the one taken last, into its record.
 * The first time, the newest reading alone: the buttons down in it are the
 * ones that went down. */
static void spudinput_gameinput_read_pad(
    IGameInput *game_input,
    spudinput_gamepad_record *record) {
	spudinput_gameinput_pad *pad    = (spudinput_gameinput_pad *)record->native;
	record->state.buttons_went_down = 0;
	record->state.buttons_went_up   = 0;

	if (pad->reading) {
		for (;;) {
			IGameInputReading *next = NULL;
			const HRESULT hr        = game_input->lpVtbl->GetNextReading(game_input, pad->reading, GameInputKindGamepad, pad->device, &next);
			if (SUCCEEDED(hr)) {
				spudinput_gameinput_take_reading(record, next);
				pad->reading->lpVtbl->Release(pad->reading);
				pad->reading = next;
				continue;
			}
			/* No newer reading: what was there has all been taken. */
			if (hr != GAMEINPUT_E_REFERENCE_READING_TOO_OLD)
				return;
			/* The one taken last has left the history: start again from the
			 * newest. */
			pad->reading->lpVtbl->Release(pad->reading);
			pad->reading = NULL;
			break;
		}
	}

	IGameInputReading *current = NULL;
	if (FAILED(game_input->lpVtbl->GetCurrentReading(game_input, GameInputKindGamepad, pad->device, &current)))
		return;
	spudinput_gameinput_take_reading(record, current);
	pad->reading = current;
}

static void spudinput_gameinput_read_battery(spudinput_gamepad_record *record) {
	if (!(record->properties.features & SPUDINPUT_GAMEPAD_FEATURE_BATTERY))
		return;
	const spudinput_gameinput_pad *pad = (const spudinput_gameinput_pad *)record->native;
	GameInputBatteryState battery;
	pad->device->lpVtbl->GetBatteryState(pad->device, &battery);
	record->battery = spudinput_gameinput_battery(&battery);
}

/* ---- Backend ---- */

SPUDINPUT_BACKEND_FUNCTION SPUDRESULT spudinput_gameinput_create(spudinput_instance instance) {
	const SPUD_ALLOCATOR *allocator       = &instance->head.allocator;
	spudinput_gameinput_instance *backend = SPUD_NEW_ZEROED(allocator, spudinput_gameinput_instance);
	if (!backend)
		return SPUDRESULT_OUT_OF_MEMORY;
	backend->allocator    = allocator;
	backend->eq_allocator = spud_eq_allocator(allocator);
	eq_array_init(&backend->connected, &backend->eq_allocator);
	InitializeSRWLock(&backend->lock);

	if (FAILED(GameInputCreate(&backend->game_input))) {
		SPUD_DELETE(allocator, spudinput_gameinput_instance, backend);
		return SPUDRESULT_API_SPECIFIC_FAILURE;
	}

	/* GameInput's own default reads in the background. */
	backend->game_input->lpVtbl->SetFocusPolicy(
	    backend->game_input, instance->desc.read_in_background ? GameInputDefaultFocusPolicy : GameInputDisableBackgroundInput);

	/* The gamepads connected now are in the list before this returns, so
	 * the first poll lists them. Before the dispatcher: a blocking
	 * enumeration waits on GameInput's work, which with a dispatcher is
	 * done only when one is dispatched. */
	const SPUDRESULT result = spudinput_gameinput_register(backend, GameInputBlockingEnumeration);
	if (result != SPUD_SUCCESS) {
		backend->game_input->lpVtbl->Release(backend->game_input);
		eq_array_free(&backend->connected);
		SPUD_DELETE(allocator, spudinput_gameinput_instance, backend);
		return result;
	}

	/* From here GameInput's work, the callback's included, is done in the
	 * polls' dispatches. */
	if (FAILED(backend->game_input->lpVtbl->CreateDispatcher(backend->game_input, &backend->dispatcher))) {
		spudinput_gameinput_unregister(backend);
		spudinput_gameinput_clear_connected(backend);
		eq_array_free(&backend->connected);
		backend->game_input->lpVtbl->Release(backend->game_input);
		SPUD_DELETE(allocator, spudinput_gameinput_instance, backend);
		return SPUDRESULT_API_SPECIFIC_FAILURE;
	}

	instance->backend = backend;
	return SPUD_SUCCESS;
}

SPUDINPUT_BACKEND_FUNCTION void spudinput_gameinput_destroy(spudinput_instance instance) {
	spudinput_gameinput_instance *backend = (spudinput_gameinput_instance *)instance->backend;

	spudinput_gameinput_unregister(backend);
	spudinput_gameinput_clear_connected(backend);
	eq_array_free(&backend->connected);
	backend->dispatcher->lpVtbl->Release(backend->dispatcher);
	backend->game_input->lpVtbl->Release(backend->game_input);
	SPUD_DELETE(&instance->head.allocator, spudinput_gameinput_instance, backend);
	instance->backend = NULL;
}

SPUDINPUT_BACKEND_FUNCTION SPUDRESULT spudinput_gameinput_poll(spudinput_instance instance) {
	spudinput_gameinput_instance *backend = (spudinput_gameinput_instance *)instance->backend;

	/* GameInput's work, the device callbacks among it, without the lock:
	 * the callback takes it. */
	for (int i = 0; i < SPUDINPUT_GAMEINPUT_DISPATCH_MAX; ++i) {
		if (!backend->dispatcher->lpVtbl->Dispatch(backend->dispatcher, 0))
			break;
	}

	AcquireSRWLockExclusive(&backend->lock);
	const bool lost = backend->lost;
	ReleaseSRWLockExclusive(&backend->lock);
	if (lost) {
		/* A change went unrecorded: ask GameInput for every gamepad again.
		 * Until it has answered, none is connected; the records are kept as
		 * they are by failing this poll. */
		spudinput_gameinput_unregister(backend);
		AcquireSRWLockExclusive(&backend->lock);
		spudinput_gameinput_clear_connected(backend);
		backend->lost = false;
		ReleaseSRWLockExclusive(&backend->lock);
		/* Asynchronous: with the dispatcher made, a blocking enumeration
		 * would wait on work only a dispatch does. */
		const SPUDRESULT result = spudinput_gameinput_register(backend, GameInputAsyncEnumeration);
		return result == SPUD_SUCCESS ? SPUDRESULT_OUT_OF_MEMORY : result;
	}

	AcquireSRWLockShared(&backend->lock);

	/* Everything that can fail for want of memory, before any record is
	 * written to: a pad for each device that has no record yet, and room
	 * for them in the table. */
	uint32_t arrived_count = 0;
	for (size_t i = 0; i < backend->connected.count; ++i) {
		bool known = false;
		for (uint32_t j = 0; j < instance->record_count && !known; ++j)
			known = ((spudinput_gameinput_pad *)instance->records[j].native)->device == backend->connected.data[i];
		if (!known)
			++arrived_count;
	}

	spudinput_gameinput_pad **arrived = NULL;
	if (arrived_count) {
		arrived = SPUD_NEW_ARRAY_ZEROED(&instance->head.allocator, spudinput_gameinput_pad *, arrived_count);
		if (!arrived) {
			ReleaseSRWLockShared(&backend->lock);
			return SPUDRESULT_OUT_OF_MEMORY;
		}
		for (uint32_t i = 0; i < arrived_count; ++i) {
			arrived[i] = SPUD_NEW_ZEROED(&instance->head.allocator, spudinput_gameinput_pad);
			if (!arrived[i]) {
				for (uint32_t j = 0; j < i; ++j)
					SPUD_DELETE(&instance->head.allocator, spudinput_gameinput_pad, arrived[j]);
				SPUD_DELETE_ARRAY(&instance->head.allocator, spudinput_gameinput_pad *, arrived, arrived_count);
				ReleaseSRWLockShared(&backend->lock);
				return SPUDRESULT_OUT_OF_MEMORY;
			}
		}
	}
	const SPUDRESULT reserved = spudinput_shared_reserve(instance, arrived_count);
	if (reserved != SPUD_SUCCESS) {
		for (uint32_t i = 0; i < arrived_count; ++i)
			SPUD_DELETE(&instance->head.allocator, spudinput_gameinput_pad, arrived[i]);
		SPUD_DELETE_ARRAY(&instance->head.allocator, spudinput_gameinput_pad *, arrived, arrived_count);
		ReleaseSRWLockShared(&backend->lock);
		return reserved;
	}

	/* The gamepads already known that are still there. */
	const uint32_t known_count = instance->record_count;
	for (uint32_t j = 0; j < known_count; ++j) {
		spudinput_gamepad_record *record = &instance->records[j];
		spudinput_gameinput_pad *pad     = (spudinput_gameinput_pad *)record->native;
		if (spudinput_gameinput_find_connected(backend, pad->device) < 0)
			continue;
		spudinput_gameinput_read_pad(backend->game_input, record);
		spudinput_gameinput_read_battery(record);
		record->seen = true;
	}

	/* The new ones. */
	uint32_t next = 0;
	for (size_t i = 0; i < backend->connected.count; ++i) {
		IGameInputDevice *device = backend->connected.data[i];
		bool known               = false;
		for (uint32_t j = 0; j < known_count && !known; ++j)
			known = ((spudinput_gameinput_pad *)instance->records[j].native)->device == device;
		if (known)
			continue;

		spudinput_gameinput_pad *pad = arrived[next++];
		pad->device                  = device;
		device->lpVtbl->AddRef(device);
		spudinput_gameinput_stop_rumble(device);

		spudinput_gamepad_record *record = spudinput_shared_add(instance, pad);
		spudinput_gameinput_fill_properties(record, device);
		spudinput_gameinput_read_pad(backend->game_input, record);
		spudinput_gameinput_read_battery(record);
	}
	ReleaseSRWLockShared(&backend->lock);
	SPUD_DELETE_ARRAY(&instance->head.allocator, spudinput_gameinput_pad *, arrived, arrived_count);

	return SPUD_SUCCESS;
}

SPUDINPUT_BACKEND_FUNCTION void spudinput_gameinput_release_gamepad(
    spudinput_instance instance,
    spudinput_gamepad_record *record) {
	spudinput_gameinput_pad *pad = (spudinput_gameinput_pad *)record->native;
	spudinput_gameinput_stop_rumble(pad->device);
	if (pad->reading)
		pad->reading->lpVtbl->Release(pad->reading);
	pad->device->lpVtbl->Release(pad->device);
	SPUD_DELETE(&instance->head.allocator, spudinput_gameinput_pad, pad);
	record->native = NULL;
}

SPUDINPUT_BACKEND_FUNCTION SPUDRESULT spudinput_gameinput_set_rumble(
    spudinput_instance instance,
    spudinput_gamepad_record *record,
    const SPUDINPUT_GAMEPAD_RUMBLE *rumble) {
	(void)instance;
	const spudinput_gameinput_pad *pad = (const spudinput_gameinput_pad *)record->native;
	const GameInputRumbleParams params = {rumble->low_frequency, rumble->high_frequency, rumble->left_trigger, rumble->right_trigger};
	pad->device->lpVtbl->SetRumbleState(pad->device, &params);
	return SPUD_SUCCESS;
}

/* What follows is never reached: no gamepad is given the feature it is
 * about, and the front end checks that first. */

SPUDINPUT_BACKEND_FUNCTION SPUDRESULT spudinput_gameinput_set_motion_enabled(
    spudinput_instance instance,
    spudinput_gamepad_record *record,
    bool enabled) {
	(void)instance;
	(void)record;
	(void)enabled;
	return SPUDRESULT_SPUDINPUT_FEATURE_NOT_PRESENT;
}

SPUDINPUT_BACKEND_FUNCTION SPUDRESULT spudinput_gameinput_set_light_color(
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
	return SPUDRESULT_SPUDINPUT_FEATURE_NOT_PRESENT;
}

SPUDINPUT_BACKEND_FUNCTION SPUDRESULT spudinput_gameinput_set_player_indicator(
    spudinput_instance instance,
    spudinput_gamepad_record *record,
    uint32_t player) {
	(void)instance;
	(void)record;
	(void)player;
	return SPUDRESULT_SPUDINPUT_FEATURE_NOT_PRESENT;
}

SPUDINPUT_BACKEND_FUNCTION void spudinput_gameinput_get_button_label(
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

SPUDINPUT_BACKEND_FUNCTION void spudinput_gameinput_get_axis_label(
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

#endif // SPUDINPUT_COMPILE_GAMEINPUT
