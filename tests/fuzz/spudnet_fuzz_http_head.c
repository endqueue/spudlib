/*
 * spudnet_fuzz_http_head - a response's header block, as the stacks hand
 * it to SpudNet's front end (src/net/spudnethttp.c).
 *
 * libcurl and WinHTTP give SpudNet the raw block a line at a time, which
 * spudnet_head_add_header_line takes apart; NSURLSession gives name and
 * value pairs, which go to spudnet_head_add_header. Either way the bytes
 * are the server's. The input is:
 *
 *   byte 0   bit 0: lines (0) or pairs (1); bit 1: each line handed over
 *            with its '\n' (libcurl's way) or without it (WinHTTP's)
 *   byte 1   0xFF: no allocation refused; otherwise the allocations after
 *            which every one is refused
 *   rest     the block, lines split at '\n'; in pair mode each line is a
 *            name, the first ':', and a value, taken as they are
 *
 * Checked: nothing is read outside a line, nothing leaks or is freed
 * wrongly (on the out-of-memory paths too), every entry is terminated at
 * its length, and, with no allocation refused, the head holds exactly what
 * a plain reading of the block gives - each name once, without regard to
 * case, a repeat's value joined on with ", " - and spudnet_head_get_header
 * finds each of them under its name in any case.
 */

#include "spudfuzz.h"

#include "net/spudnetshared.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// --------------------------------------------------------------------------
// The plain reading, to compare with
// --------------------------------------------------------------------------

typedef struct ref_bytes {
	char  *data;
	size_t length;
} ref_bytes;

typedef struct ref_entry {
	ref_bytes name;
	ref_bytes value;
} ref_entry;

typedef struct ref_head {
	ref_entry *entries;
	size_t     count;
	size_t     capacity;
} ref_head;

static char ref_lower(char c) {
	return (c >= 'A' && c <= 'Z') ? (char)(c + ('a' - 'A')) : c;
}

static bool ref_same_name(const char *a, size_t a_length, const char *b, size_t b_length) {
	if (a_length != b_length)
		return false;
	for (size_t i = 0; i < a_length; ++i) {
		if (ref_lower(a[i]) != ref_lower(b[i]))
			return false;
	}
	return true;
}

static ref_bytes ref_copy(const char *data, size_t length) {
	ref_bytes bytes;
	bytes.data = (char *)malloc(length + 1);
	if (!bytes.data)
		abort();
	if (length)
		memcpy(bytes.data, data, length);
	bytes.data[length] = '\0';
	bytes.length       = length;
	return bytes;
}

static void ref_clear(ref_head *head) {
	for (size_t i = 0; i < head->count; ++i) {
		free(head->entries[i].name.data);
		free(head->entries[i].value.data);
	}
	head->count = 0;
}

static void ref_add(ref_head *head, const char *name, size_t name_length, const char *value, size_t value_length) {
	for (size_t i = 0; i < head->count; ++i) {
		ref_entry *entry = &head->entries[i];
		if (!ref_same_name(entry->name.data, entry->name.length, name, name_length))
			continue;
		size_t joined_length = entry->value.length + 2 + value_length;
		char  *joined        = (char *)malloc(joined_length + 1);
		if (!joined)
			abort();
		memcpy(joined, entry->value.data, entry->value.length);
		joined[entry->value.length]     = ',';
		joined[entry->value.length + 1] = ' ';
		if (value_length)
			memcpy(joined + entry->value.length + 2, value, value_length);
		joined[joined_length] = '\0';
		free(entry->value.data);
		entry->value.data   = joined;
		entry->value.length = joined_length;
		return;
	}
	if (head->count == head->capacity) {
		head->capacity = head->capacity ? head->capacity * 2 : 16;
		ref_entry *moved = (ref_entry *)realloc(head->entries, head->capacity * sizeof(ref_entry));
		if (!moved)
			abort();
		head->entries = moved;
	}
	head->entries[head->count].name  = ref_copy(name, name_length);
	head->entries[head->count].value = ref_copy(value, value_length);
	head->count++;
}

static bool ref_blank(char c) {
	return c == ' ' || c == '\t';
}

/* A raw line as RFC 9112 lays a header out, read the plain way: line
 * ending off; a status line starts the block again; "name: value" with the
 * value's blanks trimmed; anything else is not a header. */
static void ref_add_line(ref_head *head, const char *line, size_t length) {
	while (length > 0 && (line[length - 1] == '\r' || line[length - 1] == '\n'))
		length--;
	if (length >= 5 && ref_same_name(line, 5, "HTTP/", 5)) {
		ref_clear(head);
		return;
	}
	const char *colon = (const char *)memchr(line, ':', length);
	if (!colon || colon == line)
		return;
	size_t      name_length = (size_t)(colon - line);
	const char *value       = colon + 1;
	const char *end         = line + length;
	while (value < end && ref_blank(*value))
		value++;
	while (end > value && ref_blank(end[-1]))
		end--;
	ref_add(head, line, name_length, value, (size_t)(end - value));
}

