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

#ifndef TEXTURELESS
layout(set=2, binding=0) uniform sampler2D tex_albedo;
layout(set=2, binding=1) uniform sampler2D tex_normal;
layout(set=2, binding=2) uniform sampler2D tex_metal_rough;
layout(set=2, binding=3) uniform sampler2D tex_emissive;
layout(set=2, binding=4) uniform sampler2D tex_ao;
#else
layout(set=2, binding=0, std140) uniform PbrUniform {
	vec4 albedo;
	vec4 emissive;
	float metallic;
	float roughness;
} pbr;
#endif

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;
layout(location = 4) in vec4 inTangent;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outEmissive;
layout(location = 2) out vec4 outMaterial;
layout(location = 3) out vec3 outNormal;
layout(location = 4) out vec3 outPosition;
layout(location = 5) out float outAO;

#ifndef TEXTURELESS
vec3 getNormal() {
	vec3 normal      = normalize(inNormal);
	vec3 tangent     = normalize(inTangent.xyz);
	vec3 bitangent   = cross(inNormal, inTangent.xyz) * inTangent.w;
	mat3 TBN         = mat3(tangent, bitangent, normal);
	vec3 localNormal = texture(tex_normal, inTexCoord).xyz * 2.0 - vec3(1.0);
	return normalize(TBN * localNormal);
}
#endif

void main()
{
#ifndef TEXTURELESS
	vec4 col = texture(tex_albedo, inTexCoord).rgba;
#ifdef ALPHA_CUTOFF
	if(col.a < ALPHA_CUTOFF) {
		discard;
	}
#endif
	outColor = vec4(col.rgb, 1.0);
	outEmissive = vec4(pow(texture(tex_emissive, inTexCoord).rgb, vec3(2.2)), 1.0);

	vec2 m_r = texture(tex_metal_rough, inTexCoord).gb;
	float roughness = m_r.x;
	float metallic = m_r.y;
	outMaterial = vec4(roughness, metallic, 0.0, 0.0);

	outNormal = getNormal();
	outAO = texture(tex_ao, inTexCoord).r;
#else
#ifdef ALPHA_CUTOFF
	if(col.a < ALPHA_CUTOFF) {
		discard;
	}
#endif
	outColor = pbr.albedo;
	outEmissive = pbr.emissive;

	outMaterial = vec4(pbr.roughness, pbr.metallic, 1.0, 0.0);

	outNormal = inNormal;
	outAO = 1.0;
#endif

	outPosition = inPosition;
}