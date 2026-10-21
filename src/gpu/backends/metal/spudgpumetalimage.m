//
// SpudGPU Metal backend - images/textures and texture views.
// Implemented and verified against real Apple hardware.
//

#if SPUDGPU_COMPILE_METAL_API

#include "spudcore.h"
#include "spudgpu.h"
#include "spudgpumetal.h"
#include <Metal/Metal.h>
#include "../../../spudstructbase.h"

// A practical, non-exhaustive subset covering the common uncompressed
// formats - not the full ~100-entry DXGI-shaped SPUDGPU_FORMAT enum (block-
// compressed BC formats and the many *_TYPELESS variants have no caller in
// this workspace yet). TYPELESS formats are intentionally never mapped:
// Metal has no typeless-resource concept the way D3D12 does - every texture
// needs one concrete MTLPixelFormat at creation - so a typeless request is
// rejected with SPUDRESULT_GPU_INVALID_FORMAT rather than silently picking
// an arbitrary concrete interpretation the caller didn't ask for.
// Not static: spudgpumetalshader.m reuses this for render/depth attachment
// pixel formats, which must agree exactly with what an actual
// spudgpu_create_image call for the same SPUDGPU_FORMAT would produce.
MTLPixelFormat spudgpumetal___internal_image_pixel_format(SPUDGPU_FORMAT format) {
	switch (format) {
	// 8-bit per channel
	case SPUDGPU_FORMAT_R8_UNORM:
		return MTLPixelFormatR8Unorm;
	case SPUDGPU_FORMAT_R8_SNORM:
		return MTLPixelFormatR8Snorm;
	case SPUDGPU_FORMAT_R8_UINT:
		return MTLPixelFormatR8Uint;
	case SPUDGPU_FORMAT_R8_SINT:
		return MTLPixelFormatR8Sint;
	case SPUDGPU_FORMAT_R8G8_UNORM:
		return MTLPixelFormatRG8Unorm;
	case SPUDGPU_FORMAT_R8G8_SNORM:
		return MTLPixelFormatRG8Snorm;
	case SPUDGPU_FORMAT_R8G8_UINT:
		return MTLPixelFormatRG8Uint;
	case SPUDGPU_FORMAT_R8G8_SINT:
		return MTLPixelFormatRG8Sint;
	case SPUDGPU_FORMAT_R8G8B8A8_UNORM:
		return MTLPixelFormatRGBA8Unorm;
	case SPUDGPU_FORMAT_R8G8B8A8_UNORM_SRGB:
		return MTLPixelFormatRGBA8Unorm_sRGB;
	case SPUDGPU_FORMAT_R8G8B8A8_SNORM:
		return MTLPixelFormatRGBA8Snorm;
	case SPUDGPU_FORMAT_R8G8B8A8_UINT:
		return MTLPixelFormatRGBA8Uint;
	case SPUDGPU_FORMAT_R8G8B8A8_SINT:
		return MTLPixelFormatRGBA8Sint;
	case SPUDGPU_FORMAT_B8G8R8A8_UNORM:
		return MTLPixelFormatBGRA8Unorm;
	case SPUDGPU_FORMAT_B8G8R8A8_UNORM_SRGB:
		return MTLPixelFormatBGRA8Unorm_sRGB;

	// 16-bit per channel
	case SPUDGPU_FORMAT_R16_FLOAT:
		return MTLPixelFormatR16Float;
	case SPUDGPU_FORMAT_R16_UNORM:
		return MTLPixelFormatR16Unorm;
	case SPUDGPU_FORMAT_R16_SNORM:
		return MTLPixelFormatR16Snorm;
	case SPUDGPU_FORMAT_R16_UINT:
		return MTLPixelFormatR16Uint;
	case SPUDGPU_FORMAT_R16_SINT:
		return MTLPixelFormatR16Sint;
	case SPUDGPU_FORMAT_R16G16_FLOAT:
		return MTLPixelFormatRG16Float;
	case SPUDGPU_FORMAT_R16G16_UNORM:
		return MTLPixelFormatRG16Unorm;
	case SPUDGPU_FORMAT_R16G16_SNORM:
		return MTLPixelFormatRG16Snorm;
	case SPUDGPU_FORMAT_R16G16_UINT:
		return MTLPixelFormatRG16Uint;
	case SPUDGPU_FORMAT_R16G16_SINT:
		return MTLPixelFormatRG16Sint;
	case SPUDGPU_FORMAT_R16G16B16A16_FLOAT:
		return MTLPixelFormatRGBA16Float;
	case SPUDGPU_FORMAT_R16G16B16A16_UNORM:
		return MTLPixelFormatRGBA16Unorm;
	case SPUDGPU_FORMAT_R16G16B16A16_SNORM:
		return MTLPixelFormatRGBA16Snorm;
	case SPUDGPU_FORMAT_R16G16B16A16_UINT:
		return MTLPixelFormatRGBA16Uint;
	case SPUDGPU_FORMAT_R16G16B16A16_SINT:
		return MTLPixelFormatRGBA16Sint;

	// 32-bit per channel - no 3-component mapping: Metal has no RGB32
	// pixel format (only 1/2/4-component 32-bit formats exist).
	case SPUDGPU_FORMAT_R32_FLOAT:
		return MTLPixelFormatR32Float;
	case SPUDGPU_FORMAT_R32_UINT:
		return MTLPixelFormatR32Uint;
	case SPUDGPU_FORMAT_R32_SINT:
		return MTLPixelFormatR32Sint;
	case SPUDGPU_FORMAT_R32G32_FLOAT:
		return MTLPixelFormatRG32Float;
	case SPUDGPU_FORMAT_R32G32_UINT:
		return MTLPixelFormatRG32Uint;
	case SPUDGPU_FORMAT_R32G32_SINT:
		return MTLPixelFormatRG32Sint;
	case SPUDGPU_FORMAT_R32G32B32A32_FLOAT:
		return MTLPixelFormatRGBA32Float;
	case SPUDGPU_FORMAT_R32G32B32A32_UINT:
		return MTLPixelFormatRGBA32Uint;
	case SPUDGPU_FORMAT_R32G32B32A32_SINT:
		return MTLPixelFormatRGBA32Sint;

	// Packed
	case SPUDGPU_FORMAT_R10G10B10A2_UNORM:
		return MTLPixelFormatRGB10A2Unorm;
	case SPUDGPU_FORMAT_R10G10B10A2_UINT:
		return MTLPixelFormatRGB10A2Uint;
	case SPUDGPU_FORMAT_R11G11B10_FLOAT:
		return MTLPixelFormatRG11B10Float;

	// Depth/stencil - SPUDGPU_FORMAT_D24_UNORM_S8_UINT is intentionally not
	// mapped: MTLPixelFormatDepth24Unorm_Stencil8 is an Intel/AMD Mac-only
	// feature set value, unavailable on Apple Silicon, which is this
	// backend's only real target - there is no honest way to hand back a
	// 24-bit-depth resource here, only the 32-bit-depth
	// Depth32Float_Stencil8 that SPUDGPU_FORMAT_D32_FLOAT_S8X24_UINT below
	// already covers.
	case SPUDGPU_FORMAT_D16_UNORM:
		return MTLPixelFormatDepth16Unorm;
	case SPUDGPU_FORMAT_D32_FLOAT:
		return MTLPixelFormatDepth32Float;
	case SPUDGPU_FORMAT_D32_FLOAT_S8X24_UINT:
		return MTLPixelFormatDepth32Float_Stencil8;

	default:
		return MTLPixelFormatInvalid;
	}
}

