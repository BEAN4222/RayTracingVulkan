#pragma once

#include "lve_window.h"
#include "lve_device.h"
#include "lve_swap_chain.h"
#include "lve_acceleration_structure.h"
#include "lve_ray_tracing_pipeline.h"
#include "lve_compute_pipeline.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

#include <memory>
#include <vector>

namespace lve {

    struct CameraPushConstants {
        alignas(16) glm::vec3 position;
        alignas(16) glm::vec3 forward;
        alignas(16) glm::vec3 right;
        alignas(16) glm::vec3 up;
        float vfov;
        float defocus_angle;
        float focus_dist;
        uint32_t frameNumber;
    };

    struct ForwardProjectionPushConstants {
        alignas(16) glm::mat4 viewProjMatrix;
        alignas(16) glm::mat4 invViewProjMatrix;
        glm::vec4 resolution;
        glm::vec4 cameraPos;
        uint32_t frameNumber;
        float depthThreshold;
        float normalThreshold;
        float padding;
    };

    struct TemporalAccumulationPushConstants {
        glm::vec4 resolution;
        float alpha;
        float momentsAlpha;
        float depthThreshold;
        float normalThreshold;
        uint32_t frameNumber;
        uint32_t useAdaptiveAlpha;
        float antilagScale;
        float padding;
    };

    struct GradientSamplingPushConstants {
        glm::vec4 resolution;
        uint32_t frameNumber;
        float gradientScale;
        float depthThreshold;
        float normalThreshold;
    };

    struct GradientAtrousPushConstants {
        glm::vec4 fullResolution;   // full width, height, 1/width, 1/height
        int32_t stepSize;
        float sigmaDepth;
        float sigmaNormal;
        uint32_t frameNumber;
    };

    struct SpatialFilterPushConstants {
        glm::vec4 resolution;
        int32_t stepSize;
        float sigmaLuminance;
        float sigmaDepth;
        float sigmaNormal;
        float padding1;
        float padding2;
        float padding3;
        float padding4;
    };

    struct CameraUBO {
        alignas(16) glm::mat4 viewMatrix;
        alignas(16) glm::mat4 projMatrix;
        alignas(16) glm::mat4 viewProjMatrix;
        alignas(16) glm::mat4 invViewProjMatrix;
        alignas(16) glm::mat4 prevViewProjMatrix;

        alignas(16) glm::vec4 cameraPos;
        alignas(16) glm::vec4 cameraFront;
        alignas(16) glm::vec4 cameraUp;
        alignas(16) glm::vec4 cameraRight;
        alignas(16) glm::vec4 frustumInfo;

        alignas(16) glm::vec4 prevCameraPos;
        alignas(16) glm::vec4 prevCameraFront;
        alignas(16) glm::vec4 prevCameraUp;
        alignas(16) glm::vec4 prevCameraRight;

        alignas(16) glm::vec4 resolution;
    };

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
        void createStorageImages();
        void createVisibilityBufferImages();
        void createHistoryBuffers();
        void createUniformBuffers();

        void createRTDescriptorPool();
        void createRTDescriptorSets();

        void createFPDescriptorSetLayout();
        void createFPDescriptorPool();
        void createFPDescriptorSets();
        void createForwardProjectionImages();

        void createTADescriptorSetLayout();
        void createTADescriptorPool();
        void createTADescriptorSets();

        void createSFDescriptorSetLayout();
        void createSFDescriptorPool();
        void createSFDescriptorSets();

        void createGradientDescriptorSetLayout();
        void createGradientDescriptorPool();
        void createGradientDescriptorSets();
        void createGradientImages();

        void createCommandBuffers();
        void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
        void drawFrame();

        void initCamera();
        void processInput(float deltaTime);
        void updateCameraVectors();
        void updateUniformBuffer();
        void savePreviousFrameData();
        void copyCurrentToPreviousVisibilityBuffer(VkCommandBuffer commandBuffer);
        void copyCurrentToPreviousBuffers(VkCommandBuffer commandBuffer);
        void clearForwardProjectionBuffers(VkCommandBuffer commandBuffer);
        glm::mat4 getViewMatrix() const;
        glm::mat4 getProjectionMatrix() const;

        static void mouseCallback(GLFWwindow* window, double xpos, double ypos);
        static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

        void cleanupVisibilityBufferImages();
        void cleanupHistoryBuffers();
        void cleanupForwardProjectionImages();

        LveWindow lveWindow{ WIDTH, HEIGHT, "Ray Tracing + A-SVGF Visibility Buffer" };
        LveDevice lveDevice{ lveWindow };
        LveSwapChain lveSwapChain{ lveWindow, lveDevice };

