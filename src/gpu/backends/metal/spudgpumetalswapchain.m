//
// SpudGPU Metal backend - swap chain (CAMetalLayer/drawables).
// spudgpu_create_surface takes a CAMetalLayer already attached to the
// platform view by the caller (see spudgpu_sdl3.h's Metal branch) and
// stores it directly - it does not create or attach one itself.
// spudgpu_create_surface_from_callback stays unimplemented: it is a Vulkan-
// only interop escape hatch (VkInstance-typed callback) Metal has no
// equivalent for. The swap chain itself is implemented and verified against
// real Apple hardware.
//

#if SPUDGPU_COMPILE_METAL_API

#include "spudgpu.h"
#include "spudcore.h"
#include "spudgpumetal.h"
#include <Metal/Metal.h>
#include "../../../spudstructbase.h"

// The layer's contentsGravity for [scaling], which has passed
// spudgpu___internal_check_swap_chain_desc. Core Animation names the
// vertical gravities by the layer's own y axis: where its contents are
// flipped (an iOS layer, or one in a flipped view), kCAGravityBottomLeft is
// the top-left corner on screen. Read once, here: a layer moved later into
// a hierarchy that flips it keeps the gravity set now.
static CALayerContentsGravity spudgpumetal___contents_gravity(CAMetalLayer *layer, SPUDGPU_PRESENT_SCALING scaling) {
	switch (scaling) {
	case SPUDGPU_PRESENT_SCALING_ONE_TO_ONE:
		return [layer contentsAreFlipped] ? kCAGravityBottomLeft : kCAGravityTopLeft;
	case SPUDGPU_PRESENT_SCALING_ASPECT_FIT:
		return kCAGravityResizeAspect;
	case SPUDGPU_PRESENT_SCALING_STRETCH:
	default:
		return kCAGravityResize;
	}
}

static void spudgpumetal___internal_destroy_swap_chain(spudgpu_swap_chain_metal *swap_chain_metal) {
	if (!swap_chain_metal)
		return;
	// Everything below was made through the swap chain's allocator.
	const SPUD_ALLOCATOR allocator = swap_chain_metal->head.allocator;
	if (swap_chain_metal->_current_drawable) {
		[swap_chain_metal->_current_drawable release];
	}
	if (swap_chain_metal->_image_available_semaphore) {
		if (swap_chain_metal->_image_available_semaphore->_event_mtl) {
			[swap_chain_metal->_image_available_semaphore->_event_mtl release];
		}
		spud_string_free(&allocator, swap_chain_metal->_image_available_semaphore->head.debug_name);
		SPUD_DELETE(&allocator, spudgpu_semaphore_metal, swap_chain_metal->_image_available_semaphore);
	}
	if (swap_chain_metal->_render_finished_semaphore) {
		if (swap_chain_metal->_render_finished_semaphore->_event_mtl) {
			[swap_chain_metal->_render_finished_semaphore->_event_mtl release];
		}
		spud_string_free(&allocator, swap_chain_metal->_render_finished_semaphore->head.debug_name);
		SPUD_DELETE(&allocator, spudgpu_semaphore_metal, swap_chain_metal->_render_finished_semaphore);
	}
	if (swap_chain_metal->_in_flight_fence) {
		if (swap_chain_metal->_in_flight_fence->_shared_event_mtl) {
			[swap_chain_metal->_in_flight_fence->_shared_event_mtl release];
		}
		spud_string_free(&allocator, swap_chain_metal->_in_flight_fence->head.debug_name);
		SPUD_DELETE(&allocator, spudgpu_fence_metal, swap_chain_metal->_in_flight_fence);
	}
	if (swap_chain_metal->_back_buffer_images)
		spud_string_free(&allocator, swap_chain_metal->_back_buffer_images->head.debug_name);
	if (swap_chain_metal->_back_buffer_image_views)
		spud_string_free(&allocator, swap_chain_metal->_back_buffer_image_views->head.debug_name);
	SPUD_DELETE_ARRAY(
	    &allocator, spudgpu_image_metal, swap_chain_metal->_back_buffer_images, swap_chain_metal->_desc.buffer_count);
	SPUD_DELETE_ARRAY(
	    &allocator, spudgpu_image_view_metal, swap_chain_metal->_back_buffer_image_views,
	    swap_chain_metal->_desc.buffer_count);
	spud_string_free(&allocator, swap_chain_metal->head.debug_name);
	SPUD_DELETE(&allocator, spudgpu_swap_chain_metal, swap_chain_metal);
}

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

	SPUDRESULT sr = SPUD_SUCCESS;

	spudgpu_surface_metal *surface_metal = SPUD_NEW_ZEROED(&instance->head.allocator, spudgpu_surface_metal);
	if (!surface_metal) {
		sr = SPUDRESULT_GENERAL_FAILURE;
		goto failedattempt;
	}
	surface_metal->head.allocator   = instance->head.allocator;
	surface_metal->_parent_instance = (spudgpu_instance_metal *)instance;
	surface_metal->_window_handle   = window_handle;
	surface_metal->_display_handle  = display_handle;

	// window_handle is the CAMetalLayer itself (display_handle is unused on
	// Apple platforms) - spudgpu_sdl3.h's Metal branch gets one from SDL via
	// SDL_Metal_GetLayer() and passes it straight through.
	surface_metal->_metal_layer = (CAMetalLayer *)window_handle;

	*out_surface = (spudgpu_surface)surface_metal;

	return sr;