static MTLTextureUsage spudgpumetal___internal_image_texture_usage(SPUDGPU_IMAGE_USAGE usage) {
	MTLTextureUsage result = MTLTextureUsageUnknown;
	if (usage & SPUDGPU_IMAGE_USAGE_SAMPLED)
		result |= MTLTextureUsageShaderRead;
	if (usage & (SPUDGPU_IMAGE_USAGE_COLOR_ATTACHMENT | SPUDGPU_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT))
		result |= MTLTextureUsageRenderTarget;
	if (usage & SPUDGPU_IMAGE_USAGE_STORAGE)
		result |= (MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite);
	// TRANSFER_SRC/TRANSFER_DST/PRESENTABLE need no MTLTextureUsage bit -
	// blit-encoder copies and drawable presentation both work regardless of
	// usage.

	// A transient attachment is only ever touched through the render pass
	// attachment mechanism itself (in-tile-memory programmable blending),
	// never through ordinary shader resource binding or a differently-
	// formatted view - spudgpu_create_image already rejects combining this
	// with SAMPLED/STORAGE/etc, so RenderTarget (set above) is genuinely the
	// only usage bit that ever applies, and MTLTextureUsagePixelFormatView
	// below doesn't.
	if (usage & SPUDGPU_IMAGE_USAGE_TRANSIENT_ATTACHMENT)
		return result;

	// Always allow format-reinterpreting views. SPUDGPU_IMAGE_USAGE has no
	// flag for "I will need a differently-formatted (or stencil-aspect)
	// view of this image later" for a caller to opt into, and
	// -newTextureViewWithPixelFormat:... fails outright on a texture that
	// wasn't created with this usage - there is no way to add it after the
	// fact. Setting it unconditionally is the only choice that doesn't
	// silently foreclose spudgpu_create_image_view ever reinterpreting this
	// image's format down the line; the cost is only a possible loss of a
	// GPU-internal lossless compression optimization on some formats, which
	// SPUDGPU never promises anywhere in its contract.
	result |= MTLTextureUsagePixelFormatView;
	return result;
}

