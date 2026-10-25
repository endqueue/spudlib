//
// SpudGPU Metal backend - the format table: what each SPUDGPU_FORMAT is to
// Metal. Every function of this backend that turns a SPUDGPU_FORMAT into a
// Metal format, or asks what a format is, reads it here.
//

#if SPUDGPU_COMPILE_METAL_API

#include "spudcore.h"
#include "spudgpu.h"
#include "spudgpumetal.h"
#include <Metal/Metal.h>

// Which formats are here, and why the others aren't:
//
// - The common uncompressed formats, not the whole DXGI-shaped enum. The
//   block-compressed formats and the many *_TYPELESS ones have no caller
//   in this workspace yet.
// - TYPELESS formats are never mapped. Metal has no typeless resource the
//   way D3D12 does: every texture needs one concrete MTLPixelFormat at
//   creation, so a typeless request is refused rather than given an
//   interpretation the caller didn't ask for.
// - SPUDGPU_FORMAT_D24_UNORM_S8_UINT has no pixel format:
//   MTLPixelFormatDepth24Unorm_Stencil8 exists only on Intel and AMD Macs,
//   not on Apple Silicon, this backend's target. There is no honest way to
//   hand back a 24-bit depth resource; SPUDGPU_FORMAT_D32_FLOAT_S8X24_UINT
//   is Depth32Float_Stencil8. Its aspects are listed all the same, so a
//   view's aspects are judged the same way on every backend.
// - Vertex formats include the 3-component 32-bit ones, which have no
//   pixel format: vertex fetch doesn't go through the texture unit.
// - Presentable: what a CAMetalLayer's pixelFormat may be set to, of the
//   formats here. Apple lists BGRA8Unorm, BGRA8Unorm_sRGB, RGBA16Float,
//   RGB10A2Unorm, BGR10A2Unorm and the extended-range BGR10 formats; the
//   last two kinds have no SPUDGPU_FORMAT. RGBA8Unorm is not on the list.

#define PIX(pixel, vertex) {(pixel), MTLPixelFormatInvalid, (vertex), SPUDGPU_IMAGE_ASPECT_COLOR, false}
#define PRESENT(pixel, vertex) {(pixel), MTLPixelFormatInvalid, (vertex), SPUDGPU_IMAGE_ASPECT_COLOR, true}
#define VERTEX_ONLY(vertex) {MTLPixelFormatInvalid, MTLPixelFormatInvalid, (vertex), SPUDGPU_IMAGE_ASPECT_COLOR, false}

