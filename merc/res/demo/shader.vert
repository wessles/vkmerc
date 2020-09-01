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

layout(location = 0) out vec3 outPosition;
layout(location = 1) out vec3 outColor;
layout(location = 2) out vec2 outTexCoord;
layout(location = 3) out vec3 outNormal;

void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);

	outPosition = (ubo.model * vec4(inPosition, 1.0)).xyz;
	outColor = inColor;
	outTexCoord = inTexCoord;
	outNormal = (ubo.model * vec4(inNormal, 0.0)).xyz;
}