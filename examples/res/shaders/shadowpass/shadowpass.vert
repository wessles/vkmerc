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

layout(std140, binding = 1) uniform CascadesUniform {
	mat4 cascades[4];
	float biases[4];
} cascades;

layout(push_constant) uniform pushConstants {
    mat4 transform;
	uint cascade;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 in2;
layout(location = 2) in vec3 in3;
layout(location = 3) in vec3 in4;
layout(location = 4) in vec4 in5;

void main() {
	gl_Position = cascades.cascades[pc.cascade] * pc.transform * vec4(inPosition, 1.0);
}