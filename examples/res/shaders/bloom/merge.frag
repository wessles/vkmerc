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

float rand(float n){return fract(sin(n) * 43758.5453123);}
float noise(float p){
	float fl = floor(p);
	float fc = fract(p);
	return mix(rand(fl), rand(fl + 1.0), fc);
}

void main() {
	vec2 tc = uv;
	outColor = vec4( texture(screen,tc).rgb + texture(bloom,tc).rgb * params.intensity , 1.0);
}