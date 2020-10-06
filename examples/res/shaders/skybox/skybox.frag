#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(std140, binding = 0) uniform GlobalUniform {
	mat4 view;
	mat4 proj;
	vec4 camPos;
	vec2 screenRes;
	float time;
} global;

layout(set=2, binding = 0) uniform samplerCube environment;

layout(location = 0) in vec3 fragPosition;

layout(location = 0) out vec4 outColor;

void main() {
	vec3 dir = normalize(fragPosition);

	outColor = textureLod(environment, normalize(dir), 0.0);
}