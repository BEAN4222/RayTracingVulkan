#include "lve_acceleration_structure.h"
#include <stdexcept>
#include <iostream>
#include <cstring>
#include <cmath>

namespace lve {

    LveAccelerationStructure::LveAccelerationStructure(LveDevice& device) : lveDevice{ device } {
        // Ray tracing function pointer load
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
        // cleaning TLAS
        if (topLevelAS != VK_NULL_HANDLE) {
            vkDestroyAccelerationStructureKHR(lveDevice.device(), topLevelAS, nullptr);
        }
        if (topLevelASBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(lveDevice.device(), topLevelASBuffer, nullptr);
        }
        if (topLevelASMemory != VK_NULL_HANDLE) {
            vkFreeMemory(lveDevice.device(), topLevelASMemory, nullptr);
        }

        // cleaning all the meshes
        for (auto& mesh : meshes) {
            if (mesh.bottomLevelAS != VK_NULL_HANDLE) {
                vkDestroyAccelerationStructureKHR(lveDevice.device(), mesh.bottomLevelAS, nullptr);
            }
            if (mesh.bottomLevelASBuffer != VK_NULL_HANDLE) {
                vkDestroyBuffer(lveDevice.device(), mesh.bottomLevelASBuffer, nullptr);
            }
            if (mesh.bottomLevelASMemory != VK_NULL_HANDLE) {
                vkFreeMemory(lveDevice.device(), mesh.bottomLevelASMemory, nullptr);
            }
            if (mesh.vertexBuffer != VK_NULL_HANDLE) {
                vkDestroyBuffer(lveDevice.device(), mesh.vertexBuffer, nullptr);
            }
            if (mesh.vertexBufferMemory != VK_NULL_HANDLE) {
                vkFreeMemory(lveDevice.device(), mesh.vertexBufferMemory, nullptr);
            }
            if (mesh.indexBuffer != VK_NULL_HANDLE) {
                vkDestroyBuffer(lveDevice.device(), mesh.indexBuffer, nullptr);
            }
            if (mesh.indexBufferMemory != VK_NULL_HANDLE) {
                vkFreeMemory(lveDevice.device(), mesh.indexBufferMemory, nullptr);
            }
        }
    }

