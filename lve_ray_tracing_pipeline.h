#pragma once

#include "lve_device.h"
#include <string>
#include <vector>

namespace lve {

    class LveRayTracingPipeline {
    public:
        LveRayTracingPipeline(
            LveDevice& device,
            const std::string& raygenShader,
            const std::string& missShader,
            const std::string& closestHitShader
        );
        ~LveRayTracingPipeline();

        LveRayTracingPipeline(const LveRayTracingPipeline&) = delete;
        LveRayTracingPipeline& operator=(const LveRayTracingPipeline&) = delete;

        VkPipeline getPipeline() const { return pipeline; }
        VkPipelineLayout getPipelineLayout() const { return pipelineLayout; }
        VkDescriptorSetLayout getDescriptorSetLayout() const { return descriptorSetLayout; }  // 추가!

        VkStridedDeviceAddressRegionKHR getRaygenRegion() const { return raygenRegion; }
        VkStridedDeviceAddressRegionKHR getMissRegion() const { return missRegion; }
        VkStridedDeviceAddressRegionKHR getHitRegion() const { return hitRegion; }
        VkStridedDeviceAddressRegionKHR getCallableRegion() const { return callableRegion; }

    private:
        void createPipelineLayout();
        void createRayTracingPipeline(
            const std::string& raygenShader,
            const std::string& missShader,
            const std::string& closestHitShader
        );
        void createShaderBindingTable();

        std::vector<char> readFile(const std::string& filepath);
        VkShaderModule createShaderModule(const std::vector<char>& code);

        LveDevice& lveDevice;
        VkPipeline pipeline;
        VkPipelineLayout pipelineLayout;
        VkDescriptorSetLayout descriptorSetLayout;

        // Shader Binding Table
        VkBuffer sbtBuffer;
        VkDeviceMemory sbtMemory;

        VkStridedDeviceAddressRegionKHR raygenRegion{};
        VkStridedDeviceAddressRegionKHR missRegion{};
        VkStridedDeviceAddressRegionKHR hitRegion{};
        VkStridedDeviceAddressRegionKHR callableRegion{};

        // Ray Tracing Properties
        VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties{};

        // Function pointers
        PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR;
        PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR;
        PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR;
    };

} // namespace lve