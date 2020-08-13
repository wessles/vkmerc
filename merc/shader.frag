#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform UniformBufferObject {
	mat4 light;
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(binding = 1) uniform sampler2D texSampler;
layout(binding = 2) uniform samplerCube environment;
layout(binding = 3) uniform sampler2D shadowMap;

layout(location = 0) in vec3 fragPosition;
layout(location = 1) in vec3 fragColor;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec3 fragNormal;

layout(location = 0) out vec4 outColor;

void main() {
	outColor = texture(texSampler, fragTexCoord);

	vec3 eyePos = (inverse(ubo.view) * vec4(0., 0., 0., 1.)).xyz;
	vec3 dir = normalize(fragPosition - eyePos);


    vec4 shadowmapProj = ubo.light * vec4(fragPosition, 1.0);
    vec2 coord = (shadowmapProj.xy / vec2(shadowmapProj.w * 2.0)) - vec2(0.5);
    float light = texture(shadowMap, coord).x;
	float shadowing = float(shadowmapProj.z/shadowmapProj.w < light + 0.0001);
	shadowing *= max(0., dot(vec3(1., 1., 1.), normalize(fragNormal)));
	shadowing = clamp(shadowing, 0.0, 1.0);
	outColor.rgb *= shadowing;

	vec3 reflection = texture(environment, reflect(dir, normalize(fragNormal))).rgb;
	outColor.rgb = mix(outColor.rgb, reflection, 0.03);
	// normals
	// outColor.rgb = normalize(fragNormal.xyz) * 0.5 + .5;
	// position
	// outColor.rgb = fragPosition.xyz;

//	outColor.rg = mod(shadowmapProj.xy, 1.0);
//	outColor.b = 0.0;
}