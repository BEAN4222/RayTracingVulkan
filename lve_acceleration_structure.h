#pragma once

#include "lve_device.h"
#include <vector>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm.hpp>
#include <gtc/constants.hpp>

namespace lve {

    struct Vertex {
        glm::vec3 pos;
    };

    // Primitive info for shader (std430 layout compatible, 48 bytes)
    // (이름은 SphereInfo지만 구/쿼드 공용으로 사용)
    struct SphereInfo {
        glm::vec3 center;       // 구: 중심 / 쿼드: Q (셰이더에서 쿼드는 안 씀)
        float radius;           // 구: 반지름 / 쿼드: 0
        glm::vec3 color;
        float materialType;
        float materialParam;
        float primitiveType;    // 0 = sphere, 1 = quad
        float padding[2];       // Align to 16 bytes (48 bytes total)
    };

    // TLAS 인스턴스 1개 분량: 변환행렬 + 어느 BLAS를 쓸지
    struct PrimitiveInstance {
        VkTransformMatrixKHR transform;
        bool isQuad;
    };

    // Structure storing mesh data (단위 구 하나만 사용)
    struct MeshData {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        VkBuffer vertexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
        VkBuffer indexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;

        VkAccelerationStructureKHR bottomLevelAS = VK_NULL_HANDLE;
        VkBuffer bottomLevelASBuffer = VK_NULL_HANDLE;
        VkDeviceMemory bottomLevelASMemory = VK_NULL_HANDLE;
    };

    class LveAccelerationStructure {
    public:
        LveAccelerationStructure(LveDevice& device);
        ~LveAccelerationStructure();

        LveAccelerationStructure(const LveAccelerationStructure&) = delete;
        LveAccelerationStructure& operator=(const LveAccelerationStructure&) = delete;

        // Sphere 추가 (메시 생성 없이 정보만 저장)
        void addSphereMesh(const glm::vec3& center, const glm::vec3& color, float radius,
            float materialType = 0.0f, float materialParam = 0.0f,
            int segments = 32, int rings = 16);

        // Quad 추가 (Q = 시작 모서리, u/v = 두 변 벡터)
        void addQuad(const glm::vec3& Q, const glm::vec3& u, const glm::vec3& v,
            const glm::vec3& color,
            float materialType = 0.0f, float materialParam = 0.0f);

        // Acceleration Structure build
        void buildAccelerationStructures();

        VkAccelerationStructureKHR getTLAS() const { return topLevelAS; }

        // Sphere info buffer for shader access
        VkBuffer getSphereInfoBuffer() const { return sphereInfoBuffer; }
        uint32_t getSphereCount() const { return static_cast<uint32_t>(sphereInfos.size()); }

    private:
        // Helper function for sphere mesh (단위 구 생성용)
        MeshData createSphereMeshData(int segments, int rings);

        // 정육면체 생성용
        MeshData createQuadMeshData();

        // Upload mesh to GPU buffer
        void uploadMeshToGPU(MeshData& mesh);

        // Create BLAS for unit sphere (하나만!)
        void createBottomLevelAS(MeshData& mesh);

        // Create TLAS with instancing
        void createTopLevelAS();

        // Create sphere info buffer for shader
        void createSphereInfoBuffer();

        LveDevice& lveDevice;

        // 단위 구 BLAS (원점, 반지름 1) - 하나만!
        MeshData unitSphereMesh;
        bool unitSphereCreated = false;

        // 정육면체
        MeshData unitQuadMesh;
        bool unitQuadCreated = false;

        // 모든 프리미티브의 정보 (위치, 크기, 재질 등) - 구/쿼드 공용, customIndex 순서
        std::vector<SphereInfo> sphereInfos;

        // 각 프리미티브의 TLAS 인스턴스 정보 (sphereInfos와 동일 순서)
        std::vector<PrimitiveInstance> primitiveInstances;

        // Top-Level Acceleration Structure
        VkAccelerationStructureKHR topLevelAS = VK_NULL_HANDLE;
        VkBuffer topLevelASBuffer = VK_NULL_HANDLE;
        VkDeviceMemory topLevelASMemory = VK_NULL_HANDLE;

        // Sphere info buffer
        VkBuffer sphereInfoBuffer = VK_NULL_HANDLE;
        VkDeviceMemory sphereInfoMemory = VK_NULL_HANDLE;

        // Ray Tracing function pointers
        PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR;
        PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
        PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR;
        PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR;
        PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;
        PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR;
    };

} // namespace lve