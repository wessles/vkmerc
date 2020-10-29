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

layout(set = 2, binding = 0) uniform MergeParams {
	float intensity;
} params;

layout(set = 1, binding = 0) uniform sampler2D screen;
layout(set = 1, binding = 1) uniform sampler2D bloom;

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 outColor;


void main() {
	outColor = vec4( texture(screen,uv).rgb + texture(bloom,uv).rgb * params.intensity , 1.0);
	//outColor = vec4( mix(texture(screen,uv).rgb, texture(bloom,uv).rgb, params.intensity) , 1.0);
}