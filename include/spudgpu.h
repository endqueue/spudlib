//
// Created by nathanmoore on 5/16/26.
//

/**
 * @file spudgpu.h
 * @brief SpudGPU: one C interface over the Vulkan, Direct3D 12 and Metal
 * graphics APIs.
 *
 * Every object is an opaque handle the caller creates, owns and destroys.
 * Functions that can fail return a SPUDRESULT (see spudcore.h); the
 * `spudgpu_cmd_*` functions record into a command list and return nothing.
 */

#ifndef SPUDLIB_SPUDGPU_H
#define SPUDLIB_SPUDGPU_H

#include "spudcore.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {

#endif // __cplusplus

#if SPUDGPU_COMPILE_VULKAN_API
#endif

#if SPUDGPU_COMPILE_D3D12_API
#endif

#if SPUDGPU_COMPILE_METAL_API
#endif

/**
 * @brief Pixel and vertex-attribute data format.
 *
 * Numbered like DXGI_FORMAT.
 */
typedef uint32_t SPUDGPU_FORMAT;
enum {
	SPUDGPU_FORMAT_UNKNOWN                    = 0,
	SPUDGPU_FORMAT_R32G32B32A32_TYPELESS      = 1,
	SPUDGPU_FORMAT_R32G32B32A32_FLOAT         = 2,
	SPUDGPU_FORMAT_R32G32B32A32_UINT          = 3,
	SPUDGPU_FORMAT_R32G32B32A32_SINT          = 4,
	SPUDGPU_FORMAT_R32G32B32_TYPELESS         = 5,
	SPUDGPU_FORMAT_R32G32B32_FLOAT            = 6,
	SPUDGPU_FORMAT_R32G32B32_UINT             = 7,
	SPUDGPU_FORMAT_R32G32B32_SINT             = 8,
	SPUDGPU_FORMAT_R16G16B16A16_TYPELESS      = 9,
	SPUDGPU_FORMAT_R16G16B16A16_FLOAT         = 10,
	SPUDGPU_FORMAT_R16G16B16A16_UNORM         = 11,
	SPUDGPU_FORMAT_R16G16B16A16_UINT          = 12,
	SPUDGPU_FORMAT_R16G16B16A16_SNORM         = 13,
	SPUDGPU_FORMAT_R16G16B16A16_SINT          = 14,
	SPUDGPU_FORMAT_R32G32_TYPELESS            = 15,
	SPUDGPU_FORMAT_R32G32_FLOAT               = 16,
	SPUDGPU_FORMAT_R32G32_UINT                = 17,
	SPUDGPU_FORMAT_R32G32_SINT                = 18,
	SPUDGPU_FORMAT_R32G8X24_TYPELESS          = 19,
	SPUDGPU_FORMAT_D32_FLOAT_S8X24_UINT       = 20,
	SPUDGPU_FORMAT_R32_FLOAT_X8X24_TYPELESS   = 21,
	SPUDGPU_FORMAT_X32_TYPELESS_G8X24_UINT    = 22,
	SPUDGPU_FORMAT_R10G10B10A2_TYPELESS       = 23,
	SPUDGPU_FORMAT_R10G10B10A2_UNORM          = 24,
	SPUDGPU_FORMAT_R10G10B10A2_UINT           = 25,
	SPUDGPU_FORMAT_R11G11B10_FLOAT            = 26,
	SPUDGPU_FORMAT_R8G8B8A8_TYPELESS          = 27,
	SPUDGPU_FORMAT_R8G8B8A8_UNORM             = 28,
	SPUDGPU_FORMAT_R8G8B8A8_UNORM_SRGB        = 29,
	SPUDGPU_FORMAT_R8G8B8A8_UINT              = 30,
	SPUDGPU_FORMAT_R8G8B8A8_SNORM             = 31,
	SPUDGPU_FORMAT_R8G8B8A8_SINT              = 32,
	SPUDGPU_FORMAT_R16G16_TYPELESS            = 33,
	SPUDGPU_FORMAT_R16G16_FLOAT               = 34,
	SPUDGPU_FORMAT_R16G16_UNORM               = 35,
	SPUDGPU_FORMAT_R16G16_UINT                = 36,
	SPUDGPU_FORMAT_R16G16_SNORM               = 37,
	SPUDGPU_FORMAT_R16G16_SINT                = 38,
	SPUDGPU_FORMAT_R32_TYPELESS               = 39,
	SPUDGPU_FORMAT_D32_FLOAT                  = 40,
	SPUDGPU_FORMAT_R32_FLOAT                  = 41,
	SPUDGPU_FORMAT_R32_UINT                   = 42,
	SPUDGPU_FORMAT_R32_SINT                   = 43,
	SPUDGPU_FORMAT_R24G8_TYPELESS             = 44,
	SPUDGPU_FORMAT_D24_UNORM_S8_UINT          = 45,
	SPUDGPU_FORMAT_R24_UNORM_X8_TYPELESS      = 46,
	SPUDGPU_FORMAT_X24_TYPELESS_G8_UINT       = 47,
	SPUDGPU_FORMAT_R8G8_TYPELESS              = 48,
	SPUDGPU_FORMAT_R8G8_UNORM                 = 49,
	SPUDGPU_FORMAT_R8G8_UINT                  = 50,
	SPUDGPU_FORMAT_R8G8_SNORM                 = 51,
	SPUDGPU_FORMAT_R8G8_SINT                  = 52,
	SPUDGPU_FORMAT_R16_TYPELESS               = 53,
	SPUDGPU_FORMAT_R16_FLOAT                  = 54,
	SPUDGPU_FORMAT_D16_UNORM                  = 55,
	SPUDGPU_FORMAT_R16_UNORM                  = 56,
	SPUDGPU_FORMAT_R16_UINT                   = 57,
	SPUDGPU_FORMAT_R16_SNORM                  = 58,
	SPUDGPU_FORMAT_R16_SINT                   = 59,
	SPUDGPU_FORMAT_R8_TYPELESS                = 60,
	SPUDGPU_FORMAT_R8_UNORM                   = 61,
	SPUDGPU_FORMAT_R8_UINT                    = 62,
	SPUDGPU_FORMAT_R8_SNORM                   = 63,
	SPUDGPU_FORMAT_R8_SINT                    = 64,
	SPUDGPU_FORMAT_A8_UNORM                   = 65,
	SPUDGPU_FORMAT_R1_UNORM                   = 66,
	SPUDGPU_FORMAT_R9G9B9E5_SHAREDEXP         = 67,
	SPUDGPU_FORMAT_R8G8_B8G8_UNORM            = 68,
	SPUDGPU_FORMAT_G8R8_G8B8_UNORM            = 69,
	SPUDGPU_FORMAT_BC1_TYPELESS               = 70,
	SPUDGPU_FORMAT_BC1_UNORM                  = 71,
	SPUDGPU_FORMAT_BC1_UNORM_SRGB             = 72,
	SPUDGPU_FORMAT_BC2_TYPELESS               = 73,
	SPUDGPU_FORMAT_BC2_UNORM                  = 74,
	SPUDGPU_FORMAT_BC2_UNORM_SRGB             = 75,
	SPUDGPU_FORMAT_BC3_TYPELESS               = 76,
	SPUDGPU_FORMAT_BC3_UNORM                  = 77,
	SPUDGPU_FORMAT_BC3_UNORM_SRGB             = 78,
	SPUDGPU_FORMAT_BC4_TYPELESS               = 79,
	SPUDGPU_FORMAT_BC4_UNORM                  = 80,
	SPUDGPU_FORMAT_BC4_SNORM                  = 81,
	SPUDGPU_FORMAT_BC5_TYPELESS               = 82,
	SPUDGPU_FORMAT_BC5_UNORM                  = 83,
	SPUDGPU_FORMAT_BC5_SNORM                  = 84,
	SPUDGPU_FORMAT_B5G6R5_UNORM               = 85,
	SPUDGPU_FORMAT_B5G5R5A1_UNORM             = 86,
	SPUDGPU_FORMAT_B8G8R8A8_UNORM             = 87,
	SPUDGPU_FORMAT_B8G8R8X8_UNORM             = 88,
	SPUDGPU_FORMAT_R10G10B10_XR_BIAS_A2_UNORM = 89,
	SPUDGPU_FORMAT_B8G8R8A8_TYPELESS          = 90,
	SPUDGPU_FORMAT_B8G8R8A8_UNORM_SRGB        = 91,
	SPUDGPU_FORMAT_B8G8R8X8_TYPELESS          = 92,
	SPUDGPU_FORMAT_B8G8R8X8_UNORM_SRGB        = 93,
	SPUDGPU_FORMAT_BC6H_TYPELESS              = 94,
	SPUDGPU_FORMAT_BC6H_UF16                  = 95,
	SPUDGPU_FORMAT_BC6H_SF16                  = 96,
	SPUDGPU_FORMAT_BC7_TYPELESS               = 97,
	SPUDGPU_FORMAT_BC7_UNORM                  = 98,
	SPUDGPU_FORMAT_BC7_UNORM_SRGB             = 99,
	SPUDGPU_FORMAT_AYUV                       = 100,
	SPUDGPU_FORMAT_Y410                       = 101,
	SPUDGPU_FORMAT_Y416                       = 102,
	SPUDGPU_FORMAT_NV12                       = 103,
	SPUDGPU_FORMAT_P010                       = 104,
	SPUDGPU_FORMAT_P016                       = 105,
	SPUDGPU_FORMAT_420_OPAQUE                 = 106,
	SPUDGPU_FORMAT_YUY2                       = 107,
	SPUDGPU_FORMAT_Y210                       = 108,
	SPUDGPU_FORMAT_Y216                       = 109,
	SPUDGPU_FORMAT_NV11                       = 110,
	SPUDGPU_FORMAT_AI44                       = 111,
	SPUDGPU_FORMAT_IA44                       = 112,
	SPUDGPU_FORMAT_P8                         = 113,
	SPUDGPU_FORMAT_A8P8                       = 114,
	SPUDGPU_FORMAT_B4G4R4A4_UNORM             = 115,

	SPUDGPU_FORMAT_P208 = 130,
	SPUDGPU_FORMAT_V208 = 131,
	SPUDGPU_FORMAT_V408 = 132,

	SPUDGPU_FORMAT_FORCE_UINT = -1 /**< 0xffffffff */
};

/**
 * @brief Returns the size of one pixel of a format, in bits.
 *
 * Divide the result by 8 for the size in bytes.
 *
 * @param[in] fmt Format to query.
 *
 * @return Bits per pixel, or 0 if `fmt` is not a format with a known pixel
 *         size.
 */
uint32_t spudgpu_format_bit_count(SPUDGPU_FORMAT fmt);

/**
 * @brief Looks up a format by its name.
 *
 * @param[in] fmt_str Null-terminated format name.
 *
 * @return The matching SPUDGPU_FORMAT, or SPUDGPU_FORMAT_UNKNOWN if `fmt_str`
 *         is NULL or does not match a known format name.
 */
SPUDGPU_FORMAT spudgpu_format_from_string(const char *fmt_str);

/** Opaque handle to a SpudGPU instance: one connection to the compiled-in graphics API. */
typedef struct spudgpu_instance_t *spudgpu_instance;
/** Opaque handle to a GPU device enumerated from an instance. */
typedef struct spudgpu_device_t *spudgpu_device;
/** Opaque handle to a GPU buffer. */
typedef struct spudgpu_buffer_t *spudgpu_buffer;
/** Opaque handle to a view of a range of a buffer. */
typedef struct spudgpu_buffer_view_t *spudgpu_buffer_view;
/** Opaque handle to a GPU image (texture). */
typedef struct spudgpu_image_t *spudgpu_image;
/** Opaque handle to a view of an image's subresources. */
typedef struct spudgpu_image_view_t *spudgpu_image_view;
/** Opaque handle to a heap: one device allocation buffers and images are placed in. */
typedef struct spudgpu_heap_t *spudgpu_heap;
/** Opaque handle to a graphics pipeline. */
typedef struct spudgpu_shader_pipeline_t *spudgpu_shader_pipeline;
/** Opaque handle to a command list. */
typedef struct spudgpu_command_list_t *spudgpu_command_list;
/** Opaque handle to a command allocator, the memory command lists record into. */
typedef struct spudgpu_command_allocator_t *spudgpu_command_allocator;
/** Opaque handle to a command queue. */
typedef struct spudgpu_command_queue_t *spudgpu_command_queue;
/** Opaque handle to a swap chain. */
typedef struct spudgpu_swap_chain_t *spudgpu_swap_chain;
/** Opaque handle to a shader module. */
typedef struct spudgpu_shader_module_t *spudgpu_shader_module;
/** Opaque handle to a compute pipeline. */
typedef struct spudgpu_compute_pipeline_t *spudgpu_compute_pipeline;
/** Opaque handle to a window surface a swap chain presents to. */
typedef struct spudgpu_surface_t *spudgpu_surface;
/** Opaque handle to a semaphore, for GPU-to-GPU synchronization. */
typedef struct spudgpu_semaphore_t *spudgpu_semaphore;
/** Opaque handle to a fence, for CPU-to-GPU synchronization. */
typedef struct spudgpu_fence_t *spudgpu_fence;
/** Opaque handle to a set of GPU timestamps, written as rendering passes start and end. */
typedef struct spudgpu_timestamp_set_t *spudgpu_timestamp_set;
/** Opaque handle to a descriptor set layout. */
typedef struct spudgpu_descriptor_set_layout_t *spudgpu_descriptor_set_layout;
/** Opaque handle to a descriptor pool. */
typedef struct spudgpu_descriptor_pool_t *spudgpu_descriptor_pool;
/** Opaque handle to a descriptor set. */
typedef struct spudgpu_descriptor_set_t *spudgpu_descriptor_set;
/** Opaque handle to a pipeline layout. */
typedef struct spudgpu_pipeline_layout_t *spudgpu_pipeline_layout;

/**
 * @brief The graphics API a build of SpudGPU runs on.
 *
 * A build compiles in exactly one backend; spudgpu_get_native_gpu_api()
 * reports which.
 */
typedef uint32_t SPUDGPU_NATIVE_API;

/**
 * @name Supported Graphics APIs
 * @anchor SPUDGPU_API_Constants
 *
 * The graphics APIs SpudGPU has a backend for.
 */
/** @{ */
enum {
	/**
	 * Placeholder representing an uninitialized, unsupported, or invalid
	 * graphics API.
	 */
	SPUDGPU_NATIVE_API_NONE = 0,

	/** Vulkan. */
	SPUDGPU_NATIVE_API_VULKAN = 1,

	/** Direct3D 12. */
	SPUDGPU_NATIVE_API_D3D12 = 2,

	/** Metal. */
	SPUDGPU_NATIVE_API_METAL = 3
};

/** @} */

/**
 * @brief What the caller decides when creating a SpudGPU instance. No field
 * has a default: a zeroed desc is refused, for its `base`, and one with only
 * its `base` set is refused for its `allocator`.
 */
typedef struct spudgpu_instance_desc {
	/** `type` is SPUD_STRUCT_TYPE_GPU_INSTANCE_DESC; `next` is NULL (nothing
	 * is chained onto this struct yet). Set by the caller. See
	 * SPUD_STRUCT_BASE. */
	SPUD_STRUCT_BASE base;
	/** Where the memory of the instance, and of every object made from it,
	 * comes from (SPUD_ALLOCATOR). Copied, so the struct need not outlive
	 * the call; its functions and `user_data` must outlive the instance and
	 * every object made from it. */
	const SPUD_ALLOCATOR *allocator;
	/** Null-terminated name of the application. */
	const char *application_name;
	/** Packed 32-bit application version. */
	uint32_t application_version;
	/** Null-terminated name of the engine. */
	const char *engine_name;
	/** Packed 32-bit engine version. */
	uint32_t engine_version;
} spudgpu_instance_desc;

/**
 * @brief Creates a SpudGPU instance on the graphics API this build was
 * compiled for.
 *
 * Call this before any other SpudGPU function, then
 * spudgpu_enumerate_devices() to find the devices to work with.
 *
 * @param[in]  desc         What the caller decides.
 * @param[out] out_instance Receives the new instance on success; NULL on
 *                          failure.
 *
 * @retval SPUD_SUCCESS The instance was created.
 * @retval SPUDRESULT_NULL_DESC `desc` is NULL.
 * @retval SPUDRESULT_STRUCT_WRONG_TYPE `desc->base.type` is not
 *         SPUD_STRUCT_TYPE_GPU_INSTANCE_DESC.
 * @retval SPUDRESULT_STRUCT_UNKNOWN_IN_CHAIN `desc->base.next` is not NULL.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_instance` is NULL.
 * @retval SPUDRESULT_NULL_ALLOCATOR `desc->allocator` is NULL.
 * @retval SPUDRESULT_INVALID_ALLOCATOR `desc->allocator` lacks a function.
 * @return Another SPUDRESULT if the backend fails, such as
 *         SPUDRESULT_OUT_OF_MEMORY or SPUDRESULT_API_SPECIFIC_FAILURE.
 *
 * @see spudgpu_destroy_instance()
 */
SPUDRESULT spudgpu_create_instance(
    const spudgpu_instance_desc *desc,
    spudgpu_instance *out_instance);

/**
 * @brief Destroys a SpudGPU instance, with the devices enumerated from it and
 * their command queues.
 *
 * @warning Every other object created through the instance (buffers, images,
 * pipelines, swap chains, ...) must be destroyed first.
 *
 * @param[in] instance Instance to destroy.
 *
 * @return SPUD_SUCCESS, including when `instance` is NULL.
 */
SPUDRESULT spudgpu_destroy_instance(spudgpu_instance instance);

/**
 * @brief Lists the GPU devices available to an instance.
 *
 * No device is chosen for the caller; inspect each one with
 * spudgpu_get_device_properties() and pick. The devices and the array of
 * them belong to the instance and are released by spudgpu_destroy_instance();
 * the caller frees neither. The devices are made by the first call, and a
 * later call hands out the same array.
 *
 * @param[in]  instance          Instance to enumerate devices through.
 * @param[out] out_devices       Receives a pointer to an array of device
 *                               handles.
 * @param[out] out_devices_count Receives the number of handles in
 *                               `*out_devices`.
 *
 * @retval SPUD_SUCCESS The devices were enumerated.
 * @retval SPUDRESULT_GPU_INVALID_INSTANCE `instance` is NULL.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_devices` or
 *         `out_devices_count` is NULL.
 * @retval SPUDRESULT_GPU_DEVICE_ENUMERATION_FAILURE The graphics API reported
 *         no usable device.
 * @return Another SPUDRESULT if the backend fails, such as
 *         SPUDRESULT_OUT_OF_MEMORY or SPUDRESULT_API_SPECIFIC_FAILURE.
 */
SPUDRESULT spudgpu_enumerate_devices(
    spudgpu_instance instance,
    spudgpu_device **out_devices,
    uint32_t *out_devices_count);

/**
 * @name GPU vendor IDs
 * PCI vendor IDs, for comparing against SPUDGPU_DEVICE_PROPERTIES::vendor_id.
 */
/** @{ */
#define SPUDGPU_VENDOR_INTEL 0x8086
#define SPUDGPU_VENDOR_AMD_ATI 0x1002
#define SPUDGPU_VENDOR_NVIDIA 0X10DE
#define SPUDGPU_VENDOR_ARM 0x13B5
#define SPUDGPU_VENDOR_QUALCOMM 0x5143
#define SPUDGPU_VENDOR_IMGTEC 0x1010
#define SPUDGPU_VENDOR_APPLE 0x106B
#define SPUDGPU_VENDOR_VMWARE 0x15AD
/** @} */

/**
 * @brief Identity and memory sizes of a GPU device, as the graphics API
 * reports them.
 */
typedef struct SPUDGPU_DEVICE_PROPERTIES {
	/** `type` is SPUD_STRUCT_TYPE_GPU_DEVICE_PROPERTIES; `next` is NULL (nothing is chained onto
	 * this struct yet). The caller builds the chain and sets each type; SpudGPU fills in the rest. See SPUD_STRUCT_BASE. */
	SPUD_STRUCT_BASE_OUT base;
	/** Device name. */
	char description[128];
	/** PCI vendor ID, e.g. SPUDGPU_VENDOR_NVIDIA. */
	uint32_t vendor_id;
	/** PCI device ID. */
	uint32_t device_id;
	/**
	 * PCI subsystem ID. D3D12 only; 0 on Vulkan and Metal, which do not
	 * report one.
	 */
	uint32_t subSys_id;
	/**
	 * PCI revision. D3D12 only; 0 on Vulkan and Metal, which do not report
	 * one.
	 */
	uint32_t revision;
	/**
	 * Bytes of memory that are the device's own.
	 *
	 * D3D12: `DXGI_ADAPTER_DESC::DedicatedVideoMemory`. Vulkan: the sum of
	 * the heaps flagged `VK_MEMORY_HEAP_DEVICE_LOCAL_BIT`, which is nonzero
	 * on an integrated device too. Metal: `recommendedMaxWorkingSetSize` on
	 * a device without unified memory, 0 on one with it.
	 */
	uint64_t dedicated_video_memory;
	/**
	 * Bytes of system memory reserved for the device.
	 *
	 * D3D12: `DXGI_ADAPTER_DESC::DedicatedSystemMemory`. 0 on Vulkan and
	 * Metal, which do not report one.
	 */
	uint64_t dedicated_system_memory;
	/**
	 * Bytes of system memory the device may share with the CPU.
	 *
	 * D3D12: `DXGI_ADAPTER_DESC::SharedSystemMemory`. Vulkan: the sum of the
	 * heaps not flagged `VK_MEMORY_HEAP_DEVICE_LOCAL_BIT`. Metal:
	 * `recommendedMaxWorkingSetSize` on a device with unified memory, 0 on
	 * one without.
	 */
	uint64_t shared_system_memory;
	/**
	 * Whether the device has no memory of its own and uses the system's,
	 * as the graphics API reports it. It is a fact about this device, not
	 * about the platform or the backend: one machine can have a device of
	 * each kind.
	 *
	 * Metal: `MTLDevice.hasUnifiedMemory`. D3D12:
	 * `D3D12_FEATURE_DATA_ARCHITECTURE1::UMA`, false if the query fails.
	 * Vulkan has no such query: true when the device's type is
	 * `VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU`.
	 *
	 * The memory sizes above are not a substitute: #dedicated_video_memory
	 * is nonzero for a Vulkan device of either kind.
	 */
	bool unified_memory;
	/**
	 * Whether a buffer can be created with
	 * SPUDGPU_MEMORY_KIND_DEVICE_MAPPABLE on this device. False means
	 * spudgpu_create_buffer() returns
	 * SPUDRESULT_GPU_MEMORY_KIND_NOT_SUPPORTED for that kind.
	 *
	 * Never true unless #unified_memory is, and not implied by it.
	 *
	 * Metal: `MTLDevice.hasUnifiedMemory`. D3D12:
	 * `D3D12_FEATURE_DATA_ARCHITECTURE1::UMA`, false if the query fails.
	 * Vulkan: #unified_memory, and the device has a memory type that is
	 * both `VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT` and
	 * `VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT`.
	 */
	bool device_mappable_memory;
	/**
	 * Whether a rendering pass can render to several layers at once, with
	 * the vertex shader choosing the layer each primitive is drawn to. False
	 * means spudgpu_rendering_begin_desc::layer_count must be 0 on this
	 * device.
	 *
	 * Vulkan: `VkPhysicalDeviceVulkan12Features::shaderOutputLayer`, which
	 * is then enabled on the device. D3D12:
	 * `D3D12_FEATURE_DATA_D3D12_OPTIONS::VPAndRTArrayIndexFromAnyShaderFeedingRasterizerSupportedWithoutGSEmulation`,
	 * false if the query fails. Metal: the device is in
	 * `MTLGPUFamilyApple5` or `MTLGPUFamilyMac2`.
	 */
	bool layered_rendering;
	/**
	 * Whether a heap can be created with SPUDGPU_MEMORY_KIND_UPLOAD,
	 * SPUDGPU_MEMORY_KIND_READBACK or SPUDGPU_MEMORY_KIND_DEVICE_MAPPABLE.
	 * False means spudgpu_create_heap() returns
	 * SPUDRESULT_GPU_MEMORY_KIND_NOT_SUPPORTED for those kinds, and buffers
	 * of them are created with spudgpu_create_buffer() instead.
	 *
	 * DEVICE_MAPPABLE heaps also need #device_mappable_memory.
	 *
	 * Vulkan and D3D12: always true. Metal: whether the device is in an
	 * `MTLGPUFamilyApple` family, the GPUs that make heaps of
	 * `MTLStorageModeShared`; any other makes heaps of
	 * `MTLStorageModePrivate` only.
	 */
	bool mappable_heaps;
	/**
	 * Whether a pipeline may clamp its depth bias
	 * (spudgpu_shader_pipeline_desc::depth_bias_clamp other than 0). False
	 * means spudgpu_create_shader_pipeline() refuses such a pipeline with
	 * SPUDRESULT_GPU_DEPTH_BIAS_CLAMP_NOT_SUPPORTED.
	 *
	 * Vulkan: `VkPhysicalDeviceFeatures::depthBiasClamp`, which is then
	 * enabled on the device. D3D12 and Metal: always true.
	 */
	bool depth_bias_clamp;
	/**
	 * Whether a pipeline's color attachments may blend differently from
	 * each other (spudgpu_shader_pipeline_desc::blend_attachments entries
	 * that differ). False means spudgpu_create_shader_pipeline() refuses
	 * such a pipeline with SPUDRESULT_GPU_INDEPENDENT_BLEND_NOT_SUPPORTED.
	 *
	 * Vulkan: `VkPhysicalDeviceFeatures::independentBlend`, which is then
	 * enabled on the device. D3D12 and Metal: always true.
	 */
	bool independent_blend;
	/**
	 * The largest `draw_count` spudgpu_cmd_draw_indirect() and
	 * spudgpu_cmd_draw_indexed_indirect() take in one call. A call with a
	 * larger count records nothing; split it into calls at advancing
	 * offsets. 1 means each call draws one entry at most.
	 *
	 * Vulkan: `VkPhysicalDeviceLimits::maxDrawIndirectCount` where
	 * `VkPhysicalDeviceFeatures::multiDrawIndirect` is reported (and then
	 * enabled on the device), else 1. D3D12 and Metal: UINT32_MAX.
	 */
	uint32_t max_draw_indirect_count;
	/**
	 * Whether a rendering pass can write timestamps as it starts and ends
	 * (spudgpu_create_timestamp_set()). False means
	 * spudgpu_create_timestamp_set() returns
	 * SPUDRESULT_GPU_TIMESTAMPS_NOT_SUPPORTED.
	 *
	 * Vulkan: the graphics queue family has `timestampValidBits` above 0.
	 * D3D12: always true. Metal: the device has the timestamp counter set
	 * and `supportsCounterSampling:MTLCounterSamplingPointAtStageBoundary`.
	 */
	bool pass_timestamps;
	/**
	 * Whether an image view may be SPUDGPU_IMAGE_VIEW_TYPE_CUBE_ARRAY, and a
	 * shader may declare a cube array image or texture. False means
	 * spudgpu_create_image_view() refuses such a view with
	 * SPUDRESULT_GPU_INVALID_IMAGE_VIEW_TYPE.
	 *
	 * Vulkan: `VkPhysicalDeviceFeatures::imageCubeArray`, which is then
	 * enabled on the device. D3D12 and Metal: always true.
	 */
	bool cube_array_views;
	/**
	 * What the offset of a uniform buffer range in a descriptor write
	 * (spudgpu_descriptor_buffer_info::offset, for
	 * SPUDGPU_DESCRIPTOR_TYPE_UNIFORM_BUFFER) is a multiple of on this
	 * device. Any other offset is outside the graphics API's rules. A power
	 * of two.
	 *
	 * Vulkan: `VkPhysicalDeviceLimits::minUniformBufferOffsetAlignment`.
	 * D3D12: 256 (`D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT`). Metal
	 * has no query: 256, the largest any Metal GPU family asks of a
	 * constant buffer's offset.
	 */
	uint32_t uniform_buffer_offset_alignment;
	/**
	 * The same for a storage buffer range
	 * (SPUDGPU_DESCRIPTOR_TYPE_STORAGE_BUFFER). A power of two.
	 *
	 * Vulkan: `VkPhysicalDeviceLimits::minStorageBufferOffsetAlignment`.
	 * D3D12: 16 (`D3D12_RAW_UAV_SRV_BYTE_ALIGNMENT`: a storage buffer is a
	 * raw view). Metal has no query: 256, as for a uniform buffer.
	 */
	uint32_t storage_buffer_offset_alignment;
} SPUDGPU_DEVICE_PROPERTIES;

/**
 * @brief Reads a device's identity and memory sizes.
 *
 * @param[in]  device         Device to query.
 * @param[out] out_properties Receives the device's properties.
 *
 * @retval SPUD_SUCCESS `out_properties` was filled in.
 * @retval SPUDRESULT_GPU_INVALID_DEVICE `device` is NULL.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_properties` is NULL.
 * @retval SPUDRESULT_STRUCT_WRONG_TYPE `out_properties->base.type` is not
 *         SPUD_STRUCT_TYPE_GPU_DEVICE_PROPERTIES.
 * @retval SPUDRESULT_STRUCT_UNKNOWN_IN_CHAIN `out_properties->base.next` is not NULL.
 */
SPUDRESULT spudgpu_get_device_properties(
    spudgpu_device device,
    SPUDGPU_DEVICE_PROPERTIES *out_properties);

/**
 * @brief Returns the graphics API an instance runs on.
 *
 * @param[in] instance Instance to query.
 *
 * @return The SPUDGPU_NATIVE_API of the compiled-in backend, or
 *         SPUDGPU_NATIVE_API_NONE if `instance` is NULL.
 */
SPUDGPU_NATIVE_API spudgpu_get_native_gpu_api(spudgpu_instance instance);

/**
 * @brief Returns a device's first graphics (direct) command queue.
 *
 * The device owns the queue; it is not destroyed by the caller.
 *
 * @param[in] device Device to get the queue from.
 *
 * @return The command queue, or NULL if `device` is NULL.
 */
spudgpu_command_queue spudgpu_get_graphics_queue(spudgpu_device device);

/**
 * @brief The kind of work a command list, allocator or queue carries.
 */
typedef uint32_t SPUDGPU_COMMAND_LIST_TYPE;
enum {
	/** Graphics, compute and copy commands. */
	SPUDGPU_COMMAND_LIST_TYPE_DIRECT = 0,
	/** Copy commands only. */
	SPUDGPU_COMMAND_LIST_TYPE_COPY = 1,
	/**
	 * A bundle: draw state recorded once and replayed inside a direct
	 * command list. @see SPUDGPU_EXT_BUNDLES
	 */
	SPUDGPU_COMMAND_LIST_TYPE_BUNDLE = 2,
	/** Compute and copy commands. */
	SPUDGPU_COMMAND_LIST_TYPE_COMPUTE = 3
};

/**
 * @brief Returns how many command queues of a type a device has.
 *
 * @note Not yet implemented by the Vulkan backend.
 *
 * @param[in] device Device to query.
 * @param[in] type   Kind of queue to count.
 *
 * @return The number of queues, valid as `index` in
 *         spudgpu_get_command_queue(), or 0 if `device` is NULL or the device
 *         has no queues of `type`.
 */
uint32_t spudgpu_get_max_queue_count(
    spudgpu_device device,
    SPUDGPU_COMMAND_LIST_TYPE type);

/**
 * @brief Returns one of a device's command queues.
 *
 * The device owns the queue; it is not destroyed by the caller.
 *
 * @note Not yet implemented by the Vulkan backend.
 *
 * @param[in]  device    Device to get the queue from.
 * @param[in]  type      Kind of queue.
 * @param[in]  index     Which queue of that kind, below
 *                       spudgpu_get_max_queue_count().
 * @param[out] out_queue Receives the queue on success.
 *
 * @retval SPUD_SUCCESS `out_queue` was written.
 * @retval SPUDRESULT_GPU_INVALID_DEVICE `device` is NULL.
 * @retval SPUDRESULT_GPU_INVALID_COMMAND_LIST_TYPE The device has no queues of
 *         `type`.
 * @retval SPUDRESULT_INDEX_OUT_OF_RANGE `index` is not below the queue count.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_queue` is NULL.
 */
SPUDRESULT spudgpu_get_command_queue(
    spudgpu_device device,
    SPUDGPU_COMMAND_LIST_TYPE type,
    uint32_t index,
    spudgpu_command_queue *out_queue);

/**
 * @brief Submits closed command lists to a queue for execution.
 *
 * Performs no synchronization with a swap chain; use
 * spudgpu_submit_command_lists_synced() when rendering to one, or
 * spudgpu_queue_submit() to name the semaphores and fence yourself.
 *
 * @param[in] queue          Queue to submit to.
 * @param[in] cmd_lists      Array of command lists, each closed with
 *                           spudgpu_end_command_list().
 * @param[in] cmd_list_count Number of elements in `cmd_lists`.
 *
 * @retval SPUD_SUCCESS The command lists were submitted.
 * @retval SPUDRESULT_GPU_INVALID_COMMAND_QUEUE `queue` is NULL.
 * @retval SPUDRESULT_GPU_INVALID_COMMAND_LIST `cmd_lists` is NULL.
 * @retval SPUDRESULT_ZERO_SIZE `cmd_list_count` is 0.
 * @return Another SPUDRESULT if the backend fails, such as
 *         SPUDRESULT_OUT_OF_MEMORY or SPUDRESULT_API_SPECIFIC_FAILURE.
 */
SPUDRESULT spudgpu_submit_command_lists(
    spudgpu_command_queue queue,
    spudgpu_command_list *cmd_lists,
    uint32_t cmd_list_count);

