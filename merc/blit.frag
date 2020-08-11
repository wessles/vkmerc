#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
	float d = 0.002;
	vec2 ro = d * vec2(0.0, -1.0);
	vec2 go = d * vec2(-0.5, sqrt(3)/2.0);
	vec2 bo = d * vec2(0.5, sqrt(3)/2.0);
	outColor.r = texture(texSampler, fragTexCoord + ro).r;
	outColor.g = texture(texSampler, fragTexCoord + go).g;
	outColor.b = texture(texSampler, fragTexCoord + bo).b;
	outColor.a = 1.0;
}