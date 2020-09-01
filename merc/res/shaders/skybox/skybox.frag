#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set=2, binding = 0) uniform samplerCube environment;

layout(location = 0) in vec3 fragPosition;

layout(location = 0) out vec4 outColor;

void main() {
	vec3 dir = normalize(fragPosition);

	outColor = texture(environment, normalize(dir));
}