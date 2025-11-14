#include "lve_acceleration_structure.h"
#include <stdexcept>
#include <cstring>

namespace lve {

LveAccelerationStructure::LveAccelerationStructure(LveDevice& device) : lveDevice{device} {
    // Load ray tracing function pointers
    vkGetBufferDeviceAddressKHR = reinterpret_cast<PFN_vkGetBufferDeviceAddressKHR>(
        vkGetDeviceProcAddr(lveDevice.device(), "vkGetBufferDeviceAddressKHR"));
    vkCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(
        vkGetDeviceProcAddr(lveDevice.device(), "vkCreateAccelerationStructureKHR"));
    vkDestroyAccelerationStructureKHR = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(
        vkGetDeviceProcAddr(lveDevice.device(), "vkDestroyAccelerationStructureKHR"));
    vkGetAccelerationStructureBuildSizesKHR = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(
        vkGetDeviceProcAddr(lveDevice.device(), "vkGetAccelerationStructureBuildSizesKHR"));
    vkCmdBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(
        vkGetDeviceProcAddr(lveDevice.device(), "vkCmdBuildAccelerationStructuresKHR"));
    vkGetAccelerationStructureDeviceAddressKHR = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(
        vkGetDeviceProcAddr(lveDevice.device(), "vkGetAccelerationStructureDeviceAddressKHR"));
}

LveAccelerationStructure::~LveAccelerationStructure() {
    if (topLevelAS != VK_NULL_HANDLE) {
        vkDestroyAccelerationStructureKHR(lveDevice.device(), topLevelAS, nullptr);
    }
    if (bottomLevelAS != VK_NULL_HANDLE) {
        vkDestroyAccelerationStructureKHR(lveDevice.device(), bottomLevelAS, nullptr);
    }
    
    vkDestroyBuffer(lveDevice.device(), topLevelASBuffer, nullptr);
    vkFreeMemory(lveDevice.device(), topLevelASMemory, nullptr);
    vkDestroyBuffer(lveDevice.device(), bottomLevelASBuffer, nullptr);
    vkFreeMemory(lveDevice.device(), bottomLevelASMemory, nullptr);
    
    vkDestroyBuffer(lveDevice.device(), vertexBuffer, nullptr);
    vkFreeMemory(lveDevice.device(), vertexBufferMemory, nullptr);
    vkDestroyBuffer(lveDevice.device(), indexBuffer, nullptr);
    vkFreeMemory(lveDevice.device(), indexBufferMemory, nullptr);
}

void LveAccelerationStructure::createTriangle() {
    // 삼각형 정점 데이터
    Vertex vertices[] = {
        {{0.0f, -0.5f, 0.0f}},
        {{0.5f, 0.5f, 0.0f}},
        {{-0.5f, 0.5f, 0.0f}}
    };
    
    uint32_t indices[] = {0, 1, 2};
    
    // Vertex Buffer 생성
    VkDeviceSize vertexBufferSize = sizeof(vertices);
    lveDevice.createBuffer(
        vertexBufferSize,
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | 
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        vertexBuffer,
        vertexBufferMemory
    );
    
    void* data;
    vkMapMemory(lveDevice.device(), vertexBufferMemory, 0, vertexBufferSize, 0, &data);
    memcpy(data, vertices, vertexBufferSize);
    vkUnmapMemory(lveDevice.device(), vertexBufferMemory);
    
    // Index Buffer 생성
    VkDeviceSize indexBufferSize = sizeof(indices);
    lveDevice.createBuffer(
        indexBufferSize,
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | 
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        indexBuffer,
        indexBufferMemory
    );
    
    vkMapMemory(lveDevice.device(), indexBufferMemory, 0, indexBufferSize, 0, &data);
    memcpy(data, indices, indexBufferSize);
    vkUnmapMemory(lveDevice.device(), indexBufferMemory);
    
    createBottomLevelAS();
    createTopLevelAS();
}

void LveAccelerationStructure::createBottomLevelAS() {
    // Buffer Device Address 가져오기
    VkBufferDeviceAddressInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    bufferInfo.buffer = vertexBuffer;
    VkDeviceAddress vertexAddress = vkGetBufferDeviceAddressKHR(lveDevice.device(), &bufferInfo);
    
    bufferInfo.buffer = indexBuffer;
    VkDeviceAddress indexAddress = vkGetBufferDeviceAddressKHR(lveDevice.device(), &bufferInfo);
    
    // Geometry 정보 설정
    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    geometry.geometry.triangles.vertexData.deviceAddress = vertexAddress;
    geometry.geometry.triangles.vertexStride = sizeof(Vertex);
    geometry.geometry.triangles.maxVertex = 2;
    geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
    geometry.geometry.triangles.indexData.deviceAddress = indexAddress;
    
    // Build Info
    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &geometry;
    
    uint32_t primitiveCount = 1;
    
    // 필요한 크기 계산
    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
    sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    vkGetAccelerationStructureBuildSizesKHR(
        lveDevice.device(),
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo,
        &primitiveCount,
        &sizeInfo
    );
    
    // AS Buffer 생성
    lveDevice.createBuffer(
        sizeInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        bottomLevelASBuffer,
        bottomLevelASMemory
    );
    
    // Acceleration Structure 생성
    VkAccelerationStructureCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.buffer = bottomLevelASBuffer;
    createInfo.size = sizeInfo.accelerationStructureSize;
    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    
    vkCreateAccelerationStructureKHR(lveDevice.device(), &createInfo, nullptr, &bottomLevelAS);
    
    // Scratch Buffer
    VkBuffer scratchBuffer;
    VkDeviceMemory scratchMemory;
    lveDevice.createBuffer(
        sizeInfo.buildScratchSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        scratchBuffer,
        scratchMemory
    );
    
    bufferInfo.buffer = scratchBuffer;
    VkDeviceAddress scratchAddress = vkGetBufferDeviceAddressKHR(lveDevice.device(), &bufferInfo);
    
    // Build
    buildInfo.dstAccelerationStructure = bottomLevelAS;
    buildInfo.scratchData.deviceAddress = scratchAddress;
    
    VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
    rangeInfo.primitiveCount = primitiveCount;
    rangeInfo.primitiveOffset = 0;
    rangeInfo.firstVertex = 0;
    rangeInfo.transformOffset = 0;
    
    const VkAccelerationStructureBuildRangeInfoKHR* pRangeInfo = &rangeInfo;
    
    VkCommandBuffer commandBuffer = lveDevice.beginSingleTimeCommands();
    vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &buildInfo, &pRangeInfo);
    lveDevice.endSingleTimeCommands(commandBuffer);
    
