#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 fragPosition;
layout(location = 1) in vec3 fragColor;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec3 fragNormal;

layout(set = 2, binding = 0) uniform sampler2D albedo;

layout(location = 0) out vec4 outColor;

void main() {
	outColor = texture(albedo, fragTexCoord);
}