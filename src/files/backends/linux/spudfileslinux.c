#include "spudcore.h"
#include "spudfiles.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#if __cplusplus
extern "C" {
#endif // __cplusplus

struct sfs_file_t {
#if _DEBUG
	const char *debug_name;
#endif
	int fd;
};

// --------------------------------------------------------------------------
// File open / close
// --------------------------------------------------------------------------

SPUDRESULT
sfs_file_open(
    SFS_FILE_OPEN_ATTRIBUTES open_attribs,
    sfs_file *out_file) {
	if (!open_attribs.str_file_path || open_attribs.str_file_path[0] == '\0')
		return SPUDRESULT_SFS_NULL_PATH;
	if (!out_file)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;

	int flags = 0;

	switch (open_attribs.access_mode) {
	case SFS_EFILE_ACCESS_MODE_READ:
		flags = O_RDONLY;
		break;
	case SFS_EFILE_ACCESS_MODE_OVERWRITE:
		flags = O_WRONLY | O_CREAT | O_TRUNC;
		break;
	case SFS_EFILE_ACCESS_MODE_APPEND:
		flags = O_WRONLY | O_CREAT | O_APPEND;
		break;
	case SFS_EFILE_ACCESS_MODE_READ_UPDATE:
		flags = O_RDWR;
		break;
	case SFS_EFILE_ACCESS_MODE_OVERWRITE_UPDATE:
		flags = O_RDWR | O_CREAT | O_TRUNC;
		break;
	case SFS_EFILE_ACCESS_MODE_APPEND_UPDATE:
		flags = O_RDWR | O_CREAT | O_APPEND;
		break;
	default:
		return SPUDRESULT_DESC_INVALID_PARAMETERS;
	}

	int fd = open(open_attribs.str_file_path, flags, 0644);
	if (fd < 0)
		return SPUDRESULT_GENERAL_FAILURE;

	struct sfs_file_t *f = (struct sfs_file_t *)malloc(sizeof(struct sfs_file_t));
	if (!f) {
		close(fd);
		return SPUDRESULT_OUT_OF_MEMORY;
	}
#if _DEBUG
	f->debug_name = NULL;
#endif
	f->fd     = fd;
	*out_file = f;
	return SPUD_SUCCESS;
}

SPUDRESULT sfs_file_release(sfs_file file) {
	if (!file)
		return SPUDRESULT_SFS_INVALID_FILE;
	close(file->fd);
#if _DEBUG
	free((void *)file->debug_name);
#endif
	free(file);
	return SPUD_SUCCESS;
}

// --------------------------------------------------------------------------
// File I/O
// --------------------------------------------------------------------------

SPUDRESULT sfs_file_read(
    sfs_file file,
    void *data,
    uint64_t size) {
	if (!file)
		return SPUDRESULT_SFS_INVALID_FILE;
	if (!data)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;

	uint8_t *dst       = (uint8_t *)data;
	uint64_t remaining = size;

	while (remaining > 0) {
		ssize_t n = read(file->fd, dst, remaining);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			return SPUDRESULT_GENERAL_FAILURE;
		}
		if (n == 0)
			break; // EOF
		dst += n;
		remaining -= (uint64_t)n;
	}
	return SPUD_SUCCESS;
}

SPUDRESULT sfs_file_write(
    sfs_file file,
    const void *data,
    uint64_t size) {
	if (!file)
		return SPUDRESULT_SFS_INVALID_FILE;
	if (!data)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;

	const uint8_t *src = (const uint8_t *)data;
	uint64_t remaining = size;

	while (remaining > 0) {
		ssize_t n = write(file->fd, src, remaining);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			return SPUDRESULT_GENERAL_FAILURE;
		}
		src += n;
		remaining -= (uint64_t)n;
	}
	return SPUD_SUCCESS;
}

// --------------------------------------------------------------------------
// File position / size
// --------------------------------------------------------------------------

