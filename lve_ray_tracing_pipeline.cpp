#include "lve_ray_tracing_pipeline.h"
#include <fstream>
#include <stdexcept>
#include <cstring>
#include <iostream>

namespace lve {

    LveRayTracingPipeline::LveRayTracingPipeline(
        LveDevice& device,
        const std::string& raygenShader,
        const std::string& missShader,
        const std::string& closestHitShader
    ) : lveDevice{ device } {

        vkGetRayTracingShaderGroupHandlesKHR = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(
            vkGetDeviceProcAddr(lveDevice.device(), "vkGetRayTracingShaderGroupHandlesKHR"));
        vkCreateRayTracingPipelinesKHR = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(
            vkGetDeviceProcAddr(lveDevice.device(), "vkCreateRayTracingPipelinesKHR"));
        vkGetBufferDeviceAddressKHR = reinterpret_cast<PFN_vkGetBufferDeviceAddressKHR>(
            vkGetDeviceProcAddr(lveDevice.device(), "vkGetBufferDeviceAddressKHR"));

        rtProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
        VkPhysicalDeviceProperties2 deviceProperties{};
        deviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        deviceProperties.pNext = &rtProperties;
        vkGetPhysicalDeviceProperties2(lveDevice.getPhysicalDevice(), &deviceProperties);

        createPipelineLayout();
        createRayTracingPipeline(raygenShader, missShader, closestHitShader);
        createShaderBindingTable();
    }

    LveRayTracingPipeline::~LveRayTracingPipeline() {
        vkDestroyPipeline(lveDevice.device(), pipeline, nullptr);
        vkDestroyPipelineLayout(lveDevice.device(), pipelineLayout, nullptr);
        vkDestroyDescriptorSetLayout(lveDevice.device(), descriptorSetLayout, nullptr);
        vkDestroyBuffer(lveDevice.device(), sbtBuffer, nullptr);
        vkFreeMemory(lveDevice.device(), sbtMemory, nullptr);
    }

