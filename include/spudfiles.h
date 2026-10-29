#ifndef SPUDLIB_SPUDFILES_H
#define SPUDLIB_SPUDFILES_H

/**
 * @file spudfiles.h
 * @brief SpudFiles: files and paths, as the platform's file system has them.
 *
 * Opening, reading and writing at an offset, sizing, flushing and mapping a
 * file; what is at a path, replacing and removing a file, and making a
 * directory. One call each, translated: POSIX on Linux and every Apple
 * system, the Win32 W functions on Windows.
 *
 * Rules that hold on every backend:
 *
 * - Every path is UTF-8, null-terminated, as the platform takes it
 *   (relative to the process's working directory, or absolute). The Windows
 *   backend converts each to UTF-16. Nothing is normalised, joined or split:
 *   a path is the caller's.
 * - A NULL or empty path is refused with SPUDRESULT_SPUDFILE_NULL_PATH.
 * - There is no cursor: every read and write names its offset, so one
 *   call on a file leaves nothing behind that the next depends on.
 * - New files and directories get the platform's default permissions, with
 *   the process's umask applied on POSIX (0666 and 0777 before it).
 * - What a failure was is one of the results below; the platform's own
 *   number is not kept (not yet).
 *
 * Every object of an instance is made through the instance's allocator.
 *
 * @par Threads
 * SpudFiles is the library rule's broadest exception (@ref spud_threads
 * "Threads", spudcore.h): it keeps no state in a file, a mapping or an
 * instance that a call changes, so calls on one of them may overlap, each
 * marked `Thread:`. What stays the caller's: nothing is used while it is
 * closed, released or destroyed, or while its debug name is set; and
 * writes to one range at once, or a write to a range another call is
 * reading, are the caller's to order. The path calls take no object.
 */

#include "spudcore.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/** Opaque handle to a SpudFiles instance: the allocator its objects use. */
typedef struct spudfile_instance_t *spudfile_instance;
/** Opaque handle to an open file. */
typedef struct spudfile_file_t *spudfile_file;
/** Opaque handle to a read-only mapping of part of a file. */
typedef struct spudfile_mapping_t *spudfile_mapping;

/* ---- Instance ---- */

/**
 * @brief What the caller decides when creating an instance. No field has a
 * default.
 */
typedef struct spudfile_instance_desc {
	/** `type` is SPUD_STRUCT_TYPE_FILES_INSTANCE_DESC; `next` is NULL (nothing
	 * is chained onto this struct yet). Set by the caller. See
	 * SPUD_STRUCT_BASE. */
	SPUD_STRUCT_BASE base;
	/** Where the memory of the instance and of every file and mapping made
	 * from it comes from. Copied; its functions and `user_data` must outlive
	 * the instance and everything made from it. Required. */
	const SPUD_ALLOCATOR *allocator;
} spudfile_instance_desc;

/**
 * @brief Creates an instance.
 *
 * @param[in]  desc         What the caller decides.
 * @param[out] out_instance Receives the instance; NULL on failure.
 *
 * @retval SPUD_SUCCESS The instance was made.
 * @retval SPUDRESULT_NULL_DESC `desc` is NULL.
 * @retval SPUDRESULT_STRUCT_WRONG_TYPE `desc->base.type` is not
 *         SPUD_STRUCT_TYPE_FILES_INSTANCE_DESC.
 * @retval SPUDRESULT_STRUCT_UNKNOWN_IN_CHAIN `desc->base.next` is not NULL.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_instance` is NULL.
 * @retval SPUDRESULT_NULL_ALLOCATOR `desc->allocator` is NULL.
 * @retval SPUDRESULT_INVALID_ALLOCATOR `desc->allocator` lacks a function.
 * @retval SPUDRESULT_OUT_OF_MEMORY The instance could not be allocated.
 */
SPUDRESULT spudfile_instance_create(
    const spudfile_instance_desc *desc,
    spudfile_instance *out_instance);

/**
 * @brief Destroys an instance. Every file and mapping made from it must be
 * closed and released first.
 *
 * @param[in] instance The instance to destroy. NULL does nothing.
 */
void spudfile_instance_destroy(spudfile_instance instance);

/* ---- Opening ---- */

