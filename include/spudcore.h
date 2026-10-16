/**
 * @file spudcore.h
 * @brief SpudCore: what every SpudLib module shares.
 *
 * The SPUDRESULT enum all modules return, the fixed-width integer limits, and
 * the debug-name functions. Platform-agnostic: nothing here calls a platform
 * API.
 */

#ifndef SPUDCORE_H
#define SPUDCORE_H

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
 * @brief The result of a SpudLib call, shared by every module.
 *
 * SPUD_SUCCESS is 0 and the only success value; every other enumerator names
 * one specific failure. Test a result with SPUDFAIL() and print it with
 * spudresult_str().
 *
 * The values are grouped by module: 1-11 are general, 199-354 are SpudGPU
 * (`SPUDRESULT_GPU_*`), 1001-1007 SpudFiles (`SPUDRESULT_SFS_*`), 2001-2026
 * SpudMemory (`SPUDRESULT_SMEM_*`), 3001-3033 SpudNet (`SPUDRESULT_SPUDNET_*`),
 * 4001-4012 SpudAudio (`SPUDRESULT_SAUD_*`) and 5001-5005 SpudInput
 * (`SPUDRESULT_SPUDINPUT_*`). A value is never reused for
 * a different meaning.
 */
typedef enum SPUDRESULT {
	/** The call succeeded. */
	SPUD_SUCCESS                       = 0,
	/** The call failed and no more specific result applies. */
	SPUDRESULT_GENERAL_FAILURE         = 1,
	/** A call into the platform's own API failed. */
	SPUDRESULT_API_SPECIFIC_FAILURE    = 2,
	/** The native API asked for is not one this build can use. */
	SPUDRESULT_INVALID_API             = 3,
	/** A desc pointer is NULL. */
	SPUDRESULT_NULL_DESC               = 4,
	/** An output pointer is NULL. */
	SPUDRESULT_NULL_OUTPUT_PARAMETER   = 5,
	/** A desc holds a value the call cannot accept. */
	SPUDRESULT_DESC_INVALID_PARAMETERS = 6,
	/** An index is past the end of what it indexes. */
	SPUDRESULT_INDEX_OUT_OF_RANGE      = 7,
	/** A size or count is zero. */
	SPUDRESULT_ZERO_SIZE               = 8,
	/** An allocation failed. */
	SPUDRESULT_OUT_OF_MEMORY           = 9,
	/** The call, or the option asked of it, is not implemented yet. */
	SPUDRESULT_NOT_IMPLEMENTED_YET     = 10,
	/** An object handle is NULL. */
	SPUDRESULT_NULL_OBJECT             = 11,

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
 * @brief Returns the name of a SPUDRESULT enumerator as a string.
 *
 * For logs and error messages. The string is the enumerator's identifier
 * exactly as it is spelled in this header, such as "SPUDRESULT_NULL_DESC".
 *
 * @param[in] r The result to name.
 *
 * @return A null-terminated string literal: valid for the life of the
 *         program, and not to be freed or modified. Never NULL.
 * @retval "SPUDRESULT_UNKNOWN" `r` is a value this function has no name for.
 */
const char *spudresult_str(SPUDRESULT r);

#ifdef _DEBUG
/**
 * @brief Gives a SpudLib object a debug name, replacing any it already has.
 *
 * The string is copied, so it need not outlive the call. The object's
 * previous name is freed, and the object's own destroy call frees the name it
 * holds at that point. Exists in debug builds only.
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
 * Exists in debug builds only.
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
 * Exists in debug builds only.
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
 * Exists in debug builds only.
 *
 * @param[in] obj A handle of any type that carries a debug name, or NULL.
 *
 * @return What spud_debug_name_get() returns.
 */
#define SPUD_GET_DEBUG_NAME(obj) spud_debug_name_get((void *)(obj))
#endif

#if __cplusplus
}
#endif

#endif // SPUDCORE_H
