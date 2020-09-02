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

void main() {
	vec2 pixel = fragCoord * global.screenRes;
	float padding = 150.0;
	float offsetMultiplier = 1.0 - min(
		min(pixel.x / padding, 1.0),
		min((global.screenRes.x - pixel.x) / padding, 1.0)
	) * min(
		min(pixel.y / padding, 1.0),
		min((global.screenRes.y - pixel.y) / padding, 1.0)
	);
	offsetMultiplier = 1.0 - (offsetMultiplier * offsetMultiplier);

	vec2 uv = fragCoord;
	float amp = 1.0 / 35.0;
	float wavelength = 100.0;
	uv += vec2(1.0, 0.0) * offsetMultiplier * sin(pixel.x / wavelength + global.time) * amp;
	uv += vec2(0.0, 1.0) * offsetMultiplier * cos(pixel.x / wavelength + global.time) * amp;

	outColor = texture(screenbuffer, uv);
}