/** @brief What a file is opened for. */
typedef enum SPUDFILE_ACCESS {
	/** Not a choice: refused, so a zeroed desc hasn't chosen. */
	SPUDFILE_ACCESS_NONE       = 0,
	SPUDFILE_ACCESS_READ       = 1, /**< Read and map only. */
	SPUDFILE_ACCESS_WRITE      = 2, /**< Write and size only. */
	SPUDFILE_ACCESS_READ_WRITE = 3, /**< Both. */
} SPUDFILE_ACCESS;

/** @brief What opening does with the file there, or with no file there:
 * CreateFileW's dispositions, and POSIX's O_CREAT, O_EXCL and O_TRUNC. */
typedef enum SPUDFILE_CREATE {
	/** Not a choice: refused. */
	SPUDFILE_CREATE_NONE           = 0,
	/** Opens the file there; SPUDRESULT_SPUDFILE_NOT_FOUND if there is none. */
	SPUDFILE_CREATE_OPEN_EXISTING  = 1,
	/** Creates a new, empty file; SPUDRESULT_SPUDFILE_ALREADY_EXISTS if anything
	 * is there already. One caller of two racing for the name wins. */
	SPUDFILE_CREATE_NEW            = 2,
	/** Opens the file there, or creates an empty one. */
	SPUDFILE_CREATE_OPEN_OR_CREATE = 3,
	/** Creates an empty file, or empties the one there. Needs write access. */
	SPUDFILE_CREATE_TRUNCATE       = 4,
} SPUDFILE_CREATE;

/**
 * @brief What others may do with a file while this handle has it open, as
 * a set of bits.
 *
 * Windows enforces it (FILE_SHARE_*): an open, a rename over the file or a
 * remove that it forbids is refused with SPUDRESULT_SPUDFILE_IN_USE, this
 * process's and others'. POSIX has no share modes and enforces nothing. So
 * the rule on every backend: a caller may be refused what another holder's
 * share mode forbids, and must not rely on being refused.
 */
typedef uint32_t SPUDFILE_SHARE;
/** @brief Bits of SPUDFILE_SHARE. */
enum {
	SPUDFILE_SHARE_NONE   = 0x0, /**< Nobody else may open it. */
	SPUDFILE_SHARE_READ   = 0x1, /**< Others may open it for reading. */
	SPUDFILE_SHARE_WRITE  = 0x2, /**< Others may open it for writing. */
	SPUDFILE_SHARE_DELETE = 0x4, /**< Others may remove it, or rename over it. */
};

/** @brief What the caller decides when opening a file. No field has a
 * default. */
typedef struct spudfile_open_desc {
	/** `type` is SPUD_STRUCT_TYPE_FILES_OPEN_DESC; `next` is NULL (nothing is
	 * chained onto this struct yet). Set by the caller. See
	 * SPUD_STRUCT_BASE. */
	SPUD_STRUCT_BASE base;
	/** The file. A directory, or anything but a file, is refused with
	 * SPUDRESULT_SPUDFILE_NOT_A_FILE. */
	const char *path;
	SPUDFILE_ACCESS access; /**< What it is opened for. */
	SPUDFILE_CREATE create; /**< What happens with the file there, or with none. */
	SPUDFILE_SHARE share;   /**< What others may do while it is open. */
	/** Name for debugging, or NULL. Copied; see spud_debug_name_set(). */
	const char *debug_name;
} spudfile_open_desc;

/**
 * @brief Opens a file.
 *
 * Thread: may overlap any call on the same instance but
 * spudfile_instance_destroy().
 *
 * @param[in]  instance The instance the file is made with.
 * @param[in]  desc     What the caller decides.
 * @param[out] out_file Receives the file; NULL on failure.
 *
 * @retval SPUD_SUCCESS The file is open.
 * @retval SPUDRESULT_SPUDFILE_INVALID_INSTANCE `instance` is NULL.
 * @retval SPUDRESULT_NULL_DESC `desc` is NULL.
 * @retval SPUDRESULT_STRUCT_WRONG_TYPE `desc->base.type` is not
 *         SPUD_STRUCT_TYPE_FILES_OPEN_DESC.
 * @retval SPUDRESULT_STRUCT_UNKNOWN_IN_CHAIN `desc->base.next` is not NULL.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_file` is NULL.
 * @retval SPUDRESULT_SPUDFILE_NULL_PATH `desc->path` is NULL or empty.
 * @retval SPUDRESULT_DESC_INVALID_PARAMETERS `desc->access` or
 *         `desc->create` is not one of its values, `desc->share` has a bit
 *         that isn't, or `desc->create` is SPUDFILE_CREATE_TRUNCATE without write
 *         access.
 * @retval SPUDRESULT_SPUDFILE_NOT_FOUND No file is there (SPUDFILE_CREATE_OPEN_EXISTING),
 *         or a directory on the way to it isn't.
 * @retval SPUDRESULT_SPUDFILE_ALREADY_EXISTS Something is there (SPUDFILE_CREATE_NEW).
 * @retval SPUDRESULT_SPUDFILE_NOT_A_FILE A directory, or another kind of entry, is
 *         there.
 * @retval SPUDRESULT_SPUDFILE_ACCESS_DENIED The platform won't open it for this
 *         access.
 * @retval SPUDRESULT_SPUDFILE_IN_USE Windows: another handle's share mode
 *         forbids it.
 * @retval SPUDRESULT_OUT_OF_MEMORY The file could not be allocated.
 * @retval SPUDRESULT_API_SPECIFIC_FAILURE The platform refused for another
 *         reason.
 */