// --------------------------------------------------------------------------
// The target
// --------------------------------------------------------------------------

/* Every entry is a name and a value, each terminated at its length. */
static void check_entries(const struct spudnet_http_head *head) {
	SPUDFUZZ_CHECK(head->entries.count <= head->entries.capacity);
	SPUDFUZZ_CHECK(head->entries.capacity == 0 || head->entries.data);
	SPUDFUZZ_CHECK(head->slot_count == 2 * head->entries.capacity);
	for (size_t i = 0; i < head->entries.count; ++i) {
		const struct spudnet_http_header_entry *entry = &head->entries.data[i];
		SPUDFUZZ_CHECK(entry->name && entry->value);
		SPUDFUZZ_CHECK(entry->name[entry->name_length] == '\0');
		SPUDFUZZ_CHECK(entry->value[entry->value_length] == '\0');
	}
}

/* `name` in the case each letter is not, which a lookup must not mind. */
static char *flip_case(const char *name, size_t length) {
	char *flipped = (char *)malloc(length + 1);
	if (!flipped)
		abort();
	for (size_t i = 0; i < length; ++i) {
		char c = name[i];
		if (c >= 'a' && c <= 'z')
			c = (char)(c - ('a' - 'A'));
		else if (c >= 'A' && c <= 'Z')
			c = (char)(c + ('a' - 'A'));
		flipped[i] = c;
	}
	flipped[length] = '\0';
	return flipped;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	spudfuzz_reader reader       = {data, size};
	const uint8_t   mode         = spudfuzz_byte(&reader);
	const uint8_t   refuse       = spudfuzz_byte(&reader);
	const bool      pairs        = (mode & 1) != 0;
	const bool      keep_newline = (mode & 2) != 0;
	const size_t    refuse_after = refuse == 0xFF ? SIZE_MAX : refuse;

	spudtest_allocator       tracker;
	const SPUD_ALLOCATOR     allocator = spudfuzz_allocator(&tracker, refuse_after);
	struct spudnet_http_head head;
	memset(&head, 0, sizeof(head));
	ref_head ref;
	memset(&ref, 0, sizeof(ref));

	bool           stored = true;
	const uint8_t *at     = reader.data;
	const uint8_t *end    = reader.data + reader.size;
	while (at < end && stored) {
		const uint8_t *newline = (const uint8_t *)memchr(at, '\n', (size_t)(end - at));
		size_t         length  = newline ? (size_t)(newline - at) : (size_t)(end - at);
		size_t         handed  = length + (newline && keep_newline ? 1 : 0);
		// Each line in a block of its own size, as a stack's buffer ends.
		char *line = spudfuzz_terminated(at, handed);
		if (pairs) {
			const char *colon       = (const char *)memchr(line, ':', length);
			size_t      name_length = colon ? (size_t)(colon - line) : length;
			const char *value       = colon ? colon + 1 : line + length;
			size_t      value_len   = colon ? length - name_length - 1 : 0;
			stored                  = spudnet_head_add_header(&allocator, &head, line, name_length, value, value_len);
			if (stored)
				ref_add(&ref, line, name_length, value, value_len);
		} else {
			stored = spudnet_head_add_header_line(&allocator, &head, line, handed);
			if (stored)
				ref_add_line(&ref, line, handed);
		}
		free(line);
		check_entries(&head);
		at += newline ? length + 1 : length;
	}
	// Only running out of memory may refuse a line.
	SPUDFUZZ_CHECK(stored || refuse_after != SIZE_MAX);

	if (stored) {
		SPUDFUZZ_CHECK(head.entries.count == ref.count);
		for (size_t i = 0; i < ref.count; ++i) {
			const struct spudnet_http_header_entry *entry = &head.entries.data[i];
			SPUDFUZZ_CHECK(entry->name_length == ref.entries[i].name.length);
			SPUDFUZZ_CHECK(memcmp(entry->name, ref.entries[i].name.data, entry->name_length) == 0);
			SPUDFUZZ_CHECK(entry->value_length == ref.entries[i].value.length);
			SPUDFUZZ_CHECK(memcmp(entry->value, ref.entries[i].value.data, entry->value_length) == 0);

			// Looked up by a name with no '\0' in it, in either case, the
			// first entry of that name is the one found: there is only one.
			if (strlen(ref.entries[i].name.data) != ref.entries[i].name.length)
				continue;
			SPUDFUZZ_CHECK(spudnet_head_get_header(&head, ref.entries[i].name.data) == entry->value);
			char *flipped = flip_case(ref.entries[i].name.data, ref.entries[i].name.length);
			SPUDFUZZ_CHECK(spudnet_head_get_header(&head, flipped) == entry->value);
			free(flipped);
		}
	}

	spudnet_head_clear(&allocator, &head);
	SPUDFUZZ_CHECK(head.entries.count == 0 && head.entries.capacity == 0 && head.entries.data == NULL);
	SPUDFUZZ_CHECK(spudtest_allocator_clean(&tracker));
	ref_clear(&ref);
	free(ref.entries);
	return 0;
}
