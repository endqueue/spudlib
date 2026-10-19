
#if SPUDGPU_COMPILE_D3D12_API

#include "spudgpud3d12.hpp"
#include <vector>
#include <string>

static SPUDRESULT ___internal_spudgpu_d3d12_create_command_queues_per_family(
    spudgpu_device device,
    const D3D12_COMMAND_QUEUE_DESC *desc,
    std::array<spudgpu_command_queue, SPUD_D3D12_COMMAND_QUEUE_COUNT_PER_FAMILY>
        &arr) {
	// A queue that fails is released here; the ones already in arr are
	// released with the device (___internal_spudgpu_d3d12_destroy_device).
	for (size_t i = 0; i < arr.size(); ++i) {
		spudgpu_command_queue_d3d12 *queue =
		    (spudgpu_command_queue_d3d12 *)malloc(
		        sizeof(spudgpu_command_queue_d3d12));
		if (!queue)
			return SPUDRESULT_OUT_OF_MEMORY;
		queue          = new (queue) spudgpu_command_queue_d3d12();
		queue->_device = device;
		if (FAILED(device->_d3d_device->CreateCommandQueue(
		        desc, IID_PPV_ARGS(&queue->_d3d_cmd_queue)))) {
			queue->~spudgpu_command_queue_d3d12();
			free(queue);
			return SPUDRESULT_API_SPECIFIC_FAILURE;
		}
		arr[i] = queue;
	}
	return SPUD_SUCCESS;
}

static void ___internal_spudgpu_d3d12_destroy_command_queues_per_family(
    std::array<spudgpu_command_queue, SPUD_D3D12_COMMAND_QUEUE_COUNT_PER_FAMILY>
        &arr) {
	for (size_t i = 0; i < arr.size(); ++i) {
		if (!arr[i])
			continue;
#if _DEBUG
		free((void *)arr[i]->_debug_name);
#endif
		arr[i]->~spudgpu_command_queue_d3d12();
		free(arr[i]);
		arr[i] = nullptr;
	}
}

#if _DEBUG
static const char *spudgpu_d3d12_severity_name(D3D12_MESSAGE_SEVERITY severity) {
	switch (severity) {
	case D3D12_MESSAGE_SEVERITY_CORRUPTION:
		return "CORRUPTION";
	case D3D12_MESSAGE_SEVERITY_ERROR:
		return "ERROR";
	case D3D12_MESSAGE_SEVERITY_WARNING:
		return "WARNING";
	case D3D12_MESSAGE_SEVERITY_INFO:
		return "INFO";
	default:
		return "MESSAGE";
	}
}

// Prints the debug layer's corruptions, errors and warnings to stdout, as
// Vulkan's validation layer does with no messenger registered, so a Debug run
// shows them without a debugger attached. Info and plain messages are left
// to the debugger. Called on whichever thread made the D3D12 call that
// raised the message.
static void CALLBACK spudgpu_d3d12_debug_message(
    D3D12_MESSAGE_CATEGORY category,
    D3D12_MESSAGE_SEVERITY severity,
    D3D12_MESSAGE_ID id,
    LPCSTR description,
    void *context) {
	(void)category;
	if (severity == D3D12_MESSAGE_SEVERITY_INFO || severity == D3D12_MESSAGE_SEVERITY_MESSAGE)
		return;
	const spudgpu_device_d3d12 *device = (const spudgpu_device_d3d12 *)context;
	const char *name                   = device->_debug_name ? device->_debug_name : "";
	printf("D3D12 %s: [ %u ] %s%s%s\n",
	    spudgpu_d3d12_severity_name(severity),
	    (unsigned)id,
	    description,
	    name[0] ? " | device " : "",
	    name);
}

// Registers spudgpu_d3d12_debug_message on [device]. Nothing to do when the
// debug layer is off (no info queue) or the runtime predates
// ID3D12InfoQueue1; the device works the same either way, so neither is a
// failure.
static void spudgpu_d3d12_register_debug_callback(spudgpu_device_d3d12 *device) {
	Microsoft::WRL::ComPtr<ID3D12InfoQueue1> info_queue;
	if (FAILED(device->_d3d_device.As(&info_queue)))
		return;
	DWORD cookie = 0;
	if (FAILED(info_queue->RegisterMessageCallback(spudgpu_d3d12_debug_message, D3D12_MESSAGE_CALLBACK_FLAG_NONE, device, &cookie)))
		return;
	device->_debug_callback_cookie = cookie;
}

