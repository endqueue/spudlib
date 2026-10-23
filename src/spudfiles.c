#include "spudfiles.h"
#include "files/spudfilesshared.h"
#include "spudallocator.h"
#include "spudstructbase.h"

#include <stdbool.h>
#include <stdint.h>

/* ---- Instance ---- */

SPUDRESULT sfs_instance_create(
    const sfs_instance_desc *desc,
    sfs_instance *out_instance) {
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

	sfs_instance instance = SPUD_NEW_ZEROED(desc->allocator, struct sfs_instance_t);
	if (!instance)
		return SPUDRESULT_OUT_OF_MEMORY;
	instance->allocator = *desc->allocator;
	*out_instance       = instance;
	return SPUD_SUCCESS;
}

void sfs_instance_destroy(sfs_instance instance) {
	if (!instance)
		return;
	const SPUD_ALLOCATOR allocator = instance->allocator;
	SPUD_DELETE(&allocator, struct sfs_instance_t, instance);
}

/* ---- Opening ---- */

static bool sfs_path_given(const char *path) { return path && path[0]; }

SPUDRESULT sfs_file_open(
    sfs_instance instance,
    const sfs_open_desc *desc,
    sfs_file *out_file) {
	if (!instance)
		return SPUDRESULT_SFS_INVALID_INSTANCE;
	if (!desc)
		return SPUDRESULT_NULL_DESC;
	const SPUDRESULT base_result = spud_struct_check(&desc->base, SPUD_STRUCT_TYPE_FILES_OPEN_DESC);
	if (base_result != SPUD_SUCCESS)
		return base_result;
	if (!out_file)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	*out_file = NULL;
	if (!sfs_path_given(desc->path))
		return SPUDRESULT_SFS_NULL_PATH;
	if (desc->access != SFS_ACCESS_READ && desc->access != SFS_ACCESS_WRITE && desc->access != SFS_ACCESS_READ_WRITE)
		return SPUDRESULT_DESC_INVALID_PARAMETERS;
	if (desc->create < SFS_CREATE_OPEN_EXISTING || desc->create > SFS_CREATE_TRUNCATE)
		return SPUDRESULT_DESC_INVALID_PARAMETERS;
	if (desc->share & ~(SFS_SHARE)(SFS_SHARE_READ | SFS_SHARE_WRITE | SFS_SHARE_DELETE))
		return SPUDRESULT_DESC_INVALID_PARAMETERS;
	if (desc->create == SFS_CREATE_TRUNCATE && desc->access == SFS_ACCESS_READ)
		return SPUDRESULT_DESC_INVALID_PARAMETERS;

	sfs_file file = SPUD_NEW_ZEROED(&instance->allocator, struct sfs_file_t);
	if (!file)
		return SPUDRESULT_OUT_OF_MEMORY;
	file->head.allocator = instance->allocator;
	file->instance       = instance;
	file->access         = desc->access;

	const SPUDRESULT result = sfs_backend_open(instance, desc, &file->native);
	if (result != SPUD_SUCCESS) {
		const SPUD_ALLOCATOR allocator = file->head.allocator;
		SPUD_DELETE(&allocator, struct sfs_file_t, file);
		return result;
	}
	if (spud_debug_name_set(file, desc->debug_name) != SPUD_SUCCESS) {
		sfs_file_close(file);
		return SPUDRESULT_OUT_OF_MEMORY;
	}
	*out_file = file;
	return SPUD_SUCCESS;
}

void sfs_file_close(sfs_file file) {
	if (!file)
		return;
	sfs_backend_close(file->native);
	const SPUD_ALLOCATOR allocator = file->head.allocator;
	spud_string_free(&allocator, file->head.debug_name);
	SPUD_DELETE(&allocator, struct sfs_file_t, file);
}

/* ---- I/O at an offset ---- */

static bool sfs_readable(sfs_file file) { return file->access == SFS_ACCESS_READ || file->access == SFS_ACCESS_READ_WRITE; }
static bool sfs_writable(sfs_file file) { return file->access == SFS_ACCESS_WRITE || file->access == SFS_ACCESS_READ_WRITE; }

SPUDRESULT sfs_file_read_at(
    sfs_file file,
    uint64_t offset,
    void *buffer,
    uint64_t size,
    uint64_t *out_read) {
	if (!file)
		return SPUDRESULT_SFS_INVALID_FILE;
	if (!buffer || !out_read)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	*out_read = 0;
	if (!sfs_readable(file))
		return SPUDRESULT_SFS_NOT_READABLE;
	if (size == 0)
		return SPUD_SUCCESS;
	return sfs_backend_read_at(file->native, offset, buffer, size, out_read);
}

SPUDRESULT sfs_file_write_at(
    sfs_file file,
    uint64_t offset,
    const void *data,
    uint64_t size) {
	if (!file)
		return SPUDRESULT_SFS_INVALID_FILE;
	if (!data && size != 0)
		return SPUDRESULT_NULL_DESC;
	if (!sfs_writable(file))
		return SPUDRESULT_SFS_NOT_WRITABLE;
	if (size == 0)
		return SPUD_SUCCESS;
	return sfs_backend_write_at(file->native, offset, data, size);
}

