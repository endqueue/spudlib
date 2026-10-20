# Maturity TODO

Written on 2026-10-20. Four pieces of work that bring the parts of spudlib
written before its current rules up to them, and give it what C APIs that
stay stable for decades have and it lacks. Each is meant for a session of
its own, in the order below. Items 1 and 2 are done (2026-10-20); 3 has two parts done (version,
result names) and 4 has not started. Item 3 was added the same day, from a comparison with Vulkan,
Win32, POSIX, SQLite, zlib, libcurl and Lua; it goes before SpudFiles so
that the new `spudfiles.h` is written to its rules.

What "done" means here is measured by the workspace's `check.sh`: when items
1, 2 and 4 are finished, `check.baseline` has no `mutable-globals|spudlib` or
`struct-bases|spudlib` entries left, and nothing under `bindings/`. Item 3
adds to `spudcore.h` and removes no entry.

**Pending from item 1:** `febed1c` is pushed, but the callers' dependency
locks still name `29a8b01`, so `./check.sh`'s `dep-locks` fails.
`./check.sh --update-locks` writes no lock while spudlib has uncommitted
changes: commit and push the SpudInput dispatch and item 2 first, then run it.

Every item ends the same way:

- build on macOS and run the tests, plain and under the `-asan` preset;
- build every caller the change breaks, in the same change, and run its tests;
- `./check.sh` from the workspace, with the entries this item removes taken out
  of `check.baseline`;
- after the spudlib commit is pushed, `./check.sh --update-locks`, and commit
  the locks with the repositories that use them;
- each changed repository's `.git/COMMIT_DRAFT` rewritten to describe its
  change.

## 1. Remove `bindings/` (done 2026-10-20)

**What:** delete `bindings/rust/` and `bindings/swift/`, and the "Rust
bindings" section of `CLAUDE.md` and the Rust and Swift bullets of
`README.md`.

**Why:**

- **Nothing calls them.** spudlib's callers include its C headers
  directly, and nothing in the workspace outside spudlib names either
  binding. The workspace's own rules (`../CLAUDE.md`) say which code may
  call spudlib, and none of it is Rust or Swift.
- **A binding names its caller's language.** spudlib never names or assumes
  its caller. Its C headers are its interface, and every language with C
  interop can use them as they are.
- **Each is a second build of spudlib.** `spudlib-sys`'s `build.rs` compiles
  five of the Vulkan backend's files with the `cc` crate, and the Swift package
  compiles symlinks to `spudcore.c` and two Metal files. Neither goes through
  `CMakeLists.txt`, so neither gets its backend selection, definitions or
  warnings. Each is a partial spudlib that nothing tests.
- **They have drifted.** `bindings.rs` declares the SpudGPU descs without
  `base`, so its layouts have been wrong since 2026-10-19. Neither binding
  has been changed since 2026-05-20.

If a caller in another language ever needs spudlib, what serves it is a
separate repository of its own: a `-sys` crate whose bindgen pass reads the
real headers at build time, over a spudlib built by CMake, or a Swift module
map over the headers. Neither lives here.

**Breaks:** nothing.

**Done:** `bindings/` deleted, with `CLAUDE.md`'s "Rust bindings" section,
`README.md`'s "Bindings" section and the Rust and Swift build folders in
`.gitignore`. `STRUCT_BASE_TODO.md`'s SpudGPU record still mentions the
bindings as they were then.

## Done outside the plan: SpudInput's dispatch (2026-10-20)

Not one of the items, but it changes the files item 2 touches. Any number of
native input APIs may now be in one build (GameInput and XInput on Windows,
once XInput is written), and the caller's `native_api` chooses: the front end
has one `switch` per backend verb on it, the backends are part of
`src/spudinput.c`'s translation unit (included there, never compiled alone),
and each verb is `spudinput_<api>_<verb>`, forced inline into its case with
`SPUDINPUT_BACKEND_FUNCTION`. `CLAUDE.md`'s SpudInput paragraph and
`src/input/spudinputshared.h` have the rule.

Built and tested on macOS only (Debug, Release, `-asan`, the Xcode
generator; every caller in the workspace against it). **Still to do:** build
`windows-d3d12` (the GameInput backend, and the `/w14714 /we4714` flags) and
`linux-vulkan` (evdev), and run `spudinput_test` on each.

