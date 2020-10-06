#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(std140, binding = 0) uniform GlobalUniform {
	mat4 view;
	mat4 proj;
	vec4 camPos;
	vec2 screenRes;
	float time;
} global;

layout(set=1, binding = 0) uniform sampler2D screenbuffer;

layout(location = 0) in vec2 fragCoord;

layout(location = 0) out vec4 outColor;

void main() {
	vec2 uv = fragCoord;
	outColor = texture(screenbuffer, uv);
}