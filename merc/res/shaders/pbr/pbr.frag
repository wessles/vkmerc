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

const int MAX_REFLECTION_LOD = 6;

const float PI = 3.14159265359;

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

// cook-torrance BRDF
vec3 brdf(vec3 c, vec3 n, vec3 wo, vec3 wi, float m, float r) {
	vec3 h = normalize(wo+wi);
	float D = ndf(n, h, r);
	float F = fresnel(n, wo, m);
	float G = GeometrySmith(n, wo, wi, r);
	float WoDotN = max(0.0, dot(wo, n));
	float WiDotN = max(0.0, dot(wi, n));
	
	float Ks = 1.0 - F;

	return (Ks * c / PI + vec3(D*F*G/(4*WoDotN * WiDotN))) * WiDotN;
}

// p  = frag position
// n  = frag normal
// wo = direction to viewer
vec3 reflectance(vec3 c, vec3 p, vec3 n, vec3 wo, float m, float r) {
	return brdf(c, n, wo, -global.directionalLight.xyz, m, r);
}

void main()
{

	vec3 normal      = normalize(inNormal);
	vec3 tangent     = normalize(inTangent.xyz);
	vec3 bitangent   = cross(inNormal, inTangent.xyz) * inTangent.w;
	mat3 TBN         = mat3(tangent, bitangent, normal);
	vec3 localNormal = texture(tex_normal, inTexCoord).xyz;
	normal           = normalize(TBN * localNormal);

	vec2 m_r = texture(tex_metal_rough, inTexCoord).xy;
	
	// metalicity
	float m = m_r.r;
	//float m = 1.0;
	m = mix(0.05, 0.95, m);
	
	// roughness
	//float r = mix(0.05, 0.95, m_r.g);
	float r = m_r.g;
	
	// frag position to eye direction vector
	vec3 Wo = normalize(global.camPos.xyz - inPosition);
	
	outColor.a = 1.0;
	
	// per-source lighting
	vec3 c = texture(tex_albedo, inTexCoord).rgb;
    outColor.rgb = max(vec3(0.0), reflectance(c, inPosition, normal, Wo, m, r));
	
	// IBL ambient lighting
	vec3 F = fresnelRoughness(max(dot(normal, Wo), 0.0), mix(vec3(0.04), c, m), r);
	vec3 kS = F;
    vec3 kD = 1.0 - kS;
    kD *= 1.0 - m;
	
	vec3 prefilteredColor = textureLod(tex_spec_ibl, reflect(normal, Wo),  r * MAX_REFLECTION_LOD).rgb;
    vec2 brdf  = texture(tex_brdf_lut, vec2(max(dot(normal, Wo), 0.0), r)).rg;
    vec3 spec = prefilteredColor * (F * brdf.x + brdf.y);
	
	vec3 diffuse = textureLod(tex_diffuse_ibl, normal, r*MAX_REFLECTION_LOD).xyz * c;
	
	vec3 ambient = (kD * diffuse + spec) * texture(tex_ao, inTexCoord).r;
	
	outColor.rgb += ambient;
	
	outColor.rgb += texture(tex_emissive, inTexCoord).rgb;
}