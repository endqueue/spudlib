# SpudFiles TODO

Written on 2026-10-20, from item 4 of `MATURITY_TODO.md`, which now points
here. Not started.

SpudFiles was written before spudlib's current rules, and breaks several of
them. Converting its structs to struct bases as they are would polish an API
that has to change anyway, so the struct-base step is part of this rework, not
a step of its own.

Begin the session with a proposal for the new `spudfiles.h`, and write code
only once it is agreed.

**Before it:** `MATURITY_TODO.md`'s item 3 decides whether each module's
instance desc takes the caller's allocator, and how a version and result
names are given. Settle it first, so the new `spudfiles.h` is written to its
rules.

## What is wrong now

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

## What the callers use

As of 2026-10-20, read from the workspace with
`grep -rnoE "\bsfs_[a-z_]+\("` over its other repositories (three files):
`sfs_file_open`, `_read`, `_write`, `_set_pos`, `_get_size`, `_flush`,
`_replace`, `_remove`, `_map`, `_release` and `sfs_file_exists`.
Nothing calls the path functions, the working directory calls,
`sfs_create_directory`, `sfs_directory_exists` or the dialogs. Run the grep
again before deciding what to delete.

## The proposal to start from

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
- `README.md`: SpudFiles' platforms as they are.
- `CLAUDE.md`: the SpudFiles macOS-backend note under "Known gaps" (the
  `osascript` dialogs) removed with the dialogs, and SpudFiles taken out of
  "SpudGPU, SpudFiles and SpudMemory have no committed tests".
- `STRUCT_BASE_TODO.md`: its "spudfiles.h" section updated to what was done;
  then its step 6 (that file's classification rewritten as a record, and
  `CLAUDE.md`'s note shortened).
- `check.baseline`: remove the three `spudfiles.h` `struct-bases` entries
  and every SpudFiles `mutable-globals` entry.

## Breaks

Every caller of `sfs_file_open` and `sfs_file_exists` (the grep above finds
them).
Change them in the same change, and build and run each one's tests. Windows
and Linux backends change too: build them (`windows-d3d12`, `linux-vulkan`)
before calling it done.

## Done when

The steps every item of `MATURITY_TODO.md` ends with:

- build on macOS and run the tests, plain and under the `-asan` preset;
- build every caller the change breaks, in the same change, and run its tests;
- `./check.sh` from the workspace, with the entries above taken out of
  `check.baseline`;
- after the spudlib commit is pushed, `./check.sh --update-locks`, and commit
  the locks with the repositories that use them;
- each changed repository's `.git/COMMIT_DRAFT` rewritten to describe its
  change.
