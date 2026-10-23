#ifndef SPUDFILESSHARED_H
#define SPUDFILESSHARED_H

/*
 * What a SpudFiles backend provides. src/spudfiles.c is the front end: the
 * instance, every argument check, and every object's memory are there,
 * once. A backend is called only with arguments that passed the checks, and
 * only translates: a file is its native handle (a POSIX descriptor or a
 * Windows HANDLE, as an intptr_t), a mapping its base and size.
 */

#include "spudfiles.h"
#include "../spudallocator.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct sfs_instance_t {
	SPUD_ALLOCATOR allocator;
};

struct sfs_file_t {
	spud_object_head head;
	sfs_instance instance;
	SFS_ACCESS access;
	intptr_t native;
};

struct sfs_mapping_t {
	spud_object_head head;
	void *base;         /* what the platform mapped: aligned, before data */
	uint64_t base_size; /* what was mapped from base */
	const uint8_t *data;
	uint64_t size;
};

/* Opens [desc->path]; the checks have passed. [instance] is for memory the
 * translation needs (Windows' UTF-16 paths). */
SPUDRESULT sfs_backend_open(sfs_instance instance, const sfs_open_desc *desc, intptr_t *out_native);
void sfs_backend_close(intptr_t native);
SPUDRESULT sfs_backend_read_at(intptr_t native, uint64_t offset, void *buffer, uint64_t size, uint64_t *out_read);
SPUDRESULT sfs_backend_write_at(intptr_t native, uint64_t offset, const void *data, uint64_t size);
SPUDRESULT sfs_backend_get_size(intptr_t native, uint64_t *out_size);
SPUDRESULT sfs_backend_set_size(intptr_t native, uint64_t size);
SPUDRESULT sfs_backend_flush(intptr_t native, SFS_FLUSH_LEVEL level);
/* Maps [size] bytes from [offset], which lie inside the file: from the
 * aligned position at or before [offset], [*out_lead] bytes before it. */
SPUDRESULT sfs_backend_map(intptr_t native, uint64_t offset, uint64_t size, void **out_base, uint64_t *out_base_size, uint64_t *out_lead);
void sfs_backend_unmap(void *base, uint64_t base_size);
SPUDRESULT sfs_backend_path_get_info(sfs_instance instance, const char *path, SFS_PATH_INFO *out_info);
SPUDRESULT sfs_backend_path_replace(sfs_instance instance, const char *source, const char *target, SFS_FLUSH_LEVEL level);
SPUDRESULT sfs_backend_path_remove(sfs_instance instance, const char *path);
SPUDRESULT sfs_backend_directory_create(sfs_instance instance, const char *path);

#ifdef __cplusplus
}
#endif

#endif // SPUDFILESSHARED_H
