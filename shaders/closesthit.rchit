#version 460
#extension GL_EXT_ray_tracing : enable

layout(location = 0) rayPayloadInEXT vec3 hitValue;
hitAttributeEXT vec2 attribs;

void main() {
    // 삼각형 중심색 (빨간색)
    const vec3 barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
    
    // 삼각형의 각 꼭지점에 다른 색상 부여
    vec3 color1 = vec3(1.0, 0.0, 0.0); // 빨강
    vec3 color2 = vec3(0.0, 1.0, 0.0); // 초록
    vec3 color3 = vec3(0.0, 0.0, 1.0); // 파랑
    
    // 무게중심 좌표로 색상 보간
    hitValue = color1 * barycentrics.x + color2 * barycentrics.y + color3 * barycentrics.z;
}
