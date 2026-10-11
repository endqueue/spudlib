
#if SPUDGPU_COMPILE_VULKAN_API

#include <stdio.h>
#include <string.h>

#include "spudgpuvulkan.h"
#include "stdlib.h"
#include "stdbool.h"

VkFormat convert_spud_to_vulkan_format(SPUDGPU_FORMAT format) {
    switch (format) {
        // ── 128-bit ──────────────────────────────────────────────────────────
        case SPUDGPU_FORMAT_R32G32B32A32_TYPELESS: return VK_FORMAT_R32G32B32A32_SFLOAT;
        // no typeless in Vulkan; treat as float
        case SPUDGPU_FORMAT_R32G32B32A32_FLOAT: return VK_FORMAT_R32G32B32A32_SFLOAT;
        case SPUDGPU_FORMAT_R32G32B32A32_UINT: return VK_FORMAT_R32G32B32A32_UINT;
        case SPUDGPU_FORMAT_R32G32B32A32_SINT: return VK_FORMAT_R32G32B32A32_SINT;

        // ── 96-bit ───────────────────────────────────────────────────────────
        case SPUDGPU_FORMAT_R32G32B32_TYPELESS: return VK_FORMAT_R32G32B32_SFLOAT;
        case SPUDGPU_FORMAT_R32G32B32_FLOAT: return VK_FORMAT_R32G32B32_SFLOAT;
        case SPUDGPU_FORMAT_R32G32B32_UINT: return VK_FORMAT_R32G32B32_UINT;
        case SPUDGPU_FORMAT_R32G32B32_SINT: return VK_FORMAT_R32G32B32_SINT;

        // ── 64-bit ───────────────────────────────────────────────────────────
        case SPUDGPU_FORMAT_R16G16B16A16_TYPELESS: return VK_FORMAT_R16G16B16A16_SFLOAT;
        case SPUDGPU_FORMAT_R16G16B16A16_FLOAT: return VK_FORMAT_R16G16B16A16_SFLOAT;
        case SPUDGPU_FORMAT_R16G16B16A16_UNORM: return VK_FORMAT_R16G16B16A16_UNORM;
        case SPUDGPU_FORMAT_R16G16B16A16_UINT: return VK_FORMAT_R16G16B16A16_UINT;
        case SPUDGPU_FORMAT_R16G16B16A16_SNORM: return VK_FORMAT_R16G16B16A16_SNORM;
        case SPUDGPU_FORMAT_R16G16B16A16_SINT: return VK_FORMAT_R16G16B16A16_SINT;

        case SPUDGPU_FORMAT_R32G32_TYPELESS: return VK_FORMAT_R32G32_SFLOAT;
        case SPUDGPU_FORMAT_R32G32_FLOAT: return VK_FORMAT_R32G32_SFLOAT;
        case SPUDGPU_FORMAT_R32G32_UINT: return VK_FORMAT_R32G32_UINT;
        case SPUDGPU_FORMAT_R32G32_SINT: return VK_FORMAT_R32G32_SINT;

        // ── Depth/stencil packed (64-bit) ────────────────────────────────────
        case SPUDGPU_FORMAT_R32G8X24_TYPELESS: return VK_FORMAT_D32_SFLOAT_S8_UINT;
        case SPUDGPU_FORMAT_D32_FLOAT_S8X24_UINT: return VK_FORMAT_D32_SFLOAT_S8_UINT;
        case SPUDGPU_FORMAT_R32_FLOAT_X8X24_TYPELESS: return VK_FORMAT_D32_SFLOAT_S8_UINT;
        // no direct equivalent; sampled depth view
        case SPUDGPU_FORMAT_X32_TYPELESS_G8X24_UINT: return VK_FORMAT_D32_SFLOAT_S8_UINT; // sampled stencil view

        // ── 32-bit ───────────────────────────────────────────────────────────
        case SPUDGPU_FORMAT_R10G10B10A2_TYPELESS: return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
        case SPUDGPU_FORMAT_R10G10B10A2_UNORM: return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
        case SPUDGPU_FORMAT_R10G10B10A2_UINT: return VK_FORMAT_A2B10G10R10_UINT_PACK32;
        case SPUDGPU_FORMAT_R11G11B10_FLOAT: return VK_FORMAT_B10G11R11_UFLOAT_PACK32;

        case SPUDGPU_FORMAT_R8G8B8A8_TYPELESS: return VK_FORMAT_R8G8B8A8_UNORM;
        case SPUDGPU_FORMAT_R8G8B8A8_UNORM: return VK_FORMAT_R8G8B8A8_UNORM;
        case SPUDGPU_FORMAT_R8G8B8A8_UNORM_SRGB: return VK_FORMAT_R8G8B8A8_SRGB;
        case SPUDGPU_FORMAT_R8G8B8A8_UINT: return VK_FORMAT_R8G8B8A8_UINT;
        case SPUDGPU_FORMAT_R8G8B8A8_SNORM: return VK_FORMAT_R8G8B8A8_SNORM;
        case SPUDGPU_FORMAT_R8G8B8A8_SINT: return VK_FORMAT_R8G8B8A8_SINT;

        case SPUDGPU_FORMAT_R16G16_TYPELESS: return VK_FORMAT_R16G16_SFLOAT;
        case SPUDGPU_FORMAT_R16G16_FLOAT: return VK_FORMAT_R16G16_SFLOAT;
        case SPUDGPU_FORMAT_R16G16_UNORM: return VK_FORMAT_R16G16_UNORM;
        case SPUDGPU_FORMAT_R16G16_UINT: return VK_FORMAT_R16G16_UINT;
        case SPUDGPU_FORMAT_R16G16_SNORM: return VK_FORMAT_R16G16_SNORM;
        case SPUDGPU_FORMAT_R16G16_SINT: return VK_FORMAT_R16G16_SINT;

        case SPUDGPU_FORMAT_R32_TYPELESS: return VK_FORMAT_R32_SFLOAT;
        case SPUDGPU_FORMAT_D32_FLOAT: return VK_FORMAT_D32_SFLOAT;
        case SPUDGPU_FORMAT_R32_FLOAT: return VK_FORMAT_R32_SFLOAT;
        case SPUDGPU_FORMAT_R32_UINT: return VK_FORMAT_R32_UINT;
        case SPUDGPU_FORMAT_R32_SINT: return VK_FORMAT_R32_SINT;

        // ── Depth/stencil packed (32-bit) ────────────────────────────────────
        case SPUDGPU_FORMAT_R24G8_TYPELESS: return VK_FORMAT_D24_UNORM_S8_UINT;
        case SPUDGPU_FORMAT_D24_UNORM_S8_UINT: return VK_FORMAT_D24_UNORM_S8_UINT;
        case SPUDGPU_FORMAT_R24_UNORM_X8_TYPELESS: return VK_FORMAT_D24_UNORM_S8_UINT;
        case SPUDGPU_FORMAT_X24_TYPELESS_G8_UINT: return VK_FORMAT_D24_UNORM_S8_UINT;

        // ── 16-bit ───────────────────────────────────────────────────────────
        case SPUDGPU_FORMAT_R8G8_TYPELESS: return VK_FORMAT_R8G8_UNORM;
        case SPUDGPU_FORMAT_R8G8_UNORM: return VK_FORMAT_R8G8_UNORM;
        case SPUDGPU_FORMAT_R8G8_UINT: return VK_FORMAT_R8G8_UINT;
        case SPUDGPU_FORMAT_R8G8_SNORM: return VK_FORMAT_R8G8_SNORM;
        case SPUDGPU_FORMAT_R8G8_SINT: return VK_FORMAT_R8G8_SINT;

        case SPUDGPU_FORMAT_R16_TYPELESS: return VK_FORMAT_R16_SFLOAT;
        case SPUDGPU_FORMAT_R16_FLOAT: return VK_FORMAT_R16_SFLOAT;
        case SPUDGPU_FORMAT_D16_UNORM: return VK_FORMAT_D16_UNORM;
        case SPUDGPU_FORMAT_R16_UNORM: return VK_FORMAT_R16_UNORM;
        case SPUDGPU_FORMAT_R16_UINT: return VK_FORMAT_R16_UINT;
        case SPUDGPU_FORMAT_R16_SNORM: return VK_FORMAT_R16_SNORM;
        case SPUDGPU_FORMAT_R16_SINT: return VK_FORMAT_R16_SINT;

        // ── 8-bit ────────────────────────────────────────────────────────────
        case SPUDGPU_FORMAT_R8_TYPELESS: return VK_FORMAT_R8_UNORM;
        case SPUDGPU_FORMAT_R8_UNORM: return VK_FORMAT_R8_UNORM;
        case SPUDGPU_FORMAT_R8_UINT: return VK_FORMAT_R8_UINT;
        case SPUDGPU_FORMAT_R8_SNORM: return VK_FORMAT_R8_SNORM;
        case SPUDGPU_FORMAT_R8_SINT: return VK_FORMAT_R8_SINT;
        case SPUDGPU_FORMAT_A8_UNORM: return VK_FORMAT_R8_UNORM;
        // Vulkan has no dedicated A8; use R8 and swizzle in the image view

        // ── Exotic / packed ──────────────────────────────────────────────────
        case SPUDGPU_FORMAT_R1_UNORM: return VK_FORMAT_UNDEFINED; // no Vulkan equivalent
        case SPUDGPU_FORMAT_R9G9B9E5_SHAREDEXP: return VK_FORMAT_E5B9G9R9_UFLOAT_PACK32;
        case SPUDGPU_FORMAT_R8G8_B8G8_UNORM: return VK_FORMAT_B8G8R8G8_422_UNORM; // DXGI packed 4:2:2
        case SPUDGPU_FORMAT_G8R8_G8B8_UNORM: return VK_FORMAT_G8B8G8R8_422_UNORM;

        // ── BGR / swapchain formats ───────────────────────────────────────────
        case SPUDGPU_FORMAT_B5G6R5_UNORM: return VK_FORMAT_B5G6R5_UNORM_PACK16;
        case SPUDGPU_FORMAT_B5G5R5A1_UNORM: return VK_FORMAT_B5G5R5A1_UNORM_PACK16;
        case SPUDGPU_FORMAT_B8G8R8A8_UNORM: return VK_FORMAT_B8G8R8A8_UNORM;
        case SPUDGPU_FORMAT_B8G8R8X8_UNORM: return VK_FORMAT_B8G8R8A8_UNORM; // X = ignored alpha; same bits
        case SPUDGPU_FORMAT_B8G8R8A8_TYPELESS: return VK_FORMAT_B8G8R8A8_UNORM;
        case SPUDGPU_FORMAT_B8G8R8A8_UNORM_SRGB: return VK_FORMAT_B8G8R8A8_SRGB;
        case SPUDGPU_FORMAT_B8G8R8X8_TYPELESS: return VK_FORMAT_B8G8R8A8_UNORM;
        case SPUDGPU_FORMAT_B8G8R8X8_UNORM_SRGB: return VK_FORMAT_B8G8R8A8_SRGB;
        case SPUDGPU_FORMAT_B4G4R4A4_UNORM: return VK_FORMAT_B4G4R4A4_UNORM_PACK16;
        case SPUDGPU_FORMAT_R10G10B10_XR_BIAS_A2_UNORM: return VK_FORMAT_UNDEFINED;
        // XR bias is display-only; no Vulkan match

        // ── BC block compression ──────────────────────────────────────────────
        case SPUDGPU_FORMAT_BC1_TYPELESS: return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
        case SPUDGPU_FORMAT_BC1_UNORM: return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
        case SPUDGPU_FORMAT_BC1_UNORM_SRGB: return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
        case SPUDGPU_FORMAT_BC2_TYPELESS: return VK_FORMAT_BC2_UNORM_BLOCK;
        case SPUDGPU_FORMAT_BC2_UNORM: return VK_FORMAT_BC2_UNORM_BLOCK;
        case SPUDGPU_FORMAT_BC2_UNORM_SRGB: return VK_FORMAT_BC2_SRGB_BLOCK;
        case SPUDGPU_FORMAT_BC3_TYPELESS: return VK_FORMAT_BC3_UNORM_BLOCK;
        case SPUDGPU_FORMAT_BC3_UNORM: return VK_FORMAT_BC3_UNORM_BLOCK;
        case SPUDGPU_FORMAT_BC3_UNORM_SRGB: return VK_FORMAT_BC3_SRGB_BLOCK;
        case SPUDGPU_FORMAT_BC4_TYPELESS: return VK_FORMAT_BC4_UNORM_BLOCK;
        case SPUDGPU_FORMAT_BC4_UNORM: return VK_FORMAT_BC4_UNORM_BLOCK;
        case SPUDGPU_FORMAT_BC4_SNORM: return VK_FORMAT_BC4_SNORM_BLOCK;
        case SPUDGPU_FORMAT_BC5_TYPELESS: return VK_FORMAT_BC5_UNORM_BLOCK;
        case SPUDGPU_FORMAT_BC5_UNORM: return VK_FORMAT_BC5_UNORM_BLOCK;
        case SPUDGPU_FORMAT_BC5_SNORM: return VK_FORMAT_BC5_SNORM_BLOCK;
        case SPUDGPU_FORMAT_BC6H_TYPELESS: return VK_FORMAT_BC6H_UFLOAT_BLOCK;
        case SPUDGPU_FORMAT_BC6H_UF16: return VK_FORMAT_BC6H_UFLOAT_BLOCK;
        case SPUDGPU_FORMAT_BC6H_SF16: return VK_FORMAT_BC6H_SFLOAT_BLOCK;
        case SPUDGPU_FORMAT_BC7_TYPELESS: return VK_FORMAT_BC7_UNORM_BLOCK;
        case SPUDGPU_FORMAT_BC7_UNORM: return VK_FORMAT_BC7_UNORM_BLOCK;
        case SPUDGPU_FORMAT_BC7_UNORM_SRGB: return VK_FORMAT_BC7_SRGB_BLOCK;

        // ── YUV / video formats ───────────────────────────────────────────────
        case SPUDGPU_FORMAT_NV12: return VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
        case SPUDGPU_FORMAT_P010: return VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16;
        case SPUDGPU_FORMAT_P016: return VK_FORMAT_G16_B16R16_2PLANE_420_UNORM;
        case SPUDGPU_FORMAT_YUY2: return VK_FORMAT_G8B8G8R8_422_UNORM;
        case SPUDGPU_FORMAT_420_OPAQUE: return VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM;
        case SPUDGPU_FORMAT_Y210: return VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16;
        case SPUDGPU_FORMAT_Y216: return VK_FORMAT_G16B16G16R16_422_UNORM;
        case SPUDGPU_FORMAT_P208: return VK_FORMAT_G8_B8R8_2PLANE_422_UNORM;
        // AYUV/Y410/Y416/NV11/AI44/IA44/P8/A8P8/V208/V408 have no standard
        // Vulkan equivalent (packed 4:4:4 YUVA, 4:1:1 chroma subsampling, or
        // legacy 8-bit palette formats aren't representable by any core or
        // KHR_sampler_ycbcr_conversion VkFormat) — explicitly UNDEFINED
        // rather than falling through, so the gap is visible here rather
        // than being indistinguishable from a genuinely unmapped value.
        case SPUDGPU_FORMAT_AYUV:
        case SPUDGPU_FORMAT_Y410:
        case SPUDGPU_FORMAT_Y416:
        case SPUDGPU_FORMAT_NV11:
        case SPUDGPU_FORMAT_AI44:
        case SPUDGPU_FORMAT_IA44:
        case SPUDGPU_FORMAT_P8:
        case SPUDGPU_FORMAT_A8P8:
        case SPUDGPU_FORMAT_V208:
        case SPUDGPU_FORMAT_V408:
            return VK_FORMAT_UNDEFINED;

        // SPUDGPU_FORMAT_UNKNOWN (0) and SPUDGPU_FORMAT_FORCE_UINT (sentinel,
        // not a real format) fall through to here along with any value that
        // isn't a defined SPUDGPU_FORMAT at all.
        default: return VK_FORMAT_UNDEFINED;
    }
}


