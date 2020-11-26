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

layout(set=1, binding=0) uniform sampler2D ao;

layout(location = 0) in vec2 uv;

layout(location = 0) out float occlusion;

void main() {
	vec3 p = texture(position,uv).xyz;
	vec3 N = normalize(texture(normal,uv).xyz);
	float z = texture(depth,uv).r;

	vec3 T = normalize(hash32(uv)-vec3(0.5));
	vec3 B = cross(T, N);
	T = cross(B, N);

	mat3 basis = mat3(T,B,N);

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