    void LveAccelerationStructure::addTriangle(const glm::vec3& color) {
        MeshData mesh;

        // triangle vertex data
        mesh.vertices = {
            {{0.0f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, color},
            {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, color},
            {{-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, color}
        };

        mesh.indices = { 0, 1, 2 };

        meshes.push_back(mesh);
    }

    void LveAccelerationStructure::addSphereMesh(
        const glm::vec3& center,
        const glm::vec3& color,
        float radius,
        int segments,
        int rings
    ) {
        MeshData mesh = createSphereMeshData(center, color, radius, segments, rings);
        meshes.push_back(mesh);
    }

    MeshData LveAccelerationStructure::createSphereMeshData(
        const glm::vec3& center,
        const glm::vec3& color,
        float radius,
        int segments,
        int rings
    ) {
        MeshData mesh;

        // UV Sphere algorithm
        for (int ring = 0; ring <= rings; ++ring) {
            float phi = glm::pi<float>() * float(ring) / float(rings);  // 0 to PI
            float sinPhi = std::sin(phi);
            float cosPhi = std::cos(phi);

            for (int seg = 0; seg <= segments; ++seg) {
                float theta = 2.0f * glm::pi<float>() * float(seg) / float(segments);  // 0 to 2PI
                float sinTheta = std::sin(theta);
                float cosTheta = std::cos(theta);

                Vertex vertex;

                // normalized normal vector direction
                vertex.normal = glm::vec3(
                    sinPhi * cosTheta,
                    cosPhi,
                    sinPhi * sinTheta
                );

                vertex.pos = center + radius * vertex.normal;

                // color of the triangle
                vertex.color = color;

                mesh.vertices.push_back(vertex);
            }
        }

        // Index data of the triangle
        for (int ring = 0; ring < rings; ++ring) {
            for (int seg = 0; seg < segments; ++seg) {
                int current = ring * (segments + 1) + seg;
                int next = current + segments + 1;

                // first upper triangle
                mesh.indices.push_back(current);
                mesh.indices.push_back(next);
                mesh.indices.push_back(current + 1);

                // second lower triangle
                mesh.indices.push_back(current + 1);
                mesh.indices.push_back(next);
                mesh.indices.push_back(next + 1);
            }
        }

        return mesh;
    }

    void LveAccelerationStructure::buildAccelerationStructures() {
        std::cout << "Building acceleration structures for " << meshes.size() << " meshes..." << std::endl;

        // Upload each mesh to the GPU and create BLAS
        for (auto& mesh : meshes) {
            uploadMeshToGPU(mesh);
            createBottomLevelAS(mesh);
        }

        // Creating TLAS containing all the meshes
        createTopLevelAS();

        std::cout << "Acceleration structures built successfully!" << std::endl;
    }

    void LveAccelerationStructure::uploadMeshToGPU(MeshData& mesh) {
        // Vertex Buffer creation
        VkDeviceSize vertexBufferSize = sizeof(Vertex) * mesh.vertices.size();
        lveDevice.createBuffer(
            vertexBufferSize,
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            mesh.vertexBuffer,
            mesh.vertexBufferMemory
        );

        void* data;
        vkMapMemory(lveDevice.device(), mesh.vertexBufferMemory, 0, vertexBufferSize, 0, &data);
        memcpy(data, mesh.vertices.data(), vertexBufferSize);
        vkUnmapMemory(lveDevice.device(), mesh.vertexBufferMemory);

        // Index Buffer creation
        VkDeviceSize indexBufferSize = sizeof(uint32_t) * mesh.indices.size();
        lveDevice.createBuffer(
            indexBufferSize,
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            mesh.indexBuffer,
            mesh.indexBufferMemory
        );

        vkMapMemory(lveDevice.device(), mesh.indexBufferMemory, 0, indexBufferSize, 0, &data);
        memcpy(data, mesh.indices.data(), indexBufferSize);
        vkUnmapMemory(lveDevice.device(), mesh.indexBufferMemory);
    }

    void LveAccelerationStructure::createBottomLevelAS(MeshData& mesh) {
        // loading Buffer Device Address
        VkBufferDeviceAddressInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        bufferInfo.buffer = mesh.vertexBuffer;
        VkDeviceAddress vertexAddress = vkGetBufferDeviceAddressKHR(lveDevice.device(), &bufferInfo);

        bufferInfo.buffer = mesh.indexBuffer;
        VkDeviceAddress indexAddress = vkGetBufferDeviceAddressKHR(lveDevice.device(), &bufferInfo);

        // setting Geometry Data
        VkAccelerationStructureGeometryKHR geometry{};
        geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
        geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
        geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
        geometry.geometry.triangles.vertexData.deviceAddress = vertexAddress;
        geometry.geometry.triangles.vertexStride = sizeof(Vertex);
        geometry.geometry.triangles.maxVertex = static_cast<uint32_t>(mesh.vertices.size() - 1);
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

        uint32_t primitiveCount = static_cast<uint32_t>(mesh.indices.size() / 3);

        // Calculating the size 
        VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
        sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        vkGetAccelerationStructureBuildSizesKHR(
            lveDevice.device(),
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &buildInfo,
            &primitiveCount,
            &sizeInfo
        );

        // AS Buffer creation
        lveDevice.createBuffer(
            sizeInfo.accelerationStructureSize,
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            mesh.bottomLevelASBuffer,
            mesh.bottomLevelASMemory
        );

        // Acceleration Structure creation
        VkAccelerationStructureCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        createInfo.buffer = mesh.bottomLevelASBuffer;
        createInfo.size = sizeInfo.accelerationStructureSize;
        createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

        vkCreateAccelerationStructureKHR(lveDevice.device(), &createInfo, nullptr, &mesh.bottomLevelAS);

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
        buildInfo.dstAccelerationStructure = mesh.bottomLevelAS;
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
        if (meshes.empty()) {
            throw std::runtime_error("No meshes to build TLAS!");
        }

        std::vector<VkAccelerationStructureInstanceKHR> instances;

        // Making instance for each meshes
        for (size_t i = 0; i < meshes.size(); ++i) {
            VkAccelerationStructureDeviceAddressInfoKHR addressInfo{};
            addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
            addressInfo.accelerationStructure = meshes[i].bottomLevelAS;
            VkDeviceAddress blasAddress = vkGetAccelerationStructureDeviceAddressKHR(lveDevice.device(), &addressInfo);

            VkAccelerationStructureInstanceKHR instance{};
            // unit matrix
            instance.transform.matrix[0][0] = 1.0f;
            instance.transform.matrix[1][1] = 1.0f;
            instance.transform.matrix[2][2] = 1.0f;
            instance.instanceCustomIndex = static_cast<uint32_t>(i);
            instance.mask = 0xFF;
            instance.instanceShaderBindingTableRecordOffset = 0;
            instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
            instance.accelerationStructureReference = blasAddress;

            instances.push_back(instance);
        }

        // Instance Buffer creation
        VkBuffer instanceBuffer;
        VkDeviceMemory instanceMemory;
        VkDeviceSize instanceBufferSize = sizeof(VkAccelerationStructureInstanceKHR) * instances.size();

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
        memcpy(data, instances.data(), instanceBufferSize);
        vkUnmapMemory(lveDevice.device(), instanceMemory);

        VkBufferDeviceAddressInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        bufferInfo.buffer = instanceBuffer;
        VkDeviceAddress instanceAddress = vkGetBufferDeviceAddressKHR(lveDevice.device(), &bufferInfo);

        // setting Geometry
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

        uint32_t primitiveCount = static_cast<uint32_t>(instances.size());

        VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
        sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        vkGetAccelerationStructureBuildSizesKHR(
            lveDevice.device(),
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &buildInfo,
            &primitiveCount,
            &sizeInfo
        );

        // TLAS Buffer creation
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