// Whether a vertex shader may write the Layer built-in on this device
// (SPUDGPU_DEVICE_PROPERTIES::layered_rendering). Asked twice: once to
// enable the feature when the logical device is created, once to report it.
static bool spudgpuvulkan___internal_shader_output_layer(VkPhysicalDevice physicalDevice) {
    VkPhysicalDeviceVulkan12Features available = {0};
    available.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    VkPhysicalDeviceFeatures2 features2 = {0};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &available;
    vkGetPhysicalDeviceFeatures2(physicalDevice, &features2);
    return available.shaderOutputLayer == VK_TRUE;
}

static VkDevice spudgpuvulkan___initialize_vk_logical_device_internal(
    spudgpu_device_vulkan *device) {
    VkDevice result = VK_NULL_HANDLE;

    VkPhysicalDevice physicalDevice = device->_physical_device_vk;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, NULL);

    //std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    VkQueueFamilyProperties *queueFamilies = calloc(queueFamilyCount, sizeof(VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies);

    // Set params to create (1) command queue with type graphics.
    // TODO: Later we will implement more types of command queues.
    uint32_t queueCount = 1;
    VkDeviceQueueCreateInfo queueCreateInfo = {0};
    int graphicsQueueFamilyIndex = -1;
    {
        for (uint32_t i = 0; i < queueFamilyCount; i++) {
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                graphicsQueueFamilyIndex = i;
                break;
            }
        }

        device->_graphics_queue_family_index = (uint32_t) graphicsQueueFamilyIndex;

        float graphicsQueuePriority = 1.0f;

        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.pNext = NULL;
        queueCreateInfo.queueFamilyIndex = graphicsQueueFamilyIndex;
        queueCreateInfo.queueCount = queueCount;
        queueCreateInfo.pQueuePriorities = &graphicsQueuePriority;
    }

    VkPhysicalDeviceFeatures deviceFeatures = {0};

    // depthBounds is a core-1.0, optional VkPhysicalDeviceFeatures bit (no
    // extension, unlike VK_EXT_mesh_shader below) - query it directly rather
    // than assuming, and only request it if the hardware actually reports
    // it. device->_features_vk (populated by the caller after this function
    // returns) can't be used here since it doesn't exist yet at this point.
    VkPhysicalDeviceFeatures availableFeatures = {0};
    vkGetPhysicalDeviceFeatures(physicalDevice, &availableFeatures);
    deviceFeatures.depthBounds = availableFeatures.depthBounds;
    device->_depth_bounds_test_supported = availableFeatures.depthBounds == VK_TRUE;

    // spudgpu_create_buffer/spudgpu_create_image always call
    // vkGetBufferDeviceAddress/equivalent to populate desc.gpu_address_location
    // (see spudgpu.h's documented contract that it's valid after creation,
    // matching D3D12's always-available GetGPUVirtualAddress), so this
    // feature must be enabled here and VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
    // must be set on every buffer (see spudgpuvulkanbuffer.c).
    VkPhysicalDeviceVulkan12Features vk12Features = {0};
    vk12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    vk12Features.bufferDeviceAddress = VK_TRUE;
    // spudgpu_fence is a timeline semaphore (spudgpuvulkansync.c).
    vk12Features.timelineSemaphore = VK_TRUE;
    // Layered rendering with the layer chosen in the vertex shader. Not
    // every device has it, so it is enabled only where it is reported.
    vk12Features.shaderOutputLayer =
        spudgpuvulkan___internal_shader_output_layer(physicalDevice) ? VK_TRUE : VK_FALSE;

    // Bindless / descriptor indexing (see spudgpuvulkandescriptors.c's
    // bindless section) — core in 1.2, no extension needed.
    vk12Features.descriptorIndexing = VK_TRUE;
    vk12Features.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    vk12Features.shaderStorageImageArrayNonUniformIndexing = VK_TRUE;
    vk12Features.shaderStorageBufferArrayNonUniformIndexing = VK_TRUE;
    vk12Features.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
    vk12Features.descriptorBindingStorageImageUpdateAfterBind = VK_TRUE;
    vk12Features.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;
    vk12Features.descriptorBindingPartiallyBound = VK_TRUE;
    vk12Features.descriptorBindingVariableDescriptorCount = VK_TRUE;
    vk12Features.runtimeDescriptorArray = VK_TRUE;

    VkPhysicalDeviceVulkan13Features vk13Features = {0};
    vk13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    vk13Features.pNext = &vk12Features;
    vk13Features.dynamicRendering = VK_TRUE;
    vk13Features.synchronization2 = VK_TRUE;

    // VK_EXT_mesh_shader is the first genuinely optional device extension
    // this backend enables - unlike VK_KHR_swapchain (always required), a
    // driver/hardware combination may not expose it at all, so its presence
    // must be probed for rather than assumed (see SPUDGPU_EXT_MESH_SHADING
    // in spudgpu.h). Enumerable-but-unsupported and never-enumerated are
    // both treated the same way here: mesh shading stays off.
    bool meshShaderExtensionAvailable = false;
    {
        uint32_t extensionCount = 0;
        vkEnumerateDeviceExtensionProperties(physicalDevice, NULL, &extensionCount, NULL);
        if (extensionCount > 0) {
            VkExtensionProperties *extensions = calloc(extensionCount, sizeof(VkExtensionProperties));
            vkEnumerateDeviceExtensionProperties(physicalDevice, NULL, &extensionCount, extensions);
            for (uint32_t i = 0; i < extensionCount; i++) {
                if (strcmp(extensions[i].extensionName, VK_EXT_MESH_SHADER_EXTENSION_NAME) == 0) {
                    meshShaderExtensionAvailable = true;
                    break;
                }
            }
            free(extensions);
        }
    }

    VkPhysicalDeviceMeshShaderFeaturesEXT meshShaderFeatures = {0};
    meshShaderFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;
    bool meshShaderFeatureSupported = false;
    if (meshShaderExtensionAvailable) {
        // The extension being enumerable doesn't by itself guarantee the
        // driver actually supports meshShader - query the real feature bit
        // via the same VkPhysicalDeviceFeatures2 chaining mechanism used
        // for Vulkan 1.2/1.3 features above.
        VkPhysicalDeviceFeatures2 features2 = {0};
        features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        features2.pNext = &meshShaderFeatures;
        vkGetPhysicalDeviceFeatures2(physicalDevice, &features2);
        meshShaderFeatureSupported = meshShaderFeatures.meshShader == VK_TRUE;
    }

    const char *deviceExtensions[2] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    uint32_t deviceExtensionCount = 1;
    if (meshShaderFeatureSupported) {
        deviceExtensions[deviceExtensionCount++] = VK_EXT_MESH_SHADER_EXTENSION_NAME;
        // meshShaderFeatures was just used as a vkGetPhysicalDeviceFeatures2
        // query target, so every bit in it (not just meshShader) now reflects
        // whatever the hardware happens to support - e.g. multiviewMeshShader
        // or primitiveFragmentShadingRateMeshShader can come back VK_TRUE even
        // though this backend never enables their prerequisite features
        // (multiview / primitiveFragmentShadingRate). Reset to a clean struct
        // before reusing it as the *enable* chain so only the bits actually
        // requested below get turned on. meshShader is the only bit this
        // sample family needs; taskShader stays off since nothing here uses
        // amplification shaders yet.
        meshShaderFeatures = (VkPhysicalDeviceMeshShaderFeaturesEXT){0};
        meshShaderFeatures.sType      = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;
        meshShaderFeatures.taskShader = VK_FALSE;
        meshShaderFeatures.meshShader = VK_TRUE;
        meshShaderFeatures.pNext      = vk13Features.pNext;
        vk13Features.pNext            = &meshShaderFeatures;
    }

    VkDeviceCreateInfo createInfo = {0};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pNext = &vk13Features;
    createInfo.pQueueCreateInfos = &queueCreateInfo;
    createInfo.queueCreateInfoCount = queueCount;
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = deviceExtensionCount;
    createInfo.ppEnabledExtensionNames = deviceExtensions;

    VkResult r = vkCreateDevice(physicalDevice, &createInfo, NULL, &result);
    if (r != VK_SUCCESS) {
        //throw std::runtime_error("failed to create logical device!");
        printf("failed to create logical device\n");
        free(queueFamilies);
        return NULL;
    }

    device->_mesh_shading_supported = meshShaderFeatureSupported;
    if (meshShaderFeatureSupported) {
        device->_vkCmdDrawMeshTasksEXT = (PFN_vkCmdDrawMeshTasksEXT)
            vkGetDeviceProcAddr(result, "vkCmdDrawMeshTasksEXT");

        VkPhysicalDeviceMeshShaderPropertiesEXT meshShaderProperties = {0};
        meshShaderProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT;
        VkPhysicalDeviceProperties2 properties2 = {0};
        properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        properties2.pNext = &meshShaderProperties;
        vkGetPhysicalDeviceProperties2(physicalDevice, &properties2);

        device->_mesh_shading_max_output_vertices      = meshShaderProperties.maxMeshOutputVertices;
        device->_mesh_shading_max_output_primitives    = meshShaderProperties.maxMeshOutputPrimitives;
        device->_mesh_shading_max_workgroup_invocations = meshShaderProperties.maxMeshWorkGroupInvocations;
    } else {
        device->_vkCmdDrawMeshTasksEXT = NULL;
    }

    /*
    // Take that graphics queue and make a SpudGPU command queue from it.
    // TODO: Later we will implement more types of command queues.
    m_command_queues.resize(1);
    {
        VkQueue graphicsQueue = VK_NULL_HANDLE;
        vkGetDeviceQueue(m_device, graphicsQueueFamilyIndex, 0, &graphicsQueue);
        auto spudCommandQueue = std::make_shared<command_queue_vulkan>(graphicsQueue);
        m_command_queues[0] = spudCommandQueue;
    }*/

    /*
    // Create the SpudGPU resource pool object
    {
        std::shared_ptr<gpu_resource_pool_vulkan> spudGpuVulkanPool = std::make_shared<
            gpu_resource_pool_vulkan>();
        m_resource_pool = std::static_pointer_cast<gpu_resource_pool_vulkan>(spudGpuVulkanPool);
    }*/

    free(queueFamilies);
    return result;
};

