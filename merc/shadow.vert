#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform UniformBufferObject {
	mat4 light;
    mat4 model;
    mat4 view;
    mat4 proj;
	vec4 lightDir;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;

void main() {
    gl_Position = ubo.light * ubo.model * vec4(inPosition, 1.0);
	vec3 c = inColor * 0.;
	vec2 t = inTexCoord * 0.;
	vec3 n = inNormal * 0.;
}