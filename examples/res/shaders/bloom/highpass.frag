#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(std140, binding = 0) uniform GlobalUniform {
	mat4 view;
	mat4 proj;
	vec4 camPos;
	vec4 directionalLight;
	vec2 screenRes;
	float time;
} global;

layout(set = 2, binding = 0) uniform HighpassParams {
	vec4 cutoff;
} params;

layout(set = 1, binding = 0) uniform sampler2D screen;

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 outColor;


void main() {
	vec3 col = texture(screen, uv).rgb;
	float luminance = (0.2126*col.r + 0.7152*col.g + 0.0722*col.b);
	
	// high luminance filter
	col *= smoothstep(params.cutoff.x, params.cutoff.y, luminance);
	
	// up contrast
	col = (vec3(1.0) - col);
	col *= col;
	col = (vec3(1.0) - col);

	outColor = vec4(col, 1.0);
}