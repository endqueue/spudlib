#include "spudfiles.h"
#include "files/spudfilesshared.h"
#include "spudallocator.h"
#include "spudstructbase.h"

#include <stdbool.h>
#include <stdint.h>

/* ---- Instance ---- */

SPUDRESULT spudfile_instance_create(
    const spudfile_instance_desc *desc,
    spudfile_instance *out_instance) {
	if (!desc)
		return SPUDRESULT_NULL_DESC;
	const SPUDRESULT base_result = spud_struct_check(&desc->base, SPUD_STRUCT_TYPE_FILES_INSTANCE_DESC);
	if (base_result != SPUD_SUCCESS)
		return base_result;
	if (!out_instance)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	*out_instance                      = NULL;
	const SPUDRESULT allocator_result  = spud_allocator_check(desc->allocator);
	if (allocator_result != SPUD_SUCCESS)
		return allocator_result;

	spudfile_instance instance = SPUD_NEW_ZEROED(desc->allocator, struct spudfile_instance_t);
	if (!instance)
		return SPUDRESULT_OUT_OF_MEMORY;
	instance->head.allocator = *desc->allocator;
	SPUD_BIRTH(instance);
	*out_instance       = instance;
	return SPUD_SUCCESS;
}

void spudfile_instance_destroy(spudfile_instance instance) {
	if (!instance)
		return;
	if (SPUD_STALE(instance, "spudfile_instance_destroy", "instance"))
		return;
	SPUD_KILL(instance);
	const SPUD_ALLOCATOR allocator = instance->head.allocator;
	SPUD_DELETE(&allocator, struct spudfile_instance_t, instance);
}

/* ---- Opening ---- */

static bool spudfile_path_given(const char *path) { return path && path[0]; }

SPUDRESULT spudfile_file_open(
    spudfile_instance instance,
    const spudfile_open_desc *desc,
    spudfile_file *out_file) {
	if (!instance)
		return SPUDRESULT_SPUDFILE_INVALID_INSTANCE;
	if (SPUD_STALE(instance, "spudfile_file_open", "instance"))
		return SPUDRESULT_SPUDFILE_INVALID_INSTANCE;
	if (!desc)
		return SPUDRESULT_NULL_DESC;
	const SPUDRESULT base_result = spud_struct_check(&desc->base, SPUD_STRUCT_TYPE_FILES_OPEN_DESC);
	if (base_result != SPUD_SUCCESS)
		return base_result;
	if (!out_file)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	*out_file = NULL;
	if (!spudfile_path_given(desc->path))
		return SPUDRESULT_SPUDFILE_NULL_PATH;
	if (desc->access != SPUDFILE_ACCESS_READ && desc->access != SPUDFILE_ACCESS_WRITE && desc->access != SPUDFILE_ACCESS_READ_WRITE)
		return SPUDRESULT_DESC_INVALID_PARAMETERS;
	if (desc->create < SPUDFILE_CREATE_OPEN_EXISTING || desc->create > SPUDFILE_CREATE_TRUNCATE)
		return SPUDRESULT_DESC_INVALID_PARAMETERS;
	if (desc->share & ~(SPUDFILE_SHARE)(SPUDFILE_SHARE_READ | SPUDFILE_SHARE_WRITE | SPUDFILE_SHARE_DELETE))
		return SPUDRESULT_DESC_INVALID_PARAMETERS;
	if (desc->create == SPUDFILE_CREATE_TRUNCATE && desc->access == SPUDFILE_ACCESS_READ)
		return SPUDRESULT_DESC_INVALID_PARAMETERS;

	spudfile_file file = SPUD_NEW_ZEROED(&instance->head.allocator, struct spudfile_file_t);
	if (!file)
		return SPUDRESULT_OUT_OF_MEMORY;
	file->head.allocator = instance->head.allocator;
	SPUD_BIRTH(file);
	file->instance       = instance;
	file->access         = desc->access;

	const SPUDRESULT result = spudfile_backend_open(instance, desc, &file->native);
	if (result != SPUD_SUCCESS) {
		const SPUD_ALLOCATOR allocator = file->head.allocator;
		SPUD_DELETE(&allocator, struct spudfile_file_t, file);
		return result;
	}
	if (spud_debug_name_set(file, desc->debug_name) != SPUD_SUCCESS) {
		spudfile_file_close(file);
		return SPUDRESULT_OUT_OF_MEMORY;
	}
	*out_file = file;
	return SPUD_SUCCESS;
}

void spudfile_file_close(spudfile_file file) {
	if (!file)
		return;
	if (SPUD_STALE(file, "spudfile_file_close", "file"))
		return;
	spudfile_backend_close(file->native);
	SPUD_KILL(file);
	const SPUD_ALLOCATOR allocator = file->head.allocator;
	spud_string_free(&allocator, file->head.debug_name);
	SPUD_DELETE(&allocator, struct spudfile_file_t, file);
}

