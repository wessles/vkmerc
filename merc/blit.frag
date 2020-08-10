#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
	float d = 0.01;
	vec2 ro = d * vec2(0.0, -1.0);
	vec2 go = d * vec2(-1.0, sqrt(3));
	vec2 bo = d * vec2(1.0, sqrt(3));
	outColor.r = texture(texSampler, fragTexCoord + vec2(0.01, 0.01)).r;
	outColor.g = texture(texSampler, fragTexCoord + vec2(-0.01, 0.01)).g;
	outColor.b = texture(texSampler, fragTexCoord + vec2(0.0, -0.01)).b;
	outColor.a = 1.0;
}