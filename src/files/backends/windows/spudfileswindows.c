#if SPUDLIB_PLATFORM_WIN32

#include "../../spudfilesshared.h"

#include <Windows.h>
#include <stdint.h>

/* [utf8] as UTF-16, through [instance]'s allocator; freed with
 * spudfile_windows_free_wide and the count it gives. */
static SPUDRESULT spudfile_windows_wide(spudfile_instance instance, const char *utf8, wchar_t **out_wide, int *out_count) {
	*out_wide       = NULL;
	*out_count      = 0;
	const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, NULL, 0);
	if (count <= 0)
		return SPUDRESULT_SPUDFILE_NOT_FOUND; // not UTF-8: no file can have that name
	wchar_t *wide = SPUD_NEW_ARRAY(&instance->allocator, wchar_t, (size_t)count);
	if (!wide)
		return SPUDRESULT_OUT_OF_MEMORY;
	MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, wide, count);
	*out_wide  = wide;
	*out_count = count;
	return SPUD_SUCCESS;
}

static void spudfile_windows_free_wide(spudfile_instance instance, wchar_t *wide, int count) {
	if (wide)
		SPUD_DELETE_ARRAY(&instance->allocator, wchar_t, wide, (size_t)count);
}

static bool spudfile_windows_is_directory(const wchar_t *path) {
	const DWORD attributes = GetFileAttributesW(path);
	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY);
}

/* The result a GetLastError() value stands for. Whatever has no result of
 * its own is the platform's failure. */
static SPUDRESULT spudfile_windows_result(DWORD error) {
	switch (error) {
	case ERROR_FILE_NOT_FOUND:
	case ERROR_PATH_NOT_FOUND:
	case ERROR_INVALID_NAME:
	case ERROR_BAD_NETPATH:
		return SPUDRESULT_SPUDFILE_NOT_FOUND;
	case ERROR_FILE_EXISTS:
	case ERROR_ALREADY_EXISTS:
		return SPUDRESULT_SPUDFILE_ALREADY_EXISTS;
	case ERROR_ACCESS_DENIED:
	case ERROR_WRITE_PROTECT:
		return SPUDRESULT_SPUDFILE_ACCESS_DENIED;
	case ERROR_SHARING_VIOLATION:
	case ERROR_LOCK_VIOLATION:
	case ERROR_USER_MAPPED_FILE:
		return SPUDRESULT_SPUDFILE_IN_USE;
	case ERROR_NOT_SAME_DEVICE:
		return SPUDRESULT_SPUDFILE_DIFFERENT_VOLUME;
	case ERROR_NOT_ENOUGH_MEMORY:
	case ERROR_OUTOFMEMORY:
		return SPUDRESULT_OUT_OF_MEMORY;
	default:
		return SPUDRESULT_API_SPECIFIC_FAILURE;
	}
}