/* ---- I/O at an offset ---- */

static bool spudfile_readable(spudfile_file file) { return file->access == SPUDFILE_ACCESS_READ || file->access == SPUDFILE_ACCESS_READ_WRITE; }
static bool spudfile_writable(spudfile_file file) { return file->access == SPUDFILE_ACCESS_WRITE || file->access == SPUDFILE_ACCESS_READ_WRITE; }

SPUDRESULT spudfile_file_read_at(
    spudfile_file file,
    uint64_t offset,
    void *buffer,
    uint64_t size,
    uint64_t *out_read) {
	if (!file)
		return SPUDRESULT_SPUDFILE_INVALID_FILE;
	if (SPUD_STALE(file, "spudfile_file_read_at", "file"))
		return SPUDRESULT_SPUDFILE_INVALID_FILE;
	if (!buffer || !out_read)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	*out_read = 0;
	if (!spudfile_readable(file))
		return SPUDRESULT_SPUDFILE_NOT_READABLE;
	if (size == 0)
		return SPUD_SUCCESS;
	return spudfile_backend_read_at(file->native, offset, buffer, size, out_read);
}

SPUDRESULT spudfile_file_write_at(
    spudfile_file file,
    uint64_t offset,
    const void *data,
    uint64_t size) {
	if (!file)
		return SPUDRESULT_SPUDFILE_INVALID_FILE;
	if (SPUD_STALE(file, "spudfile_file_write_at", "file"))
		return SPUDRESULT_SPUDFILE_INVALID_FILE;
	if (!data && size != 0)
		return SPUDRESULT_NULL_DESC;
	if (!spudfile_writable(file))
		return SPUDRESULT_SPUDFILE_NOT_WRITABLE;
	if (size == 0)
		return SPUD_SUCCESS;
	return spudfile_backend_write_at(file->native, offset, data, size);
}

SPUDRESULT spudfile_file_get_size(
    spudfile_file file,
    uint64_t *out_size) {
	if (!file)
		return SPUDRESULT_SPUDFILE_INVALID_FILE;
	if (SPUD_STALE(file, "spudfile_file_get_size", "file"))
		return SPUDRESULT_SPUDFILE_INVALID_FILE;
	if (!out_size)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	return spudfile_backend_get_size(file->native, out_size);
}

SPUDRESULT spudfile_file_set_size(
    spudfile_file file,
    uint64_t size) {
	if (!file)
		return SPUDRESULT_SPUDFILE_INVALID_FILE;
	if (SPUD_STALE(file, "spudfile_file_set_size", "file"))
		return SPUDRESULT_SPUDFILE_INVALID_FILE;
	if (!spudfile_writable(file))
		return SPUDRESULT_SPUDFILE_NOT_WRITABLE;
	return spudfile_backend_set_size(file->native, size);
}

SPUDRESULT spudfile_file_flush(
    spudfile_file file,
    SPUDFILE_FLUSH_LEVEL level) {
	if (!file)
		return SPUDRESULT_SPUDFILE_INVALID_FILE;
	if (SPUD_STALE(file, "spudfile_file_flush", "file"))
		return SPUDRESULT_SPUDFILE_INVALID_FILE;
	if (level != SPUDFILE_FLUSH_LEVEL_DEVICE && level != SPUDFILE_FLUSH_LEVEL_MEDIA)
		return SPUDRESULT_DESC_INVALID_PARAMETERS;
	if (!spudfile_writable(file))
		return SPUDRESULT_SPUDFILE_NOT_WRITABLE;
	return spudfile_backend_flush(file->native, level);
}

/* ---- Mapping ---- */

SPUDRESULT spudfile_file_map(
    spudfile_file file,
    uint64_t offset,
    uint64_t size,
    spudfile_mapping *out_mapping) {
	if (!out_mapping)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	*out_mapping = NULL;
	if (!file)
		return SPUDRESULT_SPUDFILE_INVALID_FILE;
	if (SPUD_STALE(file, "spudfile_file_map", "file"))
		return SPUDRESULT_SPUDFILE_INVALID_FILE;
	if (size == 0)
		return SPUDRESULT_ZERO_SIZE;
	if (!spudfile_readable(file))
		return SPUDRESULT_SPUDFILE_NOT_READABLE;
	uint64_t file_size      = 0;
	const SPUDRESULT sized  = spudfile_backend_get_size(file->native, &file_size);
	if (sized != SPUD_SUCCESS)
		return sized;
	if (offset > file_size || size > file_size - offset)
		return SPUDRESULT_INDEX_OUT_OF_RANGE;

	spudfile_mapping mapping = SPUD_NEW_ZEROED(&file->head.allocator, struct spudfile_mapping_t);
	if (!mapping)
		return SPUDRESULT_OUT_OF_MEMORY;
	mapping->head.allocator = file->head.allocator;
	SPUD_BIRTH(mapping);
	uint64_t lead           = 0;
	const SPUDRESULT result = spudfile_backend_map(file->native, offset, size, &mapping->base, &mapping->base_size, &lead);
	if (result != SPUD_SUCCESS) {
		const SPUD_ALLOCATOR allocator = mapping->head.allocator;
		SPUD_DELETE(&allocator, struct spudfile_mapping_t, mapping);
		return result;
	}
	mapping->data = (const uint8_t *)mapping->base + lead;
	mapping->size = size;
	*out_mapping  = mapping;
	return SPUD_SUCCESS;
}

