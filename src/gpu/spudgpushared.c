#include "spudgpu.h"
#include "spudgpushared.h"

#if __cplusplus
extern "C" {
#endif

bool spudgpu___internal_blend_attachments_equal(
    const spudgpu_blend_attachment_desc *a,
    const spudgpu_blend_attachment_desc *b) {
	if (a->blend_enable != b->blend_enable)
		return false;
	if (!a->blend_enable)
		return true;
	return a->src_color_blend_factor == b->src_color_blend_factor && a->dst_color_blend_factor == b->dst_color_blend_factor &&
	       a->color_blend_op == b->color_blend_op && a->src_alpha_blend_factor == b->src_alpha_blend_factor &&
	       a->dst_alpha_blend_factor == b->dst_alpha_blend_factor && a->alpha_blend_op == b->alpha_blend_op;
}

SPUDRESULT spudgpu___internal_check_pipeline_outputs(
    const spudgpu_shader_pipeline_desc *desc,
    const SPUDGPU_DEVICE_PROPERTIES *properties) {
	if (desc->color_attachment_count > SPUDGPU_MAX_COLOR_ATTACHMENTS)
		return SPUDRESULT_GPU_TOO_MANY_COLOR_ATTACHMENTS;
	if (desc->depth_bias_clamp != 0.0f && !properties->depth_bias_clamp)
		return SPUDRESULT_GPU_DEPTH_BIAS_CLAMP_NOT_SUPPORTED;
	if (!properties->independent_blend)
		for (uint32_t i = 1; i < desc->color_attachment_count; ++i)
			if (!spudgpu___internal_blend_attachments_equal(&desc->blend_attachments[0], &desc->blend_attachments[i]))
				return SPUDRESULT_GPU_INDEPENDENT_BLEND_NOT_SUPPORTED;
	return SPUD_SUCCESS;
}

#if __cplusplus
}
#endif