/**
 * @brief Submits command lists synchronized with a swap chain.
 *
 * Waits for the swap chain's acquired image and signals what
 * spudgpu_swap_chain_present() and the next
 * spudgpu_swap_chain_acquire_next_image() wait on. Call this instead of
 * spudgpu_submit_command_lists() when rendering to a swap chain. It is
 * spudgpu_queue_submit() with only the command lists and
 * spudgpu_submit_desc::swap_chain set; use that to signal a fence as well.
 *
 * @param[in] queue          Queue to submit to.
 * @param[in] cmd_lists      Array of closed command lists.
 * @param[in] cmd_list_count Number of elements in `cmd_lists`.
 * @param[in] swap_chain     Swap chain to synchronize with.
 *
 * @retval SPUD_SUCCESS The command lists were submitted.
 * @retval SPUDRESULT_GPU_INVALID_COMMAND_QUEUE `queue` is NULL.
 * @retval SPUDRESULT_GPU_INVALID_COMMAND_LIST `cmd_lists` is NULL.
 * @retval SPUDRESULT_ZERO_SIZE `cmd_list_count` is 0.
 * @retval SPUDRESULT_GPU_INVALID_SWAP_CHAIN `swap_chain` is NULL.
 * @return Another SPUDRESULT if the backend fails, such as
 *         SPUDRESULT_OUT_OF_MEMORY or SPUDRESULT_API_SPECIFIC_FAILURE.
 */
SPUDRESULT spudgpu_submit_command_lists_synced(
    spudgpu_command_queue queue,
    spudgpu_command_list *cmd_lists,
    uint32_t cmd_list_count,
    spudgpu_swap_chain swap_chain);

/**
 * @brief What spudgpu_queue_submit() submits and how it is synchronized.
 */
typedef struct spudgpu_submit_desc {
	/** `type` is SPUD_STRUCT_TYPE_GPU_SUBMIT_DESC; `next` is NULL (nothing is chained onto
	 * this struct yet). Set by the caller. See SPUD_STRUCT_BASE. */
	SPUD_STRUCT_BASE base;
	/** Array of closed command lists to submit. */
	spudgpu_command_list *cmd_lists;
	/** Number of elements in cmd_lists. */
	uint32_t cmd_list_count;

	/** GPU-to-GPU sync: semaphores to wait on before executing. */
	spudgpu_semaphore *wait_semaphores;
	/** Number of elements in wait_semaphores. */
	uint32_t wait_semaphore_count;
	/**
	 * One SPUDGPU_PIPELINE_STAGE mask per wait semaphore. Required when
	 * wait_semaphore_count > 0.
	 */
	uint32_t *wait_stage_masks;
	/** GPU-to-GPU sync: semaphores to signal when execution completes. */
	spudgpu_semaphore *signal_semaphores;
	/** Number of elements in signal_semaphores. */
	uint32_t signal_semaphore_count;

	/**
	 * CPU-to-GPU sync: fence to signal when execution completes. Optional,
	 * can be NULL.
	 */
	spudgpu_fence signal_fence;

	/**
	 * The value #signal_fence is signaled to once every command list of
	 * this submission has run. Ignored if #signal_fence is NULL. It is the
	 * caller's to choose, and must be greater than every value the fence has
	 * been signaled to or has been asked to be signaled to by a submission
	 * that hasn't finished: a fence's value only goes up.
	 */
	uint64_t signal_fence_value;

	/**
	 * Swap chain the submission renders to. Optional, can be NULL. When set,
	 * the submission is synchronized with it as
	 * spudgpu_submit_command_lists_synced() does - it waits for the acquired
	 * image and signals what spudgpu_swap_chain_present() and the next
	 * spudgpu_swap_chain_acquire_next_image() wait on - in addition to the
	 * semaphores and the fence above.
	 */
	spudgpu_swap_chain swap_chain;
} spudgpu_submit_desc;

/**
 * @brief Submits command lists with caller-chosen synchronization.
 *
 * @param[in] queue Queue to submit to.
 * @param[in] desc  Command lists to submit, the semaphores to wait on and
 *                  signal, the fence to signal and the value to signal it
 *                  to, and the swap chain to synchronize with.
 *
 * @retval SPUD_SUCCESS The command lists were submitted.
 * @retval SPUDRESULT_GPU_INVALID_COMMAND_QUEUE `queue` is NULL.
 * @retval SPUDRESULT_NULL_DESC `desc` is NULL.
 * @retval SPUDRESULT_STRUCT_WRONG_TYPE `desc->base.type` is not
 *         SPUD_STRUCT_TYPE_GPU_SUBMIT_DESC.
 * @retval SPUDRESULT_STRUCT_UNKNOWN_IN_CHAIN `desc->base.next` is not NULL.
 * @retval SPUDRESULT_GPU_INVALID_COMMAND_LIST `desc->cmd_lists` is NULL.
 * @retval SPUDRESULT_ZERO_SIZE `desc->cmd_list_count` is 0.
 * @return Another SPUDRESULT if the backend fails, such as
 *         SPUDRESULT_OUT_OF_MEMORY or SPUDRESULT_API_SPECIFIC_FAILURE.
 */
SPUDRESULT spudgpu_queue_submit(
    spudgpu_command_queue queue,
    const spudgpu_submit_desc *desc);

/**
 * @brief Blocks the calling thread until all work submitted to a queue has
 * completed.
 *
 * Equivalent to vkQueueWaitIdle. Prefer fence-based synchronization in hot
 * paths.
 *
 * @param[in] queue Queue to wait on.
 */
void spudgpu_queue_wait_idle(spudgpu_command_queue queue);

/**
 * @brief Configuration descriptor for a command allocator.
 */
typedef struct spudgpu_command_allocator_desc {
	/** `type` is SPUD_STRUCT_TYPE_GPU_COMMAND_ALLOCATOR_DESC; `next` is NULL (nothing is chained onto
	 * this struct yet). Set by the caller. See SPUD_STRUCT_BASE. */
	SPUD_STRUCT_BASE base;
	/**
	 * The kind of command list recorded into the allocator.
	 * @see SPUDGPU_COMMAND_LIST_TYPE
	 */
	SPUDGPU_COMMAND_LIST_TYPE type;
	/** Reserved: no backend reads it. Set to 0. */
	uint32_t flags;
	// uint32_t queue_family_index;
} spudgpu_command_allocator_desc;

/**
 * @brief Creates a command allocator on a device.
 *
 * @param[in]  device        Device to create the allocator on.
 * @param[in]  desc          Allocator configuration.
 * @param[out] out_allocator Receives the new allocator on success.
 *
 * @retval SPUD_SUCCESS The allocator was created.
 * @retval SPUDRESULT_GPU_INVALID_DEVICE `device` is NULL.
 * @retval SPUDRESULT_NULL_DESC `desc` is NULL.
 * @retval SPUDRESULT_STRUCT_WRONG_TYPE `desc->base.type` is not
 *         SPUD_STRUCT_TYPE_GPU_COMMAND_ALLOCATOR_DESC.
 * @retval SPUDRESULT_STRUCT_UNKNOWN_IN_CHAIN `desc->base.next` is not NULL.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_allocator` is NULL.
 * @return Another SPUDRESULT if the backend fails, such as
 *         SPUDRESULT_OUT_OF_MEMORY or SPUDRESULT_API_SPECIFIC_FAILURE.
 *
 * @see spudgpu_destroy_command_allocator()
 */
SPUDRESULT spudgpu_create_command_allocator(
    spudgpu_device device,
    const spudgpu_command_allocator_desc *desc,
    spudgpu_command_allocator *out_allocator);

/**
 * @brief Destroys a command allocator.
 *
 * @param[in] allocator Allocator to destroy.
 */
void spudgpu_destroy_command_allocator(spudgpu_command_allocator allocator);

/**
 * @brief Resets a command allocator so its memory can be recorded into
 * again.
 *
 * Until an allocator is reset, every recording made into it keeps the
 * memory it took, on a backend that pools command memory in the allocator
 * (D3D12; Vulkan's pool with its buffers reset one at a time). Opening a
 * command list again does not give that memory back. A caller that records
 * every frame resets each allocator once the GPU is done with it, or memory
 * grows for as long as the allocator lives.
 *
 * Two rules hold on every backend:
 *
 * - Reset only when no recording made into the allocator is still
 *   executing on the GPU, and no command list of the allocator is open for
 *   recording.
 * - A recording made into the allocator before the reset must not be
 *   submitted after it. Open the list and record again.
 *
 * @param[in] allocator Allocator to reset.
 *
 * @retval SPUD_SUCCESS The allocator was reset.
 * @retval SPUDRESULT_GPU_INVALID_COMMAND_ALLOCATOR `allocator` is NULL.
 * @retval SPUDRESULT_API_SPECIFIC_FAILURE The graphics API failed to reset
 *         it.
 */
SPUDRESULT spudgpu_reset_command_allocator(spudgpu_command_allocator allocator);

/**
 * @brief Creates a command list that records into a command allocator.
 *
 * @param[in]  allocator    Allocator the command list records into.
 * @param[out] out_cmd_list Receives the new command list on success.
 *
 * @retval SPUD_SUCCESS The command list was created.
 * @retval SPUDRESULT_GPU_INVALID_COMMAND_ALLOCATOR `allocator` is NULL.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_cmd_list` is NULL.
 * @return Another SPUDRESULT if the backend fails, such as
 *         SPUDRESULT_OUT_OF_MEMORY or SPUDRESULT_API_SPECIFIC_FAILURE.
 *
 * @see spudgpu_destroy_command_list()
 */
SPUDRESULT spudgpu_create_command_list(
    spudgpu_command_allocator allocator,
    spudgpu_command_list *out_cmd_list);

/**
 * @brief Destroys a command list.
 *
 * @param[in] cmd Command list to destroy.
 */
void spudgpu_destroy_command_list(spudgpu_command_list cmd);

/**
 * @brief Opens a command list for recording.
 *
 * Resets the command list's state tracking and puts it in recording mode.
 *
 * What is recorded between this and spudgpu_end_command_list() is submitted
 * once. To run the same commands again, open the list and record them
 * again: a Metal command buffer is committed once, and Vulkan's is begun as
 * a one-time submit.
 *
 * @warning A command list must not be recorded from more than one thread at a
 * time.
 *
 * @warning Submitting one recording a second time is undefined behavior.
 *
 * @warning Recording a spudgpu_cmd_*() call into a list that is not open
 * (before this call, or after spudgpu_end_command_list()) is undefined
 * behavior: Vulkan and D3D12 both refuse it, and a driver may fault.
 *
 * @warning Do not open a list while its last recording may still be
 * executing on the GPU: Vulkan cannot reset a command buffer that is
 * pending. To record while that one runs, record into another list.
 *
 * @warning At most one command list of an allocator is open at a time: end
 * it before opening another list of the same allocator. D3D12 refuses to
 * open a second list on an allocator another open list is recording into,
 * and the list is left closed, so what is then recorded into it is
 * undefined. To keep two lists open at once, make them from two
 * allocators. A debug build asserts this on every backend, and asserts in
 * spudgpu_reset_command_allocator() that no list of the allocator is open.
 *
 * @param[in] cmd Command list to open.
 */
void spudgpu_begin_command_list(spudgpu_command_list cmd);

/**
 * @brief Closes a command list, making it ready to submit to a queue.
 *
 * @param[in] cmd Command list to close.
 */
void spudgpu_end_command_list(spudgpu_command_list cmd);

/**
 * @brief Bitmask of fence creation flags.
 */
typedef uint32_t SPUDGPU_FENCE_FLAGS;
enum {
	/** No flags. */
	SPUDGPU_FENCE_FLAG_NONE = 0,
	/**
	 * The fence can be shared with another process or graphics API. D3D12:
	 * `D3D12_FENCE_FLAG_SHARED`. Vulkan and Metal have nothing to select at
	 * creation and ignore it.
	 */
	SPUDGPU_FENCE_FLAG_SHARED = 1 << 0
};

/**
 * @brief Creates a fence: a counter the GPU raises as submitted work
 * finishes, and the CPU reads and waits on.
 *
 * A fence holds one 64-bit value that only goes up. A submission raises it
 * to a value the caller chooses (spudgpu_submit_desc::signal_fence_value)
 * once its command lists have run; spudgpu_signal_fence() raises it from the
 * CPU. spudgpu_get_fence_value() reads it and spudgpu_wait_for_fences()
 * blocks until it reaches a value. There is nothing to reset: a fence is
 * reused by signaling it to a higher value.
 *
 * Maps to: a timeline VkSemaphore (Vulkan), ID3D12Fence (D3D12),
 * MTLSharedEvent (Metal).
 *
 * @param[in]  device        Device that owns the fence.
 * @param[in]  flags         Fence creation flags.
 * @param[in]  initial_value Value the fence starts at.
 * @param[out] out_fence     Receives the new fence on success.
 *
 * @retval SPUD_SUCCESS The fence was created.
 * @retval SPUDRESULT_GPU_INVALID_DEVICE `device` is NULL.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_fence` is NULL.
 * @return Another SPUDRESULT if the backend fails, such as
 *         SPUDRESULT_OUT_OF_MEMORY or SPUDRESULT_API_SPECIFIC_FAILURE.
 *
 * @see spudgpu_destroy_fence()
 */
SPUDRESULT spudgpu_create_fence(
    spudgpu_device device,
    SPUDGPU_FENCE_FLAGS flags,
    uint64_t initial_value,
    spudgpu_fence *out_fence);

/**
 * @brief Destroys a fence.
 *
 * @warning Every submission that signals the fence must have finished, and
 * no thread may be waiting on it.
 *
 * @param[in] fence Fence to destroy.
 */
void spudgpu_destroy_fence(spudgpu_fence fence);

/**
 * @brief Reads the value a fence has reached.
 *
 * It is the highest value signaled so far by work that has finished or by
 * spudgpu_signal_fence(), not a value a submission still running has been
 * asked to signal.
 *
 * @param[in] fence Fence to read.
 *
 * @return The fence's value, or 0 if `fence` is NULL or the graphics API
 *         fails to read it.
 */
uint64_t spudgpu_get_fence_value(spudgpu_fence fence);

/**
 * @brief Signals a fence to a value from the CPU.
 *
 * @param[in] device Device that owns the fence.
 * @param[in] fence  Fence to signal.
 * @param[in] value  Value to signal the fence to. Greater than its current
 *                   value and than every value a submission that hasn't
 *                   finished has been asked to signal it to.
 *
 * @retval SPUD_SUCCESS The fence was signaled.
 * @retval SPUDRESULT_GPU_INVALID_DEVICE `device` is NULL.
 * @retval SPUDRESULT_GPU_INVALID_FENCE `fence` is NULL.
 * @retval SPUDRESULT_API_SPECIFIC_FAILURE The graphics API failed to signal
 *         it.
 */
SPUDRESULT spudgpu_signal_fence(
    spudgpu_device device,
    spudgpu_fence fence,
    uint64_t value);

/**
 * @brief Blocks the calling thread until one or all of the given fences have
 * reached the given values.
 *
 * `fences[i]` is waited on until its value is at least `values[i]`. A fence
 * already there counts at once, so a wait for a value of 0 never blocks.
 *
 * @param[in] device      Device that owns the fences.
 * @param[in] fences      Array of fences to wait on. No entry may be NULL.
 * @param[in] values      Array of the value to wait for on each fence.
 * @param[in] fence_count Number of elements in `fences` and in `values`. At
 *                        most 64.
 * @param[in] wait_all    If true, waits for every fence; if false, for at
 *                        least one.
 * @param[in] timeout_ns  Longest time to wait, in nanoseconds. UINT64_MAX waits
 *                        without limit; 0 only checks.
 *
 * @retval SPUD_SUCCESS The fences reached their values within the timeout.
 * @retval SPUDRESULT_GPU_INVALID_DEVICE `device` is NULL.
 * @retval SPUDRESULT_GPU_INVALID_FENCE `fences` is NULL, or one of its
 *         entries is.
 * @retval SPUDRESULT_NULL_OBJECT `values` is NULL.
 * @retval SPUDRESULT_ZERO_SIZE `fence_count` is 0.
 * @retval SPUDRESULT_DESC_INVALID_PARAMETERS `fence_count` is above 64.
 * @retval SPUDRESULT_GPU_FENCE_WAIT_TIMED_OUT The timeout ran out first.
 * @return Another SPUDRESULT if the backend fails, such as
 *         SPUDRESULT_OUT_OF_MEMORY or SPUDRESULT_API_SPECIFIC_FAILURE.
 */
SPUDRESULT spudgpu_wait_for_fences(
    spudgpu_device device,
    spudgpu_fence *fences,
    const uint64_t *values,
    uint32_t fence_count,
    bool wait_all,
    uint64_t timeout_ns);

/**
 * @brief Creates a binary semaphore for GPU-to-GPU synchronization.
 *
 * @param[in]  device        Device that owns the semaphore.
 * @param[out] out_semaphore Receives the new semaphore on success.
 *
 * @retval SPUD_SUCCESS The semaphore was created.
 * @retval SPUDRESULT_GPU_INVALID_DEVICE `device` is NULL.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_semaphore` is NULL.
 * @return Another SPUDRESULT if the backend fails, such as
 *         SPUDRESULT_OUT_OF_MEMORY or SPUDRESULT_API_SPECIFIC_FAILURE.
 *
 * @see spudgpu_destroy_semaphore()
 */
SPUDRESULT spudgpu_create_semaphore(
    spudgpu_device device,
    spudgpu_semaphore *out_semaphore);

/**
 * @brief Destroys a semaphore.
 *
 * @param[in] semaphore Semaphore to destroy.
 */
void spudgpu_destroy_semaphore(spudgpu_semaphore semaphore);

/**
 * @brief The kind of memory a buffer is allocated in.
 *
 * One value, not a bitmask. Each kind is a contract that holds on every
 * backend: what the CPU may do with the buffer, what the GPU may do with it,
 * and which usages it may be created with. Where the memory physically is
 * follows from the kind and the device, and is not part of the contract.
 *
 * | Kind            | Map | CPU writes | CPU reads | GPU reads | GPU writes |
 * |-----------------|-----|------------|-----------|-----------|------------|
 * | DEVICE          | no  | -          | -         | yes       | yes        |
 * | UPLOAD          | yes | yes        | no        | yes       | no         |
 * | READBACK        | yes | no         | yes       | no        | copies in  |
 * | DEVICE_MAPPABLE | yes | yes        | yes       | yes       | yes        |
 *
 * Two rules hold for a mapped buffer on every backend, whatever the memory
 * underneath does by itself:
 *
 * - After writing through a mapping, call spudgpu_flush_buffer() over the
 *   bytes written, while the buffer is still mapped and before submitting
 *   work that reads them. What the GPU reads from a byte written and not
 *   flushed is undefined.
 * - Before reading through a mapping, call spudgpu_invalidate_buffer() over
 *   the bytes to read, while the buffer is mapped and after the work that
 *   wrote them has finished. What the CPU reads from a byte the GPU wrote and
 *   that was not invalidated since is undefined.
 *
 * No kind and no device makes either call optional.
 *
 * Images have no memory kind. An image is always in the device's memory, is
 * never mapped, and is written and read with spudgpu_cmd_copy_buffer_to_image()
 * and spudgpu_cmd_copy_image_to_buffer().
 *
 * @see SPUDGPU_MEMORY_KIND_Constants
 */
typedef uint32_t SPUDGPU_MEMORY_KIND;

/**
 * @name Memory Kinds
 * @anchor SPUDGPU_MEMORY_KIND_Constants
 */
/** @{ */
enum {
	/**
	 * Not a kind. spudgpu_create_buffer() returns
	 * SPUDRESULT_GPU_INVALID_MEMORY_KIND for it, as for any value not listed
	 * here: a zeroed desc has not chosen its memory.
	 */
	SPUDGPU_MEMORY_KIND_NONE = 0,

	/**
	 * @brief The device's memory. Never mapped.
	 *
	 * Valid with every usage. The GPU reads and writes it; the CPU reaches
	 * it only through copies (spudgpu_cmd_copy_buffer() from an UPLOAD
	 * buffer, or into a READBACK one). spudgpu_map_buffer() returns
	 * SPUDRESULT_GPU_INVALID_MEMORY_KIND.
	 *
	 * Vulkan: a memory type with `VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT`.
	 * D3D12: `D3D12_HEAP_TYPE_DEFAULT`. Metal: `MTLStorageModePrivate`.
	 */
	SPUDGPU_MEMORY_KIND_DEVICE = 1,

	/**
	 * @brief Memory the CPU writes and the GPU reads.
	 *
	 * Mappable. The CPU writes through the mapping and flushes; reading
	 * through the mapping is outside the contract. The GPU only reads: the
	 * usage must not include SPUDGPU_BUFFER_USAGE_STORAGE,
	 * SPUDGPU_BUFFER_USAGE_TRANSFER_DST or
	 * SPUDGPU_BUFFER_USAGE_RAYTRACING_ACCELERATION_STRUCTURE, and the buffer
	 * is never given to spudgpu_cmd_pipeline_barrier(): it is readable in
	 * every way its usage allows from creation on.
	 *
	 * Vulkan: a memory type with `VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT`.
	 * D3D12: `D3D12_HEAP_TYPE_UPLOAD`. Metal: `MTLStorageModeShared` with
	 * `MTLCPUCacheModeWriteCombined`.
	 */
	SPUDGPU_MEMORY_KIND_UPLOAD = 2,

	/**
	 * @brief Memory the GPU copies into and the CPU reads.
	 *
	 * Mappable. Its usage is exactly SPUDGPU_BUFFER_USAGE_TRANSFER_DST: the
	 * only thing the GPU does with it is copy into it
	 * (spudgpu_cmd_copy_buffer(), spudgpu_cmd_copy_image_to_buffer()). The
	 * CPU invalidates and reads through the mapping; writing through it is
	 * outside the contract. The buffer is never given to
	 * spudgpu_cmd_pipeline_barrier(): it is a copy destination from creation
	 * on.
	 *
	 * Vulkan: a memory type with `VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT`, one
	 * that is also `VK_MEMORY_PROPERTY_HOST_CACHED_BIT` if the buffer can
	 * use one. D3D12: `D3D12_HEAP_TYPE_READBACK`. Metal:
	 * `MTLStorageModeShared` with `MTLCPUCacheModeDefaultCache`.
	 */
	SPUDGPU_MEMORY_KIND_READBACK = 3,

	/**
	 * @brief The device's memory, mapped: everything
	 * SPUDGPU_MEMORY_KIND_DEVICE allows, and the CPU reads and writes it
	 * too.
	 *
	 * Valid with every usage, shader-writable ones included, and moved
	 * between resource states like a DEVICE buffer. The flush and invalidate
	 * rules apply as to any mapped buffer. Nothing orders a CPU access
	 * against GPU work that is still running: that is the caller's to do.
	 *
	 * Exists only on a device whose
	 * SPUDGPU_DEVICE_PROPERTIES::device_mappable_memory is true, which is
	 * never a device with memory of its own. On any other device
	 * spudgpu_create_buffer() returns
	 * SPUDRESULT_GPU_MEMORY_KIND_NOT_SUPPORTED, and the same data goes in a
	 * DEVICE buffer written through an UPLOAD one.
	 *
	 * Vulkan: a memory type with both `VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT`
	 * and `VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT`. D3D12:
	 * `D3D12_HEAP_TYPE_CUSTOM` with `D3D12_MEMORY_POOL_L0` and
	 * `D3D12_CPU_PAGE_PROPERTY_WRITE_BACK`. Metal: `MTLStorageModeShared`
	 * with `MTLCPUCacheModeDefaultCache`.
	 */
	SPUDGPU_MEMORY_KIND_DEVICE_MAPPABLE = 4
};

/** @} */

/**
 * @brief Bitmask defining the intended pipeline binding targets for a GPU
 * buffer.
 *
 * Informs the driver how the memory will be bound within hardware command
 * buffers to allow internal optimization.
 *
 * @see SPUDGPU_BUFFER_USAGE_Constants
 */
typedef uint32_t SPUDGPU_BUFFER_USAGE;

/**
 * @name Buffer Pipeline Usage Flags
 * @anchor SPUDGPU_BUFFER_USAGE_Constants
 *
 * Bitmask flags denoting buffer compatibility. A single buffer can fulfill
 * multiple roles by combining flags via bitwise OR (`|`).
 */
/** @{ */
enum {
	/** No hardware binding target specified. */
	SPUDGPU_BUFFER_USAGE_NONE = 0,

	/**
	 * Buffer is compatible for binding as a Input Assembler Vertex Stream
	 * (Vertex Buffer).
	 */
	SPUDGPU_BUFFER_USAGE_VERTEX = 1 << 0,

	/**
	 * Buffer contains index arrays (16-bit or 32-bit indices) for indexed draw
	 * calls (Index Buffer).
	 */
	SPUDGPU_BUFFER_USAGE_INDEX = 1 << 1,

	/**
	 * @brief Buffer acts as a Uniform/Constant Buffer block (Uniform Buffer
	 * or Constant Buffer).
	 *
	 * Optimized for uniform, read-only shader data accessed globally across
	 * shader stages.
	 */
	SPUDGPU_BUFFER_USAGE_UNIFORM = 1 << 2,

	/**
	 * @brief Buffer acts as a Shader Storage Buffer Object (SSBO) /
	 * Structured Buffer.
	 *
	 * Typically used for large, variable-sized data structures requiring
	 * read/write capabilities within compute or graphics shaders.
	 */
	SPUDGPU_BUFFER_USAGE_STORAGE = 1 << 3,

	/**
	 * Buffer can be used as the source of a copy operation (e.g. a staging
	 * buffer read by spudgpu_cmd_copy_buffer_to_image).
	 */
	SPUDGPU_BUFFER_USAGE_TRANSFER_SRC = 1 << 4,

	/**
	 * Buffer can be used as the destination of a copy operation (e.g. a
	 * readback buffer written by spudgpu_cmd_copy_image_to_buffer).
	 */
	SPUDGPU_BUFFER_USAGE_TRANSFER_DST = 1 << 5,

	/**
	 * Buffer holds a built raytracing acceleration structure (BLAS/TLAS).
	 * This is a binding-compatibility fact like the other usage bits, not a
	 * resource-creation flag.
	 */
	SPUDGPU_BUFFER_USAGE_RAYTRACING_ACCELERATION_STRUCTURE = 1 << 6,

	/**
	 * @brief Buffer holds spudgpu_draw_indirect_args /
	 * spudgpu_draw_indexed_indirect_args consumed by spudgpu_cmd_draw_indirect
	 * / spudgpu_cmd_draw_indexed_indirect.
	 *
	 * Vulkan-specific creation-time fact (maps to
	 * VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT) — D3D12/Metal have no equivalent
	 * creation-time bit for indirect-argument buffers (D3D12 only cares about
	 * the resource *state* at draw time; Metal takes any id<MTLBuffer>), so
	 * this is a no-op on those two backends.
	 */
	SPUDGPU_BUFFER_USAGE_INDIRECT = 1 << 7
};

/** @} */

/**
 * @brief Descriptor used for a physical GPU buffer.
 */
typedef struct spudgpu_buffer_desc {
	/** `type` is SPUD_STRUCT_TYPE_GPU_BUFFER_DESC; `next` is NULL (nothing is chained onto
	 * this struct yet). Set by the caller. See SPUD_STRUCT_BASE. */
	SPUD_STRUCT_BASE base;
	/**
	 * Every way the buffer will be bound or copied, as a bitmask. Which
	 * usages are allowed depends on #memory_kind.
	 * @see SPUDGPU_BUFFER_USAGE
	 */
	SPUDGPU_BUFFER_USAGE usage;

	/**
	 * The kind of memory the buffer is allocated in. Decides whether it can
	 * be mapped and which usages it may have.
	 * @see SPUDGPU_MEMORY_KIND
	 */
	SPUDGPU_MEMORY_KIND memory_kind;

	/** Total allocated size of the buffer resource in bytes. */
	uint64_t size;

	/** @brief A name for diagnostics, or NULL. Copied, in every build; see
	 * spud_debug_name_set(). */
	const char *debug_name;
} spudgpu_buffer_desc;

/**
 * @brief Allocates a GPU buffer.
 *
 * The buffer's contents are undefined until written, on every backend: what
 * the CPU or the GPU reads from a byte nothing has written is not zero or
 * any other value the caller can rely on.
 *
 * @param[in]  device     Device the buffer is allocated on.
 * @param[in]  desc       Buffer configuration.
 * @param[out] out_buffer Receives the new buffer on success.
 *
 * @retval SPUD_SUCCESS The buffer was created.
 * @retval SPUDRESULT_GPU_INVALID_DEVICE `device` is NULL.
 * @retval SPUDRESULT_NULL_DESC `desc` is NULL.
 * @retval SPUDRESULT_STRUCT_WRONG_TYPE `desc->base.type` is not
 *         SPUD_STRUCT_TYPE_GPU_BUFFER_DESC.
 * @retval SPUDRESULT_STRUCT_UNKNOWN_IN_CHAIN `desc->base.next` is not NULL.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_buffer` is NULL.
 * @retval SPUDRESULT_GPU_ZERO_BUFFER_SIZE `desc->size` is 0.
 * @retval SPUDRESULT_GPU_INVALID_BUFFER_USAGE `desc->usage` is
 *         SPUDGPU_BUFFER_USAGE_NONE, or is one `desc->memory_kind` does not
 *         allow (see SPUDGPU_MEMORY_KIND).
 * @retval SPUDRESULT_GPU_INVALID_MEMORY_KIND `desc->memory_kind` is
 *         SPUDGPU_MEMORY_KIND_NONE or not a SPUDGPU_MEMORY_KIND.
 * @retval SPUDRESULT_GPU_MEMORY_KIND_NOT_SUPPORTED `desc->memory_kind` is
 *         SPUDGPU_MEMORY_KIND_DEVICE_MAPPABLE and the device's
 *         SPUDGPU_DEVICE_PROPERTIES::device_mappable_memory is false.
 * @return Another SPUDRESULT if the backend fails, such as
 *         SPUDRESULT_OUT_OF_MEMORY or SPUDRESULT_API_SPECIFIC_FAILURE.
 *
 * The checks run in the order listed: SPUDGPU_BUFFER_USAGE_NONE, then the
 * memory kind, then the usage against the kind, then the device.
 *
 * @see spudgpu_destroy_buffer()
 */
SPUDRESULT spudgpu_create_buffer(
    spudgpu_device device,
    const spudgpu_buffer_desc *desc,
    spudgpu_buffer *out_buffer);

/**
 * @brief Destroys a GPU buffer and frees its memory.
 *
 * A buffer placed in a heap (spudgpu_create_placed_buffer()) frees nothing
 * to the device: its bytes stay the heap's.
 *
 * @warning Views of the buffer must be destroyed first, and command lists that
 * reference it must have finished executing.
 *
 * @param[in] buffer Buffer to destroy.
 */
void spudgpu_destroy_buffer(spudgpu_buffer buffer);

/**
 * @brief Reads the descriptor a buffer was created with.
 *
 * `out_desc->debug_name` is the buffer's own copy of its name (see
 * spud_debug_name_set()): valid until the buffer is renamed or destroyed,
 * and NULL if it has none.
 *
 * @param[in]  buffer   Buffer to query.
 * @param[out] out_desc Receives the buffer's descriptor.
 *
 * @retval SPUD_SUCCESS `out_desc` was filled in.
 * @retval SPUDRESULT_GPU_INVALID_BUFFER `buffer` is NULL.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_desc` is NULL.
 */
SPUDRESULT spudgpu_get_buffer_desc(
    spudgpu_buffer buffer,
    spudgpu_buffer_desc *out_desc);

/**
 * @brief Reads a buffer's GPU virtual address.
 *
 * Every buffer has one, on every backend, from the moment it is created
 * until it is destroyed: D3D12's GetGPUVirtualAddress, Vulkan's
 * vkGetBufferDeviceAddress, Metal's gpuAddress. A buffer placed in a heap
 * has one too.
 *
 * @param[in]  buffer      Buffer to query.
 * @param[out] out_address Receives the address.
 *
 * @retval SPUD_SUCCESS `out_address` was filled in.
 * @retval SPUDRESULT_GPU_INVALID_BUFFER `buffer` is NULL.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_address` is NULL.
 */
SPUDRESULT spudgpu_get_buffer_gpu_address(
    spudgpu_buffer buffer,
    uint64_t *out_address);

/**
 * @brief Configuration descriptor defining a sub-allocated window (view) into
 * an existing buffer.
 *
 * Buffer views are lightweight abstractions allowing you to reinterpret
 * specific segments of a parent buffer without duplicating or allocating
 * additional physical device memory.
 */
typedef struct spudgpu_buffer_view_desc {
	/** `type` is SPUD_STRUCT_TYPE_GPU_BUFFER_VIEW_DESC; `next` is NULL (nothing is chained onto
	 * this struct yet). Set by the caller. See SPUD_STRUCT_BASE. */
	SPUD_STRUCT_BASE base;
	/** Handle to the GPU buffer that this view window reads from or writes to. */
	spudgpu_buffer parent_buffer;

	/**
	 * The byte offset to where this specific view window begins inside the
	 * parent buffer.
	 */
	uint64_t offset_from_parent_buffer;

	/**
	 * @brief The data structure element stride in bytes.
	 *
	 * Crucial for structured storage arrays or vertex inputs (e.g., byte size
	 * of a single instance of your custom Vertex struct). Pass `0` for raw,
	 * unformatted buffers.
	 */
	uint64_t stride;

	/** The span/length of this specific buffer view window in bytes. */
	uint64_t size;
} spudgpu_buffer_view_desc;