    vkDestroyBuffer(lveDevice.device(), scratchBuffer, nullptr);
    vkFreeMemory(lveDevice.device(), scratchMemory, nullptr);
}

void LveAccelerationStructure::createTopLevelAS() {
    // BLAS의 device address 가져오기
    VkAccelerationStructureDeviceAddressInfoKHR addressInfo{};
    addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    addressInfo.accelerationStructure = bottomLevelAS;
    VkDeviceAddress blasAddress = vkGetAccelerationStructureDeviceAddressKHR(lveDevice.device(), &addressInfo);
    
    // Instance 데이터
    VkAccelerationStructureInstanceKHR instance{};
    instance.transform.matrix[0][0] = 1.0f;
    instance.transform.matrix[1][1] = 1.0f;
    instance.transform.matrix[2][2] = 1.0f;
    instance.instanceCustomIndex = 0;
    instance.mask = 0xFF;
    instance.instanceShaderBindingTableRecordOffset = 0;
    instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
    instance.accelerationStructureReference = blasAddress;
    
    // Instance Buffer
    VkBuffer instanceBuffer;
    VkDeviceMemory instanceMemory;
    VkDeviceSize instanceBufferSize = sizeof(VkAccelerationStructureInstanceKHR);
    
    lveDevice.createBuffer(
        instanceBufferSize,
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | 
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        instanceBuffer,
        instanceMemory
    );
    
    void* data;
    vkMapMemory(lveDevice.device(), instanceMemory, 0, instanceBufferSize, 0, &data);
    memcpy(data, &instance, instanceBufferSize);
    vkUnmapMemory(lveDevice.device(), instanceMemory);
    
    VkBufferDeviceAddressInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    bufferInfo.buffer = instanceBuffer;
    VkDeviceAddress instanceAddress = vkGetBufferDeviceAddressKHR(lveDevice.device(), &bufferInfo);
    
    // Geometry 설정
    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    geometry.geometry.instances.arrayOfPointers = VK_FALSE;
    geometry.geometry.instances.data.deviceAddress = instanceAddress;
    
    // Build Info
    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &geometry;
    
    uint32_t primitiveCount = 1;
    
    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
    sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    vkGetAccelerationStructureBuildSizesKHR(
        lveDevice.device(),
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo,
        &primitiveCount,
        &sizeInfo
    );
    
    // TLAS Buffer 생성
    lveDevice.createBuffer(
        sizeInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        topLevelASBuffer,
        topLevelASMemory
    );
    
    VkAccelerationStructureCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.buffer = topLevelASBuffer;
    createInfo.size = sizeInfo.accelerationStructureSize;
    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    
    vkCreateAccelerationStructureKHR(lveDevice.device(), &createInfo, nullptr, &topLevelAS);
    
    // Scratch Buffer
    VkBuffer scratchBuffer;
    VkDeviceMemory scratchMemory;
    lveDevice.createBuffer(
        sizeInfo.buildScratchSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        scratchBuffer,
        scratchMemory
    );
    
    bufferInfo.buffer = scratchBuffer;
    VkDeviceAddress scratchAddress = vkGetBufferDeviceAddressKHR(lveDevice.device(), &bufferInfo);
    
    buildInfo.dstAccelerationStructure = topLevelAS;
    buildInfo.scratchData.deviceAddress = scratchAddress;
    
    VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
    rangeInfo.primitiveCount = primitiveCount;
    
    const VkAccelerationStructureBuildRangeInfoKHR* pRangeInfo = &rangeInfo;
    
    VkCommandBuffer commandBuffer = lveDevice.beginSingleTimeCommands();
    vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &buildInfo, &pRangeInfo);
    lveDevice.endSingleTimeCommands(commandBuffer);
    
    vkDestroyBuffer(lveDevice.device(), scratchBuffer, nullptr);
    vkFreeMemory(lveDevice.device(), scratchMemory, nullptr);
    vkDestroyBuffer(lveDevice.device(), instanceBuffer, nullptr);
    vkFreeMemory(lveDevice.device(), instanceMemory, nullptr);
}

} // namespace lve
