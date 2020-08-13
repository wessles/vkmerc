#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform UniformBufferObject {
	mat4 light;
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(binding = 1) uniform sampler2D texSampler;
layout(binding = 2) uniform samplerCube environment;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec3 lightPos;
layout(location = 4) in vec3 fragPosition;
layout(location = 5) in vec3 lookDir;

layout(location = 0) out vec4 outColor;

void main() {
	vec3 dir = normalize(fragPosition);

	outColor = texture(environment, normalize(dir));
}