/**
 * @brief Creates a view of a range of an existing GPU buffer.
 *
 * @param[in]  buffer          Buffer the view looks into.
 * @param[in]  desc            View configuration.
 * @param[out] out_buffer_view Receives the new view on success.
 *
 * @retval SPUD_SUCCESS The view was created.
 * @retval SPUDRESULT_GPU_INVALID_BUFFER `buffer` is NULL.
 * @retval SPUDRESULT_NULL_DESC `desc` is NULL.
 * @retval SPUDRESULT_STRUCT_WRONG_TYPE `desc->base.type` is not
 *         SPUD_STRUCT_TYPE_GPU_BUFFER_VIEW_DESC.
 * @retval SPUDRESULT_STRUCT_UNKNOWN_IN_CHAIN `desc->base.next` is not NULL.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_buffer_view` is NULL.
 * @retval SPUDRESULT_GPU_ZERO_BUFFER_SIZE `desc->size` is 0.
 * @retval SPUDRESULT_GPU_BUFFER_OR_IMAGE_VIEW_RANGE_OUT_OF_SCOPE
 *         `desc->offset_from_parent_buffer + desc->size` runs past the end of
 *         the buffer.
 * @return Another SPUDRESULT if the backend fails, such as
 *         SPUDRESULT_OUT_OF_MEMORY or SPUDRESULT_API_SPECIFIC_FAILURE.
 *
 * @see spudgpu_destroy_buffer_view()
 */
SPUDRESULT spudgpu_create_buffer_view(
    spudgpu_buffer buffer,
    const spudgpu_buffer_view_desc *desc,
    spudgpu_buffer_view *out_buffer_view);

/**
 * @brief Destroys a GPU buffer view.
 *
 * The parent buffer and its contents are not affected.
 *
 * @param[in] buffer_view View to destroy.
 */
void spudgpu_destroy_buffer_view(spudgpu_buffer_view buffer_view);

/**
 * @brief Reads the descriptor a buffer view was created with.
 *
 * @param[in]  view     Buffer view to query.
 * @param[out] out_desc Receives the view's descriptor.
 *
 * @retval SPUD_SUCCESS `out_desc` was filled in.
 * @retval SPUDRESULT_GPU_INVALID_BUFFER_VIEW `view` is NULL.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_desc` is NULL.
 */
SPUDRESULT spudgpu_get_buffer_view_desc(
    spudgpu_buffer_view view,
    spudgpu_buffer_view_desc *out_desc);

/**
 * @brief Maps a buffer's memory into CPU-accessible address space.
 *
 * Valid on a buffer of every SPUDGPU_MEMORY_KIND except
 * SPUDGPU_MEMORY_KIND_DEVICE. The kind decides what may be done through the
 * pointer: see SPUDGPU_MEMORY_KIND.
 *
 * A buffer has one mapping at a time: it is unmapped before it is mapped
 * again. It may stay mapped for as long as it exists, including while the GPU
 * uses it.
 *
 * @param[in]  buffer Buffer to map.
 * @param[in]  offset Byte offset into the buffer where the mapped region
 *                    begins.
 * @param[in]  size   Number of bytes to map. Pass 0 to map the entire buffer.
 * @param[out] ppData Receives the CPU address of the byte at `offset`.
 *
 * @retval SPUD_SUCCESS The buffer was mapped.
 * @retval SPUDRESULT_GPU_INVALID_BUFFER `buffer` is NULL.
 * @retval SPUDRESULT_GPU_MAP_OUT_OF_RANGE `offset + size` runs past the end of
 *         the buffer.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `ppData` is NULL.
 * @retval SPUDRESULT_GPU_INVALID_MEMORY_KIND The buffer is of
 *         SPUDGPU_MEMORY_KIND_DEVICE.
 * @retval SPUDRESULT_API_SPECIFIC_FAILURE The graphics API failed to map it.
 *
 * @see spudgpu_unmap_buffer()
 */
SPUDRESULT spudgpu_map_buffer(
    spudgpu_buffer buffer,
    uint64_t offset,
    uint64_t size,
    void **ppData);

/**
 * @brief Unmaps a mapped buffer; the pointer from spudgpu_map_buffer() is no
 * longer valid.
 *
 * Unmapping flushes nothing: bytes written through the mapping are flushed
 * with spudgpu_flush_buffer() first.
 *
 * @param[in] buffer Buffer to unmap.
 */
void spudgpu_unmap_buffer(spudgpu_buffer buffer);

/**
 * @brief Makes bytes written through a mapping visible to the GPU.
 *
 * Called after every write through a mapping, on every backend and every
 * device, over the bytes written: while the buffer is still mapped, and
 * before submitting work that reads them. `offset` is from the start of the
 * buffer, not of the mapping, and the range lies inside what was mapped.
 *
 * Valid on a buffer of SPUDGPU_MEMORY_KIND_UPLOAD or
 * SPUDGPU_MEMORY_KIND_DEVICE_MAPPABLE, the kinds the CPU writes.
 *
 * @param[in] buffer Buffer whose writes need flushing.
 * @param[in] offset Byte offset of the written region.
 * @param[in] size   Byte length of the written region. Pass 0 for everything
 *                   from `offset` to the end of the buffer.
 *
 * @retval SPUD_SUCCESS The range was flushed.
 * @retval SPUDRESULT_GPU_INVALID_BUFFER `buffer` is NULL.
 * @retval SPUDRESULT_GPU_INVALID_MEMORY_KIND The buffer is of
 *         SPUDGPU_MEMORY_KIND_DEVICE or SPUDGPU_MEMORY_KIND_READBACK.
 * @retval SPUDRESULT_GPU_MAP_OUT_OF_RANGE `offset + size` runs past the end of
 *         the buffer.
 * @retval SPUDRESULT_API_SPECIFIC_FAILURE The graphics API failed to flush
 *         it.
 */
SPUDRESULT spudgpu_flush_buffer(
    spudgpu_buffer buffer,
    uint64_t offset,
    uint64_t size);

/**
 * @brief Makes bytes the GPU wrote visible through a mapping.
 *
 * Called before every read through a mapping of bytes the GPU has written
 * since they were last invalidated, on every backend and every device: while
 * the buffer is mapped, and after the work that wrote them has finished.
 * `offset` is from the start of the buffer, not of the mapping, and the
 * range lies inside what was mapped.
 *
 * Valid on a buffer of SPUDGPU_MEMORY_KIND_READBACK or
 * SPUDGPU_MEMORY_KIND_DEVICE_MAPPABLE, the kinds the CPU reads.
 *
 * @param[in] buffer Buffer to invalidate.
 * @param[in] offset Byte offset of the region to invalidate.
 * @param[in] size   Byte length of the region. Pass 0 for everything from
 *                   `offset` to the end of the buffer.
 *
 * @retval SPUD_SUCCESS The range was invalidated.
 * @retval SPUDRESULT_GPU_INVALID_BUFFER `buffer` is NULL.
 * @retval SPUDRESULT_GPU_INVALID_MEMORY_KIND The buffer is of
 *         SPUDGPU_MEMORY_KIND_DEVICE or SPUDGPU_MEMORY_KIND_UPLOAD.
 * @retval SPUDRESULT_GPU_MAP_OUT_OF_RANGE `offset + size` runs past the end of
 *         the buffer.
 * @retval SPUDRESULT_API_SPECIFIC_FAILURE The graphics API failed to
 *         invalidate it.
 */
SPUDRESULT spudgpu_invalidate_buffer(
    spudgpu_buffer buffer,
    uint64_t offset,
    uint64_t size);

/**
 * @brief Dimensionality of an image.
 *
 * One value: the constants are numbered as bits, and are not combined.
 */
typedef uint32_t SPUDGPU_IMAGE_TYPE;

enum {
	/** Not a type. */
	SPUDGPU_IMAGE_TYPE_NONE = 0,
	/** One dimension: `height` and `depth` are 1. */
	SPUDGPU_IMAGE_TYPE_1D   = 1 << 0,
	/**
	 * Two dimensions: `depth` is 1. Also the type of a cube's image: a cube
	 * is not an image type but a way of viewing six layers of a 2D image
	 * created with spudgpu_image_desc::cube_capable.
	 */
	SPUDGPU_IMAGE_TYPE_2D   = 1 << 1,
	/** Three dimensions. */
	SPUDGPU_IMAGE_TYPE_3D   = 1 << 2
};

/**
 * @brief Bitmask determining the type and capabilities of a GPU image object.
 *
 * @see SPUDGPU_IMAGE_USAGE_Constants
 */
typedef uint32_t SPUDGPU_IMAGE_USAGE;

/**
 * @name Image Usage Flags
 * @anchor SPUDGPU_IMAGE_USAGE_Constants
 *
 * Bitmask configurations dictating how the GPU image object is structured and
 * sampled.
 */
/** @{ */
enum {
	/** No usage target specified. Uninitialized configuration state. */
	SPUDGPU_IMAGE_USAGE_NONE = 0,

	/** Read by a shader through a sampled image binding. */
	SPUDGPU_IMAGE_USAGE_SAMPLED = 1 << 0,

	/** Rendered into as a color attachment. */
	SPUDGPU_IMAGE_USAGE_COLOR_ATTACHMENT         = 1 << 1,
	/** Rendered into as the depth/stencil attachment. */
	SPUDGPU_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT = 1 << 2,
	/** Read and written by a shader through a storage image binding. */
	SPUDGPU_IMAGE_USAGE_STORAGE                  = 1 << 3,
	/** Read by a copy or blit. */
	SPUDGPU_IMAGE_USAGE_TRANSFER_SRC             = 1 << 4,
	/** Written by a copy or blit. */
	SPUDGPU_IMAGE_USAGE_TRANSFER_DST             = 1 << 5,

	/**
	 * Image is compatible with being presented to a swap chain surface.
	 * Reserved for future callers creating a presentable image outside of
	 * spudgpu_swap_chain's own internally-owned backbuffers — no current
	 * caller in this workspace sets it.
	 */
	SPUDGPU_IMAGE_USAGE_PRESENTABLE               = 1 << 6,

	/**
	 * @brief Marks a render-pass attachment as transient: written and
	 * read only within the render pass that produces it, never sampled,
	 * read back, or persisted afterward.
	 *
	 * On tile-based GPUs (all Metal devices, and mobile Vulkan hardware such
	 * as Mali/Adreno) this lets the implementation back the image with
	 * on-chip tile memory instead of a real allocation, entirely skipping
	 * device-memory traffic — the standard technique for a multisampled
	 * render target that only exists to be resolved, or a deferred-shading
	 * G-buffer read back via programmable blending within the same pass.
	 * Maps to Metal's `MTLStorageModeMemoryless` and Vulkan's
	 * `VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT` +
	 * `VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT`; D3D12 has no equivalent and
	 * treats it as a no-op hint.
	 *
	 * Must be combined with `SPUDGPU_IMAGE_USAGE_COLOR_ATTACHMENT` and/or
	 * `SPUDGPU_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT`, and must not be
	 * combined with `SPUDGPU_IMAGE_USAGE_SAMPLED`, `_STORAGE`,
	 * `_TRANSFER_SRC`, `_TRANSFER_DST`, or `_PRESENTABLE` — all of those
	 * require the image to be readable/writable outside the render pass
	 * that produced it,
	 * which a transient attachment structurally cannot be. Enforced at
	 * creation on every backend, not just the ones that act on it, so a
	 * desc that's invalid here doesn't silently behave differently
	 * depending on which backend happens to be compiled in.
	 */
	SPUDGPU_IMAGE_USAGE_TRANSIENT_ATTACHMENT      = 1 << 7
};


/**
 * @brief A depth value and a stencil value, used together as a clear value.
 */
/* Frozen: a value, held by value in SPUDGPU_CLEAR_VALUE. It never changes; a new need gets a new type. */
typedef struct SPUDGPU_DEPTH_STENCIL_VALUE {
	/** Depth, 0 to 1. */
	float depth;
	/** Stencil. */
	uint8_t stencil;
} SPUDGPU_DEPTH_STENCIL_VALUE;

/**
 * @brief The value an image is expected to be cleared to: a color, or a
 * depth/stencil pair, depending on `format`.
 */
/* Frozen: a value, held by value in spudgpu_image_desc. It never changes; a new need gets a new type. */
typedef struct SPUDGPU_CLEAR_VALUE {
	/** The format the value is for. */
	SPUDGPU_FORMAT format;
	union {
		/** RGBA, for a color format. */
		float color[4];
		/** For a depth/stencil format. */
		SPUDGPU_DEPTH_STENCIL_VALUE depth_stencil;
	};
} SPUDGPU_CLEAR_VALUE;

/** @} */

/**
 * @brief Configuration descriptor used to allocate a physical GPU image
 * resource (texture/surface).
 *
 * There is no memory kind to choose: an image is always in the device's
 * memory and is never mapped (see SPUDGPU_MEMORY_KIND).
 */
typedef struct spudgpu_image_desc {
	/** `type` is SPUD_STRUCT_TYPE_GPU_IMAGE_DESC; `next` is NULL (nothing is chained onto
	 * this struct yet). Set by the caller. See SPUD_STRUCT_BASE. */
	SPUD_STRUCT_BASE base;
	/**
	 * Every way the image will be used, as a bitmask.
	 * @see SPUDGPU_IMAGE_USAGE
	 */
	SPUDGPU_IMAGE_USAGE usage;

	/**
	 * The image's dimensionality.
	 * @see SPUDGPU_IMAGE_TYPE
	 */
	SPUDGPU_IMAGE_TYPE type;

	/**
	 * The value an attachment will most often be cleared to. D3D12 creates
	 * a color or depth/stencil attachment with it as the optimized clear
	 * value, taking the format from #format and not from
	 * SPUDGPU_CLEAR_VALUE::format. Vulkan and Metal do not read it.
	 */
	SPUDGPU_CLEAR_VALUE clear_value;

	/**
	 * The texel data layout and channel bit-depth configuration (e.g.,
	 * RGBA8_UNORM, R32_SFLOAT).
	 * @see SPUDGPU_FORMAT
	 */
	SPUDGPU_FORMAT format;

	/** The horizontal width the image base layer in texels/pixels. */
	uint32_t width;

	/** The vertical height of the image base layer in texels/pixels. */
	uint32_t height;

	/**
	 * @brief The volumetric depth of the image in texels.
	 *
	 * Must be set to `1` for 1D and 2D images. The third axis of a
	 * SPUDGPU_IMAGE_TYPE_3D image.
	 */
	uint32_t depth;

	/**
	 * @brief Number of layers inside a texture array allocation.
	 *
	 * Allows for texture arrays (e.g., a series of 2D textures grouped into a
	 * single bindable allocation). Must be set to at least `1` for standard
	 * standalone textures.
	 */
	uint32_t array_layers;

	/**
	 * @brief Total number of downsampled mipmap resolution steps allocated
	 * for this texture.
	 *
	 * Pass `1` for no mipmaps. Level [0] represents the original
	 * full-resolution image data.
	 */
	uint32_t mip_levels;
	/**
	 * @brief Whether the image's layers can be viewed as the faces of cubes.
	 *
	 * Required for a view of SPUDGPU_IMAGE_VIEW_TYPE_CUBE or
	 * SPUDGPU_IMAGE_VIEW_TYPE_CUBE_ARRAY, and it is the same rule on every
	 * backend, though only one graphics API has to be told when the image is
	 * created: a cube view of an image created without it is refused
	 * everywhere. Such an image is SPUDGPU_IMAGE_TYPE_2D, has `width` equal
	 * to `height`, and has `array_layers` a multiple of six, each six in a
	 * row being the +X, -X, +Y, -Y, +Z and -Z faces of one cube. It is
	 * otherwise an ordinary layered 2D image: one layer can still be viewed
	 * as SPUDGPU_IMAGE_VIEW_TYPE_2D, to render a face, and several as
	 * SPUDGPU_IMAGE_VIEW_TYPE_2D_ARRAY.
	 */
	bool cube_capable;
	/** @brief A name for diagnostics, or NULL. Copied, in every build; see
	 * spud_debug_name_set(). */
	const char *debug_name;
} spudgpu_image_desc;

/**
 * @brief Allocates a GPU image (texture).
 *
 * The image's contents are undefined until written or cleared, on every
 * backend: what is read from a texel nothing has written is not zero or any
 * other value the caller can rely on.
 *
 * @param[in]  device    Device the image is allocated on.
 * @param[in]  desc      Image configuration.
 * @param[out] out_image Receives the new image on success.
 *
 * @retval SPUD_SUCCESS The image was created.
 * @retval SPUDRESULT_GPU_INVALID_DEVICE `device` is NULL.
 * @retval SPUDRESULT_NULL_DESC `desc` is NULL.
 * @retval SPUDRESULT_STRUCT_WRONG_TYPE `desc->base.type` is not
 *         SPUD_STRUCT_TYPE_GPU_IMAGE_DESC.
 * @retval SPUDRESULT_STRUCT_UNKNOWN_IN_CHAIN `desc->base.next` is not NULL.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_image` is NULL.
 * @retval SPUDRESULT_DESC_INVALID_PARAMETERS `desc->width`, `height`, `depth`,
 *         `array_layers` or `mip_levels` is 0.
 * @retval SPUDRESULT_GPU_INVALID_IMAGE_USAGE `desc->usage` is
 *         SPUDGPU_IMAGE_USAGE_NONE, or combines
 *         SPUDGPU_IMAGE_USAGE_TRANSIENT_ATTACHMENT with usages it does not
 *         allow.
 * @retval SPUDRESULT_GPU_INVALID_IMAGE_TYPE `desc->cube_capable` is set and
 *         `desc->type` is not SPUDGPU_IMAGE_TYPE_2D.
 * @retval SPUDRESULT_DESC_INVALID_PARAMETERS `desc->cube_capable` is set and
 *         `desc->width` is not `desc->height`, or `desc->array_layers` is not
 *         a multiple of six.
 * @retval SPUDRESULT_GPU_INVALID_FORMAT `desc->format` is
 *         SPUDGPU_FORMAT_UNKNOWN.
 * @return Another SPUDRESULT if the backend fails, such as
 *         SPUDRESULT_OUT_OF_MEMORY or SPUDRESULT_API_SPECIFIC_FAILURE.
 *
 * @see spudgpu_destroy_image()
 */
SPUDRESULT spudgpu_create_image(
    spudgpu_device device,
    const spudgpu_image_desc *desc,
    spudgpu_image *out_image);

/**
 * @brief Destroys a GPU image and frees its memory.
 *
 * An image placed in a heap (spudgpu_create_placed_image()) frees nothing
 * to the device: its bytes stay the heap's.
 *
 * @warning Views of the image must be destroyed first, and command lists that
 * reference it must have finished executing.
 *
 * @param[in] image Image to destroy.
 */
void spudgpu_destroy_image(spudgpu_image image);

/**
 * @brief Reads the descriptor an image was created with.
 *
 * `out_desc->debug_name` is the image's own copy of its name (see
 * spud_debug_name_set()): valid until the image is renamed or destroyed,
 * and NULL if it has none.
 *
 * @param[in]  image    Image to query.
 * @param[out] out_desc Receives the image's descriptor.
 *
 * @retval SPUD_SUCCESS `out_desc` was filled in.
 * @retval SPUDRESULT_GPU_INVALID_IMAGE `image` is NULL.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_desc` is NULL.
 */
SPUDRESULT spudgpu_get_image_desc(
    spudgpu_image image,
    spudgpu_image_desc *out_desc);

/**
 * @brief How an image view presents its image to shaders.
 *
 * One value: the constants are numbered as bits, and are not combined.
 */
typedef uint32_t SPUDGPU_IMAGE_VIEW_TYPE;

enum {
	/** Not a type. */
	SPUDGPU_IMAGE_VIEW_TYPE_NONE       = 0,
	/** One layer of a 1D image. */
	SPUDGPU_IMAGE_VIEW_TYPE_1D         = 1 << 0,
	/** One layer of a 2D image. */
	SPUDGPU_IMAGE_VIEW_TYPE_2D         = 1 << 1,
	/** A 3D image. */
	SPUDGPU_IMAGE_VIEW_TYPE_3D         = 1 << 2,
	/**
	 * Six layers of a 2D image, as the faces of a cube. The image is
	 * spudgpu_image_desc::cube_capable, and the view's range is exactly
	 * layers 0 to 5: `base_array_layer` 0 and `array_layer_count` 6. One
	 * cube out of several is reached with SPUDGPU_IMAGE_VIEW_TYPE_CUBE_ARRAY,
	 * since not every graphics API can start a single cube's view at
	 * another layer.
	 */
	SPUDGPU_IMAGE_VIEW_TYPE_CUBE       = 1 << 3,
	/** Several layers of a 1D image. */
	SPUDGPU_IMAGE_VIEW_TYPE_1D_ARRAY   = 1 << 4,
	/** Several layers of a 2D image. */
	SPUDGPU_IMAGE_VIEW_TYPE_2D_ARRAY   = 1 << 5,
	/**
	 * A multiple of six layers of a 2D image, as an array of cubes. The
	 * image is spudgpu_image_desc::cube_capable, and `array_layer_count` is
	 * a multiple of six and not 0.
	 */
	SPUDGPU_IMAGE_VIEW_TYPE_CUBE_ARRAY = 1 << 6
};

/**
 * @brief The aspects an image's format can have, for
 * spudgpu_image_view_desc_subresource_range::aspect_mask.
 *
 * The values are `VkImageAspectFlagBits`' (Vulkan). D3D12 reaches the
 * stencil as plane 1 of the resource through a view format of its own, and
 * Metal through a view in a stencil pixel format.
 */
typedef uint64_t SPUDGPU_IMAGE_ASPECT;
enum {
	/** No aspect. Refused wherever an aspect mask is taken. */
	SPUDGPU_IMAGE_ASPECT_NONE    = 0,
	/** The color of a color format. */
	SPUDGPU_IMAGE_ASPECT_COLOR   = 1,
	/** The depth of a depth format, or of a depth and stencil format. */
	SPUDGPU_IMAGE_ASPECT_DEPTH   = 2,
	/** The stencil of a depth and stencil format. */
	SPUDGPU_IMAGE_ASPECT_STENCIL = 4
};

/**
 * @brief Defines a specific subsection (slice) of a multi-layered or mipmapped
 * texture.
 *
 * Allows a pipeline stage to isolate and bind specific layers or mip levels
 * of a parent image rather than the entire allocation.
 */
/* Frozen: held by value in spudgpu_image_view_desc. It never changes; a new need gets a new type. */
typedef struct spudgpu_image_view_desc_subresource_range {
	/**
	 * @brief Which aspects of the image the range covers, as
	 * SPUDGPU_IMAGE_ASPECT bits.
	 *
	 * For a view (spudgpu_create_image_view()) these rules hold on every
	 * backend, whatever the graphics API underneath would also accept:
	 *
	 * - Never 0, and never an aspect the image's format lacks: a color
	 *   format has SPUDGPU_IMAGE_ASPECT_COLOR alone, a depth format
	 *   SPUDGPU_IMAGE_ASPECT_DEPTH alone, and a depth and stencil format
	 *   SPUDGPU_IMAGE_ASPECT_DEPTH and SPUDGPU_IMAGE_ASPECT_STENCIL. A view
	 *   that breaks this is refused.
	 * - A view given to spudgpu_cmd_begin_rendering() as the depth/stencil
	 *   attachment covers every aspect of its format.
	 * - A view written into a descriptor set covers exactly one aspect. Of
	 *   a depth and stencil format that is the depth, read as a float in
	 *   the first component, or the stencil, read as an unsigned integer
	 *   in the first component. What a shader reads through a view of both
	 *   aspects is undefined.
	 * - What a shader reads from the components after the first, through a
	 *   depth or a stencil view, is undefined.
	 *
	 * So a depth and stencil image that is rendered to and sampled has a
	 * view for each use.
	 */
	uint64_t aspect_mask;

	/**
	 * The starting mipmap level index for this view window (0 being the
	 * highest resolution).
	 */
	uint64_t base_mip_level;

	/**
	 * The total number of downsampled mipmap levels to include in this view
	 * range.
	 */
	uint64_t mip_level_count;

	/**
	 * The starting array layer index for this view window (used for texture
	 * arrays/cubemaps).
	 */
	uint64_t base_array_layer;

	/** The total number of layers to include in this view range. */
	uint64_t array_layer_count;
} spudgpu_image_view_desc_subresource_range;

/**
 * @brief Configuration descriptor defining a read/write window (view) into an
 * existing GPU image.
 *
 * Like buffer views, image views do not allocate raw VRAM; they wrap existing
 * physical images to declare how shader stages should interpret their
 * boundaries and contents.
 *
 * @todo Component swizzle is not exposed: a view reads its image's channels
 * in their own order.
 */
typedef struct spudgpu_image_view_desc {
	/** `type` is SPUD_STRUCT_TYPE_GPU_IMAGE_VIEW_DESC; `next` is NULL (nothing is chained onto
	 * this struct yet). Set by the caller. See SPUD_STRUCT_BASE. */
	SPUD_STRUCT_BASE base;
	/** Handle to the physical GPU image containing the pixel data. */
	spudgpu_image parent_image;

	/**
	 * The image view type
	 * @see SPUDGPU_IMAGE_VIEW_TYPE
	 */
	uint32_t type;

	/**
	 * The isolated subresource layer and mip boundary definitions for this
	 * view.
	 */
	spudgpu_image_view_desc_subresource_range subresource_range;
} spudgpu_image_view_desc;

/**
 * @brief Creates a view of a subresource range of a GPU image.
 *
 * @param[in]  image          Image the view looks into.
 * @param[in]  desc           View configuration.
 * @param[out] out_image_view Receives the new view on success.
 *
 * @retval SPUD_SUCCESS The view was created.
 * @retval SPUDRESULT_GPU_INVALID_IMAGE `image` is NULL.
 * @retval SPUDRESULT_NULL_DESC `desc` is NULL.
 * @retval SPUDRESULT_STRUCT_WRONG_TYPE `desc->base.type` is not
 *         SPUD_STRUCT_TYPE_GPU_IMAGE_VIEW_DESC.
 * @retval SPUDRESULT_STRUCT_UNKNOWN_IN_CHAIN `desc->base.next` is not NULL.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_image_view` is NULL.
 * @retval SPUDRESULT_GPU_INVALID_IMAGE_VIEW_TYPE `desc->type` is
 *         SPUDGPU_IMAGE_VIEW_TYPE_CUBE or SPUDGPU_IMAGE_VIEW_TYPE_CUBE_ARRAY
 *         and the image was not created with
 *         spudgpu_image_desc::cube_capable, or it is
 *         SPUDGPU_IMAGE_VIEW_TYPE_CUBE_ARRAY on a device without
 *         SPUDGPU_DEVICE_PROPERTIES::cube_array_views.
 * @retval SPUDRESULT_GPU_BUFFER_OR_IMAGE_VIEW_RANGE_OUT_OF_SCOPE `desc->type`
 *         is SPUDGPU_IMAGE_VIEW_TYPE_CUBE and the range is not layers 0 to
 *         5, or it is SPUDGPU_IMAGE_VIEW_TYPE_CUBE_ARRAY and
 *         `array_layer_count` is 0 or not a multiple of six.
 * @retval SPUDRESULT_DESC_INVALID_PARAMETERS
 *         `desc->subresource_range.aspect_mask` is 0 or names an aspect the
 *         image's format lacks.
 * @return Another SPUDRESULT if the backend fails, such as
 *         SPUDRESULT_OUT_OF_MEMORY or SPUDRESULT_API_SPECIFIC_FAILURE.
 *
 * @see spudgpu_destroy_image_view()
 */
SPUDRESULT spudgpu_create_image_view(
    spudgpu_image image,
    const spudgpu_image_view_desc *desc,
    spudgpu_image_view *out_image_view);

/**
 * @brief Destroys a GPU image view.
 *
 * The parent image and its contents are not affected.
 *
 * @param[in] image_view View to destroy.
 */
void spudgpu_destroy_image_view(spudgpu_image_view image_view);

/**
 * @brief Reads the descriptor an image view was created with.
 *
 * @param[in]  image_view Image view to query.
 * @param[out] out_desc   Receives the view's descriptor.
 *
 * @retval SPUD_SUCCESS `out_desc` was filled in.
 * @retval SPUDRESULT_GPU_INVALID_IMAGE_VIEW `image_view` is NULL.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_desc` is NULL.
 */
SPUDRESULT spudgpu_get_image_view_desc(
    spudgpu_image_view image_view,
    spudgpu_image_view_desc *out_desc);

// ============================================================================
//  Heaps and placed resources
//  Maps to: VkDeviceMemory (Vulkan), ID3D12Heap (D3D12), MTLHeap (Metal)
// ============================================================================

/**
 * @brief What a heap holds: buffers, images that are never attachments, or
 * images that are.
 *
 * A heap holds one class and nothing else, on every backend. That is D3D12's
 * resource heap tier 1 made the rule (D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS,
 * _ALLOW_ONLY_NON_RT_DS_TEXTURES, _ALLOW_ONLY_RT_DS_TEXTURES, set on every
 * D3D12 device, tier 2 included). Vulkan picks a heap's memory type by its
 * class, and never has buffers and images in one allocation.
 *
 * Which class an image is in follows from its usage, not a choice: one with
 * SPUDGPU_IMAGE_USAGE_COLOR_ATTACHMENT or
 * SPUDGPU_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT goes in an
 * SPUDGPU_HEAP_RESOURCE_CLASS_ATTACHMENT_IMAGES heap, any other in an
 * SPUDGPU_HEAP_RESOURCE_CLASS_IMAGES heap.
 *
 * @see SPUDGPU_HEAP_RESOURCE_CLASS_Constants
 */
typedef uint32_t SPUDGPU_HEAP_RESOURCE_CLASS;

/**
 * @name Heap Resource Classes
 * @anchor SPUDGPU_HEAP_RESOURCE_CLASS_Constants
 */
/** @{ */
enum {
	/**
	 * Not a class. spudgpu_create_heap() returns
	 * SPUDRESULT_GPU_INVALID_HEAP_RESOURCE_CLASS for it, as for any value
	 * not listed here: a zeroed desc has not chosen.
	 */
	SPUDGPU_HEAP_RESOURCE_CLASS_NONE = 0,
	/** Buffers of the heap's memory kind. */
	SPUDGPU_HEAP_RESOURCE_CLASS_BUFFERS = 1,
	/** Images with neither attachment usage. The heap's kind is
	 *  SPUDGPU_MEMORY_KIND_DEVICE. */
	SPUDGPU_HEAP_RESOURCE_CLASS_IMAGES = 2,
	/** Images with a color or depth/stencil attachment usage. The heap's
	 *  kind is SPUDGPU_MEMORY_KIND_DEVICE. */
	SPUDGPU_HEAP_RESOURCE_CLASS_ATTACHMENT_IMAGES = 3
};
/** @} */

/**
 * Every heap's size is a multiple of this many bytes. It is D3D12's heap
 * alignment (`D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT`) made the rule.
 */
#define SPUDGPU_HEAP_SIZE_GRANULARITY 65536u

/**
 * @brief Describes a heap: one device allocation that buffers and images are
 * placed in at offsets the caller chooses.
 *
 * SpudGPU keeps no record of what is placed where in a heap. Which heap a
 * resource goes in, at which offset, and when a heap is created or destroyed
 * are the caller's.
 */
typedef struct spudgpu_heap_desc {
	/** `type` is SPUD_STRUCT_TYPE_GPU_HEAP_DESC; `next` is NULL (nothing is chained onto
	 * this struct yet). Set by the caller. See SPUD_STRUCT_BASE. */
	SPUD_STRUCT_BASE base;
	/**
	 * The memory every resource placed in the heap is in, with the contract
	 * SPUDGPU_MEMORY_KIND states for a buffer of that kind. An image heap's
	 * is SPUDGPU_MEMORY_KIND_DEVICE: images have no other.
	 * @see SPUDGPU_MEMORY_KIND
	 */
	SPUDGPU_MEMORY_KIND memory_kind;

	/** What the heap holds. @see SPUDGPU_HEAP_RESOURCE_CLASS */
	SPUDGPU_HEAP_RESOURCE_CLASS resource_class;

	/** Bytes. A multiple of SPUDGPU_HEAP_SIZE_GRANULARITY, not 0. */
	uint64_t size;
	/** @brief A name for diagnostics, or NULL. Copied, in every build; see
	 * spud_debug_name_set(). */
	const char *debug_name;
} spudgpu_heap_desc;

/**
 * @brief Allocates a heap.
 *
 * Its contents are undefined, as a new buffer's are: placing a resource in it
 * does not clear the bytes the resource covers.
 *
 * @param[in]  device   Device the heap is allocated on.
 * @param[in]  desc     Heap configuration.
 * @param[out] out_heap Receives the new heap on success.
 *
 * @retval SPUD_SUCCESS The heap was created.
 * @retval SPUDRESULT_GPU_INVALID_DEVICE `device` is NULL.
 * @retval SPUDRESULT_NULL_DESC `desc` is NULL.
 * @retval SPUDRESULT_STRUCT_WRONG_TYPE `desc->base.type` is not
 *         SPUD_STRUCT_TYPE_GPU_HEAP_DESC.
 * @retval SPUDRESULT_STRUCT_UNKNOWN_IN_CHAIN `desc->base.next` is not NULL.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_heap` is NULL.
 * @retval SPUDRESULT_ZERO_SIZE `desc->size` is 0.
 * @retval SPUDRESULT_GPU_INVALID_HEAP_SIZE `desc->size` is not a multiple of
 *         SPUDGPU_HEAP_SIZE_GRANULARITY.
 * @retval SPUDRESULT_GPU_INVALID_HEAP_RESOURCE_CLASS `desc->resource_class`
 *         is SPUDGPU_HEAP_RESOURCE_CLASS_NONE or not a class.
 * @retval SPUDRESULT_GPU_INVALID_MEMORY_KIND `desc->memory_kind` is
 *         SPUDGPU_MEMORY_KIND_NONE or not a kind, or the class is an image
 *         class and the kind is not SPUDGPU_MEMORY_KIND_DEVICE.
 * @retval SPUDRESULT_GPU_MEMORY_KIND_NOT_SUPPORTED The kind is
 *         SPUDGPU_MEMORY_KIND_UPLOAD, SPUDGPU_MEMORY_KIND_READBACK or
 *         SPUDGPU_MEMORY_KIND_DEVICE_MAPPABLE and the device's
 *         SPUDGPU_DEVICE_PROPERTIES::mappable_heaps is false, or it is
 *         SPUDGPU_MEMORY_KIND_DEVICE_MAPPABLE and
 *         SPUDGPU_DEVICE_PROPERTIES::device_mappable_memory is false.
 * @return Another SPUDRESULT if the backend fails, such as
 *         SPUDRESULT_OUT_OF_MEMORY or SPUDRESULT_API_SPECIFIC_FAILURE (the
 *         device having no room for a heap that size is the usual one).
 *
 * The checks run in the order listed.
 *
 * @see spudgpu_destroy_heap()
 */
