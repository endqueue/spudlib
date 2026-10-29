#pragma once

/* The WinHTTP backend's reading of a response's header block, on its own so
 * that tests/fuzz can drive it from bytes. */

#include "../../spudnetshared.h"

#include <stddef.h>

/* The raw block WinHTTP gives for WINHTTP_QUERY_RAW_HEADERS_CRLF, `wide_count`
 * UTF-16 code units of it, into `head`: made UTF-8 and handed to
 * spudnet_head_add_header_line a line at a time, in the order sent, each
 * without its '\n'. What follows the last '\n' is not a line and is left
 * out. Everything is made and freed through `allocator`. Fails with
 * SPUDRESULT_OUT_OF_MEMORY, or SPUDRESULT_SPUDNET_HTTP_FAILED for a block of
 * more than INT_MAX code units. */
SPUDRESULT spudnet_win_add_raw_headers(const SPUD_ALLOCATOR *allocator, const wchar_t *raw, size_t wide_count, struct spudnet_http_head *head);