SPUDRESULT sfs_file_get_size(
    sfs_file file,
    uint64_t *out_size) {
	if (!file)
		return SPUDRESULT_SFS_INVALID_FILE;
	if (!out_size)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;

	struct stat st;
	if (fstat(file->fd, &st) != 0)
		return SPUDRESULT_GENERAL_FAILURE;

	*out_size = (uint64_t)st.st_size;
	return SPUD_SUCCESS;
}

SPUDRESULT sfs_file_get_pos(
    sfs_file file,
    uint64_t *out_pos) {
	if (!file)
		return SPUDRESULT_SFS_INVALID_FILE;
	if (!out_pos)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;

	off_t pos = lseek(file->fd, 0, SEEK_CUR);
	if (pos < 0)
		return SPUDRESULT_GENERAL_FAILURE;

	*out_pos = (uint64_t)pos;
	return SPUD_SUCCESS;
}

SPUDRESULT
sfs_file_set_pos(
    sfs_file file,
    uint64_t offset,
    SFS_FILE_POS_ORIGIN origin) {
	if (!file)
		return SPUDRESULT_SFS_INVALID_FILE;

	int whence;
	switch (origin) {
	case SFS_FILE_POS_ORIGIN_START:
		whence = SEEK_SET;
		break;
	case SFS_FILE_POS_ORIGIN_CURRENT:
		whence = SEEK_CUR;
		break;
	case SFS_FILE_POS_ORIGIN_END:
		whence = SEEK_END;
		break;
	default:
		return SPUDRESULT_DESC_INVALID_PARAMETERS;
	}

	if (lseek(file->fd, (off_t)offset, whence) < 0)
		return SPUDRESULT_GENERAL_FAILURE;

	return SPUD_SUCCESS;
}

// --------------------------------------------------------------------------
// Flush / replace / map
// --------------------------------------------------------------------------

struct sfs_mapping_t {
	void *base;         // what mmap returned - page-aligned, before [data]
	uint64_t base_size; // what was mapped from [base]
	const uint8_t *data;
	uint64_t size;
};

// Flushes [fd] to [level] (DEVICE or MEDIA). Linux's fsync already flushes
// the drive's cache, so both levels are the same call.
static SPUDRESULT sfs_sync_fd(int fd, SFS_FLUSH_LEVEL level) {
	if (level != SFS_FLUSH_LEVEL_DEVICE && level != SFS_FLUSH_LEVEL_MEDIA)
		return SPUDRESULT_DESC_INVALID_PARAMETERS;
	int r;
	do {
		r = fsync(fd);
	} while (r != 0 && errno == EINTR);
	return r == 0 ? SPUD_SUCCESS : SPUDRESULT_GENERAL_FAILURE;
}

SPUDRESULT sfs_file_flush(sfs_file file, SFS_FLUSH_LEVEL level) {
	if (!file)
		return SPUDRESULT_SFS_INVALID_FILE;
	return sfs_sync_fd(file->fd, level);
}

// Makes a rename in [path]'s directory durable: the directory entry lives in
// the directory, not the file, so it's the directory that's synced.
static SPUDRESULT sfs_flush_parent_directory(const char *path, SFS_FLUSH_LEVEL level) {
	char dir[PATH_MAX];
	size_t len = strlen(path);
	if (len >= sizeof(dir))
		return SPUDRESULT_DESC_INVALID_PARAMETERS;
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

	int fd = open(dir, O_RDONLY | O_DIRECTORY);
	if (fd < 0)
		return SPUDRESULT_GENERAL_FAILURE;
	SPUDRESULT result = sfs_sync_fd(fd, level);
	close(fd);
	return result;
}

