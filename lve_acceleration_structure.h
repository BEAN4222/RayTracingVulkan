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
    };

    // structure storing mesh data
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

        // mesh creatin function
        void addTriangle(const glm::vec3& color);
        void addSphereMesh(const glm::vec3& center, const glm::vec3& color, float radius,
            int segments = 32, int rings = 16);

        // Acceleration Structure build
        void buildAccelerationStructures();

        VkAccelerationStructureKHR getTLAS() const { return topLevelAS; }

    private:
        //helper function for sphere mesh
        MeshData createSphereMeshData(const glm::vec3& center, const glm::vec3& color,
            float radius, int segments, int rings);

        // upload mesh to GPU buffer
        void uploadMeshToGPU(MeshData& mesh);

        // create each mesh of BLAS
        void createBottomLevelAS(MeshData& mesh);

        // create TLAS containing all meshes
        void createTopLevelAS();

        LveDevice& lveDevice;

        // storing meshes
        std::vector<MeshData> meshes;

        // Top-Level Acceleration Structure
        VkAccelerationStructureKHR topLevelAS = VK_NULL_HANDLE;
        VkBuffer topLevelASBuffer = VK_NULL_HANDLE;
        VkDeviceMemory topLevelASMemory = VK_NULL_HANDLE;

        // Ray Tracing function pointers
        PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR;
        PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
        PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR;
        PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR;
        PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;
        PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR;
    };

} // namespace lve