SPUDRESULT spudfile_backend_open(spudfile_instance instance, const spudfile_open_desc *desc, intptr_t *out_native) {
	DWORD access = 0;
	if (desc->access == SPUDFILE_ACCESS_READ || desc->access == SPUDFILE_ACCESS_READ_WRITE)
		access |= GENERIC_READ;
	if (desc->access == SPUDFILE_ACCESS_WRITE || desc->access == SPUDFILE_ACCESS_READ_WRITE)
		access |= GENERIC_WRITE;
	DWORD disposition = 0;
	switch (desc->create) {
	case SPUDFILE_CREATE_OPEN_EXISTING:
		disposition = OPEN_EXISTING;
		break;
	case SPUDFILE_CREATE_NEW:
		disposition = CREATE_NEW;
		break;
	case SPUDFILE_CREATE_OPEN_OR_CREATE:
		disposition = OPEN_ALWAYS;
		break;
	case SPUDFILE_CREATE_TRUNCATE:
		disposition = CREATE_ALWAYS;
		break;
	default:
		return SPUDRESULT_DESC_INVALID_PARAMETERS;
	}
	DWORD share = 0;
	if (desc->share & SPUDFILE_SHARE_READ)
		share |= FILE_SHARE_READ;
	if (desc->share & SPUDFILE_SHARE_WRITE)
		share |= FILE_SHARE_WRITE;
	if (desc->share & SPUDFILE_SHARE_DELETE)
		share |= FILE_SHARE_DELETE;

	wchar_t *path = NULL;
	int count     = 0;
	SPUDRESULT result = spudfile_windows_wide(instance, desc->path, &path, &count);
	if (result != SPUD_SUCCESS)
		return result;
	HANDLE handle = CreateFileW(path, access, share, NULL, disposition, FILE_ATTRIBUTE_NORMAL, NULL);
	if (handle == INVALID_HANDLE_VALUE) {
		const DWORD error = GetLastError();
		// A directory is refused as access denied.
		result = error == ERROR_ACCESS_DENIED && spudfile_windows_is_directory(path) ? SPUDRESULT_SPUDFILE_NOT_A_FILE : spudfile_windows_result(error);
		spudfile_windows_free_wide(instance, path, count);
		return result;
	}
	spudfile_windows_free_wide(instance, path, count);
	// A device (CON, a pipe) opens too; a file is what is wanted.
	if (GetFileType(handle) != FILE_TYPE_DISK) {
		CloseHandle(handle);
		return SPUDRESULT_SPUDFILE_NOT_A_FILE;
	}
	*out_native = (intptr_t)handle;
	return SPUD_SUCCESS;
}

void spudfile_backend_close(intptr_t native) { CloseHandle((HANDLE)native); }

/* ReadFile and WriteFile take a DWORD count. */
static const uint64_t SPUDFILE_WINDOWS_CHUNK = 1u << 30;

SPUDRESULT spudfile_backend_read_at(intptr_t native, uint64_t offset, void *buffer, uint64_t size, uint64_t *out_read) {
	uint8_t *dst   = (uint8_t *)buffer;
	uint64_t total = 0;
	while (total < size) {
		const uint64_t remaining = size - total;
		const DWORD chunk        = (DWORD)(remaining > SPUDFILE_WINDOWS_CHUNK ? SPUDFILE_WINDOWS_CHUNK : remaining);
		const uint64_t at        = offset + total;
		OVERLAPPED position;
		ZeroMemory(&position, sizeof(position));
		position.Offset     = (DWORD)(at & 0xFFFFFFFFu);
		position.OffsetHigh = (DWORD)(at >> 32);
		DWORD n             = 0;
		if (!ReadFile((HANDLE)native, dst + total, chunk, &n, &position)) {
			if (GetLastError() == ERROR_HANDLE_EOF)
				break; // at or past the end
			*out_read = total;
			return SPUDRESULT_API_SPECIFIC_FAILURE;
		}
		if (n == 0)
			break;
		total += n;
	}
	*out_read = total;
	return SPUD_SUCCESS;
}

SPUDRESULT spudfile_backend_write_at(intptr_t native, uint64_t offset, const void *data, uint64_t size) {
	const uint8_t *src = (const uint8_t *)data;
	uint64_t total     = 0;
	while (total < size) {
		const uint64_t remaining = size - total;
		const DWORD chunk        = (DWORD)(remaining > SPUDFILE_WINDOWS_CHUNK ? SPUDFILE_WINDOWS_CHUNK : remaining);
		const uint64_t at        = offset + total;
		OVERLAPPED position;
		ZeroMemory(&position, sizeof(position));
		position.Offset     = (DWORD)(at & 0xFFFFFFFFu);
		position.OffsetHigh = (DWORD)(at >> 32);
		DWORD n             = 0;
		if (!WriteFile((HANDLE)native, src + total, chunk, &n, &position) || n == 0)
			return SPUDRESULT_API_SPECIFIC_FAILURE;
		total += n;
	}
	return SPUD_SUCCESS;
}

