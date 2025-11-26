#pragma once

#include "lve_window.h"
#include "lve_device.h"
#include "lve_swap_chain.h"
#include "lve_acceleration_structure.h"
#include "lve_ray_tracing_pipeline.h"

#include <memory>
#include <vector>

namespace lve {

class FirstAppRayTracing {
public:
    static constexpr int WIDTH = 800;
    static constexpr int HEIGHT = 600;

    FirstAppRayTracing();
    ~FirstAppRayTracing();

    FirstAppRayTracing(const FirstAppRayTracing&) = delete;
    FirstAppRayTracing& operator=(const FirstAppRayTracing&) = delete;

    void run();

private:
    void createStorageImage();
    void createDescriptorPool();
    void createDescriptorSets();
    void createCommandBuffers();
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void drawFrame();

    LveWindow lveWindow{WIDTH, HEIGHT, "Ray Tracing Vulkan!"};
    LveDevice lveDevice{lveWindow};
    LveSwapChain lveSwapChain{lveWindow, lveDevice};
    
    std::unique_ptr<LveAccelerationStructure> accelerationStructure;
    std::unique_ptr<LveRayTracingPipeline> rayTracingPipeline;
    
    // Storage Image
    VkImage storageImage;
    VkDeviceMemory storageImageMemory;
    VkImageView storageImageView;
    
    VkDescriptorPool descriptorPool;
    VkDescriptorSet descriptorSet;
    std::vector<VkCommandBuffer> commandBuffers;
    
    // Ray tracing function pointer
    PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR;
};

} // namespace lve
