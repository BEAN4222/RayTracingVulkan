#include "lve_compute_pipeline.h"
#include <fstream>
#include <stdexcept>
#include <iostream>

namespace lve {

    LveComputePipeline::LveComputePipeline(
        LveDevice& device,
        const std::string& computeShader,
        VkDescriptorSetLayout descriptorSetLayout,
        uint32_t pushConstantSize
    ) : lveDevice{ device } {
        createPipelineLayout(descriptorSetLayout, pushConstantSize);
        createComputePipeline(computeShader);
    }

    LveComputePipeline::~LveComputePipeline() {
        vkDestroyPipeline(lveDevice.device(), pipeline, nullptr);
        vkDestroyPipelineLayout(lveDevice.device(), pipelineLayout, nullptr);
    }

    void LveComputePipeline::createPipelineLayout(
        VkDescriptorSetLayout descriptorSetLayout,
        uint32_t pushConstantSize
    ) {
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;

        VkPushConstantRange pushConstantRange{};
        if (pushConstantSize > 0) {
            pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
            pushConstantRange.offset = 0;
            pushConstantRange.size = pushConstantSize;
            pipelineLayoutInfo.pushConstantRangeCount = 1;
            pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
        }
        else {
            pipelineLayoutInfo.pushConstantRangeCount = 0;
            pipelineLayoutInfo.pPushConstantRanges = nullptr;
        }

        if (vkCreatePipelineLayout(lveDevice.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create compute pipeline layout!");
        }
    }

    void LveComputePipeline::createComputePipeline(const std::string& computeShader) {
        auto computeCode = readFile(computeShader);
        VkShaderModule computeModule = createShaderModule(computeCode);

        VkPipelineShaderStageCreateInfo computeStage{};
        computeStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        computeStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        computeStage.module = computeModule;
        computeStage.pName = "main";

        VkComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.stage = computeStage;
        pipelineInfo.layout = pipelineLayout;

        if (vkCreateComputePipelines(lveDevice.device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
            throw std::runtime_error("failed to create compute pipeline!");
        }

        vkDestroyShaderModule(lveDevice.device(), computeModule, nullptr);

        std::cout << "Compute pipeline created successfully" << std::endl;
    }

    std::vector<char> LveComputePipeline::readFile(const std::string& filepath) {
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

    VkShaderModule LveComputePipeline::createShaderModule(const std::vector<char>& code) {
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

} // namespace lve