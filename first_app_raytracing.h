#pragma once

#include "lve_window.h"
#include "lve_device.h"
#include "lve_swap_chain.h"
#include "lve_acceleration_structure.h"
#include "lve_ray_tracing_pipeline.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

#include <memory>
#include <vector>

namespace lve {

    // Push Constants로 GPU에 전달 (최대 128 bytes)
    struct CameraPushConstants {
        alignas(16) glm::vec3 position;    // 16 bytes
        alignas(16) glm::vec3 forward;     // 16 bytes
        alignas(16) glm::vec3 right;       // 16 bytes
        alignas(16) glm::vec3 up;          // 16 bytes
        float vfov;                        // 4 bytes
        float defocus_angle;               // 4 bytes
        float focus_dist;                  // 4 bytes
        float padding;                     // 4 bytes
    };  // 총 80 bytes

    class FirstAppRayTracing {
    public:
        static constexpr int WIDTH = 1200;
        static constexpr int HEIGHT = 675;

        FirstAppRayTracing();
        ~FirstAppRayTracing();

        FirstAppRayTracing(const FirstAppRayTracing&) = delete;
        FirstAppRayTracing& operator=(const FirstAppRayTracing&) = delete;

        void run();

    private:
        void createOneWeekendFinalScene();
        void createStorageImage();
        void createDescriptorPool();
        void createDescriptorSets();
        void createCommandBuffers();
        void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
        void drawFrame();

        // Camera system
        void initCamera();
        void processInput(float deltaTime);
        void updateCameraVectors();

        // Input callbacks
        static void mouseCallback(GLFWwindow* window, double xpos, double ypos);
        static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

        LveWindow lveWindow{ WIDTH, HEIGHT, "Ray Tracing - WASD Move, Mouse Look, ESC Release" };
        LveDevice lveDevice{ lveWindow };
        LveSwapChain lveSwapChain{ lveWindow, lveDevice };

        std::unique_ptr<LveAccelerationStructure> accelerationStructure;
        std::unique_ptr<LveRayTracingPipeline> rayTracingPipeline;

        // Storage Image
        VkImage storageImage;
        VkDeviceMemory storageImageMemory;
        VkImageView storageImageView;

        VkDescriptorPool descriptorPool;
        VkDescriptorSet descriptorSet;
        std::vector<VkCommandBuffer> commandBuffers;

        // Camera state
        glm::vec3 cameraPos;
        glm::vec3 cameraFront;
        glm::vec3 cameraUp;
        glm::vec3 cameraRight;
        float yaw;
        float pitch;
        float moveSpeed;
        float mouseSensitivity;
        float vfov;
        float defocusAngle;
        float focusDist;

        // Mouse state
        bool firstMouse;
        double lastX, lastY;
        bool mouseCaptured;

        // Timing
        float lastFrameTime;

        // Ray tracing function pointer
        PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR;

        // Static pointer for callbacks
        static FirstAppRayTracing* instance;
    };

} // namespace lve