failedattempt:
	SPUD_DELETE(&instance->head.allocator, spudgpu_surface_metal, surface_metal);
	return sr;
}

void spudgpu_destroy_surface(spudgpu_surface surface) {
	spudgpu_surface_metal *surface_metal = (spudgpu_surface_metal *)surface;
	if (surface_metal) {
		const SPUD_ALLOCATOR allocator = surface_metal->head.allocator;
		spud_string_free(&allocator, surface_metal->head.debug_name);
		SPUD_DELETE(&allocator, spudgpu_surface_metal, surface_metal);
	}
}

spudgpu_surface spudgpu_create_surface_from_callback(
    spudgpu_instance instance,
    void *user_data,
    spudgpu_surface_create_fn create_fn) {
	if (!instance || !create_fn)
		return NULL;

	// METAL API CODE
	// This callback shape (create_fn receiving a VkInstance-typed void*) is a
	// Vulkan interop escape hatch - Metal has no equivalent instance object to
	// hand back (see spudgpu_metal_natives.h), so this stays unimplemented on
	// this backend rather than fabricating an unused parameter.

	return NULL;
}

SPUDRESULT spudgpu_get_surface_present_scalings(
    spudgpu_device device,
    spudgpu_surface surface,
    uint32_t *out_scalings) {
	if (!device)
		return SPUDRESULT_GPU_INVALID_DEVICE;
	const SPUDRESULT sr = spudgpu___internal_check_surface_present_scalings(device->_features, surface, out_scalings);
	if (sr != SPUD_SUCCESS)
		return sr;
	// contentsGravity has all three on every CAMetalLayer.
	*out_scalings = SPUDGPU_PRESENT_SCALING_ONE_TO_ONE | SPUDGPU_PRESENT_SCALING_STRETCH | SPUDGPU_PRESENT_SCALING_ASPECT_FIT;
	return SPUD_SUCCESS;
}

