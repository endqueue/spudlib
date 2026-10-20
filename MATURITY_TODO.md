# Maturity TODO

Written on 2026-10-20. Four pieces of work that bring the parts of spudlib
written before its current rules up to them, and give it what C APIs that
stay stable for decades have and it lacks. Each is meant for a session of
its own, in the order below. Item 1 is done (2026-10-20); 2, 3 and 4 have
not started. Item 3 was added the same day, from a comparison with Vulkan,
Win32, POSIX, SQLite, zlib, libcurl and Lua; it goes before SpudFiles so
that the new `spudfiles.h` is written to its rules.

What "done" means here is measured by the workspace's `check.sh`: when items
1, 2 and 4 are finished, `check.baseline` has no `mutable-globals|spudlib` or
`struct-bases|spudlib` entries left, and nothing under `bindings/`. Item 3
adds to `spudcore.h` and removes no entry.

**Pending from item 1:** `febed1c` is pushed, but trellis's and latran's
`deps.lock` still name `29a8b01`, so `./check.sh`'s `dep-locks` fails.
`./check.sh --update-locks` writes no lock while spudlib has uncommitted
changes: commit and push the SpudInput dispatch below first, then run it.

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
generator; latran and trellislib against it). **Still to do:** build
`windows-d3d12` (the GameInput backend, and the `/w14714 /we4714` flags) and
`linux-vulkan` (evdev), and run `spudinput_test` on each.

## 2. Convert SpudInput to struct bases

Step 3 of `STRUCT_BASE_TODO.md`, for SpudInput. That file's "spudinput.h"
section classifies every struct; this is how to apply it.

- `spudcore.h`: `SPUD_STRUCT_TYPE_INPUT_INSTANCE_DESC` = 4000 and
  `SPUD_STRUCT_TYPE_INPUT_GAMEPAD_PROPERTIES` = 4001, each documented with its
  struct and base kind, as the other types are.
- `spudinput.h`: `SPUDINPUT_INSTANCE_DESC` starts with a `SPUD_STRUCT_BASE`,
  `SPUDINPUT_GAMEPAD_PROPERTIES` with a `SPUD_STRUCT_BASE_OUT`; the base
  comment names the type, as SpudGPU's do. The other seven structs (state,
  motion sample, touch state, touch contact, touch surface, rumble, battery)
  get a `/* Frozen: <why> ... */` line. Each of the two functions documents
  the two results for a wrong base.
- **Checks go in the front end, `src/spudinput.c`, only.** `CLAUDE.md`
  says every argument check is there, and the backends only fill the front
  end's records, so no backend changes. (Since the dispatch above, the
  backends are in the same translation unit, but they still read and write
  only the records.)
  - `spudinput_instance_create`: `spud_struct_check` right after
    `if (!desc)`, before the `out_instance` check. `instance->desc = *desc`
    would keep the caller's `base.next`, which needn't outlive the call: keep
    the copy with `next` set to NULL.
  - `spudinput_gamepad_get_properties`: `spud_struct_out_check` right after
    the `!out_properties` check. `*out_properties = record->properties`
    overwrites the caller's base, so write it back after the copy with
    `spud_struct_base_out(SPUD_STRUCT_TYPE_INPUT_GAMEPAD_PROPERTIES)`, as
    SpudAudio's backends do (`next` is NULL, or the check refused it).
- `tests/spudinput_test.c`: set the bases, and add a part that refuses a
  zeroed base, another struct's type and a chained struct for both calls,
  as `spudnet_test`'s "Struct bases" does. With no gamepad connected,
  `spudinput_gamepad_get_properties` returns
  `SPUDRESULT_SPUDINPUT_GAMEPAD_NOT_CONNECTED` before it reaches the base
  (its checks are in parameter order), so its three refusals run only when a
  gamepad is connected: write them to run then and to say they were skipped
  otherwise, and run the test once with a gamepad.
- `check.baseline`: remove the nine `spudinput.h` entries.

