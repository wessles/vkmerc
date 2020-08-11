#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform UniformBufferObject {
	vec3 light;
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(binding = 1) uniform sampler2D texSampler;
layout(binding = 2) uniform samplerCube environment;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec3 lightPos;
layout(location = 4) in vec3 fragPosition;
layout(location = 5) in vec3 lookDir;

layout(location = 0) out vec4 outColor;

float diffuse(vec3 l, vec3 n) {
	return clamp(dot(l, n), 0.0, 1.0);
}

float specular(vec3 l, vec3 n, vec3 v) {
	return pow(max(0.0, dot(reflect(v, n), l)), 5.);
}

float falloff(vec3 l, vec3 p) {
	float d = length(p - l);
	return 2.0 / (d*d);
}

void main() {
	outColor = texture(texSampler, fragTexCoord);
	outColor.rgb *= 0.03;


	vec3 eyePos = (inverse(ubo.view) * vec4(0., 0., 0., 1.)).xyz;
	vec3 dir = normalize(fragPosition - eyePos);

	vec3 lightDir = normalize(lightPos - fragPosition);

	float d = diffuse(lightDir, fragNormal);
	float s = specular(lightDir, fragNormal, lookDir);
	outColor.rgb *= .1 + falloff(lightPos, fragPosition) * d + s * 1.0;

	// gamma correction
	outColor.rgb = pow(outColor.rgb, vec3(1.0 / 2.2));

	outColor = texture(environment, reflect(normalize(dir), normalize(fragNormal)));
}