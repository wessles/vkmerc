#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(std140, binding = 0) uniform GlobalUniform {
	mat4 cascade0;
	mat4 cascade1;
	mat4 cascade2;
	mat4 view;
	mat4 proj;
	vec4 camPos;
	vec4 directionalLight;
	vec2 screenRes;
	float time;
} global;

layout(push_constant) uniform pushConstants {
    mat4 transform;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 in2;
layout(location = 2) in vec3 in3;
layout(location = 3) in vec3 in4;
layout(location = 4) in vec4 in5;

void main() {
    vec4 fragPosition = pc.transform * vec4(inPosition, 1.0);
    gl_Position = 
#ifdef CASCADE_0
	global.cascade0
#endif
#ifdef CASCADE_1
	global.cascade0
#endif
#ifdef CASCADE_2
	global.cascade0
#endif
	* fragPosition;
}