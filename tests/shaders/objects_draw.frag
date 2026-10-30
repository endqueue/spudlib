// spudgpu_objects_test's fragment shader: the draw's colour, plus a tint
// from a uniform buffer, plus a texel of a 2 x 2 texture as much as the
// draw asks for.
#version 450

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform Tint {
	vec4 add;
} tint;
layout(set = 0, binding = 1) uniform texture2D tex;
layout(set = 0, binding = 2) uniform sampler smp;

layout(push_constant) uniform Push {
	vec4 color;
	vec4 params;
} pc;

void main() {
	out_color = pc.color + tint.add + texture(sampler2D(tex, smp), uv) * pc.params.y;
}