const void *spudfile_mapping_get_data(spudfile_mapping mapping) { return mapping ? mapping->data : NULL; }

uint64_t spudfile_mapping_get_size(spudfile_mapping mapping) {
	if (!mapping)
		return 0;
	if (SPUD_STALE(mapping, "spudfile_mapping_get_size", "mapping"))
		return 0;
	return mapping->size;
}

void spudfile_mapping_release(spudfile_mapping mapping) {
	if (!mapping)
		return;
	if (SPUD_STALE(mapping, "spudfile_mapping_release", "mapping"))
		return;
	spudfile_backend_unmap(mapping->base, mapping->base_size);
	SPUD_KILL(mapping);
	const SPUD_ALLOCATOR allocator = mapping->head.allocator;
	spud_string_free(&allocator, mapping->head.debug_name);
	SPUD_DELETE(&allocator, struct spudfile_mapping_t, mapping);
}

/* ---- Paths ---- */

SPUDRESULT spudfile_path_get_info(
    spudfile_instance instance,
    const char *path,
    SPUDFILE_PATH_INFO *out_info) {
	if (!instance)
		return SPUDRESULT_SPUDFILE_INVALID_INSTANCE;
	if (SPUD_STALE(instance, "spudfile_path_get_info", "instance"))
		return SPUDRESULT_SPUDFILE_INVALID_INSTANCE;
	if (!spudfile_path_given(path))
		return SPUDRESULT_SPUDFILE_NULL_PATH;
	if (!out_info)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	const SPUDRESULT base_result = spud_struct_out_check(&out_info->base, SPUD_STRUCT_TYPE_FILES_PATH_INFO);
	if (base_result != SPUD_SUCCESS)
		return base_result;
	out_info->kind        = (SPUDFILE_PATH_KIND)0;
	out_info->size        = 0;
	out_info->modified_ns = 0;
	return spudfile_backend_path_get_info(instance, path, out_info);
}

SPUDRESULT spudfile_path_replace(
    spudfile_instance instance,
    const char *source,
    const char *target,
    SPUDFILE_FLUSH_LEVEL level) {
	if (!instance)
		return SPUDRESULT_SPUDFILE_INVALID_INSTANCE;
	if (SPUD_STALE(instance, "spudfile_path_replace", "instance"))
		return SPUDRESULT_SPUDFILE_INVALID_INSTANCE;
	if (!spudfile_path_given(source))
		return SPUDRESULT_SPUDFILE_NULL_PATH;
	if (!spudfile_path_given(target))
		return SPUDRESULT_SPUDFILE_NULL_PATH;
	if (level != SPUDFILE_FLUSH_LEVEL_NONE && level != SPUDFILE_FLUSH_LEVEL_DEVICE && level != SPUDFILE_FLUSH_LEVEL_MEDIA)
		return SPUDRESULT_DESC_INVALID_PARAMETERS;
	return spudfile_backend_path_replace(instance, source, target, level);
}

SPUDRESULT spudfile_path_remove(
    spudfile_instance instance,
    const char *path) {
	if (!instance)
		return SPUDRESULT_SPUDFILE_INVALID_INSTANCE;
	if (SPUD_STALE(instance, "spudfile_path_remove", "instance"))
		return SPUDRESULT_SPUDFILE_INVALID_INSTANCE;
	if (!spudfile_path_given(path))
		return SPUDRESULT_SPUDFILE_NULL_PATH;
	return spudfile_backend_path_remove(instance, path);
}

SPUDRESULT spudfile_directory_create(
    spudfile_instance instance,
    const char *path) {
	if (!instance)
		return SPUDRESULT_SPUDFILE_INVALID_INSTANCE;
	if (SPUD_STALE(instance, "spudfile_directory_create", "instance"))
		return SPUDRESULT_SPUDFILE_INVALID_INSTANCE;
	if (!spudfile_path_given(path))
		return SPUDRESULT_SPUDFILE_NULL_PATH;
	return spudfile_backend_directory_create(instance, path);
}