SPUDRESULT sfs_file_replace(
    const char *source_path,
    const char *target_path,
    SFS_FLUSH_LEVEL level) {
	if (!source_path || source_path[0] == '\0' || !target_path || target_path[0] == '\0')
		return SPUDRESULT_SFS_NULL_PATH;

	if (level != SFS_FLUSH_LEVEL_NONE && level != SFS_FLUSH_LEVEL_DEVICE && level != SFS_FLUSH_LEVEL_MEDIA)
		return SPUDRESULT_DESC_INVALID_PARAMETERS;

	if (rename(source_path, target_path) != 0)
		return errno == EXDEV ? SPUDRESULT_SFS_DIFFERENT_VOLUME : SPUDRESULT_GENERAL_FAILURE;

	if (level != SFS_FLUSH_LEVEL_NONE)
		return sfs_flush_parent_directory(target_path, level);
	return SPUD_SUCCESS;
}

SPUDRESULT sfs_file_remove(const char *file_path) {
	if (!file_path || file_path[0] == '\0')
		return SPUDRESULT_SFS_NULL_PATH;
	if (unlink(file_path) != 0) {
		// EISDIR on Linux, EPERM on Apple: a directory isn't a file.
		if (errno == ENOENT || errno == ENOTDIR || errno == EISDIR || errno == EPERM)
			return SPUDRESULT_SFS_INVALID_FILE;
		return SPUDRESULT_GENERAL_FAILURE;
	}
	return SPUD_SUCCESS;
}

SPUDRESULT sfs_file_map(
    sfs_file file,
    uint64_t offset,
    uint64_t size,
    sfs_mapping *out_mapping) {
	if (!out_mapping)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	*out_mapping = NULL;
	if (!file)
		return SPUDRESULT_SFS_INVALID_FILE;
	if (size == 0)
		return SPUDRESULT_ZERO_SIZE;

	int flags = fcntl(file->fd, F_GETFL);
	if (flags < 0)
		return SPUDRESULT_GENERAL_FAILURE;
	if ((flags & O_ACCMODE) == O_WRONLY)
		return SPUDRESULT_SFS_NOT_READABLE;

	struct stat st;
	if (fstat(file->fd, &st) != 0)
		return SPUDRESULT_GENERAL_FAILURE;
	uint64_t file_size = (uint64_t)st.st_size;
	if (offset > file_size || size > file_size - offset)
		return SPUDRESULT_INDEX_OUT_OF_RANGE;

	// mmap offsets must be page-aligned: map from the page holding [offset].
	uint64_t page    = (uint64_t)sysconf(_SC_PAGESIZE);
	uint64_t aligned = offset - offset % page;
	uint64_t lead    = offset - aligned;
	if (size > (uint64_t)SIZE_MAX - lead)
		return SPUDRESULT_INDEX_OUT_OF_RANGE; // more than the address space

	void *base = mmap(NULL, (size_t)(lead + size), PROT_READ, MAP_SHARED, file->fd, (off_t)aligned);
	if (base == MAP_FAILED)
		return errno == ENOMEM ? SPUDRESULT_OUT_OF_MEMORY : SPUDRESULT_GENERAL_FAILURE;

	struct sfs_mapping_t *m = (struct sfs_mapping_t *)malloc(sizeof(struct sfs_mapping_t));
	if (!m) {
		munmap(base, (size_t)(lead + size));
		return SPUDRESULT_OUT_OF_MEMORY;
	}
	m->base      = base;
	m->base_size = lead + size;
	m->data      = (const uint8_t *)base + lead;
	m->size      = size;
	*out_mapping = m;
	return SPUD_SUCCESS;
}

const void *sfs_mapping_get_data(sfs_mapping mapping) {
	return mapping ? mapping->data : NULL;
}

uint64_t sfs_mapping_get_size(sfs_mapping mapping) {
	return mapping ? mapping->size : 0;
}

SPUDRESULT sfs_mapping_release(sfs_mapping mapping) {
	if (!mapping)
		return SPUDRESULT_SFS_INVALID_MAPPING;
	munmap(mapping->base, (size_t)mapping->base_size);
	free(mapping);
	return SPUD_SUCCESS;
}

