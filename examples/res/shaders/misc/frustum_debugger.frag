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
	vec4 color;
	bool mode;
} pc;

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec4 outColor;

void main()
{
	outColor = pc.color;
	if(pc.mode)
		outColor.rgb *= fract(inPosition.z * 4.0);
}