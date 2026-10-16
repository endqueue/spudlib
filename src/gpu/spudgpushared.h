#ifndef SPUDGPUSHARED_H
#define SPUDGPUSHARED_H

/* Internal header - never included outside spudlib/src/gpu/. What every
 * backend shares: format facts the public header doesn't expose, and the
 * checks of a call that are the same on every backend, written once so that
 * all three return the same result for the same condition. */

#include "spudgpu.h"

#if __cplusplus
extern "C" {
#endif

/* The size of one texel block of [fmt] in bytes: a texel for an uncompressed
 * format, a 4x4 block for a block-compressed one (BC1-BC7). 0 for a format
 * whose texels aren't a whole number of bytes or have no known size. */
uint32_t spudgpu___internal_format_block_bytes(SPUDGPU_FORMAT fmt);

/* The checks of spudgpu_create_shader_pipeline() on its color attachments
 * and its depth bias, in this order, after the shader stage checks:
 * SPUDRESULT_GPU_TOO_MANY_COLOR_ATTACHMENTS,
 * SPUDRESULT_GPU_DEPTH_BIAS_CLAMP_NOT_SUPPORTED,
 * SPUDRESULT_GPU_INDEPENDENT_BLEND_NOT_SUPPORTED. [properties] are the
 * device's. */
SPUDRESULT spudgpu___internal_check_pipeline_outputs(
    const spudgpu_shader_pipeline_desc *desc,
    const SPUDGPU_DEVICE_PROPERTIES *properties);

/* Whether blend attachment [a] and [b] blend the same way. Two that don't
 * blend are the same whatever their factors. */
bool spudgpu___internal_blend_attachments_equal(
    const spudgpu_blend_attachment_desc *a,
    const spudgpu_blend_attachment_desc *b);

#if __cplusplus
}
#endif

#endif // SPUDGPUSHARED_H
