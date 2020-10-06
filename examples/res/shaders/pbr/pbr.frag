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

layout(set=2, binding=0) uniform sampler2D tex_albedo;
layout(set=2, binding=1) uniform sampler2D tex_normal;
layout(set=2, binding=2) uniform sampler2D tex_metal_rough;
layout(set=2, binding=3) uniform sampler2D tex_emissive;
layout(set=2, binding=4) uniform sampler2D tex_ao;
layout(set=2, binding=5) uniform samplerCube tex_spec_ibl;
layout(set=2, binding=6) uniform samplerCube tex_diffuse_ibl;
layout(set=2, binding=7) uniform sampler2D tex_brdf_lut;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;
layout(location = 4) in vec4 inTangent;

layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;

/*
float GeometrySchlickGGX(float NdotV, float k)
{
    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return nom / denom;
}
float GeometrySmith(vec3 N, vec3 V, vec3 L, float k)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx1 = GeometrySchlickGGX(NdotV, k);
    float ggx2 = GeometrySchlickGGX(NdotL, k);
	
    return ggx1 * ggx2;
}

// schlick approximation of fresnel
float fresnel(vec3 n, vec3 v, float Fo) {
	float x = (1.0 - dot(n, v));
	return Fo + (1.0 - Fo) * (x*x*x*x*x);
}

vec3 fresnelRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}   

// GGX Trowbridge-Reitz NDF
float ndf(vec3 n, vec3 h, float r)
{
    float a2     = r*r;
    float NdotH  = max(dot(n, h), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float denom  = (NdotH2 * (a2 - 1.0) + 1.0);
    denom        = PI * denom * denom;
	
    return a2 / denom;
}
*/

// ----------------------------------------------------------------------------
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}
// ----------------------------------------------------------------------------
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}
// ----------------------------------------------------------------------------
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}
// ----------------------------------------------------------------------------
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}
// ----------------------------------------------------------------------------
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}


// cook-torrance BRDF
vec3 brdf(vec3 c, vec3 n, vec3 wo, vec3 wi, float m, float r, vec3 F0) {
	vec3 h = normalize(wo+wi);
	float WoDotN = max(0.0, dot(wo, n));
	float WiDotN = max(0.0, dot(wi, n));
	float D = DistributionGGX(n, h, r);
	vec3 F = fresnelSchlickRoughness(WoDotN, F0, r);
	float G = GeometrySmith(n, wo, wi, r);
	
	vec3 Ks = 1.0 - F;

	return (Ks * c / PI + D*F*G/(4*WoDotN * WiDotN)) * WiDotN;
}

vec3 reflectance(vec3 c, vec3 p, vec3 n, vec3 wo, float m, float r, vec3 F0) {
	return brdf(c, n, wo, -global.directionalLight.xyz, m, r, F0);
}

vec3 getNormal() {
	vec3 normal      = normalize(inNormal);
	vec3 tangent     = normalize(inTangent.xyz);
	vec3 bitangent   = cross(inNormal, inTangent.xyz) * inTangent.w;
	mat3 TBN         = mat3(tangent, bitangent, normal);
	vec3 localNormal = texture(tex_normal, inTexCoord).xyz * 2.0 - vec3(1.0);
	return normalize(TBN * localNormal);
}

void main()
{
	vec3 albedo = pow(texture(tex_albedo, inTexCoord).rgb, vec3(2.2));
	vec2 m_r = texture(tex_metal_rough, inTexCoord).gb;
	float roughness = m_r.x;
	float metallic = m_r.y;
	float ao = texture(tex_ao, inTexCoord).r;

	vec3 N = getNormal();
	vec3 V = normalize(global.camPos.xyz - inPosition);
    vec3 R = reflect(-V, N); 

	// calculate reflectance at normal incidence; if dia-electric (like plastic) use F0 
    // of 0.04 and if it's a metal, use the albedo color as F0 (metallic workflow)    
    #define METALLIC
	#ifdef METALLIC
	vec3 F0 = albedo;
	#else
	vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, metallic);
	#endif
	
	outColor = vec4(0.0, 0.0, 0.0, 1.0);
	
	// per-source lighting
	{
		outColor.rgb += max(vec3(0.0), reflectance(albedo, inPosition, N, V, metallic, roughness, F0));
	}

	// IBL ambient lighting
	{
		// ambient lighting (we now use IBL as the ambient term)
		vec3 F = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
		
		vec3 kS = F;
		vec3 kD = 1.0 - kS;
		kD *= 1.0 - metallic;
		
		const float MAX_REFLECTION_LOD = 9.0;

		vec3 prefilteredColor = textureLod(tex_spec_ibl, R,  roughness * MAX_REFLECTION_LOD).rgb;
		vec2 brdf  = texture(tex_brdf_lut, vec2(max(dot(N, V), 0.0), roughness)).rg;
		vec3 spec = prefilteredColor * (F * brdf.x + brdf.y);
		
		vec3 diffuse = textureLod(tex_diffuse_ibl, N, roughness*MAX_REFLECTION_LOD).xyz * albedo;
		
		vec3 ambient = (kD * diffuse + spec) * ao;
		
		outColor.rgb += ambient;
	}
	
	// emissive lighting
	{
		outColor.rgb += pow(texture(tex_emissive, inTexCoord).rgb, vec3(2.2));
	}
	
	outColor.rgb = pow(outColor.rgb, vec3(1.0/2.0));
}