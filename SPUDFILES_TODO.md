# SpudFiles TODO

Written on 2026-10-20, from item 4 of `MATURITY_TODO.md`, which now points
here. **Done on 2026-10-22:** the proposal below was agreed, with the four
decisions as recommended, and built. What follows "The proposal to start
from" is the record of what was built and what is left.

SpudFiles was written before spudlib's current rules, and breaks several of
them. Converting its structs to struct bases as they are would polish an API
that has to change anyway, so the struct-base step is part of this rework, not
a step of its own.

Begin the session with a proposal for the new `spudfiles.h`, and write code
only once it is agreed.

**Before it:** `MATURITY_TODO.md`'s item 3 is settled for what this needs:
the new instance desc takes a `SPUD_ALLOCATOR`, with no default, and every
result has a name from `spudresult_str`. The new `spudfiles.h` is written to
those rules.

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
  `sfs_file_read` returns `SPUD_SUCCESS` at the end of the file with fewer
  bytes read than asked and no count, so a short read can't be seen.
- **Windows paths in the code page.** `sfs_file_open` calls `CreateFileA`
  (`src/files/backends/windows/spudfileswindows.c:70`), so a path outside the
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
  in a temporary directory. Written 2026-10-22 against today's API (passing
  on macOS); the rework moves it to the new calls.
- `CLAUDE.md`: the SpudFiles macOS-backend note under "Known gaps" (the
  `osascript` dialogs) removed with the dialogs.