// Unregisters it, before the device it was given as context goes.
static void spudgpu_d3d12_unregister_debug_callback(spudgpu_device_d3d12 *device) {
	if (device->_debug_callback_cookie == 0)
		return;
	Microsoft::WRL::ComPtr<ID3D12InfoQueue1> info_queue;
	if (SUCCEEDED(device->_d3d_device.As(&info_queue)))
		info_queue->UnregisterMessageCallback(device->_debug_callback_cookie);
	device->_debug_callback_cookie = 0;
}
#endif

// Releases a device and everything it owns: its command queues and, if one
// was ever created, its bindless state.
static void ___internal_spudgpu_d3d12_destroy_device(spudgpu_device device) {
	if (!device)
		return;
#if _DEBUG
	// First: the callback's context is this device.
	spudgpu_d3d12_unregister_debug_callback(device);
#endif
	___internal_spudgpu_d3d12_destroy_command_queues_per_family(
	    device->_cmd_queues_direct);
	___internal_spudgpu_d3d12_destroy_command_queues_per_family(
	    device->_cmd_queues_copy);
	___internal_spudgpu_d3d12_destroy_command_queues_per_family(
	    device->_cmd_queues_compute);
	if (device->_bindless) {
#if _DEBUG
		if (device->_bindless->layout)
			free((void *)device->_bindless->layout->_debug_name);
#endif
		free(device->_bindless->layout);
		device->_bindless->~spudgpu_bindless_state_d3d12();
		free(device->_bindless);
	}
#if _DEBUG
	free((void *)device->_debug_name);
#endif
	device->~spudgpu_device_d3d12();
	free(device);
}

static void ___internal_spudgpu_d3d12_destroy_devices(
    spudgpu_device *devices, size_t count) {
	for (size_t i = 0; i < count; ++i)
		___internal_spudgpu_d3d12_destroy_device(devices[i]);
}

static SPUDRESULT
___internal_spudgpu_d3d12_create_device_command_queues(spudgpu_device device) {
	if (!device)
		return SPUDRESULT_GPU_INVALID_DEVICE;

	D3D12_COMMAND_QUEUE_DESC desc = {};
	desc.Flags                    = D3D12_COMMAND_QUEUE_FLAG_NONE;
	desc.NodeMask                 = 1;
	desc.Priority                 = D3D12_COMMAND_QUEUE_PRIORITY_HIGH;

	SPUDRESULT r = SPUD_SUCCESS;

	// Direct Queues
	desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	r         = ___internal_spudgpu_d3d12_create_command_queues_per_family(
	    device, &desc, device->_cmd_queues_direct);
	if (r != SPUD_SUCCESS)
		return r;

	// Copy Queues
	desc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
	r         = ___internal_spudgpu_d3d12_create_command_queues_per_family(
	    device, &desc, device->_cmd_queues_copy);
	if (r != SPUD_SUCCESS)
		return r;

	// Compute Queues
	desc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
	r         = ___internal_spudgpu_d3d12_create_command_queues_per_family(
	    device, &desc, device->_cmd_queues_compute);
	if (r != SPUD_SUCCESS)
		return r;

	return r;
}

extern "C" {

SPUDRESULT spudgpu_create_instance(
    const char *application_name,
    uint32_t application_version,
    const char *engine_name,
    uint32_t engine_version,
    spudgpu_instance *out_instance) {
	if (!out_instance)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;

	spudgpu_instance_d3d12 *pResult =
	    (spudgpu_instance_d3d12 *)malloc(sizeof(spudgpu_instance_d3d12));
	if (!pResult)
		return SPUDRESULT_OUT_OF_MEMORY;
	pResult                          = new (pResult) spudgpu_instance_d3d12();
	pResult->application_name        = application_name;
	pResult->application_version     = application_version;
	pResult->engine_name             = engine_name;
	pResult->engine_version          = engine_version;
	pResult->_gpu_devices            = nullptr;
	pResult->_gpu_device_count       = 0;
	pResult->_gpu_devices_enumerated = false;

#ifdef _DEBUG
	Microsoft::WRL::ComPtr<ID3D12Debug1> d3dDebugController;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&d3dDebugController)))) {
		d3dDebugController->EnableDebugLayer();
		printf("Enabled D3D12 Debug Interface Layer\r\n");
	}
#endif

	UINT dxgiFlags = 0;
