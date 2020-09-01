#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform UniformBufferObject {
	mat4 light;
    mat4 model;
    mat4 view;
    mat4 proj;
	vec4 lightDir;
} ubo;

layout(binding = 1) uniform sampler2D texSampler;
layout(binding = 2) uniform samplerCube environment;
layout(binding = 3) uniform sampler2D shadowMap;

layout(location = 0) in vec3 fragPosition;
layout(location = 1) in vec3 fragColor;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec3 fragNormal;

layout(location = 0) out vec4 outColor;

void main() {
	outColor = vec4(fragNormal, .5);
}