SPUDRESULT spudfile_file_open(
    spudfile_instance instance,
    const spudfile_open_desc *desc,
    spudfile_file *out_file);

/**
 * @brief Closes a file. Mappings of it stay valid.
 *
 * @param[in] file The file to close. NULL does nothing.
 */
void spudfile_file_close(spudfile_file file);

/* ---- I/O at an offset ---- */

/**
 * @brief Reads from a file at an offset: pread, or ReadFile at an
 * OVERLAPPED offset.
 *
 * Thread: may overlap any call on the same file but spudfile_file_close()
 * and spud_debug_name_set(); a write to the range being read is the
 * caller's to order.
 *
 * @param[in]  file     The file, opened with read access.
 * @param[in]  offset   Where to start, in bytes from the file's start.
 * @param[out] buffer   Receives what was read.
 * @param[in]  size     How many bytes to read.
 * @param[out] out_read Receives how many were read: `size`, unless the
 *                      file ends first, then what there was (0 at or past
 *                      its end).
 *
 * @retval SPUD_SUCCESS `*out_read` bytes were read.
 * @retval SPUDRESULT_SPUDFILE_INVALID_FILE `file` is NULL.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `buffer` or `out_read` is NULL.
 * @retval SPUDRESULT_SPUDFILE_NOT_READABLE `file` wasn't opened for reading.
 * @retval SPUDRESULT_API_SPECIFIC_FAILURE The platform's read failed.
 */
SPUDRESULT spudfile_file_read_at(
    spudfile_file file,
    uint64_t offset,
    void *buffer,
    uint64_t size,
    uint64_t *out_read);

/**
 * @brief Writes to a file at an offset: pwrite, or WriteFile at an
 * OVERLAPPED offset. Writing past the end grows the file; the bytes between
 * its old end and `offset` read zero. Every byte is written, or the call
 * fails.
 *
 * Thread: may overlap any call on the same file but spudfile_file_close()
 * and spud_debug_name_set(); a second write to the same range, or a read
 * of it, is the caller's to order.
 *
 * @param[in] file   The file, opened with write access.
 * @param[in] offset Where to start, in bytes from the file's start.
 * @param[in] data   What to write.
 * @param[in] size   How many bytes. 0 writes nothing and succeeds.
 *
 * @retval SPUD_SUCCESS Every byte was written.
 * @retval SPUDRESULT_SPUDFILE_INVALID_FILE `file` is NULL.
 * @retval SPUDRESULT_NULL_DESC `data` is NULL and `size` isn't 0.
 * @retval SPUDRESULT_SPUDFILE_NOT_WRITABLE `file` wasn't opened for writing.
 * @retval SPUDRESULT_API_SPECIFIC_FAILURE The platform's write failed (a
 *         full disk among them).
 */
SPUDRESULT spudfile_file_write_at(
    spudfile_file file,
    uint64_t offset,
    const void *data,
    uint64_t size);

/**
 * @brief Reads a file's size.
 *
 * Thread: may overlap any call on the same file but spudfile_file_close()
 * and spud_debug_name_set().
 *
 * @param[in]  file     The file.
 * @param[out] out_size Receives its size in bytes.
 *
 * @retval SPUD_SUCCESS `*out_size` was written.
 * @retval SPUDRESULT_SPUDFILE_INVALID_FILE `file` is NULL.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_size` is NULL.
 * @retval SPUDRESULT_API_SPECIFIC_FAILURE The platform couldn't say.
 */