SPUDRESULT spudfile_backend_get_size(intptr_t native, uint64_t *out_size) {
	LARGE_INTEGER size;
	if (!GetFileSizeEx((HANDLE)native, &size))
		return SPUDRESULT_API_SPECIFIC_FAILURE;
	*out_size = (uint64_t)size.QuadPart;
	return SPUD_SUCCESS;
}

SPUDRESULT spudfile_backend_set_size(intptr_t native, uint64_t size) {
	FILE_END_OF_FILE_INFO end;
	end.EndOfFile.QuadPart = (LONGLONG)size;
	if (!SetFileInformationByHandle((HANDLE)native, FileEndOfFileInfo, &end, sizeof(end)))
		return SPUDRESULT_API_SPECIFIC_FAILURE;
	return SPUD_SUCCESS;
}

SPUDRESULT spudfile_backend_flush(intptr_t native, SPUDFILE_FLUSH_LEVEL level) {
	// FlushFileBuffers reaches the media on Windows: DEVICE and MEDIA alike.
	(void)level;
	return FlushFileBuffers((HANDLE)native) ? SPUD_SUCCESS : SPUDRESULT_API_SPECIFIC_FAILURE;
}

SPUDRESULT spudfile_backend_map(intptr_t native, uint64_t offset, uint64_t size, void **out_base, uint64_t *out_base_size, uint64_t *out_lead) {
	// View offsets must be multiples of the allocation granularity (64 KiB),
	// not just the page size: map from the granule holding [offset].
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	const uint64_t granularity = info.dwAllocationGranularity;
	const uint64_t aligned     = offset - offset % granularity;
	const uint64_t lead        = offset - aligned;
	if (size > (uint64_t)SIZE_MAX - lead)
		return SPUDRESULT_INDEX_OUT_OF_RANGE; // more than the address space

	HANDLE section = CreateFileMappingW((HANDLE)native, NULL, PAGE_READONLY, 0, 0, NULL);
	if (!section)
		return spudfile_windows_result(GetLastError());
	void *base = MapViewOfFile(section, FILE_MAP_READ, (DWORD)(aligned >> 32), (DWORD)(aligned & 0xFFFFFFFFu), (SIZE_T)(lead + size));
	// The view keeps the section, and through it the file, alive on its own.
	const DWORD error = base ? 0 : GetLastError();
	CloseHandle(section);
	if (!base)
		return spudfile_windows_result(error);
	*out_base      = base;
	*out_base_size = lead + size;
	*out_lead      = lead;
	return SPUD_SUCCESS;
}

void spudfile_backend_unmap(void *base, uint64_t base_size) {
	(void)base_size;
	UnmapViewOfFile(base);
}

/* Nanoseconds since 1970 from a FILETIME's 100 ns since 1601. */
static int64_t spudfile_windows_unix_ns(FILETIME time) {
	const uint64_t ticks = ((uint64_t)time.dwHighDateTime << 32) | time.dwLowDateTime;
	return ((int64_t)ticks - 116444736000000000LL) * 100;
}

