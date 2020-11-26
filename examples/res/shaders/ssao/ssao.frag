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

layout(set=2, binding=0) uniform SSAOParams {
	float near;
	float far;
	float intensity;
} params;

layout(set=1, binding=0) uniform sampler2D position;
layout(set=1, binding=1) uniform sampler2D normal;
layout(set=1, binding=2) uniform sampler2D depth;
layout(set=1, binding=3) uniform sampler2D inOcclusion;

layout(location = 0) in vec2 uv;

layout(location = 0) out float occlusion;

float noise(vec2 n) { 
	return fract(sin(dot(n, vec2(12.9898, 4.1414))) * 43758.5453);
}

// thanks to nojima
// https://www.shadertoy.com/view/ttc3zr
// 3 outputs, 2 inputs
uvec3 murmurHash32(uvec2 src) {
    const uint M = 0x5bd1e995u;
    uvec3 h = uvec3(1190494759u, 2147483647u, 3559788179u);
    src *= M; src ^= src>>24u; src *= M;
    h *= M; h ^= src.x; h *= M; h ^= src.y;
    h ^= h>>13u; h *= M; h ^= h>>15u;
    return h;
}
vec3 hash32(vec2 src) {
    uvec3 h = murmurHash32(floatBitsToUint(src));
    return uintBitsToFloat(h & 0x007fffffu | 0x3f800000u) - 1.0;
}


const vec3 randomPointsOnSphere[] = {
	vec3(0.537262630720041,0.289368696401469,0.32124543372977354),
	vec3(-0.037960198036538495,0.25511758876840374,-0.549033134461611),
	vec3(0.39289327379867633,0.369377267531391,-0.5852172082802685),
	vec3(-0.672809464823107,0.19011107191437415,-0.09923590571910867),
	vec3(0.5267526207764024,0.17989339354464262,-0.33518060579870057),
	vec3(-0.07273624725041605,0.2425492310337477,-0.06089212412889755),
	vec3(0.4813941262104505,0.11875019298501877,-0.17862176556732734),
	vec3(-0.5032574401534604,0.08117835750389701,-0.08775738391993315),
	vec3(-0.046370809830639126,0.11779902354060012,0.6404052114451055),
	vec3(-0.2004783239912149,0.48161038770631226,0.657705748123935),
	vec3(0.007805849491585026,0.41175859476034526,0.5122628668632851),
	vec3(-0.41290997549239417,0.45943467232033386,0.029985638203068654),
	vec3(0.48101171289295275,0.028880900891200878,0.36824670250552405),
	vec3(0.006777025392717073,0.5246617647894474,0.07596661700740825),
	vec3(-0.5992992784166986,0.44813340795871637,0.3381719657337511),
	vec3(0.536520853301774,0.36930817931190596,0.06609281762428276)
};

float linearDepth(float s) {
	return params.near *params.far / (params.far + s * (params.near - params.far));
}

void main() {
	vec3 p = texture(position,uv).xyz;
	vec3 N = normalize(texture(normal,uv).xyz);
	float z = texture(depth,uv).r;

	vec3 T = normalize(hash32(uv)-vec3(0.5));
	vec3 B = cross(T, N);
	T = cross(B, N);

	mat3 basis = mat3(B,N,T);

	occlusion = 1.0;

	float r = params.intensity;

	for(int i = 0; i < 16; i++) {
		vec3 x = r*(basis * randomPointsOnSphere[i]);
		vec4 samplePoint = vec4(p + x, 1.0);
		samplePoint = global.proj * global.view * samplePoint;
		samplePoint /= samplePoint.w;
		float sampleZ = linearDepth(samplePoint.z);
		float bufferZ = linearDepth(texture(depth, 0.5*(samplePoint.xy+vec2(1.0))).r);
		float diff = sampleZ - bufferZ;
		if(sampleZ > bufferZ && diff < r) {
			occlusion -= 1.0/16.0;
		}
	}

	occlusion *= texture(inOcclusion,uv).r;
}