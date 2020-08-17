#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform UniformBufferObject {
	mat4 light;
    mat4 model;
    mat4 view;
    mat4 proj;
	vec4 lightDir;
} ubo;

layout(binding = 1) uniform sampler2D texSampler;
layout(binding = 2) uniform samplerCube environment;
layout(binding = 3) uniform sampler2D shadowMap;

layout(location = 0) in vec3 fragPosition;
layout(location = 1) in vec3 fragColor;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec3 fragNormal;

layout(location = 0) out vec4 outColor;

const int pcfCount = 5;
const float totalTexels = (pcfCount * 2.0 + 1.0) * (pcfCount * 2.0 + 1.0);
const float shadowmapSize = 256.0;
const float shadowTexelSize = 1.0 / shadowmapSize;

float rand(vec4 seed4) {
	float dot_product = dot(seed4, vec4(12.9898,78.233,45.164,94.673));
	return fract(sin(dot_product) * 43758.5453);
}

float evalShadow() {
	vec4 projected = ubo.light * vec4(fragPosition, 1.0);
	// translate from NDC to texture coordinates
    vec2 coord = projected.xy / (projected.w * 2.0) - 0.5;
    
	float shadowing = 0.0;
	for(int x = -pcfCount; x <= pcfCount; x++) {
		for(int y = -pcfCount; y <= pcfCount; y++) {
			shadowing += float((projected.z / projected.w) < (texture(shadowMap, coord + vec2(x, y) * shadowTexelSize * rand(projected+vec4(x,y,x,y) * 2.0)).x + 0.05));
		}
	}
	
	shadowing /= totalTexels;

	return shadowing;
}

void main() {
	outColor = texture(texSampler, fragTexCoord);

	vec3 eyePos = (inverse(ubo.view) * vec4(0., 0., 0., 1.)).xyz;
	vec3 dir = normalize(fragPosition - eyePos);

    float shadow = evalShadow();
	outColor.rgb *= max(shadow, 0.03);
	outColor.rgb *= max(dot(ubo.lightDir.xyz, normalize(fragNormal)), 0.);

	//	vec3 reflection = texture(environment, reflect(dir, normalize(fragNormal))).rgb;
	//	outColor.rgb = mix(outColor.rgb, reflection, 0.03);

	// normals
	// outColor.rgb = normalize(fragNormal.xyz) * 0.5 + .5;
	// position
	// outColor.rgb = fragPosition.xyz;

	//	outColor.rg = mod(shadowmapProj.xy, 1.0);
	//	outColor.b = 0.0;
}