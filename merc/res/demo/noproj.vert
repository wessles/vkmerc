#version 450
#extension GL_ARB_separate_shader_objects : enable

// vertex info
layout(location = 0) in vec3 inPosition;
layout(location = 2) in vec2 inTexCoord;

// output
layout(location = 0) out vec2 fragTexCoord;

void main() {
    gl_Position = vec4(inPosition, 1.0);
    fragTexCoord = inTexCoord;
}