#if SPUDLIB_PLATFORM_LINUX || SPUDLIB_PLATFORM_APPLE

#if SPUDLIB_PLATFORM_LINUX && !defined(_GNU_SOURCE)
#define _GNU_SOURCE // pread, pwrite, the st_mtim member
#endif

#include "../../spudfilesshared.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h> // rename
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* The result an errno from open, stat, mkdir, unlink or rename stands for.
 * Whatever has no result of its own is the platform's failure. */
static SPUDRESULT sfs_posix_result(int error) {
	switch (error) {
	case ENOENT:
	case ENOTDIR:
		return SPUDRESULT_SFS_NOT_FOUND;
	case EEXIST:
		return SPUDRESULT_SFS_ALREADY_EXISTS;
	case EACCES:
	case EPERM:
	case EROFS:
		return SPUDRESULT_SFS_ACCESS_DENIED;
	case EISDIR:
		return SPUDRESULT_SFS_NOT_A_FILE;
	case EXDEV:
		return SPUDRESULT_SFS_DIFFERENT_VOLUME;
	case ENOMEM:
		return SPUDRESULT_OUT_OF_MEMORY;
	default:
		return SPUDRESULT_API_SPECIFIC_FAILURE;
	}
}

SPUDRESULT sfs_backend_open(sfs_instance instance, const sfs_open_desc *desc, intptr_t *out_native) {
	(void)instance;
	int flags = desc->access == SFS_ACCESS_READ ? O_RDONLY : desc->access == SFS_ACCESS_WRITE ? O_WRONLY : O_RDWR;
	switch (desc->create) {
	case SFS_CREATE_OPEN_EXISTING:
		break;
	case SFS_CREATE_NEW:
		flags |= O_CREAT | O_EXCL;
		break;
	case SFS_CREATE_OPEN_OR_CREATE:
		flags |= O_CREAT;
		break;
	case SFS_CREATE_TRUNCATE:
		flags |= O_CREAT | O_TRUNC;
		break;
	default:
		return SPUDRESULT_DESC_INVALID_PARAMETERS;
	}
	// POSIX has no share modes: desc->share is the caller's statement of
	// what others may do, and nothing here can enforce it.
	flags |= O_CLOEXEC;
	int fd;
	do {
		fd = open(desc->path, flags, 0666);
	} while (fd < 0 && errno == EINTR);
	if (fd < 0)
		return sfs_posix_result(errno);
	// open() takes a directory for reading; a file is what is wanted.
	struct stat st;
	if (fstat(fd, &st) != 0) {
		const int error = errno;
		close(fd);
		return sfs_posix_result(error);
	}
	if (!S_ISREG(st.st_mode)) {
		close(fd);
		return SPUDRESULT_SFS_NOT_A_FILE;
	}
	*out_native = (intptr_t)fd;
	return SPUD_SUCCESS;
}

void sfs_backend_close(intptr_t native) { close((int)native); }

SPUDRESULT sfs_backend_read_at(intptr_t native, uint64_t offset, void *buffer, uint64_t size, uint64_t *out_read) {
	uint8_t *dst   = (uint8_t *)buffer;
	uint64_t total = 0;
	while (total < size) {
		const uint64_t remaining = size - total;
		const size_t chunk       = remaining > (uint64_t)SSIZE_MAX ? (size_t)SSIZE_MAX : (size_t)remaining;
		const ssize_t n          = pread((int)native, dst + total, chunk, (off_t)(offset + total));
		if (n < 0) {
			if (errno == EINTR)
				continue;
			*out_read = total;
			return SPUDRESULT_API_SPECIFIC_FAILURE;
		}
		if (n == 0)
			break; // the end of the file
		total += (uint64_t)n;
	}
	*out_read = total;
	return SPUD_SUCCESS;
}

SPUDRESULT sfs_backend_write_at(intptr_t native, uint64_t offset, const void *data, uint64_t size) {
	const uint8_t *src = (const uint8_t *)data;
	uint64_t total     = 0;
	while (total < size) {
		const uint64_t remaining = size - total;
		const size_t chunk       = remaining > (uint64_t)SSIZE_MAX ? (size_t)SSIZE_MAX : (size_t)remaining;
		const ssize_t n          = pwrite((int)native, src + total, chunk, (off_t)(offset + total));
		if (n < 0) {
			if (errno == EINTR)
				continue;
			return SPUDRESULT_API_SPECIFIC_FAILURE;
		}
		total += (uint64_t)n;
	}
	return SPUD_SUCCESS;
}

SPUDRESULT sfs_backend_get_size(intptr_t native, uint64_t *out_size) {
	struct stat st;
	if (fstat((int)native, &st) != 0)
		return SPUDRESULT_API_SPECIFIC_FAILURE;
	*out_size = (uint64_t)st.st_size;
	return SPUD_SUCCESS;
}

SPUDRESULT sfs_backend_set_size(intptr_t native, uint64_t size) {
	int r;
	do {
		r = ftruncate((int)native, (off_t)size);
	} while (r != 0 && errno == EINTR);
	return r == 0 ? SPUD_SUCCESS : SPUDRESULT_API_SPECIFIC_FAILURE;
}

/* Flushes [fd] to [level] (DEVICE or MEDIA). fsync only hands data to the
 * drive, which may keep it in a volatile cache; F_FULLFSYNC asks the drive
 * to write it to the media. Linux's fsync already flushes the drive cache. */