SPUDRESULT spudgpu_create_heap(
    spudgpu_device device,
    const spudgpu_heap_desc *desc,
    spudgpu_heap *out_heap);

/**
 * @brief Frees a heap's memory.
 *
 * @warning Every buffer and image placed in the heap must be destroyed first,
 * which means the command lists that reference them have finished. A debug
 * build asserts that none is left.
 *
 * @param[in] heap Heap to destroy. NULL does nothing.
 */
void spudgpu_destroy_heap(spudgpu_heap heap);

/**
 * @brief Reads the descriptor a heap was created with.
 *
 * @param[in]  heap     Heap to query.
 * @param[out] out_desc Receives the heap's descriptor.
 *
 * @retval SPUD_SUCCESS `out_desc` was filled in.
 * @retval SPUDRESULT_GPU_INVALID_HEAP `heap` is NULL.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_desc` is NULL.
 */
SPUDRESULT spudgpu_get_heap_desc(
    spudgpu_heap heap,
    spudgpu_heap_desc *out_desc);

/**
 * @brief What a resource needs from a heap: how many bytes from its offset,
 * and what its offset is a multiple of.
 */
typedef struct spudgpu_placement_requirements {
	/** `type` is SPUD_STRUCT_TYPE_GPU_PLACEMENT_REQUIREMENTS; `next` is NULL (nothing is chained onto
	 * this struct yet). The caller builds the chain and sets each type; SpudGPU fills in the rest. See SPUD_STRUCT_BASE. */
	SPUD_STRUCT_BASE_OUT base;
	/**
	 * Bytes the resource covers from its offset. Never less than a buffer's
	 * size, and often more: on D3D12 every buffer covers a multiple of
	 * 64 KiB.
	 */
	uint64_t size;
	/** A power of two the offset is a multiple of. */
	uint64_t alignment;
} spudgpu_placement_requirements;

/**
 * @brief Reports what a buffer of `desc` needs to be placed in `heap`.
 *
 * The heap is asked, not the device, because the answer is its memory's. In
 * a heap of a mappable kind, the size and alignment are both multiples of
 * what the device flushes and invalidates in, so spudgpu_flush_buffer() and
 * spudgpu_invalidate_buffer() on one placed buffer never touch a byte of
 * another.
 *
 * The same `desc` and heap give the same answer for as long as the heap
 * exists. Nothing is allocated in the heap.
 *
 * @param[in]  heap             Heap the buffer would be placed in.
 * @param[in]  desc             The buffer, as spudgpu_create_placed_buffer()
 *                              would be given it.
 * @param[out] out_requirements Receives the size and alignment.
 *
 * @retval SPUD_SUCCESS `out_requirements` was filled in.
 * @retval SPUDRESULT_GPU_INVALID_HEAP `heap` is NULL.
 * @retval SPUDRESULT_NULL_DESC `desc` is NULL.
 * @retval SPUDRESULT_STRUCT_WRONG_TYPE `desc->base.type` is not
 *         SPUD_STRUCT_TYPE_GPU_BUFFER_DESC.
 * @retval SPUDRESULT_STRUCT_UNKNOWN_IN_CHAIN `desc->base.next` is not NULL.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_requirements` is NULL.
 * @retval SPUDRESULT_STRUCT_WRONG_TYPE `out_requirements->base.type` is not
 *         SPUD_STRUCT_TYPE_GPU_PLACEMENT_REQUIREMENTS.
 * @retval SPUDRESULT_STRUCT_UNKNOWN_IN_CHAIN `out_requirements->base.next` is not NULL.
 * @retval SPUDRESULT_GPU_ZERO_BUFFER_SIZE `desc->size` is 0.
 * @retval SPUDRESULT_GPU_INVALID_BUFFER_USAGE `desc->usage` is
 *         SPUDGPU_BUFFER_USAGE_NONE, or is one `desc->memory_kind` does not
 *         allow.
 * @retval SPUDRESULT_GPU_INVALID_MEMORY_KIND `desc->memory_kind` is
 *         SPUDGPU_MEMORY_KIND_NONE or not a kind.
 * @retval SPUDRESULT_GPU_MEMORY_KIND_NOT_SUPPORTED As for
 *         spudgpu_create_buffer().
 * @retval SPUDRESULT_GPU_HEAP_MISMATCH The heap is not a
 *         SPUDGPU_HEAP_RESOURCE_CLASS_BUFFERS heap, or `desc->memory_kind`
 *         is not the heap's.
 * @return SPUDRESULT_API_SPECIFIC_FAILURE if the graphics API's query fails.
 *
 * The desc checks are spudgpu_create_buffer()'s, in its order; the heap
 * checks come after them.
 */
SPUDRESULT spudgpu_get_buffer_placement_requirements(
    spudgpu_heap heap,
    const spudgpu_buffer_desc *desc,
    spudgpu_placement_requirements *out_requirements);

/**
 * @brief Reports what an image of `desc` needs to be placed in `heap`.
 *
 * The same `desc` and heap give the same answer for as long as the heap
 * exists. Nothing is allocated in the heap.
 *
 * @param[in]  heap             Heap the image would be placed in.
 * @param[in]  desc             The image, as spudgpu_create_placed_image()
 *                              would be given it.
 * @param[out] out_requirements Receives the size and alignment.
 *
 * @retval SPUD_SUCCESS `out_requirements` was filled in.
 * @retval SPUDRESULT_GPU_INVALID_HEAP `heap` is NULL.
 * @retval SPUDRESULT_NULL_DESC `desc` is NULL.
 * @retval SPUDRESULT_STRUCT_WRONG_TYPE `desc->base.type` is not
 *         SPUD_STRUCT_TYPE_GPU_IMAGE_DESC.
 * @retval SPUDRESULT_STRUCT_UNKNOWN_IN_CHAIN `desc->base.next` is not NULL.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_requirements` is NULL.
 * @retval SPUDRESULT_STRUCT_WRONG_TYPE `out_requirements->base.type` is not
 *         SPUD_STRUCT_TYPE_GPU_PLACEMENT_REQUIREMENTS.
 * @retval SPUDRESULT_STRUCT_UNKNOWN_IN_CHAIN `out_requirements->base.next` is not NULL.
 * @retval (each result of spudgpu_create_image()'s desc checks, in its
 *         order.)
 * @retval SPUDRESULT_GPU_INVALID_IMAGE_USAGE `desc->usage` has
 *         SPUDGPU_IMAGE_USAGE_TRANSIENT_ATTACHMENT or
 *         SPUDGPU_IMAGE_USAGE_PRESENTABLE. A transient attachment's memory
 *         is the tile's or lazily allocated and a presentable image's is the
 *         swap chain's, never a heap's.
 * @retval SPUDRESULT_GPU_HEAP_MISMATCH The heap's class is not the image's
 *         (see SPUDGPU_HEAP_RESOURCE_CLASS).
 * @return SPUDRESULT_API_SPECIFIC_FAILURE if the graphics API's query fails.
 */
SPUDRESULT spudgpu_get_image_placement_requirements(
    spudgpu_heap heap,
    const spudgpu_image_desc *desc,
    spudgpu_placement_requirements *out_requirements);

/**
 * @brief Creates a buffer in `heap`'s memory at `offset`.
 *
 * The result is a spudgpu_buffer like any other: its views, binding, copies,
 * barriers, spudgpu_get_buffer_desc() and spudgpu_destroy_buffer() are
 * unchanged, and spudgpu_get_buffer_gpu_address() gives its address. Destroying it frees
 * nothing to the device; the bytes stay the heap's, and once the buffer is
 * destroyed another resource may be placed over them. Its contents are
 * undefined until written, as any new buffer's are, whatever was in those
 * bytes before.
 *
 * Mapping keeps its contract: a placed buffer has one mapping at a time and
 * may stay mapped for as long as it exists. Several buffers of one heap may
 * be mapped at once.
 *
 * Nothing checks that the bytes are free. Two live resources placed over one
 * byte is undefined, on every backend.
 *
 * @param[in]  heap       Heap to place the buffer in.
 * @param[in]  offset     Byte offset in the heap: a multiple of the
 *                        alignment spudgpu_get_buffer_placement_requirements()
 *                        reports for `desc`.
 * @param[in]  desc       Buffer configuration. `memory_kind` is the heap's.
 * @param[out] out_buffer Receives the new buffer on success.
 *
 * @retval SPUD_SUCCESS The buffer was created.
 * @retval (each result of spudgpu_get_buffer_placement_requirements(), in
 *         its order, with `out_buffer` in place of `out_requirements`, and
 *         without the two about `out_requirements->base`.)
 * @retval SPUDRESULT_GPU_PLACEMENT_MISALIGNED `offset` is not a multiple of
 *         the reported alignment.
 * @retval SPUDRESULT_GPU_PLACEMENT_OUT_OF_RANGE `offset` plus the reported
 *         size runs past the end of the heap.
 * @return Another SPUDRESULT if the backend fails.
 *
 * @see spudgpu_destroy_buffer()
 */
SPUDRESULT spudgpu_create_placed_buffer(
    spudgpu_heap heap,
    uint64_t offset,
    const spudgpu_buffer_desc *desc,
    spudgpu_buffer *out_buffer);

/**
 * @brief Creates an image in `heap`'s memory at `offset`.
 *
 * The result is a spudgpu_image like any other, destroyed with
 * spudgpu_destroy_image(), with a placed buffer's rules for what destroying
 * it frees and what overlapping placements are.
 *
 * An image in a SPUDGPU_HEAP_RESOURCE_CLASS_ATTACHMENT_IMAGES heap has one
 * rule more, on every backend: the first thing done with it writes all of
 * every subresource, by a rendering pass that clears each attachment over
 * the whole image (depth and stencil both, for a format with the two) or by
 * copies into all of every subresource. What anything
 * else done first reads or leaves behind is undefined. (D3D12 requires a
 * placed render target or depth/stencil resource to be initialized by a
 * clear, a full copy or a discard before any other use.)
 *
 * @param[in]  heap      Heap to place the image in.
 * @param[in]  offset    Byte offset in the heap: a multiple of the alignment
 *                       spudgpu_get_image_placement_requirements() reports
 *                       for `desc`.
 * @param[in]  desc      Image configuration.
 * @param[out] out_image Receives the new image on success.
 *
 * @retval SPUD_SUCCESS The image was created.
 * @retval (each result of spudgpu_get_image_placement_requirements(), in
 *         its order, with `out_image` in place of `out_requirements`, and
 *         without the two about `out_requirements->base`.)
 * @retval SPUDRESULT_GPU_PLACEMENT_MISALIGNED `offset` is not a multiple of
 *         the reported alignment.
 * @retval SPUDRESULT_GPU_PLACEMENT_OUT_OF_RANGE `offset` plus the reported
 *         size runs past the end of the heap.
 * @return Another SPUDRESULT if the backend fails.
 *
 * @see spudgpu_destroy_image()
 */
SPUDRESULT spudgpu_create_placed_image(
    spudgpu_heap heap,
    uint64_t offset,
    const spudgpu_image_desc *desc,
    spudgpu_image *out_image);

// ============================================================================
//  Image Layout
//  Maps to: VkImageLayout (Vulkan)
// ============================================================================

/**
 * @brief The layout an image is in, which decides what it may be used for.
 *
 * Maps to VkImageLayout on Vulkan.
 */
typedef uint32_t SPUDGPU_IMAGE_LAYOUT;

enum {
	/**
	 * No layout: what a new image is in. As the old layout of a barrier, it
	 * says the image's contents need not be kept.
	 */
	SPUDGPU_IMAGE_LAYOUT_UNDEFINED                        = 0,
	/**
	 * What a storage image is read and written in through a storage slot.
	 * That is all it promises: one graphics API lets an image in this layout
	 * be used for anything, and another has no such state, so a caller that
	 * samples it, renders to it or copies it moves it to the layout for
	 * that first. For an image created without SPUDGPU_IMAGE_USAGE_STORAGE
	 * it is only a layout with nothing in particular allowed.
	 *
	 * A barrier whose old and new layouts are both this is not a no-op: see
	 * spudgpu_cmd_image_barrier().
	 */
	SPUDGPU_IMAGE_LAYOUT_GENERAL                          = 1,
	/** Rendered into as a color attachment. */
	SPUDGPU_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL         = 2,
	/** Rendered into as the depth/stencil attachment. */
	SPUDGPU_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL = 3,
	/** Sampled by a shader. */
	SPUDGPU_IMAGE_LAYOUT_SHADER_READ_ONLY                 = 4,
	/** The source of a copy or blit. */
	SPUDGPU_IMAGE_LAYOUT_TRANSFER_SRC                     = 5,
	/** The destination of a copy or blit. */
	SPUDGPU_IMAGE_LAYOUT_TRANSFER_DST                     = 6,
	/** Ready to present: what a swap chain image is handed back in. */
	SPUDGPU_IMAGE_LAYOUT_PRESENT_SRC                      = 7,
};

// ============================================================================
//  Image Barrier
//  Maps to: vkCmdPipelineBarrier (Vulkan)
// ============================================================================

/**
 * @brief Transitions an image from one layout to another, inserting the
 * pipeline barrier dependent stages need to wait correctly.
 *
 * Call before spudgpu_cmd_begin_rendering() to move a swap chain image from
 * UNDEFINED/PRESENT_SRC to COLOR_ATTACHMENT_OPTIMAL, and after
 * spudgpu_cmd_end_rendering() to move it back to PRESENT_SRC.
 *
 * Three rules hold on every backend, for this call and its two variants:
 *
 * - It moves every mip level and array layer of the image. To move some of
 *   them, use spudgpu_cmd_image_barrier_subresource(); once that has left
 *   them in different layouts, this call's `old_layout` can no longer be
 *   true of all of them, and the caller moves each range from its own.
 * - `old_layout` is the layout the image is in, or
 *   SPUDGPU_IMAGE_LAYOUT_UNDEFINED to say its contents need not be kept.
 * - A barrier from SPUDGPU_IMAGE_LAYOUT_GENERAL to
 *   SPUDGPU_IMAGE_LAYOUT_GENERAL is not a no-op. It moves the image
 *   nowhere and orders its accesses: every write a shader made to it
 *   through a storage slot before the barrier is complete before any read
 *   or write after it. It is the caller's to record between two draws or
 *   dispatches when the second reads or overwrites what the first wrote.
 *   A barrier between any other two equal layouts does nothing.
 *
 * @param[in] cmd        Command list being recorded.
 * @param[in] image      Image whose layout is changed.
 * @param[in] old_layout Layout the image is in now.
 * @param[in] new_layout Layout to transition into.
 */
void spudgpu_cmd_image_barrier(
    spudgpu_command_list cmd,
    spudgpu_image image,
    SPUDGPU_IMAGE_LAYOUT old_layout,
    SPUDGPU_IMAGE_LAYOUT new_layout);

/**
 * @brief Variant of spudgpu_cmd_image_barrier() that takes an image view.
 *
 * For swap chain images, which are not created through spudgpu_create_image()
 * and so have no spudgpu_image handle.
 *
 * @param[in] cmd        Command list being recorded.
 * @param[in] image_view View whose parent image is transitioned.
 * @param[in] old_layout Layout the image is in now.
 * @param[in] new_layout Layout to transition into.
 */
void spudgpu_cmd_image_barrier_view(
    spudgpu_command_list cmd,
    spudgpu_image_view image_view,
    SPUDGPU_IMAGE_LAYOUT old_layout,
    SPUDGPU_IMAGE_LAYOUT new_layout);

/**
 * @brief Transitions one mip/layer range of an image, leaving its other
 * subresources in the layout they were already in.
 *
 * Needed whenever subresources of one image must be in different layouts at
 * once, most notably mip chain generation with spudgpu_cmd_blit_image(), where
 * mip level N is SPUDGPU_IMAGE_LAYOUT_TRANSFER_SRC while level N+1 is
 * SPUDGPU_IMAGE_LAYOUT_TRANSFER_DST. spudgpu_cmd_image_barrier() transitions
 * the whole image and cannot express this.
 *
 * @param[in] cmd        Command list being recorded.
 * @param[in] image      Image whose subresource range is changed.
 * @param[in] range      Mip levels and array layers to transition.
 * @param[in] old_layout Layout the range is in now.
 * @param[in] new_layout Layout to transition into.
 */
void spudgpu_cmd_image_barrier_subresource(
    spudgpu_command_list cmd,
    spudgpu_image image,
    const spudgpu_image_view_desc_subresource_range *range,
    SPUDGPU_IMAGE_LAYOUT old_layout,
    SPUDGPU_IMAGE_LAYOUT new_layout);
/**
 * @brief What a buffer or image is being used as, for
 * spudgpu_cmd_pipeline_barrier().
 */
typedef uint32_t SPUDGPU_RESOURCE_STATE;
enum {
	SPUDGPU_RESOURCE_STATE_COMMON = 0,       /**< No particular use; what a new SPUDGPU_MEMORY_KIND_DEVICE buffer is in */
	SPUDGPU_RESOURCE_STATE_VERTEX_BUFFER,    /**< Bound as a vertex or uniform buffer */
	SPUDGPU_RESOURCE_STATE_INDEX_BUFFER,     /**< Bound as an index buffer */
	SPUDGPU_RESOURCE_STATE_RENDER_TARGET,    /**< Rendered into as a color attachment */
	SPUDGPU_RESOURCE_STATE_DEPTH_WRITE,      /**< Rendered into as the depth/stencil attachment */
	SPUDGPU_RESOURCE_STATE_SHADER_RESOURCE,  /**< Read-only in shader */
	SPUDGPU_RESOURCE_STATE_UNORDERED_ACCESS, /**< Read/Write (SSBO/UAV) */
	SPUDGPU_RESOURCE_STATE_PRESENT,          /**< Ready to present */
	SPUDGPU_RESOURCE_STATE_INDIRECT_ARGUMENT, /**< Read by spudgpu_cmd_draw_indirect / _indexed_indirect */
	SPUDGPU_RESOURCE_STATE_COPY_SOURCE,       /**< Read by a copy command */
	SPUDGPU_RESOURCE_STATE_COPY_DEST          /**< Written by a copy command */
};

/**
 * @brief Pipeline stage flags used in semaphore wait masks on queue submission.
 *
 * Values are bit-compatible with VkPipelineStageFlagBits and may be OR'd
 * together. One mask must be provided per wait semaphore in
 * spudgpu_submit_desc.
 */
typedef uint32_t SPUDGPU_PIPELINE_STAGE;
enum {
	SPUDGPU_PIPELINE_STAGE_TOP_OF_PIPE             = 0x00000001, /**< Before any stage: wait for nothing */
	SPUDGPU_PIPELINE_STAGE_DRAW_INDIRECT           = 0x00000002, /**< Where indirect draw arguments are read */
	SPUDGPU_PIPELINE_STAGE_VERTEX_INPUT            = 0x00000004, /**< Where vertex and index buffers are read */
	SPUDGPU_PIPELINE_STAGE_VERTEX_SHADER           = 0x00000008, /**< The vertex shader */
	SPUDGPU_PIPELINE_STAGE_TESSELLATION_CONTROL    = 0x00000010, /**< The tessellation control shader */
	SPUDGPU_PIPELINE_STAGE_TESSELLATION_EVALUATION = 0x00000020, /**< The tessellation evaluation shader */
	SPUDGPU_PIPELINE_STAGE_GEOMETRY_SHADER         = 0x00000040, /**< The geometry shader */
	SPUDGPU_PIPELINE_STAGE_FRAGMENT_SHADER         = 0x00000080, /**< The fragment shader */
	SPUDGPU_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS    = 0x00000100, /**< Depth and stencil tests before the fragment shader */
	SPUDGPU_PIPELINE_STAGE_LATE_FRAGMENT_TESTS     = 0x00000200, /**< Depth and stencil tests after the fragment shader */
	SPUDGPU_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT = 0x00000400, /**< Where color attachments are written; the stage to wait at for a swap chain image */
	SPUDGPU_PIPELINE_STAGE_COMPUTE_SHADER          = 0x00000800, /**< The compute shader */
	SPUDGPU_PIPELINE_STAGE_TRANSFER                = 0x00001000, /**< Copies and blits */
	SPUDGPU_PIPELINE_STAGE_BOTTOM_OF_PIPE          = 0x00002000, /**< After every stage */
	SPUDGPU_PIPELINE_STAGE_ALL_GRAPHICS            = 0x00008000, /**< Every graphics stage */
	SPUDGPU_PIPELINE_STAGE_ALL_COMMANDS            = 0x00010000, /**< Every stage of every command */
};

/**
 * @brief One buffer state transition for spudgpu_cmd_pipeline_barrier().
 */
typedef struct spudgpu_buffer_barrier {
	/** `type` is SPUD_STRUCT_TYPE_GPU_BUFFER_BARRIER; `next` is NULL (nothing is chained onto
	 * this struct yet). Set by the caller. See SPUD_STRUCT_BASE. */
	SPUD_STRUCT_BASE base;
	/**
	 * The buffer to move. Never one of SPUDGPU_MEMORY_KIND_UPLOAD or
	 * SPUDGPU_MEMORY_KIND_READBACK, which stay in the state they are
	 * created in.
	 */
	spudgpu_buffer buffer;
	/** The state the buffer is in when the barrier executes. */
	SPUDGPU_RESOURCE_STATE state_before;
	/** The state the buffer is in after it. */
	SPUDGPU_RESOURCE_STATE state_after;
} spudgpu_buffer_barrier;

/**
 * @brief One image state transition for spudgpu_cmd_pipeline_barrier().
 *
 * The whole image is moved: there is no range of mip levels or array layers
 * here. spudgpu_cmd_image_barrier_subresource() moves a range between
 * layouts.
 */
typedef struct spudgpu_image_barrier {
	/** `type` is SPUD_STRUCT_TYPE_GPU_IMAGE_BARRIER; `next` is NULL (nothing is chained onto
	 * this struct yet). Set by the caller. See SPUD_STRUCT_BASE. */
	SPUD_STRUCT_BASE base;
	/** The image to move. */
	spudgpu_image image;
	/** The state the image is in when the barrier executes. */
	SPUDGPU_RESOURCE_STATE state_before;
	/** The state the image is in after it. */
	SPUDGPU_RESOURCE_STATE state_after;
} spudgpu_image_barrier;

/**
 * @brief Records an execution and memory barrier moving buffers and images
 * between resource states.
 *
 * Three rules hold on every backend, whatever the graphics API underneath
 * does with a given pair of states:
 *
 * - An entry whose `state_before` and `state_after` are both
 *   SPUDGPU_RESOURCE_STATE_UNORDERED_ACCESS is not a no-op. It moves the
 *   resource nowhere, and orders its accesses: every write made through a
 *   storage slot before the barrier is complete before any read or write
 *   after it.
 * - That barrier is the caller's to record between two draws or dispatches
 *   when the second reads or overwrites what the first wrote to the resource
 *   through a storage slot. Without it, what the second sees is undefined.
 *   It is never recorded on the caller's behalf: two writes that don't depend
 *   on each other need none, and only the caller knows which they are.
 * - An entry whose two states are equal and are anything else does nothing.
 *
 * Maps to: vkCmdPipelineBarrier (Vulkan); ID3D12GraphicsCommandList::
 * ResourceBarrier, with a transition barrier between two states and a UAV
 * barrier for the UNORDERED_ACCESS pair (D3D12). Records nothing on Metal,
 * which orders access to a resource itself.
 *
 * Nothing is recorded if an element of `buffer_barriers` has a `base` other than {SPUD_STRUCT_TYPE_GPU_BUFFER_BARRIER, NULL} or an element of `image_barriers` has a `base` other than {SPUD_STRUCT_TYPE_GPU_IMAGE_BARRIER, NULL}
 * (see SPUD_STRUCT_BASE).
 *
 * @param[in] cmd                  Command list being recorded.
 * @param[in] buffer_barriers      Array of buffer state transitions. May be
 *                                 NULL when `buffer_barrier_count` is 0.
 * @param[in] buffer_barrier_count Number of elements in `buffer_barriers`.
 * @param[in] image_barriers       Array of image state transitions. May be
 *                                 NULL when `image_barrier_count` is 0.
 * @param[in] image_barrier_count  Number of elements in `image_barriers`.
 */
void spudgpu_cmd_pipeline_barrier(
    spudgpu_command_list cmd,
    const spudgpu_buffer_barrier *buffer_barriers,
    uint32_t buffer_barrier_count,
    const spudgpu_image_barrier *image_barriers,
    uint32_t image_barrier_count);

/**
 * @brief Creates a surface for a native window, for a swap chain to present
 * to.
 *
 * @param[in]  instance       Instance to create the surface on.
 * @param[in]  window_handle  The platform's native window object:
 *                            - Windows (Win32): `HWND`.
 *                            - macOS (AppKit): `NSWindow*` or `CAMetalLayer*`.
 *                            - Linux (X11 / Wayland): `Window` or
 *                              `wl_surface*`.
 * @param[in]  display_handle The platform's display connection:
 *                            - Linux Wayland: `wl_display*`.
 *                            - Linux X11: `Display*`.
 *                            - Windows: unused, pass NULL.
 * @param[out] out_surface    Receives the new surface on success.
 *
 * @retval SPUD_SUCCESS The surface was created.
 * @retval SPUDRESULT_GPU_INVALID_INSTANCE `instance` is NULL.
 * @retval SPUDRESULT_GPU_INVALID_WINDOW_HANDLE `window_handle` is NULL.
 * @retval SPUDRESULT_GPU_INVALID_DISPLAY_HANDLE `display_handle` is NULL where
 *         the platform needs one (Vulkan, other than on Windows).
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_surface` is NULL.
 * @return Another SPUDRESULT if the backend fails, such as
 *         SPUDRESULT_OUT_OF_MEMORY or SPUDRESULT_API_SPECIFIC_FAILURE.
 *
 * @see spudgpu_destroy_surface()
 */
SPUDRESULT spudgpu_create_surface(
    spudgpu_instance instance,
    void *window_handle,
    void *display_handle,
    spudgpu_surface *out_surface);

/**
 * @brief Destroys a surface.
 *
 * @warning Destroy any swap chain created on the surface first.
 *
 * @param[in] surface Surface to destroy.
 */
void spudgpu_destroy_surface(spudgpu_surface surface);

/**
 * @brief Callback that creates the native surface for
 * spudgpu_create_surface_from_callback().
 *
 * The native types are passed as `void *` to keep this header free of Vulkan
 * types.
 *
 * @param[in]  vk_instance The instance's `VkInstance`.
 * @param[in]  user_data   The `user_data` given to
 *                         spudgpu_create_surface_from_callback().
 * @param[out] out_surface A `VkSurfaceKHR *` that receives the created
 *                         surface.
 *
 * @return true if the surface was created and written to `out_surface`,
 *         false otherwise.
 */
typedef bool (*spudgpu_surface_create_fn)(
    void *vk_instance,
    void *user_data,
    void *out_surface);

/**
 * @brief Creates a surface by having the caller make the native surface
 * object.
 *
 * For windowing libraries that create the native surface themselves: `create_fn`
 * is called once, during this call, with the instance's native handle.
 *
 * @note Not implemented by the D3D12 backend.
 *
 * @param[in] instance  Instance to create the surface on.
 * @param[in] user_data Passed through to `create_fn` unchanged.
 * @param[in] create_fn Callback that creates the native surface.
 *
 * @return The new surface, or NULL if `instance` or `create_fn` is NULL, the
 *         surface could not be allocated, or `create_fn` returned false.
 */
spudgpu_surface spudgpu_create_surface_from_callback(
    spudgpu_instance instance,
    void *user_data,
    spudgpu_surface_create_fn create_fn);

/**
 * @brief Dictates the sync relationship between the GPU frame completion and
 * the monitor's refresh cycle.
 *
 * Governs whether presentation waits for the display to prevent tearing or
 * uses extra back buffers to reduce input latency.
 *
 * @see SPUDGPU_PRESENT_MODE_Constants
 */
typedef uint32_t SPUDGPU_PRESENT_MODE;

/**
 * @name Presentation Sync Modes
 * @anchor SPUDGPU_PRESENT_MODE_Constants
 *
 * Configurations mapped to underlying driver swap rules (e.g.,
 * VkPresentModeKHR or NSOpenGLContext).
 */
/** @{ */
enum {
	/**
	 * @brief Immediate Mode (V-Sync Disabled).
	 *
	 * The GPU transfers completed frames to the screen instantly. Offers the
	 * lowest possible input latency but results in visible screen tearing as
	 * the monitor splits images mid-refresh.
	 */
	SPUDGPU_PRESENT_MODE_IMMEDIATE = 0,

	/**
	 * @brief FIFO Mode (V-Sync Enabled - First In, First Out).
	 *
	 * Frames are queued up and synchronized strictly with the display's
	 * vertical refresh rate (e.g., 60Hz/144Hz). Completely eliminates screen
	 * tearing, but will throttle the CPU/GPU thread if the queue fills up.
	 */
	SPUDGPU_PRESENT_MODE_FIFO = 1,

	/**
	 * @brief Mailbox Mode (Triple Buffering / Ultra-Low Latency V-Sync).
	 *
	 * Synchronizes with the vertical refresh rate to eliminate tearing, but
	 * does not block the application when the queue is full. Instead, the
	 * newest completed frame continuously replaces unrendered frames in the
	 * queue, ensuring the monitor always pulls the absolute freshest data.
	 */
	SPUDGPU_PRESENT_MODE_MAILBOX = 2
};

/**
 * @brief How a swap chain's window occupies the display.
 */
typedef uint8_t SPUDGPU_FULLSCREEN_MODE;
enum {
	/** An ordinary window. */
	SPUDGPU_FULLSCREEN_MODE_WINDOWED   = 0,
	/** Exclusive fullscreen: the swap chain owns the display. */
	SPUDGPU_FULLSCREEN_MODE_FULLSCREEN = 1,
	/** A borderless window covering the display. */
	SPUDGPU_FULLSCREEN_MODE_BORDERLESS = 2,
};

/** @} */

/**
 * @brief Configuration descriptor used to initialize a rendering surface target
 * linked to the OS windowing system.
 *
 * Handled by the driver to allocate a ring buffer of textures (back buffers)
 * that flip onto the screen.
 */
typedef struct spudgpu_swap_chain_desc {
	/** `type` is SPUD_STRUCT_TYPE_GPU_SWAP_CHAIN_DESC; `next` is NULL (nothing is chained onto
	 * this struct yet). Set by the caller. See SPUD_STRUCT_BASE. */
	SPUD_STRUCT_BASE base;
	/** The window surface the swap chain presents to. */
	spudgpu_surface surface;

	/**
	 * @brief The queue this swap chain's presentation is ordered against.
	 *
	 * Required on every backend, not just a hint: D3D12 must bind the
	 * DXGI swap chain to a specific ID3D12CommandQueue at creation time and
	 * cannot rebind it later, so this is where that queue comes from instead
	 * of a backend silently assuming "the graphics queue." Render all work
	 * destined for this swap chain's images on this same queue before
	 * calling spudgpu_swap_chain_present - presentation is ordered relative
	 * to it, not to whichever queue you happen to submit on.
	 */
	spudgpu_command_queue queue;

	/**
	 * Requested back-buffer width in pixels. Usually matches the window client
	 * area width.
	 */
	uint32_t width;

	/**
	 * Requested back-buffer height in pixels. Usually matches the window
	 * client area height.
	 */
	uint32_t height;

	/**
	 * @brief Total number of images in the swap chain ring buffer.
	 *
	 * Typically configured as `2` for simple Double Buffering or `3` for
	 * Mailbox Triple Buffering layouts.
	 */
	uint32_t buffer_count;

	/**
	 * The color and pixel format schema requested for the surface (e.g.,
	 * BGRA8_UNORM or RGBA16_SFLOAT).
	 */
	SPUDGPU_FORMAT format;

	/**
	 * The sync and presentation timing rule to apply when presenting completed
	 * frames.
	 * @see SPUDGPU_PRESENT_MODE
	 */
	SPUDGPU_PRESENT_MODE present_mode;

	/**
	 * @brief Toggles whether the graphics device initializes in exclusive
	 * fullscreen monitor mode or stays bounded inside a standard desktop window
	 * framework.
	 */
	SPUDGPU_FULLSCREEN_MODE fullscreen_mode;

	/**
	 * @brief Usages the back buffers need beyond being rendered into.
	 *
	 * Back buffers are always usable as a color attachment; this adds to
	 * that, e.g. SPUDGPU_IMAGE_USAGE_TRANSFER_DST to copy/blit a finished
	 * image into them with spudgpu_cmd_blit_image. Leave 0 for none.
	 * spudgpu_create_swap_chain fails if the surface can't support a
	 * requested usage.
	 * @see SPUDGPU_IMAGE_USAGE
	 */
	SPUDGPU_IMAGE_USAGE usage;
} spudgpu_swap_chain_desc;

