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
layout(binding = 3) uniform sampler2DShadow shadowMap;

layout(location = 0) in vec3 fragPosition;
layout(location = 1) in vec3 fragColor;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec3 fragNormal;

layout(location = 0) out vec4 outColor;

const vec2 poissonDisk[16] = vec2[](
	vec2( 0.0f, 0.0f ),
	vec2( -0.5766585165773277f, 0.8126243374282105f ),
	vec2( -0.9237677607727984f, -0.3579596327367057f ),
	vec2( 0.8062058671137168f, 0.5898398974519624f ),
	vec2( -0.1310995526149723f, -0.983252809818838f ),
	vec2( 0.8193682799756199f, -0.3337801777815311f ),
	vec2( 0.19911693963625532f, 0.9659833338241128f ),
	vec2( -0.8322523479006605f, 0.2363326276555715f ),
	vec2( 0.26186490285491026f, -0.6159577568298416f ),
	vec2( -0.3604179050541564f, -0.5069675343853017f ),
	vec2( 0.46657709893065247f, 0.20917002120473202f ),
	vec2( -0.15341973992963037f, 0.498484383101324f ),
	vec2( 0.9672633520951874f, 0.11761744355929109f ),
	vec2( -0.44629088116561516f, -0.051802399930734426f ),
	vec2( 0.6873546364199118f, -0.7197761501606675f ),
	vec2( 0.2934478567929312f, 0.5631166718844003f )
);

const int pcfCount = 2;
const float totalTexels = (pcfCount * 2.0 + 1.0) * (pcfCount * 2.0 + 1.0);
const float shadowmapSize = 2048.0;
const float shadowTexelSize = 1.0 / shadowmapSize;

float random(vec4 seed4) {
	float dot_product = dot(seed4, vec4(12.9898,78.233,45.164,94.673));
	return fract(sin(dot_product) * 43758.5453);
}

vec2 rotate(vec2 v, float a) {
	float s = sin(a);
	float c = cos(a);
	mat2 m = mat2(c, -s, s, c);
	return m * v;
}

float evalShadow() {
	vec4 projected = ubo.light * vec4(fragPosition, 1.0);
	// translate from NDC to texture coordinates (half-z in vulkan!)
    vec3 coord = vec3(projected.xy / (projected.w * 2.0) - 0.5, projected.z / projected.w);
    
	float shadowing = 1.0;

	for (int i=0; i<16; i++){
		const float x = 100.0;
		float poissonAtten = random(vec4(coord.xy, 0.0, 0.0));


		
		vec2 poisson = poissonDisk[i] / 100.0;
		poisson = rotate(poisson, poissonAtten);
				
		shadowing -= (1.0/(16.0)) * (1.0 - texture(shadowMap, coord + vec3(poisson, 0.0)));
	}

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