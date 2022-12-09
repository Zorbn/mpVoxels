#version 450

layout(binding = 1) uniform sampler2DArray texSampler;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragTexCoord;
layout(location = 2) in float fragFogDepth;
layout(location = 3) in vec4 fragFogColor;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = texture(texSampler, fragTexCoord) * mix(vec4(fragColor, 1.0), fragFogColor, min(fragFogDepth, 1));
}