static void spudgpuvulkan__determine_vulkan_extensions(VkInstanceCreateInfo *pOutput) {
    if (!pOutput) return;
#if defined(_WIN32)
    static const char *extensions[] = {"VK_KHR_surface", "VK_KHR_win32_surface"};
    pOutput->enabledExtensionCount = 2;
#elif defined(__linux__)
    // SDL picks Wayland vs Xlib at runtime (independent of whichever one
    // SPUDGPU_PLATFORM_* was compiled against), so both surface extensions
    // must be enabled on the instance rather than just one.
    static const char *extensions[] = {"VK_KHR_surface", "VK_KHR_wayland_surface", "VK_KHR_xlib_surface"};
    pOutput->enabledExtensionCount = 3;
#endif
    pOutput->ppEnabledExtensionNames = extensions;
}

#if __cplusplus
extern "C" {



#endif


SPUDRESULT spudgpu_create_instance(
    const char *application_name,
    uint32_t application_version,
    const char *engine_name,
    uint32_t engine_version,
    spudgpu_instance *out_instance) {
    if (!out_instance) return SPUDRESULT_NULL_OUTPUT_PARAMETER;
    spudgpu_instance_vulkan result = {0};

    VkApplicationInfo appInfo = {0};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pNext = NULL;
    appInfo.apiVersion = VK_API_VERSION_1_3;
    appInfo.pApplicationName = application_name;
    appInfo.applicationVersion = application_version;
    appInfo.pEngineName = engine_name;
    appInfo.engineVersion = engine_version;

    VkInstanceCreateInfo createInfo = {0};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pNext = NULL;
    createInfo.flags = 0;
    createInfo.pApplicationInfo = &appInfo;
    spudgpuvulkan__determine_vulkan_extensions(&createInfo);

    createInfo.enabledLayerCount = 0;
    createInfo.ppEnabledLayerNames = NULL;

#if _DEBUG
    // Without this, invalid API usage tends to surface as an access violation
    // deep inside the ICD (e.g. igvk64.dll) instead of a VK_ERROR_* return,
    // since the driver isn't required to validate its own inputs.
    static const char *validationLayer = "VK_LAYER_KHRONOS_validation";
    uint32_t availableLayerCount = 0;
    vkEnumerateInstanceLayerProperties(&availableLayerCount, NULL);
    VkLayerProperties *availableLayers = malloc(sizeof(VkLayerProperties) * availableLayerCount);
    vkEnumerateInstanceLayerProperties(&availableLayerCount, availableLayers);
    for (uint32_t i = 0; i < availableLayerCount; i++) {
        if (strcmp(availableLayers[i].layerName, validationLayer) == 0) {
            createInfo.enabledLayerCount = 1;
            createInfo.ppEnabledLayerNames = &validationLayer;
            break;
        }
    }
    free(availableLayers);
#endif

    if (vkCreateInstance(&createInfo, NULL, &result._instance_vk) != VK_SUCCESS) {
        //throw std::runtime_error("failed to create vulkan instance!");
        return SPUDRESULT_API_SPECIFIC_FAILURE;
    }

    // If successful, return a memcpy'ed heap pointer of the result
    spudgpu_instance_vulkan *pResult = malloc(sizeof(spudgpu_instance_vulkan));
    memcpy(pResult, &result, sizeof(spudgpu_instance_vulkan));
    *out_instance = pResult;
    return SPUD_SUCCESS;
}

SPUDRESULT spudgpu_destroy_instance(spudgpu_instance instance) {
    if (!instance) return SPUD_SUCCESS;

    // instance owns every spudgpu_device handed out by spudgpu_enumerate_devices
    // (there is no public spudgpu_destroy_device) — _devices_pointer_array
    // exists precisely to make that cleanup possible; see its declaration.
    for (uint32_t i = 0; i < instance->_devices_count; i++) {
        spudgpu_device_vulkan *dev = (spudgpu_device_vulkan *) instance->_devices_pointer_array[i];
        if (!dev) continue;
        // Created for every device in spudgpu_enumerate_devices.
        spudgpuvulkan___destroy_bindless_state((spudgpu_device) dev);
        vkDestroyDevice(dev->_logical_device_vk, NULL);
#if _DEBUG
        free((void *)dev->_debug_name);
#endif
        free(dev);
    }
    free(instance->_devices_pointer_array);

    vkDestroyInstance(instance->_instance_vk, NULL);
#if _DEBUG
    free((void *)instance->_debug_name);
#endif
    free(instance);
    return SPUD_SUCCESS;
}

static void spudgpuvulkan___internal_make_device_properties(
    spudgpu_device_vulkan *device) {
    if (!device) return;

    const VkPhysicalDeviceProperties *props = &device->_properties_vk;

    size_t nameLen = strnlen(
        props->deviceName, sizeof(device->_properties.description) - 1);
    memcpy(device->_properties.description, props->deviceName, nameLen);
    device->_properties.description[nameLen] = '\0';

    device->_properties.vendor_id = props->vendorID;
    device->_properties.device_id = props->deviceID;
    // Vulkan has no standard equivalent to DXGI's subsystem ID or hardware
    // revision, so these are left unset.
    device->_properties.subSys_id = 0;
    device->_properties.revision  = 0;

    VkPhysicalDeviceMemoryProperties memProps = {0};
    vkGetPhysicalDeviceMemoryProperties(device->_physical_device_vk, &memProps);

    uint64_t deviceLocalMemory = 0;
    uint64_t hostVisibleMemory = 0;
    for (uint32_t i = 0; i < memProps.memoryHeapCount; i++) {
        if (memProps.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
            deviceLocalMemory += memProps.memoryHeaps[i].size;
        else
            hostVisibleMemory += memProps.memoryHeaps[i].size;
    }

    device->_properties.dedicated_video_memory = deviceLocalMemory;
    // Vulkan does not distinguish a GPU-private system memory pool from
    // shared system memory the way DXGI does.
    device->_properties.dedicated_system_memory = 0;
    device->_properties.shared_system_memory    = hostVisibleMemory;
    // Vulkan doesn't say whether a device's memory is the system's. The
    // device type is what it does say, and an integrated GPU is the type
    // that has none of its own.
    device->_properties.unified_memory =
        props->deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;

    // SPUDGPU_MEMORY_KIND_DEVICE_MAPPABLE needs a memory type that is the
    // device's and can be mapped, and is only offered where the device's
    // memory is the system's (see SPUDGPU_DEVICE_PROPERTIES in spudgpu.h).
    bool deviceLocalHostVisibleType = false;
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        const VkMemoryPropertyFlags both =
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        if ((memProps.memoryTypes[i].propertyFlags & both) == both)
            deviceLocalHostVisibleType = true;
    }
    device->_properties.device_mappable_memory =
        device->_properties.unified_memory && deviceLocalHostVisibleType;

    device->_properties.layered_rendering =
        spudgpuvulkan___internal_shader_output_layer(device->_physical_device_vk);
}

SPUDRESULT spudgpu_enumerate_devices(
    spudgpu_instance instance,
    spudgpu_device **ppOutputDevices,
    uint32_t *pOutputDevicesCount) {
    if (!instance) return SPUDRESULT_GPU_INVALID_INSTANCE;
    if (!ppOutputDevices) return SPUDRESULT_NULL_OUTPUT_PARAMETER;
    if (!pOutputDevicesCount) return SPUDRESULT_NULL_OUTPUT_PARAMETER;

    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance->_instance_vk, &deviceCount, NULL);
    if (deviceCount == 0) {
        //throw std::runtime_error("failed to find GPUs with Vulkan support!");
        return SPUDRESULT_GPU_DEVICE_ENUMERATION_FAILURE;
    }
    VkPhysicalDevice *physicalDevices = malloc(sizeof(VkPhysicalDevice) * deviceCount);
    vkEnumeratePhysicalDevices(instance->_instance_vk, &deviceCount, physicalDevices);

    // Finally initialize the Vulkan logical devices and gather them.
    *ppOutputDevices = malloc(sizeof(spudgpu_device) * deviceCount);
    *pOutputDevicesCount = deviceCount;

    // Instance needs to keep track for memory management.
    instance->_devices_pointer_array = malloc(sizeof(uint64_t) * deviceCount);
    instance->_devices_count = deviceCount;

    for (size_t i = 0; i < deviceCount; i++) {
        spudgpu_device_vulkan *pDeviceVulkan = malloc(sizeof(spudgpu_device_vulkan));
#if _DEBUG
        pDeviceVulkan->_debug_name = NULL;
#endif
        pDeviceVulkan->_bindless = NULL;
        pDeviceVulkan->_instance = *instance;
        pDeviceVulkan->_physical_device_vk = physicalDevices[i];
        pDeviceVulkan->_logical_device_vk = spudgpuvulkan___initialize_vk_logical_device_internal(pDeviceVulkan);

        // Grab the properties and feature descriptors.
        vkGetPhysicalDeviceProperties(pDeviceVulkan->_physical_device_vk, &pDeviceVulkan->_properties_vk);
        vkGetPhysicalDeviceFeatures(pDeviceVulkan->_physical_device_vk, &pDeviceVulkan->_features_vk);
        spudgpuvulkan___internal_make_device_properties(pDeviceVulkan);

        // Must happen before this device is ever copied by value (every
        // other Vulkan-backend struct embeds spudgpu_device_vulkan as a
        // snapshot) — see spudgpuvulkandescriptors.c's bindless section.
        spudgpuvulkan___ensure_bindless_state(pDeviceVulkan);

        (*ppOutputDevices)[i] = (spudgpu_device) pDeviceVulkan;

        // Instance needs to keep track for memory management.
        instance->_devices_pointer_array[i] = (uint64_t) (*ppOutputDevices)[i];
    }

    free(physicalDevices);
    return SPUD_SUCCESS;
}