// --------------------------------------------------------------------------
// File / directory queries
// --------------------------------------------------------------------------

bool sfs_file_exists(const char *file_path) {
	if (!file_path || file_path[0] == '\0')
		return false;
	struct stat st;
	if (stat(file_path, &st) != 0)
		return false;
	return !S_ISDIR(st.st_mode);
}

bool sfs_directory_exists(const char *dir_path) {
	if (!dir_path || dir_path[0] == '\0')
		return false;
	struct stat st;
	if (stat(dir_path, &st) != 0)
		return false;
	return S_ISDIR(st.st_mode);
}
SPUDRESULT sfs_set_working_directory(const char *dir) {
	if (!dir || dir[0] == '\0')
		return SPUDRESULT_SFS_NULL_PATH;
	if (chdir(dir) != 0)
		return SPUDRESULT_GENERAL_FAILURE;
	return SPUD_SUCCESS;
}

const char *sfs_get_working_directory() {
	static char buf[PATH_MAX];
	if (!getcwd(buf, sizeof(buf)))
		return NULL;
	return buf;
}

// --------------------------------------------------------------------------
// Directory creation
// --------------------------------------------------------------------------

SPUDRESULT sfs_create_directory(const char *path) {
	if (path == NULL || path[0] == '\0')
		return SPUDRESULT_SFS_NULL_PATH;

	char dir_path[PATH_MAX];
	strncpy(dir_path, path, PATH_MAX - 1);
	dir_path[PATH_MAX - 1] = '\0';

	// normalize slashes
	char *dp = dir_path;
	sfs_correct_slashes(&dp, SFS_SLASH);

	char *search = dir_path;
	char *slash_pos;

	do {
		slash_pos = strchr(search, SFS_SLASH);

		char parent[PATH_MAX];
		if (slash_pos != NULL) {
			uint64_t len = (uint64_t)(slash_pos - dir_path);
			if (len == 0) {
				// leading slash (absolute path root) - skip
				search = slash_pos + 1;
				continue;
			}
			strncpy(parent, dir_path, len);
			parent[len] = '\0';
			search      = slash_pos + 1;
		} else {
			strncpy(parent, dir_path, PATH_MAX - 1);
			parent[PATH_MAX - 1] = '\0';
		}

		if (parent[0] == '\0')
			continue;

		if (mkdir(parent, 0755) != 0) {
			if (errno != EEXIST)
				return SPUDRESULT_GENERAL_FAILURE;

			struct stat st;
			if (stat(parent, &st) != 0 || !S_ISDIR(st.st_mode))
				return SPUDRESULT_GENERAL_FAILURE;
		}
	} while (slash_pos != NULL);

	return SPUD_SUCCESS;
}

// --------------------------------------------------------------------------
// File dialogs
// --------------------------------------------------------------------------

// Runs `argv[0]` (searched on PATH, never through a shell) with the given
// argv, captures its trimmed stdout into a static buffer, and returns it.
// Returns NULL if the binary can't be found/run, exits non-zero, or prints
// nothing (e.g. the user cancelled the dialog).
static const char *run_dialog_helper(char *const argv[]) {
	static char result[PATH_MAX * 4];

	int pipefd[2];
	if (pipe(pipefd) != 0)
		return NULL;

	pid_t pid = fork();
	if (pid < 0) {
		close(pipefd[0]);
		close(pipefd[1]);
		return NULL;
	}

	if (pid == 0) {
		// child
		close(pipefd[0]);
		dup2(pipefd[1], STDOUT_FILENO);
		close(pipefd[1]);
		int devnull = open("/dev/null", O_WRONLY);
		if (devnull >= 0)
			dup2(devnull, STDERR_FILENO);
		execvp(argv[0], argv);
		_exit(127);
	}

	// parent
	close(pipefd[1]);

	uint64_t total = 0;
	for (;;) {
		ssize_t n = read(pipefd[0], result + total, sizeof(result) - 1 - total);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			break;
		}
		if (n == 0)
			break;
		total += (uint64_t)n;
		if (total >= sizeof(result) - 1)
			break;
	}
	close(pipefd[0]);
	result[total] = '\0';

	int status = 0;
	waitpid(pid, &status, 0);

	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
		return NULL;

	// strip trailing newline emitted by the dialog helpers
	while (total > 0 && (result[total - 1] == '\n' || result[total - 1] == '\r'))
		result[--total] = '\0';

	if (total == 0)
		return NULL;

	return result;
}

