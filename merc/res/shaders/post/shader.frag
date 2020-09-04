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
layout(set=1, binding = 1) uniform sampler2D bloom;

layout(location = 0) in vec2 fragCoord;

layout(location = 0) out vec4 outColor;

void main() {
	vec2 pixel = fragCoord * global.screenRes;
	vec2 uv = fragCoord;

	float a = -0.5;
	float t = length(uv - 0.5) * 2.0 + a;
	float c = 0.01;
	float k = mix(0.0, 1.0, c + (1.0 - c) * t);

	float atten = mix(1.0, 0.0, k);

	outColor = texture(screenbuffer, uv) + 0.9 * texture(bloom, uv);
	outColor.rgb *= atten;
}