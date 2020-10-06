#version 450
#extension GL_ARB_separate_shader_objects : enable


layout(std140, binding = 0) uniform GlobalUniform {
	mat4 view;
	mat4 proj;
	vec4 camPos;
	vec2 screenRes;
	float time;
} global;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;
layout(location = 4) in vec4 inTangent;

layout(location = 0) out vec3 fragPosition;

void main() {
	gl_Position = global.proj * vec4(inPosition, 1.0);
	
	fragPosition = (inverse(global.view) * vec4(inPosition, 0.0)).xyz;
}