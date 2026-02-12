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
        glm::vec3 normal;
        glm::vec3 color;
        float materialType;  // 0=Lambertian, 1=Metal, 2=Dielectric
        float materialParam; // Metal: fuzz, Dielectric: refraction_index
        float padding[2];    // Alignment to 16 bytes
    };

    // Sphere info for shader (std430 layout compatible)
    struct SphereInfo {
        glm::vec3 center;
        float radius;
        glm::vec3 color;
        float materialType;
        float materialParam;
        float padding[3];  // Align to 16 bytes (48 bytes total)
    };

    // Structure storing mesh data (ë‹¨ìœ„ êµ¬ í•˜ë‚˜ë§Œ ì‚¬ìš©)
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

        // Sphere ì¶”ê°€ (ë©”ì‹œ ìƒì„± ì—†ì´ ì •ë³´ë§Œ ì €ìž¥)
        void addSphereMesh(const glm::vec3& center, const glm::vec3& color, float radius,
            float materialType = 0.0f, float materialParam = 0.0f,
            int segments = 32, int rings = 16);

        // Acceleration Structure build
        void buildAccelerationStructures();

        VkAccelerationStructureKHR getTLAS() const { return topLevelAS; }

        // Sphere info buffer for shader access
        VkBuffer getSphereInfoBuffer() const { return sphereInfoBuffer; }
        uint32_t getSphereCount() const { return static_cast<uint32_t>(sphereInfos.size()); }

        // =========================================================================
        // Buffer accessors for ray tracing descriptor sets
        // =========================================================================
        VkBuffer getVertexBuffer() const { return unitSphereMesh.vertexBuffer; }
        VkBuffer getIndexBuffer() const { return unitSphereMesh.indexBuffer; }
        VkBuffer getMeshInfoBuffer() const { return sphereInfoBuffer; }

    private:
        // Helper function for sphere mesh (ë‹¨ìœ„ êµ¬ ìƒì„±ìš©)
        MeshData createSphereMeshData(int segments, int rings);

        // Upload mesh to GPU buffer
        void uploadMeshToGPU(MeshData& mesh);

        // Create BLAS for unit sphere (í•˜ë‚˜ë§Œ!)
        void createBottomLevelAS(MeshData& mesh);

        // Create TLAS with instancing
        void createTopLevelAS();

        // Create sphere info buffer for shader
        void createSphereInfoBuffer();

        LveDevice& lveDevice;

        // ë‹¨ìœ„ êµ¬ BLAS (ì›ì , ë°˜ì§€ë¦„ 1) - í•˜ë‚˜ë§Œ!
        MeshData unitSphereMesh;
        bool unitSphereCreated = false;

        // ëª¨ë“  êµ¬ì˜ ì •ë³´ (ìœ„ì¹˜, í¬ê¸°, ìž¬ì§ˆ ë“±)
        std::vector<SphereInfo> sphereInfos;

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