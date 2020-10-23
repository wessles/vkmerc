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

layout(set=1, binding=0) uniform sampler2D tex_screenbuf;

layout(location = 0) in vec2 inTexCoord;

layout(location = 0) out vec4 outColor;

void main()
{
	outColor.rgb = texture(tex_screenbuf, inTexCoord).rgb;
	outColor.r = 1.0;
}