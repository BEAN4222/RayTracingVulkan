#version 460
#extension GL_EXT_ray_tracing : require

struct RayPayload {
    vec3 color;
    vec3 origin;
    vec3 direction;
    uint seed;
    bool hit;
    bool scattered;
};

// Sphere info structure (matches C++ SphereInfo, std430 layout)
struct SphereInfo {
    vec3 center;
    float radius;
    vec3 color;
    float materialType;
    float materialParam;
    float padding1;
    float padding2;
    float padding3;
};

layout(location = 0) rayPayloadInEXT RayPayload payload;
hitAttributeEXT vec2 attribs;

// Sphere info buffer (binding 2)
layout(binding = 2, set = 0, std430) readonly buffer SphereInfoBuffer {
    SphereInfo spheres[];
};

uint hash(uint x) {
    x += (x << 10u);
    x ^= (x >> 6u);
    x += (x << 3u);
    x ^= (x >> 11u);
    x += (x << 15u);
    return x;
}

float random_double(inout uint seed) {
    seed = hash(seed);
    return float(seed) / 4294967295.0;
}

float random_double_range(inout uint seed, float min, float max) {
    return min + (max - min) * random_double(seed);
}

vec3 random_vec3(inout uint seed, float min, float max) {
    return vec3(
        random_double_range(seed, min, max),
        random_double_range(seed, min, max),
        random_double_range(seed, min, max)
    );
}

vec3 random_unit_vector(inout uint seed) {
    for (int i = 0; i < 100; i++) {
        vec3 p = random_vec3(seed, -1.0, 1.0);
        float lensq = dot(p, p);
        if (1e-160 < lensq && lensq <= 1.0)
            return p / sqrt(lensq);
    }
    return vec3(0.0, 1.0, 0.0);
}

bool near_zero(vec3 v) {
    float s = 1e-8;
    return (abs(v.x) < s) && (abs(v.y) < s) && (abs(v.z) < s);
}

float reflectance(float cosine, float refraction_index) {
    float r0 = (1.0 - refraction_index) / (1.0 + refraction_index);
    r0 = r0 * r0;
    return r0 + (1.0 - r0) * pow((1.0 - cosine), 5.0);
}

const float MATERIAL_LAMBERTIAN = 0.0;
const float MATERIAL_METAL = 1.0;
const float MATERIAL_DIELECTRIC = 2.0;

void main() {
    payload.hit = true;
    
    vec3 world_pos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
    
    // Get sphere info from buffer using instance index
    int sphere_idx = gl_InstanceCustomIndexEXT;
    SphereInfo sphere = spheres[sphere_idx];
    
    vec3 sphere_center = sphere.center;
    vec3 albedo = sphere.color;
    float material_type = sphere.materialType;
    float material_param = sphere.materialParam;
    
    // outward_normal: always points from sphere center to surface (outward)
    vec3 outward_normal = normalize(world_pos - sphere_center);
    
    // front_face: which side of the surface is the ray coming from?
    bool front_face = dot(gl_WorldRayDirectionEXT, outward_normal) < 0.0;
    
    // normal: surface normal facing the ray
    vec3 normal = front_face ? outward_normal : -outward_normal;
    
    vec3 scattered_direction;
    vec3 attenuation;
    bool did_scatter = false;
    
    const float EPSILON = 0.001;
    
    // LAMBERTIAN
    if (abs(material_type - MATERIAL_LAMBERTIAN) < 0.1) {
        vec3 scatter_dir = normal + random_unit_vector(payload.seed);
        
        if (near_zero(scatter_dir)) {
            scatter_dir = normal;
        }
        
        scattered_direction = normalize(scatter_dir);
        attenuation = albedo;
        did_scatter = true;
    }
    // METAL
    else if (abs(material_type - MATERIAL_METAL) < 0.1) {
        vec3 unit_direction = normalize(gl_WorldRayDirectionEXT);
        vec3 reflected = reflect(unit_direction, normal);
        float fuzz = material_param;
        vec3 scattered = normalize(reflected) + (fuzz * random_unit_vector(payload.seed));

        if (near_zero(scattered)) {
            scattered = normal;
        }
        
        if (dot(scattered, normal) > 0.0) {
            scattered_direction = normalize(scattered);
            attenuation = albedo;
            did_scatter = true;
        } else {
            did_scatter = false;
        }
    }
    // DIELECTRIC
    else if (abs(material_type - MATERIAL_DIELECTRIC) < 0.1) {
        attenuation = vec3(1.0, 1.0, 1.0);
        
        float ri = front_face ? (1.0 / material_param) : material_param;
        vec3 unit_direction = normalize(gl_WorldRayDirectionEXT);
        float cos_theta = min(dot(-unit_direction, normal), 1.0);
        float sin_theta = sqrt(1.0 - cos_theta * cos_theta);
        
        bool cannot_refract = ri * sin_theta > 1.0;
        
        vec3 direction;
        
        if (cannot_refract || reflectance(cos_theta, ri) > random_double(payload.seed)) {
            // Reflect
            direction = reflect(unit_direction, normal);
        } else {
            // Refract
            direction = refract(unit_direction, normal, ri);
        }
        
        scattered_direction = normalize(direction);
        did_scatter = true;
    }
    
    if (did_scatter) {
        payload.scattered = true;
        payload.color = attenuation;
        
        // Offset origin based on scatter direction
        float offset_sign = dot(scattered_direction, outward_normal) > 0.0 ? 1.0 : -1.0;
        payload.origin = world_pos + outward_normal * (EPSILON * offset_sign);
        
        payload.direction = scattered_direction;
    } else {
        payload.scattered = false;
        payload.color = vec3(0.0);
    }
}
