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

struct spudfile_instance_t {
	/* No debug name: the instance desc has none. */
	spud_object_head head;
};

struct spudfile_file_t {
	spud_object_head head;
	spudfile_instance instance;
	SPUDFILE_ACCESS access;
	intptr_t native;
};

struct spudfile_mapping_t {
	spud_object_head head;
	void *base;         /* what the platform mapped: aligned, before data */
	uint64_t base_size; /* what was mapped from base */
	const uint8_t *data;
	uint64_t size;
};

/* Opens [desc->path]; the checks have passed. [instance] is for memory the
 * translation needs (Windows' UTF-16 paths). */
SPUDRESULT spudfile_backend_open(spudfile_instance instance, const spudfile_open_desc *desc, intptr_t *out_native);
void spudfile_backend_close(intptr_t native);
SPUDRESULT spudfile_backend_read_at(intptr_t native, uint64_t offset, void *buffer, uint64_t size, uint64_t *out_read);
SPUDRESULT spudfile_backend_write_at(intptr_t native, uint64_t offset, const void *data, uint64_t size);
SPUDRESULT spudfile_backend_get_size(intptr_t native, uint64_t *out_size);
SPUDRESULT spudfile_backend_set_size(intptr_t native, uint64_t size);
SPUDRESULT spudfile_backend_flush(intptr_t native, SPUDFILE_FLUSH_LEVEL level);
/* Maps [size] bytes from [offset], which lie inside the file: from the
 * aligned position at or before [offset], [*out_lead] bytes before it. */
SPUDRESULT spudfile_backend_map(intptr_t native, uint64_t offset, uint64_t size, void **out_base, uint64_t *out_base_size, uint64_t *out_lead);
void spudfile_backend_unmap(void *base, uint64_t base_size);
SPUDRESULT spudfile_backend_path_get_info(spudfile_instance instance, const char *path, SPUDFILE_PATH_INFO *out_info);
SPUDRESULT spudfile_backend_path_replace(spudfile_instance instance, const char *source, const char *target, SPUDFILE_FLUSH_LEVEL level);
SPUDRESULT spudfile_backend_path_remove(spudfile_instance instance, const char *path);
SPUDRESULT spudfile_backend_directory_create(spudfile_instance instance, const char *path);

#ifdef __cplusplus
}
#endif

#endif // SPUDFILESSHARED_H