SPUDRESULT spudfile_file_get_size(
    spudfile_file file,
    uint64_t *out_size);

/**
 * @brief Sets a file's size: ftruncate, or SetFileInformationByHandle's
 * end of file. Growing it adds bytes that read zero.
 *
 * Thread: may overlap any call on the same file but spudfile_file_close()
 * and spud_debug_name_set(); a read or write past the new end at the
 * same time is the caller's to order.
 *
 * @param[in] file The file, opened with write access.
 * @param[in] size Its new size in bytes.
 *
 * @retval SPUD_SUCCESS The file is `size` bytes.
 * @retval SPUDRESULT_SPUDFILE_INVALID_FILE `file` is NULL.
 * @retval SPUDRESULT_SPUDFILE_NOT_WRITABLE `file` wasn't opened for writing.
 * @retval SPUDRESULT_API_SPECIFIC_FAILURE The platform refused.
 */
SPUDRESULT spudfile_file_set_size(
    spudfile_file file,
    uint64_t size);

/** @brief How far spudfile_file_flush() and spudfile_path_replace() push data before
 * returning. */
typedef enum SPUDFILE_FLUSH_LEVEL {
	/** Nothing is waited for (spudfile_path_replace() only). */
	SPUDFILE_FLUSH_LEVEL_NONE,
	/** Handed to the drive: fsync on Linux and Apple, FlushFileBuffers on
	 * Windows. On Apple, drives may still hold it in a volatile cache. */
	SPUDFILE_FLUSH_LEVEL_DEVICE,
	/** On the storage media itself, past any drive cache: fcntl(F_FULLFSYNC)
	 * on Apple; the same calls as DEVICE on Linux and Windows, which already
	 * flush the drive cache there. Apple file systems that don't support
	 * F_FULLFSYNC (some network and FUSE volumes) fail with
	 * SPUDRESULT_SPUDFILE_FLUSH_LEVEL_NOT_SUPPORTED - fall back to DEVICE if
	 * that's acceptable. */
	SPUDFILE_FLUSH_LEVEL_MEDIA,
} SPUDFILE_FLUSH_LEVEL;

/**
 * @brief Blocks until a file's written data and metadata reach a level.
 *
 * Thread: may overlap any call on the same file but spudfile_file_close()
 * and spud_debug_name_set().
 *
 * @param[in] file  The file, opened with write access.
 * @param[in] level SPUDFILE_FLUSH_LEVEL_DEVICE or SPUDFILE_FLUSH_LEVEL_MEDIA.
 *
 * @retval SPUD_SUCCESS The data reached `level`.
 * @retval SPUDRESULT_SPUDFILE_INVALID_FILE `file` is NULL.
 * @retval SPUDRESULT_DESC_INVALID_PARAMETERS `level` is neither.
 * @retval SPUDRESULT_SPUDFILE_NOT_WRITABLE `file` wasn't opened for writing.
 * @retval SPUDRESULT_SPUDFILE_FLUSH_LEVEL_NOT_SUPPORTED See SPUDFILE_FLUSH_LEVEL_MEDIA.
 * @retval SPUDRESULT_API_SPECIFIC_FAILURE The platform's flush failed.
 */
SPUDRESULT spudfile_file_flush(
    spudfile_file file,
    SPUDFILE_FLUSH_LEVEL level);

/* ---- Mapping ---- */

/**
 * @brief Maps part of a file into memory, read-only. Pages are read from
 * disk as they're first touched and can be dropped again under memory
 * pressure. Any offset works: the alignment the platform needs is handled
 * here. The mapping holds what it needs on its own, so the file may be
 * closed first. Shrinking the file while it's mapped makes touching the
 * cut pages fault (SIGBUS / EXCEPTION_IN_PAGE_ERROR): don't map a file
 * something else may truncate.
 *
 * Thread: may overlap any call on the same file but spudfile_file_close()
 * and spud_debug_name_set().
 *
 * @param[in]  file        The file, opened with read access.
 * @param[in]  offset      The first byte to map.
 * @param[in]  size        How many bytes; the range lies inside the file.
 * @param[out] out_mapping Receives the mapping; NULL on failure.
 *
 * @retval SPUD_SUCCESS The range is mapped.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_mapping` is NULL.
 * @retval SPUDRESULT_SPUDFILE_INVALID_FILE `file` is NULL.
 * @retval SPUDRESULT_ZERO_SIZE `size` is 0.
 * @retval SPUDRESULT_SPUDFILE_NOT_READABLE `file` wasn't opened for reading.
 * @retval SPUDRESULT_INDEX_OUT_OF_RANGE The range goes past the file's end.
 * @retval SPUDRESULT_OUT_OF_MEMORY No address space or memory for it.
 * @retval SPUDRESULT_API_SPECIFIC_FAILURE The platform refused.
 */