SPUDRESULT spudgpu_create_swap_chain(
    spudgpu_device device,
    const spudgpu_swap_chain_desc *desc,
    spudgpu_swap_chain *out_swap_chain) {
	if (!device)
		return SPUDRESULT_GPU_INVALID_DEVICE;
	const SPUDRESULT desc_sr = spudgpu___internal_check_swap_chain_desc(device->_features, desc, out_swap_chain);
	if (desc_sr != SPUD_SUCCESS)
		return desc_sr;
	// Metal has no stable, enumerable N-image array the way Vulkan/D3D12 do -
	// CAMetalLayer hands out one drawable at a time from an internal pool, so
	// there is exactly one addressable image at any moment. Reject anything
	// else rather than silently reinterpreting the caller's request.
	if (desc->buffer_count != 1)
		return SPUDRESULT_DESC_INVALID_PARAMETERS;
	// MAILBOX has no CAMetalLayer equivalent - there is no "replace the
	// queued frame instead of blocking" toggle distinct from vsync. FIFO and
	// IMMEDIATE are both accepted but not distinguished from one another on
	// this backend (no displaySyncEnabled wiring in this pass).
	if (desc->present_mode == SPUDGPU_PRESENT_MODE_MAILBOX)
		return SPUDRESULT_DESC_INVALID_PARAMETERS;

	spudgpu_surface_metal *surface_metal = (spudgpu_surface_metal *)desc->surface;
	if (!surface_metal->_metal_layer)
		return SPUDRESULT_GPU_INVALID_SURFACE;
	// Every layer takes all three (spudgpu_get_surface_present_scalings), so
	// none is refused with SPUDRESULT_GPU_PRESENT_SCALING_NOT_SUPPORTED here.

	// The format table's presentable formats: the ones a CAMetalLayer takes.
	const spudgpumetal_format_info *format_info = spudgpumetal___format_info(desc->format);
	if (!format_info->presentable)
		return SPUDRESULT_DESC_INVALID_PARAMETERS;
	MTLPixelFormat pixel_format = format_info->pixel;

	SPUDRESULT sr = SPUD_SUCCESS;

	spudgpu_swap_chain_metal *swap_chain_metal = SPUD_NEW_ZEROED(&device->head.allocator, spudgpu_swap_chain_metal);
	if (!swap_chain_metal) {
		sr = SPUDRESULT_GENERAL_FAILURE;
		goto failedattempt;
	}
	swap_chain_metal->head.allocator    = device->head.allocator;
	swap_chain_metal->_parent_device    = (spudgpu_device_metal *)device;
	swap_chain_metal->_desc             = *desc;
	swap_chain_metal->_desc.buffer_count = 1; // Truthful, not the caller's raw request - see above.
	swap_chain_metal->_metal_layer      = surface_metal->_metal_layer;
	swap_chain_metal->_present_queue    = (spudgpu_command_queue_metal *)desc->queue;

	swap_chain_metal->_metal_layer.device          = swap_chain_metal->_parent_device->_device_mtl;
	swap_chain_metal->_metal_layer.pixelFormat     = pixel_format;
	swap_chain_metal->_metal_layer.drawableSize    = CGSizeMake(desc->width, desc->height);
	// framebufferOnly drawables can only be rendered into; any other usage
	// (blit source/destination, sampling, storage) needs it off.
	swap_chain_metal->_metal_layer.framebufferOnly = desc->usage == 0 ? YES : NO;
	// How a drawable of another size than the layer's bounds is shown. No
	// magnificationFilter: no filter is promised on any backend.
	[CATransaction begin];
	[CATransaction setDisableActions:YES];
	swap_chain_metal->_metal_layer.contentsGravity =
	    spudgpumetal___contents_gravity(swap_chain_metal->_metal_layer, desc->present_scaling);
	[CATransaction commit];

	// _desc.buffer_count (1) long each, the count they are freed with.
	swap_chain_metal->_back_buffer_images =
	    SPUD_NEW_ARRAY_ZEROED(&swap_chain_metal->head.allocator, spudgpu_image_metal, swap_chain_metal->_desc.buffer_count);
	swap_chain_metal->_back_buffer_image_views = SPUD_NEW_ARRAY_ZEROED(
	    &swap_chain_metal->head.allocator, spudgpu_image_view_metal, swap_chain_metal->_desc.buffer_count);
	if (!swap_chain_metal->_back_buffer_images || !swap_chain_metal->_back_buffer_image_views) {
		sr = SPUDRESULT_GENERAL_FAILURE;
		goto failedattempt;
	}
	swap_chain_metal->_back_buffer_images[0].head.allocator      = swap_chain_metal->head.allocator;
	swap_chain_metal->_back_buffer_image_views[0].head.allocator = swap_chain_metal->head.allocator;
	swap_chain_metal->_back_buffer_images[0]._parent_device      = swap_chain_metal->_parent_device;
	swap_chain_metal->_back_buffer_images[0]._desc.usage          = SPUDGPU_IMAGE_USAGE_COLOR_ATTACHMENT | desc->usage;
	swap_chain_metal->_back_buffer_images[0]._desc.format          = desc->format;
	swap_chain_metal->_back_buffer_images[0]._desc.width           = desc->width;
	swap_chain_metal->_back_buffer_images[0]._desc.height          = desc->height;
	swap_chain_metal->_back_buffer_images[0]._desc.array_layers    = 1;
	swap_chain_metal->_back_buffer_images[0]._desc.mip_levels      = 1;
	swap_chain_metal->_back_buffer_image_views[0]._parent_image   = &swap_chain_metal->_back_buffer_images[0];
	swap_chain_metal->_back_buffer_image_views[0]._parent_device  = swap_chain_metal->_parent_device;
	// What spudgpu_get_image_view_desc reports for the back buffer's view -
	// the same as the D3D12 backend's, so a caller can get from the view to
	// the image it copies into.
	swap_chain_metal->_back_buffer_image_views[0]._desc.type         = SPUDGPU_IMAGE_VIEW_TYPE_2D;
	swap_chain_metal->_back_buffer_image_views[0]._desc.parent_image = (spudgpu_image)&swap_chain_metal->_back_buffer_images[0];

	// image_available: attach it to the struct before doing anything that
	// can fail, so spudgpumetal___internal_destroy_swap_chain always finds
	// and cleans it up regardless of where creation stops.
	swap_chain_metal->_image_available_semaphore = SPUD_NEW_ZEROED(&swap_chain_metal->head.allocator, spudgpu_semaphore_metal);
	if (!swap_chain_metal->_image_available_semaphore) {
		sr = SPUDRESULT_GENERAL_FAILURE;
		goto failedattempt;
	}
	swap_chain_metal->_image_available_semaphore->head.allocator = swap_chain_metal->head.allocator;
	swap_chain_metal->_image_available_semaphore->_parent_device = swap_chain_metal->_parent_device;
	swap_chain_metal->_image_available_semaphore->_event_mtl =
	    [swap_chain_metal->_parent_device->_device_mtl newEvent];
	if (!swap_chain_metal->_image_available_semaphore->_event_mtl) {
		sr = SPUDRESULT_API_SPECIFIC_FAILURE;
		goto failedattempt;
	}
	// Signal it exactly once, right now, and never touch it again - Metal's
	// nextDrawable already blocks until an image is genuinely available
	// before spudgpu_swap_chain_acquire_next_image returns, so every
	// subsequent wait against this value is, and should be, a permanent
	// no-op. Blocking briefly here at creation is fine; this never runs
	// per-frame.
	swap_chain_metal->_image_available_semaphore->_signal_value = 1;
	{
		id<MTLCommandBuffer> init_buffer =
		    [swap_chain_metal->_present_queue->_command_queue_mtl commandBuffer];
		[init_buffer encodeSignalEvent:swap_chain_metal->_image_available_semaphore->_event_mtl value:1];
		[init_buffer commit];
		[init_buffer waitUntilCompleted];
	}

	// render_finished / in_flight: real, usable objects (a caller can signal
	// render_finished via spudgpu_queue_submit same as on Vulkan/D3D12), but
	// spudgpu_swap_chain_present below never consumes either of them itself -
	// see spudgpu_swap_chain_metal's struct comment for why.
	swap_chain_metal->_render_finished_semaphore = SPUD_NEW_ZEROED(&swap_chain_metal->head.allocator, spudgpu_semaphore_metal);
	if (!swap_chain_metal->_render_finished_semaphore) {
		sr = SPUDRESULT_GENERAL_FAILURE;
		goto failedattempt;
	}
	swap_chain_metal->_render_finished_semaphore->head.allocator = swap_chain_metal->head.allocator;
	swap_chain_metal->_render_finished_semaphore->_parent_device = swap_chain_metal->_parent_device;
	swap_chain_metal->_render_finished_semaphore->_event_mtl =
	    [swap_chain_metal->_parent_device->_device_mtl newEvent];
	if (!swap_chain_metal->_render_finished_semaphore->_event_mtl) {
		sr = SPUDRESULT_API_SPECIFIC_FAILURE;
		goto failedattempt;
	}

	swap_chain_metal->_in_flight_fence = SPUD_NEW_ZEROED(&swap_chain_metal->head.allocator, spudgpu_fence_metal);
	if (!swap_chain_metal->_in_flight_fence) {
		sr = SPUDRESULT_GENERAL_FAILURE;
		goto failedattempt;
	}
	swap_chain_metal->_in_flight_fence->head.allocator = swap_chain_metal->head.allocator;
	swap_chain_metal->_in_flight_fence->_parent_device = swap_chain_metal->_parent_device;
	swap_chain_metal->_in_flight_fence->_shared_event_mtl =
	    [swap_chain_metal->_parent_device->_device_mtl newSharedEvent];
	if (!swap_chain_metal->_in_flight_fence->_shared_event_mtl) {
		sr = SPUDRESULT_API_SPECIFIC_FAILURE;
		goto failedattempt;
	}

	SPUDGPU___LIVE_ADD(&swap_chain_metal->_parent_device->_live, SPUDGPU___OBJECT_SWAP_CHAIN);
	*out_swap_chain = (spudgpu_swap_chain)swap_chain_metal;

	return sr;

failedattempt:
	spudgpumetal___internal_destroy_swap_chain(swap_chain_metal);
	return sr;
}

