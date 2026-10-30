// spudgpu_objects_test's vertex shader: a unit quad's corner (per vertex)
// moved to a cell of an 8 x 8 grid (per instance), so each draw call's
// vertex and instance fetches decide which cells it covers. Clip space is
// Vulkan's on every backend (+Y down), so cell (0, 0) is the top left.
#version 450

layout(location = 0) in vec2 corner;
layout(location = 1) in vec2 cell;

layout(push_constant) uniform Push {
	vec4 color;
	// x: depth; y: how much of the texture to add; zw: the mesh shader's
	// first cell.
	vec4 params;
} pc;

layout(location = 0) out vec2 uv;

void main() {
	vec2 p      = (cell + corner) / 8.0;
	gl_Position = vec4(p * 2.0 - 1.0, pc.params.x, 1.0);
	uv          = corner;
}