static bool dialog_helper_available(const char *name) {
	char probe[PATH_MAX];
	const char *dirs = getenv("PATH");
	if (!dirs)
		return false;

	char path_copy[PATH_MAX * 4];
	strncpy(path_copy, dirs, sizeof(path_copy) - 1);
	path_copy[sizeof(path_copy) - 1] = '\0';

	char *saveptr = NULL;
	for (char *dir = strtok_r(path_copy, ":", &saveptr); dir; dir = strtok_r(NULL, ":", &saveptr)) {
		int n = snprintf(probe, sizeof(probe), "%s/%s", dir, name);
		if (n > 0 && (size_t)n < sizeof(probe) && access(probe, X_OK) == 0)
			return true;
	}
	return false;
}

const char *sfs_file_dialog(SFS_FILE_DIALOG_ATTRIBUTES attributes) {
	const char *title = attributes.title ? attributes.title : "";

	if (dialog_helper_available("zenity")) {
		char *argv[8];
		int argc     = 0;
		argv[argc++] = "zenity";
		argv[argc++] = "--file-selection";
		if (attributes.type == SFS_FILE_DIALOG_TYPE_SAVE)
			argv[argc++] = "--save";
		if (attributes.flags & SFS_FILE_DIALOG_FLAG_OVERWRITE_PROMPT)
			argv[argc++] = "--confirm-overwrite";
		static char title_arg[512];
		snprintf(title_arg, sizeof(title_arg), "--title=%s", title);
		argv[argc++] = title_arg;
		argv[argc]   = NULL;
		return run_dialog_helper(argv);
	}

	if (dialog_helper_available("kdialog")) {
		char *argv[6];
		int argc     = 0;
		argv[argc++] = "kdialog";
		argv[argc++] = (char *)(attributes.type == SFS_FILE_DIALOG_TYPE_SAVE ? "--getsavefilename" : "--getopenfilename");
		static char title_arg[512];
		snprintf(title_arg, sizeof(title_arg), "--title=%s", title);
		argv[argc++] = title_arg;
		argv[argc]   = NULL;
		return run_dialog_helper(argv);
	}

	return NULL;
}

const char *sfs_open_folder_dialog(const char *title) {
	if (!title)
		title = "Select Folder";

	if (dialog_helper_available("zenity")) {
		char *argv[6];
		int argc     = 0;
		argv[argc++] = "zenity";
		argv[argc++] = "--file-selection";
		argv[argc++] = "--directory";
		static char title_arg[512];
		snprintf(title_arg, sizeof(title_arg), "--title=%s", title);
		argv[argc++] = title_arg;
		argv[argc]   = NULL;
		return run_dialog_helper(argv);
	}

	if (dialog_helper_available("kdialog")) {
		char *argv[5];
		int argc     = 0;
		argv[argc++] = "kdialog";
		argv[argc++] = "--getexistingdirectory";
		static char title_arg[512];
		snprintf(title_arg, sizeof(title_arg), "--title=%s", title);
		argv[argc++] = title_arg;
		argv[argc]   = NULL;
		return run_dialog_helper(argv);
	}

	return NULL;
}

#if __cplusplus
}
#endif // __cplusplus