**Breaks:** every caller that fills an instance desc or queries gamepad
properties. Find them with `grep -rn "SPUDINPUT_INSTANCE_DESC\|SPUDINPUT_GAMEPAD_PROPERTIES"`
over the workspace's other repositories, and change them in the same change.
As of 2026-10-20 that is latran's player alone
(`latran/player/src/latranplayer.c`: the desc at line 287, the properties at
425); trellis has none.

**Also worth adding here, or as its own change:** a query of whether a
native API built in will start on this machine (GameInput's runtime may be
missing), such as `spudinput_native_api_is_available(api)`, so the caller can
choose before `spudinput_instance_create` fails with
`SPUDRESULT_API_SPECIFIC_FAILURE`. Not agreed yet; propose it first.

## 3. What every module shares: version, result names, allocator, policy

Begin with a proposal for the `spudcore.h` additions and the policy text,
and write code only once it is agreed.

Long-lived C APIs have these and spudlib does not:

- **A version, at compile time and at run time.** `ZLIB_VERNUM` and
  `zlibVersion()`, `SQLITE_VERSION_NUMBER` and `sqlite3_libversion_number()`,
  `LIBCURL_VERSION_NUM` and `curl_version_info()`, `VK_HEADER_VERSION` and
  `vkEnumerateInstanceVersion`. spudlib has neither. It matters first for
  struct chains: `spud_struct_check` refuses any `next`, and once a chained
  struct exists a caller needs to ask which ones the library it runs on
  takes, as Vulkan's callers ask the API version and extensions before they
  chain. Add `SPUDLIB_VERSION_MAJOR`/`_MINOR`/`_PATCH` and
  `spud_get_version()` before the first chained struct.
- **A name for each result.** Vulkan's `string_VkResult`
  (`vk_enum_string_helper.h`) and `curl_easy_strerror`: `spud_result_name(SPUDRESULT)`, returning a static
  string (read-only data, which `mutable-globals` allows), so no caller keeps
  its own table that drifts.
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
is agreed.

SpudFiles was written before spudlib's current rules, and breaks several of
them. Converting its structs to struct bases as they are would polish an API
that has to change anyway, so the struct-base step is part of this rework, not
a step of its own.

**What is wrong now:**

- **Writable static data.** The path functions (`sfs_get_path_components`,
  `sfs_simplify_path`, `sfs_split_path`, `sfs_get_relative_path`,
  `sfs_file_dialog_multi_path`) and `sfs_get_working_directory`, the dialogs
  and `run_dialog_helper` return pointers into `static` buffers: not
  reentrant, not thread-safe, and each call overwrites the last answer. These
  are fifteen of spudlib's `mutable-globals` baseline entries.
  `sfs_set_working_directory` changes the whole process's state.
- **A default chosen for the caller.** `sfs_default_file_dialog_attributes`
  picks dialog flags on the caller's behalf.
- **GUI.** spudlib owns no window, and GUI belongs to the caller, but
  SpudFiles shows modal dialogs (`sfs_file_dialog`, `sfs_open_folder_dialog`);
  the macOS one runs `osascript` and ignores `SFS_FILE_DIALOG_FLAG_OVERWRITE_PROMPT`.
- **No platform API translated.** The path-string functions (`sfs_is_slash`,
  `sfs_correct_slashes`, `sfs_is_absolute`, `sfs_split_path`,
  `sfs_build_path_from_components`, `sfs_simplify_path`,
  `sfs_get_relative_path`) are string handling that decides what a path
  means, not a translation of any platform call.
