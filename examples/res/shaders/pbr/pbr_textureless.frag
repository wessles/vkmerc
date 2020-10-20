#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(std140, binding = 0) uniform GlobalUniform {
	mat4 cascade0;
	mat4 cascade1;
	mat4 cascade2;
	mat4 view;
	mat4 proj;
	vec4 camPos;
	vec4 directionalLight;
	vec2 screenRes;
	float time;
} global;

layout(set=1, binding=0) uniform sampler2D cascade;

layout(set=2, binding=0, std140) uniform PbrUniform {
	vec4 albedo;
	vec4 emissive;
	float metallic;
	float roughness;
} pbr;

layout(set=2, binding=1) uniform samplerCube tex_spec_ibl;
layout(set=2, binding=2) uniform samplerCube tex_diffuse_ibl;
layout(set=2, binding=3) uniform sampler2D tex_brdf_lut;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;
layout(location = 4) in vec4 inTangent;

layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;

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

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}


const int pcfCount = 2;
const float totalTexels = (pcfCount * 2.0 + 1.0) * (pcfCount * 2.0 + 1.0);
const float shadowmapSize = 1024.0;
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

// query the shadowmap cascades. 0 = shadow, 1 = lit
float exposureToSun() {
	float cascadeScale = 1.0;
	vec4 cascadeProj4Vec = global.cascade0 * vec4(inPosition, 1.0);
	vec2 cascadeProj = cascadeProj4Vec.xy;
	if(cascadeProj.x > -1.0 && cascadeProj.y > -1.0 && cascadeProj.x < 1.0 && cascadeProj.y < 1.0) {
		// cascade 0 is in quadrant 1 of cascade texture, from <0 0> to <0.5, 0.5>
		cascadeProj = (cascadeProj+1.0) / 4.0; // -1,1 space -> 0,1 space -> quadrant 1
		cascadeScale = 50. / 100.;
	} else {
		cascadeProj4Vec = global.cascade1 * vec4(inPosition, 1.0);
		cascadeProj = cascadeProj4Vec.xy;
		if(cascadeProj.x > -1.0 && cascadeProj.y > -1.0 && cascadeProj.x < 1.0 && cascadeProj.y < 1.0) {
			// cascade 1 is in quadrant 1 of cascade texture, from <0 0.5> to <1.0, 0.5>
			cascadeProj = (cascadeProj+1.0) / 4.0 + vec2(0.0, 0.5); // -1,1 space -> 0,1 space -> quadrant 2
			cascadeScale = 8. / 100.;
		} else {
			cascadeProj4Vec = global.cascade2 * vec4(inPosition, 1.0);
			cascadeProj = cascadeProj4Vec.xy;
			if(cascadeProj.x > -1.0 && cascadeProj.y > -1.0 && cascadeProj.x < 1.0 && cascadeProj.y < 1.0) {
				// cascade 2 is in quadrant 3 of cascade texture, from <0.5 0> to <1.0, 0.5>
				cascadeProj = (cascadeProj+1.0) / 4.0 + vec2(0.5, 0.0); // -1,1 space -> 0,1 space -> quadrant 3
			}
		}
	}
	
	float shadowing = 1.0;
	for (int i=0; i<16; i++){
		float poissonAtten = random(vec4(cascadeProj.xy, 0.0, 0.0));
		
		vec2 poisson = poissonDisk[i] * cascadeScale / 100.0;
		poisson = rotate(poisson, poissonAtten);
				
		vec2 samplept = cascadeProj.xy + poisson;
		if(samplept.x > 1.0 || samplept.x < 0.0 || samplept.y > 1.0 || samplept.y < 0.0) {
			continue;
		}
		shadowing -= (1.0/(16.0)) * (1.0 - float(texture(cascade, samplept).r - cascadeProj4Vec.z > 0.001));
	}

	return shadowing;
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

	return (Ks * c / PI + D*F*G/(4*WoDotN * WiDotN)) * WiDotN * exposureToSun();
}

vec3 reflectance(vec3 c, vec3 p, vec3 n, vec3 wo, float m, float r, vec3 F0) {
	return brdf(c, n, wo, -global.directionalLight.xyz, m, r, F0);
}

vec3 getNormal() {
	return normalize(inNormal);
}

void main()
{
	vec3 albedo = pbr.albedo.rgb;
	vec3 emissive = pow(pbr.emissive.rgb, vec3(2.2));
	float roughness = pbr.roughness;
	float metallic = pbr.metallic;
	
	// no AO in textureless mode
	//float ao = 1.0;

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
		
		vec3 ambient = (kD * diffuse + spec);

#ifdef AMBIENT_FACTOR
		ambient *= AMBIENT_FACTOR;
#endif
		
		outColor.rgb += ambient;
	}
	
	// emissive lighting
	{
		outColor.rgb += emissive;
	}
	
	outColor.rgb = pow(outColor.rgb, vec3(1.0/2.0));
}