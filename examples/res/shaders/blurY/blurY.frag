#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(std140, binding = 0) uniform GlobalUniform {
	mat4 view;
	mat4 proj;
	vec4 camPos;
	vec2 screenRes;
	float time;
} global;

layout(set=1, binding = 0) uniform sampler2D screenbuffer;

layout(location = 0) in vec2 fragCoord;

layout(location = 0) out vec4 outColor;

const float spreadMult = 4.0;
const int samples = 16;

void main() {
	vec2 pixel = fragCoord * global.screenRes;

	float pixelSize = spreadMult / global.screenRes.y;

	outColor = vec4(0.0, 0.0, 0.0, 1.0);
	for(int i = -int(floor(samples / 2.0)); i < int(ceil(samples / 2.0)); i++) {
		outColor += texture(screenbuffer, fragCoord + vec2(0.0, i * pixelSize)) / samples;
	}

	const float x = 0.4;
	outColor.rgb = 1.0 - (outColor.rgb - x);
	outColor.rgb = 1.0 - (outColor.rgb * outColor.rgb * outColor.rgb * outColor.rgb);
	outColor = clamp(outColor, 0.0, 1.0);
	outColor.rgb = vec3(outColor.r + outColor.g + outColor.g) / 3.0;
}