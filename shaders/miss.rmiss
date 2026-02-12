#version 460
#extension GL_EXT_ray_tracing : require

// Extended ray payload (must match raygen and closesthit)
struct RayPayload {
    vec3 color;
    vec3 origin;
    vec3 direction;
    uint seed;
    bool hit;
    bool scattered;
    vec3 worldPosition;
    vec3 worldNormal;
    uint instanceId;
    float hitT;
};

layout(location = 0) rayPayloadInEXT RayPayload payload;

void main() {
    payload.hit = false;
    payload.scattered = false;
    
    // Clear G-buffer data for miss
    payload.worldPosition = vec3(0.0);
    payload.worldNormal = vec3(0.0);
    payload.instanceId = 0xFFFFFFFF;
    payload.hitT = -1.0;
    
    // Sky gradient background
    vec3 unit_direction = normalize(gl_WorldRayDirectionEXT);
    float a = 0.5 * (unit_direction.y + 1.0);
    vec3 background_color = (1.0 - a) * vec3(1.0, 1.0, 1.0) + a * vec3(0.5, 0.7, 1.0);
    
    payload.color = background_color;
}