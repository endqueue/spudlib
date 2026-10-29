# SpudNet fuzz targets

libFuzzer targets over SpudNet's own parsers of bytes it doesn't control.
HTTP and WebSocket framing themselves (status lines, chunked bodies,
WebSocket frame headers, masking, fragmentation, close frames) are parsed by
the platform's stack - NSURLSession, libcurl, WinHTTP - not by SpudLib, so
what SpudLib parses, and what is fuzzed here, is what the stacks hand on:

| Target | Function (file) | Bytes from |
|---|---|---|
| `spudnet_fuzz_http_head` | `spudnet_head_add_header_line`, `spudnet_head_add_header`, `spudnet_head_get_header` (`src/net/spudnethttp.c`) | the server: a response's header block, a line at a time (libcurl, WinHTTP) or as pairs (NSURLSession) |
| `spudnet_fuzz_certificate` | `spudnet_certificate_public_key_info` (`src/spudnet.c`) | the server: its certificate's DER, walked to the key a pin is checked against (WinHTTP, NSURLSession) |
| `spudnet_fuzz_text` | `spudnet_utf8_valid`, `spudnet_error_record`, `spudnet_ascii_equal_no_case`, `spudnet_url_has_scheme`, `spudnet_headers_accept_encoding` | the caller or the stack: text checked before a WebSocket send, a stack's message cut to fit a record |
| `spudnet_fuzz_proxy` | `spudnet_proxy_parse` (`src/spudnet.c`) | the caller: a proxy URL |

Each checks, besides AddressSanitizer's and UndefinedBehaviorSanitizer's
own findings, that nothing leaks or is freed wrongly through the caller's
allocator (with allocations refused part way, for the out-of-memory paths),
and that the answer matches a second, plainer reading of the same bytes. The
input layout of each is at the top of its file.

## Running

Clang with libFuzzer is needed: Apple's clang has none, Homebrew's LLVM
(`brew install llvm`) has, and so does clang on Linux. The presets build the
whole library with `-fsanitize=fuzzer-no-link,address,undefined`, Debug only,
in a binary dir of their own:

```sh
cmake --preset macos-fuzz            # or linux-fuzz
cmake --build --preset macos-fuzz-debug
mkdir -p /tmp/fuzz-head && cp tests/fuzz/corpus/spudnet_fuzz_http_head/* /tmp/fuzz-head/
build-macos-fuzz/Debug/spudnet_fuzz_http_head -max_total_time=1200 -max_len=4096 /tmp/fuzz-head
```

Fuzz into a scratch copy of the corpus, not `corpus/` itself, which holds
only a few small, valid seeds. On macOS, LeakSanitizer reports a leak in
libFuzzer's own RSS thread when a run ends; pass
`LSAN_OPTIONS=suppressions=<file>` with `leak:fuzzer::StartRssThread` in it.

## When it finds something

1. Minimize the input: `<target> -minimize_crash=1 -runs=100000 crash-<hash>`.
2. Fix the cause in `src/`.
3. Keep the minimized input as `regressions/<target>/<what-it-was>`.

Every build with `SPUDLIB_BUILD_TESTS` (the default) builds each target a
second time with `spudfuzz_replay.c`'s plain `main` and registers it with
ctest as `<target>_replay`, fed every file in `corpus/<target>` and
`regressions/<target>`. That runs on every platform and compiler, libFuzzer
or not.