static MTLTextureType spudgpumetal___internal_image_texture_type(
    SPUDGPU_IMAGE_TYPE type,
    uint32_t array_layers) {
	if (type & SPUDGPU_IMAGE_TYPE_3D)
		return MTLTextureType3D; // Metal has no 3D texture arrays.
	if (type & SPUDGPU_IMAGE_TYPE_1D)
		return (array_layers > 1) ? MTLTextureType1DArray : MTLTextureType1D;
	// Default/SPUDGPU_IMAGE_TYPE_2D. Cube maps have no dedicated
	// SPUDGPU_IMAGE_TYPE value - like Vulkan, they're a 2D array of 6 (or
	// 6*N) layers, reinterpreted as SPUDGPU_IMAGE_VIEW_TYPE_CUBE via
	// spudgpu_create_image_view - so the base texture is created as a plain
	// (array) 2D texture regardless, cube_capable or not: a 2D array
	// texture can be viewed as a cube or a cube array, and as single 2D
	// layers to render its faces.
	return (array_layers > 1) ? MTLTextureType2DArray : MTLTextureType2D;
}

SPUDRESULT spudgpumetal___check_image_desc(const spudgpu_image_desc *desc) {
	if (!desc->width || !desc->height || !desc->depth || !desc->array_layers || !desc->mip_levels)
		return SPUDRESULT_DESC_INVALID_PARAMETERS;
	if (desc->usage == SPUDGPU_IMAGE_USAGE_NONE)
		return SPUDRESULT_GPU_INVALID_IMAGE_USAGE;
	// SPUDGPU_IMAGE_USAGE_TRANSIENT_ATTACHMENT's contract (see spudgpu.h) is
	// enforced identically on every backend, not just the ones that act on
	// it, so an invalid desc doesn't silently behave differently depending
	// on which backend happens to be compiled in.
	if (desc->usage & SPUDGPU_IMAGE_USAGE_TRANSIENT_ATTACHMENT) {
		if (!(desc->usage & (SPUDGPU_IMAGE_USAGE_COLOR_ATTACHMENT | SPUDGPU_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT)))
			return SPUDRESULT_GPU_INVALID_IMAGE_USAGE;
		if (desc->usage & (SPUDGPU_IMAGE_USAGE_SAMPLED | SPUDGPU_IMAGE_USAGE_STORAGE |
		                   SPUDGPU_IMAGE_USAGE_TRANSFER_SRC | SPUDGPU_IMAGE_USAGE_TRANSFER_DST |
		                   SPUDGPU_IMAGE_USAGE_PRESENTABLE))
			return SPUDRESULT_GPU_INVALID_IMAGE_USAGE;
	}
	// spudgpu_image_desc::cube_capable's contract (see spudgpu.h) is the
	// same on every backend, though only Vulkan acts on it at creation.
	if (desc->cube_capable) {
		if (desc->type != SPUDGPU_IMAGE_TYPE_2D)
			return SPUDRESULT_GPU_INVALID_IMAGE_TYPE;
		if (desc->width != desc->height || desc->array_layers % 6 != 0)
			return SPUDRESULT_DESC_INVALID_PARAMETERS;
	}
	if (spudgpumetal___internal_image_pixel_format(desc->format) == MTLPixelFormatInvalid)
		return SPUDRESULT_GPU_INVALID_FORMAT;
	return SPUD_SUCCESS;
}

