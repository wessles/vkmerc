#version 450

// PN patch data
struct PnPatch
{
 float b210;
 float b120;
 float b021;
 float b012;
 float b102;
 float b201;
 float b111;
 float n110;
 float n011;
 float n101;
};

#define uTessAlpha 3./4.

layout(binding = 0) uniform UniformBufferObject {
	mat4 light;
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

//Layout specification.
layout (triangles, equal_spacing, cw) in;
 
//In parameters.
layout(location = 0) in vec3 inPosition[];
layout(location = 1) in vec3 inColor[];
layout(location = 2) in vec2 inTexCoord[];
layout(location = 3) in vec3 inNormal[];
layout(location = 4) in PnPatch inPnPatch[];
 
//Out parameters.
layout(location = 0) out vec3 fragPosition;
layout(location = 1) out vec3 fragColor;
layout(location = 2) out vec2 fragTexCoord;
layout(location = 3) out vec3 fragNormal;
 
#define b300    gl_in[0].gl_Position.xyz
#define b030    gl_in[1].gl_Position.xyz
#define b003    gl_in[2].gl_Position.xyz
#define n200    inNormal[0]
#define n020    inNormal[1]
#define n002    inNormal[2]
#define uvw     gl_TessCoord

void main()
{
	vec3 uvwSquared = uvw*uvw;
	vec3 uvwCubed   = uvwSquared*uvw;

	// extract control points
	vec3 b210 = vec3(inPnPatch[0].b210, inPnPatch[1].b210, inPnPatch[2].b210);
	vec3 b120 = vec3(inPnPatch[0].b120, inPnPatch[1].b120, inPnPatch[2].b120);
	vec3 b021 = vec3(inPnPatch[0].b021, inPnPatch[1].b021, inPnPatch[2].b021);
	vec3 b012 = vec3(inPnPatch[0].b012, inPnPatch[1].b012, inPnPatch[2].b012);
	vec3 b102 = vec3(inPnPatch[0].b102, inPnPatch[1].b102, inPnPatch[2].b102);
	vec3 b201 = vec3(inPnPatch[0].b201, inPnPatch[1].b201, inPnPatch[2].b201);
	vec3 b111 = vec3(inPnPatch[0].b111, inPnPatch[1].b111, inPnPatch[2].b111);

	// extract control normals
	vec3 n110 = normalize(vec3(inPnPatch[0].n110,
							inPnPatch[1].n110,
							inPnPatch[2].n110));
	vec3 n011 = normalize(vec3(inPnPatch[0].n011,
							inPnPatch[1].n011,
							inPnPatch[2].n011));
	vec3 n101 = normalize(vec3(inPnPatch[0].n101,
							inPnPatch[1].n101,
							inPnPatch[2].n101));

	// compute texcoords
	fragTexCoord = gl_TessCoord[2]*inTexCoord[0]
			+ gl_TessCoord[0]*inTexCoord[1]
			+ gl_TessCoord[1]*inTexCoord[2];

	// normal
	vec3 barNormal = gl_TessCoord[2]*inNormal[0]
				+ gl_TessCoord[0]*inNormal[1]
				+ gl_TessCoord[1]*inNormal[2];
	vec3 pnNormal  = n200*uvwSquared[2]
				+ n020*uvwSquared[0]
				+ n002*uvwSquared[1]
				+ n110*uvw[2]*uvw[0]
				+ n011*uvw[0]*uvw[1]
				+ n101*uvw[2]*uvw[1];
	fragNormal = uTessAlpha*pnNormal + (1.0-uTessAlpha)*barNormal;


	// compute interpolated pos
	vec3 barPos = gl_TessCoord[2]*b300
				+ gl_TessCoord[0]*b030
				+ gl_TessCoord[1]*b003;

	// save some computations
	uvwSquared *= 3.0;

	// compute PN position
	vec3 pnPos  = b300*uvwCubed[2]
				+ b030*uvwCubed[0]
				+ b003*uvwCubed[1]
				+ b210*uvwSquared[2]*uvw[0]
				+ b120*uvwSquared[0]*uvw[2]
				+ b201*uvwSquared[2]*uvw[1]
				+ b021*uvwSquared[0]*uvw[1]
				+ b102*uvwSquared[1]*uvw[2]
				+ b012*uvwSquared[1]*uvw[0]
				+ b111*6.0*uvw[0]*uvw[1]*uvw[2];

	// final position and normal
	vec3 finalPos = (1.0-uTessAlpha)*barPos + uTessAlpha*pnPos;

	fragColor = gl_TessCoord[2]*inColor[0] + gl_TessCoord[0]*inColor[1] + gl_TessCoord[1]*inColor[2];

    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(finalPos, 1.0f);
	fragPosition = (ubo.model * vec4(finalPos, 1.0)).xyz;
	fragNormal = normalize((ubo.model * vec4(fragNormal, 0.0)).xyz);
}