- `STRUCT_BASE_TODO.md`: its "spudfiles.h" section updated to what was done;
  then its step 6 (that file's classification rewritten as a record, and
  `CLAUDE.md`'s note shortened).
- `check.baseline`: remove the three `spudfiles.h` `struct-bases` entries
  and every SpudFiles `mutable-globals` entry.

## The design, as agreed and built (2026-10-22)

Written against the callers of the day (re-grepped 2026-10-22): one that
reads and writes a document's archive at offsets and replaces it by
rename, one that reads a SPIR-V file whole, and one demo; two folders
outside `projects.conf` call the old API too and are left as they are.
This is `include/spudfiles.h` as built, in short:

```c
/* ---- Instance: the caller's allocator, as every other module. ---- */
typedef struct sfs_instance_t *sfs_instance;
typedef struct sfs_instance_desc {
	SPUD_STRUCT_BASE base;            /* SPUD_STRUCT_TYPE_FILES_INSTANCE_DESC */
	const SPUD_ALLOCATOR *allocator;  /* required; copied */
} sfs_instance_desc;
SPUDRESULT sfs_instance_create(const sfs_instance_desc *desc, sfs_instance *out_instance);
void sfs_instance_destroy(sfs_instance instance);   /* every file and mapping first */

/* ---- Opening. Every path is UTF-8; Windows converts to UTF-16 and calls
 * only W functions. ---- */
typedef enum SFS_ACCESS SPUD_ENUM_BASE {
	SFS_ACCESS_NONE = 0,               /* refused: a zeroed desc hasn't chosen */
	SFS_ACCESS_READ = 1, SFS_ACCESS_WRITE = 2, SFS_ACCESS_READ_WRITE = 3,
} SFS_ACCESS;
typedef enum SFS_CREATE SPUD_ENUM_BASE {   /* CreateFileW's dispositions / O_CREAT, O_EXCL, O_TRUNC */
	SFS_CREATE_NONE = 0,               /* refused */
	SFS_CREATE_OPEN_EXISTING = 1,      /* SPUDRESULT_SFS_NOT_FOUND if there's none */
	SFS_CREATE_NEW = 2,                /* SPUDRESULT_SFS_ALREADY_EXISTS if there's one */
	SFS_CREATE_OPEN_OR_CREATE = 3,
	SFS_CREATE_TRUNCATE = 4,           /* create, or empty the one there */
} SFS_CREATE;
typedef uint32_t SFS_SHARE;            /* what others may do while it's open */
enum { SFS_SHARE_NONE = 0, SFS_SHARE_READ = 1, SFS_SHARE_WRITE = 2, SFS_SHARE_DELETE = 4 };
/* Windows enforces it (FILE_SHARE_*); POSIX has no share modes and enforces
 * nothing. The rule on every backend: a caller may be refused what another
 * holder's share mode forbids, and must not rely on being refused. */

typedef struct sfs_open_desc {
	SPUD_STRUCT_BASE base;             /* SPUD_STRUCT_TYPE_FILES_OPEN_DESC (5000) */
	const char *path;
	SFS_ACCESS access;
	SFS_CREATE create;
	SFS_SHARE share;
	const char *debug_name;            /* copied, or NULL */
} sfs_open_desc;
SPUDRESULT sfs_file_open(sfs_instance instance, const sfs_open_desc *desc, sfs_file *out_file);
void sfs_file_close(sfs_file file);    /* NULL does nothing */

/* ---- I/O at an offset: pread/pwrite, ReadFile/WriteFile with an
 * OVERLAPPED offset. No cursor, so two threads can read one file. ---- */
SPUDRESULT sfs_file_read_at(sfs_file file, uint64_t offset, void *buffer, uint64_t size,
                            uint64_t *out_read);   /* fewer than asked only at the end */
SPUDRESULT sfs_file_write_at(sfs_file file, uint64_t offset, const void *data, uint64_t size);
SPUDRESULT sfs_file_get_size(sfs_file file, uint64_t *out_size);
SPUDRESULT sfs_file_set_size(sfs_file file, uint64_t size);   /* ftruncate / SetFileInformationByHandle */
SPUDRESULT sfs_file_flush(sfs_file file, SFS_FLUSH_LEVEL level);   /* as today */

/* ---- Mapping: as today, released with a void call. ---- */
SPUDRESULT sfs_file_map(sfs_file file, uint64_t offset, uint64_t size, sfs_mapping *out_mapping);
const void *sfs_mapping_get_data(sfs_mapping mapping);
uint64_t sfs_mapping_get_size(sfs_mapping mapping);
void sfs_mapping_release(sfs_mapping mapping);

/* ---- Paths: one call each, translated. ---- */
typedef enum SFS_PATH_KIND SPUD_ENUM_BASE {
	SFS_PATH_KIND_FILE = 1, SFS_PATH_KIND_DIRECTORY = 2, SFS_PATH_KIND_OTHER = 3,
} SFS_PATH_KIND;
typedef struct SFS_PATH_INFO {
	SPUD_STRUCT_BASE_OUT base;         /* SPUD_STRUCT_TYPE_FILES_PATH_INFO */
	SFS_PATH_KIND kind;
	uint64_t size;                     /* bytes; 0 for a directory */
	int64_t modified_ns;               /* since the Unix epoch */
} SFS_PATH_INFO;
SPUDRESULT sfs_path_get_info(sfs_instance instance, const char *path, SFS_PATH_INFO *out_info);
                                       /* SPUDRESULT_SFS_NOT_FOUND: nothing there */
SPUDRESULT sfs_path_replace(sfs_instance instance, const char *source, const char *target,
                            SFS_FLUSH_LEVEL level);   /* as today's sfs_file_replace */
SPUDRESULT sfs_path_remove(sfs_instance instance, const char *path);   /* a file; as today */
SPUDRESULT sfs_directory_create(sfs_instance instance, const char *path);
                                       /* one level: SPUDRESULT_SFS_NOT_FOUND without its parent,
                                        * SPUDRESULT_SFS_ALREADY_EXISTS if it's there */
```

New results, in SpudFiles' block: `SPUDRESULT_SFS_INVALID_INSTANCE`,
`SPUDRESULT_SFS_NOT_FOUND`, `SPUDRESULT_SFS_ALREADY_EXISTS`,
`SPUDRESULT_SFS_ACCESS_DENIED`, `SPUDRESULT_SFS_NOT_A_FILE`. Today's
`SPUDRESULT_GENERAL_FAILURE` for a failed `open`/`read`/`write` becomes the
specific one, or `SPUDRESULT_API_SPECIFIC_FAILURE`, with the platform's
number kept for a `_get_error` as SpudNet does.

Gone: the dialogs and their attributes and flags, `SFS_FIND_FILE_DATA`, the
path-string functions, the working directory calls, the six access modes
(`SFS_EFILE_ACCESS_MODE_*`; append was used by no caller), the cursor
(`_get_pos`/`_set_pos`) and `sfs_file_exists`/`sfs_directory_exists` (both
`sfs_path_get_info`).

Every caller in the workspace moved in the same change: each makes a
SpudFiles instance with the system heap for the length of its call, reads
at an offset where it seeked and read, and a save's temporary file is
created with `SFS_CREATE_NEW` (the old OVERWRITE could clobber a stray
file of the same name). One caller's loader closed no file it opened; it
does now.

The four decisions, as answered on 2026-10-22: offsets only, no cursor;
one directory level per `sfs_directory_create`; the working directory
calls deleted; the share mode a required field of the open desc.

Beyond the proposal: `SPUDRESULT_SFS_NOT_WRITABLE` (1013), the twin of
`_NOT_READABLE`, for a write, a size or a flush on a file opened only to
read; `SPUDRESULT_SFS_INVALID_MAPPING` (1003) is retired, since releasing a
mapping can't fail. Files and directories get the platform's default
permissions with the umask (0666 and 0777 before it), where the old code
chose 0644 and 0755.

Run on macOS (2026-10-22): `tests/spudfiles_test` (every call and check, a
non-ASCII name, two threads reading one file, a counting allocator) and
spudlib's other seven ctests, plain and under ASan and TSan; the callers'
tests over it. Run on Windows 11 (2026-10-22): built with MSVC under the
`windows-d3d12` preset (Debug) with no warnings, `spudfiles_test` passing
every check (the non-ASCII name and the two threads included) and
spudlib's other seven ctests with it. The POSIX backend's Linux build
hasn't been compiled.

Left:

- The platform's own error number for a failure (as SpudNet's `_get_error`
  keeps it): not built. Every failure is one of the results, and the rest
  `SPUDRESULT_API_SPECIFIC_FAILURE`.
- Directory listing and the platform's known folders: only when a caller
  needs them.

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
