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
        // Cleaning sphere info buffer
        if (sphereInfoBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(lveDevice.device(), sphereInfoBuffer, nullptr);
        }
        if (sphereInfoMemory != VK_NULL_HANDLE) {
            vkFreeMemory(lveDevice.device(), sphereInfoMemory, nullptr);
        }

        // Cleaning TLAS
        if (topLevelAS != VK_NULL_HANDLE) {
            vkDestroyAccelerationStructureKHR(lveDevice.device(), topLevelAS, nullptr);
        }
        if (topLevelASBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(lveDevice.device(), topLevelASBuffer, nullptr);
        }
        if (topLevelASMemory != VK_NULL_HANDLE) {
            vkFreeMemory(lveDevice.device(), topLevelASMemory, nullptr);
        }

        // Cleaning unit sphere BLAS (하나만!)
        if (unitSphereMesh.bottomLevelAS != VK_NULL_HANDLE) {
            vkDestroyAccelerationStructureKHR(lveDevice.device(), unitSphereMesh.bottomLevelAS, nullptr);
        }
        if (unitSphereMesh.bottomLevelASBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(lveDevice.device(), unitSphereMesh.bottomLevelASBuffer, nullptr);
        }
        if (unitSphereMesh.bottomLevelASMemory != VK_NULL_HANDLE) {
            vkFreeMemory(lveDevice.device(), unitSphereMesh.bottomLevelASMemory, nullptr);
        }
        if (unitSphereMesh.vertexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(lveDevice.device(), unitSphereMesh.vertexBuffer, nullptr);
        }
        if (unitSphereMesh.vertexBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(lveDevice.device(), unitSphereMesh.vertexBufferMemory, nullptr);
        }
        if (unitSphereMesh.indexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(lveDevice.device(), unitSphereMesh.indexBuffer, nullptr);
        }
        if (unitSphereMesh.indexBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(lveDevice.device(), unitSphereMesh.indexBufferMemory, nullptr);
        }
    }

    void LveAccelerationStructure::addSphereMesh(
        const glm::vec3& center,
        const glm::vec3& color,
        float radius,
        float materialType,
        float materialParam,
        int segments,  // 사용 안 함 (단위 구가 고정 해상도)
        int rings      // 사용 안 함
    ) {
        // 메시 생성 없이 정보만 저장!
        SphereInfo info{};
        info.center = center;
        info.radius = radius;
        info.color = color;
        info.materialType = materialType;
        info.materialParam = materialParam;
        info.padding[0] = 0.0f;
        info.padding[1] = 0.0f;
        info.padding[2] = 0.0f;

        sphereInfos.push_back(info);
    }

    // 단위 구 생성 (원점, 반지름 1)
    MeshData LveAccelerationStructure::createSphereMeshData(int segments, int rings) {
        MeshData mesh;

        // UV Sphere algorithm - 원점에 반지름 1인 구
        for (int ring = 0; ring <= rings; ++ring) {
            float phi = glm::pi<float>() * float(ring) / float(rings);  // 0 to PI
            float sinPhi = std::sin(phi);
            float cosPhi = std::cos(phi);

            for (int seg = 0; seg <= segments; ++seg) {
                float theta = 2.0f * glm::pi<float>() * float(seg) / float(segments);  // 0 to 2PI
                float sinTheta = std::sin(theta);
                float cosTheta = std::cos(theta);

                Vertex vertex;

                // Normal = Position (단위 구니까!)
                vertex.normal = glm::vec3(
                    sinPhi * cosTheta,
                    cosPhi,
                    sinPhi * sinTheta
                );

                // Position = Normal * radius (반지름 1)
                vertex.pos = vertex.normal;  // 원점 기준, 반지름 1

                // 색깔/재질은 버퍼에서 읽을 거라 의미 없음
                vertex.color = glm::vec3(1.0f, 1.0f, 1.0f);
                vertex.materialType = 0.0f;
                vertex.materialParam = 0.0f;
                vertex.padding[0] = 0.0f;
                vertex.padding[1] = 0.0f;

                mesh.vertices.push_back(vertex);
            }
        }

        // Index data
        for (int ring = 0; ring < rings; ++ring) {
            for (int seg = 0; seg < segments; ++seg) {
                int current = ring * (segments + 1) + seg;
                int next = current + segments + 1;

                // First upper triangle
                mesh.indices.push_back(current);
                mesh.indices.push_back(next);
                mesh.indices.push_back(current + 1);

                // Second lower triangle
                mesh.indices.push_back(current + 1);
                mesh.indices.push_back(next);
                mesh.indices.push_back(next + 1);
            }
        }

        return mesh;
    }

    void LveAccelerationStructure::buildAccelerationStructures() {
        if (sphereInfos.empty()) {
            throw std::runtime_error("No spheres added!");
        }

        std::cout << "Building optimized acceleration structures..." << std::endl;
        std::cout << "Sphere count: " << sphereInfos.size() << std::endl;

        // 1. 단위 구 BLAS 하나만 생성 (처음 한 번만)
        if (!unitSphereCreated) {
            std::cout << "Creating unit sphere BLAS (single instance)..." << std::endl;

            // 고품질 단위 구 (모든 구가 공유)
            unitSphereMesh = createSphereMeshData(32, 16);

            std::cout << "Unit sphere vertices: " << unitSphereMesh.vertices.size() << std::endl;
            std::cout << "Unit sphere indices: " << unitSphereMesh.indices.size() << std::endl;

            uploadMeshToGPU(unitSphereMesh);
            createBottomLevelAS(unitSphereMesh);
            unitSphereCreated = true;

            std::cout << "Unit sphere BLAS created!" << std::endl;
        }

        // 2. SphereInfo 버퍼 생성
        createSphereInfoBuffer();

        // 3. TLAS 생성 (Transform으로 인스턴싱)
        createTopLevelAS();

        std::cout << "Acceleration structures built successfully!" << std::endl;
        std::cout << "BLAS count: 1 (optimized from " << sphereInfos.size() << ")" << std::endl;
        std::cout << "TLAS instances: " << sphereInfos.size() << std::endl;
    }

    void LveAccelerationStructure::uploadMeshToGPU(MeshData& mesh) {
        // Vertex Buffer
        VkDeviceSize vertexBufferSize = sizeof(Vertex) * mesh.vertices.size();

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;

        lveDevice.createBuffer(
            vertexBufferSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            stagingBuffer,
            stagingMemory
        );

        void* data;
        vkMapMemory(lveDevice.device(), stagingMemory, 0, vertexBufferSize, 0, &data);
        memcpy(data, mesh.vertices.data(), vertexBufferSize);
        vkUnmapMemory(lveDevice.device(), stagingMemory);

        lveDevice.createBuffer(
            vertexBufferSize,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT |
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            mesh.vertexBuffer,
            mesh.vertexBufferMemory
        );

        lveDevice.copyBuffer(stagingBuffer, mesh.vertexBuffer, vertexBufferSize);

        vkDestroyBuffer(lveDevice.device(), stagingBuffer, nullptr);
        vkFreeMemory(lveDevice.device(), stagingMemory, nullptr);

        // Index Buffer
        VkDeviceSize indexBufferSize = sizeof(uint32_t) * mesh.indices.size();

        lveDevice.createBuffer(
            indexBufferSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            stagingBuffer,
            stagingMemory
        );

        vkMapMemory(lveDevice.device(), stagingMemory, 0, indexBufferSize, 0, &data);
        memcpy(data, mesh.indices.data(), indexBufferSize);
        vkUnmapMemory(lveDevice.device(), stagingMemory);

        lveDevice.createBuffer(
            indexBufferSize,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT |
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            mesh.indexBuffer,
            mesh.indexBufferMemory
        );

        lveDevice.copyBuffer(stagingBuffer, mesh.indexBuffer, indexBufferSize);

        vkDestroyBuffer(lveDevice.device(), stagingBuffer, nullptr);
        vkFreeMemory(lveDevice.device(), stagingMemory, nullptr);
    }

    void LveAccelerationStructure::createSphereInfoBuffer() {
        VkDeviceSize bufferSize = sizeof(SphereInfo) * sphereInfos.size();

        lveDevice.createBuffer(
            bufferSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            sphereInfoBuffer,
            sphereInfoMemory
        );

        void* data;
        vkMapMemory(lveDevice.device(), sphereInfoMemory, 0, bufferSize, 0, &data);
        memcpy(data, sphereInfos.data(), bufferSize);
        vkUnmapMemory(lveDevice.device(), sphereInfoMemory);

        std::cout << "Sphere info buffer created: " << sphereInfos.size() << " spheres" << std::endl;
    }

    void LveAccelerationStructure::createBottomLevelAS(MeshData& mesh) {
        // Get buffer addresses
        VkBufferDeviceAddressInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        bufferInfo.buffer = mesh.vertexBuffer;
        VkDeviceAddress vertexAddress = vkGetBufferDeviceAddressKHR(lveDevice.device(), &bufferInfo);

        bufferInfo.buffer = mesh.indexBuffer;
        VkDeviceAddress indexAddress = vkGetBufferDeviceAddressKHR(lveDevice.device(), &bufferInfo);

        // Geometry description
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

        // Build info
        VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
        buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        buildInfo.geometryCount = 1;
        buildInfo.pGeometries = &geometry;

        uint32_t primitiveCount = static_cast<uint32_t>(mesh.indices.size() / 3);

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
        if (sphereInfos.empty()) {
            throw std::runtime_error("No spheres to build TLAS!");
        }

        // 단위 구 BLAS의 주소 (하나뿐!)
        VkAccelerationStructureDeviceAddressInfoKHR addressInfo{};
        addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        addressInfo.accelerationStructure = unitSphereMesh.bottomLevelAS;
        VkDeviceAddress blasAddress = vkGetAccelerationStructureDeviceAddressKHR(
            lveDevice.device(), &addressInfo);

        std::vector<VkAccelerationStructureInstanceKHR> instances;
        instances.reserve(sphereInfos.size());

        // 각 구마다 Transform으로 위치/크기 적용!
        for (size_t i = 0; i < sphereInfos.size(); ++i) {
            const SphereInfo& sphere = sphereInfos[i];

            VkAccelerationStructureInstanceKHR instance{};

            // Transform Matrix 설정 (3x4 row-major)
            // VkTransformMatrixKHR는 [3][4] 배열
            // | m[0][0]  m[0][1]  m[0][2]  m[0][3] |   | sx  0   0   tx |
            // | m[1][0]  m[1][1]  m[1][2]  m[1][3] | = | 0   sy  0   ty |
            // | m[2][0]  m[2][1]  m[2][2]  m[2][3] |   | 0   0   sz  tz |

            float r = sphere.radius;
            glm::vec3 c = sphere.center;

            // 초기화 (0으로)
            memset(&instance.transform, 0, sizeof(instance.transform));

            // Scale (대각선)
            instance.transform.matrix[0][0] = r;    // scale X
            instance.transform.matrix[1][1] = r;    // scale Y
            instance.transform.matrix[2][2] = r;    // scale Z

            // Translation (마지막 열)
            instance.transform.matrix[0][3] = c.x;  // translate X
            instance.transform.matrix[1][3] = c.y;  // translate Y
            instance.transform.matrix[2][3] = c.z;  // translate Z

            instance.instanceCustomIndex = static_cast<uint32_t>(i);
            instance.mask = 0xFF;
            instance.instanceShaderBindingTableRecordOffset = 0;
            instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
            instance.accelerationStructureReference = blasAddress;  // 모두 같은 BLAS!

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

        // Setting Geometry
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

        std::cout << "TLAS created with " << instances.size() << " instances (all sharing 1 BLAS)" << std::endl;
    }

} // namespace lve