## 2. Convert SpudInput to struct bases (done 2026-10-20)

Done as `STRUCT_BASE_TODO.md` classifies it; that file's "SpudInput,
converted" is the record of what changed and what was run. The nine
`spudinput.h` entries are out of `check.baseline`. **Still to do:** run
`spudinput_test` once with a gamepad connected, which is when
`spudinput_gamepad_get_properties`' three struct-base refusals run.

**Proposed alongside it, as its own change:** a query of whether a
native API built in will start on this machine (GameInput's runtime may be
missing), such as `spudinput_native_api_is_available(api)`, so the caller can
choose before `spudinput_instance_create` fails with
`SPUDRESULT_API_SPECIFIC_FAILURE`. Not agreed yet; propose it first.

## 3. What every module shares: version, result names, allocator, policy

Begin with a proposal for the `spudcore.h` additions and the policy text,
and write code only once it is agreed.

Long-lived C APIs have these and spudlib does not:

- **A version, at compile time and at run time: done (2026-10-20).**
  `ZLIB_VERNUM` and `zlibVersion()`, `SQLITE_VERSION_NUMBER` and
  `sqlite3_libversion_number()`, `LIBCURL_VERSION_NUM` and
  `curl_version_info()`, `VK_HEADER_VERSION` and
  `vkEnumerateInstanceVersion`. `spudcore.h` has `SPUDLIB_VERSION_MAJOR`/
  `_MINOR`/`_PATCH` (0.1.0), `SPUDLIB_VERSION`, Vulkan's packing
  (`SPUDLIB_MAKE_VERSION`, 10/10/12 bits, no variant) and
  `spud_get_version()`; `CMakeLists.txt` reads the version from the header.
  One version for the library, no version string, and no caller refused for
  its version. The header's "Version" group has what major, minor and patch
  promise from 1.0.0, which belongs with the stability policy below. Still
  open: tags (none yet), and a `check.sh` check that a `vX.Y.Z` tag matches
  the header, once there are tags.
- **A name for each result: done (2026-10-20).** Vulkan's `string_VkResult`
  (`vk_enum_string_helper.h`) and `curl_easy_strerror`. spudlib already had
  it as `spudresult_str()` (`spudcore.h`), which keeps its name; it was
  missing 11 results (SpudGPU's six heap and placement results, SpudFiles'
  1003-1007), which read as "SPUDRESULT_UNKNOWN". All 151 have a case now,
  and the switch has no `default`, so clang's `-Wswitch` (on by default)
  and GCC's (in `-Wall`) report a result added without one. MSVC's C4062
  is off by default, so a Windows-only change is not caught.
- **The caller's allocator.** `VkAllocationCallbacks`, `SQLITE_CONFIG_MALLOC`,
  `lua_Alloc`, zlib's `zalloc`/`zfree`. spudlib calls `malloc` itself, which
  is a decision made for the caller. An optional allocator on each module's
  instance desc fits the rule. Decide it before item 4 writes SpudFiles'
  desc. On a desc already based (SpudGPU, SpudAudio, SpudNet, and SpudInput
  after item 2) it is a chained struct, or, while no version has been
  released, a new field.
- **A stated stability and threading policy.** `CLAUDE.md` says a released
  layout never changes, but nothing says what "released" is: there are no
  tags or versions, and spudlib is STATIC only, pinned by commit in each
  caller's `deps.lock`, so in practice only source compatibility holds. Write
  down what is promised and from which version (as SQLite and Vulkan do), and
  per call what may be called from which thread at once (Vulkan's "externally
  synchronized"); today only about 15 places in the headers say.
- **Before any shared build of spudlib** (not needed while it is static
  only): an export macro, default-hidden symbols, a `SOVERSION`, and symbol
  versioning or a `.def` file.

**Breaks:** nothing, if the allocator is optional and chained or added with
the struct bases.

## 4. Rework SpudFiles

The largest of the four, and the one to design before writing: begin the
session with a proposal for the new `spudfiles.h`, and write code only once it
is agreed. The plan is in `SPUDFILES_TODO.md`: what is wrong now, what the
callers use, the proposal to start from, and what it breaks.