        std::unique_ptr<LveAccelerationStructure> accelerationStructure;
        std::unique_ptr<LveRayTracingPipeline> rayTracingPipeline;
        std::unique_ptr<LveComputePipeline> forwardProjectionPipeline;
        std::unique_ptr<LveComputePipeline> temporalAccumulationPipeline;
        std::unique_ptr<LveComputePipeline> spatialFilterPipeline;
        std::unique_ptr<LveComputePipeline> gradientSamplingPipeline;
        std::unique_ptr<LveComputePipeline> gradientAtrousPipeline;

        VkImage rtOutputImage;
        VkDeviceMemory rtOutputImageMemory;
        VkImageView rtOutputImageView;

        VkImage reshadedImage;
        VkDeviceMemory reshadedImageMemory;
        VkImageView reshadedImageView;

        VkImage seedImage;
        VkDeviceMemory seedImageMemory;
        VkImageView seedImageView;

        VkImage denoisedImage;
        VkDeviceMemory denoisedImageMemory;
        VkImageView denoisedImageView;

        VkImage visibilityBuffer;
        VkDeviceMemory visibilityBufferMemory;
        VkImageView visibilityBufferView;

        VkImage prevVisibilityBuffer;
        VkDeviceMemory prevVisibilityBufferMemory;
        VkImageView prevVisibilityBufferView;

        VkImage gBufferMotion;
        VkDeviceMemory gBufferMotionMemory;
        VkImageView gBufferMotionView;

        VkImage historyColor[2];
        VkDeviceMemory historyColorMemory[2];
        VkImageView historyColorView[2];

        VkImage historyMoments[2];
        VkDeviceMemory historyMomentsMemory[2];
        VkImageView historyMomentsView[2];

        VkImage historyLength[2];
        VkDeviceMemory historyLengthMemory[2];
        VkImageView historyLengthView[2];

        VkImage prevColor;
        VkDeviceMemory prevColorMemory;
        VkImageView prevColorView;

        VkImage prevSeed;
        VkDeviceMemory prevSeedMemory;
        VkImageView prevSeedView;

        VkImage forwardProjectedColor;
        VkDeviceMemory forwardProjectedColorMemory;
        VkImageView forwardProjectedColorView;

        VkImage forwardProjectedSeed;
        VkDeviceMemory forwardProjectedSeedMemory;
        VkImageView forwardProjectedSeedView;

        VkImage forwardProjectedDepth;
        VkDeviceMemory forwardProjectedDepthMemory;
        VkImageView forwardProjectedDepthView;

        VkBuffer cameraUBOBuffer;
        VkDeviceMemory cameraUBOMemory;
        void* cameraUBOMapped;

        VkDescriptorPool rtDescriptorPool;
        VkDescriptorSet rtDescriptorSet;

        VkDescriptorSetLayout fpDescriptorSetLayout;
        VkDescriptorPool fpDescriptorPool;
        VkDescriptorSet fpDescriptorSet;

        VkDescriptorSetLayout taDescriptorSetLayout;
        VkDescriptorPool taDescriptorPool;
        VkDescriptorSet taDescriptorSets[2];

        VkDescriptorSetLayout sfDescriptorSetLayout;
        VkDescriptorPool sfDescriptorPool;
        VkDescriptorSet sfDescriptorSets[2];

        VkImage filterPingImage;
        VkDeviceMemory filterPingMemory;
        VkImageView filterPingView;

        VkImage filterPongImage;
        VkDeviceMemory filterPongMemory;
        VkImageView filterPongView;

        VkImage gradientImage[2];
        VkDeviceMemory gradientMemory[2];
        VkImageView gradientView[2];

        VkDescriptorSetLayout gradientSamplingDescriptorSetLayout;
        VkDescriptorPool gradientDescriptorPool;
        VkDescriptorSet gradientSamplingDescriptorSet;
        VkDescriptorSetLayout gradientAtrousDescriptorSetLayout;
        VkDescriptorSet gradientAtrousDescriptorSets[2];

        std::vector<VkCommandBuffer> commandBuffers;

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
        float nearPlane = 0.1f;
        float farPlane = 1000.0f;

        glm::vec3 prevCameraPos;
        glm::vec3 prevCameraFront;
        glm::vec3 prevCameraUp;
        glm::vec3 prevCameraRight;
        glm::mat4 prevViewProjMatrix;

        bool firstMouse;
        double lastX, lastY;
        bool mouseCaptured;

        float lastFrameTime;
        uint32_t frameNumber = 0;
        bool firstFrame = true;
        uint32_t currentHistoryIndex = 0;

        float temporalAlpha = 0.1f;
        float momentsAlpha = 0.2f;
        float depthThreshold = 0.1f;
        float normalThreshold = 0.9f;

        float sfSigmaLuminance = 3.0f;
        float sfSigmaDepth = 3.0f;
        float sfSigmaNormal = 128.0f;
        int sfIterations = 5;

        bool useAdaptiveAlpha = true;
        float gradientScale = 2.0f;
        float antilagScale = 4.0f;

        PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR;

        static FirstAppRayTracing* instance;
    };

}