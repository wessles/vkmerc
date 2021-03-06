#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(std140, binding = 0) uniform GlobalUniform {
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
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;
layout(location = 4) in vec4 inTangent;

layout(location = 0) out vec3 fragPosition;

void main() {
	vec4 vertPos = vec4(inPosition, 1.0);
	// create 0 to 1 space from 0,0,0 to 1,1,1
	vertPos = max(vec4(0.0), vertPos);
	vertPos = pc.transform * vertPos;

    fragPosition = vertPos.xyz;
	
    gl_Position = global.proj * global.view * vec4(fragPosition, 1.0);
}