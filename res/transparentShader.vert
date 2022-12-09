#version 450

precision highp float;

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(binding = 1) uniform FogUniform {
    vec4 color;
    float maxDistance;
} fog;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;
layout(location = 3) in vec3 instancePos;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out float fragFogDepth;
layout(location = 2) out vec4 fragFogColor;

void main() {
    vec4 position = vec4(inPosition + instancePos, 1.0);
    gl_Position = ubo.proj * ubo.view * ubo.model * position;
    fragColor = inColor;

    vec4 fogPos = ubo.view * ubo.model * position;
    fragFogDepth = clamp(dot(fogPos, fogPos) / (fog.maxDistance * fog.maxDistance), 0.0, 1.0);
    fragFogColor = fog.color;
}