SPUDRESULT spudfile_file_map(
    spudfile_file file,
    uint64_t offset,
    uint64_t size,
    spudfile_mapping *out_mapping);

/**
 * @brief The first mapped byte: the file's byte at the mapping's offset.
 *
 * Thread: may overlap any call on the same mapping but
 * spudfile_mapping_release() and spud_debug_name_set(); so may reading the
 * mapped bytes.
 *
 * @param[in] mapping The mapping.
 * @return The first byte, or NULL for a NULL mapping.
 */
const void *spudfile_mapping_get_data(spudfile_mapping mapping);
/**
 * @brief How many bytes are mapped.
 *
 * Thread: may overlap any call on the same mapping but
 * spudfile_mapping_release() and spud_debug_name_set().
 *
 * @param[in] mapping The mapping.
 * @return The size in bytes, or 0 for a NULL mapping.
 */
uint64_t spudfile_mapping_get_size(spudfile_mapping mapping);
/**
 * @brief Unmaps a mapping.
 * @param[in] mapping The mapping to release. NULL does nothing.
 */
void spudfile_mapping_release(spudfile_mapping mapping);

/* ---- Paths ---- */

/** @brief What is at a path. */
typedef enum SPUDFILE_PATH_KIND {
	SPUDFILE_PATH_KIND_FILE      = 1, /**< A file. */
	SPUDFILE_PATH_KIND_DIRECTORY = 2, /**< A directory. */
	SPUDFILE_PATH_KIND_OTHER     = 3, /**< A device, socket, pipe or the like. */
} SPUDFILE_PATH_KIND;

/** @brief What spudfile_path_get_info() reports. A symbolic link is followed:
 * this is what it leads to. */
typedef struct SPUDFILE_PATH_INFO {
	/** `type` is SPUD_STRUCT_TYPE_FILES_PATH_INFO; `next` is NULL (nothing
	 * is chained onto this struct yet). The caller sets the type; SpudFiles
	 * fills in the rest. See SPUD_STRUCT_BASE. */
	SPUD_STRUCT_BASE_OUT base;
	SPUDFILE_PATH_KIND kind;  /**< What it is. */
	uint64_t size;       /**< Its size in bytes, for a file; 0 otherwise. */
	/** When its contents last changed, in nanoseconds since 1970-01-01 UTC,
	 * to the resolution the file system keeps. */
	int64_t modified_ns;
} SPUDFILE_PATH_INFO;

/**
 * @brief Reports what is at a path: stat, or GetFileAttributesExW.
 *
 * @param[in]  instance The instance.
 * @param[in]  path     The path.
 * @param[out] out_info Receives what is there.
 *
 * @retval SPUD_SUCCESS Something is there; `*out_info` says what.
 * @retval SPUDRESULT_SPUDFILE_INVALID_INSTANCE `instance` is NULL.
 * @retval SPUDRESULT_SPUDFILE_NULL_PATH `path` is NULL or empty.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_info` is NULL.
 * @retval SPUDRESULT_STRUCT_WRONG_TYPE `out_info->base.type` is not
 *         SPUD_STRUCT_TYPE_FILES_PATH_INFO.
 * @retval SPUDRESULT_STRUCT_UNKNOWN_IN_CHAIN `out_info->base.next` is not NULL.
 * @retval SPUDRESULT_SPUDFILE_NOT_FOUND Nothing is there. This is an answer, not
 *         a failure.
 * @retval SPUDRESULT_SPUDFILE_ACCESS_DENIED A directory on the way can't be read.
 * @retval SPUDRESULT_API_SPECIFIC_FAILURE The platform couldn't say.
 */
SPUDRESULT spudfile_path_get_info(
    spudfile_instance instance,
    const char *path,
    SPUDFILE_PATH_INFO *out_info);