MTLTextureDescriptor *spudgpumetal___new_texture_descriptor(const spudgpu_image_desc *desc) {
	MTLTextureDescriptor *texture_desc = [[MTLTextureDescriptor alloc] init];
	texture_desc.textureType           = spudgpumetal___internal_image_texture_type(desc->type, desc->array_layers);
	texture_desc.pixelFormat           = spudgpumetal___internal_image_pixel_format(desc->format);
	texture_desc.width                 = desc->width;
	texture_desc.height                = desc->height;
	texture_desc.depth                 = (texture_desc.textureType == MTLTextureType3D) ? desc->depth : 1;
	texture_desc.arrayLength           = (texture_desc.textureType == MTLTextureType3D) ? 1 : desc->array_layers;
	texture_desc.mipmapLevelCount      = desc->mip_levels;
	texture_desc.usage                 = spudgpumetal___internal_image_texture_usage(desc->usage);
	// An image has no memory kind (see spudgpu_image_desc): it is in
	// the device's memory and never mapped, which is Private storage.
	// A transient attachment has no memory at all outside its pass.
	if (desc->usage & SPUDGPU_IMAGE_USAGE_TRANSIENT_ATTACHMENT) {
		texture_desc.storageMode = MTLStorageModeMemoryless;
	} else {
		texture_desc.storageMode = MTLStorageModePrivate;
	}

	return texture_desc;
}

SPUDRESULT spudgpu_create_image(
    spudgpu_device device,
    const spudgpu_image_desc *desc,
    spudgpu_image *out_image) {
	if (!device)
		return SPUDRESULT_GPU_INVALID_DEVICE;
	if (!desc)
		return SPUDRESULT_NULL_DESC;
	const SPUDRESULT base_sr = spud_struct_check(&desc->base, SPUD_STRUCT_TYPE_GPU_IMAGE_DESC);
	if (base_sr != SPUD_SUCCESS)
		return base_sr;
	if (!out_image)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	const SPUDRESULT desc_result = spudgpumetal___check_image_desc(desc);
	if (desc_result != SPUD_SUCCESS)
		return desc_result;

	spudgpu_device_metal *device_metal = (spudgpu_device_metal *)device;
	SPUDRESULT sr                      = SPUD_SUCCESS;

	spudgpu_image_metal *image_metal = SPUD_NEW_ZEROED(&device_metal->head.allocator, spudgpu_image_metal);
	if (!image_metal) {
		sr = SPUDRESULT_GENERAL_FAILURE;
		goto failedattempt;
	}
	image_metal->head.allocator = device_metal->head.allocator;
	image_metal->_parent_device = device_metal;
	image_metal->_desc          = *desc;

	{
		MTLTextureDescriptor *texture_desc = spudgpumetal___new_texture_descriptor(desc);
		image_metal->_texture_mtl          = [device_metal->_device_mtl newTextureWithDescriptor:texture_desc];
		[texture_desc release];
	}
	if (!image_metal->_texture_mtl) {
		sr = SPUDRESULT_API_SPECIFIC_FAILURE;
		goto failedattempt;
	}

	if (spud_debug_name_set(image_metal, desc->debug_name) != SPUD_SUCCESS) {
		spudgpu_destroy_image((spudgpu_image)image_metal);
		return SPUDRESULT_OUT_OF_MEMORY;
	}
	image_metal->_desc.debug_name = image_metal->head.debug_name;

	*out_image = (spudgpu_image)image_metal;

	return sr;

failedattempt:
	SPUD_DELETE(&device_metal->head.allocator, spudgpu_image_metal, image_metal);
	return sr;
}

void spudgpu_destroy_image(spudgpu_image image) {
	spudgpu_image_metal *image_metal = (spudgpu_image_metal *)image;
	if (image_metal) {
		if (image_metal->_texture_mtl) {
			[image_metal->_texture_mtl release];
		}
#if _DEBUG
		if (image_metal->_heap)
			image_metal->_heap->_placed_count--;
#endif
		const SPUD_ALLOCATOR allocator = image_metal->head.allocator;
		spud_string_free(&allocator, image_metal->head.debug_name);
		SPUD_DELETE(&allocator, spudgpu_image_metal, image_metal);
	}
}