static const spudgpumetal_format_info spudgpumetal___formats[] = {
	// 8 bits per channel
	[SPUDGPU_FORMAT_R8_UNORM]            = PIX(MTLPixelFormatR8Unorm, MTLVertexFormatUCharNormalized),
	[SPUDGPU_FORMAT_R8_SNORM]            = PIX(MTLPixelFormatR8Snorm, MTLVertexFormatCharNormalized),
	[SPUDGPU_FORMAT_R8_UINT]             = PIX(MTLPixelFormatR8Uint, MTLVertexFormatUChar),
	[SPUDGPU_FORMAT_R8_SINT]             = PIX(MTLPixelFormatR8Sint, MTLVertexFormatChar),
	[SPUDGPU_FORMAT_R8G8_UNORM]          = PIX(MTLPixelFormatRG8Unorm, MTLVertexFormatUChar2Normalized),
	[SPUDGPU_FORMAT_R8G8_SNORM]          = PIX(MTLPixelFormatRG8Snorm, MTLVertexFormatChar2Normalized),
	[SPUDGPU_FORMAT_R8G8_UINT]           = PIX(MTLPixelFormatRG8Uint, MTLVertexFormatUChar2),
	[SPUDGPU_FORMAT_R8G8_SINT]           = PIX(MTLPixelFormatRG8Sint, MTLVertexFormatChar2),
	[SPUDGPU_FORMAT_R8G8B8A8_UNORM]      = PIX(MTLPixelFormatRGBA8Unorm, MTLVertexFormatUChar4Normalized),
	[SPUDGPU_FORMAT_R8G8B8A8_UNORM_SRGB] = PIX(MTLPixelFormatRGBA8Unorm_sRGB, MTLVertexFormatInvalid),
	[SPUDGPU_FORMAT_R8G8B8A8_SNORM]      = PIX(MTLPixelFormatRGBA8Snorm, MTLVertexFormatChar4Normalized),
	[SPUDGPU_FORMAT_R8G8B8A8_UINT]       = PIX(MTLPixelFormatRGBA8Uint, MTLVertexFormatUChar4),
	[SPUDGPU_FORMAT_R8G8B8A8_SINT]       = PIX(MTLPixelFormatRGBA8Sint, MTLVertexFormatChar4),
	[SPUDGPU_FORMAT_B8G8R8A8_UNORM]      = PRESENT(MTLPixelFormatBGRA8Unorm, MTLVertexFormatInvalid),
	[SPUDGPU_FORMAT_B8G8R8A8_UNORM_SRGB] = PRESENT(MTLPixelFormatBGRA8Unorm_sRGB, MTLVertexFormatInvalid),

	// 16 bits per channel
	[SPUDGPU_FORMAT_R16_FLOAT]           = PIX(MTLPixelFormatR16Float, MTLVertexFormatHalf),
	[SPUDGPU_FORMAT_R16_UNORM]           = PIX(MTLPixelFormatR16Unorm, MTLVertexFormatUShortNormalized),
	[SPUDGPU_FORMAT_R16_SNORM]           = PIX(MTLPixelFormatR16Snorm, MTLVertexFormatShortNormalized),
	[SPUDGPU_FORMAT_R16_UINT]            = PIX(MTLPixelFormatR16Uint, MTLVertexFormatUShort),
	[SPUDGPU_FORMAT_R16_SINT]            = PIX(MTLPixelFormatR16Sint, MTLVertexFormatShort),
	[SPUDGPU_FORMAT_R16G16_FLOAT]        = PIX(MTLPixelFormatRG16Float, MTLVertexFormatHalf2),
	[SPUDGPU_FORMAT_R16G16_UNORM]        = PIX(MTLPixelFormatRG16Unorm, MTLVertexFormatUShort2Normalized),
	[SPUDGPU_FORMAT_R16G16_SNORM]        = PIX(MTLPixelFormatRG16Snorm, MTLVertexFormatShort2Normalized),
	[SPUDGPU_FORMAT_R16G16_UINT]         = PIX(MTLPixelFormatRG16Uint, MTLVertexFormatUShort2),
	[SPUDGPU_FORMAT_R16G16_SINT]         = PIX(MTLPixelFormatRG16Sint, MTLVertexFormatShort2),
	[SPUDGPU_FORMAT_R16G16B16A16_FLOAT]  = PRESENT(MTLPixelFormatRGBA16Float, MTLVertexFormatHalf4),
	[SPUDGPU_FORMAT_R16G16B16A16_UNORM]  = PIX(MTLPixelFormatRGBA16Unorm, MTLVertexFormatUShort4Normalized),
	[SPUDGPU_FORMAT_R16G16B16A16_SNORM]  = PIX(MTLPixelFormatRGBA16Snorm, MTLVertexFormatShort4Normalized),
	[SPUDGPU_FORMAT_R16G16B16A16_UINT]   = PIX(MTLPixelFormatRGBA16Uint, MTLVertexFormatUShort4),
	[SPUDGPU_FORMAT_R16G16B16A16_SINT]   = PIX(MTLPixelFormatRGBA16Sint, MTLVertexFormatShort4),

	// 32 bits per channel
	[SPUDGPU_FORMAT_R32_FLOAT]           = PIX(MTLPixelFormatR32Float, MTLVertexFormatFloat),
	[SPUDGPU_FORMAT_R32_UINT]            = PIX(MTLPixelFormatR32Uint, MTLVertexFormatUInt),
	[SPUDGPU_FORMAT_R32_SINT]            = PIX(MTLPixelFormatR32Sint, MTLVertexFormatInt),
	[SPUDGPU_FORMAT_R32G32_FLOAT]        = PIX(MTLPixelFormatRG32Float, MTLVertexFormatFloat2),
	[SPUDGPU_FORMAT_R32G32_UINT]         = PIX(MTLPixelFormatRG32Uint, MTLVertexFormatUInt2),
	[SPUDGPU_FORMAT_R32G32_SINT]         = PIX(MTLPixelFormatRG32Sint, MTLVertexFormatInt2),
	[SPUDGPU_FORMAT_R32G32B32_FLOAT]     = VERTEX_ONLY(MTLVertexFormatFloat3),
	[SPUDGPU_FORMAT_R32G32B32_UINT]      = VERTEX_ONLY(MTLVertexFormatUInt3),
	[SPUDGPU_FORMAT_R32G32B32_SINT]      = VERTEX_ONLY(MTLVertexFormatInt3),
	[SPUDGPU_FORMAT_R32G32B32A32_FLOAT]  = PIX(MTLPixelFormatRGBA32Float, MTLVertexFormatFloat4),
	[SPUDGPU_FORMAT_R32G32B32A32_UINT]   = PIX(MTLPixelFormatRGBA32Uint, MTLVertexFormatUInt4),
	[SPUDGPU_FORMAT_R32G32B32A32_SINT]   = PIX(MTLPixelFormatRGBA32Sint, MTLVertexFormatInt4),

	// Packed
	[SPUDGPU_FORMAT_R10G10B10A2_UNORM]   = PRESENT(MTLPixelFormatRGB10A2Unorm, MTLVertexFormatUInt1010102Normalized),
	[SPUDGPU_FORMAT_R10G10B10A2_UINT]    = PIX(MTLPixelFormatRGB10A2Uint, MTLVertexFormatInvalid),
	[SPUDGPU_FORMAT_R11G11B10_FLOAT]     = PIX(MTLPixelFormatRG11B10Float, MTLVertexFormatInvalid),

	// Depth and stencil
	[SPUDGPU_FORMAT_D16_UNORM] = {MTLPixelFormatDepth16Unorm, MTLPixelFormatInvalid, MTLVertexFormatInvalid,
	                              SPUDGPU_IMAGE_ASPECT_DEPTH, false},
	[SPUDGPU_FORMAT_D32_FLOAT] = {MTLPixelFormatDepth32Float, MTLPixelFormatInvalid, MTLVertexFormatInvalid,
	                              SPUDGPU_IMAGE_ASPECT_DEPTH, false},
	[SPUDGPU_FORMAT_D24_UNORM_S8_UINT] = {MTLPixelFormatInvalid, MTLPixelFormatInvalid, MTLVertexFormatInvalid,
	                                      SPUDGPU_IMAGE_ASPECT_DEPTH | SPUDGPU_IMAGE_ASPECT_STENCIL, false},
	// The stencil of a Depth32Float_Stencil8 texture is read through an
	// X32_Stencil8 view of it.
	[SPUDGPU_FORMAT_D32_FLOAT_S8X24_UINT] = {MTLPixelFormatDepth32Float_Stencil8, MTLPixelFormatX32_Stencil8,
	                                         MTLVertexFormatInvalid,
	                                         SPUDGPU_IMAGE_ASPECT_DEPTH | SPUDGPU_IMAGE_ASPECT_STENCIL, false},
};

#undef PIX
#undef PRESENT
#undef VERTEX_ONLY

// A format with no entry: a color format Metal has nothing for here.
static const spudgpumetal_format_info spudgpumetal___no_format = {
    MTLPixelFormatInvalid, MTLPixelFormatInvalid, MTLVertexFormatInvalid, SPUDGPU_IMAGE_ASPECT_COLOR, false};

const spudgpumetal_format_info *spudgpumetal___format_info(SPUDGPU_FORMAT format) {
	if (format >= sizeof(spudgpumetal___formats) / sizeof(spudgpumetal___formats[0]))
		return &spudgpumetal___no_format;
	const spudgpumetal_format_info *info = &spudgpumetal___formats[format];
	// An index the table skips is zeroed, with no aspects: it has no entry.
	return info->aspects ? info : &spudgpumetal___no_format;
}

#endif // SPUDGPU_COMPILE_METAL_API
