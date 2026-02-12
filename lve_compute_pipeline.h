#pragma once

#include "lve_device.h"
#include <string>
#include <vector>

namespace lve {

    class LveComputePipeline {
    public:
        LveComputePipeline(
            LveDevice& device,
            const std::string& computeShader,
            VkDescriptorSetLayout descriptorSetLayout,
            uint32_t pushConstantSize = 0
        );
        ~LveComputePipeline();

        LveComputePipeline(const LveComputePipeline&) = delete;
        LveComputePipeline& operator=(const LveComputePipeline&) = delete;

        VkPipeline getPipeline() const { return pipeline; }
        VkPipelineLayout getPipelineLayout() const { return pipelineLayout; }

    private:
        void createPipelineLayout(VkDescriptorSetLayout descriptorSetLayout, uint32_t pushConstantSize);
        void createComputePipeline(const std::string& computeShader);

        std::vector<char> readFile(const std::string& filepath);
        VkShaderModule createShaderModule(const std::vector<char>& code);

        LveDevice& lveDevice;
        VkPipeline pipeline;
        VkPipelineLayout pipelineLayout;
    };

} // namespace lve