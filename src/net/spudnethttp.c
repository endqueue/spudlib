
/*
 * SpudNet HTTP transfer - the part that is the same on every stack.
 *
 * spudnet.h gives a transfer an order of use, a set of arguments it
 * refuses, a count to keep of a sized request body, and a response's
 * status and headers to hold until it is destroyed. None of that depends
 * on which stack carries the request, so it is here once: every
 * spudnet_http_transfer_* in spudnet.h is defined in this file, and each
 * does its checking and then hands the stack the one thing left to do,
 * through the spudnet_http_backend_* functions in spudnetshared.h.
 *
 * A backend can therefore take for granted that it is only asked for what
 * spudnet.h allows next, and this file can take for granted that a
 * backend's result is the network's answer. The rule that joins the two:
 * any failure but SPUDRESULT_SPUDNET_TIMED_OUT ends the transfer, and after
 * that nothing reaches the backend but destroy.
 */

#include "spudnetshared.h"
#include "spudnet.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "../spudstructbase.h"

#if __cplusplus
extern "C" {
#endif

// --------------------------------------------------------------------------
// A response's status and headers
// --------------------------------------------------------------------------

void spudnet_head_clear(struct spudnet_http_head *head) {
	for (uint32_t i = 0; i < head->count; ++i) {
		free(head->entries[i].name);
		free(head->entries[i].value);
	}
	free(head->entries);
	memset(head, 0, sizeof(*head));
}

/* Drops the headers and keeps the room they were in. */
static void spudnet_head_drop_headers(struct spudnet_http_head *head) {
	for (uint32_t i = 0; i < head->count; ++i) {
		free(head->entries[i].name);
		free(head->entries[i].value);
	}
	head->count = 0;
}

static struct spudnet_http_header_entry *spudnet_head_find(const struct spudnet_http_head *head, const char *name, size_t name_length) {
	for (uint32_t i = 0; i < head->count; ++i) {
		if (strlen(head->entries[i].name) == name_length && spudnet_ascii_equal_no_case(head->entries[i].name, name, name_length))
			return &head->entries[i];
	}
	return NULL;
}

bool spudnet_head_add_header(
    struct spudnet_http_head *head,
    const char *name,
    size_t name_length,
    const char *value,
    size_t value_length) {
	// A name that is there already gets the new value joined onto its own.
	struct spudnet_http_header_entry *entry = spudnet_head_find(head, name, name_length);
	if (entry) {
		size_t old_length = strlen(entry->value);
		char  *joined     = (char *)realloc(entry->value, old_length + 2 + value_length + 1);
		if (!joined)
			return false;
		memcpy(joined + old_length, ", ", 2);
		memcpy(joined + old_length + 2, value, value_length);
		joined[old_length + 2 + value_length] = '\0';
		entry->value                          = joined;
		return true;
	}

	if (head->count == head->capacity) {
		if (head->capacity > SPUD_UINT32_MAX / 2)
			return false;
		uint32_t                          capacity = head->capacity ? head->capacity * 2 : 16;
		struct spudnet_http_header_entry *moved =
		    (struct spudnet_http_header_entry *)realloc(head->entries, (size_t)capacity * sizeof(struct spudnet_http_header_entry));
		if (!moved)
			return false;
		head->entries  = moved;
		head->capacity = capacity;
	}
	char *name_copy  = (char *)malloc(name_length + 1);
	char *value_copy = (char *)malloc(value_length + 1);
	if (!name_copy || !value_copy) {
		free(name_copy);
		free(value_copy);
		return false;
	}
	memcpy(name_copy, name, name_length);
	name_copy[name_length] = '\0';
	memcpy(value_copy, value, value_length);
	value_copy[value_length]         = '\0';
	head->entries[head->count].name  = name_copy;
	head->entries[head->count].value = value_copy;
	head->count++;
	return true;
}

bool spudnet_head_add_header_line(
    struct spudnet_http_head *head,
    const char *line,
    size_t length) {
	while (length > 0 && (line[length - 1] == '\r' || line[length - 1] == '\n'))
		length--;

	if (length >= 5 && spudnet_ascii_equal_no_case(line, "HTTP/", 5)) {
		spudnet_head_drop_headers(head);
		return true;
	}

	size_t colon = 0;
	while (colon < length && line[colon] != ':')
		colon++;
	if (colon == 0 || colon == length)
		return true; // blank line, or nothing a header looks like

	const char *value        = line + colon + 1;
	size_t      value_length = length - colon - 1;
	while (value_length > 0 && (*value == ' ' || *value == '\t')) {
		value++;
		value_length--;
	}
	while (value_length > 0 && (value[value_length - 1] == ' ' || value[value_length - 1] == '\t'))
		value_length--;

	return spudnet_head_add_header(head, line, colon, value, value_length);
}

const char *spudnet_head_get_header(const struct spudnet_http_head *head, const char *name) {
	struct spudnet_http_header_entry *entry = spudnet_head_find(head, name, strlen(name));
	return entry ? entry->value : NULL;
}

// --------------------------------------------------------------------------
// What a request's headers may and may not say
// --------------------------------------------------------------------------

/* True when a header's name is `name`, without regard to case. */
static bool spudnet_header_is(const spudnet_header *header, const char *name) {
	size_t length = strlen(name);
	return strlen(header->name) == length && spudnet_ascii_equal_no_case(header->name, name, length);
}

enum spudnet_accept_encoding spudnet_headers_accept_encoding(
    const spudnet_header *headers,
    uint32_t header_count) {
	static const char identity[] = "identity";

	enum spudnet_accept_encoding found = SPUDNET_ACCEPT_ENCODING_STACK;
	for (uint32_t i = 0; i < header_count; ++i) {
		if (!headers[i].name || !headers[i].value || !spudnet_header_is(&headers[i], "Accept-Encoding"))
			continue;

		// The value with the blanks around it left off.
		const char *value  = headers[i].value;
		size_t      length = strlen(value);
		while (length > 0 && (*value == ' ' || *value == '\t')) {
			value++;
			length--;
		}
		while (length > 0 && (value[length - 1] == ' ' || value[length - 1] == '\t'))
			length--;
		if (length != sizeof(identity) - 1 || !spudnet_ascii_equal_no_case(value, identity, length))
			return SPUDNET_ACCEPT_ENCODING_INVALID;
		found = SPUDNET_ACCEPT_ENCODING_IDENTITY;
	}
	return found;
}

/* Everything spudnet.h has spudnet_http_transfer_start refuse before
 * anything is sent. `*out_encoding` is what the headers say of the
 * response's encoding. */
static SPUDRESULT spudnet_http_desc_check(const spudnet_http_transfer_desc *desc, enum spudnet_accept_encoding *out_encoding) {
	*out_encoding = SPUDNET_ACCEPT_ENCODING_STACK;
	if (!desc->method || desc->method[0] == '\0' || !desc->url || (desc->header_count > 0 && !desc->headers))
		return SPUDRESULT_DESC_INVALID_PARAMETERS;

	for (uint32_t i = 0; i < desc->header_count; ++i) {
		if (!desc->headers[i].name || !desc->headers[i].value)
			return SPUDRESULT_DESC_INVALID_PARAMETERS;
		// These two follow from the desc's body and are written for it.
		if (spudnet_header_is(&desc->headers[i], "Content-Length") || spudnet_header_is(&desc->headers[i], "Transfer-Encoding"))
			return SPUDRESULT_DESC_INVALID_PARAMETERS;
	}
	*out_encoding = spudnet_headers_accept_encoding(desc->headers, desc->header_count);
	if (*out_encoding == SPUDNET_ACCEPT_ENCODING_INVALID)
		return SPUDRESULT_DESC_INVALID_PARAMETERS;

	switch (desc->body) {
	case SPUDNET_HTTP_BODY_SIZED:
		break;
	case SPUDNET_HTTP_BODY_NONE:
	case SPUDNET_HTTP_BODY_UNSIZED:
		// A size that would be ignored is refused instead.
		if (desc->body_size != 0)
			return SPUDRESULT_DESC_INVALID_PARAMETERS;
		break;
	default:
		return SPUDRESULT_DESC_INVALID_PARAMETERS;
	}
	// A redirect may want the body sent again, and it has passed through.
	if (desc->follow_redirects && desc->body != SPUDNET_HTTP_BODY_NONE)
		return SPUDRESULT_DESC_INVALID_PARAMETERS;

	if (!spudnet_url_has_scheme(desc->url, "http") && !spudnet_url_has_scheme(desc->url, "https"))
		return SPUDRESULT_SPUDNET_INVALID_URL;
	return SPUD_SUCCESS;
}

// --------------------------------------------------------------------------
// The transfer
// --------------------------------------------------------------------------

/* What a backend returned, after the transfer has been marked for it:
 * anything but success and a time-out that may be tried again leaves only
 * destroy. */
static SPUDRESULT spudnet_http_settle(struct spudnet_http_transfer_t *transfer, SPUDRESULT result, bool timeout_ends_it) {
	if (SPUDFAIL(result) && (result != SPUDRESULT_SPUDNET_TIMED_OUT || timeout_ends_it))
		transfer->phase = SPUDNET_HTTP_PHASE_FAILED;
	return result;
}

SPUDRESULT spudnet_http_transfer_create(
    spudnet_http_client client,
    spudnet_http_transfer *out_transfer) {
	if (!out_transfer)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	*out_transfer = NULL;
	if (!client)
		return SPUDRESULT_SPUDNET_INVALID_HTTP_CLIENT;

	struct spudnet_http_transfer_t *transfer = (struct spudnet_http_transfer_t *)calloc(1, sizeof(struct spudnet_http_transfer_t));
	if (!transfer)
		return SPUDRESULT_OUT_OF_MEMORY;
	transfer->client  = client;
	transfer->phase   = SPUDNET_HTTP_PHASE_CREATED;
	SPUDRESULT result = spudnet_http_backend_create(transfer);
	if (SPUDFAIL(result)) {
		free(transfer);
		return result;
	}

	*out_transfer = transfer;
	return SPUD_SUCCESS;
}

void spudnet_http_transfer_destroy(spudnet_http_transfer transfer) {
	if (!transfer)
		return;
	spudnet_http_backend_destroy(transfer);
	spudnet_head_clear(&transfer->head);
	free((void *)transfer->debug_name);
	free(transfer);
}

void spudnet_http_transfer_abort(spudnet_http_transfer transfer) {
	if (!transfer)
		return;
	spudnet_http_backend_abort(transfer);
}

void spudnet_http_transfer_get_error(
    spudnet_http_transfer transfer,
    spudnet_error *out_error) {
	spudnet_error_get(transfer ? &transfer->error : NULL, out_error);
}

SPUDRESULT spudnet_http_transfer_start(
    spudnet_http_transfer transfer,
    const spudnet_http_transfer_desc *desc) {
	if (!transfer)
		return SPUDRESULT_SPUDNET_INVALID_HTTP_TRANSFER;
	if (!desc)
		return SPUDRESULT_NULL_DESC;
	const SPUDRESULT base_sr = spud_struct_check(&desc->base, SPUD_STRUCT_TYPE_NET_HTTP_TRANSFER_DESC);
	if (base_sr != SPUD_SUCCESS)
		return base_sr;
	if (spudnet_http_backend_aborted(transfer))
		return SPUDRESULT_SPUDNET_ABORTED;
	if (transfer->phase != SPUDNET_HTTP_PHASE_CREATED)
		return SPUDRESULT_SPUDNET_INVALID_HTTP_TRANSFER;

	enum spudnet_accept_encoding encoding = SPUDNET_ACCEPT_ENCODING_STACK;
	SPUDRESULT                   result   = spudnet_http_desc_check(desc, &encoding);
	if (SPUDFAIL(result))
		return result;
	// A request can't be started without waiting on the other end; with no
	// waiting allowed nothing is done, and the transfer is still unstarted.
	if (desc->timeout_ms == SPUDNET_NO_WAIT)
		return SPUDRESULT_SPUDNET_TIMED_OUT;

	transfer->body             = desc->body;
	transfer->body_size        = desc->body_size;
	transfer->body_sent        = 0;
	transfer->is_head          = strcmp(desc->method, "HEAD") == 0;
	transfer->follow_redirects = desc->follow_redirects;
	transfer->phase            = SPUDNET_HTTP_PHASE_SENDING;
	// Out of time here means a request that may be part sent, which can't
	// be picked up again.
	return spudnet_http_settle(transfer, spudnet_http_backend_start(transfer, desc, encoding), true);
}

SPUDRESULT spudnet_http_transfer_send(
    spudnet_http_transfer transfer,
    const void *data,
    uint64_t size,
    uint32_t timeout_ms,
    uint64_t *out_sent) {
	if (!out_sent)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	*out_sent = 0;
	if (!transfer)
		return SPUDRESULT_SPUDNET_INVALID_HTTP_TRANSFER;
	if (spudnet_http_backend_aborted(transfer))
		return SPUDRESULT_SPUDNET_ABORTED;
	if (transfer->phase != SPUDNET_HTTP_PHASE_SENDING || transfer->body == SPUDNET_HTTP_BODY_NONE)
		return SPUDRESULT_SPUDNET_INVALID_HTTP_TRANSFER;

	// A sized body takes what it announced and not a byte more.
	uint64_t room = SPUD_UINT64_MAX;
	if (transfer->body == SPUDNET_HTTP_BODY_SIZED) {
		room = transfer->body_size - transfer->body_sent;
		if (room == 0)
			return SPUDRESULT_SPUDNET_INVALID_HTTP_TRANSFER;
	}
	if (!data || size == 0)
		return SPUDRESULT_ZERO_SIZE;
	if (size > room)
		size = room;
	if (size > SIZE_MAX)
		size = SIZE_MAX;

	size_t     sent   = 0;
	SPUDRESULT result = spudnet_http_settle(transfer, spudnet_http_backend_send(transfer, data, (size_t)size, timeout_ms, &sent), false);
	if (SPUDFAIL(result))
		return result;
	transfer->body_sent += sent;
	*out_sent            = sent;
	return SPUD_SUCCESS;
}

SPUDRESULT spudnet_http_transfer_receive_response(
    spudnet_http_transfer transfer,
    uint32_t timeout_ms) {
	if (!transfer)
		return SPUDRESULT_SPUDNET_INVALID_HTTP_TRANSFER;
	if (spudnet_http_backend_aborted(transfer))
		return SPUDRESULT_SPUDNET_ABORTED;
	if (transfer->phase != SPUDNET_HTTP_PHASE_SENDING && transfer->phase != SPUDNET_HTTP_PHASE_AWAITING)
		return SPUDRESULT_SPUDNET_INVALID_HTTP_TRANSFER;
	// A request that stops short of the length it announced can't be
	// finished. It isn't ended either: the rest can still be sent.
	if (transfer->phase == SPUDNET_HTTP_PHASE_SENDING && transfer->body == SPUDNET_HTTP_BODY_SIZED && transfer->body_sent < transfer->body_size)
		return SPUDRESULT_SPUDNET_INVALID_HTTP_TRANSFER;

	transfer->phase   = SPUDNET_HTTP_PHASE_AWAITING;
	SPUDRESULT result = spudnet_http_settle(transfer, spudnet_http_backend_receive_response(transfer, timeout_ms), false);
	if (!SPUDFAIL(result))
		transfer->phase = SPUDNET_HTTP_PHASE_RESPONDED;
	return result;
}

uint32_t spudnet_http_transfer_get_status(spudnet_http_transfer transfer) {
	return transfer ? transfer->head.status : 0;
}

const char *spudnet_http_transfer_get_header(
    spudnet_http_transfer transfer,
    const char *name) {
	if (!transfer || !name)
		return NULL;
	return spudnet_head_get_header(&transfer->head, name);
}

uint32_t spudnet_http_transfer_get_header_count(spudnet_http_transfer transfer) {
	return transfer ? transfer->head.count : 0;
}

SPUDRESULT spudnet_http_transfer_get_header_at(
    spudnet_http_transfer transfer,
    uint32_t index,
    const char **out_name,
    const char **out_value) {
	if (!out_name || !out_value)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	*out_name  = NULL;
	*out_value = NULL;
	if (!transfer || index >= transfer->head.count)
		return SPUDRESULT_INDEX_OUT_OF_RANGE;
	*out_name  = transfer->head.entries[index].name;
	*out_value = transfer->head.entries[index].value;
	return SPUD_SUCCESS;
}

SPUDRESULT spudnet_http_transfer_recv(
    spudnet_http_transfer transfer,
    void *data,
    uint64_t size,
    uint32_t timeout_ms,
    uint64_t *out_received) {
	if (!out_received)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	*out_received = 0;
	if (!transfer)
		return SPUDRESULT_SPUDNET_INVALID_HTTP_TRANSFER;
	if (spudnet_http_backend_aborted(transfer))
		return SPUDRESULT_SPUDNET_ABORTED;
	// Past the body's end every call says so again.
	if (transfer->phase == SPUDNET_HTTP_PHASE_ENDED)
		return SPUD_SUCCESS;
	if (transfer->phase != SPUDNET_HTTP_PHASE_RESPONDED)
		return SPUDRESULT_SPUDNET_INVALID_HTTP_TRANSFER;
	if (!data || size == 0)
		return SPUDRESULT_ZERO_SIZE;
	if (size > SIZE_MAX)
		size = SIZE_MAX;

	size_t     received = 0;
	SPUDRESULT result   = spudnet_http_settle(transfer, spudnet_http_backend_recv(transfer, data, (size_t)size, timeout_ms, &received), false);
	if (SPUDFAIL(result))
		return result;
	if (received == 0)
		transfer->phase = SPUDNET_HTTP_PHASE_ENDED;
	*out_received = received;
	return SPUD_SUCCESS;
}

#if __cplusplus
}
#endif
