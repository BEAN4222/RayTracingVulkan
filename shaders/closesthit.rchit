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

// Primitive info structure (matches C++ SphereInfo, std430 layout)
struct SphereInfo {
    vec3 center;
    float radius;
    vec3 color;
    float materialType;
    float materialParam;
    float primitiveType;   // 0 = sphere, 1 = quad
    float padding1;
    float padding2;
};

layout(location = 0) rayPayloadInEXT RayPayload payload;
hitAttributeEXT vec2 attribs;

// Sphere info buffer (binding 2)
layout(binding = 2, set = 0, std430) readonly buffer SphereInfoBuffer {
    SphereInfo spheres[];
};

const float PI = 3.14159265358979323846;

// ===== Random =====
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
        if (1e-30 < lensq && lensq <= 1.0)
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

// ===== Orthonormal basis (book: onb class) =====
// 표면 법선 n을 z축(w)으로 하는 직교 정규기저를 만든다.
void build_onb(vec3 n, out vec3 u, out vec3 v, out vec3 w) {
    w = normalize(n);
    vec3 a = (abs(w.x) > 0.9) ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    v = normalize(cross(w, a));
    u = cross(w, v);
}

// cos(θ) 분포로 가중된 로컬 방향
vec3 random_cosine_direction(inout uint seed) {
    float r1 = random_double(seed);
    float r2 = random_double(seed);
    float phi = 2.0 * PI * r1;
    float x = cos(phi) * sqrt(r2);
    float y = sin(phi) * sqrt(r2);
    float z = sqrt(1.0 - r2);
    return vec3(x, y, z);
}

// ===== Cornell Box 천장 광원  =====
// addQuad(Q=(343,554,332), u=(-130,0,0), v=(0,0,-105)) 와 일치:
//   x ∈ [213,343], z ∈ [227,332], y = 554, 법선 = (0,-1,0) (아래로 발광)
const float LIGHT_X0 = 213.0;
const float LIGHT_X1 = 343.0;
const float LIGHT_Z0 = 227.0;
const float LIGHT_Z1 = 332.0;
const float LIGHT_Y  = 554.0;
const float LIGHT_AREA = (LIGHT_X1 - LIGHT_X0) * (LIGHT_Z1 - LIGHT_Z0); // 130 * 105

// 광원 위의 임의의 점을 향하는 방향 생성
vec3 light_generate(vec3 origin, inout uint seed) {
    vec3 on_light = vec3(
        random_double_range(seed, LIGHT_X0, LIGHT_X1),
        LIGHT_Y,
        random_double_range(seed, LIGHT_Z0, LIGHT_Z1)
    );
    return on_light - origin;
}

// 주어진 방향 dir 에 대한 광원 PDF 값
//   solid angle 단위로 변환:  p(ω) = dist^2 / (cos * area)
//   dir 이 광원 사각형에 실제로 닿지 않으면 0.
float light_pdf_value(vec3 origin, vec3 dir) {
    if (abs(dir.y) < 1e-8)
        return 0.0;

    float t = (LIGHT_Y - origin.y) / dir.y;
    if (t < 0.001)
        return 0.0;

    vec3 p = origin + t * dir;
    if (p.x < LIGHT_X0 || p.x > LIGHT_X1 || p.z < LIGHT_Z0 || p.z > LIGHT_Z1)
        return 0.0;

    float dist_squared = t * t * dot(dir, dir);
    float cosine = abs(dir.y) / length(dir);
    if (cosine < 1e-8)
        return 0.0;

    return dist_squared / (cosine * LIGHT_AREA);
}

// cosine 분포의 PDF 값 (book: cosine_pdf::value)
float cosine_pdf_value(vec3 normal, vec3 dir) {
    float cosine = dot(normalize(dir), normal);
    return max(0.0, cosine / PI);
}

const float MATERIAL_LAMBERTIAN    = 0.0;
const float MATERIAL_METAL         = 1.0;
const float MATERIAL_DIELECTRIC    = 2.0;
const float MATERIAL_DIFFUSE_LIGHT = 3.0;

const float EPSILON = 0.001;