/**
 * @brief Creates a swap chain for rendering to a surface.
 *
 * @param[in]  device         Device to create the swap chain on.
 * @param[in]  desc           Swap chain configuration.
 * @param[out] out_swap_chain Receives the new swap chain on success.
 *
 * @retval SPUD_SUCCESS The swap chain was created.
 * @retval SPUDRESULT_GPU_INVALID_DEVICE `device` is NULL.
 * @retval SPUDRESULT_NULL_DESC `desc` is NULL.
 * @retval SPUDRESULT_STRUCT_WRONG_TYPE `desc->base.type` is not
 *         SPUD_STRUCT_TYPE_GPU_SWAP_CHAIN_DESC.
 * @retval SPUDRESULT_STRUCT_UNKNOWN_IN_CHAIN `desc->base.next` is not NULL.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_swap_chain` is NULL.
 * @retval SPUDRESULT_GPU_INVALID_SURFACE `desc->surface` is NULL.
 * @retval SPUDRESULT_GPU_INVALID_COMMAND_QUEUE `desc->queue` is NULL.
 * @retval SPUDRESULT_DESC_INVALID_PARAMETERS The backend cannot create a swap
 *         chain with this configuration (Metal: `desc->buffer_count` other than
 *         1, or SPUDGPU_PRESENT_MODE_MAILBOX).
 * @return Another SPUDRESULT if the backend fails, such as
 *         SPUDRESULT_OUT_OF_MEMORY or SPUDRESULT_API_SPECIFIC_FAILURE.
 *
 * @see spudgpu_destroy_swap_chain()
 */
SPUDRESULT spudgpu_create_swap_chain(
    spudgpu_device device,
    const spudgpu_swap_chain_desc *desc,
    spudgpu_swap_chain *out_swap_chain);

/**
 * @brief Destroys a swap chain with its back buffer images, views,
 * semaphores and fences.
 *
 * @param[in] swap_chain Swap chain to destroy.
 */
void spudgpu_destroy_swap_chain(spudgpu_swap_chain swap_chain);

/**
 * @brief Reads a swap chain's configuration as actually created.
 *
 * This can differ from what was requested: `width`/`height` are the real
 * back-buffer size (on Vulkan the surface can dictate it, e.g. a window still
 * 1x1 before layout), and `buffer_count` the real image count where a backend
 * reports it.
 *
 * @param[in]  swap_chain Swap chain to query.
 * @param[out] out_desc   Receives the swap chain's descriptor.
 *
 * @retval SPUD_SUCCESS `out_desc` was filled in.
 * @retval SPUDRESULT_GPU_INVALID_SWAP_CHAIN `swap_chain` is NULL.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_desc` is NULL.
 */
SPUDRESULT spudgpu_get_swap_chain_desc(
    spudgpu_swap_chain swap_chain,
    spudgpu_swap_chain_desc *out_desc);

/**
 * @brief Acquires the next back buffer image to render into.
 *
 * Blocks while the GPU is still working on the frame slot being reused.
 *
 * @param[in] swap_chain Swap chain to acquire from.
 *
 * @return The index of the acquired back buffer, for
 *         spudgpu_get_swap_chain_image_view(), or SPUD_UINT32_MAX if
 *         `swap_chain` is NULL or no image could be acquired.
 */
uint32_t spudgpu_swap_chain_acquire_next_image(spudgpu_swap_chain swap_chain);

/**
 * @brief Presents the current back buffer to the screen.
 *
 * Call after recording and submitting the frame's commands to the queue given
 * as spudgpu_swap_chain_desc::queue. Presentation is ordered against that
 * queue's work, not against whichever queue was submitted on.
 *
 * @param[in] swap_chain Swap chain holding the image to display.
 */
void spudgpu_swap_chain_present(spudgpu_swap_chain swap_chain);

/**
 * @brief Returns the image view of one of a swap chain's back buffers.
 *
 * The swap chain owns the view; do not destroy it.
 *
 * @param[in] swap_chain  Swap chain to get the view from.
 * @param[in] image_index Back buffer index, as returned by
 *                        spudgpu_swap_chain_acquire_next_image().
 *
 * @return The image view, or NULL if `swap_chain` is NULL or `image_index` is
 *         not one of its back buffers.
 */
spudgpu_image_view spudgpu_get_swap_chain_image_view(
    spudgpu_swap_chain swap_chain,
    uint32_t image_index);

/**
 * @brief Returns the image-available semaphore of the swap chain's current
 * frame slot.
 *
 * It is signaled when the acquired image is ready to render into. Pass it as a
 * wait semaphore in spudgpu_submit_desc. The swap chain owns it; do not destroy
 * it.
 *
 * @note Not implemented by the D3D12 backend.
 *
 * @param[in] swap_chain Swap chain to get the semaphore from.
 *
 * @return The semaphore, or NULL if `swap_chain` is NULL.
 */
spudgpu_semaphore spudgpu_swap_chain_get_image_available_semaphore(spudgpu_swap_chain swap_chain);

/**
 * @brief Returns the render-finished semaphore of the swap chain's current
 * frame slot.
 *
 * Pass it as a signal semaphore in spudgpu_submit_desc so presentation waits
 * for rendering. The swap chain owns it; do not destroy it.
 *
 * @note Not implemented by the D3D12 backend.
 *
 * @param[in] swap_chain Swap chain to get the semaphore from.
 *
 * @return The semaphore, or NULL if `swap_chain` is NULL.
 */
spudgpu_semaphore spudgpu_swap_chain_get_render_finished_semaphore(spudgpu_swap_chain swap_chain);

// ============================================================================
//  Descriptor Set Layout
//  Maps to: VkDescriptorSetLayout (Vulkan) / MTLArgumentEncoder schema (Metal)
// ============================================================================


/**
 * @brief Enumerates the kinds of resources a shader binding slot can hold.
 *
 * Maps directly to VkDescriptorType on Vulkan and the corresponding
 * MTLArgumentEncoder argument kinds on Metal.
 */
typedef uint32_t SPUDGPU_DESCRIPTOR_TYPE;

enum {
	/** Read-only structured constant block (UBO / constant buffer). */
	SPUDGPU_DESCRIPTOR_TYPE_UNIFORM_BUFFER = 0,

	/** Read/write large data array (SSBO / structured buffer). */
	SPUDGPU_DESCRIPTOR_TYPE_STORAGE_BUFFER = 1,

	/** Combined image + sampler in a single binding (texture2D + sampler). */
	SPUDGPU_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER = 2,

	/** Sampled image without a sampler (use alongside SAMPLER binding). */
	SPUDGPU_DESCRIPTOR_TYPE_SAMPLED_IMAGE = 3,

	/** Standalone sampler object. */
	SPUDGPU_DESCRIPTOR_TYPE_SAMPLER = 4,

	/** Image the shader can both read and write (compute UAV / storage image). */
	SPUDGPU_DESCRIPTOR_TYPE_STORAGE_IMAGE = 5,
};

/**
 * @brief Bitmask of shader stages.
 */
typedef uint32_t SPUDGPU_SHADER_STAGE;

enum {
	/** No stage. */
	SPUDGPU_SHADER_STAGE_NONE                    = 0,
	/** Vertex shader. */
	SPUDGPU_SHADER_STAGE_VERTEX                  = 1 << 0,
	/** Fragment (pixel) shader. */
	SPUDGPU_SHADER_STAGE_FRAGMENT                = 1 << 1,
	/** Compute shader. */
	SPUDGPU_SHADER_STAGE_COMPUTE                 = 1 << 2,
	/** Geometry shader. */
	SPUDGPU_SHADER_STAGE_GEOMETRY                = 1 << 3,
	/** Tessellation control (hull) shader. */
	SPUDGPU_SHADER_STAGE_TESSELLATION_CONTROL    = 1 << 4,
	/** Tessellation evaluation (domain) shader. */
	SPUDGPU_SHADER_STAGE_TESSELLATION_EVALUATION = 1 << 5,

	/** Mesh shader. @see SPUDGPU_EXT_MESH_SHADING */
	SPUDGPU_SHADER_STAGE_MESH                    = 1 << 6,
	/**
	 * Amplification/task shader, upstream of a mesh shader. Not used by any
	 * sample yet — declared alongside MESH for completeness.
	 * @see SPUDGPU_EXT_MESH_SHADING
	 */
	SPUDGPU_SHADER_STAGE_TASK                    = 1 << 7
};

/**
 * @brief Describes a single binding slot within a descriptor set layout.
 *
 * Each entry corresponds to one `layout(set=N, binding=M)` declaration in GLSL.
 */
/* Frozen: an array element, held by value in spudgpu_descriptor_set_layout_desc. It never changes; a new need gets a new type. */
typedef struct spudgpu_descriptor_binding_desc {
	/**
	 * Slot index matching `layout(binding = N)` in GLSL. Must be unique within
	 * a layout.
	 */
	uint32_t binding;

	/**
	 * Resource type expected at this binding slot.
	 * @see SPUDGPU_DESCRIPTOR_TYPE
	 */
	SPUDGPU_DESCRIPTOR_TYPE descriptor_type;

	/**
	 * Number of resources in this binding. Use 1 for a single resource,
	 * or N for a fixed-size array (e.g. `uniform sampler2D textures[8]`).
	 */
	uint32_t count;

	/**
	 * Bitmask of shader stages that can access this binding.
	 * @see SPUDGPU_SHADER_STAGE
	 */
	SPUDGPU_SHADER_STAGE stage_flags;
} spudgpu_descriptor_binding_desc;

/**
 * @brief Capacity of spudgpu_descriptor_set_layout_desc::bindings.
 */
#define SPUDGPU_MAX_DESCRIPTOR_BINDINGS_PER_SET 16

/**
 * @brief Configuration descriptor for a descriptor set layout.
 *
 * Describes the binding slots that make up one set. Pass this to
 * spudgpu_create_descriptor_set_layout(), then hand the resulting
 * handle into spudgpu_shader_pipeline_desc::descriptor_set_layouts[].
 */
typedef struct spudgpu_descriptor_set_layout_desc {
	/** `type` is SPUD_STRUCT_TYPE_GPU_DESCRIPTOR_SET_LAYOUT_DESC; `next` is NULL (nothing is chained onto
	 * this struct yet). Set by the caller. See SPUD_STRUCT_BASE. */
	SPUD_STRUCT_BASE base;
	/** @brief A name for diagnostics, or NULL. Copied, in every build; see
	 * spud_debug_name_set(). */
	const char *debug_name;

	/** The set's binding slots. Only the first #binding_count are read. */
	spudgpu_descriptor_binding_desc bindings[SPUDGPU_MAX_DESCRIPTOR_BINDINGS_PER_SET];
	/** Number of valid entries in #bindings. */
	uint32_t binding_count;
} spudgpu_descriptor_set_layout_desc;

/**
 * @brief Creates a descriptor set layout: the fixed list of binding slots a
 * descriptor set exposes to shaders.
 *
 * The layout cannot be changed after creation. Pass it to a pipeline desc to
 * declare the binding shape the pipeline expects. The caller owns the returned
 * layout and releases it with spudgpu_destroy_descriptor_set_layout().
 *
 * Backend mapping:
 * - Vulkan: creates a VkDescriptorSetLayout.
 * - Metal: creates an MTLArgumentEncoder describing the bindings.
 * - D3D12: creates no native object; records each binding's offset into the
 *   set's CBV/SRV/UAV and sampler heap ranges.
 *
 * @note Immutable samplers are not supported. On Metal every texture binding
 * is declared as a 2D texture.
 *
 * @param[in]  device     Device the layout is created on.
 * @param[in]  desc       Binding slots to declare. The struct and its
 *                        `debug_name` string are copied; neither needs to
 *                        outlive the call.
 * @param[out] out_layout Receives the new layout on success. Not written on
 *                        failure.
 *
 * @retval SPUD_SUCCESS The layout was created.
 * @retval SPUDRESULT_GPU_INVALID_DEVICE `device` is NULL.
 * @retval SPUDRESULT_NULL_DESC `desc` is NULL.
 * @retval SPUDRESULT_STRUCT_WRONG_TYPE `desc->base.type` is not
 *         SPUD_STRUCT_TYPE_GPU_DESCRIPTOR_SET_LAYOUT_DESC.
 * @retval SPUDRESULT_STRUCT_UNKNOWN_IN_CHAIN `desc->base.next` is not NULL.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_layout` is NULL.
 * @retval SPUDRESULT_GPU_TOO_MANY_DESCRIPTOR_BINDINGS `desc->binding_count`
 *         exceeds SPUDGPU_MAX_DESCRIPTOR_BINDINGS_PER_SET.
 * @retval SPUDRESULT_GPU_CANNOT_RESOLVE_API_SPECIFIC_DESCRIPTOR_TYPE A
 *         binding's `descriptor_type` has no equivalent on the backend
 *         (Vulkan and Metal only; D3D12 does not check).
 * @retval SPUDRESULT_API_SPECIFIC_FAILURE The native API failed to create the
 *         object (Vulkan and Metal only).
 *
 * @see spudgpu_destroy_descriptor_set_layout()
 */
SPUDRESULT spudgpu_create_descriptor_set_layout(
    spudgpu_device device,
    const spudgpu_descriptor_set_layout_desc *desc,
    spudgpu_descriptor_set_layout *out_layout);

/**
 * @brief Destroys a descriptor set layout.
 *
 * @warning All descriptor sets allocated from this layout, and all pipelines
 * referencing it, must be destroyed before calling this.
 *
 * @param[in] layout Layout to destroy.
 */
void spudgpu_destroy_descriptor_set_layout(spudgpu_descriptor_set_layout layout);
// ============================================================================
//  Descriptor Pool
//  Maps to: VkDescriptorPool (Vulkan) / heap of argument buffers (Metal)
// ============================================================================

/**
 * @brief Declares how many descriptors of each type a pool should pre-allocate.
 *
 * Vulkan requires knowing the total capacity up front. Size your pool to cover
 * the worst-case count across all frames-in-flight.
 */
/* Frozen: an array element, held by value in spudgpu_descriptor_pool_desc. It never changes; a new need gets a new type. */
typedef struct spudgpu_descriptor_pool_size {
	/** The type of descriptor counted. */
	SPUDGPU_DESCRIPTOR_TYPE descriptor_type;
	/** How many descriptors of that type the pool holds, across all its sets. */
	uint32_t count;
} spudgpu_descriptor_pool_size;

/**
 * @brief Capacity of spudgpu_descriptor_pool_desc::pool_sizes.
 */
#define SPUDGPU_MAX_DESCRIPTOR_POOL_SIZES 8

/**
 * @brief Configuration descriptor for a descriptor pool.
 */
typedef struct spudgpu_descriptor_pool_desc {
	/** `type` is SPUD_STRUCT_TYPE_GPU_DESCRIPTOR_POOL_DESC; `next` is NULL (nothing is chained onto
	 * this struct yet). Set by the caller. See SPUD_STRUCT_BASE. */
	SPUD_STRUCT_BASE base;
	/** @brief A name for diagnostics, or NULL. Copied, in every build; see
	 * spud_debug_name_set(). */
	const char *debug_name;

	/** Maximum number of descriptor sets that can be allocated from this pool. */
	uint32_t max_sets;

	/** Capacity per descriptor type. Only the first #pool_size_count are read. */
	spudgpu_descriptor_pool_size pool_sizes[SPUDGPU_MAX_DESCRIPTOR_POOL_SIZES];
	/** Number of valid entries in #pool_sizes. */
	uint32_t pool_size_count;
} spudgpu_descriptor_pool_desc;

/**
 * @brief Creates a descriptor pool, the backing memory for descriptor sets.
 *
 * Create one pool per frame-in-flight (or one large shared pool) and reset it
 * each frame rather than allocating and freeing individual sets every frame.
 *
 * @param[in]  device   Device to create the pool on.
 * @param[in]  desc     Pool capacity.
 * @param[out] out_pool Receives the new pool on success.
 *
 * @retval SPUD_SUCCESS The pool was created.
 * @retval SPUDRESULT_GPU_INVALID_DEVICE `device` is NULL.
 * @retval SPUDRESULT_NULL_DESC `desc` is NULL.
 * @retval SPUDRESULT_STRUCT_WRONG_TYPE `desc->base.type` is not
 *         SPUD_STRUCT_TYPE_GPU_DESCRIPTOR_POOL_DESC.
 * @retval SPUDRESULT_STRUCT_UNKNOWN_IN_CHAIN `desc->base.next` is not NULL.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_pool` is NULL.
 * @retval SPUDRESULT_GPU_TOO_MANY_DESCRIPTOR_POOLS `desc->pool_size_count`
 *         exceeds SPUDGPU_MAX_DESCRIPTOR_POOL_SIZES.
 * @retval SPUDRESULT_GPU_CANNOT_RESOLVE_API_SPECIFIC_DESCRIPTOR_TYPE A pool
 *         size's `descriptor_type` has no equivalent on the backend (Vulkan).
 * @return Another SPUDRESULT if the backend fails, such as
 *         SPUDRESULT_OUT_OF_MEMORY or SPUDRESULT_API_SPECIFIC_FAILURE.
 *
 * @see spudgpu_destroy_descriptor_pool()
 */
SPUDRESULT spudgpu_create_descriptor_pool(
    spudgpu_device device,
    const spudgpu_descriptor_pool_desc *desc,
    spudgpu_descriptor_pool *out_pool);

/**
 * @brief Resets a pool, freeing every set allocated from it at once.
 *
 * Cheaper than freeing sets individually. Call once per frame before writing
 * new descriptors.
 *
 * @param[in] pool Pool to reset.
 */
void spudgpu_reset_descriptor_pool(spudgpu_descriptor_pool pool);

/**
 * @brief Destroys a descriptor pool and all sets allocated from it.
 *
 * @warning Command lists using sets from this pool must have finished
 * executing before calling this.
 *
 * @param[in] pool Pool to destroy.
 */
void spudgpu_destroy_descriptor_pool(spudgpu_descriptor_pool pool);
// ============================================================================
//  Descriptor Set
//  Maps to: VkDescriptorSet (Vulkan) / MTLBuffer argument buffer (Metal)
// ============================================================================

/**
 * @brief Most descriptor set layouts a pipeline can declare, and most sets one
 * spudgpu_create_descriptor_sets() call can allocate.
 */
#define SPUDGPU_MAX_DESCRIPTOR_SET_LAYOUTS 4

/**
 * @brief Allocation descriptor for one or more descriptor sets.
 *
 * All sets in a single call are allocated from the same pool in one
 * driver round-trip (matches vkAllocateDescriptorSets semantics).
 */
typedef struct spudgpu_descriptor_set_desc {
	/** `type` is SPUD_STRUCT_TYPE_GPU_DESCRIPTOR_SET_DESC; `next` is NULL (nothing is chained onto
	 * this struct yet). Set by the caller. See SPUD_STRUCT_BASE. */
	SPUD_STRUCT_BASE base;
	/** The pool the sets are allocated from, and released by. */
	spudgpu_descriptor_pool pool;

	/** Each element describes the layout for one set being allocated. */
	spudgpu_descriptor_set_layout set_layouts[SPUDGPU_MAX_DESCRIPTOR_SET_LAYOUTS];
	/** Number of sets to allocate: the valid entries in #set_layouts. */
	uint32_t set_count;
} spudgpu_descriptor_set_desc;

/**
 * @brief Allocates descriptor sets from a pool.
 *
 * The sets are released by their pool, when it is reset or destroyed.
 *
 * @param[in]  device   Device that owns the pool.
 * @param[in]  desc     Pool to allocate from and the layout of each set.
 * @param[out] out_sets Caller-supplied array of at least `desc->set_count`
 *                      elements that receives the set handles.
 *
 * @retval SPUD_SUCCESS The sets were allocated.
 * @retval SPUDRESULT_GPU_INVALID_DEVICE `device` is NULL.
 * @retval SPUDRESULT_NULL_DESC `desc` is NULL.
 * @retval SPUDRESULT_STRUCT_WRONG_TYPE `desc->base.type` is not
 *         SPUD_STRUCT_TYPE_GPU_DESCRIPTOR_SET_DESC.
 * @retval SPUDRESULT_STRUCT_UNKNOWN_IN_CHAIN `desc->base.next` is not NULL.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_sets` is NULL.
 * @retval SPUDRESULT_GPU_INVALID_DESCRIPTOR_POOL `desc->pool` is NULL.
 * @retval SPUDRESULT_GPU_ZERO_DESCRIPTOR_SET_LAYOUTS `desc->set_count` is 0.
 * @retval SPUDRESULT_GPU_TOO_MANY_DESCRIPTOR_SET_LAYOUTS `desc->set_count`
 *         exceeds SPUDGPU_MAX_DESCRIPTOR_SET_LAYOUTS.
 * @retval SPUDRESULT_GPU_INVALID_DESCRIPTOR_SET_LAYOUT An entry of
 *         `desc->set_layouts` is NULL.
 * @retval SPUDRESULT_GPU_INTERNAL_DESCRIPTOR_SET_ALLOCATION_FAIL The sets
 *         could not be allocated, e.g. the pool is out of capacity.
 * @return Another SPUDRESULT if the backend fails, such as
 *         SPUDRESULT_OUT_OF_MEMORY or SPUDRESULT_API_SPECIFIC_FAILURE.
 */
SPUDRESULT spudgpu_create_descriptor_sets(
    spudgpu_device device,
    const spudgpu_descriptor_set_desc *desc,
    spudgpu_descriptor_set *out_sets);

// ============================================================================
//  Sampler
//  Maps to: VkSampler (Vulkan) / a D3D12_SAMPLER_DESC written into a sampler
//  descriptor heap slot at write time, not a standalone device object
//  (D3D12) / id<MTLSamplerState> (Metal).
// ============================================================================

/**
 * @brief Opaque handle to a texture sampler.
 */
typedef struct spudgpu_sampler_t *spudgpu_sampler;

/**
 * @brief Sampling filter used when a blit's source and destination regions
 * differ in size, or (see spudgpu_sampler_desc below) when a shader samples
 * a texture between texels/mip levels.
 */
typedef uint32_t SPUDGPU_FILTER;
enum {
	/** The nearest texel, or mip level. */
	SPUDGPU_FILTER_NEAREST = 0,
	/** A linear blend of the neighbouring texels, or mip levels. */
	SPUDGPU_FILTER_LINEAR = 1
};

/**
 * @brief How a sampler handles texture coordinates outside [0, 1].
 */
typedef uint32_t SPUDGPU_ADDRESS_MODE;
enum {
	/** The texture repeats. */
	SPUDGPU_ADDRESS_MODE_REPEAT          = 0,
	/** The texture repeats, mirrored every other time. */
	SPUDGPU_ADDRESS_MODE_MIRRORED_REPEAT = 1,
	/** The edge texel extends outwards. */
	SPUDGPU_ADDRESS_MODE_CLAMP_TO_EDGE   = 2,
	/** A border color is read outside the texture. */
	SPUDGPU_ADDRESS_MODE_CLAMP_TO_BORDER = 3,
};

/**
 * @brief Comparison function, used by the depth test and by a comparison
 * sampler (spudgpu_sampler_desc::compare_enable).
 *
 * Each compares the incoming value against the stored one and passes when
 * the comparison holds.
 */
typedef uint32_t SPUDGPU_COMPARE_OP;

enum {
	SPUDGPU_COMPARE_OP_NEVER            = 0, /**< Never passes */
	SPUDGPU_COMPARE_OP_LESS             = 1, /**< Passes when incoming < stored */
	SPUDGPU_COMPARE_OP_EQUAL            = 2, /**< Passes when incoming == stored */
	SPUDGPU_COMPARE_OP_LESS_OR_EQUAL    = 3, /**< Passes when incoming <= stored */
	SPUDGPU_COMPARE_OP_GREATER          = 4, /**< Passes when incoming > stored */
	SPUDGPU_COMPARE_OP_NOT_EQUAL        = 5, /**< Passes when incoming != stored */
	SPUDGPU_COMPARE_OP_GREATER_OR_EQUAL = 6, /**< Passes when incoming >= stored */
	SPUDGPU_COMPARE_OP_ALWAYS           = 7  /**< Always passes */
};

/**
 * @brief Configuration descriptor for a texture sampler.
 */
typedef struct spudgpu_sampler_desc {
	/** `type` is SPUD_STRUCT_TYPE_GPU_SAMPLER_DESC; `next` is NULL (nothing is chained onto
	 * this struct yet). Set by the caller. See SPUD_STRUCT_BASE. */
	SPUD_STRUCT_BASE base;
	/** Filter when a texel covers more than one pixel (magnification). */
	SPUDGPU_FILTER mag_filter;
	/** Filter when a pixel covers more than one texel (minification). */
	SPUDGPU_FILTER min_filter;
	/** Filter between mip levels. */
	SPUDGPU_FILTER mipmap_filter;

	/** Addressing outside [0, 1] along U. */
	SPUDGPU_ADDRESS_MODE address_mode_u;
	/** Addressing outside [0, 1] along V. */
	SPUDGPU_ADDRESS_MODE address_mode_v;
	/** Addressing outside [0, 1] along W. */
	SPUDGPU_ADDRESS_MODE address_mode_w;

	/** Added to the computed level of detail before the mip level is chosen. */
	float mip_lod_bias;
	/** Lowest level of detail sampled: 0 is the full-size mip. */
	float min_lod;
	/** Highest level of detail sampled. */
	float max_lod;

	/** 1.0 disables anisotropic filtering. */
	float max_anisotropy;

	/**
	 * A comparison sampler: each texel read is compared with a reference
	 * the shader gives (GLSL's `sampler2DShadow` family, `texture()` with a
	 * depth to compare), and the read returns 1 where #compare_op holds and
	 * 0 where it doesn't, filtered as the texels would be - so a linear
	 * filter gives the share of four texels that pass. For depth textures.
	 * false for an ordinary sampler, whose #compare_op is not read.
	 */
	bool compare_enable;
	/** What the reference is compared with the texel by, the reference first: SPUDGPU_COMPARE_OP_LESS_OR_EQUAL passes where reference <= texel. */
	SPUDGPU_COMPARE_OP compare_op;
	/** @brief A name for diagnostics, or NULL. Copied, in every build; see
	 * spud_debug_name_set(). */
	const char *debug_name;
} spudgpu_sampler_desc;

/**
 * @brief Creates a sampler.
 *
 * @param[in]  device      Device to create the sampler on.
 * @param[in]  desc        Sampler configuration.
 * @param[out] out_sampler Receives the new sampler on success.
 *
 * @retval SPUD_SUCCESS The sampler was created.
 * @retval SPUDRESULT_GPU_INVALID_DEVICE `device` is NULL.
 * @retval SPUDRESULT_NULL_DESC `desc` is NULL.
 * @retval SPUDRESULT_STRUCT_WRONG_TYPE `desc->base.type` is not
 *         SPUD_STRUCT_TYPE_GPU_SAMPLER_DESC.
 * @retval SPUDRESULT_STRUCT_UNKNOWN_IN_CHAIN `desc->base.next` is not NULL.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_sampler` is NULL.
 * @return Another SPUDRESULT if the backend fails, such as
 *         SPUDRESULT_OUT_OF_MEMORY or SPUDRESULT_API_SPECIFIC_FAILURE.
 *
 * @see spudgpu_destroy_sampler()
 */
SPUDRESULT spudgpu_create_sampler(
    spudgpu_device device,
    const spudgpu_sampler_desc *desc,
    spudgpu_sampler *out_sampler);

/**
 * @brief Destroys a sampler.
 *
 * @param[in] sampler Sampler to destroy.
 */
void spudgpu_destroy_sampler(spudgpu_sampler sampler);

// ============================================================================
//  Descriptor Writes
//  Wires actual GPU resources into the allocated binding slots.
// ============================================================================

/**
 * @brief Describes a buffer range to write into a binding slot.
 */
/* Frozen: an array element of spudgpu_write_descriptor_set. It never changes; a new need gets a new type. */
typedef struct spudgpu_descriptor_buffer_info {
	/** The buffer to bind. */
	spudgpu_buffer buffer;

	/** Byte offset from the start of the buffer to begin the binding window. */
	uint64_t offset;

	/** Byte size of the binding window. Pass 0 to bind the entire buffer. */
	uint64_t range;
} spudgpu_descriptor_buffer_info;

/**
 * @brief Describes an image view + sampler to write into a binding slot.
 */
/* Frozen: an array element of spudgpu_write_descriptor_set. It never changes; a new need gets a new type. */
typedef struct spudgpu_descriptor_image_info {
	/** The image view to bind. */
	spudgpu_image_view image_view;

	/**
	 * @brief The layout the image is expected to be in when shaders access it.
	 *
	 * A SPUDGPU_IMAGE_LAYOUT value, never a native one. Common values:
	 *   - SPUDGPU_IMAGE_LAYOUT_SHADER_READ_ONLY for sampled textures.
	 *   - SPUDGPU_IMAGE_LAYOUT_GENERAL for storage images (read/write).
	 *
	 * @see SPUDGPU_IMAGE_LAYOUT
	 */
	SPUDGPU_IMAGE_LAYOUT image_layout;
} spudgpu_descriptor_image_info;

/**
 * @brief A single write operation targeting one binding slot in a descriptor
 * set.
 *
 * Fill either buffer_info or image_info depending on the descriptor_type.
 * The unused field is ignored by the backend.
 */
typedef struct spudgpu_write_descriptor_set {
	/** `type` is SPUD_STRUCT_TYPE_GPU_WRITE_DESCRIPTOR_SET; `next` is NULL (nothing is chained onto
	 * this struct yet). Set by the caller. See SPUD_STRUCT_BASE. */
	SPUD_STRUCT_BASE base;
	/** The descriptor set to write into. */
	spudgpu_descriptor_set dst_set;

	/**
	 * The binding slot index to update (matches
	 * spudgpu_descriptor_binding_desc::binding).
	 */
	uint32_t dst_binding;

	/** First array element to update. Use 0 for non-array bindings. */
	uint32_t dst_array_element;

	/** Number of descriptors to update starting at dst_array_element. */
	uint32_t descriptor_count;

	/**
	 * The type of descriptor being written. Must match the layout's declared
	 * type.
	 */
	SPUDGPU_DESCRIPTOR_TYPE descriptor_type;

	/** Set when writing UNIFORM_BUFFER or STORAGE_BUFFER descriptors. */
	const spudgpu_descriptor_buffer_info *buffer_info;

	/**
	 * Set when writing SAMPLED_IMAGE, STORAGE_IMAGE, or COMBINED_IMAGE_SAMPLER
	 * descriptors.
	 */
	const spudgpu_descriptor_image_info *image_info;

	/** Set when writing SAMPLER or COMBINED_IMAGE_SAMPLER descriptors. */
	spudgpu_sampler sampler;
} spudgpu_write_descriptor_set;

/**
 * @brief Writes resource handles into one or more descriptor sets.
 *
 * The equivalent of vkUpdateDescriptorSets. Call after allocating sets and
 * before binding them to a command list.
 *
 * Nothing is recorded if an element of `writes` has a `base` other than {SPUD_STRUCT_TYPE_GPU_WRITE_DESCRIPTOR_SET, NULL}
 * (see SPUD_STRUCT_BASE).
 *
 * @param[in] device      Device that owns the sets.
 * @param[in] writes      Array of write operations.
 * @param[in] write_count Number of elements in `writes`.
 */
void spudgpu_update_descriptor_sets(
    spudgpu_device device,
    const spudgpu_write_descriptor_set *writes,
    uint32_t write_count);

// ============================================================================
//  Command list binding
// ============================================================================

/**
 * @brief Binds descriptor sets for subsequent draws with a graphics pipeline.
 *
 * Maps to vkCmdBindDescriptorSets. Call after binding the pipeline and before
 * the draw.
 *
 * @param[in] cmd       Command list being recorded.
 * @param[in] pipeline  Graphics pipeline whose layout defines the set slots.
 * @param[in] first_set Set index of the first element of `sets` (usually 0).
 * @param[in] sets      Array of descriptor sets to bind.
 * @param[in] set_count Number of elements in `sets`.
 */
void spudgpu_cmd_bind_descriptor_sets(
    spudgpu_command_list cmd,
    spudgpu_shader_pipeline pipeline,
    uint32_t first_set,
    const spudgpu_descriptor_set *sets,
    uint32_t set_count);

/**
 * @brief Compute-pipeline variant of spudgpu_cmd_bind_descriptor_sets().
 *
 * @param[in] cmd       Command list being recorded.
 * @param[in] pipeline  Compute pipeline whose layout defines the set slots.
 * @param[in] first_set Set index of the first element of `sets` (usually 0).
 * @param[in] sets      Array of descriptor sets to bind.
 * @param[in] set_count Number of elements in `sets`.
 */
void spudgpu_cmd_bind_descriptor_sets_compute(
    spudgpu_command_list cmd,
    spudgpu_compute_pipeline pipeline,
    uint32_t first_set,
    const spudgpu_descriptor_set *sets,
    uint32_t set_count);

// ============================================================================
//  Bindless / Descriptor Indexing
// ============================================================================