SPUDRESULT spudfile_backend_path_get_info(spudfile_instance instance, const char *path, SPUDFILE_PATH_INFO *out_info) {
	wchar_t *wide     = NULL;
	int count         = 0;
	SPUDRESULT result = spudfile_windows_wide(instance, path, &wide, &count);
	if (result != SPUD_SUCCESS)
		return result;
	// Opened for nothing but its attributes, with backup semantics so that a
	// directory opens too; a symbolic link is followed, as stat follows it.
	HANDLE handle = CreateFileW(wide, FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
	                            FILE_FLAG_BACKUP_SEMANTICS, NULL);
	const DWORD error = handle == INVALID_HANDLE_VALUE ? GetLastError() : 0;
	spudfile_windows_free_wide(instance, wide, count);
	if (handle == INVALID_HANDLE_VALUE)
		return spudfile_windows_result(error);
	BY_HANDLE_FILE_INFORMATION information;
	if (!GetFileInformationByHandle(handle, &information)) {
		CloseHandle(handle);
		return SPUDRESULT_API_SPECIFIC_FAILURE;
	}
	const DWORD type = GetFileType(handle);
	CloseHandle(handle);
	if (information.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
		out_info->kind = SPUDFILE_PATH_KIND_DIRECTORY;
		out_info->size = 0;
	} else if (type == FILE_TYPE_DISK) {
		out_info->kind = SPUDFILE_PATH_KIND_FILE;
		out_info->size = ((uint64_t)information.nFileSizeHigh << 32) | information.nFileSizeLow;
	} else {
		out_info->kind = SPUDFILE_PATH_KIND_OTHER;
		out_info->size = 0;
	}
	out_info->modified_ns = spudfile_windows_unix_ns(information.ftLastWriteTime);
	return SPUD_SUCCESS;
}

SPUDRESULT spudfile_backend_path_replace(spudfile_instance instance, const char *source, const char *target, SPUDFILE_FLUSH_LEVEL level) {
	wchar_t *wide_source = NULL;
	wchar_t *wide_target = NULL;
	int source_count     = 0;
	int target_count     = 0;
	SPUDRESULT result    = spudfile_windows_wide(instance, source, &wide_source, &source_count);
	if (result == SPUD_SUCCESS)
		result = spudfile_windows_wide(instance, target, &wide_target, &target_count);
	if (result == SPUD_SUCCESS) {
		DWORD flags = MOVEFILE_REPLACE_EXISTING;
		if (level != SPUDFILE_FLUSH_LEVEL_NONE)
			flags |= MOVEFILE_WRITE_THROUGH;
		if (!MoveFileExW(wide_source, wide_target, flags)) {
			const DWORD error = GetLastError();
			// A target open without FILE_SHARE_DELETE, mapped or read-only is
			// reported as access denied; a directory target the same way.
			if (error == ERROR_ACCESS_DENIED)
				result = spudfile_windows_is_directory(wide_target) ? SPUDRESULT_SPUDFILE_NOT_A_FILE : SPUDRESULT_SPUDFILE_IN_USE;
			else
				result = spudfile_windows_result(error);
		}
	}
	spudfile_windows_free_wide(instance, wide_source, source_count);
	spudfile_windows_free_wide(instance, wide_target, target_count);
	return result;
}

SPUDRESULT spudfile_backend_path_remove(spudfile_instance instance, const char *path) {
	wchar_t *wide     = NULL;
	int count         = 0;
	SPUDRESULT result = spudfile_windows_wide(instance, path, &wide, &count);
	if (result != SPUD_SUCCESS)
		return result;
	if (!DeleteFileW(wide)) {
		const DWORD error = GetLastError();
		// An open or mapped file, a read-only one, or a directory: access
		// denied for all of them, told apart by what is there.
		if (error == ERROR_ACCESS_DENIED)
			result = spudfile_windows_is_directory(wide) ? SPUDRESULT_SPUDFILE_NOT_A_FILE : SPUDRESULT_SPUDFILE_IN_USE;
		else
			result = spudfile_windows_result(error);
	}
	spudfile_windows_free_wide(instance, wide, count);
	return result;
}

SPUDRESULT spudfile_backend_directory_create(spudfile_instance instance, const char *path) {
	wchar_t *wide     = NULL;
	int count         = 0;
	SPUDRESULT result = spudfile_windows_wide(instance, path, &wide, &count);
	if (result != SPUD_SUCCESS)
		return result;
	if (!CreateDirectoryW(wide, NULL))
		result = spudfile_windows_result(GetLastError());
	spudfile_windows_free_wide(instance, wide, count);
	return result;
}

#endif // SPUDLIB_PLATFORM_WIN32