SPUDRESULT spudgpu_get_device_properties(
    spudgpu_device device, SPUDGPU_DEVICE_PROPERTIES *out_properties) {
	if (!device)
		return SPUDRESULT_GPU_INVALID_DEVICE;
	if (!out_properties)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	memcpy(
	    out_properties, &device->_properties,
	    sizeof(SPUDGPU_DEVICE_PROPERTIES));
	return SPUD_SUCCESS;
}

SPUDGPU_NATIVE_API spudgpu_get_native_gpu_api(spudgpu_instance instance) {
    return instance ? SPUDGPU_NATIVE_API_VULKAN : SPUDGPU_NATIVE_API_NONE;
}

#if SPUDGPU_EXT_MESH_SHADING
SPUDRESULT spudgpu_get_mesh_shading_capabilities(
    spudgpu_device device, spudgpu_mesh_shading_capabilities *out_caps) {
    if (!device)
        return SPUDRESULT_GPU_INVALID_DEVICE;
    if (!out_caps)
        return SPUDRESULT_NULL_OUTPUT_PARAMETER;

    memset(out_caps, 0, sizeof(*out_caps));
    out_caps->supported = device->_mesh_shading_supported;
    if (device->_mesh_shading_supported) {
        out_caps->max_mesh_output_vertices       = device->_mesh_shading_max_output_vertices;
        out_caps->max_mesh_output_primitives     = device->_mesh_shading_max_output_primitives;
        out_caps->max_mesh_workgroup_invocations = device->_mesh_shading_max_workgroup_invocations;
    }
    return SPUD_SUCCESS;
}
#endif