static SPUDRESULT sfs_posix_sync(int fd, SFS_FLUSH_LEVEL level) {
#if defined(F_FULLFSYNC)
	if (level == SFS_FLUSH_LEVEL_MEDIA) {
		if (fcntl(fd, F_FULLFSYNC) == 0)
			return SPUD_SUCCESS;
		return (errno == ENOTSUP || errno == EINVAL || errno == ENOTTY) ? SPUDRESULT_SFS_FLUSH_LEVEL_NOT_SUPPORTED
		                                                               : SPUDRESULT_API_SPECIFIC_FAILURE;
	}
#else
	(void)level;
#endif
	int r;
	do {
		r = fsync(fd);
	} while (r != 0 && errno == EINTR);
	return r == 0 ? SPUD_SUCCESS : SPUDRESULT_API_SPECIFIC_FAILURE;
}

SPUDRESULT sfs_backend_flush(intptr_t native, SFS_FLUSH_LEVEL level) { return sfs_posix_sync((int)native, level); }

SPUDRESULT sfs_backend_map(intptr_t native, uint64_t offset, uint64_t size, void **out_base, uint64_t *out_base_size, uint64_t *out_lead) {
	// mmap offsets must be page-aligned: map from the page holding [offset].
	const uint64_t page    = (uint64_t)sysconf(_SC_PAGESIZE);
	const uint64_t aligned = offset - offset % page;
	const uint64_t lead    = offset - aligned;
	if (size > (uint64_t)SIZE_MAX - lead)
		return SPUDRESULT_INDEX_OUT_OF_RANGE; // more than the address space
	void *base = mmap(NULL, (size_t)(lead + size), PROT_READ, MAP_SHARED, (int)native, (off_t)aligned);
	if (base == MAP_FAILED)
		return errno == ENOMEM ? SPUDRESULT_OUT_OF_MEMORY : SPUDRESULT_API_SPECIFIC_FAILURE;
	*out_base      = base;
	*out_base_size = lead + size;
	*out_lead      = lead;
	return SPUD_SUCCESS;
}

void sfs_backend_unmap(void *base, uint64_t base_size) { munmap(base, (size_t)base_size); }

SPUDRESULT sfs_backend_path_get_info(sfs_instance instance, const char *path, SFS_PATH_INFO *out_info) {
	(void)instance;
	struct stat st;
	if (stat(path, &st) != 0)
		return sfs_posix_result(errno);
	if (S_ISREG(st.st_mode)) {
		out_info->kind = SFS_PATH_KIND_FILE;
		out_info->size = (uint64_t)st.st_size;
	} else {
		out_info->kind = S_ISDIR(st.st_mode) ? SFS_PATH_KIND_DIRECTORY : SFS_PATH_KIND_OTHER;
		out_info->size = 0;
	}
#if SPUDLIB_PLATFORM_APPLE
	out_info->modified_ns = (int64_t)st.st_mtimespec.tv_sec * 1000000000 + st.st_mtimespec.tv_nsec;
#else
	out_info->modified_ns = (int64_t)st.st_mtim.tv_sec * 1000000000 + st.st_mtim.tv_nsec;
#endif
	return SPUD_SUCCESS;
}

/* Makes a rename in [path]'s directory durable: the directory entry lives in
 * the directory, not the file, so it's the directory that's synced. */
static SPUDRESULT sfs_posix_flush_parent(const char *path, SFS_FLUSH_LEVEL level) {
	char dir[PATH_MAX];
	const size_t len = strlen(path);
	if (len >= sizeof(dir))
		return SPUDRESULT_API_SPECIFIC_FAILURE;
	memcpy(dir, path, len + 1);
	char *slash = strrchr(dir, '/');
	if (!slash) {
		dir[0] = '.';
		dir[1] = '\0';
	} else if (slash == dir) {
		dir[1] = '\0'; // the root
	} else {
		*slash = '\0';
	}
	const int fd = open(dir, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (fd < 0)
		return SPUDRESULT_API_SPECIFIC_FAILURE;
	const SPUDRESULT result = sfs_posix_sync(fd, level);
	close(fd);
	return result;
}

SPUDRESULT sfs_backend_path_replace(sfs_instance instance, const char *source, const char *target, SFS_FLUSH_LEVEL level) {
	(void)instance;
	if (rename(source, target) != 0) {
		const int error = errno;
		// A directory in the way of the rename is still not a file to
		// replace; anything else follows the common table.
		return error == EISDIR ? SPUDRESULT_SFS_NOT_A_FILE : sfs_posix_result(error);
	}
	if (level != SFS_FLUSH_LEVEL_NONE)
		return sfs_posix_flush_parent(target, level);
	return SPUD_SUCCESS;
}

SPUDRESULT sfs_backend_path_remove(sfs_instance instance, const char *path) {
	(void)instance;
	if (unlink(path) == 0)
		return SPUD_SUCCESS;
	const int error = errno;
	// unlink of a directory is EISDIR on Linux and EPERM on Apple; EPERM is
	// also a real refusal, so it is told apart by what is there.
	if (error == EISDIR || error == EPERM) {
		struct stat st;
		if (stat(path, &st) == 0 && S_ISDIR(st.st_mode))
			return SPUDRESULT_SFS_NOT_A_FILE;
	}
	return sfs_posix_result(error);
}

SPUDRESULT sfs_backend_directory_create(sfs_instance instance, const char *path) {
	(void)instance;
	if (mkdir(path, 0777) == 0)
		return SPUD_SUCCESS;
	return sfs_posix_result(errno);
}

#endif // SPUDLIB_PLATFORM_LINUX || SPUDLIB_PLATFORM_APPLE
