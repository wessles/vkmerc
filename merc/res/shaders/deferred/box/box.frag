#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 fragPosition;
layout(location = 1) in vec3 fragColor;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec3 fragNormal;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outNorm;
layout(location = 2) out vec4 outPos;

void main() {
	outColor = vec4(1.0);
	outNorm = vec4(normalize(fragNormal), 0.0);
	outPos = vec4(fragPosition, 1.0);
}