- **Old shapes.** `sfs_file_open` and `sfs_file_dialog` take their structs by
  value; results come back as `bool` (`sfs_file_exists` can't tell "no file"
  from a failure) or `const char *` instead of `SPUDRESULT`. The header's
  `// FileOpenAttribs`-style comments name an older API. `SFS_FIND_FILE_DATA`
  is used by no function. `include/spudfiles.h:181` ends in a stray `\`
  that splices it onto the next declaration, and
  `sfs_get_working_directory()` has empty parentheses rather than `(void)`.
  `README.md` still says SpudFiles is Windows only. No test is committed for
  any of it.
- **Windows paths in the code page.** `sfs_file_open` calls `CreateFileA`
  (`src/files/backends/windows/spudfileswindows.c:67`), so a path outside the
  system code page fails there, while `sfs_file_replace` already converts to
  UTF-16 for `MoveFileExW`.
- **A share mode chosen for the caller.** The same call passes
  `FILE_SHARE_READ` always.

**What the callers use** (as of 2026-10-20, read from the workspace with
`grep -rnoE "\bsfs_[a-z_]+\("` over its other repositories): `sfs_file_open`,
`_read`, `_write`, `_set_pos`, `_get_size`, `_flush`, `_replace`, `_remove`,
`_map`, `_release` and `sfs_file_exists`. Nothing calls the path functions,
the working directory calls, `sfs_create_directory`, `sfs_directory_exists`
or the dialogs. Run the grep again before deciding what to delete.

**The proposal to start from:**

- Delete the dialogs, `sfs_default_file_dialog_attributes`,
  `SFS_FILE_DIALOG_ATTRIBUTES` and its flags, and `SFS_FIND_FILE_DATA`.
- Delete the path-string functions. A caller that needs them owns them.
- Delete `sfs_set_working_directory` and `sfs_get_working_directory`, or keep
  only a plain translation that writes into a caller's buffer.
- Every remaining call returns `SPUDRESULT`. A call that reports a string or
  a yes/no answer writes it into the caller's buffer or output parameter
  (`out_buf`, `capacity`, `out_size`, as SpudNet's getters do). A buffer too
  small gets a result of its own and the size it needs, as Vulkan's
  two-call idiom does with `VK_INCOMPLETE`.
- Paths are UTF-8, said once in the header; the Windows backend converts
  every one to UTF-16 and calls only the `W` functions.
- `sfs_file_exists` becomes a stat-like call (`stat`,
  `GetFileAttributesExW`): `SPUDRESULT_NOT_FOUND`-style for no file, else
  the kind, size and modification time.
- Consider reads and writes at an offset (`pread`/`pwrite`, `ReadFile` with
  an offset), which the platforms have natively and which share no cursor
  between threads, beside or instead of `_set_pos`/`_get_pos`.
- Candidates, only if a caller needs them: listing a directory (an iterator
  over `opendir`/`FindFirstFileW`) and the platform's known folders
  (`SHGetKnownFolderPath`, `NSSearchPathForDirectories`); both translate a
  platform call, unlike the path-string functions.
- `sfs_file_open` takes a `const SFS_FILE_OPEN_ATTRIBUTES *` that starts with
  a `SPUD_STRUCT_BASE` (`SPUD_STRUCT_TYPE_FILES_OPEN_DESC`, 5000; perhaps
  renamed to a `_desc`, as the other modules' are), refused with
  `SPUDRESULT_NULL_DESC` when NULL. It is the one struct likely to grow
  (exclusive create, share modes, access hints). Its share mode is the
  caller's, with no default.
- Its checks are written once, in a validate function in `src/spudfiles.c`
  that each backend calls first, as `spudaudio_validate_stream_desc` is for
  SpudAudio, and not three times in the backends. (SpudFiles has one backend
  per platform, so it needs none of SpudInput's dispatch.)
- A test, `tests/spudfiles_test.c`, registered with ctest: the argument
  checks and refusals, and open, write, read, map, flush, replace and remove
  in a temporary directory.
- `STRUCT_BASE_TODO.md`: its "spudfiles.h" section updated to what was done;
  then its step 6 (that file's classification rewritten as a record, and
  `CLAUDE.md`'s note shortened).
- `check.baseline`: remove the three `spudfiles.h` `struct-bases` entries
  and every SpudFiles `mutable-globals` entry.

**Breaks:** every caller of `sfs_file_open` and `sfs_file_exists` (the grep
above finds them). Change them in the same change. Windows and Linux
backends change too: build them (`windows-d3d12`, `linux-vulkan`) before
calling it done.
