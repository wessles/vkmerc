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

layout(set=1, binding=0) uniform sampler2D tex_screenbuf;
layout(set=1, binding=1) uniform sampler2D tex_cascade0;

layout(location = 0) in vec2 inTexCoord;

layout(location = 0) out vec4 outColor;

#define PREVIEW_SIZE 400
#define BORDER 2

void main()
{
	vec2 coords = inTexCoord * global.screenRes;
	
	outColor.rgb = texture(tex_screenbuf, inTexCoord).rgb;
	outColor.a = 1.0;
	
	if(coords.y < PREVIEW_SIZE && coords.x < PREVIEW_SIZE) {
		outColor.rgb = vec3(0.0);
	}
	if(coords.y < PREVIEW_SIZE-BORDER && coords.x < PREVIEW_SIZE-BORDER) {
		coords = coords/PREVIEW_SIZE;
		coords = vec2(1.0) - coords;
		outColor.rgb = vec3(texture(tex_cascade0, coords).r);
	}
}