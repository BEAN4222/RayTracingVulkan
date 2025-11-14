#pragma once

#include "lve_device.h"
#include <vector>

namespace lve {

struct Vertex {
    float pos[3];
};

class LveAccelerationStructure {
public:
    LveAccelerationStructure(LveDevice& device);
    ~LveAccelerationStructure();

    LveAccelerationStructure(const LveAccelerationStructure&) = delete;
    LveAccelerationStructure& operator=(const LveAccelerationStructure&) = delete;

    void createTriangle();
    VkAccelerationStructureKHR getTLAS() const { return topLevelAS; }

private:
    void createBottomLevelAS();
    void createTopLevelAS();
    
    LveDevice& lveDevice;
    
    // Buffers
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;
    
    // Acceleration Structures
    VkAccelerationStructureKHR bottomLevelAS;
    VkBuffer bottomLevelASBuffer;
    VkDeviceMemory bottomLevelASMemory;
    
    VkAccelerationStructureKHR topLevelAS;
    VkBuffer topLevelASBuffer;
    VkDeviceMemory topLevelASMemory;
    
    // Ray Tracing function pointers
    PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR;
    PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
    PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR;
    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR;
    PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR;
};

} // namespace lve