void spudgpu_destroy_swap_chain(spudgpu_swap_chain swap_chain) {
	if (!swap_chain)
		return;
	SPUDGPU___LIVE_REMOVE(&((spudgpu_swap_chain_metal *)swap_chain)->_parent_device->_live, SPUDGPU___OBJECT_SWAP_CHAIN);
	spudgpumetal___internal_destroy_swap_chain((spudgpu_swap_chain_metal *)swap_chain);
}

SPUDRESULT spudgpu_get_swap_chain_desc(
    spudgpu_swap_chain swap_chain,
    spudgpu_swap_chain_desc *out_desc) {
	spudgpu_swap_chain_metal *swap_chain_metal = (spudgpu_swap_chain_metal *)swap_chain;
	if (!swap_chain_metal)
		return SPUDRESULT_GPU_INVALID_SWAP_CHAIN;
	if (!out_desc)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	*out_desc = swap_chain_metal->_desc;
	out_desc->base = spud_struct_base(SPUD_STRUCT_TYPE_GPU_SWAP_CHAIN_DESC);
	return SPUD_SUCCESS;
}

uint32_t spudgpu_swap_chain_acquire_next_image(spudgpu_swap_chain swap_chain) {
	spudgpu_swap_chain_metal *swap_chain_metal = (spudgpu_swap_chain_metal *)swap_chain;
	if (!swap_chain_metal)
		return SPUD_UINT32_MAX;

	// Blocks until a drawable is genuinely available - this is exactly what
	// makes _image_available_semaphore a permanent no-op wait everywhere
	// else in this backend (see spudgpu_create_swap_chain).
	id<CAMetalDrawable> drawable = [swap_chain_metal->_metal_layer nextDrawable];
	if (!drawable)
		return 0;

	// -nextDrawable returns an autoreleased object (unlike -newCommandQueue/
	// -newEvent elsewhere in this backend) - retain it to hold it past
	// acquire, and drop whatever the previous frame was holding.
	if (swap_chain_metal->_current_drawable) {
		[swap_chain_metal->_current_drawable release];
	}
	swap_chain_metal->_current_drawable = [drawable retain];

	id<MTLTexture> texture                                       = drawable.texture;
	swap_chain_metal->_back_buffer_images[0]._texture_mtl         = texture;
	swap_chain_metal->_back_buffer_image_views[0]._texture_view_mtl = texture;

	// The size this drawable really is, which spudgpu_get_swap_chain_desc
	// reports (as on Vulkan, where the surface can dictate it): the layer
	// sizes its drawables to its bounds when those change, which can be
	// before the caller has recreated the swap chain at the new size.
	swap_chain_metal->_desc.width                         = (uint32_t)texture.width;
	swap_chain_metal->_desc.height                        = (uint32_t)texture.height;
	swap_chain_metal->_back_buffer_images[0]._desc.width  = (uint32_t)texture.width;
	swap_chain_metal->_back_buffer_images[0]._desc.height = (uint32_t)texture.height;

	// Always 0: there is exactly one addressable image at a time on Metal,
	// not a stable N-image array to index into - see
	// spudgpu_swap_chain_metal's struct comment.
	return 0;
}

