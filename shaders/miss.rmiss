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

void main() {
    // From camera.h ray_color:
    // if (!world.hit(r, interval(0.001, infinity), rec))
    //     return background;
    
    // Also from the commented code:
    // vec3 unit_direction = unit_vector(r.direction());
    // auto a = 0.5 * (unit_direction.y() + 1.0);
    // return (1.0 - a) * color(1.0, 1.0, 1.0) + a * color(0.5, 0.7, 1.0);
    
    payload.hit = false; // We didn't hit anything
    payload.scattered = false; // No scattering (ray terminates)
    
    // Sky gradient background
    vec3 unit_direction = normalize(gl_WorldRayDirectionEXT);
    float a = 0.5 * (unit_direction.y + 1.0);
    vec3 background_color = (1.0 - a) * vec3(1.0, 1.0, 1.0) + a * vec3(0.5, 0.7, 1.0);
    
    payload.color = background_color;
}
