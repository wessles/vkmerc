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

layout(std140, binding = 1) uniform CascadesUniform {
	mat4 cascades[4];
	vec4 data[4];
	mat4 cameraFrust;
	vec2 clipPlanes;
} cascades;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;
layout(location = 4) in vec4 inTangent;
layout(location = 5) in float inDepth;

layout(location = 0) out vec4 outColor;

// from iq, the wise one
float sdBox( in vec2 p, in vec2 b )
{
    vec2 d = abs(p)-b;
    return length(max(d,0.0)) + min(max(d.x,d.y),0.0);
}

void main()
{
	outColor.rgb = vec3(1.0);
	outColor.a = 1.0;

	outColor.rgb *= 0.5+0.5*dot(-normalize(inNormal), vec3(global.directionalLight));

	vec4 projected = cascades.cameraFrust * vec4(inPosition, 1.0);
	float n = cascades.clipPlanes[0], f = cascades.clipPlanes[1];
	float z = (projected.z - n) / (f - n);
	
	vec4 cascadeProj;
	for(int i = 0; i < 4; i++) {
		cascadeProj = cascades.cascades[i] * vec4(inPosition, 1.0);
		float dist = sdBox(cascadeProj.xy, vec2(1.0, 1.0));
		if(dist < 0.0f) {
			if(i == 0) {
				outColor.rgb *= vec3(1.0, 0.0, 0.0);
				break;
			} else if(i == 1) {
				outColor.rgb *= vec3(0.0, 1.0, 0.0);
				break;
			} else if(i == 2) {
				outColor.rgb *= vec3(0.0, 0.0, 1.0);
				break;
			} else if(i == 3) {
				outColor.rgb *= vec3(1.0, 1.0, 0.0);
				break;
			} else {
				outColor.rgb *= vec3(0.0, 1.0, 1.0);
				break;
			}
		}
	}
}