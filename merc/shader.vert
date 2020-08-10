#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform UniformBufferObject {
	vec3 light;
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out vec3 lightPos;
layout(location = 4) out vec3 fragPosition;
layout(location = 5) out vec3 lookDir;

void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    fragColor = inColor;
    fragTexCoord = inTexCoord;
	fragNormal = normalize((ubo.model * vec4(inNormal, 0.0)).xyz);
	// light direction in world space
	lightPos = ubo.light;
	// position in world space
	fragPosition = (ubo.model * vec4(inPosition, 1.0)).xyz;

	lookDir = normalize(fragPosition - (inverse(ubo.view) * vec4(0.0, 0.0, 0.0, 1.0)).xyz);
}