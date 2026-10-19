#ifndef SPUDSTRUCTBASE_H
#define SPUDSTRUCTBASE_H

// The checks every call makes of a caller's based struct (SPUD_STRUCT_BASE,
// spudcore.h) before it reads any other field, written once so that every
// module and backend returns the same result for the same struct.
//
// No struct can be chained onto any SpudLib struct yet, so anything in a
// chain is a struct this build doesn't take there. When the first chained
// struct is added, the call that takes it walks its own chain instead.

#include "spudcore.h"

// A struct the caller filled in: [type] is the type the call takes there.
static inline SPUDRESULT spud_struct_check(const SPUD_STRUCT_BASE *base, SPUD_STRUCT_TYPE type) {
	if (base->type != type)
		return SPUDRESULT_STRUCT_WRONG_TYPE;
	if (base->next)
		return SPUDRESULT_STRUCT_UNKNOWN_IN_CHAIN;
	return SPUD_SUCCESS;
}

// A struct the caller hands over for SpudLib to fill in: the same checks,
// made before anything is written to it.
static inline SPUDRESULT spud_struct_out_check(const SPUD_STRUCT_BASE_OUT *base, SPUD_STRUCT_TYPE type) {
	if (base->type != type)
		return SPUDRESULT_STRUCT_WRONG_TYPE;
	if (base->next)
		return SPUDRESULT_STRUCT_UNKNOWN_IN_CHAIN;
	return SPUD_SUCCESS;
}

// The base a struct SpudLib hands back has: its own type, and no chain.
static inline SPUD_STRUCT_BASE spud_struct_base(SPUD_STRUCT_TYPE type) {
	SPUD_STRUCT_BASE base;
	base.type = type;
	base.next = NULL;
	return base;
}

static inline SPUD_STRUCT_BASE_OUT spud_struct_base_out(SPUD_STRUCT_TYPE type) {
	SPUD_STRUCT_BASE_OUT base;
	base.type = type;
	base.next = NULL;
	return base;
}

#endif // SPUDSTRUCTBASE_H
