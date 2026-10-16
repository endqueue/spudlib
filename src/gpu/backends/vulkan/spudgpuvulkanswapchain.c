//
// Created by Nathan on 5/13/2026.
//

#if SPUDGPU_COMPILE_VULKAN_API

#include "spudgpuvulkan.h"
#include "spudgpu.h"

#if defined(SPUDLIB_PLATFORM_WIN32)
#include <windows.h>
#include <vulkan/vulkan_win32.h>

#elif defined(SPUDGPU_PLATFORM_WAYLAND)
#include <wayland-client.h>
#include <vulkan/vulkan_wayland.h>

#elif defined(SPUDGPU_PLATFORM_XLIB)
#include <vulkan/vulkan_xlib.h>
#include <X11/Xlib.h>
#endif

#include <stdlib.h>
#include <string.h>


#if __cplusplus
extern "C" {
#endif

void spudgpuvulkan___get_available_formats_internal(
	spudgpu_swap_chain_vulkan *pSwapChain,
	VkSurfaceFormatKHR **ppOutput,
	uint32_t *pOutputCount) {
	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(pSwapChain->_device._physical_device_vk, pSwapChain->_surface_vk, &formatCount,
	                                     NULL);
	*ppOutput = malloc(sizeof(VkSurfaceFormatKHR) * formatCount);
	*pOutputCount = formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(pSwapChain->_device._physical_device_vk, pSwapChain->_surface_vk, &formatCount,
	                                     *ppOutput);
}

void spudgpuvulkan___get_available_present_modes_internal(
	spudgpu_swap_chain_vulkan *pSwapChain,
	VkPresentModeKHR **ppOutput,
	uint32_t *pOutputCount) {
	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(pSwapChain->_device._physical_device_vk, pSwapChain->_surface_vk,
	                                          &presentModeCount, NULL);
	*ppOutput = malloc(sizeof(VkPresentModeKHR) * presentModeCount);
	*pOutputCount = presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(pSwapChain->_device._physical_device_vk, pSwapChain->_surface_vk,
	                                          &presentModeCount, *ppOutput);
}

VkSurfaceFormatKHR spudgpuvulkan___choose_surface_format_internal(
	VkSurfaceFormatKHR *pAvailableFormats,
	uint32_t availableFormatCount,
	VkFormat requestedFormat) {
	// The caller (pSwapChain->_desc.format) already decided which format it
	// wants - e.g. so it matches the format a pipeline was created against
	// (spudgpu_shader_pipeline_desc::color_attachment_formats). Picking a
	// different one here on the caller's behalf is exactly the kind of
	// silent SpudLib-side default this backend must not make; only fall
	// back to SRGB (still colorSpace-matched) if the exact request isn't
	// actually present on this surface.
	for (uint32_t i = 0; i < availableFormatCount; i++)
		if (pAvailableFormats[i].format == requestedFormat &&
		    pAvailableFormats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			return pAvailableFormats[i];
	for (uint32_t i = 0; i < availableFormatCount; i++)
		if (pAvailableFormats[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
		    pAvailableFormats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			return pAvailableFormats[i];
	return (VkSurfaceFormatKHR){0};
}

VkPresentModeKHR spudgpuvulkan___translate_present_mode_internal(
	SPUDGPU_PRESENT_MODE mode) {
	switch (mode) {
		case SPUDGPU_PRESENT_MODE_IMMEDIATE: return VK_PRESENT_MODE_IMMEDIATE_KHR;
		case SPUDGPU_PRESENT_MODE_MAILBOX: return VK_PRESENT_MODE_MAILBOX_KHR;
		case SPUDGPU_PRESENT_MODE_FIFO:
		default: return VK_PRESENT_MODE_FIFO_KHR;
	}
}

VkPresentModeKHR spudgpuvulkan___choose_present_mode_internal(
	VkPresentModeKHR *pAvailablePresentModes,
	uint32_t availablePresentModeCount,
	SPUDGPU_PRESENT_MODE requested) {
	VkPresentModeKHR wanted = spudgpuvulkan___translate_present_mode_internal(requested);
	for (uint32_t i = 0; i < availablePresentModeCount; i++)
		if (pAvailablePresentModes[i] == wanted)
			return wanted;
	return VK_PRESENT_MODE_FIFO_KHR; // FIFO is guaranteed to be available by the Vulkan spec (Standard V-Sync)
}

VkExtent2D spudgpuvulkan___choose_extent_internal(
	VkSurfaceCapabilitiesKHR capabilities,
	uint32_t width,
	uint32_t height) {
	// If currentExtent is NOT set to the max value of uint32_t,
	// it means the surface size is determined by the window manager.
	if (capabilities.currentExtent.width != SPUD_UINT32_MAX) return capabilities.currentExtent;
	VkExtent2D actualExtent = {width, height};
	if (actualExtent.width < capabilities.minImageExtent.width)
		actualExtent.width = capabilities.minImageExtent.width;
	if (actualExtent.width > capabilities.maxImageExtent.width)
		actualExtent.width = capabilities.maxImageExtent.width;
	if (actualExtent.height < capabilities.minImageExtent.height)
		actualExtent.height = capabilities.minImageExtent.height;
	if (actualExtent.height > capabilities.maxImageExtent.height)
		actualExtent.height = capabilities.maxImageExtent.height;
	return actualExtent;
}


VkResult spudgpuvulkan___create_swapchain_internal(
	spudgpu_swap_chain_vulkan *pSwapChain,
	uint32_t width,
	uint32_t height,
	VkSwapchainKHR oldHandle) {
	VkResult result = VK_SUCCESS;
	VkPhysicalDevice physicalDevice = pSwapChain->_device._physical_device_vk;
	VkDevice logicalDevice = pSwapChain->_device._logical_device_vk;
	// Query capabilities
	VkSurfaceCapabilitiesKHR capabilities;
	VkSurfaceFormatKHR surfaceFormat;
	VkPresentModeKHR presentMode;
	{
		result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, pSwapChain->_surface_vk, &capabilities);
		if (result != VK_SUCCESS) return result;

		VkSurfaceFormatKHR *pSurfaceFormats = NULL;
		uint32_t surfaceFormatCount = 0;
		spudgpuvulkan___get_available_formats_internal(pSwapChain, &pSurfaceFormats, &surfaceFormatCount);
		surfaceFormat = spudgpuvulkan___choose_surface_format_internal(
			pSurfaceFormats, surfaceFormatCount,
			convert_spud_to_vulkan_format(pSwapChain->_desc.format));
		free(pSurfaceFormats);

		VkPresentModeKHR *pPresentModes = NULL;
		uint32_t presentModeCount = 0;
		spudgpuvulkan___get_available_present_modes_internal(pSwapChain, &pPresentModes, &presentModeCount);
		presentMode = spudgpuvulkan___choose_present_mode_internal(pPresentModes, presentModeCount, pSwapChain->_desc.present_mode);
		free(pPresentModes);
		pSwapChain->_extent_vk = spudgpuvulkan___choose_extent_internal(capabilities, width, height);
		// The surface can dictate the extent (currentExtent), overriding the
		// requested size - report what the back buffers really are.
		pSwapChain->_desc.width  = pSwapChain->_extent_vk.width;
		pSwapChain->_desc.height = pSwapChain->_extent_vk.height;
		pSwapChain->_format_vk = surfaceFormat.format;
	}

	// Determine image count
	uint32_t imageCount = capabilities.minImageCount + 1;
	if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount)
		imageCount = capabilities.maxImageCount;

	// Pick the best supported composite alpha mode — prefer OPAQUE (the
	// window isn't meant to be blended with what's behind it), falling back
	// to whatever the surface actually supports if OPAQUE isn't available.
	static const VkCompositeAlphaFlagBitsKHR compositeAlphaCandidates[] = {
		VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
	};
	VkCompositeAlphaFlagBitsKHR compositeAlpha = compositeAlphaCandidates[0];
	for (size_t i = 0; i < sizeof(compositeAlphaCandidates) / sizeof(compositeAlphaCandidates[0]); i++) {
		if (capabilities.supportedCompositeAlpha & compositeAlphaCandidates[i]) {
			compositeAlpha = compositeAlphaCandidates[i];
			break;
		}
	}

	// Populate Creation Info
	VkSwapchainCreateInfoKHR createInfo = {0};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.pNext = NULL;
	createInfo.surface = pSwapChain->_surface_vk;
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = pSwapChain->_extent_vk;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	{
		SPUDGPU_IMAGE_USAGE usage = pSwapChain->_desc.usage;
		if (usage & SPUDGPU_IMAGE_USAGE_TRANSFER_SRC)
			createInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		if (usage & SPUDGPU_IMAGE_USAGE_TRANSFER_DST)
			createInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		if (usage & SPUDGPU_IMAGE_USAGE_SAMPLED)
			createInfo.imageUsage |= VK_IMAGE_USAGE_SAMPLED_BIT;
		if (usage & SPUDGPU_IMAGE_USAGE_STORAGE)
			createInfo.imageUsage |= VK_IMAGE_USAGE_STORAGE_BIT;
		// The caller asked for these; fail rather than silently drop one.
		if ((createInfo.imageUsage & capabilities.supportedUsageFlags) != createInfo.imageUsage)
			return VK_ERROR_FEATURE_NOT_PRESENT;
	}
	createInfo.preTransform = capabilities.currentTransform;
	createInfo.compositeAlpha = compositeAlpha;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;

	// Pass the old swap chain handle when this function is called inside of recreate()
	// This is for Vulkan optimization
	createInfo.oldSwapchain = oldHandle;

	// Handle graphics vs presentation queue sharing here (Concurrent vs Exclusive)
	// ...

	result = vkCreateSwapchainKHR(logicalDevice, &createInfo, NULL, &pSwapChain->_swapchain_vk);
	if (result != VK_SUCCESS) {
		//throw std::runtime_error("failed to create swap chain!");
		return result;
	}

	// Retrieve handles to the images
	result = vkGetSwapchainImagesKHR(logicalDevice, pSwapChain->_swapchain_vk, &imageCount, NULL);
	if (result != VK_SUCCESS) return result;

	pSwapChain->_swapchain_images_count = imageCount; // Set the image count before malloc()
	pSwapChain->_swapchain_images_vk = malloc(pSwapChain->_swapchain_images_count * sizeof(VkImage));
	result = vkGetSwapchainImagesKHR(
		logicalDevice, pSwapChain->_swapchain_vk, &imageCount,
		pSwapChain->_swapchain_images_vk);
	if (result != VK_SUCCESS) return result;

	return result;
}

void spudgpuvulkan___fences_semaphores_swapchain_creation_internal(
	spudgpu_swap_chain_vulkan *pSwapChain) {
	// This is the CPU-GPU pipelining depth, not the swapchain image count —
	// deliberately independent of desc.buffer_count. The caller records into
	// a single shared command buffer/allocator per frame (not one per frame
	// in flight), so _in_flight_fences_vk[_current_frame] only actually
	// tracks that command buffer's previous submission if there is exactly
	// one slot: with buffer_count (e.g. 2) slots instead, alternate frames
	// wait on a fence that was never signaled by a real submission (still in
	// its initial signaled state), so the wait is a no-op and the shared
	// command buffer gets reset/re-recorded while the GPU may still be
	// executing it from the previous frame — see app_render (main.cpp),
	// which resets app.cmd_allocator/app.cmd_list right after acquire.
	uint32_t max_frames = 1;
	pSwapChain->_max_frames_in_flight = max_frames;
	pSwapChain->_current_frame = 0;

	// render_finished_semaphores is sized/indexed by swapchain IMAGE index
	// (_current_image_index), not by _current_frame — it's waited on by
	// vkQueuePresentKHR, and the presentation engine can hold a semaphore in
	// use past the point where our fence signals GPU completion. With only
	// max_frames_in_flight (1) of them shared across desc.buffer_count (e.g.
	// 2) images, present("image 0", sem) followed by acquire+submit+present
	// for image 1 re-signals that same semaphore while the engine may still
	// be consuming it for image 0 (VUID-vkQueueSubmit-pSignalSemaphores-00067).
	uint32_t image_count = pSwapChain->_swapchain_images_count > 0 ? pSwapChain->_swapchain_images_count : max_frames;

	pSwapChain->_image_available_semaphores = calloc(max_frames, sizeof(spudgpu_semaphore_vulkan));
	pSwapChain->_render_finished_semaphores = calloc(image_count, sizeof(spudgpu_semaphore_vulkan));
	pSwapChain->_in_flight_fences_vk        = calloc(max_frames, sizeof(VkFence));

	VkDevice vk_device = pSwapChain->_device._logical_device_vk;

	VkSemaphoreCreateInfo semInfo = {0};
	semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo = {0};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // <-- CRITICAL: first frame must not block

	for (uint32_t i = 0; i < max_frames; i++) {
		pSwapChain->_image_available_semaphores[i]._device_vk = vk_device;
		vkCreateSemaphore(vk_device, &semInfo, NULL, &pSwapChain->_image_available_semaphores[i]._semaphore_vk);
		vkCreateFence(vk_device, &fenceInfo, NULL, &pSwapChain->_in_flight_fences_vk[i]);
	}

	for (uint32_t i = 0; i < image_count; i++) {
		pSwapChain->_render_finished_semaphores[i]._device_vk = vk_device;
		vkCreateSemaphore(vk_device, &semInfo, NULL, &pSwapChain->_render_finished_semaphores[i]._semaphore_vk);
	}
}

/*
VkExtent2D swap_chain_vulkan::choose_extent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t width,
                                            uint32_t height) {
	// If currentExtent is NOT set to the max value of uint32_t,
	// it means the surface size is determined by the window manager.
	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
		return capabilities.currentExtent;
	} else {
		// Otherwise, we manually clamp our window dimensions to the
		// min/max supported by the GPU surface.
		VkExtent2D actualExtent = {width, height};

		actualExtent.width = std::clamp(actualExtent.width,
		                                capabilities.minImageExtent.width,
		                                capabilities.maxImageExtent.width);

		actualExtent.height = std::clamp(actualExtent.height,
		                                 capabilities.minImageExtent.height,
		                                 capabilities.maxImageExtent.height);

		return actualExtent;
	}
}*/

VkResult spudgpuvulkan_create_image_views_internal(
	spudgpu_swap_chain_vulkan *pSwapChain,
	uint32_t swap_chain_image_view_count) {
	pSwapChain->_swapchain_image_views_count = swap_chain_image_view_count; // Set image view count before malloc()
	pSwapChain->_swapchain_image_views_vk = calloc(pSwapChain->_swapchain_image_views_count,
	                                               sizeof(spudgpu_image_view_vulkan));

	VkResult result = VK_SUCCESS;
	for (size_t i = 0; i < pSwapChain->_swapchain_images_count; i++) {
		VkImageViewCreateInfo createInfo = {0};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = pSwapChain->_swapchain_images_vk[i];

		// Treat the image as a standard 2D texture
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = pSwapChain->_format_vk; // Use the format chosen in chooseSurfaceFormat

		// Components allow you to remap color channels (swizzling)
		// VK_COMPONENT_SWIZZLE_IDENTITY means use the default mapping
		createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

		// SubresourceRange describes what the image's purpose is
		// and which parts of the image should be accessed
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;

		spudgpu_image_view_vulkan *view = &pSwapChain->_swapchain_image_views_vk[i];
		spudgpu_image_vulkan *img = calloc(1, sizeof(spudgpu_image_vulkan));
		img->_device = pSwapChain->_device;
		img->_image_vk = pSwapChain->_swapchain_images_vk[i];
		img->_format_vk = pSwapChain->_format_vk;
		view->_desc.parent_image = (spudgpu_image) img;
		result = vkCreateImageView(pSwapChain->_device._logical_device_vk, &createInfo, NULL,
		                           &pSwapChain->_swapchain_image_views_vk[i]._image_view_vk);
		if (result != VK_SUCCESS) {
			//throw std::runtime_error("failed to create image views!");
			return result;
		}


	}
	return result;
}


SPUDRESULT spudgpu_create_surface(
	spudgpu_instance instance,
	void *window_handle,
	void *display_handle,
	spudgpu_surface *out_surface) {
	if (!instance) return SPUDRESULT_GPU_INVALID_INSTANCE;
	if (!window_handle) return SPUDRESULT_GPU_INVALID_WINDOW_HANDLE;
#if !defined(SPUDLIB_PLATFORM_WIN32)
	// Only Wayland/X11 surfaces need the display connection; Win32 has none
	// (spudgpu.h: "Windows: unused (set to NULL)").
	if (!display_handle) return SPUDRESULT_GPU_INVALID_DISPLAY_HANDLE;
#endif
	if (!out_surface) return SPUDRESULT_NULL_OUTPUT_PARAMETER;

	spudgpu_surface_vulkan result = {0};
	result._instance = *instance;

	VkResult r = VK_SUCCESS;

#if defined(SPUDLIB_PLATFORM_WIN32)
	VkWin32SurfaceCreateInfoKHR ci = {0};
	ci.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	ci.hwnd = (HWND) window_handle;
	ci.hinstance = GetModuleHandle(NULL);
	r = vkCreateWin32SurfaceKHR(instance->_instance_vk, &ci, NULL, &result._surface_vk);

#elif defined(SPUDGPU_PLATFORM_WAYLAND)
	VkWaylandSurfaceCreateInfoKHR ci = {0};
	ci.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
	ci.display = (struct wl_display *) display_handle;
	ci.surface = (struct wl_surface *) window_handle;
	r = vkCreateWaylandSurfaceKHR(instance->_instance_vk, &ci, NULL, &result._surface_vk);

#elif defined(SPUDGPU_PLATFORM_XLIB)
	VkXlibSurfaceCreateInfoKHR ci = {0};
	ci.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
	ci.dpy = (Display *) display_handle;
	ci.window = (Window)(uintptr_t) window_handle;
	r = vkCreateXlibSurfaceKHR(instance->_instance_vk, &ci, NULL, &result._surface_vk);

#else
#error "No platform defined."
#endif

	if (r != VK_SUCCESS) return SPUDRESULT_API_SPECIFIC_FAILURE;

	spudgpu_surface_vulkan *pResult = malloc(sizeof(spudgpu_surface_vulkan));
	memcpy(pResult, &result, sizeof(spudgpu_surface_vulkan));
	*out_surface = pResult;
	return SPUD_SUCCESS;
}

void spudgpu_destroy_surface(spudgpu_surface surface) {
	if (!surface) return;
	vkDestroySurfaceKHR(
		surface->_instance._instance_vk,
		surface->_surface_vk, NULL);
#if _DEBUG
	free((void *)surface->_debug_name);
#endif
	free(surface);
}

spudgpu_surface spudgpu_create_surface_from_callback(
	spudgpu_instance instance,
	void *user_data,
	spudgpu_surface_create_fn create_fn) {
	if (!instance || !create_fn) return NULL;

	spudgpu_surface_vulkan *result = calloc(1, sizeof(spudgpu_surface_vulkan));
	if (!result) return NULL;

	result->_instance = *instance;

	bool ok = create_fn(
		(void *) instance->_instance_vk,
		user_data,
		(void *) &result->_surface_vk);

	if (!ok) {
		free(result);
		return NULL;
	}

	return result;
}


SPUDRESULT spudgpu_create_swap_chain(
	spudgpu_device device,
	const spudgpu_swap_chain_desc *desc,
	spudgpu_swap_chain *out_swap_chain) {
	if (!device) return SPUDRESULT_GPU_INVALID_DEVICE;
	if (!desc) return SPUDRESULT_NULL_DESC;
	if (!out_swap_chain) return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	if (!desc->surface) return SPUDRESULT_GPU_INVALID_SURFACE;
	if (!desc->queue) return SPUDRESULT_GPU_INVALID_COMMAND_QUEUE;

	spudgpu_swap_chain_vulkan result = {0};
	result._device = *((spudgpu_device_vulkan *) device);
	memcpy(&result._desc, desc, sizeof(spudgpu_swap_chain_desc));
	result._swapchain_images_count = result._desc.buffer_count;
	result._present_queue_vk = ((spudgpu_command_queue_vulkan *) desc->queue)->_queue_vk;

	if (desc->surface) {
		result._surface_vk = desc->surface->_surface_vk;
	}

	VkResult r = VK_SUCCESS;
	if (r != VK_SUCCESS) return SPUDRESULT_API_SPECIFIC_FAILURE;

	//result._desc = desc; Can't do this because of 'const'
	r = spudgpuvulkan___create_swapchain_internal(&result, desc->width, desc->height, NULL);
	if (r != VK_SUCCESS) return SPUDRESULT_API_SPECIFIC_FAILURE;

	r = spudgpuvulkan_create_image_views_internal(&result, result._swapchain_images_count);
	if (r != VK_SUCCESS) return SPUDRESULT_API_SPECIFIC_FAILURE;

	spudgpuvulkan___fences_semaphores_swapchain_creation_internal(&result);

	spudgpu_swap_chain_vulkan *pResult = malloc(sizeof(spudgpu_swap_chain_vulkan));
	memcpy(pResult, &result, sizeof(spudgpu_swap_chain_vulkan));
	*out_swap_chain = pResult;
	return SPUD_SUCCESS;
}

void spudgpu_destroy_swap_chain(spudgpu_swap_chain swap_chain) {
	if (!swap_chain) return;
	VkDevice dev = swap_chain->_device._logical_device_vk;

	for (uint32_t i = 0; i < swap_chain->_max_frames_in_flight; i++) {
		vkDestroySemaphore(dev, swap_chain->_image_available_semaphores[i]._semaphore_vk, NULL);
		vkDestroyFence(dev, swap_chain->_in_flight_fences_vk[i], NULL);
#if _DEBUG
		free((void *)swap_chain->_image_available_semaphores[i]._debug_name);
#endif
	}
	// render_finished_semaphores is sized/indexed by swapchain image count, not
	// _max_frames_in_flight — see spudgpuvulkan___fences_semaphores_swapchain_creation_internal.
	for (uint32_t i = 0; i < swap_chain->_swapchain_images_count; i++) {
		vkDestroySemaphore(dev, swap_chain->_render_finished_semaphores[i]._semaphore_vk, NULL);
#if _DEBUG
		free((void *)swap_chain->_render_finished_semaphores[i]._debug_name);
#endif
	}
	free(swap_chain->_image_available_semaphores);
	free(swap_chain->_render_finished_semaphores);
	free(swap_chain->_in_flight_fences_vk);

	for (uint32_t i = 0; i < swap_chain->_swapchain_image_views_count; i++) {
#if _DEBUG
		if (swap_chain->_swapchain_image_views_vk[i]._desc.parent_image)
			free((void *)swap_chain->_swapchain_image_views_vk[i]._desc.parent_image->_debug_name);
		free((void *)swap_chain->_swapchain_image_views_vk[i]._debug_name);
#endif
		free(swap_chain->_swapchain_image_views_vk[i]._desc.parent_image);
		vkDestroyImageView(dev, swap_chain->_swapchain_image_views_vk[i]._image_view_vk, NULL);
	}
	free(swap_chain->_swapchain_image_views_vk);
	free(swap_chain->_swapchain_images_vk);

	// The surface is owned by the caller — do not destroy it here.
	// The caller is responsible for calling spudgpu_destroy_surface separately.
	if (swap_chain->_swapchain_vk != VK_NULL_HANDLE)
		vkDestroySwapchainKHR(dev, swap_chain->_swapchain_vk, NULL);

#if _DEBUG
	free((void *)swap_chain->_debug_name);
#endif
	free(swap_chain);
}


SPUDRESULT spudgpu_get_swap_chain_desc(
	spudgpu_swap_chain swap_chain,
	spudgpu_swap_chain_desc *out_desc) {
	if (!swap_chain) return SPUDRESULT_GPU_INVALID_SWAP_CHAIN;
	if (!out_desc) return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	*out_desc = ((spudgpu_swap_chain_vulkan *) swap_chain)->_desc;
	return SPUD_SUCCESS;
}

uint32_t spudgpu_swap_chain_acquire_next_image(spudgpu_swap_chain swap_chain) {
	if (!swap_chain) return SPUD_UINT32_MAX;
	VkDevice device = swap_chain->_device._logical_device_vk;

	// Wait for the fence of the current frame slot, so we don't overwrite
	// GPU resources that are still in flight.
	vkWaitForFences(device, 1, &swap_chain->_in_flight_fences_vk[swap_chain->_current_frame],
	                VK_TRUE, SPUD_UINT64_MAX);

	// Ask the driver for the next available swapchain image.
	// Signals _image_available_semaphores[_current_frame] when the image is ready.
	VkResult result = vkAcquireNextImageKHR(
		device,
		swap_chain->_swapchain_vk,
		SPUD_UINT64_MAX, // No timeout — block until an image is available
		swap_chain->_image_available_semaphores[swap_chain->_current_frame]._semaphore_vk,
		VK_NULL_HANDLE,
		&swap_chain->_current_image_index);

	if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		// Caller should handle swap chain recreation on VK_ERROR_OUT_OF_DATE_KHR.
		// Do NOT reset the fence here: _current_frame only advances on a
		// successful present (see below), so a caller that bails out on a
		// failed acquire submits nothing that
		// would ever re-signal this fence. Resetting it unconditionally,
		// before knowing the acquire would succeed, left it permanently
		// unsignaled the moment a resize made the swap chain go out of
		// date — and the very next acquire's wait above would then block
		// forever on that same fence, freezing the render thread.
		return SPUD_UINT32_MAX;
	}

	// Only reset now that we know real work is about to be submitted for
	// this frame, which will eventually re-signal the fence.
	vkResetFences(device, 1, &swap_chain->_in_flight_fences_vk[swap_chain->_current_frame]);

	return swap_chain->_current_image_index;
}

void spudgpu_swap_chain_present(spudgpu_swap_chain swap_chain) {
	if (!swap_chain) return;

	// Tell the presentation engine to display _current_image_index.
	// Wait on _render_finished_semaphores[_current_image_index] — indexed by
	// swapchain image, not frame-in-flight slot, since this semaphore is
	// specifically what the present engine consumes for this image.
	VkSemaphore waitSemaphores[] = {
		swap_chain->_render_finished_semaphores[swap_chain->_current_image_index]._semaphore_vk
	};

	VkPresentInfoKHR presentInfo = {0};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = waitSemaphores;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swap_chain->_swapchain_vk;
	presentInfo.pImageIndices = &swap_chain->_current_image_index;
	presentInfo.pResults = NULL; // Optional per-swapchain result

	// Present against the queue given as spudgpu_swap_chain_desc::queue at
	// creation, not a freshly re-derived "the graphics queue" guess - the
	// caller is required to have submitted this frame's rendering there too.
	vkQueuePresentKHR(swap_chain->_present_queue_vk, &presentInfo);

	// Advance the frame-in-flight cursor
	swap_chain->_current_frame = (swap_chain->_current_frame + 1) % swap_chain->_max_frames_in_flight;
}

spudgpu_image_view spudgpu_get_swap_chain_image_view(
	spudgpu_swap_chain swap_chain,
	uint32_t image_index) {
	if (!swap_chain) return NULL;
	else if (image_index >= swap_chain->_swapchain_image_views_count) return NULL;
	else return &swap_chain->_swapchain_image_views_vk[image_index];
}

spudgpu_semaphore spudgpu_swap_chain_get_image_available_semaphore(spudgpu_swap_chain swap_chain) {
	if (!swap_chain) return NULL;
	else return &swap_chain->_image_available_semaphores[swap_chain->_current_frame];
}

spudgpu_semaphore spudgpu_swap_chain_get_render_finished_semaphore(spudgpu_swap_chain swap_chain) {
	if (!swap_chain) return NULL;
	// Indexed by swapchain image, not frame-in-flight slot — see spudgpu_swap_chain_present.
	else return &swap_chain->_render_finished_semaphores[swap_chain->_current_image_index];
}


#if __cplusplus
}
#endif

#endif // SPUDGPU_COMPILE_VULKAN_API
