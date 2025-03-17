#version 330 core

layout (location = 0) in vec2 aPosition;
layout (location = 1) in float aDensity;
layout (location = 2) in vec2 aVelocity;

out vec3 Color;

uniform bool drawingDensity;

const float mixCorrection = 7;

void main() {
    vec2 pos = aPosition * 2.0 - 1.0;
    gl_Position = vec4(pos, 1.0, 1.0);
    
    if (drawingDensity) {
        // For the main display
        // float speed = aVelocity.x + aVelocity.y;
        float speed = dot(aVelocity, aVelocity);
        Color = aDensity * mix(vec3(0,0,1), vec3(1,0,1), clamp(speed * mixCorrection, 0, 1));
    } else {
        // For visualizing velocity field
        Color = vec3(1);
    }
}