    void LveRayTracingPipeline::createPipelineLayout() {
        std::vector<VkDescriptorSetLayoutBinding> bindings(9);

        // 0: Acceleration Structure
        bindings[0].binding = 0;
        bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        bindings[0].pImmutableSamplers = nullptr;

        // 1: RT Output Image
        bindings[1].binding = 1;
        bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        bindings[1].descriptorCount = 1;
        bindings[1].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        bindings[1].pImmutableSamplers = nullptr;

        // 2: Sphere Info Buffer
        bindings[2].binding = 2;
        bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[2].descriptorCount = 1;
        bindings[2].stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        bindings[2].pImmutableSamplers = nullptr;

        // 3: Visibility Buffer
        bindings[3].binding = 3;
        bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        bindings[3].descriptorCount = 1;
        bindings[3].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        bindings[3].pImmutableSamplers = nullptr;

        // 4: Motion Vector
        bindings[4].binding = 4;
        bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        bindings[4].descriptorCount = 1;
        bindings[4].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        bindings[4].pImmutableSamplers = nullptr;

        // 5: Camera UBO
        bindings[5].binding = 5;
        bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[5].descriptorCount = 1;
        bindings[5].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        bindings[5].pImmutableSamplers = nullptr;

        // 6: Seed Output
        bindings[6].binding = 6;
        bindings[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        bindings[6].descriptorCount = 1;
        bindings[6].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        bindings[6].pImmutableSamplers = nullptr;

        // 7: Forward Projected Seed
        bindings[7].binding = 7;
        bindings[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        bindings[7].descriptorCount = 1;
        bindings[7].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        bindings[7].pImmutableSamplers = nullptr;

        // 8: Reshaded Output
        bindings[8].binding = 8;
        bindings[8].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        bindings[8].descriptorCount = 1;
        bindings[8].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        bindings[8].pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(lveDevice.device(), &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor set layout!");
        }

        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        pushConstantRange.offset = 0;
        pushConstantRange.size = 80;

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

        if (vkCreatePipelineLayout(lveDevice.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create pipeline layout!");
        }

        std::cout << "Ray tracing pipeline layout created with 9 bindings" << std::endl;
    }

    void LveRayTracingPipeline::createRayTracingPipeline(
        const std::string& raygenShader,
        const std::string& missShader,
        const std::string& closestHitShader
    ) {
        auto raygenCode = readFile(raygenShader);
        auto missCode = readFile(missShader);
        auto chitCode = readFile(closestHitShader);

        VkShaderModule raygenModule = createShaderModule(raygenCode);
        VkShaderModule missModule = createShaderModule(missCode);
        VkShaderModule chitModule = createShaderModule(chitCode);

        VkPipelineShaderStageCreateInfo raygenStage{};
        raygenStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        raygenStage.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        raygenStage.module = raygenModule;
        raygenStage.pName = "main";

        VkPipelineShaderStageCreateInfo missStage{};
        missStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        missStage.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
        missStage.module = missModule;
        missStage.pName = "main";

        VkPipelineShaderStageCreateInfo chitStage{};
        chitStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        chitStage.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        chitStage.module = chitModule;
        chitStage.pName = "main";

        VkPipelineShaderStageCreateInfo stages[] = { raygenStage, missStage, chitStage };

        VkRayTracingShaderGroupCreateInfoKHR raygenGroup{};
        raygenGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
        raygenGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        raygenGroup.generalShader = 0;
        raygenGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
        raygenGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
        raygenGroup.intersectionShader = VK_SHADER_UNUSED_KHR;

        VkRayTracingShaderGroupCreateInfoKHR missGroup{};
        missGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
        missGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        missGroup.generalShader = 1;
        missGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
        missGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
        missGroup.intersectionShader = VK_SHADER_UNUSED_KHR;

        VkRayTracingShaderGroupCreateInfoKHR hitGroup{};
        hitGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
        hitGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
        hitGroup.generalShader = VK_SHADER_UNUSED_KHR;
        hitGroup.closestHitShader = 2;
        hitGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
        hitGroup.intersectionShader = VK_SHADER_UNUSED_KHR;

        VkRayTracingShaderGroupCreateInfoKHR groups[] = { raygenGroup, missGroup, hitGroup };

        VkRayTracingPipelineCreateInfoKHR pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
        pipelineInfo.stageCount = 3;
        pipelineInfo.pStages = stages;
        pipelineInfo.groupCount = 3;
        pipelineInfo.pGroups = groups;
        pipelineInfo.maxPipelineRayRecursionDepth = 10;
        pipelineInfo.layout = pipelineLayout;

        if (vkCreateRayTracingPipelinesKHR(
            lveDevice.device(),
            VK_NULL_HANDLE,
            VK_NULL_HANDLE,
            1,
            &pipelineInfo,
            nullptr,
            &pipeline) != VK_SUCCESS) {
            throw std::runtime_error("failed to create ray tracing pipeline!");
        }

        vkDestroyShaderModule(lveDevice.device(), raygenModule, nullptr);
        vkDestroyShaderModule(lveDevice.device(), missModule, nullptr);
        vkDestroyShaderModule(lveDevice.device(), chitModule, nullptr);

        std::cout << "Ray tracing pipeline created successfully" << std::endl;
    }

    void LveRayTracingPipeline::createShaderBindingTable() {
        const uint32_t handleSize = rtProperties.shaderGroupHandleSize;
        const uint32_t handleAlignment = rtProperties.shaderGroupHandleAlignment;
        const uint32_t baseAlignment = rtProperties.shaderGroupBaseAlignment;
        const uint32_t groupCount = 3;

        std::cout << "SBT Properties:" << std::endl;
        std::cout << "  handleSize: " << handleSize << std::endl;
        std::cout << "  handleAlignment: " << handleAlignment << std::endl;
        std::cout << "  baseAlignment: " << baseAlignment << std::endl;

        const uint32_t handleSizeAligned = (handleSize + baseAlignment - 1) & ~(baseAlignment - 1);
        std::cout << "  handleSizeAligned: " << handleSizeAligned << std::endl;

        const uint32_t dataSize = groupCount * handleSize;
        std::vector<uint8_t> shaderHandleStorage(dataSize);
        if (vkGetRayTracingShaderGroupHandlesKHR(
            lveDevice.device(),
            pipeline,
            0,
            groupCount,
            dataSize,
            shaderHandleStorage.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to get ray tracing shader group handles!");
        }

        const uint32_t sbtSize = handleSizeAligned * groupCount;

        lveDevice.createBuffer(
            sbtSize,
            VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            sbtBuffer,
            sbtMemory
        );

        void* mapped;
        vkMapMemory(lveDevice.device(), sbtMemory, 0, sbtSize, 0, &mapped);
        auto* pData = reinterpret_cast<uint8_t*>(mapped);

        for (uint32_t i = 0; i < groupCount; i++) {
            memcpy(pData + i * handleSizeAligned,
                shaderHandleStorage.data() + i * handleSize,
                handleSize);
        }

        vkUnmapMemory(lveDevice.device(), sbtMemory);

        VkBufferDeviceAddressInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        bufferInfo.buffer = sbtBuffer;
        VkDeviceAddress sbtAddress = vkGetBufferDeviceAddressKHR(lveDevice.device(), &bufferInfo);

        std::cout << "SBT Address: " << sbtAddress << " (alignment check: " << (sbtAddress % baseAlignment) << ")" << std::endl;

        if (sbtAddress % baseAlignment != 0) {
            std::cerr << "ERROR: SBT buffer address not aligned!" << std::endl;
            throw std::runtime_error("SBT buffer address not aligned to baseAlignment!");
        }

        raygenRegion.deviceAddress = sbtAddress;
        raygenRegion.stride = handleSizeAligned;
        raygenRegion.size = handleSizeAligned;

        missRegion.deviceAddress = sbtAddress + handleSizeAligned;
        missRegion.stride = handleSizeAligned;
        missRegion.size = handleSizeAligned;

        hitRegion.deviceAddress = sbtAddress + handleSizeAligned * 2;
        hitRegion.stride = handleSizeAligned;
        hitRegion.size = handleSizeAligned;

        callableRegion = {};

        std::cout << "Shader Binding Table created successfully" << std::endl;
    }

    std::vector<char> LveRayTracingPipeline::readFile(const std::string& filepath) {
        std::ifstream file{ filepath, std::ios::ate | std::ios::binary };

        if (!file.is_open()) {
            throw std::runtime_error("failed to open file: " + filepath);
        }

        size_t fileSize = static_cast<size_t>(file.tellg());
        std::vector<char> buffer(fileSize);

        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();

        return buffer;
    }

    VkShaderModule LveRayTracingPipeline::createShaderModule(const std::vector<char>& code) {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

        VkShaderModule shaderModule;
        if (vkCreateShaderModule(lveDevice.device(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            throw std::runtime_error("failed to create shader module!");
        }

        return shaderModule;
    }

}