void spudgpu_swap_chain_present(spudgpu_swap_chain swap_chain) {
	spudgpu_swap_chain_metal *swap_chain_metal = (spudgpu_swap_chain_metal *)swap_chain;
	if (!swap_chain_metal)
		return;
	if (!swap_chain_metal->_current_drawable)
		return;

	// No explicit wait needed: this buffer is committed to
	// _desc.queue - the same queue the caller was required to render this
	// frame's work on - and Metal executes command buffers on one queue
	// strictly in commit order (the same guarantee spudgpu_queue_wait_idle
	// relies on). So this buffer, and the presentDrawable: it carries,
	// cannot begin executing until the caller's rendering has completed.
	// Mirrors the D3D12 backend, which also does zero explicit
	// synchronization in its own present() for the same reason.
	// A use of the swap chain's queue (spudgpu.h).
	spudgpumetal___queue_enter(swap_chain_metal->_present_queue);
	id<MTLCommandBuffer> present_buffer =
	    [swap_chain_metal->_present_queue->_command_queue_mtl commandBuffer];
	[present_buffer presentDrawable:swap_chain_metal->_current_drawable];
	[present_buffer commit];
	spudgpumetal___queue_leave(swap_chain_metal->_present_queue);
}

spudgpu_image_view spudgpu_get_swap_chain_image_view(
    spudgpu_swap_chain swap_chain,
    uint32_t image_index) {
	spudgpu_swap_chain_metal *swap_chain_metal = (spudgpu_swap_chain_metal *)swap_chain;
	if (!swap_chain_metal)
		return NULL;
	if (image_index >= swap_chain_metal->_desc.buffer_count)
		return NULL;
	if (!swap_chain_metal->_back_buffer_image_views)
		return NULL;
	return (spudgpu_image_view)&swap_chain_metal->_back_buffer_image_views[image_index];
}

spudgpu_semaphore spudgpu_swap_chain_get_image_available_semaphore(spudgpu_swap_chain swap_chain) {
	spudgpu_swap_chain_metal *swap_chain_metal = (spudgpu_swap_chain_metal *)swap_chain;
	if (!swap_chain_metal)
		return NULL;
	return (spudgpu_semaphore)swap_chain_metal->_image_available_semaphore;
}

spudgpu_semaphore spudgpu_swap_chain_get_render_finished_semaphore(spudgpu_swap_chain swap_chain) {
	spudgpu_swap_chain_metal *swap_chain_metal = (spudgpu_swap_chain_metal *)swap_chain;
	if (!swap_chain_metal)
		return NULL;
	return (spudgpu_semaphore)swap_chain_metal->_render_finished_semaphore;
}

#endif // SPUDGPU_COMPILE_METAL_API