void main() {
    payload.hit = true;

    vec3 world_pos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;

    int sphere_idx = gl_InstanceCustomIndexEXT;
    SphereInfo sphere = spheres[sphere_idx];

    vec3  sphere_center  = sphere.center;
    vec3  albedo         = sphere.color;
    float material_type  = sphere.materialType;
    float material_param = sphere.materialParam;

    // ---- 법선 / front_face 먼저 계산 (방향성 발광에도 필요) ----
    vec3 outward_normal;
    if (sphere.primitiveType < 0.5) {
        outward_normal = normalize(world_pos - sphere_center);          // sphere
    } else {
        outward_normal = normalize(gl_ObjectToWorldEXT * vec4(0.0, 0.0, 1.0, 0.0)); // quad
    }

    bool front_face = dot(gl_WorldRayDirectionEXT, outward_normal) < 0.0;
    vec3 normal = front_face ? outward_normal : -outward_normal;

    // ---- DIFFUSE_LIGHT: 방향성 발광, 산란 없음 (emitted, front_face only) ----
    if (abs(material_type - MATERIAL_DIFFUSE_LIGHT) < 0.1) {
        payload.scattered = false;
        payload.color = front_face ? albedo : vec3(0.0);  // 빛을 받는 면만 발광
        return;
    }

    // =========================================================
    //  LAMBERTIAN: 렌더링 방정식 + mixture PDF (cosine / light)
    //    throughput *= albedo * scattering_pdf / pdf_value
    // =========================================================
    if (abs(material_type - MATERIAL_LAMBERTIAN) < 0.1) {
        vec3 dir;

        // mixture_pdf::generate() — 50% cosine, 50% 광원 방향
        if (random_double(payload.seed) < 0.5) {
            vec3 ou, ov, ow;
            build_onb(normal, ou, ov, ow);
            vec3 c = random_cosine_direction(payload.seed);
            dir = c.x * ou + c.y * ov + c.z * ow;
        } else {
            dir = light_generate(world_pos, payload.seed);
        }
        dir = normalize(dir);

        // mixture_pdf::value() — 두 PDF의 가중 평균
        float pdf_value = 0.5 * cosine_pdf_value(normal, dir)
                        + 0.5 * light_pdf_value(world_pos, dir);

        if (pdf_value < 1e-8) {
            // 유효하지 않은 방향 (예: 지평선 아래) → 경로 종료
            payload.scattered = false;
            payload.color = vec3(0.0);
            return;
        }

        // lambertian scattering_pdf = cos(θ)/π
        float cos_theta = dot(normal, dir);
        float scattering_pdf = (cos_theta < 0.0) ? 0.0 : cos_theta / PI;

        payload.scattered = true;
        payload.color = albedo * scattering_pdf / pdf_value;  // 이번 바운스의 throughput 계수
        payload.direction = dir;

        float offset_sign = (dot(dir, outward_normal) > 0.0) ? 1.0 : -1.0;
        payload.origin = world_pos + outward_normal * (EPSILON * offset_sign);
        return;
    }

    // =========================================================
    //  METAL / DIELECTRIC: 정반사 — PDF 가중치 건너뜀 (book: skip_pdf)
    //    throughput *= attenuation
    // =========================================================
    vec3 scattered_direction;
    vec3 attenuation;
    bool did_scatter = false;

    if (abs(material_type - MATERIAL_METAL) < 0.1) {
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
        }
    }
    else if (abs(material_type - MATERIAL_DIELECTRIC) < 0.1) {
        attenuation = vec3(1.0, 1.0, 1.0);

        float ri = front_face ? (1.0 / material_param) : material_param;
        vec3 unit_direction = normalize(gl_WorldRayDirectionEXT);
        float cos_theta = min(dot(-unit_direction, normal), 1.0);
        float sin_theta = sqrt(1.0 - cos_theta * cos_theta);

        bool cannot_refract = ri * sin_theta > 1.0;

        vec3 direction;
        if (cannot_refract || reflectance(cos_theta, ri) > random_double(payload.seed)) {
            direction = reflect(unit_direction, normal);
        } else {
            direction = refract(unit_direction, normal, ri);
        }

        scattered_direction = normalize(direction);
        did_scatter = true;
    }

    if (did_scatter) {
        payload.scattered = true;
        payload.color = attenuation;

        float offset_sign = (dot(scattered_direction, outward_normal) > 0.0) ? 1.0 : -1.0;
        payload.origin = world_pos + outward_normal * (EPSILON * offset_sign);
        payload.direction = scattered_direction;
    } else {
        payload.scattered = false;
        payload.color = vec3(0.0);
    }
}
