/*
 * spudfiles_test — SpudFiles, in a temporary directory.
 *
 * Every call and its argument checks: the instance; opening with each
 * access and creation choice and what each refuses; reading and writing at
 * an offset, a short read at the end, a write past it; sizing; flushing;
 * mapping at an offset no page boundary falls on, outliving its file; what
 * is at a path; replacing, removing and making a directory; a file with a
 * name outside ASCII; and two threads reading one file at once. Every block
 * goes through a counting allocator, checked at the end. Needs nothing but
 * a writable temporary directory.
 *
 * Exits 0 when every check passes, 1 when one fails, 77 (skipped) with no
 * temporary directory.
 */

#include <spudfiles.h>

#include "spudtest_allocator.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if SPUDLIB_PLATFORM_WIN32
#include <Windows.h>
#include <direct.h>
#include <process.h>
#define rmdir _rmdir
#define SLASH "\\"
#else
#include <pthread.h>
#include <unistd.h>
#define SLASH "/"
#endif

#define EXIT_SKIPPED 77

static int failures = 0;

#define CHECK(cond)                                                              \
	do {                                                                         \
		if (!(cond)) {                                                           \
			fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
			++failures;                                                          \
		}                                                                        \
	} while (0)

#define CHECK_RESULT(call, expected)                                                                                         \
	do {                                                                                                                     \
		const SPUDRESULT r_ = (call);                                                                                        \
		if (r_ != (expected)) {                                                                                              \
			fprintf(stderr, "%s:%d: %s returned %s, expected %s\n", __FILE__, __LINE__, #call, spudresult_str(r_), #expected); \
			++failures;                                                                                                      \
		}                                                                                                                    \
	} while (0)

static char g_dir[1024];
static sfs_instance g_files;

static const char *path_in(
    char *buf,
    size_t cap,
    const char *name) {
	snprintf(buf, cap, "%s" SLASH "%s", g_dir, name);
	return buf;
}

static sfs_open_desc open_desc(
    const char *path,
    SFS_ACCESS access,
    SFS_CREATE create) {
	sfs_open_desc desc;
	memset(&desc, 0, sizeof(desc));
	desc.base.type = SPUD_STRUCT_TYPE_FILES_OPEN_DESC;
	desc.path      = path;
	desc.access    = access;
	desc.create    = create;
	desc.share     = SFS_SHARE_READ | SFS_SHARE_WRITE | SFS_SHARE_DELETE;
	return desc;
}

static sfs_file open_file(
    const char *path,
    SFS_ACCESS access,
    SFS_CREATE create) {
	const sfs_open_desc desc = open_desc(path, access, create);
	sfs_file file            = NULL;
	CHECK_RESULT(sfs_file_open(g_files, &desc, &file), SPUD_SUCCESS);
	return file;
}

static void write_whole(
    const char *path,
    const char *text) {
	sfs_file file = open_file(path, SFS_ACCESS_WRITE, SFS_CREATE_TRUNCATE);
	if (!file)
		return;
	CHECK_RESULT(sfs_file_write_at(file, 0, text, strlen(text)), SPUD_SUCCESS);
	sfs_file_close(file);
}

static int holds(
    const char *path,
    const char *text) {
	sfs_file file = open_file(path, SFS_ACCESS_READ, SFS_CREATE_OPEN_EXISTING);
	if (!file)
		return 0;
	char buf[256]  = {0};
	uint64_t count = 0;
	CHECK_RESULT(sfs_file_read_at(file, 0, buf, sizeof(buf) - 1, &count), SPUD_SUCCESS);
	sfs_file_close(file);
	return count == strlen(text) && memcmp(buf, text, (size_t)count) == 0;
}

static SFS_PATH_INFO info_of(
    const char *path,
    SPUDRESULT *out_result) {
	SFS_PATH_INFO info;
	memset(&info, 0, sizeof(info));
	info.base.type = SPUD_STRUCT_TYPE_FILES_PATH_INFO;
	*out_result    = sfs_path_get_info(g_files, path, &info);
	return info;
}

static void test_instance(const SPUD_ALLOCATOR *allocator) {
	sfs_instance instance = (sfs_instance)(uintptr_t)1;
	sfs_instance_desc desc;
	memset(&desc, 0, sizeof(desc));
	CHECK_RESULT(sfs_instance_create(NULL, &instance), SPUDRESULT_NULL_DESC);
	CHECK_RESULT(sfs_instance_create(&desc, &instance), SPUDRESULT_STRUCT_WRONG_TYPE);
	desc.base.type = SPUD_STRUCT_TYPE_FILES_INSTANCE_DESC;
	desc.base.next = (const SPUD_STRUCT_BASE *)&desc;
	CHECK_RESULT(sfs_instance_create(&desc, &instance), SPUDRESULT_STRUCT_UNKNOWN_IN_CHAIN);
	desc.base.next = NULL;
	CHECK_RESULT(sfs_instance_create(&desc, NULL), SPUDRESULT_NULL_OUTPUT_PARAMETER);
	CHECK_RESULT(sfs_instance_create(&desc, &instance), SPUDRESULT_NULL_ALLOCATOR);
	CHECK(instance == NULL);
	SPUD_ALLOCATOR missing = *allocator;
	missing.allocate       = NULL;
	desc.allocator         = &missing;
	CHECK_RESULT(sfs_instance_create(&desc, &instance), SPUDRESULT_INVALID_ALLOCATOR);
	sfs_instance_destroy(NULL);
}

static void test_open_checks(void) {
	char path[1100];
	path_in(path, sizeof(path), "checks.txt");
	sfs_file file      = (sfs_file)(uintptr_t)1;
	sfs_open_desc desc = open_desc(path, SFS_ACCESS_READ, SFS_CREATE_OPEN_EXISTING);

	CHECK_RESULT(sfs_file_open(NULL, &desc, &file), SPUDRESULT_SFS_INVALID_INSTANCE);
	CHECK_RESULT(sfs_file_open(g_files, NULL, &file), SPUDRESULT_NULL_DESC);
	desc.base.type = SPUD_STRUCT_TYPE_FILES_PATH_INFO;
	CHECK_RESULT(sfs_file_open(g_files, &desc, &file), SPUDRESULT_STRUCT_WRONG_TYPE);
	desc.base.type = SPUD_STRUCT_TYPE_FILES_OPEN_DESC;
	CHECK_RESULT(sfs_file_open(g_files, &desc, NULL), SPUDRESULT_NULL_OUTPUT_PARAMETER);
	desc.path = NULL;
	CHECK_RESULT(sfs_file_open(g_files, &desc, &file), SPUDRESULT_SFS_NULL_PATH);
	CHECK(file == NULL);
	desc.path = "";
	CHECK_RESULT(sfs_file_open(g_files, &desc, &file), SPUDRESULT_SFS_NULL_PATH);
	desc.path   = path;
	desc.access = SFS_ACCESS_NONE;
	CHECK_RESULT(sfs_file_open(g_files, &desc, &file), SPUDRESULT_DESC_INVALID_PARAMETERS);
	desc.access = (SFS_ACCESS)9;
	CHECK_RESULT(sfs_file_open(g_files, &desc, &file), SPUDRESULT_DESC_INVALID_PARAMETERS);
	desc.access = SFS_ACCESS_READ;
	desc.create = SFS_CREATE_NONE;
	CHECK_RESULT(sfs_file_open(g_files, &desc, &file), SPUDRESULT_DESC_INVALID_PARAMETERS);
	desc.create = (SFS_CREATE)9;
	CHECK_RESULT(sfs_file_open(g_files, &desc, &file), SPUDRESULT_DESC_INVALID_PARAMETERS);
	desc.create = SFS_CREATE_OPEN_EXISTING;
	desc.share  = 0x80;
	CHECK_RESULT(sfs_file_open(g_files, &desc, &file), SPUDRESULT_DESC_INVALID_PARAMETERS);
	desc.share  = SFS_SHARE_NONE;
	desc.create = SFS_CREATE_TRUNCATE; // emptying a file opened only to read
	CHECK_RESULT(sfs_file_open(g_files, &desc, &file), SPUDRESULT_DESC_INVALID_PARAMETERS);

	// Nothing there yet.
	desc.create = SFS_CREATE_OPEN_EXISTING;
	CHECK_RESULT(sfs_file_open(g_files, &desc, &file), SPUDRESULT_SFS_NOT_FOUND);
	char deeper[1200];
	snprintf(deeper, sizeof(deeper), "%s" SLASH "missing" SLASH "x.txt", g_dir);
	desc.path   = deeper;
	desc.access = SFS_ACCESS_WRITE;
	desc.create = SFS_CREATE_OPEN_OR_CREATE;
	CHECK_RESULT(sfs_file_open(g_files, &desc, &file), SPUDRESULT_SFS_NOT_FOUND);
	// A directory isn't a file.
	desc.path   = g_dir;
	desc.access = SFS_ACCESS_READ;
	desc.create = SFS_CREATE_OPEN_EXISTING;
	CHECK_RESULT(sfs_file_open(g_files, &desc, &file), SPUDRESULT_SFS_NOT_A_FILE);
	CHECK(file == NULL);
	sfs_file_close(NULL);
}

static void test_create_modes(void) {
	char path[1100];
	path_in(path, sizeof(path), "modes.txt");

	// NEW makes it, and refuses a second time.
	sfs_file file = open_file(path, SFS_ACCESS_WRITE, SFS_CREATE_NEW);
	if (file) {
		CHECK_RESULT(sfs_file_write_at(file, 0, "first", 5), SPUD_SUCCESS);
		sfs_file_close(file);
	}
	sfs_open_desc desc = open_desc(path, SFS_ACCESS_WRITE, SFS_CREATE_NEW);
	file               = NULL;
	CHECK_RESULT(sfs_file_open(g_files, &desc, &file), SPUDRESULT_SFS_ALREADY_EXISTS);
	CHECK(holds(path, "first"));

	// OPEN_OR_CREATE keeps what's there.
	file = open_file(path, SFS_ACCESS_READ_WRITE, SFS_CREATE_OPEN_OR_CREATE);
	if (file) {
		CHECK_RESULT(sfs_file_write_at(file, 0, "F", 1), SPUD_SUCCESS);
		sfs_file_close(file);
	}
	CHECK(holds(path, "First"));
	// TRUNCATE empties it.
	file = open_file(path, SFS_ACCESS_WRITE, SFS_CREATE_TRUNCATE);
	if (file) {
		uint64_t size = 99;
		CHECK_RESULT(sfs_file_get_size(file, &size), SPUD_SUCCESS);
		CHECK(size == 0);
		sfs_file_close(file);
	}
	// OPEN_OR_CREATE makes one too.
	char other[1100];
	path_in(other, sizeof(other), "created.txt");
	file = open_file(other, SFS_ACCESS_WRITE, SFS_CREATE_OPEN_OR_CREATE);
	sfs_file_close(file);
	SPUDRESULT r;
	SFS_PATH_INFO info = info_of(other, &r);
	CHECK(r == SPUD_SUCCESS && info.kind == SFS_PATH_KIND_FILE && info.size == 0);
	CHECK_RESULT(sfs_path_remove(g_files, other), SPUD_SUCCESS);
	CHECK_RESULT(sfs_path_remove(g_files, path), SPUD_SUCCESS);
}

static void test_read_write(void) {
	char path[1100];
	path_in(path, sizeof(path), "rw.bin");
	sfs_open_desc desc = open_desc(path, SFS_ACCESS_READ_WRITE, SFS_CREATE_TRUNCATE);
	desc.debug_name    = "rw file";
	sfs_file file      = NULL;
	CHECK_RESULT(sfs_file_open(g_files, &desc, &file), SPUD_SUCCESS);
	if (!file)
		return;
	CHECK(spud_debug_name_get(file) && strcmp(spud_debug_name_get(file), "rw file") == 0);

	uint8_t bytes[1000];
	for (int i = 0; i < 1000; ++i)
		bytes[i] = (uint8_t)(i * 7);
	CHECK_RESULT(sfs_file_write_at(file, 0, bytes, sizeof(bytes)), SPUD_SUCCESS);
	CHECK_RESULT(sfs_file_write_at(file, 0, NULL, 0), SPUD_SUCCESS);
	uint64_t value = 0;
	CHECK_RESULT(sfs_file_get_size(file, &value), SPUD_SUCCESS);
	CHECK(value == 1000);

	uint8_t back[200];
	uint64_t count = 0;
	CHECK_RESULT(sfs_file_read_at(file, 400, back, sizeof(back), &count), SPUD_SUCCESS);
	CHECK(count == 200 && memcmp(back, bytes + 400, 200) == 0);
	// The file ends first: what there was, then nothing at or past the end.
	CHECK_RESULT(sfs_file_read_at(file, 900, back, sizeof(back), &count), SPUD_SUCCESS);
	CHECK(count == 100 && memcmp(back, bytes + 900, 100) == 0);
	CHECK_RESULT(sfs_file_read_at(file, 1000, back, sizeof(back), &count), SPUD_SUCCESS);
	CHECK(count == 0);
	CHECK_RESULT(sfs_file_read_at(file, 5000, back, sizeof(back), &count), SPUD_SUCCESS);
	CHECK(count == 0);

	// Over the middle changes only those bytes.
	const uint8_t patch[4] = {1, 2, 3, 4};
	CHECK_RESULT(sfs_file_write_at(file, 10, patch, sizeof(patch)), SPUD_SUCCESS);
	uint8_t around[8];
	CHECK_RESULT(sfs_file_read_at(file, 8, around, sizeof(around), &count), SPUD_SUCCESS);
	CHECK(around[0] == bytes[8] && around[1] == bytes[9] && memcmp(around + 2, patch, 4) == 0 && around[6] == bytes[14]);

	// Past the end grows the file; the gap reads zero.
	CHECK_RESULT(sfs_file_write_at(file, 1100, patch, sizeof(patch)), SPUD_SUCCESS);
	CHECK_RESULT(sfs_file_get_size(file, &value), SPUD_SUCCESS);
	CHECK(value == 1104);
	uint8_t gap[128];
	memset(gap, 0xAA, sizeof(gap));
	CHECK_RESULT(sfs_file_read_at(file, 1000, gap, 100, &count), SPUD_SUCCESS);
	int zeros = count == 100;
	for (int i = 0; i < 100; ++i)
		zeros = zeros && gap[i] == 0;
	CHECK(zeros);

	// Sizing: shorter, then longer with zeros.
	CHECK_RESULT(sfs_file_set_size(file, 500), SPUD_SUCCESS);
	CHECK_RESULT(sfs_file_get_size(file, &value), SPUD_SUCCESS);
	CHECK(value == 500);
	CHECK_RESULT(sfs_file_set_size(file, 600), SPUD_SUCCESS);
	CHECK_RESULT(sfs_file_read_at(file, 499, gap, 101, &count), SPUD_SUCCESS);
	CHECK(count == 101 && gap[0] == bytes[499] && gap[1] == 0 && gap[100] == 0);

	CHECK_RESULT(sfs_file_flush(file, SFS_FLUSH_LEVEL_DEVICE), SPUD_SUCCESS);
	const SPUDRESULT media = sfs_file_flush(file, SFS_FLUSH_LEVEL_MEDIA);
	CHECK(media == SPUD_SUCCESS || media == SPUDRESULT_SFS_FLUSH_LEVEL_NOT_SUPPORTED);
	CHECK_RESULT(sfs_file_flush(file, SFS_FLUSH_LEVEL_NONE), SPUDRESULT_DESC_INVALID_PARAMETERS);

	// Argument checks.
	CHECK_RESULT(sfs_file_read_at(NULL, 0, back, 1, &count), SPUDRESULT_SFS_INVALID_FILE);
	CHECK_RESULT(sfs_file_read_at(file, 0, NULL, 1, &count), SPUDRESULT_NULL_OUTPUT_PARAMETER);
	CHECK_RESULT(sfs_file_read_at(file, 0, back, 1, NULL), SPUDRESULT_NULL_OUTPUT_PARAMETER);
	CHECK_RESULT(sfs_file_write_at(NULL, 0, back, 1), SPUDRESULT_SFS_INVALID_FILE);
	CHECK_RESULT(sfs_file_write_at(file, 0, NULL, 1), SPUDRESULT_NULL_DESC);
	CHECK_RESULT(sfs_file_get_size(NULL, &value), SPUDRESULT_SFS_INVALID_FILE);
	CHECK_RESULT(sfs_file_get_size(file, NULL), SPUDRESULT_NULL_OUTPUT_PARAMETER);
	CHECK_RESULT(sfs_file_set_size(NULL, 0), SPUDRESULT_SFS_INVALID_FILE);
	CHECK_RESULT(sfs_file_flush(NULL, SFS_FLUSH_LEVEL_DEVICE), SPUDRESULT_SFS_INVALID_FILE);
	sfs_file_close(file);

	// What each access refuses.
	file = open_file(path, SFS_ACCESS_READ, SFS_CREATE_OPEN_EXISTING);
	if (file) {
		CHECK_RESULT(sfs_file_write_at(file, 0, patch, 1), SPUDRESULT_SFS_NOT_WRITABLE);
		CHECK_RESULT(sfs_file_set_size(file, 0), SPUDRESULT_SFS_NOT_WRITABLE);
		CHECK_RESULT(sfs_file_flush(file, SFS_FLUSH_LEVEL_DEVICE), SPUDRESULT_SFS_NOT_WRITABLE);
		sfs_file_close(file);
	}
	file = open_file(path, SFS_ACCESS_WRITE, SFS_CREATE_OPEN_EXISTING);
	if (file) {
		CHECK_RESULT(sfs_file_read_at(file, 0, back, 1, &count), SPUDRESULT_SFS_NOT_READABLE);
		sfs_mapping mapping = NULL;
		CHECK_RESULT(sfs_file_map(file, 0, 1, &mapping), SPUDRESULT_SFS_NOT_READABLE);
		sfs_file_close(file);
	}
	CHECK_RESULT(sfs_path_remove(g_files, path), SPUD_SUCCESS);
}

static void test_map(void) {
	char path[1100];
	path_in(path, sizeof(path), "map.bin");
	const uint64_t size = 100000;
	uint8_t *bytes      = (uint8_t *)malloc((size_t)size);
	CHECK(bytes != NULL);
	if (!bytes)
		return;
	for (uint64_t i = 0; i < size; ++i)
		bytes[i] = (uint8_t)(i * 13 + 5);
	sfs_file file = open_file(path, SFS_ACCESS_WRITE, SFS_CREATE_TRUNCATE);
	if (file) {
		CHECK_RESULT(sfs_file_write_at(file, 0, bytes, size), SPUD_SUCCESS);
		sfs_file_close(file);
	}
	file = open_file(path, SFS_ACCESS_READ, SFS_CREATE_OPEN_EXISTING);
	if (!file) {
		free(bytes);
		return;
	}
	sfs_mapping mapping = (sfs_mapping)(uintptr_t)1;
	CHECK_RESULT(sfs_file_map(file, 0, 10, NULL), SPUDRESULT_NULL_OUTPUT_PARAMETER);
	CHECK_RESULT(sfs_file_map(NULL, 0, 10, &mapping), SPUDRESULT_SFS_INVALID_FILE);
	CHECK(mapping == NULL);
	CHECK_RESULT(sfs_file_map(file, 0, 0, &mapping), SPUDRESULT_ZERO_SIZE);
	CHECK_RESULT(sfs_file_map(file, size, 1, &mapping), SPUDRESULT_INDEX_OUT_OF_RANGE);
	CHECK_RESULT(sfs_file_map(file, 10, size, &mapping), SPUDRESULT_INDEX_OUT_OF_RANGE);

	sfs_mapping whole = NULL;
	sfs_mapping part  = NULL;
	CHECK_RESULT(sfs_file_map(file, 0, size, &whole), SPUD_SUCCESS);
	CHECK_RESULT(sfs_file_map(file, 12345, 54321, &part), SPUD_SUCCESS);
	sfs_file_close(file); // a mapping keeps what it needs
	if (whole) {
		CHECK(sfs_mapping_get_size(whole) == size);
		CHECK(memcmp(sfs_mapping_get_data(whole), bytes, (size_t)size) == 0);
		sfs_mapping_release(whole);
	}
	if (part) {
		CHECK(sfs_mapping_get_size(part) == 54321);
		CHECK(memcmp(sfs_mapping_get_data(part), bytes + 12345, 54321) == 0);
		sfs_mapping_release(part);
	}
	CHECK(sfs_mapping_get_data(NULL) == NULL);
	CHECK(sfs_mapping_get_size(NULL) == 0);
	sfs_mapping_release(NULL);
	free(bytes);
	CHECK_RESULT(sfs_path_remove(g_files, path), SPUD_SUCCESS);
}

static void test_paths(void) {
	char file_path[1100];
	path_in(file_path, sizeof(file_path), "info.txt");
	write_whole(file_path, "twelve bytes");

	SPUDRESULT r;
	SFS_PATH_INFO info = info_of(file_path, &r);
	CHECK(r == SPUD_SUCCESS);
	CHECK(info.kind == SFS_PATH_KIND_FILE);
	CHECK(info.size == 12);
	// Modified within a day of now.
	CHECK(info.modified_ns > (int64_t)1700000000 * 1000000000);
	info = info_of(g_dir, &r);
	CHECK(r == SPUD_SUCCESS && info.kind == SFS_PATH_KIND_DIRECTORY && info.size == 0);
	char missing[1100];
	path_in(missing, sizeof(missing), "missing.txt");
	info = info_of(missing, &r);
	CHECK(r == SPUDRESULT_SFS_NOT_FOUND);

	SFS_PATH_INFO wrong;
	memset(&wrong, 0, sizeof(wrong));
	CHECK_RESULT(sfs_path_get_info(g_files, file_path, &wrong), SPUDRESULT_STRUCT_WRONG_TYPE);
	CHECK_RESULT(sfs_path_get_info(NULL, file_path, &info), SPUDRESULT_SFS_INVALID_INSTANCE);
	CHECK_RESULT(sfs_path_get_info(g_files, NULL, &info), SPUDRESULT_SFS_NULL_PATH);
	CHECK_RESULT(sfs_path_get_info(g_files, "", &info), SPUDRESULT_SFS_NULL_PATH);
	CHECK_RESULT(sfs_path_get_info(g_files, file_path, NULL), SPUDRESULT_NULL_OUTPUT_PARAMETER);

	// Replace: onto nothing, onto a file, at each level.
	char source[1100];
	char target[1100];
	path_in(source, sizeof(source), "new.txt");
	path_in(target, sizeof(target), "doc.txt");
	const SFS_FLUSH_LEVEL levels[] = {SFS_FLUSH_LEVEL_NONE, SFS_FLUSH_LEVEL_DEVICE, SFS_FLUSH_LEVEL_MEDIA};
	const char *texts[]            = {"one", "two", "three"};
	for (int i = 0; i < 3; ++i) {
		write_whole(source, texts[i]);
		r = sfs_path_replace(g_files, source, target, levels[i]);
		CHECK(r == SPUD_SUCCESS || (levels[i] == SFS_FLUSH_LEVEL_MEDIA && r == SPUDRESULT_SFS_FLUSH_LEVEL_NOT_SUPPORTED));
		info_of(source, &r);
		CHECK(r == SPUDRESULT_SFS_NOT_FOUND);
		CHECK(holds(target, texts[i]));
	}
	CHECK_RESULT(sfs_path_replace(g_files, source, target, SFS_FLUSH_LEVEL_NONE), SPUDRESULT_SFS_NOT_FOUND);
	CHECK(holds(target, "three"));
	CHECK_RESULT(sfs_path_replace(NULL, source, target, SFS_FLUSH_LEVEL_NONE), SPUDRESULT_SFS_INVALID_INSTANCE);
	CHECK_RESULT(sfs_path_replace(g_files, NULL, target, SFS_FLUSH_LEVEL_NONE), SPUDRESULT_SFS_NULL_PATH);
	CHECK_RESULT(sfs_path_replace(g_files, source, "", SFS_FLUSH_LEVEL_NONE), SPUDRESULT_SFS_NULL_PATH);
	CHECK_RESULT(sfs_path_replace(g_files, source, target, (SFS_FLUSH_LEVEL)9), SPUDRESULT_DESC_INVALID_PARAMETERS);

	// Remove.
	CHECK_RESULT(sfs_path_remove(g_files, target), SPUD_SUCCESS);
	CHECK_RESULT(sfs_path_remove(g_files, target), SPUDRESULT_SFS_NOT_FOUND);
	CHECK_RESULT(sfs_path_remove(g_files, g_dir), SPUDRESULT_SFS_NOT_A_FILE);
	CHECK_RESULT(sfs_path_remove(NULL, target), SPUDRESULT_SFS_INVALID_INSTANCE);
	CHECK_RESULT(sfs_path_remove(g_files, NULL), SPUDRESULT_SFS_NULL_PATH);
	CHECK_RESULT(sfs_path_remove(g_files, file_path), SPUD_SUCCESS);

	// Directories, one level at a time.
	char top[1100];
	char nested[1200];
	path_in(top, sizeof(top), "a");
	snprintf(nested, sizeof(nested), "%s" SLASH "b", top);
	CHECK_RESULT(sfs_directory_create(g_files, nested), SPUDRESULT_SFS_NOT_FOUND);
	CHECK_RESULT(sfs_directory_create(g_files, top), SPUD_SUCCESS);
	CHECK_RESULT(sfs_directory_create(g_files, nested), SPUD_SUCCESS);
	CHECK_RESULT(sfs_directory_create(g_files, nested), SPUDRESULT_SFS_ALREADY_EXISTS);
	info = info_of(nested, &r);
	CHECK(r == SPUD_SUCCESS && info.kind == SFS_PATH_KIND_DIRECTORY);
	char in_the_way[1300];
	snprintf(in_the_way, sizeof(in_the_way), "%s" SLASH "file", top);
	write_whole(in_the_way, "x");
	CHECK_RESULT(sfs_directory_create(g_files, in_the_way), SPUDRESULT_SFS_ALREADY_EXISTS);
	CHECK_RESULT(sfs_directory_create(NULL, top), SPUDRESULT_SFS_INVALID_INSTANCE);
	CHECK_RESULT(sfs_directory_create(g_files, ""), SPUDRESULT_SFS_NULL_PATH);
	CHECK_RESULT(sfs_path_remove(g_files, in_the_way), SPUD_SUCCESS);
	CHECK(rmdir(nested) == 0);
	CHECK(rmdir(top) == 0);
}

/* A name outside ASCII: UTF-8 here, UTF-16 on Windows. */
static void test_unicode_name(void) {
	char path[1100];
	path_in(path, sizeof(path), "pl\xC3\xA4n \xE5\x9B\xB3.txt"); // "plän 図.txt"
	write_whole(path, "unicode");
	CHECK(holds(path, "unicode"));
	SPUDRESULT r;
	SFS_PATH_INFO info = info_of(path, &r);
	CHECK(r == SPUD_SUCCESS && info.size == 7);
	CHECK_RESULT(sfs_path_remove(g_files, path), SPUD_SUCCESS);
}

/* Two threads reading one file at once, each at its own offsets. */
struct reader {
	sfs_file file;
	uint64_t start;
	int ok;
};

#if SPUDLIB_PLATFORM_WIN32
static unsigned __stdcall read_half(void *argument)
#else
static void *read_half(void *argument)
#endif
{
	struct reader *reader = (struct reader *)argument;
	reader->ok            = 1;
	for (int round = 0; round < 200; ++round) {
		for (uint64_t at = reader->start; at < reader->start + 4096; at += 256) {
			uint8_t buf[256];
			uint64_t count = 0;
			if (sfs_file_read_at(reader->file, at, buf, sizeof(buf), &count) != SPUD_SUCCESS || count != 256) {
				reader->ok = 0;
				continue;
			}
			for (int i = 0; i < 256; ++i)
				reader->ok = reader->ok && buf[i] == (uint8_t)((at + (uint64_t)i) * 3);
		}
	}
	return 0;
}

static void test_two_readers(void) {
	char path[1100];
	path_in(path, sizeof(path), "shared.bin");
	uint8_t bytes[8192];
	for (int i = 0; i < 8192; ++i)
		bytes[i] = (uint8_t)(i * 3);
	sfs_file file = open_file(path, SFS_ACCESS_WRITE, SFS_CREATE_TRUNCATE);
	if (file) {
		CHECK_RESULT(sfs_file_write_at(file, 0, bytes, sizeof(bytes)), SPUD_SUCCESS);
		sfs_file_close(file);
	}
	file = open_file(path, SFS_ACCESS_READ, SFS_CREATE_OPEN_EXISTING);
	if (!file)
		return;
	struct reader a = {file, 0, 0};
	struct reader b = {file, 4096, 0};
#if SPUDLIB_PLATFORM_WIN32
	HANDLE ta = (HANDLE)_beginthreadex(NULL, 0, read_half, &a, 0, NULL);
	HANDLE tb = (HANDLE)_beginthreadex(NULL, 0, read_half, &b, 0, NULL);
	WaitForSingleObject(ta, INFINITE);
	WaitForSingleObject(tb, INFINITE);
	CloseHandle(ta);
	CloseHandle(tb);
#else
	pthread_t ta;
	pthread_t tb;
	pthread_create(&ta, NULL, read_half, &a);
	pthread_create(&tb, NULL, read_half, &b);
	pthread_join(ta, NULL);
	pthread_join(tb, NULL);
#endif
	CHECK(a.ok && b.ok);
	sfs_file_close(file);
	CHECK_RESULT(sfs_path_remove(g_files, path), SPUD_SUCCESS);
}

int main(void) {
#if SPUDLIB_PLATFORM_WIN32
	const char *tmp = getenv("TEMP");
	if (!tmp)
		tmp = ".";
	snprintf(g_dir, sizeof(g_dir), "%s\\spudfiles_test.%u", tmp, (unsigned)_getpid());
	if (_mkdir(g_dir) != 0) {
		printf("spudfiles_test: skipped, no temporary directory\n");
		return EXIT_SKIPPED;
	}
#else
	const char *tmp = getenv("TMPDIR");
	if (!tmp || !tmp[0])
		tmp = "/tmp";
	snprintf(g_dir, sizeof(g_dir), "%s%sspudfiles_test.XXXXXX", tmp, tmp[strlen(tmp) - 1] == '/' ? "" : "/");
	if (!mkdtemp(g_dir)) {
		printf("spudfiles_test: skipped, no temporary directory\n");
		return EXIT_SKIPPED;
	}
#endif
	printf("spudfiles_test: in %s\n", g_dir);

	spudtest_allocator tracker;
	SPUD_ALLOCATOR allocator = spudtest_allocator_make(&tracker);
	test_instance(&allocator);
	sfs_instance_desc desc;
	memset(&desc, 0, sizeof(desc));
	desc.base.type = SPUD_STRUCT_TYPE_FILES_INSTANCE_DESC;
	desc.allocator = &allocator;
	CHECK_RESULT(sfs_instance_create(&desc, &g_files), SPUD_SUCCESS);
	if (g_files) {
		test_open_checks();
		test_create_modes();
		test_read_write();
		test_map();
		test_paths();
		test_unicode_name();
		test_two_readers();
		sfs_instance_destroy(g_files);
	}
	CHECK(spudtest_allocator_clean(&tracker));

	CHECK(rmdir(g_dir) == 0);
	if (failures) {
		fprintf(stderr, "spudfiles_test: %d failure(s)\n", failures);
		return 1;
	}
	printf("spudfiles_test: all passed\n");
	return 0;
}
