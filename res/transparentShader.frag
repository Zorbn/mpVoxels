#version 450

layout(location = 0) in vec4 fragColor;
layout(location = 1) in float fragFogDepth;
layout(location = 2) in vec4 fragFogColor;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = mix(fragColor, fragFogColor, min(fragFogDepth, 1));
}
