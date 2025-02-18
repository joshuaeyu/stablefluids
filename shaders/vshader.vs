#version 330 core

layout (location = 0) in vec2 aPosition;
layout (location = 1) in vec3 aColor;

out vec3 Color;

void main() {
    vec2 pos = aPosition * 2.0 - 1.0;
    gl_Position = vec4(pos, 1.0, 1.0);
    Color = aColor;
}