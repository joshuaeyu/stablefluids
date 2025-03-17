#version 330 core

layout (location = 0) in vec2 aPosition;
layout (location = 1) in float aDensity;
layout (location = 2) in vec2 aVelocity;

out vec3 Color;

uniform bool drawingDensity;

void main() {
    vec2 pos = aPosition * 2.0 - 1.0;
    gl_Position = vec4(pos, 1.0, 1.0);
    if (drawingDensity) {
        float speed = aVelocity.x + aVelocity.y;
        Color = aDensity * mix(vec3(0,0,1), vec3(1,0,0), speed);
    } else {
        Color = vec3(1);
    }
}