/**
 * @brief 1 when the compiled-in backend implements bindless descriptor
 * indexing and this section is declared, 0 when it is compiled out.
 *
 * Where a descriptor set has one schema per shader and is bound per draw,
 * bindless is one table per resource class, per device, that every pipeline
 * reads by a plain integer index.
 *
 * The classes are kept apart (sampled image, storage image, storage buffer)
 * and each has its own index space: Vulkan's descriptors are typed per
 * binding and cannot mix types in one array, so a single shared index space
 * could not be kept on every backend. Samplers are not part of it: a
 * spudgpu_sampler is bound the ordinary way, through a descriptor set.
 *
 * Maps to update-after-bind descriptor arrays (Vulkan 1.2 descriptor
 * indexing) and to descriptor tables under Resource Binding Tier 2 and up
 * (D3D12; spudgpu_bindless_capabilities says why tables and not Shader Model
 * 6.6's `ResourceDescriptorHeap[]`). The Metal counterpart, Argument Buffers
 * Tier 2 with `MTLResourceID`, is not implemented: it needs an
 * `MTLHeap`-backed resource allocator SpudGPU does not have.
 *
 * A build compiles in one backend, so whether this section exists is a
 * compile-time fact. Where the macro is 0 nothing in the section is
 * declared, and calling one of its functions is a compile error and not a
 * silent no-op. That is separate from
 * spudgpu_bindless_capabilities::supported, which reports whether the device
 * supports it at runtime; the calls return
 * SPUDRESULT_GPU_EXT_BINDLESS_DESCRIPTOR_INDEXING_NOT_SUPPORTED when it does
 * not. Every `SPUDGPU_EXT_<NAME>` has a matching
 * `SPUDRESULT_GPU_EXT_<NAME>_NOT_SUPPORTED` for that case.
 */
#if SPUDGPU_COMPILE_VULKAN_API || SPUDGPU_COMPILE_D3D12_API
#define SPUDGPU_EXT_BINDLESS_DESCRIPTOR_INDEXING 1
#else
#define SPUDGPU_EXT_BINDLESS_DESCRIPTOR_INDEXING 0
#endif

#if SPUDGPU_EXT_BINDLESS_DESCRIPTOR_INDEXING

/** An index that never names a registered bindless slot. */
#define SPUDGPU_BINDLESS_INVALID_INDEX 0xFFFFFFFFu

/**
 * @brief Reports whether this device supports bindless resource indexing,
 * and the addressable capacity of each resource class.
 *
 * Vulkan: reflects the shaderSampledImageArrayNonUniformIndexing /
 * descriptorBindingPartiallyBound / descriptorBindingUpdateAfterBind* /
 * runtimeDescriptorArray feature bits (core in 1.2) and the corresponding
 * maxDescriptorSetUpdateAfterBind* device limits.
 * D3D12: reflects Resource Binding Tier 2+ unbounded/update-after-bind-style
 * descriptor tables — SpudGPU's D3D12 backend cross-compiles the same SPIR-V
 * shaders used by Vulkan via SPIRV-Cross rather than hand-authoring HLSL, so
 * bindless goes through a large descriptor table per resource class (what
 * SPIRV-Cross emits for a GLSL runtime-sized binding array) rather than
 * Shader Model 6.6's ResourceDescriptorHeap[] dynamic resources, which would
 * require hand-written HLSL outside that pipeline.
 * Metal: always supported — Argument Buffers Tier 2 is required on every
 * Metal 3 device SpudGPU targets.
 *
 * Every class uses a plain 0-based index on all three backends — each
 * resource class gets its own dedicated table/array/argument-buffer slot
 * range, so unlike a single flat descriptor heap, no base offset is ever
 * needed before indexing it from the shader.
 */
typedef struct spudgpu_bindless_capabilities {
	/** `type` is SPUD_STRUCT_TYPE_GPU_BINDLESS_CAPABILITIES; `next` is NULL (nothing is chained onto
	 * this struct yet). The caller builds the chain and sets each type; SpudGPU fills in the rest. See SPUD_STRUCT_BASE. */
	SPUD_STRUCT_BASE_OUT base;
	/**
	 * Whether the device supports bindless descriptor indexing. When false
	 * the limits below are 0 and the bindless calls return
	 * SPUDRESULT_GPU_EXT_BINDLESS_DESCRIPTOR_INDEXING_NOT_SUPPORTED.
	 */
	bool supported;

	/** Number of slots in the sampled image table. */
	uint32_t max_sampled_images;
	/** Number of slots in the storage image table. */
	uint32_t max_storage_images;
	/** Number of slots in the storage buffer table. */
	uint32_t max_storage_buffers;
} spudgpu_bindless_capabilities;

/**
 * @brief Reports whether a device supports bindless resource indexing, and
 * the capacity of each resource class.
 *
 * Never returns SPUDRESULT_GPU_EXT_BINDLESS_DESCRIPTOR_INDEXING_NOT_SUPPORTED
 * itself. That code is for the register and layout functions, which are real
 * operations that can fail for that reason; a query reporting "false" is not a
 * failure.
 *
 * @param[in]  device   Device to query.
 * @param[out] out_caps Receives the capabilities. `out_caps->supported` may be
 *                      false; that is the answer, not an error.
 *
 * @retval SPUD_SUCCESS `out_caps` was filled in.
 * @retval SPUDRESULT_GPU_INVALID_DEVICE `device` is NULL.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_caps` is NULL.
 * @retval SPUDRESULT_STRUCT_WRONG_TYPE `out_caps->base.type` is not
 *         SPUD_STRUCT_TYPE_GPU_BINDLESS_CAPABILITIES.
 * @retval SPUDRESULT_STRUCT_UNKNOWN_IN_CHAIN `out_caps->base.next` is not NULL.
 */
SPUDRESULT spudgpu_get_bindless_capabilities(
    spudgpu_device device,
    spudgpu_bindless_capabilities *out_caps);

/**
 * @brief Returns the device's single global bindless descriptor set layout.
 *
 * Include this handle in spudgpu_shader_pipeline_desc::descriptor_set_layouts
 * (or spudgpu_compute_pipeline_desc's) at whichever set index the shaders
 * declare the unbounded arrays at (binding 0 = sampled images, binding 1 =
 * storage images, binding 2 = storage buffers), then pass that same index as
 * `set_index` to spudgpu_cmd_bind_bindless_resources().
 *
 * D3D12: the handle describes three large descriptor-table ranges (one per
 * resource class) rather than SM6.6 ResourceDescriptorHeap[] dynamic
 * resources; see spudgpu_bindless_capabilities for why.
 *
 * @warning Do not pass the returned handle to
 * spudgpu_destroy_descriptor_set_layout(); the device owns it for its
 * lifetime.
 *
 * @param[in] device Device to get the layout from.
 *
 * @return The layout, or NULL if `device` is NULL or the device or driver does
 *         not support bindless. Check
 *         spudgpu_bindless_capabilities::supported first to tell the two
 *         apart.
 */
spudgpu_descriptor_set_layout spudgpu_get_bindless_descriptor_set_layout(spudgpu_device device);

/**
 * @brief Registers a sampled-image view in the device's global bindless
 * table and returns a stable index for it.
 *
 * The index, and the underlying image and view, must stay valid until
 * spudgpu_bindless_unregister_sampled_image() is called. Registration uses
 * update-after-bind writes, so it is safe to call between frames, or while
 * command lists referencing *other* indices are in flight; do not rewrite an
 * index that a submitted but not yet completed command list is reading.
 *
 * @param[in]  device    Device whose bindless table is written.
 * @param[in]  view      Image view to register.
 * @param[out] out_index Receives the index shaders use to reach the view.
 *
 * @retval SPUD_SUCCESS `out_index` was written.
 * @retval SPUDRESULT_GPU_INVALID_DEVICE `device` is NULL.
 * @retval SPUDRESULT_GPU_INVALID_IMAGE_VIEW `view` is NULL.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_index` is NULL.
 * @retval SPUDRESULT_GPU_EXT_BINDLESS_DESCRIPTOR_INDEXING_NOT_SUPPORTED The
 *         device or driver does not support bindless; check
 *         spudgpu_bindless_capabilities::supported to avoid this.
 * @retval SPUDRESULT_GPU_BINDLESS_OUT_OF_SLOTS Every slot of this resource
 *         class is registered.
 */
SPUDRESULT spudgpu_bindless_register_sampled_image(
    spudgpu_device device,
    spudgpu_image_view view,
    uint32_t *out_index);

/**
 * @brief Frees a registered sampled-image slot for reuse.
 *
 * @param[in] device Device whose bindless table is written.
 * @param[in] index  Index returned by
 *                   spudgpu_bindless_register_sampled_image().
 */
void spudgpu_bindless_unregister_sampled_image(
    spudgpu_device device,
    uint32_t index);

/**
 * @brief Registers a storage-image view in the device's global bindless
 * table and returns a stable index for it.
 *
 * Same rules as spudgpu_bindless_register_sampled_image().
 *
 * @param[in]  device    Device whose bindless table is written.
 * @param[in]  view      Image view to register.
 * @param[out] out_index Receives the index shaders use to reach the view.
 *
 * @retval SPUD_SUCCESS `out_index` was written.
 * @retval SPUDRESULT_GPU_INVALID_DEVICE `device` is NULL.
 * @retval SPUDRESULT_GPU_INVALID_IMAGE_VIEW `view` is NULL.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_index` is NULL.
 * @retval SPUDRESULT_GPU_EXT_BINDLESS_DESCRIPTOR_INDEXING_NOT_SUPPORTED The
 *         device or driver does not support bindless; check
 *         spudgpu_bindless_capabilities::supported to avoid this.
 * @retval SPUDRESULT_GPU_BINDLESS_OUT_OF_SLOTS Every slot of this resource
 *         class is registered.
 */
SPUDRESULT spudgpu_bindless_register_storage_image(
    spudgpu_device device,
    spudgpu_image_view view,
    uint32_t *out_index);

/**
 * @brief Frees a registered storage-image slot for reuse.
 *
 * @param[in] device Device whose bindless table is written.
 * @param[in] index  Index returned by
 *                   spudgpu_bindless_register_storage_image().
 */
void spudgpu_bindless_unregister_storage_image(
    spudgpu_device device,
    uint32_t index);

/**
 * @brief Registers a storage-buffer view in the device's global bindless
 * table and returns a stable index for it.
 *
 * Same rules as spudgpu_bindless_register_sampled_image().
 *
 * @param[in]  device    Device whose bindless table is written.
 * @param[in]  view      Buffer view to register.
 * @param[out] out_index Receives the index shaders use to reach the view.
 *
 * @retval SPUD_SUCCESS `out_index` was written.
 * @retval SPUDRESULT_GPU_INVALID_DEVICE `device` is NULL.
 * @retval SPUDRESULT_GPU_INVALID_BUFFER_VIEW `view` is NULL.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_index` is NULL.
 * @retval SPUDRESULT_GPU_EXT_BINDLESS_DESCRIPTOR_INDEXING_NOT_SUPPORTED The
 *         device or driver does not support bindless; check
 *         spudgpu_bindless_capabilities::supported to avoid this.
 * @retval SPUDRESULT_GPU_BINDLESS_OUT_OF_SLOTS Every slot of this resource
 *         class is registered.
 */
SPUDRESULT spudgpu_bindless_register_storage_buffer(
    spudgpu_device device,
    spudgpu_buffer_view view,
    uint32_t *out_index);

/**
 * @brief Frees a registered storage-buffer slot for reuse.
 *
 * @param[in] device Device whose bindless table is written.
 * @param[in] index  Index returned by
 *                   spudgpu_bindless_register_storage_buffer().
 */
void spudgpu_bindless_unregister_storage_buffer(
    spudgpu_device device,
    uint32_t index);

/**
 * @brief Makes the device's global bindless resource tables visible to
 * subsequent draws on a command list.
 *
 * - Vulkan: vkCmdBindDescriptorSets against the global set at `set_index`.
 * - D3D12: ID3D12GraphicsCommandList::SetDescriptorHeaps with the device's
 *   global CBV/SRV/UAV heap; `set_index` is ignored.
 * - Metal: marks every currently-registered resource resident on this command
 *   buffer (useResource:usage:); `set_index` is ignored.
 *
 * Call once per command list before the first bindless-indexed draw.
 *
 * @param[in] cmd       Command list being recorded.
 * @param[in] pipeline  The bound graphics pipeline.
 * @param[in] set_index Slot in the pipeline's `descriptor_set_layouts` that
 *                      holds the handle from
 *                      spudgpu_get_bindless_descriptor_set_layout().
 */
void spudgpu_cmd_bind_bindless_resources(
    spudgpu_command_list cmd,
    spudgpu_shader_pipeline pipeline,
    uint32_t set_index);

/**
 * @brief Compute-pipeline variant of spudgpu_cmd_bind_bindless_resources().
 *
 * @param[in] cmd       Command list being recorded.
 * @param[in] pipeline  The bound compute pipeline.
 * @param[in] set_index Slot in the pipeline's `descriptor_set_layouts` that
 *                      holds the handle from
 *                      spudgpu_get_bindless_descriptor_set_layout().
 */
void spudgpu_cmd_bind_bindless_resources_compute(
    spudgpu_command_list cmd,
    spudgpu_compute_pipeline pipeline,
    uint32_t set_index);

#endif // SPUDGPU_EXT_BINDLESS_DESCRIPTOR_INDEXING

/**
 * @brief How vertices are assembled into primitives.
 */
typedef uint32_t SPUDGPU_PRIMITIVE_TOPOLOGY;

enum {
	SPUDGPU_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST  = 0, /**< Every three vertices are a triangle */
	SPUDGPU_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP = 1, /**< Each vertex after the second makes a triangle with the two before it */
	SPUDGPU_PRIMITIVE_TOPOLOGY_LINE_LIST      = 2, /**< Every two vertices are a line */
	SPUDGPU_PRIMITIVE_TOPOLOGY_LINE_STRIP     = 3, /**< Each vertex after the first makes a line with the one before it */
	SPUDGPU_PRIMITIVE_TOPOLOGY_POINT_LIST     = 4, /**< Every vertex is a point */
	SPUDGPU_PRIMITIVE_TOPOLOGY_PATCH_LIST     = 5  /**< Tessellation patches of spudgpu_shader_pipeline_desc::patch_control_points vertices */
};

/**
 * @brief Which triangle faces are discarded before rasterization.
 */
typedef uint32_t SPUDGPU_CULL_MODE;

enum {
	SPUDGPU_CULL_MODE_NONE  = 0, /**< No face is discarded */
	SPUDGPU_CULL_MODE_FRONT = 1, /**< Front faces are discarded */
	SPUDGPU_CULL_MODE_BACK  = 2  /**< Back faces are discarded */
};


/**
 * @brief Capacity of spudgpu_shader_pipeline_desc::vertex_attributes.
 */
#define SPUDGPU_MAX_VERTEX_ATTRIBUTES 16

/**
 * @brief Capacity of spudgpu_shader_pipeline_desc::vertex_bindings.
 */
#define SPUDGPU_MAX_VERTEX_BINDINGS 8

/**
 * @brief Capacity of a pipeline descriptor's push_constant_ranges array.
 */
#define SPUDGPU_MAX_PUSH_CONSTANT_RANGES 4

/**
 * @brief Upper bound on simultaneous color attachments: in one rendering
 * pass, and in one pipeline's spudgpu_shader_pipeline_desc.
 */
#define SPUDGPU_MAX_COLOR_ATTACHMENTS 8

/**
 * @brief One vertex attribute: where it sits in a vertex and which shader
 * input it feeds.
 */
/* Frozen: an array element of spudgpu_shader_pipeline_desc. It never changes; a new need gets a new type. */
typedef struct spudgpu_vertex_attribute_desc {
	/**
	 * Which shader location slot this attribute binds to (layout(location =
	 * N)).
	 */
	uint32_t location;

	/** Which vertex buffer binding slot this attribute is sourced from. */
	uint32_t binding;

	/**
	 * The data layout and channel bit-depth of this attribute (e.g.,
	 * SPUDGPU_FORMAT_R32G32B32_FLOAT).
	 */
	SPUDGPU_FORMAT format;

	/** Byte offset of this attribute from the start of a single vertex element. */
	uint32_t offset;
} spudgpu_vertex_attribute_desc;

/**
 * @brief One vertex buffer binding slot: its stride and whether it advances
 * per vertex or per instance.
 */
/* Frozen: an array element of spudgpu_shader_pipeline_desc. It never changes; a new need gets a new type. */
typedef struct spudgpu_vertex_binding_desc {
	/** The binding slot index this entry targets. */
	uint32_t binding;

	/**
	 * Byte distance between consecutive elements in the buffer (sizeof your
	 * vertex struct).
	 */
	uint32_t stride;

	/**
	 * When true, advances per-instance rather than per-vertex (instanced
	 * rendering).
	 */
	bool per_instance;
} spudgpu_vertex_binding_desc;

/**
 * @brief A byte range of the push constant block and the shader stages that
 * read it.
 */
/* Frozen: an array element of the pipeline descs. It never changes; a new need gets a new type. */
typedef struct spudgpu_push_constant_range_desc {
	/**
	 * Bitmask of shader stages that can read this push constant range.
	 * @see SPUDGPU_SHADER_STAGE
	 */
	SPUDGPU_SHADER_STAGE stage_flags;

	/** Byte offset within the push constant block. */
	uint32_t offset;

	/** Byte size of this push constant range. */
	uint32_t size;
} spudgpu_push_constant_range_desc;

/**
 * @brief Factor a source or destination color or alpha is multiplied by when
 * blending.
 */
typedef uint32_t SPUDGPU_BLEND_FACTOR;

enum {
	SPUDGPU_BLEND_FACTOR_ZERO                = 0, /**< 0 */
	SPUDGPU_BLEND_FACTOR_ONE                 = 1, /**< 1 */
	SPUDGPU_BLEND_FACTOR_SRC_ALPHA           = 2, /**< Source alpha */
	SPUDGPU_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA = 3, /**< 1 - source alpha */
	SPUDGPU_BLEND_FACTOR_DST_ALPHA           = 4, /**< Destination alpha */
	SPUDGPU_BLEND_FACTOR_ONE_MINUS_DST_ALPHA = 5, /**< 1 - destination alpha */
	SPUDGPU_BLEND_FACTOR_SRC_COLOR           = 6, /**< Source color, per channel */
	SPUDGPU_BLEND_FACTOR_ONE_MINUS_SRC_COLOR = 7, /**< 1 - source color, per channel */
	SPUDGPU_BLEND_FACTOR_DST_COLOR           = 8, /**< Destination color, per channel */
	SPUDGPU_BLEND_FACTOR_ONE_MINUS_DST_COLOR = 9  /**< 1 - destination color, per channel */
};

/**
 * @brief How the weighted source and destination values are combined when
 * blending.
 */
typedef uint32_t SPUDGPU_BLEND_OP;

enum {
	SPUDGPU_BLEND_OP_ADD              = 0, /**< source + destination */
	SPUDGPU_BLEND_OP_SUBTRACT         = 1, /**< source - destination */
	SPUDGPU_BLEND_OP_REVERSE_SUBTRACT = 2, /**< destination - source */
	SPUDGPU_BLEND_OP_MIN              = 3, /**< The smaller of the two, factors ignored */
	SPUDGPU_BLEND_OP_MAX              = 4  /**< The larger of the two, factors ignored */
};

/**
 * @brief Per-attachment blend configuration.
 * Ignored entirely when blend_enable is false.
 */
/* Frozen: an array element of spudgpu_shader_pipeline_desc. It never changes; a new need gets a new type. */
typedef struct spudgpu_blend_attachment_desc {
	/** When false the fragment's color replaces the attachment's. */
	bool blend_enable;

	/** What the fragment's RGB is multiplied by. */
	SPUDGPU_BLEND_FACTOR src_color_blend_factor;
	/** What the attachment's RGB is multiplied by. */
	SPUDGPU_BLEND_FACTOR dst_color_blend_factor;
	/** How the two weighted RGB values are combined. */
	SPUDGPU_BLEND_OP color_blend_op;

	/** What the fragment's alpha is multiplied by. */
	SPUDGPU_BLEND_FACTOR src_alpha_blend_factor;
	/** What the attachment's alpha is multiplied by. */
	SPUDGPU_BLEND_FACTOR dst_alpha_blend_factor;
	/** How the two weighted alpha values are combined. */
	SPUDGPU_BLEND_OP alpha_blend_op;
} spudgpu_blend_attachment_desc;

/**
 * @brief Configuration descriptor for loading a SPIR-V shader binary.
 */
typedef struct spudgpu_shader_module_desc {
	/** `type` is SPUD_STRUCT_TYPE_GPU_SHADER_MODULE_DESC; `next` is NULL (nothing is chained onto
	 * this struct yet). Set by the caller. See SPUD_STRUCT_BASE. */
	SPUD_STRUCT_BASE base;
	/**
	 * Which pipeline stage this module targets.
	 * @see SPUDGPU_SHADER_STAGE
	 */
	SPUDGPU_SHADER_STAGE stage;

	/** Pointer to the raw SPIR-V bytecode. Must be 4-byte aligned. */
	const void *spirv_code;

	/** Byte size of the SPIR-V blob. Must be a multiple of 4. */
	uint64_t spirv_size;

	/** @brief A name for diagnostics, or NULL. Copied, in every build; see
	 * spud_debug_name_set(). */
	const char *debug_name;
} spudgpu_shader_module_desc;

/**
 * @brief Creates a shader module from a SPIR-V binary.
 *
 * @param[in]  device     Device to create the module on.
 * @param[in]  desc       Shader stage and SPIR-V code.
 * @param[out] out_module Receives the new module on success.
 *
 * @retval SPUD_SUCCESS The module was created.
 * @retval SPUDRESULT_GPU_INVALID_DEVICE `device` is NULL.
 * @retval SPUDRESULT_NULL_DESC `desc` is NULL.
 * @retval SPUDRESULT_STRUCT_WRONG_TYPE `desc->base.type` is not
 *         SPUD_STRUCT_TYPE_GPU_SHADER_MODULE_DESC.
 * @retval SPUDRESULT_STRUCT_UNKNOWN_IN_CHAIN `desc->base.next` is not NULL.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_module` is NULL.
 * @retval SPUDRESULT_GPU_NULL_SPIRV `desc->spirv_code` is NULL or
 *         `desc->spirv_size` is 0.
 * @retval SPUDRESULT_GPU_INVALID_SPIRV_ALIGNMENT `desc->spirv_size` is not a
 *         multiple of 4.
 * @retval SPUDRESULT_GPU_INVALID_SHADER_STAGE `desc->stage` is not a single
 *         known shader stage.
 * @retval SPUDRESULT_GPU_SHADER_COMPILATION_FAILED The SPIR-V could not be
 *         translated or compiled for the backend.
 * @return Another SPUDRESULT if the backend fails, such as
 *         SPUDRESULT_OUT_OF_MEMORY or SPUDRESULT_API_SPECIFIC_FAILURE.
 *
 * @see spudgpu_destroy_shader_module()
 */
SPUDRESULT spudgpu_create_shader_module(
    spudgpu_device device,
    const spudgpu_shader_module_desc *desc,
    spudgpu_shader_module *out_module);

/**
 * @brief Destroys a shader module.
 *
 * @param[in] shader_module Module to destroy.
 */
void spudgpu_destroy_shader_module(spudgpu_shader_module shader_module);

/**
 * @brief Complete configuration descriptor for creating a graphics shader
 * pipeline.
 *
 * Fill in the stage modules you need, leave optional ones NULL.
 * All arrays are inline and fixed-capacity; use the corresponding _count
 * field to indicate how many entries are valid.
 */
typedef struct spudgpu_shader_pipeline_desc {
	/** `type` is SPUD_STRUCT_TYPE_GPU_SHADER_PIPELINE_DESC; `next` is NULL (nothing is chained onto
	 * this struct yet). Set by the caller. See SPUD_STRUCT_BASE. */
	SPUD_STRUCT_BASE base;

	/** @brief A name for diagnostics, or NULL. Copied, in every build; see
	 * spud_debug_name_set(). */
	const char *debug_name;

	// -----------------------------------------------------------------------
	// Shader stages
	// -----------------------------------------------------------------------

	/**
	 * Compiled vertex shader module. Required unless mesh_module is set —
	 * a pipeline is either vertex-fetch-based (vertex_module +
	 * vertex_attributes/vertex_bindings below) or mesh-shader-based
	 * (mesh_module, which supplies its own geometry with no vertex input
	 * state at all), never both. @see SPUDGPU_EXT_MESH_SHADING
	 */
	spudgpu_shader_module vertex_module;
	/** Null-terminated entry point name. Pass NULL to default to "main". */
	const char *vertex_entry_point;

	/**
	 * Compiled mesh shader module. Mutually exclusive with vertex_module —
	 * set this instead to build a mesh-shader pipeline (no vertex input
	 * layout/input-assembly stage). Optional — leave NULL for a classic
	 * vertex-fetch pipeline. @see SPUDGPU_EXT_MESH_SHADING
	 */
	spudgpu_shader_module mesh_module;
	/** Null-terminated entry point name. Pass NULL to default to "main". */
	const char *mesh_entry_point;

	/**
	 * Compiled amplification/task shader module, upstream of mesh_module.
	 * Optional — leave NULL to dispatch mesh shader workgroups directly
	 * with no task stage (this is what spudgpu_cmd_dispatch_mesh does).
	 * Ignored unless mesh_module is also set. @see SPUDGPU_EXT_MESH_SHADING
	 */
	spudgpu_shader_module task_module;
	/** Null-terminated entry point name. Pass NULL to default to "main". */
	const char *task_entry_point;

	/** Compiled fragment shader module. Required. */
	spudgpu_shader_module fragment_module;
	/** Null-terminated entry point name. Pass NULL to default to "main". */
	const char *fragment_entry_point;

	/** Compiled geometry shader module. Optional — leave NULL to skip. */
	spudgpu_shader_module geometry_module;
	/** Null-terminated entry point name. Pass NULL to default to "main". */
	const char *geometry_entry_point;

	/**
	 * Compiled tessellation control shader module. Optional — leave NULL to
	 * skip.
	 */
	spudgpu_shader_module tess_control_module;
	/** Null-terminated entry point name. Pass NULL to default to "main". */
	const char *tess_control_entry_point;

	/**
	 * Compiled tessellation evaluation shader module. Optional — leave NULL to
	 * skip.
	 */
	spudgpu_shader_module tess_eval_module;
	/** Null-terminated entry point name. Pass NULL to default to "main". */
	const char *tess_eval_entry_point;

	// -----------------------------------------------------------------------
	// Vertex input layout
	// -----------------------------------------------------------------------

	/**
	 * The vertex shader's inputs. Only the first #vertex_attribute_count are
	 * read. Unused by a mesh-shader pipeline.
	 */
	spudgpu_vertex_attribute_desc vertex_attributes[SPUDGPU_MAX_VERTEX_ATTRIBUTES];
	/** Number of valid entries in #vertex_attributes. */
	uint32_t vertex_attribute_count;

	/**
	 * The vertex buffer slots the attributes read from. Only the first
	 * #vertex_binding_count are read. Unused by a mesh-shader pipeline.
	 */
	spudgpu_vertex_binding_desc vertex_bindings[SPUDGPU_MAX_VERTEX_BINDINGS];
	/** Number of valid entries in #vertex_bindings. */
	uint32_t vertex_binding_count;

	// -----------------------------------------------------------------------
	// Input assembly
	// -----------------------------------------------------------------------

	/**
	 * How raw vertices are assembled into primitives before rasterization.
	 * @see SPUDGPU_PRIMITIVE_TOPOLOGY
	 */
	SPUDGPU_PRIMITIVE_TOPOLOGY primitive_topology;

	// -----------------------------------------------------------------------
	// Rasterizer
	// -----------------------------------------------------------------------

	/**
	 * Which triangle faces to discard before fragment shading.
	 * @see SPUDGPU_CULL_MODE
	 */
	SPUDGPU_CULL_MODE cull_mode;

	/**
	 * When true, triangles with counter-clockwise winding are treated as
	 * front-facing.
	 */
	bool front_face_ccw;

	/**
	 * When true, geometry is rasterized as wireframe lines instead of filled
	 * triangles.
	 */
	bool wireframe;

	/**
	 * Depth bias, added to each fragment's depth before the depth test and
	 * the depth write: what keeps a shadow map's surfaces from shadowing
	 * themselves. The bias is
	 *
	 *     depth_bias_constant * r + depth_bias_slope * m
	 *
	 * clamped to #depth_bias_clamp, where m is the triangle's greatest depth
	 * slope and r the smallest difference the depth format resolves. All
	 * three 0 is no bias. One rule holds on every backend: for a UNORM depth
	 * format r is 1 / 2^bits; for a float format each graphics API derives
	 * it from the triangle's exponent in its own way, so the same constant
	 * may move a float depth by a different amount per backend. What a
	 * bias does to line and point primitives differs between the APIs and
	 * is undefined here: bias triangles.
	 *
	 * An integer because D3D12's constant is one. Vulkan:
	 * `VkPipelineRasterizationStateCreateInfo::depthBias*`. D3D12:
	 * `D3D12_RASTERIZER_DESC::DepthBias`, `SlopeScaledDepthBias`,
	 * `DepthBiasClamp`. Metal: `setDepthBias:slopeScale:clamp:` on the
	 * encoder, set by spudgpu_cmd_bind_pipeline().
	 */
	int32_t depth_bias_constant;
	/** The depth slope's factor in the bias. See #depth_bias_constant. */
	float depth_bias_slope;
	/**
	 * The largest bias, if positive, or the smallest, if negative; 0 for no
	 * clamp. Anything but 0 needs
	 * SPUDGPU_DEVICE_PROPERTIES::depth_bias_clamp. See
	 * #depth_bias_constant.
	 */
	float depth_bias_clamp;

	// -----------------------------------------------------------------------
	// Depth / stencil
	// -----------------------------------------------------------------------

	/** Enable depth testing against the depth attachment. */
	bool depth_test_enable;

	/** Allow the depth test to write new values into the depth attachment. */
	bool depth_write_enable;

	/**
	 * The comparison function used when depth testing a fragment.
	 * @see SPUDGPU_COMPARE_OP
	 */
	SPUDGPU_COMPARE_OP depth_compare_op;

	/**
	 * Discards a fragment whose depth attachment value (already primed by
	 * an earlier draw, not this fragment's own depth) falls outside
	 * spudgpu_cmd_set_depth_bounds' [min, max] range. Independent of
	 * depth_test_enable/depth_compare_op above — the two mechanisms compose
	 * (both run when both are enabled) rather than one replacing the other.
	 * Ignored on Metal, which has no pipeline-level depth-bounds toggle at
	 * all — there, spudgpu_cmd_set_depth_bounds alone both enables and
	 * configures it per draw (see its own doc comment). On Vulkan/D3D12, a
	 * no-op if this device/driver doesn't support the depth bounds test
	 * (see SPUDGPU_EXT_DEPTH_BOUNDS_TEST) — the pipeline behaves as if this
	 * is false. Check spudgpu_depth_bounds_capabilities::supported before
	 * relying on it even on a backend that compiles the extension in.
	 */
	bool depth_bounds_test_enable;

	// -----------------------------------------------------------------------
	// Blend state
	// -----------------------------------------------------------------------

	/**
	 * Blend configuration for each color attachment: entry i for
	 * #color_attachment_formats[i]. Only the first #color_attachment_count
	 * are read. Entries that differ from entry 0 need
	 * SPUDGPU_DEVICE_PROPERTIES::independent_blend.
	 * @see spudgpu_blend_attachment_desc
	 */
	spudgpu_blend_attachment_desc blend_attachments[SPUDGPU_MAX_COLOR_ATTACHMENTS];

	// -----------------------------------------------------------------------
	// Attachment formats
	// -----------------------------------------------------------------------

	/**
	 * Pixel format of each color attachment the pipeline writes, in the
	 * order of the fragment shader's outputs (location i writes attachment
	 * i) and of spudgpu_rendering_begin_desc::color_attachments in a pass
	 * that draws with it, which has the same count and formats. Only the
	 * first #color_attachment_count are read.
	 * @see SPUDGPU_FORMAT
	 */
	SPUDGPU_FORMAT color_attachment_formats[SPUDGPU_MAX_COLOR_ATTACHMENTS];
	/**
	 * Number of valid entries in #color_attachment_formats and
	 * #blend_attachments. 0 for a pipeline that writes depth only. At most
	 * SPUDGPU_MAX_COLOR_ATTACHMENTS.
	 */
	uint32_t color_attachment_count;

	/**
	 * Pixel format of the depth attachment. Set to SPUDGPU_FORMAT_UNKNOWN for
	 * no depth.
	 * @see SPUDGPU_FORMAT
	 */
	SPUDGPU_FORMAT depth_format;

	// -----------------------------------------------------------------------
	// Pipeline layout
	// -----------------------------------------------------------------------

	/**
	 * Opaque spudgpu_descriptor_set_layout handles, from
	 * spudgpu_create_descriptor_set_layout() - declared as void* here
	 * rather than the proper typedef, but each backend casts entries back
	 * to its own internal descriptor-set-layout type, the same as every
	 * other opaque handle in this header. Not a raw native handle of any
	 * specific backend's API.
	 */
	void *descriptor_set_layouts[SPUDGPU_MAX_DESCRIPTOR_SET_LAYOUTS];
	/** Number of valid entries in #descriptor_set_layouts. */
	uint32_t descriptor_set_layout_count;

	/**
	 * The ranges of the push constant block the pipeline's stages read. Only
	 * the first #push_constant_range_count are read.
	 */
	spudgpu_push_constant_range_desc push_constant_ranges[SPUDGPU_MAX_PUSH_CONSTANT_RANGES];
	/** Number of valid entries in #push_constant_ranges. */
	uint32_t push_constant_range_count;

	// -----------------------------------------------------------------------
	// Tessellation
	// -----------------------------------------------------------------------

	/**
	 * Number of control points per patch. Only used when both tess stages are
	 * present. Defaults to 3 when set to 0.
	 */
	uint32_t patch_control_points;
} spudgpu_shader_pipeline_desc;

