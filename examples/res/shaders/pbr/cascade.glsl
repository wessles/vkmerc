layout(std140, binding = 1) uniform CascadesUniform {
	mat4 cascades[4];
	vec4 data[4];
	mat4 cameraFrust;
	vec2 clipPlanes;
} cascades;

layout(set=1, binding=0) uniform sampler2DShadow cascade;

vec2 transformToCascadeQuadrant(vec2 p, int i) {
	p = (p+1.0)/4.0;
	vec2 o = vec2(0.0);
	// {0, 1, 2, 3} --> {0, 0.5, 0, 0.5}
	o.y = float(i % 2) / 2.0;
	// {0, 1, 2, 3} --> {0, 0, 0.5, 0.5}
	o.x = floor(float(i) / 2.0) / 2.0;
	return o+p;
}

// from iq, the wise one
float sdBox( in vec2 p, in vec2 b )
{
    vec2 d = abs(p)-b;
    return length(max(d,0.0)) + min(max(d.x,d.y),0.0);
}

// query the shadowmap cascades. 0 = shadow, 1 = lit
float exposureToSun(vec3 position) {
	vec4 cascadeProj;
	float distToBorder;
	float bias, slopeBias;
	
	bool foundMatch = false;
	
	for(int i = 0; i < 4; i++) {
		cascadeProj = cascades.cascades[i] * vec4(inPosition, 1.0);
		float dist = sdBox(cascadeProj.xy, vec2(1.0, 1.0));
		if(dist < 0.0f) {
			cascadeProj = cascades.cascades[i] * vec4(position, 1.0);
			cascadeProj.xy = transformToCascadeQuadrant(cascadeProj.xy, i);
			bias = cascades.data[i][0];
			slopeBias = cascades.data[i][1];
			foundMatch = true;
			break;
		}
	}

	if(!foundMatch) return 1.0;

	// slope scale biasing
	float NoL = max(0.0, dot(-normalize(inNormal), global.directionalLight.xyz));
	float totalBias = bias + slopeBias * tan(acos(NoL));

	return float(texture(cascade, cascadeProj.xyz - vec3(0., 0., totalBias)) > 0);
}