#ifdef _DEBUG
	//dxgiFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
	HRESULT hr =
	    CreateDXGIFactory2(dxgiFlags, IID_PPV_ARGS(&pResult->_dxgi_factory));
	if (FAILED(hr)) {
		pResult->~spudgpu_instance_d3d12();
		free(pResult);
		return SPUDRESULT_API_SPECIFIC_FAILURE;
	}

	*out_instance = pResult;
	return SPUD_SUCCESS;
}

SPUDRESULT spudgpu_destroy_instance(spudgpu_instance instance) {
	if (!instance)
		return SPUD_SUCCESS;
	___internal_spudgpu_d3d12_destroy_devices(
	    instance->_gpu_devices, instance->_gpu_device_count);
	free(instance->_gpu_devices);
#if _DEBUG
	free((void *)instance->_debug_name);
#endif
	instance->~spudgpu_instance_d3d12();
	free(instance);
	return SPUD_SUCCESS;
}

bool ___internal_spudgpu_d3d12_make_device_properties(
    spudgpu_device_d3d12 *device) {
	if (!device)
		return false;
	if (!device->_dxgi_adapter)
		return false;
	DXGI_ADAPTER_DESC3 aDesc;
	if (FAILED(device->_dxgi_adapter->GetDesc3(&aDesc)))
		return false;
	int the_wide_dog_has_bluffed_the_smaller_dog_because_he_thinks_he_is_a_buffalo, strSize =
	WideCharToMultiByte(
	    CP_UTF8, 0, &aDesc.Description[0], 128,
	    &device->_properties.description[0], 128, NULL, FALSE);
	// device->_properties.description;
	device->_properties.dedicated_video_memory  = aDesc.DedicatedVideoMemory;
	device->_properties.dedicated_system_memory = aDesc.DedicatedSystemMemory;
	device->_properties.shared_system_memory    = aDesc.SharedSystemMemory;
	device->_properties.device_id               = aDesc.DeviceId;
	device->_properties.revision                = aDesc.Revision;
	device->_properties.subSys_id               = aDesc.SubSysId;
	device->_properties.vendor_id               = aDesc.VendorId;

	// The device exists by now: spudgpu_enumerate_devices creates it before
	// calling this. A failed query leaves the device reported as not unified.
	D3D12_FEATURE_DATA_ARCHITECTURE1 architecture = {};
	architecture.NodeIndex                        = 0;
	HRESULT hr                                    = device->_d3d_device->CheckFeatureSupport(
	    D3D12_FEATURE_ARCHITECTURE1, &architecture, sizeof(architecture));
	device->_properties.unified_memory = SUCCEEDED(hr) && architecture.UMA;
	// SPUDGPU_MEMORY_KIND_DEVICE_MAPPABLE is a custom heap in pool L0 that
	// the CPU can map, which holds a resource in any state only on a UMA
	// adapter.
	device->_properties.device_mappable_memory = device->_properties.unified_memory;

	// A layered pass needs the vertex shader to write
	// SV_RenderTargetArrayIndex, which is this capability. The pass itself
	// needs nothing: the views' array sizes are what make it layered.
	D3D12_FEATURE_DATA_D3D12_OPTIONS options = {};
	hr = device->_d3d_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &options, sizeof(options));
	device->_properties.layered_rendering =
	    SUCCEEDED(hr) && options.VPAndRTArrayIndexFromAnyShaderFeedingRasterizerSupportedWithoutGSEmulation;
	// Bindless binds SPUDGPU_BINDLESS_MAX_SLOTS_PER_CLASS storage images and
	// as many storage buffers in one table, more UAVs than tier 2 allows a
	// stage (64).
	device->_bindless_supported = SUCCEEDED(hr) && options.ResourceBindingTier >= D3D12_RESOURCE_BINDING_TIER_3;
	// An ID3D12Heap can have the heap properties of every kind.
	device->_properties.mappable_heaps = true;
	// D3D12_RASTERIZER_DESC::DepthBiasClamp and
	// D3D12_BLEND_DESC::IndependentBlendEnable are on every device.
	device->_properties.depth_bias_clamp  = true;
	device->_properties.independent_blend = true;
	// TEXTURECUBEARRAY views are in every feature level this backend lists.
	device->_properties.cube_array_views = true;
	// ExecuteIndirect takes any MaxCommandCount.
	device->_properties.max_draw_indirect_count = UINT32_MAX;
	// A CBV's BufferLocation, and a raw buffer view's first byte, on every
	// device.
	device->_properties.uniform_buffer_offset_alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
	device->_properties.storage_buffer_offset_alignment = D3D12_RAW_UAV_SRV_BYTE_ALIGNMENT;
	// A direct queue's command lists can write timestamps anywhere.
	device->_properties.pass_timestamps = true;
	return true;
}
SPUDRESULT spudgpu_enumerate_devices(
    spudgpu_instance instance,
    spudgpu_device **ppOutputDevices,
    uint32_t *pOutputDevicesCount) {
	if (!instance)
		return SPUDRESULT_GPU_INVALID_INSTANCE;
	if (!ppOutputDevices)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	if (!pOutputDevicesCount)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	// The devices are made once; a later call hands out the same array.
	if (instance->_gpu_devices_enumerated) {
		*pOutputDevicesCount = instance->_gpu_device_count;
		*ppOutputDevices     = instance->_gpu_devices;
		return SPUD_SUCCESS;
	}

	SPUDRESULT sr = SPUD_SUCCESS;

	// What the backend needs of every device: 12_0 guarantees resource
	// binding tier 2, without which a descriptor table may hold only 128 SRVs
	// per stage and SpudGPU's descriptor sets have no such limit. Everything
	// past it is optional and reported per device - mesh shading
	// (OPTIONS7), the depth bounds test (OPTIONS2), bindless (resource
	// binding tier 3) - so a device without it is listed and says so.
	const D3D_FEATURE_LEVEL minimumFeatureLevel = D3D_FEATURE_LEVEL_12_0;

	std::vector<spudgpu_device_d3d12 *> gpuDevices =
	    std::vector<spudgpu_device_d3d12 *>();

	Microsoft::WRL::ComPtr<IDXGIAdapter1> dxgiAdapter1 = nullptr;
	for (uint32_t adapterIndex = 0;
	     instance->_dxgi_factory->EnumAdapters1(adapterIndex, &dxgiAdapter1) !=
	     DXGI_ERROR_NOT_FOUND;
	     ++adapterIndex) {
		if (!dxgiAdapter1) // Don't mess with a null result.
			continue;
		DXGI_ADAPTER_DESC1 desc;
		dxgiAdapter1->GetDesc1(&desc);
		// Don't include a software graphics device.
		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			continue;

		Microsoft::WRL::ComPtr<IDXGIAdapter4> dxgiAdapter = nullptr;
		if (FAILED(dxgiAdapter1.As(&dxgiAdapter)))
			continue;

		// Validate a possible creation of an ID3D12Device
		if (SUCCEEDED(D3D12CreateDevice(
		        dxgiAdapter.Get(), minimumFeatureLevel,
		        __uuidof(ID3D12Device14), nullptr))) {

			// Create a new SpudGPU Device
			// Every failure below releases this device and the ones
			// already gathered, so nothing outlives a failed enumeration.
			spudgpu_device_d3d12 *gpuDevice =
			    (spudgpu_device_d3d12 *)malloc(sizeof(spudgpu_device_d3d12));
			if (!gpuDevice) {
				___internal_spudgpu_d3d12_destroy_devices(
				    gpuDevices.data(), gpuDevices.size());
				return SPUDRESULT_OUT_OF_MEMORY;
			}
			gpuDevice = new (gpuDevice) spudgpu_device_d3d12();
			if (FAILED(D3D12CreateDevice(
			        dxgiAdapter.Get(), minimumFeatureLevel,
			        IID_PPV_ARGS(&gpuDevice->_d3d_device)))) {
				// If a D3D12 Device creation failed,
				// just return out of this function.
				___internal_spudgpu_d3d12_destroy_device(gpuDevice);
				___internal_spudgpu_d3d12_destroy_devices(
				    gpuDevices.data(), gpuDevices.size());
				return SPUDRESULT_API_SPECIFIC_FAILURE;
			}
			gpuDevice->_dxgi_adapter = dxgiAdapter;
			gpuDevice->_instance     = instance;
#if _DEBUG
			spudgpu_d3d12_register_debug_callback(gpuDevice);
#endif
			if (!___internal_spudgpu_d3d12_make_device_properties(gpuDevice)) {
				___internal_spudgpu_d3d12_destroy_device(gpuDevice);
				___internal_spudgpu_d3d12_destroy_devices(
				    gpuDevices.data(), gpuDevices.size());
				// A feature query failed. This returned the outer sr,
				// still SPUD_SUCCESS here, reporting success with every
				// device released.
				return SPUDRESULT_API_SPECIFIC_FAILURE;
			}

			// Create the premade command queues for use.
			SPUDRESULT sr =
			    ___internal_spudgpu_d3d12_create_device_command_queues(
			        gpuDevice);
			if (sr != SPUD_SUCCESS) {
				___internal_spudgpu_d3d12_destroy_device(gpuDevice);
				___internal_spudgpu_d3d12_destroy_devices(
				    gpuDevices.data(), gpuDevices.size());
				return sr;
			}
			gpuDevices.push_back(gpuDevice);
			continue;
		} else
			continue;
	}
	// No array for zero devices: _gpu_devices stays null and the count 0.
	if (!gpuDevices.empty()) {
		spudgpu_device *devices = (spudgpu_device *)malloc(
		    sizeof(spudgpu_device) * gpuDevices.size());
		if (!devices) {
			___internal_spudgpu_d3d12_destroy_devices(
			    gpuDevices.data(), gpuDevices.size());
			return SPUDRESULT_OUT_OF_MEMORY;
		}
		memcpy(
		    devices, gpuDevices.data(),
		    sizeof(spudgpu_device) * gpuDevices.size());
		instance->_gpu_devices = devices;
	}
	instance->_gpu_device_count       = (uint32_t)gpuDevices.size();
	instance->_gpu_devices_enumerated = true;

	*pOutputDevicesCount = instance->_gpu_device_count;
	*ppOutputDevices     = instance->_gpu_devices;

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
	return instance ? SPUDGPU_NATIVE_API_D3D12 : SPUDGPU_NATIVE_API_NONE;
}

