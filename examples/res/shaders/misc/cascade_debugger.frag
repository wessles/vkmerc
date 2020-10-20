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

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;
layout(location = 4) in vec4 inTangent;

layout(location = 0) out vec4 outColor;

void main()
{
	outColor.rgb = vec3(1.0);
	outColor.a = 1.0;

	outColor.rgb *= 0.5+0.5*dot(-normalize(inNormal), vec3(global.directionalLight));
	
	vec4 cascadeProj4Vec = global.cascade0 * vec4(inPosition, 1.0);
	vec2 cascadeProj = cascadeProj4Vec.xy;
	if(cascadeProj.x > -1.0 && cascadeProj.y > -1.0 && cascadeProj.x < 1.0 && cascadeProj.y < 1.0) {
		outColor.rgb *= vec3(1.0, 0.1, 0.1);
	}
	else {
		cascadeProj4Vec = global.cascade1 * vec4(inPosition, 1.0);
		cascadeProj = cascadeProj4Vec.xy;
		if(cascadeProj.x > -1.0 && cascadeProj.y > -1.0 && cascadeProj.x < 1.0 && cascadeProj.y < 1.0) {
			outColor.rgb *= vec3(0.1, 1.0, 0.1);
		} else {
			cascadeProj4Vec = global.cascade2 * vec4(inPosition, 1.0);
			cascadeProj = cascadeProj4Vec.xy;
			if(cascadeProj.x > -1.0 && cascadeProj.y > -1.0 && cascadeProj.x < 1.0 && cascadeProj.y < 1.0) {
				outColor.rgb *= vec3(0.1, 0.1, 1.0);
			}
		}
	}
}