SPUDRESULT spudgpu_get_image_desc(
    spudgpu_image image,
    spudgpu_image_desc *out_desc) {
	spudgpu_image_metal *image_metal = (spudgpu_image_metal *)image;
	if (!image_metal)
		return SPUDRESULT_GPU_INVALID_IMAGE;
	if (!out_desc)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	*out_desc = image_metal->_desc;
	out_desc->base = spud_struct_base(SPUD_STRUCT_TYPE_GPU_IMAGE_DESC);
	return SPUD_SUCCESS;
}

static MTLTextureType spudgpumetal___internal_image_view_texture_type(
    SPUDGPU_IMAGE_VIEW_TYPE type,
    MTLTextureType parent_type) {
	switch (type) {
	case SPUDGPU_IMAGE_VIEW_TYPE_1D:
		return MTLTextureType1D;
	case SPUDGPU_IMAGE_VIEW_TYPE_1D_ARRAY:
		return MTLTextureType1DArray;
	case SPUDGPU_IMAGE_VIEW_TYPE_2D:
		return MTLTextureType2D;
	case SPUDGPU_IMAGE_VIEW_TYPE_2D_ARRAY:
		return MTLTextureType2DArray;
	case SPUDGPU_IMAGE_VIEW_TYPE_3D:
		return MTLTextureType3D;
	// Cube maps have no dedicated SPUDGPU_IMAGE_TYPE at the base-image level
	// (see spudgpumetal___internal_image_texture_type) - this is where that
	// reinterpretation actually happens, same as Vulkan's
	// VK_IMAGE_VIEW_TYPE_CUBE over a plain 2D-array VkImage.
	case SPUDGPU_IMAGE_VIEW_TYPE_CUBE:
		return MTLTextureTypeCube;
	case SPUDGPU_IMAGE_VIEW_TYPE_CUBE_ARRAY:
		return MTLTextureTypeCubeArray;
	default:
		return parent_type; // SPUDGPU_IMAGE_VIEW_TYPE_NONE: same shape as parent.
	}
}

// The aspects [format] has (SPUDGPU_IMAGE_ASPECT). The same table in every
// backend: it decides which views spudgpu_create_image_view refuses.
static uint64_t spudgpumetal___internal_format_aspects(SPUDGPU_FORMAT format) {
	switch (format) {
	case SPUDGPU_FORMAT_D24_UNORM_S8_UINT:
	case SPUDGPU_FORMAT_D32_FLOAT_S8X24_UINT:
		return SPUDGPU_IMAGE_ASPECT_DEPTH | SPUDGPU_IMAGE_ASPECT_STENCIL;
	case SPUDGPU_FORMAT_D16_UNORM:
	case SPUDGPU_FORMAT_D32_FLOAT:
		return SPUDGPU_IMAGE_ASPECT_DEPTH;
	default:
		return SPUDGPU_IMAGE_ASPECT_COLOR;
	}
}

