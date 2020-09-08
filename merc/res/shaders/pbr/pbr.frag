#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(std140, binding = 0) uniform GlobalUniform {
	mat4 view;
	mat4 proj;
	vec4 camPos;
	vec2 screenRes;
	float time;
} global;

layout(set=2, binding=0) uniform sampler2D tex_albedo;
layout(set=2, binding=1) uniform sampler2D tex_normal;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;
layout(location = 4) in vec4 inTangent;

layout(location = 0) out vec4 outColor;

void main()
{
	vec3 normal      = normalize(inNormal);
	vec3 tangent     = normalize(inTangent.xyz);
	vec3 bitangent   = cross(inNormal, inTangent.xyz) * inTangent.w;
	mat3 TBN         = mat3(tangent, bitangent, normal);
	vec3 localNormal = texture(tex_normal, inTexCoord).xyz;
	normal           = normalize(TBN * localNormal);
	
    outColor = vec4(texture(tex_albedo, inTexCoord).rgb, 1.0);
	outColor.rgb *= max(vec3(0.1), vec3(dot(normal, vec3(sin(global.time * 0.1), cos(global.time), sin(global.time)))));
}