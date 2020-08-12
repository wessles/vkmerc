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

layout(location = 0) in vec3 fragPosition;
layout(location = 1) in vec3 fragColor;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec3 fragNormal;

layout(location = 0) out vec4 outColor;

void main() {
	outColor = texture(texSampler, fragTexCoord);
	
	vec3 eyePos = (inverse(ubo.view) * vec4(0., 0., 0., 1.)).xyz;
	vec3 dir = normalize(fragPosition - eyePos);

	vec3 reflection = texture(environment, reflect(dir, normalize(fragNormal))).rgb;
	outColor.rgb = mix(outColor.rgb, reflection, 1.0);
	// normals
	// outColor.rgb = normalize(fragNormal.xyz) * 0.5 + .5;
	// position
	// outColor.rgb = fragPosition.xyz;
}