SPUDRESULT sfs_file_get_size(
    sfs_file file,
    uint64_t *out_size) {
	if (!file)
		return SPUDRESULT_SFS_INVALID_FILE;
	if (!out_size)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	return sfs_backend_get_size(file->native, out_size);
}

SPUDRESULT sfs_file_set_size(
    sfs_file file,
    uint64_t size) {
	if (!file)
		return SPUDRESULT_SFS_INVALID_FILE;
	if (!sfs_writable(file))
		return SPUDRESULT_SFS_NOT_WRITABLE;
	return sfs_backend_set_size(file->native, size);
}

SPUDRESULT sfs_file_flush(
    sfs_file file,
    SFS_FLUSH_LEVEL level) {
	if (!file)
		return SPUDRESULT_SFS_INVALID_FILE;
	if (level != SFS_FLUSH_LEVEL_DEVICE && level != SFS_FLUSH_LEVEL_MEDIA)
		return SPUDRESULT_DESC_INVALID_PARAMETERS;
	if (!sfs_writable(file))
		return SPUDRESULT_SFS_NOT_WRITABLE;
	return sfs_backend_flush(file->native, level);
}

/* ---- Mapping ---- */

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
	if (!sfs_readable(file))
		return SPUDRESULT_SFS_NOT_READABLE;
	uint64_t file_size      = 0;
	const SPUDRESULT sized  = sfs_backend_get_size(file->native, &file_size);
	if (sized != SPUD_SUCCESS)
		return sized;
	if (offset > file_size || size > file_size - offset)
		return SPUDRESULT_INDEX_OUT_OF_RANGE;

	sfs_mapping mapping = SPUD_NEW_ZEROED(&file->head.allocator, struct sfs_mapping_t);
	if (!mapping)
		return SPUDRESULT_OUT_OF_MEMORY;
	mapping->head.allocator = file->head.allocator;
	uint64_t lead           = 0;
	const SPUDRESULT result = sfs_backend_map(file->native, offset, size, &mapping->base, &mapping->base_size, &lead);
	if (result != SPUD_SUCCESS) {
		const SPUD_ALLOCATOR allocator = mapping->head.allocator;
		SPUD_DELETE(&allocator, struct sfs_mapping_t, mapping);
		return result;
	}
	mapping->data = (const uint8_t *)mapping->base + lead;
	mapping->size = size;
	*out_mapping  = mapping;
	return SPUD_SUCCESS;
}

const void *sfs_mapping_get_data(sfs_mapping mapping) { return mapping ? mapping->data : NULL; }

uint64_t sfs_mapping_get_size(sfs_mapping mapping) { return mapping ? mapping->size : 0; }

void sfs_mapping_release(sfs_mapping mapping) {
	if (!mapping)
		return;
	sfs_backend_unmap(mapping->base, mapping->base_size);
	const SPUD_ALLOCATOR allocator = mapping->head.allocator;
	spud_string_free(&allocator, mapping->head.debug_name);
	SPUD_DELETE(&allocator, struct sfs_mapping_t, mapping);
}

/* ---- Paths ---- */

SPUDRESULT sfs_path_get_info(
    sfs_instance instance,
    const char *path,
    SFS_PATH_INFO *out_info) {
	if (!instance)
		return SPUDRESULT_SFS_INVALID_INSTANCE;
	if (!sfs_path_given(path))
		return SPUDRESULT_SFS_NULL_PATH;
	if (!out_info)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	const SPUDRESULT base_result = spud_struct_out_check(&out_info->base, SPUD_STRUCT_TYPE_FILES_PATH_INFO);
	if (base_result != SPUD_SUCCESS)
		return base_result;
	out_info->kind        = (SFS_PATH_KIND)0;
	out_info->size        = 0;
	out_info->modified_ns = 0;
	return sfs_backend_path_get_info(instance, path, out_info);
}

SPUDRESULT sfs_path_replace(
    sfs_instance instance,
    const char *source,
    const char *target,
    SFS_FLUSH_LEVEL level) {
	if (!instance)
		return SPUDRESULT_SFS_INVALID_INSTANCE;
	if (!sfs_path_given(source))
		return SPUDRESULT_SFS_NULL_PATH;
	if (!sfs_path_given(target))
		return SPUDRESULT_SFS_NULL_PATH;
	if (level != SFS_FLUSH_LEVEL_NONE && level != SFS_FLUSH_LEVEL_DEVICE && level != SFS_FLUSH_LEVEL_MEDIA)
		return SPUDRESULT_DESC_INVALID_PARAMETERS;
	return sfs_backend_path_replace(instance, source, target, level);
}

SPUDRESULT sfs_path_remove(
    sfs_instance instance,
    const char *path) {
	if (!instance)
		return SPUDRESULT_SFS_INVALID_INSTANCE;
	if (!sfs_path_given(path))
		return SPUDRESULT_SFS_NULL_PATH;
	return sfs_backend_path_remove(instance, path);
}

SPUDRESULT sfs_directory_create(
    sfs_instance instance,
    const char *path) {
	if (!instance)
		return SPUDRESULT_SFS_INVALID_INSTANCE;
	if (!sfs_path_given(path))
		return SPUDRESULT_SFS_NULL_PATH;
	return sfs_backend_directory_create(instance, path);
}