#if SPUDGPU_EXT_MESH_SHADING
SPUDRESULT spudgpu_get_mesh_shading_capabilities(
    spudgpu_device device, spudgpu_mesh_shading_capabilities *out_caps) {
	if (!device)
		return SPUDRESULT_GPU_INVALID_DEVICE;
	if (!out_caps)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;

	*out_caps = {};

	D3D12_FEATURE_DATA_D3D12_OPTIONS7 options7 = {};
	HRESULT hr                                 = device->_d3d_device->CheckFeatureSupport(
	    D3D12_FEATURE_D3D12_OPTIONS7, &options7, sizeof(options7));
	out_caps->supported =
	    SUCCEEDED(hr) && options7.MeshShaderTier != D3D12_MESH_SHADER_TIER_NOT_SUPPORTED;
	if (!out_caps->supported)
		return SPUD_SUCCESS;

	// D3D12/SM6.5 spec-mandated maximums (not a per-device query the way
	// Vulkan's VkPhysicalDeviceMeshShaderPropertiesEXT is) - every tier
	// (D3D12_MESH_SHADER_TIER_1) guarantees these limits.
	out_caps->max_mesh_output_vertices       = 256;
	out_caps->max_mesh_output_primitives     = 256;
	out_caps->max_mesh_workgroup_invocations = 128;
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

	*out_caps = {};

	D3D12_FEATURE_DATA_D3D12_OPTIONS2 options2 = {};
	HRESULT hr                                  = device->_d3d_device->CheckFeatureSupport(
	    D3D12_FEATURE_D3D12_OPTIONS2, &options2, sizeof(options2));
	out_caps->supported = SUCCEEDED(hr) && options2.DepthBoundsTestSupported;
	return SPUD_SUCCESS;
}
#endif

SPUDRESULT spudgpu_create_surface(
    spudgpu_instance instance,
    void *window_handle,
    void *display_handle,
    spudgpu_surface *out_surface) {
	if (!instance)
		return SPUDRESULT_GPU_INVALID_INSTANCE;
	if (!window_handle)
		return SPUDRESULT_GPU_INVALID_WINDOW_HANDLE;
	if (!out_surface)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	spudgpu_surface_d3d12 *pResult =
	    (spudgpu_surface_d3d12 *)calloc(1, sizeof(spudgpu_surface_d3d12));
	if (!pResult)
		return SPUDRESULT_OUT_OF_MEMORY;
	pResult->_hwnd     = (HWND)window_handle;
	pResult->_instance = instance;
	*out_surface       = pResult;
	return SPUD_SUCCESS;
}
void spudgpu_destroy_surface(spudgpu_surface surface) {
	if (!surface)
		return;
#if _DEBUG
	free((void *)surface->_debug_name);
#endif
	free(surface);
}
}

#endif