SPUDRESULT spudgpu_create_image_view(
    spudgpu_image image,
    const spudgpu_image_view_desc *desc,
    spudgpu_image_view *out_image_view) {
	if (!image)
		return SPUDRESULT_GPU_INVALID_IMAGE;
	if (!desc)
		return SPUDRESULT_NULL_DESC;
	const SPUDRESULT base_sr = spud_struct_check(&desc->base, SPUD_STRUCT_TYPE_GPU_IMAGE_VIEW_DESC);
	if (base_sr != SPUD_SUCCESS)
		return base_sr;
	if (!out_image_view)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;

	spudgpu_image_metal *parent_image_metal = (spudgpu_image_metal *)image;
	// The cube view rules of spudgpu.h, the same on every backend.
	if (desc->type == SPUDGPU_IMAGE_VIEW_TYPE_CUBE || desc->type == SPUDGPU_IMAGE_VIEW_TYPE_CUBE_ARRAY) {
		if (!parent_image_metal->_desc.cube_capable)
			return SPUDRESULT_GPU_INVALID_IMAGE_VIEW_TYPE;
		if (desc->type == SPUDGPU_IMAGE_VIEW_TYPE_CUBE &&
		    (desc->subresource_range.base_array_layer != 0 || desc->subresource_range.array_layer_count != 6))
			return SPUDRESULT_GPU_BUFFER_OR_IMAGE_VIEW_RANGE_OUT_OF_SCOPE;
		if (desc->type == SPUDGPU_IMAGE_VIEW_TYPE_CUBE_ARRAY &&
		    (desc->subresource_range.array_layer_count == 0 || desc->subresource_range.array_layer_count % 6 != 0))
			return SPUDRESULT_GPU_BUFFER_OR_IMAGE_VIEW_RANGE_OUT_OF_SCOPE;
	}
	// The aspect rule of spudgpu.h, the same on every backend.
	if (desc->subresource_range.aspect_mask == SPUDGPU_IMAGE_ASPECT_NONE)
		return SPUDRESULT_DESC_INVALID_PARAMETERS;
	if (desc->subresource_range.aspect_mask & ~spudgpumetal___internal_format_aspects(parent_image_metal->_desc.format))
		return SPUDRESULT_DESC_INVALID_PARAMETERS;
	if (!parent_image_metal->_texture_mtl)
		return SPUDRESULT_GPU_INVALID_IMAGE;

	SPUDRESULT sr = SPUD_SUCCESS;

	spudgpu_image_view_metal *view_metal = SPUD_NEW_ZEROED(&parent_image_metal->head.allocator, spudgpu_image_view_metal);
	if (!view_metal) {
		sr = SPUDRESULT_GENERAL_FAILURE;
		goto failedattempt;
	}
	view_metal->head.allocator     = parent_image_metal->head.allocator;
	view_metal->_parent_image      = parent_image_metal;
	view_metal->_desc              = *desc;
	view_metal->_desc.parent_image = image; // Authoritative - same reasoning as spudgpu_buffer_view_metal.

	{
		MTLTextureType view_type = spudgpumetal___internal_image_view_texture_type(desc->type, parent_image_metal->_texture_mtl.textureType);
		NSRange levels           = NSMakeRange((NSUInteger)desc->subresource_range.base_mip_level, (NSUInteger)desc->subresource_range.mip_level_count);
		NSRange slices           = NSMakeRange((NSUInteger)desc->subresource_range.base_array_layer, (NSUInteger)desc->subresource_range.array_layer_count);

		// The parent's pixel format, except for a view of the stencil alone
		// (SPUDGPU_IMAGE_ASPECT_STENCIL), which is what a shader reads the
		// stencil through: Metal reaches the stencil of a depth and stencil
		// texture as a view in the matching stencil format. The parent was
		// created with MTLTextureUsagePixelFormatView for this (see
		// spudgpu_create_image). spudgpu_image_view_desc has no format
		// override beyond that.
		MTLPixelFormat view_format = parent_image_metal->_texture_mtl.pixelFormat;
		if (desc->subresource_range.aspect_mask == SPUDGPU_IMAGE_ASPECT_STENCIL && view_format == MTLPixelFormatDepth32Float_Stencil8)
			view_format = MTLPixelFormatX32_Stencil8;
		view_metal->_texture_view_mtl = [parent_image_metal->_texture_mtl newTextureViewWithPixelFormat:view_format
		                                                                                    textureType:view_type
		                                                                                         levels:levels
		                                                                                         slices:slices];
	}
	if (!view_metal->_texture_view_mtl) {
		sr = SPUDRESULT_API_SPECIFIC_FAILURE;
		goto failedattempt;
	}

	*out_image_view = (spudgpu_image_view)view_metal;

	return sr;

failedattempt:
	SPUD_DELETE(&parent_image_metal->head.allocator, spudgpu_image_view_metal, view_metal);
	return sr;
}

void spudgpu_destroy_image_view(spudgpu_image_view image_view) {
	spudgpu_image_view_metal *view_metal = (spudgpu_image_view_metal *)image_view;
	if (view_metal) {
		if (view_metal->_texture_view_mtl) {
			[view_metal->_texture_view_mtl release];
		}
		const SPUD_ALLOCATOR allocator = view_metal->head.allocator;
		spud_string_free(&allocator, view_metal->head.debug_name);
		SPUD_DELETE(&allocator, spudgpu_image_view_metal, view_metal);
	}
}

SPUDRESULT spudgpu_get_image_view_desc(
    spudgpu_image_view image_view,
    spudgpu_image_view_desc *out_desc) {
	spudgpu_image_view_metal *view_metal = (spudgpu_image_view_metal *)image_view;
	if (!view_metal)
		return SPUDRESULT_GPU_INVALID_IMAGE_VIEW;
	if (!out_desc)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	*out_desc = view_metal->_desc;
	out_desc->base = spud_struct_base(SPUD_STRUCT_TYPE_GPU_IMAGE_VIEW_DESC);
	return SPUD_SUCCESS;
}

#endif // SPUDGPU_COMPILE_METAL_API
