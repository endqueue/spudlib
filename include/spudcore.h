/**
 * @file spudcore.h
 * @brief SpudCore: what every SpudLib module shares.
 *
 * The SPUDRESULT enum all modules return, the library's version, the
 * fixed-width integer limits, struct bases, the caller's allocator and the
 * debug-name functions. Platform-agnostic: nothing here calls a platform
 * API.
 */

#ifndef SPUDCORE_H
#define SPUDCORE_H

#include <stddef.h>
#include <stdint.h>

#if __cplusplus
extern "C" {
#endif

/**
 * @name Unsigned integer limits
 *
 * The same values as UINT8_MAX, UINT16_MAX, UINT32_MAX and UINT64_MAX in
 * stdint.h. Defined here because VSCode IntelliSense on Windows does not
 * recognise the suffixes ('ui32', 'ui64', ...) that stdint.h ends those
 * definitions with.
 * @{
 */
/** @brief Largest value of a uint8_t. */
#define SPUD_UINT8_MAX 0xFFU
/** @brief Largest value of a uint16_t. */
#define SPUD_UINT16_MAX 0xFFFFU
/** @brief Largest value of a uint32_t. */
#define SPUD_UINT32_MAX 0xFFFFFFFFU
/** @brief Largest value of a uint64_t. */
#define SPUD_UINT64_MAX 0xFFFFFFFFFFFFFFFFULL
/** @} */

/**
 * @defgroup spud_version Version
 * @brief Which spudlib a caller was built against, and which it runs on.
 *
 * One version for the whole library: spudlib is built and linked as one
 * archive, so its modules can't be mixed from different versions.
 *
 * What a version promises, from 1.0.0 on:
 *
 * | Change in | May add | May change or remove |
 * |-----------|---------|----------------------|
 * | patch     | nothing | nothing: fixes only, no header change but comments |
 * | minor     | functions, SPUDRESULT values, SPUD_STRUCT_TYPE values, chained structs, enumerators | nothing |
 * | major     | anything | anything |
 *
 * "Released" means a commit tagged vMAJOR.MINOR.PATCH. Before 1.0.0, a
 * minor may change anything; nothing is promised.
 *
 * A struct that can be chained says in its comment which version first
 * takes it ("Since 1.2.0"), so that a caller can ask spud_get_version()
 * before chaining it. A caller that doesn't ask loses nothing but the
 * question: a spudlib that doesn't take a struct refuses the chain with
 * SPUDRESULT_STRUCT_UNKNOWN_IN_CHAIN.
 * @{
 */

/** @brief Packs a version into one number that compares with < and >:
 * 10 bits of major, 10 of minor, 12 of patch (Vulkan's layout, without its
 * variant bits). */
#define SPUDLIB_MAKE_VERSION(major, minor, patch) \
	((((uint32_t)(major)) << 22U) | (((uint32_t)(minor)) << 12U) | ((uint32_t)(patch)))
/** @brief The major part of a packed version. */
#define SPUDLIB_VERSION_GET_MAJOR(version) ((uint32_t)(version) >> 22U)
/** @brief The minor part of a packed version. */
#define SPUDLIB_VERSION_GET_MINOR(version) (((uint32_t)(version) >> 12U) & 0x3FFU)
/** @brief The patch part of a packed version. */
#define SPUDLIB_VERSION_GET_PATCH(version) ((uint32_t)(version) & 0xFFFU)

/* The one place the version is written. CMakeLists.txt reads these three
 * lines, so each stays "#define NAME <number>" and nothing else. */
/** @brief The major version of these headers. */
#define SPUDLIB_VERSION_MAJOR 0
/** @brief The minor version of these headers. */
#define SPUDLIB_VERSION_MINOR 1
/** @brief The patch version of these headers. */
#define SPUDLIB_VERSION_PATCH 0

/** @brief The version of the headers the caller is compiled against. */
#define SPUDLIB_VERSION \
	SPUDLIB_MAKE_VERSION(SPUDLIB_VERSION_MAJOR, SPUDLIB_VERSION_MINOR, SPUDLIB_VERSION_PATCH)

/**
 * @brief Returns the version of the spudlib the caller is running on: the
 * SPUDLIB_VERSION it was compiled with.
 *
 * It can differ from the caller's own SPUDLIB_VERSION only when spudlib was
 * built separately from the caller. Which one to accept is the caller's
 * decision; spudlib refuses no caller for its version.
 *
 * @return The packed version (SPUDLIB_MAKE_VERSION). Never 0.
 */
uint32_t spud_get_version(void);

/** @} */

/**
 * @brief The result of a SpudLib call, shared by every module.
 *
 * SPUD_SUCCESS is 0 and the only success value; every other enumerator names
 * one specific failure. Test a result with SPUDFAIL() and print it with
 * spudresult_str().
 *
 * The values are grouped by module: 1-16 are general, 199-354 are SpudGPU
 * (`SPUDRESULT_GPU_*`), 1001-1007 SpudFiles (`SPUDRESULT_SFS_*`), 2001-2026
 * SpudMemory (`SPUDRESULT_SMEM_*`), 3001-3033 SpudNet (`SPUDRESULT_SPUDNET_*`),
 * 4001-4012 SpudAudio (`SPUDRESULT_SAUD_*`) and 5001-5005 SpudInput
 * (`SPUDRESULT_SPUDINPUT_*`). A value is never reused for
 * a different meaning.
 */
typedef enum SPUDRESULT {
	/** The call succeeded. */
	SPUD_SUCCESS                         = 0,
	/** The call failed and no more specific result applies. */
	SPUDRESULT_GENERAL_FAILURE           = 1,
	/** A call into the platform's own API failed. */
	SPUDRESULT_API_SPECIFIC_FAILURE      = 2,
	/** The native API asked for is not one this build can use. */
	SPUDRESULT_INVALID_API               = 3,
	/** A desc pointer is NULL. */
	SPUDRESULT_NULL_DESC                 = 4,
	/** An output pointer is NULL. */
	SPUDRESULT_NULL_OUTPUT_PARAMETER     = 5,
	/** A desc holds a value the call cannot accept. */
	SPUDRESULT_DESC_INVALID_PARAMETERS   = 6,
	/** An index is past the end of what it indexes. */
	SPUDRESULT_INDEX_OUT_OF_RANGE        = 7,
	/** A size or count is zero. */
	SPUDRESULT_ZERO_SIZE                 = 8,
	/** An allocation failed. */
	SPUDRESULT_OUT_OF_MEMORY             = 9,
	/** The call, or the option asked of it, is not implemented yet. */
	SPUDRESULT_NOT_IMPLEMENTED_YET       = 10,
	/** An object handle is NULL. */
	SPUDRESULT_NULL_OBJECT               = 11,
	/** A struct's `base.type` is not the type the call takes there (see
	 * SPUD_STRUCT_BASE). 0 is never a valid type. */
	SPUDRESULT_STRUCT_WRONG_TYPE         = 12,
	/** A struct in a chain is of a type the call does not take in that
	 * chain, or one this build does not know. Never skipped. */
	SPUDRESULT_STRUCT_UNKNOWN_IN_CHAIN   = 13,
	/** One type appears twice in a chain. */
	SPUDRESULT_STRUCT_DUPLICATE_IN_CHAIN = 14,
	/** A desc's allocator pointer is NULL (see SPUD_ALLOCATOR). */
	SPUDRESULT_NULL_ALLOCATOR            = 15,
	/** An allocator's `allocate` or `free` is NULL. */
	SPUDRESULT_INVALID_ALLOCATOR         = 16,

	SPUDRESULT_GPU_DEVICE_ENUMERATION_FAILURE                  = 199,
	SPUDRESULT_GPU_INVALID_INSTANCE                            = 200,
	SPUDRESULT_GPU_INVALID_DEVICE                              = 201,
	SPUDRESULT_GPU_INVALID_COMMAND_QUEUE                       = 202,
	SPUDRESULT_GPU_INVALID_COMMAND_ALLOCATOR                   = 203,
	SPUDRESULT_GPU_INVALID_COMMAND_LIST                        = 204,
	SPUDRESULT_GPU_INVALID_SWAP_CHAIN                          = 205,
	SPUDRESULT_GPU_INVALID_SURFACE                             = 206,
	SPUDRESULT_GPU_INVALID_FORMAT                              = 207,
	SPUDRESULT_GPU_INVALID_WINDOW_HANDLE                       = 208,
	SPUDRESULT_GPU_INVALID_DISPLAY_HANDLE                      = 209,
	SPUDRESULT_GPU_INVALID_COMMAND_LIST_TYPE                   = 210,
	SPUDRESULT_GPU_INVALID_MEMORY_KIND                         = 220,
	SPUDRESULT_GPU_INVALID_BUFFER_USAGE                        = 221,
	SPUDRESULT_GPU_INVALID_IMAGE_USAGE                         = 222,
	SPUDRESULT_GPU_INVALID_BUFFER_STRIDE                       = 223,
	SPUDRESULT_GPU_INVALID_INDEX_STRIDE                        = 224,
	SPUDRESULT_GPU_MEMORY_KIND_NOT_SUPPORTED                   = 225,
	SPUDRESULT_GPU_TOO_MANY_COLOR_ATTACHMENTS                  = 226,
	SPUDRESULT_GPU_DEPTH_BIAS_CLAMP_NOT_SUPPORTED              = 227,
	SPUDRESULT_GPU_INDEPENDENT_BLEND_NOT_SUPPORTED             = 228,
	SPUDRESULT_GPU_TIMESTAMPS_NOT_SUPPORTED                    = 229,
	SPUDRESULT_GPU_INVALID_BUFFER                              = 230,
	SPUDRESULT_GPU_INVALID_BUFFER_VIEW                         = 231,
	SPUDRESULT_GPU_INVALID_IMAGE                               = 232,
	SPUDRESULT_GPU_INVALID_IMAGE_VIEW                          = 233,
	SPUDRESULT_GPU_INVALID_IMAGE_VIEW_TYPE                     = 234,
	SPUDRESULT_GPU_INVALID_IMAGE_TYPE                          = 235,
	SPUDRESULT_GPU_ZERO_BUFFER_SIZE                            = 236,
	SPUDRESULT_GPU_INVALID_BUFFER_SIZE                         = 237,
	SPUDRESULT_GPU_BUFFER_OR_IMAGE_VIEW_RANGE_OUT_OF_SCOPE     = 238,
	SPUDRESULT_GPU_MAP_OUT_OF_RANGE                            = 239,
	SPUDRESULT_GPU_INVALID_HEAP                                = 240,
	SPUDRESULT_GPU_INVALID_HEAP_RESOURCE_CLASS                 = 241,
	/** A heap's size is not a multiple of SPUDGPU_HEAP_SIZE_GRANULARITY. */
	SPUDRESULT_GPU_INVALID_HEAP_SIZE                           = 242,
	/** A resource is not of the heap's memory kind or resource class. */
	SPUDRESULT_GPU_HEAP_MISMATCH                               = 243,
	/** A placement's offset is not a multiple of the resource's alignment. */
	SPUDRESULT_GPU_PLACEMENT_MISALIGNED                        = 244,
	/** A placement runs past the end of its heap. */
	SPUDRESULT_GPU_PLACEMENT_OUT_OF_RANGE                      = 245,
	SPUDRESULT_GPU_INVALID_TIMESTAMP_SET                       = 246,
	SPUDRESULT_GPU_INVALID_SEMAPHORE                           = 250,
	SPUDRESULT_GPU_INVALID_FENCE                               = 251,
	SPUDRESULT_GPU_FENCE_WAIT_TIMED_OUT                        = 252,
	SPUDRESULT_GPU_INVALID_SHADER_STAGE                        = 280,
	SPUDRESULT_GPU_INVALID_PRIMITIVE_TOPOLOGY                  = 281,
	SPUDRESULT_GPU_INVALID_CULL_MODE                           = 282,
	SPUDRESULT_GPU_INVALID_COMPARE_OP                          = 283,
	SPUDRESULT_GPU_INVALID_BLEND_FACTOR                        = 290,
	SPUDRESULT_GPU_INVALID_BLEND_OP                            = 291,
	SPUDRESULT_GPU_INVALID_SHADER_MODULE                       = 300,
	SPUDRESULT_GPU_INVALID_SHADER_PIPELINE                     = 301,
	SPUDRESULT_GPU_INVALID_COMPUTE_MODULE                      = 302,
	SPUDRESULT_GPU_INVALID_COMPUTE_PIPELINE                    = 303,
	SPUDRESULT_GPU_NULL_SPIRV                                  = 304,
	SPUDRESULT_GPU_INVALID_SPIRV_ALIGNMENT                     = 305,
	SPUDRESULT_GPU_VERTEX_AND_FRAGMENT_SHADER_REQUIRED         = 306,
	SPUDRESULT_GPU_SHADER_COMPILATION_FAILED                   = 307,
	SPUDRESULT_GPU_INVALID_DESCRIPTOR_TYPE                     = 310,
	SPUDRESULT_GPU_INVALID_DESCRIPTOR_POOL_SIZE                = 311,
	SPUDRESULT_GPU_INVALID_DESCRIPTOR_POOL                     = 312,
	SPUDRESULT_GPU_INVALID_DESCRIPTOR_SET_LAYOUT               = 313,
	SPUDRESULT_GPU_INVALID_DESCRIPTOR_SET                      = 314,
	SPUDRESULT_GPU_INVALID_DESCRIPTOR_BUFFER_INFO              = 315,
	SPUDRESULT_GPU_INVALID_DESCRIPTOR_IMAGE_INFO               = 316,
	SPUDRESULT_GPU_INVALID_WRITE_DESCRIPTOR_SET                = 317,
	SPUDRESULT_GPU_TOO_MANY_DESCRIPTOR_BINDINGS                = 318,
	SPUDRESULT_GPU_TOO_MANY_DESCRIPTOR_POOLS                   = 319,
	SPUDRESULT_GPU_ZERO_DESCRIPTOR_SET_LAYOUTS                 = 320,
	SPUDRESULT_GPU_TOO_MANY_DESCRIPTOR_SET_LAYOUTS             = 321,
	SPUDRESULT_GPU_INTERNAL_DESCRIPTOR_SET_ALLOCATION_FAIL     = 322,
	SPUDRESULT_GPU_CANNOT_RESOLVE_API_SPECIFIC_DESCRIPTOR_TYPE = 323,
	SPUDRESULT_GPU_BINDLESS_OUT_OF_SLOTS                       = 324,

	/**
	 * @name Extension not supported by the device
	 *
	 * One SPUDRESULT_GPU_EXT_<NAME>_NOT_SUPPORTED per SPUDGPU_EXT_<NAME>
	 * compile-time capability macro in spudgpu.h. Returned by that
	 * extension's runtime entry points when the active backend compiles the
	 * extension in (the macro is 1) but the specific device/driver still
	 * doesn't support it. Never returned when the macro is 0: the extension's
	 * functions aren't declared at all in that case, so calling one is a
	 * compile/link error, not a runtime result.
	 * @{
	 */
	SPUDRESULT_GPU_EXT_BINDLESS_DESCRIPTOR_INDEXING_NOT_SUPPORTED = 325,
	SPUDRESULT_GPU_EXT_MESH_SHADING_NOT_SUPPORTED                 = 326,
	SPUDRESULT_GPU_EXT_DEPTH_BOUNDS_TEST_NOT_SUPPORTED            = 327,
	/** @} */

	SPUDRESULT_GPU_INVALID_RESOURCE_STATE                      = 350,
	SPUDRESULT_GPU_INVALID_IMAGE_LAYOUT                        = 351,
	SPUDRESULT_GPU_INVALID_PIPELINE_STAGE                      = 352,
	SPUDRESULT_GPU_INVALID_BUFFER_BARRIER                      = 353,
	SPUDRESULT_GPU_INVALID_IMAGE_BARRIER                       = 354,

	SPUDRESULT_SFS_NULL_PATH = 1001,
	SPUDRESULT_SFS_INVALID_FILE = 1002,
	SPUDRESULT_SFS_INVALID_MAPPING = 1003,
	SPUDRESULT_SFS_NOT_READABLE = 1004, /**< Mapping a file not opened for reading. */
	SPUDRESULT_SFS_DIFFERENT_VOLUME = 1005, /**< Replace across volumes (EXDEV / ERROR_NOT_SAME_DEVICE). */
	SPUDRESULT_SFS_IN_USE = 1006, /**< Replace target in use or read-only (Windows). */
	SPUDRESULT_SFS_FLUSH_LEVEL_NOT_SUPPORTED = 1007, /**< The file system can't flush to the level asked for. */

	SPUDRESULT_SMEM_ZERO_CAPACITY = 2001,
	SPUDRESULT_SMEM_INSUFFICIENT_CAPACITY = 2002,
	SPUDRESULT_SMEM_PLAT_COMMIT_FAIL = 2003,
	SPUDRESULT_SMEM_INVALID_ARENA = 2026,

	SPUDRESULT_SPUDNET_STARTUP_FAILED        = 3001, /**< spudnet_instance_create(): the platform's networking wouldn't start. */
	SPUDRESULT_SPUDNET_INVALID_TCP_SOCKET    = 3002,
	SPUDRESULT_SPUDNET_RESOLVE_FAILED        = 3003,
	SPUDRESULT_SPUDNET_CONNECT_FAILED        = 3004,
	SPUDRESULT_SPUDNET_LISTEN_FAILED         = 3005,
	SPUDRESULT_SPUDNET_ACCEPT_FAILED         = 3006,
	SPUDRESULT_SPUDNET_SEND_FAILED           = 3007,
	SPUDRESULT_SPUDNET_RECV_FAILED           = 3008,
	/* 3009 and 3010 (WOULD_BLOCK, SET_BLOCKING_FAILED) went with SpudNet's
	 * non-blocking mode and are not reused. */
	SPUDRESULT_SPUDNET_INVALID_URL           = 3011, /**< Not a URL, or a scheme the call doesn't carry. */
	SPUDRESULT_SPUDNET_INVALID_HTTP_CLIENT   = 3012,
	SPUDRESULT_SPUDNET_INVALID_WEBSOCKET     = 3013,
	SPUDRESULT_SPUDNET_TLS_FAILED            = 3014, /**< Handshake or certificate check failed. */
	SPUDRESULT_SPUDNET_TIMED_OUT             = 3015, /**< The call's own time limit ran out, and nothing else. */
	SPUDRESULT_SPUDNET_HTTP_FAILED           = 3016, /**< An HTTP transfer's start or its wait for the response failed. */
	SPUDRESULT_SPUDNET_WS_UPGRADE_REFUSED    = 3017, /**< The server answered, but not with 101. */
	SPUDRESULT_SPUDNET_WS_CLOSED             = 3018, /**< Closed by a close frame, either side's. */
	SPUDRESULT_SPUDNET_WS_MESSAGE_TOO_BIG    = 3019, /**< Over the connect desc's max_message_size. */
	SPUDRESULT_SPUDNET_UNSUPPORTED           = 3020, /**< This build's platform stack doesn't have it. */
	SPUDRESULT_SPUDNET_ABORTED               = 3022, /**< The object's _abort was called. */
	SPUDRESULT_SPUDNET_INVALID_TCP_LISTENER  = 3024,
	SPUDRESULT_SPUDNET_INVALID_ADDRESS       = 3025, /**< Not a numeric IPv4 or IPv6 address. */
	SPUDRESULT_SPUDNET_ADDRESS_IN_USE        = 3026, /**< Another listener already has that address and port. */
	SPUDRESULT_SPUDNET_INVALID_WAIT_SET      = 3027,
	SPUDRESULT_SPUDNET_ALREADY_IN_WAIT_SET   = 3028, /**< The object is in this set already, or a WebSocket in another. */
	SPUDRESULT_SPUDNET_INVALID_RESOLVER      = 3029, /**< NULL, or one whose lookup has already been run. */
	SPUDRESULT_SPUDNET_SYSTEM_TIMED_OUT      = 3030, /**< TCP: the operating system gave up waiting for the other end. */
	SPUDRESULT_SPUDNET_STACK_TIMED_OUT       = 3031, /**< HTTP, WebSocket: the client stack gave up waiting, or says something under it did. */
	SPUDRESULT_SPUDNET_INVALID_HTTP_TRANSFER = 3032, /**< NULL, or a call out of the transfer's order of use. */
	SPUDRESULT_SPUDNET_INVALID_INSTANCE      = 3033, /**< NULL, or not the instance the other object was made with. */

	SPUDRESULT_SAUD_INVALID_INSTANCE      = 4001,
	SPUDRESULT_SAUD_INVALID_DEVICE        = 4002,
	SPUDRESULT_SAUD_INVALID_STREAM        = 4003,
	SPUDRESULT_SAUD_FORMAT_NOT_SUPPORTED  = 4004,
	SPUDRESULT_SAUD_EXCLUSIVE_UNAVAILABLE = 4005,
	SPUDRESULT_SAUD_DEVICE_LOST           = 4006,
	SPUDRESULT_SAUD_PERIOD_OUT_OF_RANGE   = 4007,
	SPUDRESULT_SAUD_BUFFER_OUT_OF_RANGE   = 4008,
	SPUDRESULT_SAUD_THREAD_PRIORITY_UNSUPPORTED = 4009,
	SPUDRESULT_SAUD_THREAD_PRIORITY_DENIED      = 4010,
	SPUDRESULT_SAUD_SAMPLE_RATE_NOT_SETTABLE    = 4011,
	SPUDRESULT_SAUD_OS_CONVERSION_UNAVAILABLE   = 4012,

	SPUDRESULT_SPUDINPUT_INVALID_INSTANCE      = 5001,
	SPUDRESULT_SPUDINPUT_GAMEPAD_NOT_CONNECTED = 5002, /**< The id names no gamepad as of the last poll. */
	SPUDRESULT_SPUDINPUT_UNSUPPORTED           = 5003, /**< The native API doesn't offer the choice asked of it. */
	SPUDRESULT_SPUDINPUT_FEATURE_NOT_PRESENT   = 5004, /**< The device doesn't have what the call is about. */
	SPUDRESULT_SPUDINPUT_ACCESS_DENIED         = 5005, /**< The platform won't let this process read or write the device. */

} SPUDRESULT;

/**
 * @brief Tests whether a SPUDRESULT is a failure.
 *
 * @param[in] sr The SPUDRESULT to test. Evaluated once.
 *
 * @return Nonzero if `sr` is anything other than SPUD_SUCCESS, 0 if it is
 *         SPUD_SUCCESS.
 */
#define SPUDFAIL(sr) ((sr) != SPUD_SUCCESS)

/**
 * @defgroup spud_struct_base Struct bases
 * @brief How a struct that may gain fields is extended without changing
 * its layout.
 *
 * An extensible struct's first member is a SPUD_STRUCT_BASE (one the
 * caller fills in) or a SPUD_STRUCT_BASE_OUT (one SpudLib fills in), named
 * `base`. `base.type` says which struct it is; `base.next` points to the
 * next struct in its chain, or is NULL. A struct's layout never changes
 * once it is released: a new field comes as a new struct with a base of
 * its own, which the caller chains on. A struct with no base is frozen and
 * never changes at all; a new need gets a new type.
 *
 * - The caller sets every `base.type` in a chain, the root's included.
 *   A type that isn't the one the call takes is refused with
 *   SPUDRESULT_STRUCT_WRONG_TYPE.
 * - A chained struct the call doesn't take there, or this build doesn't
 *   know, is refused with SPUDRESULT_STRUCT_UNKNOWN_IN_CHAIN, never
 *   skipped: skipping it would decide for the caller that it didn't
 *   matter. One type twice in a chain is refused with
 *   SPUDRESULT_STRUCT_DUPLICATE_IN_CHAIN.
 * - Leaving a chained struct out means that addition isn't used. Each
 *   chained struct's fields have no defaults, as no desc's do.
 *
 * ```c
 * spudgpu_image_desc desc = {
 * 	.base = { SPUD_STRUCT_TYPE_GPU_IMAGE_DESC, NULL },
 * 	...
 * };
 * ```
 *
 * The member is nested, not two loose fields, so that a pointer to any based
 * struct converts to a pointer to its base and back (C11 6.7.2.1p15): a
 * chain is walked through SPUD_STRUCT_BASE pointers with defined behaviour.
 * @{
 */

/**
 * @brief Which struct a based struct is.
 *
 * Numbered in blocks per module, as SPUDRESULT is: 1000+ SpudGPU, 2000+
 * SpudAudio, 3000+ SpudNet, 4000+ SpudInput, 5000+ SpudFiles. A value is
 * never renumbered or reused, a retired struct's included.
 */
typedef enum SPUD_STRUCT_TYPE {
	/** Never valid: a struct left zeroed is refused, not misread. */
	SPUD_STRUCT_TYPE_UNKNOWN  = 0,

	/* SpudGPU */
	/** SPUDGPU_DEVICE_PROPERTIES (SPUD_STRUCT_BASE_OUT). */
	SPUD_STRUCT_TYPE_GPU_DEVICE_PROPERTIES          = 1000,
	/** spudgpu_submit_desc (SPUD_STRUCT_BASE). */
	SPUD_STRUCT_TYPE_GPU_SUBMIT_DESC                = 1001,
	/** spudgpu_command_allocator_desc (SPUD_STRUCT_BASE). */
	SPUD_STRUCT_TYPE_GPU_COMMAND_ALLOCATOR_DESC     = 1002,
	/** spudgpu_buffer_desc (SPUD_STRUCT_BASE). */
	SPUD_STRUCT_TYPE_GPU_BUFFER_DESC                = 1003,
	/** spudgpu_buffer_view_desc (SPUD_STRUCT_BASE). */
	SPUD_STRUCT_TYPE_GPU_BUFFER_VIEW_DESC           = 1004,
	/** spudgpu_image_desc (SPUD_STRUCT_BASE). */
	SPUD_STRUCT_TYPE_GPU_IMAGE_DESC                 = 1005,
	/** spudgpu_image_view_desc (SPUD_STRUCT_BASE). */
	SPUD_STRUCT_TYPE_GPU_IMAGE_VIEW_DESC            = 1006,
	/** spudgpu_heap_desc (SPUD_STRUCT_BASE). */
	SPUD_STRUCT_TYPE_GPU_HEAP_DESC                  = 1007,
	/** spudgpu_placement_requirements (SPUD_STRUCT_BASE_OUT). */
	SPUD_STRUCT_TYPE_GPU_PLACEMENT_REQUIREMENTS     = 1008,
	/** spudgpu_buffer_barrier (SPUD_STRUCT_BASE). */
	SPUD_STRUCT_TYPE_GPU_BUFFER_BARRIER             = 1009,
	/** spudgpu_image_barrier (SPUD_STRUCT_BASE). */
	SPUD_STRUCT_TYPE_GPU_IMAGE_BARRIER              = 1010,
	/** spudgpu_swap_chain_desc (SPUD_STRUCT_BASE). */
	SPUD_STRUCT_TYPE_GPU_SWAP_CHAIN_DESC            = 1011,
	/** spudgpu_descriptor_set_layout_desc (SPUD_STRUCT_BASE). */
	SPUD_STRUCT_TYPE_GPU_DESCRIPTOR_SET_LAYOUT_DESC = 1012,
	/** spudgpu_descriptor_pool_desc (SPUD_STRUCT_BASE). */
	SPUD_STRUCT_TYPE_GPU_DESCRIPTOR_POOL_DESC       = 1013,
	/** spudgpu_descriptor_set_desc (SPUD_STRUCT_BASE). */
	SPUD_STRUCT_TYPE_GPU_DESCRIPTOR_SET_DESC        = 1014,
	/** spudgpu_sampler_desc (SPUD_STRUCT_BASE). */
	SPUD_STRUCT_TYPE_GPU_SAMPLER_DESC               = 1015,
	/** spudgpu_write_descriptor_set (SPUD_STRUCT_BASE). */
	SPUD_STRUCT_TYPE_GPU_WRITE_DESCRIPTOR_SET       = 1016,
	/** spudgpu_bindless_capabilities (SPUD_STRUCT_BASE_OUT). */
	SPUD_STRUCT_TYPE_GPU_BINDLESS_CAPABILITIES      = 1017,
	/** spudgpu_shader_module_desc (SPUD_STRUCT_BASE). */
	SPUD_STRUCT_TYPE_GPU_SHADER_MODULE_DESC         = 1018,
	/** spudgpu_shader_pipeline_desc (SPUD_STRUCT_BASE). */
	SPUD_STRUCT_TYPE_GPU_SHADER_PIPELINE_DESC       = 1019,
	/** spudgpu_mesh_shading_capabilities (SPUD_STRUCT_BASE_OUT). */
	SPUD_STRUCT_TYPE_GPU_MESH_SHADING_CAPABILITIES  = 1020,
	/** spudgpu_depth_bounds_capabilities (SPUD_STRUCT_BASE_OUT). */
	SPUD_STRUCT_TYPE_GPU_DEPTH_BOUNDS_CAPABILITIES  = 1021,
	/** spudgpu_compute_pipeline_desc (SPUD_STRUCT_BASE). */
	SPUD_STRUCT_TYPE_GPU_COMPUTE_PIPELINE_DESC      = 1022,
	/** spudgpu_rendering_begin_desc (SPUD_STRUCT_BASE). */
	SPUD_STRUCT_TYPE_GPU_RENDERING_BEGIN_DESC       = 1023,
	/** spudgpu_bundle_inheritance_desc (SPUD_STRUCT_BASE). */
	SPUD_STRUCT_TYPE_GPU_BUNDLE_INHERITANCE_DESC    = 1024,
	/** spudgpu_instance_desc (SPUD_STRUCT_BASE). */
	SPUD_STRUCT_TYPE_GPU_INSTANCE_DESC              = 1025,

	/* SpudAudio */
	/** SPUDAUDIO_DEVICE_PROPERTIES (SPUD_STRUCT_BASE_OUT). */
	SPUD_STRUCT_TYPE_AUDIO_DEVICE_PROPERTIES        = 2000,
	/** SPUDAUDIO_TIMING_CAPS (SPUD_STRUCT_BASE_OUT). */
	SPUD_STRUCT_TYPE_AUDIO_TIMING_CAPS              = 2001,
	/** SPUDAUDIO_STREAM_DESC (SPUD_STRUCT_BASE). */
	SPUD_STRUCT_TYPE_AUDIO_STREAM_DESC              = 2002,
	/** SPUDAUDIO_STREAM_CONFIG (SPUD_STRUCT_BASE_OUT). */
	SPUD_STRUCT_TYPE_AUDIO_STREAM_CONFIG            = 2003,
	/** SPUDAUDIO_INSTANCE_DESC (SPUD_STRUCT_BASE). */
	SPUD_STRUCT_TYPE_AUDIO_INSTANCE_DESC            = 2004,

	/* SpudNet */
	/** spudnet_resolver_desc (SPUD_STRUCT_BASE). */
	SPUD_STRUCT_TYPE_NET_RESOLVER_DESC              = 3000,
	/** spudnet_tcp_listener_desc (SPUD_STRUCT_BASE). */
	SPUD_STRUCT_TYPE_NET_TCP_LISTENER_DESC          = 3001,
	/** spudnet_tcp_socket_connect_desc (SPUD_STRUCT_BASE). */
	SPUD_STRUCT_TYPE_NET_TCP_SOCKET_CONNECT_DESC    = 3002,
	/** spudnet_http_client_desc (SPUD_STRUCT_BASE). */
	SPUD_STRUCT_TYPE_NET_HTTP_CLIENT_DESC           = 3003,
	/** spudnet_http_transfer_desc (SPUD_STRUCT_BASE). */
	SPUD_STRUCT_TYPE_NET_HTTP_TRANSFER_DESC         = 3004,
	/** spudnet_websocket_connect_desc (SPUD_STRUCT_BASE). */
	SPUD_STRUCT_TYPE_NET_WEBSOCKET_CONNECT_DESC     = 3005,
	/** spudnet_instance_desc (SPUD_STRUCT_BASE). */
	SPUD_STRUCT_TYPE_NET_INSTANCE_DESC              = 3006,

	/* SpudInput */
	/** SPUDINPUT_INSTANCE_DESC (SPUD_STRUCT_BASE). */
	SPUD_STRUCT_TYPE_INPUT_INSTANCE_DESC            = 4000,
	/** SPUDINPUT_GAMEPAD_PROPERTIES (SPUD_STRUCT_BASE_OUT). */
	SPUD_STRUCT_TYPE_INPUT_GAMEPAD_PROPERTIES       = 4001,

	/** Keeps the enum 32 bits wide on every compiler. Never a valid type. */
	SPUD_STRUCT_TYPE_MAX_ENUM = 0x7FFFFFFF
} SPUD_STRUCT_TYPE;

/** @brief The first member, `base`, of every extensible struct the caller
 * fills in. */
/* Frozen: the base every based struct starts with. It never changes; a new need gets a new type. */
typedef struct SPUD_STRUCT_BASE {
	/** This struct's own type. */
	SPUD_STRUCT_TYPE type;
	/** The next struct in the chain, or NULL. SpudLib only reads it. */
	const struct SPUD_STRUCT_BASE *next;
} SPUD_STRUCT_BASE;

/** @brief The first member, `base`, of every extensible struct SpudLib
 * fills in for the caller. The caller builds the chain and sets each
 * `type`; SpudLib writes the other fields and leaves `type` and `next` as
 * they are. */
/* Frozen: the base every based struct starts with. It never changes; a new need gets a new type. */
typedef struct SPUD_STRUCT_BASE_OUT {
	/** This struct's own type. */
	SPUD_STRUCT_TYPE type;
	/** The next struct in the chain, or NULL. */
	struct SPUD_STRUCT_BASE_OUT *next;
} SPUD_STRUCT_BASE_OUT;

/** @} */

/**
 * @defgroup spud_allocator The caller's allocator
 * @brief Where SpudLib's own memory comes from.
 *
 * SpudLib never chooses where its memory comes from. Each module's
 * instance is made with a SPUD_ALLOCATOR, and SpudLib takes every block
 * it needs for that instance, and for every object made from it, from that
 * allocator alone: the handles, the copies it keeps (debug names, descs),
 * and its own lists and tables. An object never has an allocator of its
 * own, and a destroy call never takes one: an object frees through the
 * allocator of the instance it was made from.
 *
 * Memory that the platform, the operating system or a graphics driver
 * allocates for itself is not covered, because those offer SpudLib no way
 * to pass an allocator on.
 *
 * There is no default. A create call given no allocator refuses it with
 * SPUDRESULT_NULL_ALLOCATOR, and one missing either function with
 * SPUDRESULT_INVALID_ALLOCATOR. spud_allocator_system() is the system heap,
 * for a caller that chooses it.
 *
 * The rules an allocator keeps:
 *
 * - `allocate` returns a block of at least `size` bytes whose address is a
 *   multiple of `alignment`, or NULL if it can't. SpudLib then fails the
 *   call it was making with SPUDRESULT_OUT_OF_MEMORY and leaves everything
 *   as it was before the call. The block's contents are undefined.
 * - SpudLib never asks for 0 bytes. `alignment` is always a power of two,
 *   1 or more. An allocator that can't meet an alignment returns NULL.
 * - `free` is called once for each block, with the same `size` and
 *   `alignment` the block was allocated with, so an allocator need not
 *   record either. It is never called with NULL, and it can't fail.
 * - Either function may be called from any thread, and from several
 *   threads at once, unless a module's header says otherwise for one of its
 *   calls. An allocator that is safe on one thread only is a choice the
 *   caller keeps to itself, by calling that instance from one thread.
 * - Neither function may call SpudLib.
 * - SpudLib copies the SPUD_ALLOCATOR when the instance is made, so the
 *   caller's struct need not outlive the call. The functions and
 *   `user_data` must stay valid until the instance and every object made
 *   from it are destroyed, and by then every block SpudLib took from them
 *   has been freed. Where a module lets an object's memory outlast its
 *   destroy call, its header says until when (a SpudNet resolver whose
 *   lookup is still running).
 * @{
 */

/**
 * @brief Allocates a block for SpudLib.
 *
 * @param[in] user_data The allocator's `user_data`, passed as it was given.
 * @param[in] size      The bytes wanted. Never 0.
 * @param[in] alignment What the address must be a multiple of. A power of
 *                      two, 1 or more.
 *
 * @return The block, or NULL if it can't be given.
 */
typedef void *(*SPUD_ALLOCATE_FUNCTION)(
    void *user_data,
    size_t size,
    size_t alignment);

/**
 * @brief Frees a block SPUD_ALLOCATE_FUNCTION gave.
 *
 * @param[in] user_data The allocator's `user_data`, passed as it was given.
 * @param[in] memory    The block. Never NULL.
 * @param[in] size      The `size` it was allocated with.
 * @param[in] alignment The `alignment` it was allocated with.
 */
typedef void (*SPUD_FREE_FUNCTION)(
    void *user_data,
    void *memory,
    size_t size,
    size_t alignment);

/** @brief The caller's allocator: two functions and the state they share. */
/* Frozen: a table of functions. A new function or hint is a new struct. */
typedef struct SPUD_ALLOCATOR {
	/** Passed to both functions as it is. SpudLib never reads it. May be NULL. */
	void *user_data;
	/** Gives a block. Not NULL. */
	SPUD_ALLOCATE_FUNCTION allocate;
	/** Frees a block `allocate` gave. Not NULL. */
	SPUD_FREE_FUNCTION free;
} SPUD_ALLOCATOR;

/**
 * @brief Returns an allocator over the system heap.
 *
 * Exists so that a caller choosing the system heap says so by name. It is
 * the C library's heap (the CRT's aligned functions on Windows), safe
 * from any thread at once, with a NULL `user_data`. Calling it allocates
 * nothing; it only fills in the struct.
 *
 * @return The allocator, by value.
 */
SPUD_ALLOCATOR spud_allocator_system(void);

/** @} */

/**
 * @brief Returns the name of a SPUDRESULT enumerator as a string.
 *
 * For logs and error messages. The string is the enumerator's identifier
 * exactly as it is spelled in this header, such as "SPUDRESULT_NULL_DESC".
 *
 * @param[in] r The result to name.
 *
 * @return A null-terminated string literal: valid for the life of the
 *         program, and not to be freed or modified. Never NULL.
 * @retval "SPUDRESULT_UNKNOWN" `r` is not one of SPUDRESULT's values. Every
 *         value has its own name.
 */
const char *spudresult_str(SPUDRESULT r);

/**
 * @brief Gives a SpudLib object a debug name, replacing any it already has.
 *
 * The string is copied, through the allocator of the instance the object
 * was made from (SPUD_ALLOCATOR), so it need not outlive the call. The
 * object's previous name is freed, and the object's own destroy call frees the name it
 * holds at that point. Exists in every build, and the name is kept in every
 * build: a debug name never changes a struct's layout or a library's symbols.
 *
 * @param[in,out] object A handle to an object that carries a debug name.
 *                       Every SpudGPU, SpudAudio and SpudNet handle does, and
 *                       so does an sfs_file. The name it holds is replaced.
 * @param[in]     name   The new name, a null-terminated string, or NULL to
 *                       remove the current one. May be the object's own
 *                       current name.
 *
 * @retval SPUD_SUCCESS The name was replaced.
 * @retval SPUDRESULT_NULL_OBJECT `object` is NULL.
 * @retval SPUDRESULT_OUT_OF_MEMORY The copy could not be allocated. The
 *         object keeps the name it had.
 */
SPUDRESULT spud_debug_name_set(void *object, const char *name);

/**
 * @brief Calls spud_debug_name_set(), casting the handle to `void *`.

 *
 * @param[in] obj  A handle of any type that carries a debug name.
 * @param[in] name The new name, or NULL to remove the current one.
 *
 * @return What spud_debug_name_set() returns.
 */
#define SPUD_SET_DEBUG_NAME(obj, name)                                         \
	spud_debug_name_set((void *)(obj), (name))

/**
 * @brief Returns an object's debug name.

 *
 * @param[in] object A handle to an object that carries a debug name (see
 *                   spud_debug_name_set()), or NULL.
 *
 * @return The object's name, a null-terminated string. It belongs to the
 *         object: it stays valid until the name is next set or the object is
 *         destroyed, and is not to be freed or modified.
 * @retval NULL `object` is NULL, or the object has no name.
 */
const char *spud_debug_name_get(void *object);

/**
 * @brief Calls spud_debug_name_get(), casting the handle to `void *`.

 *
 * @param[in] obj A handle of any type that carries a debug name, or NULL.
 *
 * @return What spud_debug_name_get() returns.
 */
#define SPUD_GET_DEBUG_NAME(obj) spud_debug_name_get((void *)(obj))

#if __cplusplus
}
#endif

#endif // SPUDCORE_H
