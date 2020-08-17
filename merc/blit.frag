#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 1) uniform sampler2D texSampler;
layout(binding = 2) uniform sampler2D shadowTex;

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
	/*
	float d = 0.002;
	vec2 ro = d * vec2(0.0, -1.0);
	vec2 go = d * vec2(-0.5, sqrt(3)/2.0);
	vec2 bo = d * vec2(0.5, sqrt(3)/2.0);
	outColor.r = texture(texSampler, fragTexCoord + ro).r;
	outColor.g = texture(texSampler, fragTexCoord + go).g;
	outColor.b = texture(texSampler, fragTexCoord + bo).b;
	outColor.a = 1.0;
	*/

	outColor = texture(texSampler, fragTexCoord);

	float size = 0.3;
	if(fragTexCoord.x < size  && fragTexCoord.y < size) {
		outColor.rgb = vec3(texture(shadowTex, fragTexCoord.xy / size).r);
	}

	outColor.a = 1.0;

// preview the shadow map
//	float x = 0.5;
//	outColor = vec4(vec3(max(0.0, texture(shadowTex, fragTexCoord).r - x) / (1.-x)), 1.0);
}