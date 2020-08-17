#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform UniformBufferObject {
	mat4 light;
    mat4 model;
    mat4 view;
    mat4 proj;
	vec4 lightDir;
	mat4 testProj;
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
	vec3 pos = inPosition;
	pos.z = (pos.z + 1.0) / 2.0; // to 0-1 clip space

    gl_Position = ubo.proj * ubo.view * ubo.testProj * vec4(pos, 1.0);

	outPosition = (vec4(pos, 1.0)).xyz;
	outColor = vec3(1.0);
	outTexCoord = vec2(0.0);
	outNormal = (vec4(inNormal, 0.0)).xyz;
}