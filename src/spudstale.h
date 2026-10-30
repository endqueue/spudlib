#ifndef SPUDSTALE_H
#define SPUDSTALE_H

// A stale or double-freed handle, caught in Debug builds (decision X16 in
// the workspace's DECISIONS.md): a tag word in every handle's
// spud_object_head (spudallocator.h) that reads SPUD_LIVE while the object
// lives and SPUD_DEAD from its destroy call on.
//
// - Set to SPUD_LIVE where a create (or the owner of an object with no
//   destroy call of its own) fills the head: SPUD_BIRTH.
// - Written SPUD_DEAD at the destroy call, before the memory is freed, and
//   by an owner for each object it releases: SPUD_KILL.
// - Checked in every public function that takes a handle, right after that
//   handle's null check, as its own `if` that refuses exactly as the null
//   check does (the same result, the same fallback for a getter, a plain
//   return for a void call): SPUD_STALE. A Debug build prints one line on
//   stderr when it refuses, so a stale handle is seen even where the result
//   is ignored.
//
// One LIVE and one DEAD value for the whole library, not one per type:
// enough for a call after a destroy and for a second destroy. Anything but
// SPUD_LIVE refuses, so memory the allocator has written over since is
// caught too. Memory reused by a new object of the library can still pass;
// AddressSanitizer covers that case.
//
// Release builds have no tag, no check and no call: SPUD_STALE is the
// constant false, so the `if` it is in disappears, and SPUD_BIRTH and
// SPUD_KILL are nothing. Only _DEBUG decides it.

#include <stdbool.h>
#include <stdint.h>

// "SPUDLIVE" and "SPUDDEAD" in ASCII, as a memory view shows the word on a
// little-endian machine.
#define SPUD_LIVE UINT64_C(0x4556494C44555053)
#define SPUD_DEAD UINT64_C(0x4441454444555053)

#if _DEBUG
#include <stdio.h>

// True, with a line on stderr, when [tag] isn't SPUD_LIVE.
static inline bool spud_stale_tag(uint64_t tag, const char *function, const char *param) {
	if (tag == SPUD_LIVE)
		return false;
	fprintf(stderr, "spudlib: %s: called with a destroyed %s\n", function, param);
	return true;
}

// [obj] is a handle, never NULL. Every handle's struct starts with its
// spud_object_head (spudallocator.h), so the head is reached through the
// pointer itself, also where the struct is a backend's and isn't complete
// (a front end checking a handle it passes on).
#define SPUD_HEAD_OF(obj) ((spud_object_head *)(void *)(obj))
#define SPUD_BIRTH(obj) (SPUD_HEAD_OF(obj)->debug_tag = SPUD_LIVE)
#define SPUD_KILL(obj) (SPUD_HEAD_OF(obj)->debug_tag = SPUD_DEAD)
#define SPUD_STALE(obj, function, param) spud_stale_tag(SPUD_HEAD_OF(obj)->debug_tag, (function), (param))
// The same check through a spud_object_head pointer.
#define SPUD_STALE_HEAD(head_ptr, function, param) spud_stale_tag((head_ptr)->debug_tag, (function), (param))
#else
#define SPUD_BIRTH(obj) ((void)0)
#define SPUD_KILL(obj) ((void)0)
#define SPUD_STALE(obj, function, param) false
#define SPUD_STALE_HEAD(head_ptr, function, param) false
#endif

#endif // SPUDSTALE_H