/**
 * @brief Atomically replaces `target` with `source`: anyone opening `target`
 * sees its old contents or the new ones, never a mix, and `source` no longer
 * exists afterwards. `target` needn't exist.
 *
 * Both must be on the same volume (SPUDRESULT_SPUDFILE_DIFFERENT_VOLUME): write
 * the source beside the target. Only the name is replaced: flush the source
 * with spudfile_file_flush() first, or a crash can leave the target named but
 * empty. `level` is how far the rename itself is pushed before returning:
 * an fsync (F_FULLFSYNC for MEDIA on Apple) of the target's directory on
 * Linux and Apple, MOVEFILE_WRITE_THROUGH on Windows. With NONE the rename
 * is still atomic, but a power loss shortly after can undo it. If the rename
 * succeeds but that flush fails, the target already names the new contents
 * and the result is the flush's. On Windows the replace fails with
 * SPUDRESULT_SPUDFILE_IN_USE while the target is open without SPUDFILE_SHARE_DELETE,
 * or mapped, by this process or another, or read-only. POSIX allows it, and
 * existing handles and mappings keep seeing the old contents.
 *
 * @param[in] instance The instance.
 * @param[in] source   The file that takes the target's name.
 * @param[in] target   The name it takes.
 * @param[in] level    How far the rename is pushed.
 *
 * @retval SPUD_SUCCESS `target` names what `source` did.
 * @retval SPUDRESULT_SPUDFILE_INVALID_INSTANCE `instance` is NULL.
 * @retval SPUDRESULT_SPUDFILE_NULL_PATH `source` or `target` is NULL or empty.
 * @retval SPUDRESULT_DESC_INVALID_PARAMETERS `level` isn't an SPUDFILE_FLUSH_LEVEL.
 * @retval SPUDRESULT_SPUDFILE_NOT_FOUND There is no `source`.
 * @retval SPUDRESULT_SPUDFILE_DIFFERENT_VOLUME They're on different volumes.
 * @retval SPUDRESULT_SPUDFILE_IN_USE Windows: see above.
 * @retval SPUDRESULT_SPUDFILE_FLUSH_LEVEL_NOT_SUPPORTED See SPUDFILE_FLUSH_LEVEL_MEDIA.
 * @retval SPUDRESULT_API_SPECIFIC_FAILURE The platform refused.
 */
SPUDRESULT spudfile_path_replace(
    spudfile_instance instance,
    const char *source,
    const char *target,
    SPUDFILE_FLUSH_LEVEL level);

/**
 * @brief Removes a file: unlink, or DeleteFileW.
 *
 * @param[in] instance The instance.
 * @param[in] path     The file.
 *
 * @retval SPUD_SUCCESS It is gone.
 * @retval SPUDRESULT_SPUDFILE_INVALID_INSTANCE `instance` is NULL.
 * @retval SPUDRESULT_SPUDFILE_NULL_PATH `path` is NULL or empty.
 * @retval SPUDRESULT_SPUDFILE_NOT_FOUND Nothing is there.
 * @retval SPUDRESULT_SPUDFILE_NOT_A_FILE A directory is there.
 * @retval SPUDRESULT_SPUDFILE_IN_USE Windows: it is open without
 *         SPUDFILE_SHARE_DELETE, mapped or read-only.
 * @retval SPUDRESULT_SPUDFILE_ACCESS_DENIED POSIX: its directory can't be written.
 * @retval SPUDRESULT_API_SPECIFIC_FAILURE The platform refused.
 */
SPUDRESULT spudfile_path_remove(
    spudfile_instance instance,
    const char *path);

/**
 * @brief Makes one directory: mkdir, or CreateDirectoryW. Its parent must
 * exist; making the directories on the way is the caller's.
 *
 * @param[in] instance The instance.
 * @param[in] path     The directory to make.
 *
 * @retval SPUD_SUCCESS It was made.
 * @retval SPUDRESULT_SPUDFILE_INVALID_INSTANCE `instance` is NULL.
 * @retval SPUDRESULT_SPUDFILE_NULL_PATH `path` is NULL or empty.
 * @retval SPUDRESULT_SPUDFILE_NOT_FOUND Its parent isn't there.
 * @retval SPUDRESULT_SPUDFILE_ALREADY_EXISTS Something is there already (a
 *         directory or anything else; spudfile_path_get_info() says which).
 * @retval SPUDRESULT_SPUDFILE_ACCESS_DENIED Its parent can't be written.
 * @retval SPUDRESULT_API_SPECIFIC_FAILURE The platform refused.
 */
SPUDRESULT spudfile_directory_create(
    spudfile_instance instance,
    const char *path);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // SPUDLIB_SPUDFILES_H
