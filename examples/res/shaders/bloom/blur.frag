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

layout(std140, set = 2, binding = 0) uniform BlurUniform {
	vec2 dir;
	int pixelStep;
} blurParams;

layout(set = 1, binding = 0) uniform sampler2D screen;

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 outColor;


// from mattdesl
void main() {
	vec4 sum = vec4(0.0);
	
	float blur = blurParams.pixelStep / global.screenRes.x; 
    
	float hstep = blurParams.dir.x;
	float vstep = blurParams.dir.y;
    
	sum += texture(screen, vec2(uv.x - 4.0*blur*hstep, uv.y - 4.0*blur*vstep)) * 0.0162162162;
	sum += texture(screen, vec2(uv.x - 3.0*blur*hstep, uv.y - 3.0*blur*vstep)) * 0.0540540541;
	sum += texture(screen, vec2(uv.x - 2.0*blur*hstep, uv.y - 2.0*blur*vstep)) * 0.1216216216;
	sum += texture(screen, vec2(uv.x - 1.0*blur*hstep, uv.y - 1.0*blur*vstep)) * 0.1945945946;
	
	sum += texture(screen, vec2(uv.x, uv.y)) * 0.2270270270;
	
	sum += texture(screen, vec2(uv.x + 1.0*blur*hstep, uv.y + 1.0*blur*vstep)) * 0.1945945946;
	sum += texture(screen, vec2(uv.x + 2.0*blur*hstep, uv.y + 2.0*blur*vstep)) * 0.1216216216;
	sum += texture(screen, vec2(uv.x + 3.0*blur*hstep, uv.y + 3.0*blur*vstep)) * 0.0540540541;
	sum += texture(screen, vec2(uv.x + 4.0*blur*hstep, uv.y + 4.0*blur*vstep)) * 0.0162162162;

	outColor = vec4(sum.rgb, 1.0);
}