#if SPUDGPU_EXT_DEPTH_BOUNDS_TEST
SPUDRESULT spudgpu_get_depth_bounds_capabilities(
    spudgpu_device device, spudgpu_depth_bounds_capabilities *out_caps) {
    if (!device)
        return SPUDRESULT_GPU_INVALID_DEVICE;
    if (!out_caps)
        return SPUDRESULT_NULL_OUTPUT_PARAMETER;

    memset(out_caps, 0, sizeof(*out_caps));
    out_caps->supported = device->_depth_bounds_test_supported;
    return SPUD_SUCCESS;
}
#endif

#if __cplusplus
}
#endif

/*
std::vector<std::shared_ptr<gpu_device> > initialize_devices_vulkan() {
    std::vector<std::shared_ptr<gpu_device_vulkan> > spudVulkanDevices = std::vector<std::shared_ptr<
        gpu_device_vulkan> >();

    uint32_t deviceCount = 0;
    {
        vkEnumeratePhysicalDevices(g_pVulkanInstance, &deviceCount, NULL);
        if (deviceCount == 0) {
            throw std::runtime_error("failed to find GPUs with Vulkan support!");
        }
        std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
        vkEnumeratePhysicalDevices(g_pVulkanInstance, &deviceCount, physicalDevices.data());
        spudVulkanDevices.resize(deviceCount);

        for (size_t i = 0; i < deviceCount; i++) {
            auto spudVulkanDevice = std::make_shared<gpu_device_vulkan>();
            spudVulkanDevice->initialize_from_physical_device_impl(physicalDevices[i]);
            spudVulkanDevices[i] = spudVulkanDevice;
        }
    }

    // Convert all to base classes for return.
    std::vector<std::shared_ptr<gpu_device> > spudGpuDevices(spudVulkanDevices.size());
    for (size_t i = 0; i < spudVulkanDevices.size(); i++)
        spudGpuDevices[i] = spudVulkanDevices[i];

    return spudGpuDevices;
}

void terminate_devices_vulkan() {
}
}*/

#endif //SPUDGPU_COMPILE_VULKAN_API