/**
 * @brief Creates a graphics pipeline.
 *
 * @param[in]  device       Device to create the pipeline on.
 * @param[in]  desc         Pipeline configuration.
 * @param[out] out_pipeline Receives the new pipeline on success.
 *
 * @retval SPUD_SUCCESS The pipeline was created.
 * @retval SPUDRESULT_GPU_INVALID_DEVICE `device` is NULL.
 * @retval SPUDRESULT_NULL_DESC `desc` is NULL.
 * @retval SPUDRESULT_STRUCT_WRONG_TYPE `desc->base.type` is not
 *         SPUD_STRUCT_TYPE_GPU_SHADER_PIPELINE_DESC.
 * @retval SPUDRESULT_STRUCT_UNKNOWN_IN_CHAIN `desc->base.next` is not NULL.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_pipeline` is NULL.
 * @retval SPUDRESULT_GPU_VERTEX_AND_FRAGMENT_SHADER_REQUIRED
 *         `desc->fragment_module` is NULL, or both `desc->vertex_module` and
 *         `desc->mesh_module` are.
 * @retval SPUDRESULT_GPU_INVALID_SHADER_STAGE `desc->vertex_module` and
 *         `desc->mesh_module` are both set.
 * @retval SPUDRESULT_GPU_TOO_MANY_COLOR_ATTACHMENTS
 *         `desc->color_attachment_count` is above
 *         SPUDGPU_MAX_COLOR_ATTACHMENTS.
 * @retval SPUDRESULT_GPU_DEPTH_BIAS_CLAMP_NOT_SUPPORTED
 *         `desc->depth_bias_clamp` is not 0 and the device can't clamp
 *         (SPUDGPU_DEVICE_PROPERTIES::depth_bias_clamp).
 * @retval SPUDRESULT_GPU_INDEPENDENT_BLEND_NOT_SUPPORTED Two of the first
 *         `desc->color_attachment_count` blend attachments differ and the
 *         device can't blend them independently
 *         (SPUDGPU_DEVICE_PROPERTIES::independent_blend).
 * @return Another SPUDRESULT if the backend fails, such as
 *         SPUDRESULT_OUT_OF_MEMORY or SPUDRESULT_API_SPECIFIC_FAILURE.
 *
 * @see spudgpu_destroy_shader_pipeline()
 */
SPUDRESULT spudgpu_create_shader_pipeline(
    spudgpu_device device,
    const spudgpu_shader_pipeline_desc *desc,
    spudgpu_shader_pipeline *out_pipeline);

/**
 * @brief Destroys a graphics pipeline.
 *
 * @warning Command lists that bind the pipeline must have finished executing.
 *
 * @param[in] pipeline Pipeline to destroy.
 */
void spudgpu_destroy_shader_pipeline(spudgpu_shader_pipeline pipeline);

/**
 * @brief Binds a graphics pipeline for subsequent draws.
 *
 * Call inside a rendering pass, after spudgpu_cmd_begin_rendering().
 *
 * @param[in] cmd      Command list being recorded.
 * @param[in] pipeline Graphics pipeline to bind.
 */
void spudgpu_cmd_bind_pipeline(
    spudgpu_command_list cmd,
    spudgpu_shader_pipeline pipeline);

/**
 * @brief Pushes constant data to the shader stages declared in the pipeline
 * layout.
 *
 * Stage flags are derived from the pipeline's push constant ranges: any range
 * that overlaps [offset, offset+size) contributes its stages.
 *
 * Graphics and compute pipelines do not have a push constant block each:
 * after spudgpu_cmd_push_constants_compute(), what a draw reads from the
 * block is undefined until it is pushed again with this call.
 *
 * @param[in] cmd      Command list being recorded.
 * @param[in] pipeline The bound pipeline, used to look up the layout.
 * @param[in] offset   Byte offset within the push constant block.
 * @param[in] size     Number of bytes to update. Must be a non-zero multiple
 *                     of 4.
 * @param[in] data     Pointer to `size` bytes of data to push.
 *
 * @see spudgpu_cmd_push_constants_compute()
 */
void spudgpu_cmd_push_constants(
    spudgpu_command_list cmd,
    spudgpu_shader_pipeline pipeline,
    uint32_t offset,
    uint32_t size,
    const void *data);

/**
 * @brief Defines the normalized window transformation dimensions for rendering
 * output coordinates.
 *
 * Maps normalized device coordinates (NDC) ranging [-1, 1] horizontally and
 * vertically directly into a target render target screen pixel domain.
 */
/* Frozen: a value. It never changes; a new need gets a new type. */
typedef struct SPUDGPU_VIEWPORT {
	/** X-coordinate of the upper-left corner of the viewport region in pixels. */
	float x;

	/** Y-coordinate of the upper-left corner of the viewport region in pixels. */
	float y;

	/** Total width of the targeted viewport frame in pixels. */
	float width;

	/** Total height of the targeted viewport frame in pixels. */
	float height;

	/**
	 * Minimum depth boundary slice. Usually maps to `0.0f` (near clipping
	 * plane).
	 */
	float minDepth;

	/**
	 * Maximum depth boundary slice. Usually maps to `1.0f` (far clipping
	 * plane).
	 */
	float maxDepth;
} SPUDGPU_VIEWPORT;

/**
 * @brief Sets viewports for subsequent draws.
 *
 * @param[in] cmd            Command list being recorded.
 * @param[in] first_viewport Index of the first viewport slot to set.
 * @param[in] viewport_count Number of elements in `viewports`.
 * @param[in] viewports      Array of viewports.
 */
void spudgpu_cmd_set_viewports(
    spudgpu_command_list cmd,
    uint32_t first_viewport,
    uint32_t viewport_count,
    const SPUDGPU_VIEWPORT *viewports);

/**
 * @brief Defines a dynamic screen space bounding box for rasterization
 * discarding.
 *
 * Pixels falling outside the scissor rectangle bounds are completely dropped
 * by the rasterizer, optimizing rendering execution for partial UI elements or
 * UI windows.
 */
/* Frozen: a value. It never changes; a new need gets a new type. */
typedef struct SPUDGPU_SCISSOR_RECT {
	/** Leftmost X coordinate of the scissor bounding box in pixels. */
	float x;

	/** Topmost Y coordinate of the scissor bounding box in pixels. */
	float y;

	/** Total horizontal layout width of the box in pixels. */
	float width;

	/** Total vertical layout height of the box in pixels. */
	float height;
} SPUDGPU_SCISSOR_RECT;

/**
 * @brief Sets scissor rectangles for subsequent draws.
 *
 * @param[in] cmd                Command list being recorded.
 * @param[in] first_scissor_rect Index of the first scissor slot to set.
 * @param[in] scissor_rect_count Number of elements in `scissor_rects`.
 * @param[in] scissor_rects      Array of scissor rectangles.
 */
void spudgpu_cmd_set_scissor_rects(
    spudgpu_command_list cmd,
    uint32_t first_scissor_rect,
    uint32_t scissor_rect_count,
    const SPUDGPU_SCISSOR_RECT *scissor_rects);

/**
 * @brief Binds vertex buffers to consecutive input assembler slots.
 *
 * @param[in] cmd          Command list being recorded.
 * @param[in] start_slot   First vertex buffer slot to set.
 * @param[in] view_count   Number of elements in `buffer_views`.
 * @param[in] buffer_views Array of buffer views holding vertex data.
 */
void spudgpu_cmd_set_vertex_buffers(
    spudgpu_command_list cmd,
    uint32_t start_slot,
    uint32_t view_count,
    spudgpu_buffer_view *buffer_views);

/**
 * @brief Binds the index buffer used by indexed draws.
 *
 * @param[in] cmd         Command list being recorded.
 * @param[in] buffer_view Buffer view holding index data.
 */
void spudgpu_cmd_set_index_buffer(
    spudgpu_command_list cmd,
    spudgpu_buffer_view buffer_view);

/**
 * @brief Records a non-indexed, non-instanced draw.
 *
 * @param[in] cmd                   Command list being recorded.
 * @param[in] vertex_count          Number of vertices to draw.
 * @param[in] start_vertex_location Index of the first vertex.
 */
void spudgpu_cmd_draw(
    spudgpu_command_list cmd,
    uint32_t vertex_count,
    uint32_t start_vertex_location);

/**
 * @brief Records an indexed, non-instanced draw.
 *
 * @param[in] cmd                  Command list being recorded.
 * @param[in] index_count          Number of indices to read from the bound
 *                                 index buffer.
 * @param[in] start_index_location Index of the first index to read.
 * @param[in] base_vertex_location Value added to each index before it selects
 *                                 a vertex.
 */
void spudgpu_cmd_draw_indexed(
    spudgpu_command_list cmd,
    uint32_t index_count,
    uint32_t start_index_location,
    int32_t base_vertex_location);

/**
 * @brief Records a non-indexed, instanced draw.
 *
 * @param[in] cmd                       Command list being recorded.
 * @param[in] vertex_count_per_instance Number of vertices to draw per
 *                                      instance.
 * @param[in] instance_count            Number of instances to draw.
 * @param[in] start_vertex_location     Index of the first vertex.
 * @param[in] start_instance_location   Value added to each instance index
 *                                      before per-instance data is read.
 */
void spudgpu_cmd_draw_instanced(
    spudgpu_command_list cmd,
    uint32_t vertex_count_per_instance,
    uint32_t instance_count,
    uint32_t start_vertex_location,
    uint32_t start_instance_location);

/**
 * @brief Records an indexed, instanced draw.
 *
 * @param[in] cmd                      Command list being recorded.
 * @param[in] index_count_per_instance Number of indices to read per instance.
 * @param[in] instance_count           Number of instances to draw.
 * @param[in] start_index_location     Index of the first index to read.
 * @param[in] base_vertex_location     Value added to each index before it
 *                                     selects a vertex.
 * @param[in] start_instance_location  Value added to each instance index
 *                                     before per-instance data is read.
 */
void spudgpu_cmd_draw_indexed_instanced(
    spudgpu_command_list cmd,
    uint32_t index_count_per_instance,
    uint32_t instance_count,
    uint32_t start_index_location,
    int32_t base_vertex_location,
    uint32_t start_instance_location);

/**
 * @brief Per-draw argument layout consumed by spudgpu_cmd_draw_indirect.
 *
 * Bit-for-bit identical to VkDrawIndirectCommand / D3D12_DRAW_ARGUMENTS /
 * MTLDrawPrimitivesIndirectArguments — a buffer of these can be written
 * directly by a compute shader and consumed by any of the three backends
 * with no repacking.
 */
/* Frozen: read by the GPU, so its layout is the GPU's. It never changes; a new need gets a new type. */
typedef struct spudgpu_draw_indirect_args {
	/** Number of vertices to draw. */
	uint32_t vertex_count;
	/** Number of instances to draw. */
	uint32_t instance_count;
	/** Index of the first vertex. */
	uint32_t first_vertex;
	/** Index of the first instance. */
	uint32_t first_instance;
} spudgpu_draw_indirect_args;

/**
 * @brief Per-draw argument layout consumed by
 * spudgpu_cmd_draw_indexed_indirect.
 *
 * Bit-for-bit identical to VkDrawIndexedIndirectCommand /
 * D3D12_DRAW_INDEXED_ARGUMENTS / MTLDrawIndexedPrimitivesIndirectArguments.
 */
/* Frozen: read by the GPU, so its layout is the GPU's. It never changes; a new need gets a new type. */
typedef struct spudgpu_draw_indexed_indirect_args {
	/** Number of indices to draw. */
	uint32_t index_count;
	/** Number of instances to draw. */
	uint32_t instance_count;
	/** Position of the first index in the index buffer. */
	uint32_t first_index;
	/** Added to each index before it selects a vertex. */
	int32_t base_vertex;
	/** Index of the first instance. */
	uint32_t first_instance;
} spudgpu_draw_indexed_indirect_args;

/**
 * @brief Records `draw_count` non-indexed draws, each reading its arguments
 * from a consecutive spudgpu_draw_indirect_args entry in a buffer.
 *
 * The buffer must be in SPUDGPU_RESOURCE_STATE_INDIRECT_ARGUMENT (see
 * spudgpu_cmd_pipeline_barrier()) and have been created with
 * SPUDGPU_BUFFER_USAGE_INDIRECT.
 *
 * Maps to: vkCmdDrawIndirect (Vulkan), ID3D12GraphicsCommandList::ExecuteIndirect
 * against an internal draw-only command signature (D3D12),
 * drawPrimitives:indirectBuffer:indirectBufferOffset: looped `draw_count` times
 * (Metal, which has no native multi-draw-from-buffer primitive).
 *
 * @param[in] cmd        Command list being recorded.
 * @param[in] buffer     Buffer holding `draw_count` consecutive
 *                       spudgpu_draw_indirect_args entries.
 * @param[in] offset     Byte offset of the first entry.
 * @param[in] draw_count Number of draws to issue, at most
 *                       SPUDGPU_DEVICE_PROPERTIES::max_draw_indirect_count;
 *                       with more, nothing is recorded.
 * @param[in] stride     Byte stride between consecutive entries: a multiple
 *                       of 4, and at least
 *                       sizeof(spudgpu_draw_indirect_args); otherwise
 *                       nothing is recorded. Pass that size for a tightly
 *                       packed buffer; a larger stride skips the caller's
 *                       own data between entries, on every backend.
 */
void spudgpu_cmd_draw_indirect(
    spudgpu_command_list cmd,
    spudgpu_buffer buffer,
    uint64_t offset,
    uint32_t draw_count,
    uint32_t stride);

/**
 * @brief Indexed variant of spudgpu_cmd_draw_indirect().
 *
 * Reads spudgpu_draw_indexed_indirect_args entries and draws with the index
 * buffer bound by spudgpu_cmd_set_index_buffer().
 *
 * @param[in] cmd        Command list being recorded.
 * @param[in] buffer     Buffer holding `draw_count` consecutive
 *                       spudgpu_draw_indexed_indirect_args entries.
 * @param[in] offset     Byte offset of the first entry.
 * @param[in] draw_count Number of draws to issue, at most
 *                       SPUDGPU_DEVICE_PROPERTIES::max_draw_indirect_count;
 *                       with more, nothing is recorded.
 * @param[in] stride     Byte stride between consecutive entries: a multiple
 *                       of 4, and at least
 *                       sizeof(spudgpu_draw_indexed_indirect_args);
 *                       otherwise nothing is recorded. Pass that size for a
 *                       tightly packed buffer; a larger stride skips the
 *                       caller's own data between entries, on every backend.
 */
void spudgpu_cmd_draw_indexed_indirect(
    spudgpu_command_list cmd,
    spudgpu_buffer buffer,
    uint64_t offset,
    uint32_t draw_count,
    uint32_t stride);

// ============================================================================
//  Mesh Shading
// ============================================================================

/**
 * @brief 1 when the compiled-in backend implements mesh shading and this
 * section is declared, 0 when it is compiled out.
 *
 * A mesh-shader pipeline replaces vertex fetch and input assembly with a
 * compute-like stage (spudgpu_shader_pipeline_desc::mesh_module) that emits
 * meshlet geometry itself. It has no vertex attributes or bindings and no
 * bound vertex or index buffer: the mesh shader reads what it needs from
 * descriptor-bound buffers by index, as a compute shader would.
 *
 * Maps to `VK_EXT_mesh_shader` (Vulkan), mesh-shader pipeline state objects
 * with Shader Model 6.5 and up (D3D12), and `MTLMeshRenderPipelineDescriptor`
 * in Metal 3 (Metal).
 *
 * The macro is 1 on all three backends. Whether a device can draw with it is
 * a runtime fact: spudgpu_mesh_shading_capabilities::supported.
 */
#if SPUDGPU_COMPILE_VULKAN_API || SPUDGPU_COMPILE_D3D12_API || SPUDGPU_COMPILE_METAL_API
#define SPUDGPU_EXT_MESH_SHADING 1
#else
#define SPUDGPU_EXT_MESH_SHADING 0
#endif

#if SPUDGPU_EXT_MESH_SHADING

/**
 * @brief Reports whether this device supports mesh shading, and the
 * hardware limits a mesh shader must respect.
 *
 * Vulkan: reflects VK_EXT_mesh_shader support + VkPhysicalDeviceMeshShader
 * PropertiesEXT. D3D12: reflects D3D12_FEATURE_DATA_D3D12_OPTIONS7's
 * MeshShaderTier and D3D_SHADER_MODEL_6_5+ support. Metal: always supported
 * — Metal 3 mesh shading is required on every device this backend targets.
 */
typedef struct spudgpu_mesh_shading_capabilities {
	/** `type` is SPUD_STRUCT_TYPE_GPU_MESH_SHADING_CAPABILITIES; `next` is NULL (nothing is chained onto
	 * this struct yet). The caller builds the chain and sets each type; SpudGPU fills in the rest. See SPUD_STRUCT_BASE. */
	SPUD_STRUCT_BASE_OUT base;
	/**
	 * Whether the device can draw with a mesh-shader pipeline. When false
	 * the limits below are 0.
	 */
	bool supported;

	/** Most vertices one mesh shader workgroup may emit. */
	uint32_t max_mesh_output_vertices;
	/** Most primitives one mesh shader workgroup may emit. */
	uint32_t max_mesh_output_primitives;
	/** Most invocations in one mesh shader workgroup. */
	uint32_t max_mesh_workgroup_invocations;
} spudgpu_mesh_shading_capabilities;

/**
 * @brief Reports whether a device supports mesh shading, and its limits.
 *
 * @param[in]  device   Device to query.
 * @param[out] out_caps Receives the capabilities. `out_caps->supported` may be
 *                      false; that is the answer, not an error.
 *
 * @retval SPUD_SUCCESS `out_caps` was filled in.
 * @retval SPUDRESULT_GPU_INVALID_DEVICE `device` is NULL.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_caps` is NULL.
 * @retval SPUDRESULT_STRUCT_WRONG_TYPE `out_caps->base.type` is not
 *         SPUD_STRUCT_TYPE_GPU_MESH_SHADING_CAPABILITIES.
 * @retval SPUDRESULT_STRUCT_UNKNOWN_IN_CHAIN `out_caps->base.next` is not NULL.
 */
SPUDRESULT spudgpu_get_mesh_shading_capabilities(
    spudgpu_device device,
    spudgpu_mesh_shading_capabilities *out_caps);

/**
 * @brief Records a mesh-shader dispatch against the bound mesh-shader
 * pipeline.
 *
 * Callable only inside a rendering pass (between spudgpu_cmd_begin_rendering()
 * and spudgpu_cmd_end_rendering()), unlike spudgpu_cmd_dispatch(), which
 * targets a compute pipeline outside one. Does nothing if the bound pipeline
 * is not a mesh-shader pipeline, or if the device or driver does not support
 * mesh shading; check spudgpu_mesh_shading_capabilities::supported first.
 *
 * Maps to: vkCmdDrawMeshTasksEXT (Vulkan), ID3D12GraphicsCommandList6::
 * DispatchMesh (D3D12), drawMeshThreadgroups:threadsPerObjectThreadgroup:
 * threadsPerMeshThreadgroup: with a nil object function (Metal, since
 * spudgpu_shader_pipeline_desc has no task_module-driven path yet).
 *
 * The group counts are in workgroups, not threads: they match the
 * [numthreads]/local_size the pipeline's mesh module declares, the same
 * convention spudgpu_cmd_dispatch() uses for compute.
 *
 * @param[in] cmd           Command list being recorded.
 * @param[in] group_count_x Number of mesh shader workgroups in X.
 * @param[in] group_count_y Number of mesh shader workgroups in Y.
 * @param[in] group_count_z Number of mesh shader workgroups in Z.
 */
void spudgpu_cmd_dispatch_mesh(
    spudgpu_command_list cmd,
    uint32_t group_count_x,
    uint32_t group_count_y,
    uint32_t group_count_z);

#endif // SPUDGPU_EXT_MESH_SHADING

// ============================================================================
//  Depth Bounds Test
// ============================================================================

/**
 * @brief 1 when the compiled-in backend implements the depth bounds test and
 * this section is declared, 0 when it is compiled out.
 *
 * The test discards a fragment by the value the depth attachment already
 * holds at that pixel, written by an earlier draw: it passes when that value
 * is inside a [min, max] range the caller sets. It is independent of the
 * ordinary depth test (spudgpu_shader_pipeline_desc::depth_test_enable and
 * ::depth_compare_op) and runs alongside it. The usual shape is a depth-only
 * pass that primes the attachment, then a pass whose visible region the
 * range clips.
 *
 * Maps to `vkCmdSetDepthBounds` with
 * `VkPipelineDepthStencilStateCreateInfo::depthBoundsTestEnable` (Vulkan),
 * `ID3D12GraphicsCommandList1::OMSetDepthBounds` with
 * `D3D12_DEPTH_STENCIL_DESC1::DepthBoundsTestEnable` (D3D12), and
 * `-[MTLRenderCommandEncoder setDepthTestMinBound:maxBound:]` (Metal).
 *
 * The macro is 1 on all three backends, and support varies per device:
 * spudgpu_depth_bounds_capabilities::supported reports it, from
 * `VkPhysicalDeviceFeatures::depthBounds`,
 * `D3D12_FEATURE_DATA_D3D12_OPTIONS2::DepthBoundsTestSupported`, or
 * `MTLGPUFamilyApple10`.
 *
 * spudgpu_shader_pipeline_desc::depth_bounds_test_enable is declared whether
 * or not this macro is 1. Vulkan and D3D12 need it when the pipeline is
 * created; Metal has no pipeline-level switch, and there
 * spudgpu_cmd_set_depth_bounds() alone turns the test on and sets its range.
 */
#if SPUDGPU_COMPILE_VULKAN_API || SPUDGPU_COMPILE_D3D12_API || SPUDGPU_COMPILE_METAL_API
#define SPUDGPU_EXT_DEPTH_BOUNDS_TEST 1
#else
#define SPUDGPU_EXT_DEPTH_BOUNDS_TEST 0
#endif

#if SPUDGPU_EXT_DEPTH_BOUNDS_TEST

/**
 * @brief Reports whether this device supports the depth bounds test.
 *
 * Vulkan: reflects VkPhysicalDeviceFeatures::depthBounds. D3D12: reflects
 * D3D12_FEATURE_DATA_D3D12_OPTIONS2::DepthBoundsTestSupported. Metal:
 * reflects [MTLDevice supportsFamily:MTLGPUFamilyApple10] — this is a very
 * recently added Metal primitive (macOS/iOS 26+, MTLGPUFamilyApple10
 * hardware only), so expect this to be false on most Metal devices in the
 * field today.
 */
typedef struct spudgpu_depth_bounds_capabilities {
	/** `type` is SPUD_STRUCT_TYPE_GPU_DEPTH_BOUNDS_CAPABILITIES; `next` is NULL (nothing is chained onto
	 * this struct yet). The caller builds the chain and sets each type; SpudGPU fills in the rest. See SPUD_STRUCT_BASE. */
	SPUD_STRUCT_BASE_OUT base;
	/** Whether the device supports the depth bounds test. */
	bool supported;
} spudgpu_depth_bounds_capabilities;

/**
 * @brief Reports whether a device supports the depth bounds test.
 *
 * @param[in]  device   Device to query.
 * @param[out] out_caps Receives the capabilities. `out_caps->supported` may be
 *                      false; that is the answer, not an error.
 *
 * @retval SPUD_SUCCESS `out_caps` was filled in.
 * @retval SPUDRESULT_GPU_INVALID_DEVICE `device` is NULL.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_caps` is NULL.
 * @retval SPUDRESULT_STRUCT_WRONG_TYPE `out_caps->base.type` is not
 *         SPUD_STRUCT_TYPE_GPU_DEPTH_BOUNDS_CAPABILITIES.
 * @retval SPUDRESULT_STRUCT_UNKNOWN_IN_CHAIN `out_caps->base.next` is not NULL.
 */
SPUDRESULT spudgpu_get_depth_bounds_capabilities(
    spudgpu_device device,
    spudgpu_depth_bounds_capabilities *out_caps);

/**
 * @brief Sets the [min, max] depth-attachment-value range the depth bounds
 * test clips against for subsequent draws.
 *
 * On Vulkan/D3D12, call after binding a pipeline created with
 * depth_bounds_test_enable=true, before any draw that should be clipped by it.
 * Metal has no pipeline-level toggle: depth_bounds_test_enable is ignored
 * there, and this call alone both enables and configures the test for
 * subsequent draws on `cmd`. Passing (0, 1), the default, disables it;
 * anything else enables it. Calling this with (0, 1) on Vulkan/D3D12 is
 * harmless too (a bounds window spanning the whole depth range), so a caller
 * that wants one code path across all three backends can always call this
 * rather than branching on whether the bound pipeline requested
 * depth_bounds_test_enable.
 *
 * Does nothing if the device or driver does not support the depth bounds
 * test; check spudgpu_depth_bounds_capabilities::supported first.
 *
 * Maps to: vkCmdSetDepthBounds (Vulkan), ID3D12GraphicsCommandList1::
 * OMSetDepthBounds (D3D12), [MTLRenderCommandEncoder setDepthTestMinBound:
 * maxBound:] (Metal).
 *
 * @param[in] cmd              Command list being recorded.
 * @param[in] min_depth_bounds Lower bound, in [0, 1], no greater than
 *                             `max_depth_bounds`.
 * @param[in] max_depth_bounds Upper bound, in [0, 1].
 */
void spudgpu_cmd_set_depth_bounds(
    spudgpu_command_list cmd,
    float min_depth_bounds,
    float max_depth_bounds);

#endif // SPUDGPU_EXT_DEPTH_BOUNDS_TEST

/**
 * @brief Complete configuration descriptor for creating a compute shader
 * pipeline.
 *
 * Simpler than the graphics pipeline desc — no vertex input, rasterizer,
 * blend state, or render pass. Just a single compute stage and its layout.
 */
typedef struct spudgpu_compute_pipeline_desc {
	/** `type` is SPUD_STRUCT_TYPE_GPU_COMPUTE_PIPELINE_DESC; `next` is NULL (nothing is chained onto
	 * this struct yet). Set by the caller. See SPUD_STRUCT_BASE. */
	SPUD_STRUCT_BASE base;
	/** Compiled compute shader module. Required. */
	spudgpu_shader_module compute_module;

	/** Null-terminated entry point name. Pass NULL to default to "main". */
	const char *compute_entry_point;

	// -----------------------------------------------------------------------
	// Pipeline layout
	// -----------------------------------------------------------------------

	/**
	 * Opaque spudgpu_descriptor_set_layout handles, from
	 * spudgpu_create_descriptor_set_layout() - declared as void* here
	 * rather than the proper typedef, but each backend casts entries back
	 * to its own internal descriptor-set-layout type, the same as every
	 * other opaque handle in this header. Not a raw native handle of any
	 * specific backend's API.
	 */
	void *descriptor_set_layouts[SPUDGPU_MAX_DESCRIPTOR_SET_LAYOUTS];
	/** Number of valid entries in #descriptor_set_layouts. */
	uint32_t descriptor_set_layout_count;

	/**
	 * The ranges of the push constant block the pipeline's stages read. Only
	 * the first #push_constant_range_count are read.
	 */
	spudgpu_push_constant_range_desc push_constant_ranges[SPUDGPU_MAX_PUSH_CONSTANT_RANGES];
	/** Number of valid entries in #push_constant_ranges. */
	uint32_t push_constant_range_count;

	/** @brief A name for diagnostics, or NULL. Copied, in every build; see
	 * spud_debug_name_set(). */
	const char *debug_name;
} spudgpu_compute_pipeline_desc;

/**
 * @brief Creates a compute pipeline.
 *
 * @param[in]  device       Device to create the pipeline on.
 * @param[in]  desc         Pipeline configuration.
 * @param[out] out_pipeline Receives the new pipeline on success.
 *
 * @retval SPUD_SUCCESS The pipeline was created.
 * @retval SPUDRESULT_GPU_INVALID_DEVICE `device` is NULL.
 * @retval SPUDRESULT_NULL_DESC `desc` is NULL.
 * @retval SPUDRESULT_STRUCT_WRONG_TYPE `desc->base.type` is not
 *         SPUD_STRUCT_TYPE_GPU_COMPUTE_PIPELINE_DESC.
 * @retval SPUDRESULT_STRUCT_UNKNOWN_IN_CHAIN `desc->base.next` is not NULL.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_pipeline` is NULL.
 * @retval SPUDRESULT_GPU_INVALID_COMPUTE_MODULE `desc->compute_module` is
 *         NULL.
 * @return Another SPUDRESULT if the backend fails, such as
 *         SPUDRESULT_OUT_OF_MEMORY or SPUDRESULT_API_SPECIFIC_FAILURE.
 *
 * @see spudgpu_destroy_compute_pipeline()
 */
SPUDRESULT spudgpu_create_compute_pipeline(
    spudgpu_device device,
    const spudgpu_compute_pipeline_desc *desc,
    spudgpu_compute_pipeline *out_pipeline);

/**
 * @brief Destroys a compute pipeline.
 *
 * @warning Command lists that dispatch against the pipeline must have
 * finished executing.
 *
 * @param[in] pipeline Pipeline to destroy.
 */
void spudgpu_destroy_compute_pipeline(spudgpu_compute_pipeline pipeline);

/**
 * @brief Reads the descriptor a compute pipeline was created with.
 *
 * @param[in]  pipeline Compute pipeline to query.
 * @param[out] out_desc Receives the pipeline's descriptor.
 *
 * @retval SPUD_SUCCESS `out_desc` was filled in.
 * @retval SPUDRESULT_GPU_INVALID_COMPUTE_PIPELINE `pipeline` is NULL.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_desc` is NULL.
 */
SPUDRESULT spudgpu_get_compute_pipeline_desc(
    spudgpu_compute_pipeline pipeline,
    spudgpu_compute_pipeline_desc *out_desc);

/**
 * @brief Binds a compute pipeline for subsequent spudgpu_cmd_dispatch()
 * calls.
 *
 * Compute counterpart of spudgpu_cmd_bind_pipeline(). Bind descriptor sets
 * (spudgpu_cmd_bind_descriptor_sets_compute()) or bindless resources after
 * this, then dispatch.
 *
 * @param[in] cmd      Command list being recorded.
 * @param[in] pipeline Compute pipeline to bind.
 */
void spudgpu_cmd_bind_compute_pipeline(
    spudgpu_command_list cmd,
    spudgpu_compute_pipeline pipeline);

/**
 * @brief Compute-pipeline variant of spudgpu_cmd_push_constants(): pushes
 * constant data to the compute shader of a compute pipeline.
 *
 * Two rules hold on every backend, whatever the graphics API underneath
 * keeps or shares:
 *
 * - Push after spudgpu_cmd_bind_compute_pipeline() and before the
 *   spudgpu_cmd_dispatch() that reads the data. What a dispatch reads from a
 *   byte that has not been pushed since the compute pipeline was last bound
 *   is undefined.
 * - Graphics and compute pipelines do not have a push constant block each:
 *   after this call, what a draw reads from the block is undefined until it
 *   is pushed again with spudgpu_cmd_push_constants(), and the other way
 *   round.
 *
 * Does nothing if `cmd`, `pipeline` or `data` is NULL, `size` is 0, or the
 * pipeline declares no push constant ranges.
 *
 * Maps to: vkCmdPushConstants (Vulkan),
 * ID3D12GraphicsCommandList::SetComputeRoot32BitConstants (D3D12),
 * setBytes:length:atIndex: on the compute encoder (Metal).
 *
 * @param[in] cmd      Command list being recorded.
 * @param[in] pipeline The bound compute pipeline, used to look up the layout.
 * @param[in] offset   Byte offset within the push constant block. A multiple
 *                     of 4.
 * @param[in] size     Number of bytes to update. Must be a non-zero multiple
 *                     of 4, and `offset + size` must lie inside the ranges
 *                     the pipeline declares
 *                     (spudgpu_compute_pipeline_desc::push_constant_ranges).
 * @param[in] data     Pointer to `size` bytes of data to push.
 */
void spudgpu_cmd_push_constants_compute(
    spudgpu_command_list cmd,
    spudgpu_compute_pipeline pipeline,
    uint32_t offset,
    uint32_t size,
    const void *data);

/**
 * @brief Records a compute dispatch against the bound compute pipeline.
 *
 * Maps to: vkCmdDispatch (Vulkan), ID3D12GraphicsCommandList::Dispatch
 * (D3D12), dispatchThreadgroups:threadsPerThreadgroup: (Metal).
 *
 * The group counts are in workgroups, not threads: they match the
 * [numthreads]/local_size declared in the compute shader.
 *
 * @param[in] cmd           Command list being recorded.
 * @param[in] group_count_x Number of workgroups in X.
 * @param[in] group_count_y Number of workgroups in Y.
 * @param[in] group_count_z Number of workgroups in Z.
 */
void spudgpu_cmd_dispatch(
    spudgpu_command_list cmd,
    uint32_t group_count_x,
    uint32_t group_count_y,
    uint32_t group_count_z);

// ============================================================================
//  Image <-> Buffer Copies
//  Maps to: vkCmdCopyImageToBuffer / vkCmdCopyBufferToImage (Vulkan)
// ============================================================================

/**
 * @brief Describes a rectangular texel region and target mip/layer used for a
 * copy between an image and a buffer.
 *
 * Maps to Vulkan's VkBufferImageCopy / D3D12's PlacedFootprint region.
 */
