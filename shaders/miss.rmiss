#version 460
#extension GL_EXT_ray_tracing : require

// Payload structure (must match raygen and closesthit)
struct RayPayload {
    vec3 color;           // Attenuation or final color
    vec3 origin;          // Next ray origin
    vec3 direction;       // Next ray direction
    uint seed;            // Random seed
    bool hit;             // Did we hit something?
    bool scattered;       // Should we continue tracing?
};

layout(location = 0) rayPayloadInEXT RayPayload payload;

// push constant block (raygen과 동일 레이아웃이어야 함)
layout(push_constant) uniform CameraPushConstants {
    vec3 position;
    vec3 forward;
    vec3 right;
    vec3 up;
    float vfov;
    float defocus_angle;
    float focus_dist;
    float padding;
    vec4 background;
} camera;

void main() {
    // 책의 ray_color: 아무것도 못 맞히면 background 반환
    payload.hit = false;       // We didn't hit anything
    payload.scattered = false; // No scattering (ray terminates)

    // 솔리드 배경색 (씬마다 push constant로 지정)
    payload.color = camera.background.rgb;
}