/* Frozen: a copy region, passed in arrays. It never changes; a new need gets a new type. */
typedef struct spudgpu_image_buffer_copy_desc {
	/** Byte offset into the buffer where this region starts (or is read from). */
	uint64_t buffer_offset;

	/**
	 * Row length in texels, used to interpret the buffer's layout.
	 * Pass 0 for tightly packed rows (== region width).
	 */
	uint32_t buffer_row_length;

	/**
	 * Image height in texels, used to interpret the buffer's 2D/3D layout.
	 * Pass 0 for tightly packed rows (== region height).
	 */
	uint32_t buffer_image_height;

	/** Mip level of the image being copied. */
	uint32_t mip_level;

	/** Starting array layer of the image being copied. */
	uint32_t base_array_layer;

	/** Number of array layers to copy. */
	uint32_t array_layer_count;

	/** Texel-space offset into the image where the region begins. */
	uint32_t image_x, image_y, image_z;

	/** Texel-space size of the copied region. */
	uint32_t width, height, depth;
} spudgpu_image_buffer_copy_desc;

/**
 * @brief Records a copy of `size` bytes from one buffer to another.
 *
 * `src_buffer` must have been created with SPUDGPU_BUFFER_USAGE_TRANSFER_SRC,
 * `dst_buffer` with SPUDGPU_BUFFER_USAGE_TRANSFER_DST. The caller is
 * responsible for any pipeline barrier `dst_buffer` needs after the copy
 * before a later stage reads or writes it (see spudgpu_cmd_pipeline_barrier());
 * this call does no synchronization beyond ordering the copy within the
 * command list. To use `dst_buffer` in the same submission as the copy, move
 * it into SPUDGPU_RESOURCE_STATE_COPY_DEST before the copy and into the state
 * it is used in after.
 *
 * The primary use is writing data into a SPUDGPU_MEMORY_KIND_DEVICE buffer,
 * which is never mapped: create a SPUDGPU_MEMORY_KIND_UPLOAD staging buffer
 * with SPUDGPU_BUFFER_USAGE_TRANSFER_SRC, map it, write the data, flush and
 * unmap, then copy from the staging buffer into the device buffer with this
 * call. Reading one back is the same the other way, into a
 * SPUDGPU_MEMORY_KIND_READBACK buffer.
 *
 * Maps to: vkCmdCopyBuffer (Vulkan), CopyBufferRegion (D3D12),
 * copyFromBuffer:sourceOffset:toBuffer:destinationOffset:size: (Metal).
 *
 * @param[in] cmd        Command list being recorded.
 * @param[in] src_buffer Buffer to read bytes from.
 * @param[in] dst_buffer Buffer to write bytes into.
 * @param[in] src_offset Byte offset into `src_buffer` to start reading at.
 * @param[in] dst_offset Byte offset into `dst_buffer` to start writing at.
 * @param[in] size       Number of bytes to copy.
 */
void spudgpu_cmd_copy_buffer(
    spudgpu_command_list cmd,
    spudgpu_buffer src_buffer,
    spudgpu_buffer dst_buffer,
    uint64_t src_offset,
    uint64_t dst_offset,
    uint64_t size);

/**
 * @brief Records a copy from an image subresource region into a buffer.
 *
 * The image must be in SPUDGPU_IMAGE_LAYOUT_TRANSFER_SRC. Use
 * spudgpu_get_image_buffer_copy_size() to size the destination buffer first.
 *
 * Maps to: vkCmdCopyImageToBuffer (Vulkan), CopyTextureRegion (D3D12),
 * copyFromTexture:toBuffer: (Metal).
 *
 * @param[in] cmd        Command list being recorded.
 * @param[in] src_image  Image to read texel data from.
 * @param[in] dst_buffer Buffer that receives the copied bytes.
 * @param[in] desc       Region, offset and subresource to copy.
 */
void spudgpu_cmd_copy_image_to_buffer(
    spudgpu_command_list cmd,
    spudgpu_image src_image,
    spudgpu_buffer dst_buffer,
    const spudgpu_image_buffer_copy_desc *desc);

/**
 * @brief Records a copy of buffer bytes into an image subresource region.
 * Used for texture uploads.
 *
 * The image must be in SPUDGPU_IMAGE_LAYOUT_TRANSFER_DST. Use
 * spudgpu_get_image_buffer_copy_size() for the expected source row pitch and
 * total size.
 *
 * Maps to: vkCmdCopyBufferToImage (Vulkan), CopyTextureRegion (D3D12),
 * copyFromBuffer:toTexture: (Metal).
 *
 * @param[in] cmd        Command list being recorded.
 * @param[in] src_buffer Buffer to read bytes from.
 * @param[in] dst_image  Image to write texel data into.
 * @param[in] desc       Region, offset and subresource to copy.
 */
void spudgpu_cmd_copy_buffer_to_image(
    spudgpu_command_list cmd,
    spudgpu_buffer src_buffer,
    spudgpu_image dst_image,
    const spudgpu_image_buffer_copy_desc *desc);

/**
 * @brief Computes the buffer row pitch and total size needed to receive (or
 * supply) a copy of an image mip level.
 *
 * Some backends (D3D12) require a specific row-pitch alignment for buffer
 * footprints; the values returned are already aligned, so the caller can
 * allocate a correctly sized staging buffer without hardcoding backend rules.
 *
 * Does nothing if `image` is NULL.
 *
 * @param[in]  image          Image the copy targets.
 * @param[in]  mip_level      Mip level being copied.
 * @param[out] out_row_pitch  Receives the aligned row pitch, in bytes.
 * @param[out] out_total_size Receives the required buffer size, in bytes.
 */
void spudgpu_get_image_buffer_copy_size(
    spudgpu_image image,
    uint32_t mip_level,
    uint64_t *out_row_pitch,
    uint64_t *out_total_size);

/**
 * @brief Gives the alignment the buffer offset of a copy between a buffer
 * and an image must have.
 *
 * spudgpu_image_buffer_copy_desc::buffer_offset of a copy to or from `image`
 * must be a multiple of the value returned, so a caller that places several
 * copies in one buffer knows where each may start. The value is the
 * requirement and no more: an offset of 0 always meets it.
 *
 * It depends on the image's format and the graphics API, and is not always a
 * power of two (a 12-byte texel can give 12).
 *
 * @param[in]  image         Image the copy reads or writes.
 * @param[out] out_alignment Receives the alignment, in bytes.
 *
 * @retval SPUD_SUCCESS `out_alignment` was written.
 * @retval SPUDRESULT_GPU_INVALID_IMAGE `image` is NULL.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_alignment` is NULL.
 * @retval SPUDRESULT_GPU_INVALID_FORMAT The image's format has no whole
 *         number of bytes to a texel or block (see
 *         spudgpu_format_bit_count()), so no copy of it has a known layout.
 */
SPUDRESULT spudgpu_get_image_buffer_copy_offset_alignment(
    spudgpu_image image,
    uint64_t *out_alignment);

// ============================================================================
//  Image Blit
//  Maps to: vkCmdBlitImage (Vulkan)
// ============================================================================

/**
 * @brief Describes a source and destination subresource/region pair for an
 * image blit.
 *
 * Each region is given as two opposite corners (x0,y0,z0)-(x1,y1,z1) rather
 * than an offset+extent, matching Vulkan's VkImageBlit. This is what allows
 * the source and destination regions to be different sizes (the whole point
 * of a blit vs. a plain copy) — most commonly src = mip N at full size, dst =
 * mip N+1 at half size, when generating a mip chain.
 */
/* Frozen: a blit region, passed in arrays. It never changes; a new need gets a new type. */
typedef struct spudgpu_image_blit_desc {
	/** Mip level read from. */
	uint32_t src_mip_level;
	/** First array layer read from. */
	uint32_t src_base_array_layer;
	/** Number of array layers read. */
	uint32_t src_array_layer_count;
	/** One corner of the source region, in texels of #src_mip_level. */
	uint32_t src_x0, src_y0, src_z0;
	/** The opposite corner of the source region. */
	uint32_t src_x1, src_y1, src_z1;

	/** Mip level written to. */
	uint32_t dst_mip_level;
	/** First array layer written to. */
	uint32_t dst_base_array_layer;
	/** Number of array layers written. */
	uint32_t dst_array_layer_count;
	/** One corner of the destination region, in texels of #dst_mip_level. */
	uint32_t dst_x0, dst_y0, dst_z0;
	/** The opposite corner of the destination region. */
	uint32_t dst_x1, dst_y1, dst_z1;

	/** Filter applied when src and dst region sizes differ. */
	SPUDGPU_FILTER filter;
} spudgpu_image_blit_desc;

/**
 * @brief Records a (possibly scaling) copy between two image regions.
 *
 * Unlike spudgpu_cmd_copy_image_to_buffer(), source and destination regions
 * may differ in size; the driver resamples using `desc->filter`. The primary
 * use is mip chain generation: blit mip level N (full size) into mip level
 * N+1 (half size) of the same image, one level pair at a time.
 *
 * `src_image` must be in SPUDGPU_IMAGE_LAYOUT_TRANSFER_SRC and `dst_image` in
 * SPUDGPU_IMAGE_LAYOUT_TRANSFER_DST. They may be the same image.
 *
 * Maps to: vkCmdBlitImage (Vulkan). Neither D3D12 nor Metal exposes a direct
 * blit command with filtering equivalent to Vulkan's.
 *
 * @note Not yet implemented by the Metal backend.
 *
 * @param[in] cmd       Command list being recorded.
 * @param[in] src_image Image to read from.
 * @param[in] dst_image Image to write into.
 * @param[in] desc      Source and destination subresources, regions and
 *                      filter.
 */
void spudgpu_cmd_blit_image(
    spudgpu_command_list cmd,
    spudgpu_image src_image,
    spudgpu_image dst_image,
    const spudgpu_image_blit_desc *desc);

// ============================================================================
//  Rendering
// ============================================================================

/**
 * @brief What happens to an attachment's contents when a rendering pass
 * begins.
 */
typedef uint32_t SPUDGPU_LOAD_OP;
enum {
	/** Keep the attachment's existing contents. */
	SPUDGPU_LOAD_OP_LOAD = 0,
	/** Clear to the attachment's clear value before drawing. */
	SPUDGPU_LOAD_OP_CLEAR,
	/** Contents are undefined; the driver may skip the load entirely. */
	SPUDGPU_LOAD_OP_DONT_CARE,
};

/**
 * @brief What happens to an attachment's contents when a rendering pass ends.
 */
typedef uint32_t SPUDGPU_STORE_OP;
enum {
	/** Write the render result back to the attachment. */
	SPUDGPU_STORE_OP_STORE = 0,
	/** Discard the result; the driver may skip the writeback. */
	SPUDGPU_STORE_OP_DONT_CARE,
};

/**
 * @brief One color attachment bound for a rendering pass.
 */
/* Frozen: an array element, held by value in spudgpu_rendering_begin_desc. It never changes; a new need gets a new type. */
typedef struct spudgpu_color_attachment_desc {
	/** The view to render into. */
	spudgpu_image_view image_view;

	/** @see SPUDGPU_LOAD_OP */
	SPUDGPU_LOAD_OP load_op;
	/** @see SPUDGPU_STORE_OP */
	SPUDGPU_STORE_OP store_op;

	/** RGBA clear color; only read when load_op == SPUDGPU_LOAD_OP_CLEAR. */
	float clear_color[4];
} spudgpu_color_attachment_desc;

/**
 * @brief The depth/stencil attachment bound for a rendering pass.
 */
/* Frozen: held by value in spudgpu_rendering_begin_desc. It never changes; a new need gets a new type. */
typedef struct spudgpu_depth_attachment_desc {
	/** NULL for no depth/stencil attachment. */
	spudgpu_image_view image_view;

	/** What happens to the depth values when the pass begins. */
	SPUDGPU_LOAD_OP depth_load_op;
	/** What happens to the depth values when the pass ends. */
	SPUDGPU_STORE_OP depth_store_op;
	/** What happens to the stencil values when the pass begins. Ignored
	 *  when the view's format has no stencil. */
	SPUDGPU_LOAD_OP stencil_load_op;
	/** What happens to the stencil values when the pass ends. Ignored when
	 *  the view's format has no stencil. */
	SPUDGPU_STORE_OP stencil_store_op;

	/** Only read when depth_load_op == SPUDGPU_LOAD_OP_CLEAR. */
	float clear_depth;
	/** Only read when stencil_load_op == SPUDGPU_LOAD_OP_CLEAR. */
	uint32_t clear_stencil;
} spudgpu_depth_attachment_desc;

/**
 * @brief Describes the attachments and render area for one rendering pass.
 *
 * Rendering is dynamic: there is no render pass or framebuffer object, and
 * a pipeline is not created against one. The attachments and their load and
 * store operations are given per call, here.
 *
 * Carries no pipeline reference — pipelines are bound mid-pass with
 * spudgpu_cmd_bind_pipeline like any other draw-time state, not tied to a
 * specific render pass object.
 *
 * Maps to `vkCmdBeginRendering` / `vkCmdEndRendering` (Vulkan 1.3, or
 * `VK_KHR_dynamic_rendering`), `ID3D12GraphicsCommandList4::BeginRenderPass`
 * / `EndRenderPass` (D3D12), and a render command encoder made from an
 * `MTLRenderPassDescriptor` (Metal).
 *
 * @todo A Vulkan device without Vulkan 1.3 or `VK_KHR_dynamic_rendering` is
 * not supported: there is no `VkRenderPass` / `VkFramebuffer` path.
 */
typedef struct spudgpu_rendering_begin_desc {
	/** `type` is SPUD_STRUCT_TYPE_GPU_RENDERING_BEGIN_DESC; `next` is NULL (nothing is chained onto
	 * this struct yet). Set by the caller. See SPUD_STRUCT_BASE. */
	SPUD_STRUCT_BASE base;
	/** The color attachments. Only the first #color_attachment_count are read. */
	spudgpu_color_attachment_desc color_attachments[SPUDGPU_MAX_COLOR_ATTACHMENTS];
	/** Number of valid entries in #color_attachments. */
	uint32_t color_attachment_count;

	/** depth_attachment.image_view == NULL means no depth/stencil attachment. */
	spudgpu_depth_attachment_desc depth_attachment;

	/**
	 * Left edge of the render area, in pixels. Not read by any backend
	 * yet: the render area starts at 0. Set to 0.
	 */
	int32_t x;
	/**
	 * Top edge of the render area, in pixels. Not read by any backend yet:
	 * the render area starts at 0. Set to 0.
	 */
	int32_t y;

	/** Render area width in pixels. Usually the attachments' width. */
	uint32_t width;

	/** Render area height in pixels. Usually the attachments' height. */
	uint32_t height;

	/**
	 * Number of layers the pass renders to, or 0 for a pass that is not
	 * layered.
	 *
	 * These rules hold on every backend, whatever the graphics API
	 * underneath would also accept:
	 *
	 * - 0: every attachment's view covers one array layer, and every
	 *   primitive is drawn to it. What a shader's layer output does is
	 *   undefined.
	 * - Not 0: SPUDGPU_DEVICE_PROPERTIES::layered_rendering is true for the
	 *   device, and every attachment's view, depth/stencil included, is
	 *   SPUDGPU_IMAGE_VIEW_TYPE_2D_ARRAY over one mip level and exactly
	 *   this many array layers. 1 is a layered pass of one layer, not the
	 *   same as 0.
	 * - In a layered pass the vertex shader (or the mesh shader, per
	 *   primitive) writes the layer a primitive is drawn to, counted from
	 *   the view's first layer. A primitive whose layer is not written is
	 *   drawn to layer 0; one written as this count or more is undefined.
	 *   No other stage chooses the layer.
	 * - A load operation of SPUDGPU_LOAD_OP_CLEAR clears every layer, and
	 *   the store operation applies to every layer.
	 * - The image layout each attachment must be in applies to every layer
	 *   its view covers.
	 *
	 * Maps to `VkRenderingInfo::layerCount` (1 for a pass that is not
	 * layered) and the `Layer` built-in (Vulkan), the array size of the
	 * render target and depth stencil views and
	 * `SV_RenderTargetArrayIndex` (D3D12), and
	 * `MTLRenderPassDescriptor.renderTargetArrayLength` and
	 * `[[render_target_array_index]]` (Metal).
	 */
	uint32_t layer_count;

	/**
	 * Set true only if a bundle (SPUDGPU_COMMAND_LIST_TYPE_BUNDLE, see
	 * SPUDGPU_EXT_BUNDLES) will be executed via spudgpu_cmd_execute_bundle
	 * inside this rendering pass. Required on Vulkan, where secondary
	 * command buffers may only be recorded into a scope that was opened
	 * with VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT — SpudLib
	 * never infers this from whether a bundle happens to get executed
	 * later, since the caller always knows this decision up front and
	 * SpudLib does not guess on the caller's behalf. No-op on backends
	 * without a bundle/secondary-command-buffer distinction.
	 */
	bool will_execute_bundles;

	/**
	 * A timestamp set the pass writes two timestamps into: one as its
	 * first work starts and one as its last work ends, or NULL for none.
	 * The set was created for the queue the command list is submitted on,
	 * and both entries were reset since they were last written
	 * (spudgpu_cmd_reset_timestamps()). See "Timestamps".
	 */
	spudgpu_timestamp_set timestamp_set;
	/** The entry written as the pass starts. Read only with #timestamp_set. */
	uint32_t timestamp_begin_index;
	/**
	 * The entry written as the pass ends, at spudgpu_cmd_end_rendering().
	 * Not #timestamp_begin_index. Read only with #timestamp_set.
	 */
	uint32_t timestamp_end_index;
} spudgpu_rendering_begin_desc;

/**
 * @brief Begins a rendering pass, binding the color and optional
 * depth/stencil attachments and issuing their load operations.
 *
 * Must be matched with spudgpu_cmd_end_rendering() before submitting. Bind a
 * pipeline with spudgpu_cmd_bind_pipeline() after this call, not before.
 *
 * Nothing is recorded if `desc->base` is not {SPUD_STRUCT_TYPE_GPU_RENDERING_BEGIN_DESC, NULL}
 * (see SPUD_STRUCT_BASE).
 *
 * @param[in] cmd  Command list being recorded.
 * @param[in] desc Attachments and render area.
 */
void spudgpu_cmd_begin_rendering(
    spudgpu_command_list cmd,
    const spudgpu_rendering_begin_desc *desc);

/**
 * @brief Ends the current rendering pass, issuing the attachments' store
 * operations.
 *
 * @param[in] cmd Command list being recorded.
 */
void spudgpu_cmd_end_rendering(spudgpu_command_list cmd);

/**
 * @brief Clears a single color attachment to a solid color.
 *
 * Wraps spudgpu_cmd_begin_rendering() / spudgpu_cmd_end_rendering() with
 * load_op == CLEAR and no draws; no pipeline is required. The attachment must
 * already be in SPUDGPU_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL (see
 * spudgpu_cmd_image_barrier()).
 *
 * @param[in] cmd        Command list being recorded.
 * @param[in] attachment View to clear.
 * @param[in] r          Red component of the clear color.
 * @param[in] g          Green component of the clear color.
 * @param[in] b          Blue component of the clear color.
 * @param[in] a          Alpha component of the clear color.
 * @param[in] width      Attachment width in pixels.
 * @param[in] height     Attachment height in pixels.
 */
void spudgpu_cmd_clear_color_attachment(
    spudgpu_command_list cmd,
    spudgpu_image_view attachment,
    float r,
    float g,
    float b,
    float a,
    uint32_t width,
    uint32_t height);

/**
 * @brief Clears a depth/stencil attachment.
 *
 * Wraps spudgpu_cmd_begin_rendering() / spudgpu_cmd_end_rendering() with no
 * draws; no pipeline is required. The attachment must already be in
 * SPUDGPU_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL (see
 * spudgpu_cmd_image_barrier()).
 *
 * @param[in] cmd           Command list being recorded.
 * @param[in] attachment    Depth/stencil view to clear.
 * @param[in] clear_depth   Whether to clear the depth aspect.
 * @param[in] clear_stencil Whether to clear the stencil aspect.
 * @param[in] depth         Depth clear value (typically 1.0f).
 * @param[in] stencil       Stencil clear value (typically 0).
 * @param[in] width         Attachment width in pixels.
 * @param[in] height        Attachment height in pixels.
 */
void spudgpu_cmd_clear_depth_attachment(
    spudgpu_command_list cmd,
    spudgpu_image_view attachment,
    bool clear_depth,
    bool clear_stencil,
    float depth,
    uint32_t stencil,
    uint32_t width,
    uint32_t height);

// ============================================================================
//  Bundles
// ============================================================================

/**
 * @brief 1 when the compiled-in backend implements bundles and this section
 * is declared, 0 when it is compiled out.
 *
 * A bundle is a command list made from a SPUDGPU_COMMAND_LIST_TYPE_BUNDLE
 * allocator (spudgpu_command_allocator_desc::type). It records a fixed
 * sequence of pipeline, descriptor and draw state once, and is replayed
 * unchanged from inside a spudgpu_cmd_begin_rendering() /
 * spudgpu_cmd_end_rendering() scope of a direct command list, as often as
 * wanted, with nothing recorded again. It draws into the attachments of the
 * direct list's pass and never begins or ends rendering itself.
 *
 * Two rules hold on every backend, whatever the graphics API inherits:
 *
 * - A bundle draws with one viewport and one scissor rectangle, named when
 *   it is recorded (spudgpu_bundle_inheritance_desc), and the direct list
 *   has set the same two, at index 0, when it executes the bundle. A bundle
 *   never calls spudgpu_cmd_set_viewports() or
 *   spudgpu_cmd_set_scissor_rects(): D3D12 refuses both in a bundle and
 *   takes them from the direct list; a Vulkan secondary command buffer
 *   inherits neither and is given them when its recording begins.
 * - After spudgpu_cmd_execute_bundle(), the direct list's pipeline, vertex
 *   and index buffers, descriptor sets and push constants are undefined
 *   until set again: D3D12 leaves the bundle's in place and Vulkan leaves
 *   none.
 *
 * Maps to `ID3D12GraphicsCommandList::ExecuteBundle` (D3D12) and to
 * `vkCmdExecuteCommands` over a secondary command buffer (Vulkan).
 *
 * The macro is 0 on Metal, which has nothing to record once and replay: a
 * command buffer and its encoders are used once. That is a gap in what the
 * API can do, so the section is compiled out there and not emulated.
 */
#if SPUDGPU_COMPILE_VULKAN_API || SPUDGPU_COMPILE_D3D12_API
#define SPUDGPU_EXT_BUNDLES 1
#else
#define SPUDGPU_EXT_BUNDLES 0
#endif

// ============================================================================
//  Debug labels
// ============================================================================

/**
 * @brief Opens a labelled region of a command list, for a capture or
 * profiling tool to show.
 *
 * A label changes nothing the GPU does. Where no tool reads it, or the
 * graphics API offers no way to record it, the call does nothing.
 *
 * Two rules hold on every backend:
 *
 * - Regions nest. spudgpu_cmd_end_debug_label() closes the region opened
 *   last, and every region opened in a command list is closed in it.
 * - A region opened inside a rendering pass closes inside the same pass,
 *   and one opened outside every pass closes outside every pass, with any
 *   pass inside it wholly inside it.
 *
 * Vulkan: `vkCmdBeginDebugUtilsLabelEXT`, when the instance has
 * `VK_EXT_debug_utils` (spudgpu_create_instance() enables it wherever the
 * Vulkan loader offers it). D3D12: `BeginEvent` in PIX's ANSI encoding.
 * Metal: `pushDebugGroup:` on the render encoder inside a pass and on the
 * command buffer outside one.
 *
 * @param[in] cmd   Command list being recorded.
 * @param[in] label Null-terminated text. Only read during the call. NULL is
 *                  taken as an empty label, so the region still opens.
 */
void spudgpu_cmd_begin_debug_label(
    spudgpu_command_list cmd,
    const char *label);

/**
 * @brief Closes the region spudgpu_cmd_begin_debug_label() opened last.
 *
 * @param[in] cmd Command list being recorded.
 */
void spudgpu_cmd_end_debug_label(spudgpu_command_list cmd);

// ============================================================================
//  Timestamps
// ============================================================================

/**
 * @brief Creates a set of GPU timestamps, written as rendering passes start
 * and end.
 *
 * A timestamp is written only at the start and the end of a rendering pass
 * (spudgpu_rendering_begin_desc::timestamp_set), because that is what every
 * backend can do: Apple GPUs sample their counters at the boundaries of a
 * pass's stages and nowhere in between. One entry is one timestamp. The
 * entries are used in this order on every backend:
 *
 * 1. spudgpu_cmd_reset_timestamps(), outside every pass, before a pass
 *    writes them.
 * 2. A pass writes them.
 * 3. spudgpu_cmd_resolve_timestamps(), outside every pass, after the pass,
 *    in the same command list or a later one on the same queue.
 * 4. Once the submission that resolved them has finished, which is the
 *    caller's to know (a fence), spudgpu_get_timestamps() reads them.
 *
 * An entry read without having been through those steps since it was last
 * reset holds an undefined value.
 *
 * Vulkan: a `VK_QUERY_TYPE_TIMESTAMP` query pool. D3D12: a timestamp query
 * heap and a readback buffer the resolve copies into. Metal: an
 * `MTLCounterSampleBuffer` of the timestamp counter set, sampled at stage
 * boundaries.
 *
 * @param[in]  device  Device to create the set on.
 * @param[in]  queue   The queue the command lists that write and resolve
 *                     the set are submitted on. D3D12 counts time per
 *                     queue.
 * @param[in]  count   Number of entries. Not 0.
 * @param[out] out_set Receives the new set on success.
 *
 * @retval SPUD_SUCCESS The set was created.
 * @retval SPUDRESULT_GPU_INVALID_DEVICE `device` is NULL.
 * @retval SPUDRESULT_GPU_INVALID_COMMAND_QUEUE `queue` is NULL.
 * @retval SPUDRESULT_ZERO_SIZE `count` is 0.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_set` is NULL.
 * @retval SPUDRESULT_GPU_TIMESTAMPS_NOT_SUPPORTED The device can't write
 *         timestamps at a pass's boundaries
 *         (SPUDGPU_DEVICE_PROPERTIES::pass_timestamps).
 * @return Another SPUDRESULT if the backend fails, such as
 *         SPUDRESULT_OUT_OF_MEMORY or SPUDRESULT_API_SPECIFIC_FAILURE.
 */
SPUDRESULT spudgpu_create_timestamp_set(
    spudgpu_device device,
    spudgpu_command_queue queue,
    uint32_t count,
    spudgpu_timestamp_set *out_set);

/**
 * @brief Destroys a timestamp set.
 *
 * @warning Command lists that write or resolve it must have finished
 * executing.
 *
 * @param[in] set Set to destroy. NULL does nothing.
 */
void spudgpu_destroy_timestamp_set(spudgpu_timestamp_set set);

/**
 * @brief Records the reset of entries of a timestamp set, before a pass
 * writes them.
 *
 * Outside every rendering pass. Vulkan: `vkCmdResetQueryPool`. D3D12 and
 * Metal have nothing to reset, and record nothing.
 *
 * @param[in] cmd   Command list being recorded.
 * @param[in] set   Timestamp set.
 * @param[in] first First entry.
 * @param[in] count Number of entries, all below the set's count.
 */
void spudgpu_cmd_reset_timestamps(
    spudgpu_command_list cmd,
    spudgpu_timestamp_set set,
    uint32_t first,
    uint32_t count);

/**
 * @brief Records what makes written entries of a timestamp set readable by
 * spudgpu_get_timestamps().
 *
 * Outside every rendering pass, after the passes that wrote them. D3D12:
 * `ResolveQueryData` into the set's readback buffer. Vulkan and Metal read
 * the results where they were written, and record nothing.
 *
 * @param[in] cmd   Command list being recorded.
 * @param[in] set   Timestamp set.
 * @param[in] first First entry.
 * @param[in] count Number of entries, all below the set's count.
 */
void spudgpu_cmd_resolve_timestamps(
    spudgpu_command_list cmd,
    spudgpu_timestamp_set set,
    uint32_t first,
    uint32_t count);

/**
 * @brief Reads resolved entries of a timestamp set, in nanoseconds.
 *
 * Call once the submission that resolved them has finished. The values are
 * nanoseconds from an origin of the backend's: only the difference between
 * two entries of one set means anything, such as a pass's end less its
 * start. Vulkan: the ticks times `VkPhysicalDeviceLimits::timestampPeriod`.
 * D3D12: the ticks over the queue's `GetTimestampFrequency`. Metal: the
 * GPU's ticks set against the CPU's clock with `sampleTimestamps:` at the
 * set's creation and at this call, as Apple describes for converting GPU
 * timestamps.
 *
 * @param[in]  set     Timestamp set.
 * @param[in]  first   First entry.
 * @param[in]  count   Number of entries, all below the set's count.
 * @param[out] out_ns  Receives `count` values.
 *
 * @retval SPUD_SUCCESS `out_ns` was written.
 * @retval SPUDRESULT_GPU_INVALID_TIMESTAMP_SET `set` is NULL.
 * @retval SPUDRESULT_ZERO_SIZE `count` is 0.
 * @retval SPUDRESULT_INDEX_OUT_OF_RANGE An entry is at or past the set's
 *         count.
 * @retval SPUDRESULT_NULL_OUTPUT_PARAMETER `out_ns` is NULL.
 * @return Another SPUDRESULT if the backend fails, such as
 *         SPUDRESULT_API_SPECIFIC_FAILURE.
 */
SPUDRESULT spudgpu_get_timestamps(
    spudgpu_timestamp_set set,
    uint32_t first,
    uint32_t count,
    uint64_t *out_ns);

#if SPUDGPU_EXT_BUNDLES

/**
 * @brief Describes the attachments, viewport and scissor rectangle a bundle
 * will be replayed under.
 *
 * Required up front on Vulkan, where a bundle is a secondary command buffer
 * and VK_KHR_dynamic_rendering requires attachment formats at recording
 * time via VkCommandBufferInheritanceRenderingInfo — there is no
 * attachment/framebuffer object to inherit them from implicitly — and where
 * a secondary command buffer inherits no viewport or scissor. Mirrors
 * the attachment format fields on spudgpu_shader_pipeline_desc; must match
 * whatever the direct command list's spudgpu_cmd_begin_rendering call
 * actually binds, and the viewport and scissor it has set, when the bundle
 * is executed.
 */
typedef struct spudgpu_bundle_inheritance_desc {
	/** `type` is SPUD_STRUCT_TYPE_GPU_BUNDLE_INHERITANCE_DESC; `next` is NULL (nothing is chained onto
	 * this struct yet). Set by the caller. See SPUD_STRUCT_BASE. */
	SPUD_STRUCT_BASE base;
	/**
	 * The pass's color attachment formats, in its order. Only the first
	 * #color_attachment_count are read.
	 * @see SPUDGPU_FORMAT
	 */
	SPUDGPU_FORMAT color_attachment_formats[SPUDGPU_MAX_COLOR_ATTACHMENTS];
	/** Number of valid entries in #color_attachment_formats. */
	uint32_t color_attachment_count;

	/**
	 * SPUDGPU_FORMAT_UNKNOWN for no depth/stencil attachment.
	 * @see SPUDGPU_FORMAT
	 */
	SPUDGPU_FORMAT depth_format;

	/**
	 * The viewport the bundle's draws use: the one the direct list has set
	 * at index 0 when it executes the bundle. Vulkan records it into the
	 * bundle; D3D12 takes it from the direct list.
	 */
	SPUDGPU_VIEWPORT viewport;
	/**
	 * The scissor rectangle the bundle's draws use, with the same rule as
	 * #viewport.
	 */
	SPUDGPU_SCISSOR_RECT scissor_rect;
} spudgpu_bundle_inheritance_desc;

/**
 * @brief Begins recording a bundle command list (one created against a
 * SPUDGPU_COMMAND_LIST_TYPE_BUNDLE allocator).
 *
 * Use this instead of spudgpu_begin_command_list() for a bundle. It is the
 * same operation on D3D12 (`desc` is ignored there; a bundle needs no
 * attachment info up front and takes its viewport and scissor from the
 * direct list), but Vulkan needs `desc` to fill in a secondary command
 * buffer's inheritance info and to set its viewport and scissor. Close the
 * bundle with the shared spudgpu_end_command_list(); there is no separate
 * "end bundle" call.
 *
 * Nothing is recorded if `desc->base` is not {SPUD_STRUCT_TYPE_GPU_BUNDLE_INHERITANCE_DESC, NULL}
 * (see SPUD_STRUCT_BASE).
 *
 * @param[in] bundle Command list created against an allocator of type
 *                   SPUDGPU_COMMAND_LIST_TYPE_BUNDLE.
 * @param[in] desc   Attachment formats, viewport and scissor rectangle the
 *                   bundle will be executed under.
 */
void spudgpu_begin_bundle_command_list(
    spudgpu_command_list bundle,
    const spudgpu_bundle_inheritance_desc *desc);

/**
 * @brief Replays a recorded, closed bundle into a direct command list that
 * is recording.
 *
 * Must be called between spudgpu_cmd_begin_rendering() (with
 * will_execute_bundles set true) and spudgpu_cmd_end_rendering() on `cmd`,
 * with attachment formats matching the bundle's
 * spudgpu_bundle_inheritance_desc, and with its viewport and scissor
 * rectangle set at index 0. Afterwards, the direct list's pipeline, vertex
 * and index buffers, descriptor sets and push constants are undefined until
 * set again. The bundle must already be closed
 * (spudgpu_end_command_list() called on it) and must not be re-recorded while
 * a submission that references it is in flight.
 *
 * Maps to: ID3D12GraphicsCommandList::ExecuteBundle (D3D12),
 * vkCmdExecuteCommands (Vulkan).
 *
 * @param[in] cmd    Direct command list being recorded.
 * @param[in] bundle Closed bundle to replay.
 */
void spudgpu_cmd_execute_bundle(
    spudgpu_command_list cmd,
    spudgpu_command_list bundle);

#endif // SPUDGPU_EXT_BUNDLES

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // SPUDLIB_SPUDGPU_H
