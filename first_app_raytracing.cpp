#include "first_app_raytracing.h"
#include <stdexcept>
#include <array>
#include <random>
#include <iostream>
#include <cmath>
#include <chrono>

namespace lve {

    FirstAppRayTracing* FirstAppRayTracing::instance = nullptr;

    class RandomGenerator {
    public:
        RandomGenerator(uint32_t seed = 42) : gen(seed), dist(0.0f, 1.0f) {}

        float randomFloat() { return dist(gen); }
        float randomFloat(float min, float max) { return min + (max - min) * dist(gen); }

        glm::vec3 randomVec3() {
            return glm::vec3(randomFloat(), randomFloat(), randomFloat());
        }

        glm::vec3 randomVec3(float min, float max) {
            return glm::vec3(randomFloat(min, max), randomFloat(min, max), randomFloat(min, max));
        }

    private:
        std::mt19937 gen;
        std::uniform_real_distribution<float> dist;
    };

    void FirstAppRayTracing::createOneWeekendFinalScene() {
        RandomGenerator rng(42);

        std::cout << "Creating Ray Tracing in One Weekend final scene..." << std::endl;

        accelerationStructure->addSphereMesh(
            glm::vec3(0.0f, -1000.0f, 0.0f),
            glm::vec3(0.5f, 0.5f, 0.5f),
            1000.0f,
            0.0f, 0.0f,
            64, 32
        );

        int sphereCount = 0;

        for (int a = -11; a < 11; a++) {
            for (int b = -11; b < 11; b++) {
                float choose_mat = rng.randomFloat();
                glm::vec3 center(
                    a + 0.9f * rng.randomFloat(),
                    0.2f,
                    b + 0.9f * rng.randomFloat()
                );

                if (glm::length(center - glm::vec3(4.0f, 0.2f, 0.0f)) > 0.9f) {
                    if (choose_mat < 0.8f) {
                        glm::vec3 albedo = rng.randomVec3() * rng.randomVec3();
                        accelerationStructure->addSphereMesh(center, albedo, 0.2f, 0.0f, 0.0f, 16, 8);
                    }
                    else if (choose_mat < 0.95f) {
                        glm::vec3 albedo = rng.randomVec3(0.5f, 1.0f);
                        float fuzz = rng.randomFloat(0.0f, 0.5f);
                        accelerationStructure->addSphereMesh(center, albedo, 0.2f, 1.0f, fuzz, 16, 8);
                    }
                    else {
                        accelerationStructure->addSphereMesh(center, glm::vec3(1.0f), 0.2f, 2.0f, 1.5f, 16, 8);
                    }
                    sphereCount++;
                }
            }
        }

        accelerationStructure->addSphereMesh(glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f), 1.0f, 2.0f, 1.5f, 32, 16);
        accelerationStructure->addSphereMesh(glm::vec3(-4.0f, 1.0f, 0.0f), glm::vec3(0.4f, 0.2f, 0.1f), 1.0f, 0.0f, 0.0f, 32, 16);
        accelerationStructure->addSphereMesh(glm::vec3(4.0f, 1.0f, 0.0f), glm::vec3(0.7f, 0.6f, 0.5f), 1.0f, 1.0f, 0.0f, 32, 16);

        std::cout << "Created " << sphereCount << " random spheres + 3 big spheres + ground" << std::endl;
    }

    void FirstAppRayTracing::initCamera() {
        cameraPos = glm::vec3(13.0f, 2.0f, 3.0f);

        glm::vec3 direction = glm::normalize(glm::vec3(0.0f) - cameraPos);
        yaw = glm::degrees(atan2(direction.z, direction.x));
        pitch = glm::degrees(asin(direction.y));

        cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
        moveSpeed = 5.0f;
        mouseSensitivity = 0.1f;
        vfov = 20.0f;
        defocusAngle = 0.0f;
        focusDist = 10.0f;

        firstMouse = true;
        lastX = WIDTH / 2.0;
        lastY = HEIGHT / 2.0;
        mouseCaptured = true;
        lastFrameTime = 0.0f;

        updateCameraVectors();

        prevCameraPos = cameraPos;
        prevCameraFront = cameraFront;
        prevCameraUp = cameraUp;
        prevCameraRight = cameraRight;
        prevViewProjMatrix = getProjectionMatrix() * getViewMatrix();

        instance = this;

        GLFWwindow* window = lveWindow.getGLFWwindow();
        glfwSetCursorPosCallback(window, mouseCallback);
        glfwSetKeyCallback(window, keyCallback);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }

    void FirstAppRayTracing::updateCameraVectors() {
        glm::vec3 front;
        front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.y = sin(glm::radians(pitch));
        front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        cameraFront = glm::normalize(front);

        cameraRight = glm::normalize(glm::cross(cameraFront, glm::vec3(0.0f, 1.0f, 0.0f)));
        cameraUp = glm::normalize(glm::cross(cameraRight, cameraFront));
    }

    glm::mat4 FirstAppRayTracing::getViewMatrix() const {
        return glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
    }

    glm::mat4 FirstAppRayTracing::getProjectionMatrix() const {
        float aspect = static_cast<float>(WIDTH) / static_cast<float>(HEIGHT);
        return glm::perspective(glm::radians(vfov), aspect, nearPlane, farPlane);
    }

    void FirstAppRayTracing::savePreviousFrameData() {
        prevCameraPos = cameraPos;
        prevCameraFront = cameraFront;
        prevCameraUp = cameraUp;
        prevCameraRight = cameraRight;
        prevViewProjMatrix = getProjectionMatrix() * getViewMatrix();
    }

    void FirstAppRayTracing::updateUniformBuffer() {
        CameraUBO ubo{};
        ubo.viewMatrix = getViewMatrix();
        ubo.projMatrix = getProjectionMatrix();
        ubo.viewProjMatrix = ubo.projMatrix * ubo.viewMatrix;
        ubo.invViewProjMatrix = glm::inverse(ubo.viewProjMatrix);
        ubo.prevViewProjMatrix = prevViewProjMatrix;
        ubo.resolution = glm::vec4(
            static_cast<float>(WIDTH),
            static_cast<float>(HEIGHT),
            1.0f / static_cast<float>(WIDTH),
            1.0f / static_cast<float>(HEIGHT)
        );
        ubo.cameraPos = glm::vec4(cameraPos, 1.0f);
        ubo.cameraFront = glm::vec4(cameraFront, 0.0f);
        ubo.cameraUp = glm::vec4(cameraUp, 0.0f);
        ubo.cameraRight = glm::vec4(cameraRight, 0.0f);

        float theta = glm::radians(vfov);
        float h = tan(theta / 2.0f);
        float aspect = static_cast<float>(WIDTH) / static_cast<float>(HEIGHT);
        float viewport_height = 2.0f * h * focusDist;
        float viewport_width = viewport_height * aspect;

        ubo.frustumInfo = glm::vec4(viewport_width, viewport_height, focusDist, 0.0f);

        ubo.prevCameraPos = glm::vec4(prevCameraPos, 1.0f);
        ubo.prevCameraFront = glm::vec4(prevCameraFront, 0.0f);
        ubo.prevCameraUp = glm::vec4(prevCameraUp, 0.0f);
        ubo.prevCameraRight = glm::vec4(prevCameraRight, 0.0f);

        ubo.resolution = glm::vec4(
            static_cast<float>(lveSwapChain.width()),
            static_cast<float>(lveSwapChain.height()),
            1.0f / static_cast<float>(lveSwapChain.width()),
            1.0f / static_cast<float>(lveSwapChain.height())
        );

        memcpy(cameraUBOMapped, &ubo, sizeof(ubo));

    }

    void FirstAppRayTracing::mouseCallback(GLFWwindow* window, double xpos, double ypos) {
        if (!instance || !instance->mouseCaptured) return;

        if (instance->firstMouse) {
            instance->lastX = xpos;
            instance->lastY = ypos;
            instance->firstMouse = false;
        }

        float xoffset = static_cast<float>(xpos - instance->lastX);
        float yoffset = static_cast<float>(instance->lastY - ypos);

        instance->lastX = xpos;
        instance->lastY = ypos;

        xoffset *= instance->mouseSensitivity;
        yoffset *= instance->mouseSensitivity;

        instance->yaw += xoffset;
        instance->pitch += yoffset;

        if (instance->pitch > 89.0f) instance->pitch = 89.0f;
        if (instance->pitch < -89.0f) instance->pitch = -89.0f;

        instance->updateCameraVectors();
    }

    void FirstAppRayTracing::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        if (!instance) return;

        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
            instance->mouseCaptured = !instance->mouseCaptured;
            if (instance->mouseCaptured) {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                instance->firstMouse = true;
            }
            else {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
        }

        if (key == GLFW_KEY_R && action == GLFW_PRESS) {
            instance->cameraPos = glm::vec3(13.0f, 2.0f, 3.0f);
            glm::vec3 direction = glm::normalize(glm::vec3(0.0f) - instance->cameraPos);
            instance->yaw = glm::degrees(atan2(direction.z, direction.x));
            instance->pitch = glm::degrees(asin(direction.y));
            instance->updateCameraVectors();
            instance->firstFrame = true;
            instance->frameNumber = 0;
            std::cout << "Camera reset - temporal history cleared" << std::endl;
        }

        if (key == GLFW_KEY_EQUAL && action == GLFW_PRESS) {
            instance->temporalAlpha = std::min(1.0f, instance->temporalAlpha + 0.05f);
            std::cout << "Temporal alpha: " << instance->temporalAlpha << std::endl;
        }
        if (key == GLFW_KEY_MINUS && action == GLFW_PRESS) {
            instance->temporalAlpha = std::max(0.01f, instance->temporalAlpha - 0.05f);
            std::cout << "Temporal alpha: " << instance->temporalAlpha << std::endl;
        }

        if (key == GLFW_KEY_RIGHT_BRACKET && action == GLFW_PRESS) {
            instance->sfIterations = std::min(5, instance->sfIterations + 1);
            std::cout << "Spatial filter iterations: " << instance->sfIterations << std::endl;
        }
        if (key == GLFW_KEY_LEFT_BRACKET && action == GLFW_PRESS) {
            instance->sfIterations = std::max(0, instance->sfIterations - 1);
            std::cout << "Spatial filter iterations: " << instance->sfIterations << std::endl;
        }

        if (key == GLFW_KEY_G && action == GLFW_PRESS) {
            instance->useAdaptiveAlpha = !instance->useAdaptiveAlpha;
            std::cout << "Adaptive alpha (gradient antilag): " << (instance->useAdaptiveAlpha ? "ON" : "OFF") << std::endl;
        }

        if (key == GLFW_KEY_PERIOD && action == GLFW_PRESS) {
            instance->antilagScale = std::min(20.0f, instance->antilagScale + 1.0f);
            std::cout << "Antilag scale: " << instance->antilagScale << std::endl;
        }
        if (key == GLFW_KEY_COMMA && action == GLFW_PRESS) {
            instance->antilagScale = std::max(1.0f, instance->antilagScale - 1.0f);
            std::cout << "Antilag scale: " << instance->antilagScale << std::endl;
        }
    }

    void FirstAppRayTracing::processInput(float deltaTime) {
        GLFWwindow* window = lveWindow.getGLFWwindow();

        float velocity = moveSpeed * deltaTime;

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            cameraPos += cameraFront * velocity;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            cameraPos -= cameraFront * velocity;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            cameraPos -= cameraRight * velocity;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            cameraPos += cameraRight * velocity;

        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
            cameraPos -= glm::vec3(0.0f, 1.0f, 0.0f) * velocity;
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
            cameraPos += glm::vec3(0.0f, 1.0f, 0.0f) * velocity;

        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
            moveSpeed = 15.0f;
        else
            moveSpeed = 5.0f;
    }

    FirstAppRayTracing::FirstAppRayTracing() {
        vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(
            vkGetDeviceProcAddr(lveDevice.device(), "vkCmdTraceRaysKHR"));

        initCamera();

        accelerationStructure = std::make_unique<LveAccelerationStructure>(lveDevice);
        createOneWeekendFinalScene();
        accelerationStructure->buildAccelerationStructures();

        rayTracingPipeline = std::make_unique<LveRayTracingPipeline>(
            lveDevice,
            "shaders/raygen.rgen.spv",
            "shaders/miss.rmiss.spv",
            "shaders/closesthit.rchit.spv"
        );

        createStorageImages();
        createVisibilityBufferImages();
        createHistoryBuffers();
        createForwardProjectionImages();
        createUniformBuffers();
        createRTDescriptorPool();
        createRTDescriptorSets();

        createFPDescriptorSetLayout();
        createFPDescriptorPool();
        createFPDescriptorSets();

        forwardProjectionPipeline = std::make_unique<LveComputePipeline>(
            lveDevice,
            "shaders/forward_projection.comp.spv",
            fpDescriptorSetLayout,
            sizeof(ForwardProjectionPushConstants)
        );

        createTADescriptorSetLayout();
        createTADescriptorPool();
        createTADescriptorSets();

        temporalAccumulationPipeline = std::make_unique<LveComputePipeline>(
            lveDevice,
            "shaders/temporal_accumulation.comp.spv",
            taDescriptorSetLayout,
            sizeof(TemporalAccumulationPushConstants)
        );

        createSFDescriptorSetLayout();
        createSFDescriptorPool();
        createSFDescriptorSets();

        spatialFilterPipeline = std::make_unique<LveComputePipeline>(
            lveDevice,
            "shaders/spatial_filter.comp.spv",
            sfDescriptorSetLayout,
            sizeof(SpatialFilterPushConstants)
        );

        createGradientImages();
        createGradientDescriptorSetLayout();
        createGradientDescriptorPool();
        createGradientDescriptorSets();

        gradientSamplingPipeline = std::make_unique<LveComputePipeline>(
            lveDevice,
            "shaders/gradient_sampling.comp.spv",
            gradientSamplingDescriptorSetLayout,
            sizeof(GradientSamplingPushConstants)
        );

        gradientAtrousPipeline = std::make_unique<LveComputePipeline>(
            lveDevice,
            "shaders/gradient_atrous.comp.spv",
            gradientAtrousDescriptorSetLayout,
            sizeof(GradientAtrousPushConstants)
        );

        createCommandBuffers();

        std::cout << "Initialization complete!" << std::endl;
        std::cout << "Controls: WASD move, Mouse look, Q/E vertical, Shift sprint" << std::endl;
        std::cout << "          +/- adjust temporal alpha, R reset camera" << std::endl;
        std::cout << "          [/] adjust spatial filter iterations, G toggle gradient" << std::endl;
        std::cout << "          ,/. adjust antilag scale" << std::endl;
    }

    FirstAppRayTracing::~FirstAppRayTracing() {
        vkDestroyImageView(lveDevice.device(), rtOutputImageView, nullptr);
        vkDestroyImage(lveDevice.device(), rtOutputImage, nullptr);
        vkFreeMemory(lveDevice.device(), rtOutputImageMemory, nullptr);

        vkDestroyImageView(lveDevice.device(), reshadedImageView, nullptr);
        vkDestroyImage(lveDevice.device(), reshadedImage, nullptr);
        vkFreeMemory(lveDevice.device(), reshadedImageMemory, nullptr);

        vkDestroyImageView(lveDevice.device(), seedImageView, nullptr);
        vkDestroyImage(lveDevice.device(), seedImage, nullptr);
        vkFreeMemory(lveDevice.device(), seedImageMemory, nullptr);

        vkDestroyImageView(lveDevice.device(), denoisedImageView, nullptr);
        vkDestroyImage(lveDevice.device(), denoisedImage, nullptr);
        vkFreeMemory(lveDevice.device(), denoisedImageMemory, nullptr);

        vkDestroyImageView(lveDevice.device(), filterPingView, nullptr);
        vkDestroyImage(lveDevice.device(), filterPingImage, nullptr);
        vkFreeMemory(lveDevice.device(), filterPingMemory, nullptr);

        vkDestroyImageView(lveDevice.device(), filterPongView, nullptr);
        vkDestroyImage(lveDevice.device(), filterPongImage, nullptr);
        vkFreeMemory(lveDevice.device(), filterPongMemory, nullptr);

        for (int i = 0; i < 2; i++) {
            vkDestroyImageView(lveDevice.device(), gradientView[i], nullptr);
            vkDestroyImage(lveDevice.device(), gradientImage[i], nullptr);
            vkFreeMemory(lveDevice.device(), gradientMemory[i], nullptr);
        }


        cleanupForwardProjectionImages();
        cleanupVisibilityBufferImages();
        cleanupHistoryBuffers();

        vkDestroyBuffer(lveDevice.device(), cameraUBOBuffer, nullptr);
        vkFreeMemory(lveDevice.device(), cameraUBOMemory, nullptr);

        vkDestroyDescriptorPool(lveDevice.device(), rtDescriptorPool, nullptr);
        vkDestroyDescriptorPool(lveDevice.device(), fpDescriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(lveDevice.device(), fpDescriptorSetLayout, nullptr);
        vkDestroyDescriptorPool(lveDevice.device(), taDescriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(lveDevice.device(), taDescriptorSetLayout, nullptr);
        vkDestroyDescriptorPool(lveDevice.device(), sfDescriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(lveDevice.device(), sfDescriptorSetLayout, nullptr);
        vkDestroyDescriptorPool(lveDevice.device(), gradientDescriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(lveDevice.device(), gradientSamplingDescriptorSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(lveDevice.device(), gradientAtrousDescriptorSetLayout, nullptr);

        vkFreeCommandBuffers(
            lveDevice.device(),
            lveDevice.getCommandPool(),
            static_cast<uint32_t>(commandBuffers.size()),
            commandBuffers.data()
        );

        instance = nullptr;
    }

    void FirstAppRayTracing::cleanupVisibilityBufferImages() {
        vkDestroyImageView(lveDevice.device(), visibilityBufferView, nullptr);
        vkDestroyImage(lveDevice.device(), visibilityBuffer, nullptr);
        vkFreeMemory(lveDevice.device(), visibilityBufferMemory, nullptr);

        vkDestroyImageView(lveDevice.device(), gBufferMotionView, nullptr);
        vkDestroyImage(lveDevice.device(), gBufferMotion, nullptr);
        vkFreeMemory(lveDevice.device(), gBufferMotionMemory, nullptr);

        vkDestroyImageView(lveDevice.device(), prevVisibilityBufferView, nullptr);
        vkDestroyImage(lveDevice.device(), prevVisibilityBuffer, nullptr);
        vkFreeMemory(lveDevice.device(), prevVisibilityBufferMemory, nullptr);
    }

    void FirstAppRayTracing::cleanupHistoryBuffers() {
        for (int i = 0; i < 2; i++) {
            vkDestroyImageView(lveDevice.device(), historyColorView[i], nullptr);
            vkDestroyImage(lveDevice.device(), historyColor[i], nullptr);
            vkFreeMemory(lveDevice.device(), historyColorMemory[i], nullptr);

            vkDestroyImageView(lveDevice.device(), historyMomentsView[i], nullptr);
            vkDestroyImage(lveDevice.device(), historyMoments[i], nullptr);
            vkFreeMemory(lveDevice.device(), historyMomentsMemory[i], nullptr);

            vkDestroyImageView(lveDevice.device(), historyLengthView[i], nullptr);
            vkDestroyImage(lveDevice.device(), historyLength[i], nullptr);
            vkFreeMemory(lveDevice.device(), historyLengthMemory[i], nullptr);
        }
    }

    void FirstAppRayTracing::cleanupForwardProjectionImages() {
        vkDestroyImageView(lveDevice.device(), prevColorView, nullptr);
        vkDestroyImage(lveDevice.device(), prevColor, nullptr);
        vkFreeMemory(lveDevice.device(), prevColorMemory, nullptr);

        vkDestroyImageView(lveDevice.device(), prevSeedView, nullptr);
        vkDestroyImage(lveDevice.device(), prevSeed, nullptr);
        vkFreeMemory(lveDevice.device(), prevSeedMemory, nullptr);

        vkDestroyImageView(lveDevice.device(), forwardProjectedColorView, nullptr);
        vkDestroyImage(lveDevice.device(), forwardProjectedColor, nullptr);
        vkFreeMemory(lveDevice.device(), forwardProjectedColorMemory, nullptr);

        vkDestroyImageView(lveDevice.device(), forwardProjectedSeedView, nullptr);
        vkDestroyImage(lveDevice.device(), forwardProjectedSeed, nullptr);
        vkFreeMemory(lveDevice.device(), forwardProjectedSeedMemory, nullptr);

        vkDestroyImageView(lveDevice.device(), forwardProjectedDepthView, nullptr);
        vkDestroyImage(lveDevice.device(), forwardProjectedDepth, nullptr);
        vkFreeMemory(lveDevice.device(), forwardProjectedDepthMemory, nullptr);
    }

    void FirstAppRayTracing::run() {
        auto startTime = std::chrono::high_resolution_clock::now();

        while (!lveWindow.shouldClose()) {
            glfwPollEvents();

            auto currentTime = std::chrono::high_resolution_clock::now();
            float time = std::chrono::duration<float>(currentTime - startTime).count();
            float deltaTime = time - lastFrameTime;
            lastFrameTime = time;

            processInput(deltaTime);
            drawFrame();
        }

        vkDeviceWaitIdle(lveDevice.device());
    }

    void createStorageImageHelper(
        LveDevice& device,
        uint32_t width, uint32_t height,
        VkFormat format,
        VkImage& image,
        VkDeviceMemory& memory,
        VkImageView& view
    ) {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        device.createImageWithInfo(
            imageInfo,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            image,
            memory
        );

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device.device(), &viewInfo, nullptr, &view) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image view!");
        }

        VkCommandBuffer commandBuffer = device.beginSingleTimeCommands();

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );

        device.endSingleTimeCommands(commandBuffer);
    }

    void FirstAppRayTracing::createStorageImages() {
        uint32_t width = lveSwapChain.width();
        uint32_t height = lveSwapChain.height();

        std::cout << "Creating storage images..." << std::endl;

        createStorageImageHelper(lveDevice, width, height, VK_FORMAT_R16G16B16A16_SFLOAT,
            rtOutputImage, rtOutputImageMemory, rtOutputImageView);

        createStorageImageHelper(lveDevice, width, height, VK_FORMAT_R16G16B16A16_SFLOAT,
            reshadedImage, reshadedImageMemory, reshadedImageView);

        createStorageImageHelper(lveDevice, width, height, VK_FORMAT_R32_UINT,
            seedImage, seedImageMemory, seedImageView);

        createStorageImageHelper(lveDevice, width, height, VK_FORMAT_R16G16B16A16_SFLOAT,
            denoisedImage, denoisedImageMemory, denoisedImageView);

        createStorageImageHelper(lveDevice, width, height, VK_FORMAT_R16G16B16A16_SFLOAT,
            filterPingImage, filterPingMemory, filterPingView);

        createStorageImageHelper(lveDevice, width, height, VK_FORMAT_R16G16B16A16_SFLOAT,
            filterPongImage, filterPongMemory, filterPongView);

        std::cout << "Storage images created successfully" << std::endl;
    }

    void FirstAppRayTracing::createForwardProjectionImages() {
        uint32_t width = lveSwapChain.width();
        uint32_t height = lveSwapChain.height();

        std::cout << "Creating forward projection images..." << std::endl;

        createStorageImageHelper(lveDevice, width, height, VK_FORMAT_R16G16B16A16_SFLOAT,
            prevColor, prevColorMemory, prevColorView);

        createStorageImageHelper(lveDevice, width, height, VK_FORMAT_R32_UINT,
            prevSeed, prevSeedMemory, prevSeedView);

        createStorageImageHelper(lveDevice, width, height, VK_FORMAT_R16G16B16A16_SFLOAT,
            forwardProjectedColor, forwardProjectedColorMemory, forwardProjectedColorView);

        createStorageImageHelper(lveDevice, width, height, VK_FORMAT_R32_UINT,
            forwardProjectedSeed, forwardProjectedSeedMemory, forwardProjectedSeedView);

        createStorageImageHelper(lveDevice, width, height, VK_FORMAT_R32_UINT,
            forwardProjectedDepth, forwardProjectedDepthMemory, forwardProjectedDepthView);

        std::cout << "Forward projection images created successfully" << std::endl;
    }

    void FirstAppRayTracing::createVisibilityBufferImages() {
        uint32_t width = lveSwapChain.width();
        uint32_t height = lveSwapChain.height();

        std::cout << "Creating Visibility Buffer images..." << std::endl;

        createStorageImageHelper(lveDevice, width, height, VK_FORMAT_R32G32_UINT,
            visibilityBuffer, visibilityBufferMemory, visibilityBufferView);

        createStorageImageHelper(lveDevice, width, height, VK_FORMAT_R16G16_SFLOAT,
            gBufferMotion, gBufferMotionMemory, gBufferMotionView);

        createStorageImageHelper(lveDevice, width, height, VK_FORMAT_R32G32_UINT,
            prevVisibilityBuffer, prevVisibilityBufferMemory, prevVisibilityBufferView);

        std::cout << "Visibility Buffer images created successfully" << std::endl;
    }

    void FirstAppRayTracing::createHistoryBuffers() {
        uint32_t width = lveSwapChain.width();
        uint32_t height = lveSwapChain.height();

        std::cout << "Creating History buffers (ping-pong)..." << std::endl;

        for (int i = 0; i < 2; i++) {
            createStorageImageHelper(lveDevice, width, height, VK_FORMAT_R16G16B16A16_SFLOAT,
                historyColor[i], historyColorMemory[i], historyColorView[i]);

            createStorageImageHelper(lveDevice, width, height, VK_FORMAT_R32G32_SFLOAT,
                historyMoments[i], historyMomentsMemory[i], historyMomentsView[i]);

            createStorageImageHelper(lveDevice, width, height, VK_FORMAT_R16_SFLOAT,
                historyLength[i], historyLengthMemory[i], historyLengthView[i]);
        }

        std::cout << "History buffers created successfully" << std::endl;
    }

    void FirstAppRayTracing::createGradientImages() {
        uint32_t width = lveSwapChain.width();
        uint32_t height = lveSwapChain.height();

        // Stratum resolution: ceil(fullRes / STRATUM_SIZE)
        const uint32_t STRATUM_SIZE = 3;
        uint32_t stratumWidth = (width + STRATUM_SIZE - 1) / STRATUM_SIZE;
        uint32_t stratumHeight = (height + STRATUM_SIZE - 1) / STRATUM_SIZE;

        std::cout << "Creating gradient images at stratum resolution ("
            << stratumWidth << "x" << stratumHeight << ")..." << std::endl;

        for (int i = 0; i < 2; i++) {
            createStorageImageHelper(lveDevice, stratumWidth, stratumHeight, VK_FORMAT_R16G16_SFLOAT,
                gradientImage[i], gradientMemory[i], gradientView[i]);
        }

        std::cout << "Gradient images created successfully" << std::endl;
    }

    void FirstAppRayTracing::createUniformBuffers() {
        VkDeviceSize bufferSize = sizeof(CameraUBO);

        lveDevice.createBuffer(
            bufferSize,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            cameraUBOBuffer,
            cameraUBOMemory
        );

        vkMapMemory(lveDevice.device(), cameraUBOMemory, 0, bufferSize, 0, &cameraUBOMapped);

        std::cout << "Camera UBO created" << std::endl;
    }

    void FirstAppRayTracing::createRTDescriptorPool() {
        VkDescriptorPoolSize poolSizes[] = {
            {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 10},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
        };

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 4;
        poolInfo.pPoolSizes = poolSizes;
        poolInfo.maxSets = 1;

        if (vkCreateDescriptorPool(lveDevice.device(), &poolInfo, nullptr, &rtDescriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create RT descriptor pool!");
        }
    }

    void FirstAppRayTracing::createRTDescriptorSets() {
        VkDescriptorSetLayout layout = rayTracingPipeline->getDescriptorSetLayout();

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = rtDescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &layout;

        if (vkAllocateDescriptorSets(lveDevice.device(), &allocInfo, &rtDescriptorSet) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate RT descriptor sets!");
        }

        std::vector<VkWriteDescriptorSet> writes;

        VkAccelerationStructureKHR tlas = accelerationStructure->getTLAS();
        VkWriteDescriptorSetAccelerationStructureKHR asInfo{};
        asInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
        asInfo.accelerationStructureCount = 1;
        asInfo.pAccelerationStructures = &tlas;

        VkWriteDescriptorSet asWrite{};
        asWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        asWrite.dstSet = rtDescriptorSet;
        asWrite.dstBinding = 0;
        asWrite.descriptorCount = 1;
        asWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        asWrite.pNext = &asInfo;
        writes.push_back(asWrite);

        std::vector<VkDescriptorImageInfo> imageInfos(9);

        imageInfos[0] = { VK_NULL_HANDLE, rtOutputImageView, VK_IMAGE_LAYOUT_GENERAL };
        imageInfos[1] = { VK_NULL_HANDLE, visibilityBufferView, VK_IMAGE_LAYOUT_GENERAL };
        imageInfos[2] = { VK_NULL_HANDLE, gBufferMotionView, VK_IMAGE_LAYOUT_GENERAL };
        imageInfos[3] = { VK_NULL_HANDLE, seedImageView, VK_IMAGE_LAYOUT_GENERAL };
        imageInfos[4] = { VK_NULL_HANDLE, forwardProjectedSeedView, VK_IMAGE_LAYOUT_GENERAL };
        imageInfos[5] = { VK_NULL_HANDLE, reshadedImageView, VK_IMAGE_LAYOUT_GENERAL };

        VkWriteDescriptorSet outputWrite{};
        outputWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        outputWrite.dstSet = rtDescriptorSet;
        outputWrite.dstBinding = 1;
        outputWrite.descriptorCount = 1;
        outputWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        outputWrite.pImageInfo = &imageInfos[0];
        writes.push_back(outputWrite);

        VkDescriptorBufferInfo sphereBufferInfo{};
        sphereBufferInfo.buffer = accelerationStructure->getSphereInfoBuffer();
        sphereBufferInfo.offset = 0;
        sphereBufferInfo.range = VK_WHOLE_SIZE;

        VkWriteDescriptorSet sphereWrite{};
        sphereWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        sphereWrite.dstSet = rtDescriptorSet;
        sphereWrite.dstBinding = 2;
        sphereWrite.descriptorCount = 1;
        sphereWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        sphereWrite.pBufferInfo = &sphereBufferInfo;
        writes.push_back(sphereWrite);

        VkWriteDescriptorSet visibilityWrite{};
        visibilityWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        visibilityWrite.dstSet = rtDescriptorSet;
        visibilityWrite.dstBinding = 3;
        visibilityWrite.descriptorCount = 1;
        visibilityWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        visibilityWrite.pImageInfo = &imageInfos[1];
        writes.push_back(visibilityWrite);

        VkWriteDescriptorSet motionWrite{};
        motionWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        motionWrite.dstSet = rtDescriptorSet;
        motionWrite.dstBinding = 4;
        motionWrite.descriptorCount = 1;
        motionWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        motionWrite.pImageInfo = &imageInfos[2];
        writes.push_back(motionWrite);

        VkDescriptorBufferInfo uboInfo{};
        uboInfo.buffer = cameraUBOBuffer;
        uboInfo.offset = 0;
        uboInfo.range = sizeof(CameraUBO);

        VkWriteDescriptorSet uboWrite{};
        uboWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        uboWrite.dstSet = rtDescriptorSet;
        uboWrite.dstBinding = 5;
        uboWrite.descriptorCount = 1;
        uboWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboWrite.pBufferInfo = &uboInfo;
        writes.push_back(uboWrite);

        VkWriteDescriptorSet seedWrite{};
        seedWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        seedWrite.dstSet = rtDescriptorSet;
        seedWrite.dstBinding = 6;
        seedWrite.descriptorCount = 1;
        seedWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        seedWrite.pImageInfo = &imageInfos[3];
        writes.push_back(seedWrite);

        VkWriteDescriptorSet fpSeedWrite{};
        fpSeedWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        fpSeedWrite.dstSet = rtDescriptorSet;
        fpSeedWrite.dstBinding = 7;
        fpSeedWrite.descriptorCount = 1;
        fpSeedWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        fpSeedWrite.pImageInfo = &imageInfos[4];
        writes.push_back(fpSeedWrite);

        VkWriteDescriptorSet reshadedWrite{};
        reshadedWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        reshadedWrite.dstSet = rtDescriptorSet;
        reshadedWrite.dstBinding = 8;
        reshadedWrite.descriptorCount = 1;
        reshadedWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        reshadedWrite.pImageInfo = &imageInfos[5];
        writes.push_back(reshadedWrite);

        vkUpdateDescriptorSets(lveDevice.device(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }

    void FirstAppRayTracing::createFPDescriptorSetLayout() {
        std::vector<VkDescriptorSetLayoutBinding> bindings(10);

        // 0~7번: Storage Image
        for (int i = 0; i < 8; i++) {
            bindings[i] = { static_cast<uint32_t>(i), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr };
        }

        // 8번: Sphere Info Buffer (쉐이더의 layout(binding = 8) buffer SphereInfoBuffer 대응)
        bindings[8] = { 8, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr };

        bindings[9] = { 9, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr };

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(lveDevice.device(), &layoutInfo, nullptr, &fpDescriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create FP descriptor set layout!");
        }
        std::cout << "FP descriptor set layout created with " << bindings.size() << " bindings" << std::endl;
    }

    void FirstAppRayTracing::createFPDescriptorPool() {
        VkDescriptorPoolSize poolSizes[] = {
         { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 8 },
         { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 },
         { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 }
        };

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 3; // poolSizes 배열 크기
        poolInfo.pPoolSizes = poolSizes;
        poolInfo.maxSets = 1;

        if (vkCreateDescriptorPool(lveDevice.device(), &poolInfo, nullptr, &fpDescriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create FP descriptor pool!");
        }
    }

    void FirstAppRayTracing::createFPDescriptorSets() {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = fpDescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &fpDescriptorSetLayout;

        if (vkAllocateDescriptorSets(lveDevice.device(), &allocInfo, &fpDescriptorSet) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate FP descriptor sets!");
        }

        // [수정] 쉐이더 바인딩 순서대로 재배열 (0~7번)
        std::vector<VkDescriptorImageInfo> imageInfos = {
            {VK_NULL_HANDLE, prevVisibilityBufferView, VK_IMAGE_LAYOUT_GENERAL},      // Binding 0: Prev Vis
            {VK_NULL_HANDLE, prevSeedView, VK_IMAGE_LAYOUT_GENERAL},                  // Binding 1: Prev Seed
            {VK_NULL_HANDLE, visibilityBufferView, VK_IMAGE_LAYOUT_GENERAL},          // Binding 2: Curr Vis (★중요: 현재 프레임 VisBuffer)
            {VK_NULL_HANDLE, prevColorView, VK_IMAGE_LAYOUT_GENERAL},                 // Binding 3: Prev Color
            {VK_NULL_HANDLE, gBufferMotionView, VK_IMAGE_LAYOUT_GENERAL},             // Binding 4: Motion Vector (★누락되었던 부분)
            {VK_NULL_HANDLE, forwardProjectedColorView, VK_IMAGE_LAYOUT_GENERAL},     // Binding 5: Output Color
            {VK_NULL_HANDLE, forwardProjectedSeedView, VK_IMAGE_LAYOUT_GENERAL},      // Binding 6: Output Seed
            {VK_NULL_HANDLE, forwardProjectedDepthView, VK_IMAGE_LAYOUT_GENERAL},     // Binding 7: Output Depth
        };

        std::vector<VkWriteDescriptorSet> writes;

        // 1. 이미지 쓰기 (0 ~ 7번)
        for (int i = 0; i < 8; i++) {
            VkWriteDescriptorSet write{};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = fpDescriptorSet;
            write.dstBinding = i;
            write.descriptorCount = 1;
            write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            write.pImageInfo = &imageInfos[i];
            writes.push_back(write);
        }

        // 2. 버퍼 쓰기 (8번: Sphere Info Buffer) - [추가됨]
        VkDescriptorBufferInfo sphereBufferInfo{};
        sphereBufferInfo.buffer = accelerationStructure->getSphereInfoBuffer();
        sphereBufferInfo.offset = 0;
        sphereBufferInfo.range = VK_WHOLE_SIZE;

        VkWriteDescriptorSet bufferWrite{};
        bufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        bufferWrite.dstSet = fpDescriptorSet;
        bufferWrite.dstBinding = 8; // Binding 8
        bufferWrite.descriptorCount = 1;
        bufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bufferWrite.pBufferInfo = &sphereBufferInfo;
        writes.push_back(bufferWrite);

        VkDescriptorBufferInfo uboInfo{};
        uboInfo.buffer = cameraUBOBuffer;
        uboInfo.offset = 0;
        uboInfo.range = sizeof(CameraUBO);

        VkWriteDescriptorSet uboWrite{};
        uboWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        uboWrite.dstSet = fpDescriptorSet;
        uboWrite.dstBinding = 9; // Binding 9
        uboWrite.descriptorCount = 1;
        uboWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboWrite.pBufferInfo = &uboInfo;


        writes.push_back(uboWrite);

        vkUpdateDescriptorSets(lveDevice.device(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

        std::cout << "FP descriptor sets created" << std::endl;
    }

    void FirstAppRayTracing::createTADescriptorSetLayout() {
        std::vector<VkDescriptorSetLayoutBinding> bindings(14);

        for (int i = 0; i < 12; i++) {
            bindings[i] = { static_cast<uint32_t>(i), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr };
        }
        bindings[12] = { 12, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr };
        bindings[13] = { 13, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr };

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(lveDevice.device(), &layoutInfo, nullptr, &taDescriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create TA descriptor set layout!");
        }
    }

    void FirstAppRayTracing::createTADescriptorPool() {
        VkDescriptorPoolSize poolSizes[] = {
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 24 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2 }
        };

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 3;
        poolInfo.pPoolSizes = poolSizes;
        poolInfo.maxSets = 2;

        if (vkCreateDescriptorPool(lveDevice.device(), &poolInfo, nullptr, &taDescriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create TA descriptor pool!");
        }
    }

    void FirstAppRayTracing::createTADescriptorSets() {
        VkDescriptorSetLayout layouts[2] = { taDescriptorSetLayout, taDescriptorSetLayout };

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = taDescriptorPool;
        allocInfo.descriptorSetCount = 2;
        allocInfo.pSetLayouts = layouts;

        if (vkAllocateDescriptorSets(lveDevice.device(), &allocInfo, taDescriptorSets) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate TA descriptor sets!");
        }

        for (int pingPong = 0; pingPong < 2; pingPong++) {
            int readIdx = pingPong;
            int writeIdx = 1 - pingPong;

            std::vector<VkDescriptorImageInfo> imageInfos = {
                {VK_NULL_HANDLE, rtOutputImageView, VK_IMAGE_LAYOUT_GENERAL},
                {VK_NULL_HANDLE, visibilityBufferView, VK_IMAGE_LAYOUT_GENERAL},
                {VK_NULL_HANDLE, prevVisibilityBufferView, VK_IMAGE_LAYOUT_GENERAL},
                {VK_NULL_HANDLE, gBufferMotionView, VK_IMAGE_LAYOUT_GENERAL},
                {VK_NULL_HANDLE, historyColorView[readIdx], VK_IMAGE_LAYOUT_GENERAL},
                {VK_NULL_HANDLE, historyMomentsView[readIdx], VK_IMAGE_LAYOUT_GENERAL},
                {VK_NULL_HANDLE, historyLengthView[readIdx], VK_IMAGE_LAYOUT_GENERAL},
                {VK_NULL_HANDLE, historyColorView[writeIdx], VK_IMAGE_LAYOUT_GENERAL},
                {VK_NULL_HANDLE, historyMomentsView[writeIdx], VK_IMAGE_LAYOUT_GENERAL},
                {VK_NULL_HANDLE, historyLengthView[writeIdx], VK_IMAGE_LAYOUT_GENERAL},
                {VK_NULL_HANDLE, denoisedImageView, VK_IMAGE_LAYOUT_GENERAL},
                {VK_NULL_HANDLE, gradientView[1], VK_IMAGE_LAYOUT_GENERAL},
            };

            std::vector<VkWriteDescriptorSet> writes(14);
            for (int i = 0; i < 12; i++) {
                writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writes[i].dstSet = taDescriptorSets[pingPong];
                writes[i].dstBinding = i;
                writes[i].descriptorCount = 1;
                writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                writes[i].pImageInfo = &imageInfos[i];
            }

            VkDescriptorBufferInfo sphereBufferInfo{};
            sphereBufferInfo.buffer = accelerationStructure->getSphereInfoBuffer();
            sphereBufferInfo.offset = 0;
            sphereBufferInfo.range = VK_WHOLE_SIZE;

            writes[12].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[12].dstSet = taDescriptorSets[pingPong];
            writes[12].dstBinding = 12;
            writes[12].descriptorCount = 1;
            writes[12].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            writes[12].pBufferInfo = &sphereBufferInfo;

            VkDescriptorBufferInfo uboInfo{};
            uboInfo.buffer = cameraUBOBuffer;
            uboInfo.offset = 0;
            uboInfo.range = sizeof(CameraUBO);

            writes[13].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[13].dstSet = taDescriptorSets[pingPong];
            writes[13].dstBinding = 13;
            writes[13].descriptorCount = 1;
            writes[13].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writes[13].pBufferInfo = &uboInfo;

            vkUpdateDescriptorSets(lveDevice.device(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
        }
    }

    void FirstAppRayTracing::createSFDescriptorSetLayout() {
        std::vector<VkDescriptorSetLayoutBinding> bindings(6);

        for (int i = 0; i < 4; i++) {
            bindings[i] = { static_cast<uint32_t>(i), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr };
        }
        bindings[4] = { 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr };
        bindings[5] = { 5, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr };

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(lveDevice.device(), &layoutInfo, nullptr, &sfDescriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create SF descriptor set layout!");
        }
    }

    void FirstAppRayTracing::createSFDescriptorPool() {
        VkDescriptorPoolSize poolSizes[] = {
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 8 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2 }
        };

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 3;
        poolInfo.pPoolSizes = poolSizes;
        poolInfo.maxSets = 2;

        if (vkCreateDescriptorPool(lveDevice.device(), &poolInfo, nullptr, &sfDescriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create SF descriptor pool!");
        }
    }

    void FirstAppRayTracing::createSFDescriptorSets() {
        VkDescriptorSetLayout layouts[2] = { sfDescriptorSetLayout, sfDescriptorSetLayout };

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = sfDescriptorPool;
        allocInfo.descriptorSetCount = 2;
        allocInfo.pSetLayouts = layouts;

        if (vkAllocateDescriptorSets(lveDevice.device(), &allocInfo, sfDescriptorSets) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate SF descriptor sets!");
        }

        VkImageView inputViews[2] = { filterPingView, filterPongView };
        VkImageView outputViews[2] = { filterPongView, filterPingView };

        for (int set = 0; set < 2; set++) {
            std::vector<VkDescriptorImageInfo> imageInfos = {
                {VK_NULL_HANDLE, inputViews[set], VK_IMAGE_LAYOUT_GENERAL},
                {VK_NULL_HANDLE, outputViews[set], VK_IMAGE_LAYOUT_GENERAL},
                {VK_NULL_HANDLE, visibilityBufferView, VK_IMAGE_LAYOUT_GENERAL},
                {VK_NULL_HANDLE, historyMomentsView[0], VK_IMAGE_LAYOUT_GENERAL},
            };

            std::vector<VkWriteDescriptorSet> writes(6);
            for (int i = 0; i < 4; i++) {
                writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writes[i].dstSet = sfDescriptorSets[set];
                writes[i].dstBinding = i;
                writes[i].descriptorCount = 1;
                writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                writes[i].pImageInfo = &imageInfos[i];
            }

            VkDescriptorBufferInfo sphereBufferInfo{};
            sphereBufferInfo.buffer = accelerationStructure->getSphereInfoBuffer();
            sphereBufferInfo.offset = 0;
            sphereBufferInfo.range = VK_WHOLE_SIZE;

            writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[4].dstSet = sfDescriptorSets[set];
            writes[4].dstBinding = 4;
            writes[4].descriptorCount = 1;
            writes[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            writes[4].pBufferInfo = &sphereBufferInfo;

            VkDescriptorBufferInfo uboInfo{};
            uboInfo.buffer = cameraUBOBuffer;
            uboInfo.offset = 0;
            uboInfo.range = sizeof(CameraUBO);

            writes[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[5].dstSet = sfDescriptorSets[set];
            writes[5].dstBinding = 5;
            writes[5].descriptorCount = 1;
            writes[5].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writes[5].pBufferInfo = &uboInfo;

            vkUpdateDescriptorSets(lveDevice.device(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
        }
    }

    void FirstAppRayTracing::createGradientDescriptorSetLayout() {
        std::vector<VkDescriptorSetLayoutBinding> samplingBindings = {
        {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, // Reshaded
        {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, // Fwd Proj
        {2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, // Curr Vis
        {3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, // Prev Vis
        {4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, // Motion
        {5, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, // Gradient Out (stratum res)
        {7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},// Sphere Buffer
        {8, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},// Camera UBO
        };
        VkDescriptorSetLayoutCreateInfo samplingLayoutInfo{};
        samplingLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        samplingLayoutInfo.bindingCount = static_cast<uint32_t>(samplingBindings.size());
        samplingLayoutInfo.pBindings = samplingBindings.data();

        if (vkCreateDescriptorSetLayout(lveDevice.device(), &samplingLayoutInfo, nullptr,
            &gradientSamplingDescriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create gradient sampling descriptor set layout!");
        }

        std::vector<VkDescriptorSetLayoutBinding> atrousBindings = {
            {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        };

        VkDescriptorSetLayoutCreateInfo atrousLayoutInfo{};
        atrousLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        atrousLayoutInfo.bindingCount = static_cast<uint32_t>(atrousBindings.size());
        atrousLayoutInfo.pBindings = atrousBindings.data();

        if (vkCreateDescriptorSetLayout(lveDevice.device(), &atrousLayoutInfo, nullptr,
            &gradientAtrousDescriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create gradient atrous descriptor set layout!");
        }
    }

    void FirstAppRayTracing::createGradientDescriptorPool() {
        VkDescriptorPoolSize poolSizes[] = {
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 20 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 }
        };

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 3;
        poolInfo.pPoolSizes = poolSizes;
        poolInfo.maxSets = 3;

        if (vkCreateDescriptorPool(lveDevice.device(), &poolInfo, nullptr, &gradientDescriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create gradient descriptor pool!");
        }
    }

    void FirstAppRayTracing::createGradientDescriptorSets() {
        VkDescriptorSetAllocateInfo samplingAllocInfo{};
        samplingAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        samplingAllocInfo.descriptorPool = gradientDescriptorPool;
        samplingAllocInfo.descriptorSetCount = 1;
        samplingAllocInfo.pSetLayouts = &gradientSamplingDescriptorSetLayout;

        if (vkAllocateDescriptorSets(lveDevice.device(), &samplingAllocInfo, &gradientSamplingDescriptorSet) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate gradient sampling descriptor set!");
        }

        // Bindings: 0=reshaded, 1=fwdProj, 2=currVis, 3=prevVis, 4=motion, 5=gradientOut(stratum)
        // Binding 6 removed (was gradientSampleMask), 7=spheres, 8=cameraUBO
        std::vector<VkDescriptorImageInfo> samplingImageInfos = {
            {VK_NULL_HANDLE, reshadedImageView, VK_IMAGE_LAYOUT_GENERAL},
            {VK_NULL_HANDLE, forwardProjectedColorView, VK_IMAGE_LAYOUT_GENERAL},
            {VK_NULL_HANDLE, visibilityBufferView, VK_IMAGE_LAYOUT_GENERAL},
            {VK_NULL_HANDLE, prevVisibilityBufferView, VK_IMAGE_LAYOUT_GENERAL},
            {VK_NULL_HANDLE, gBufferMotionView, VK_IMAGE_LAYOUT_GENERAL},
            {VK_NULL_HANDLE, gradientView[0], VK_IMAGE_LAYOUT_GENERAL},
        };

        std::vector<VkWriteDescriptorSet> samplingWrites;
        for (int i = 0; i < 6; i++) {
            VkWriteDescriptorSet write{};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = gradientSamplingDescriptorSet;
            write.dstBinding = i;
            write.descriptorCount = 1;
            write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            write.pImageInfo = &samplingImageInfos[i];
            samplingWrites.push_back(write);
        }

        VkDescriptorBufferInfo sphereBufferInfo{};
        sphereBufferInfo.buffer = accelerationStructure->getSphereInfoBuffer();
        sphereBufferInfo.offset = 0;
        sphereBufferInfo.range = VK_WHOLE_SIZE;

        VkWriteDescriptorSet sphereWrite{};
        sphereWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        sphereWrite.dstSet = gradientSamplingDescriptorSet;
        sphereWrite.dstBinding = 7; // 쉐이더와 일치시킴
        sphereWrite.descriptorCount = 1;
        sphereWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        sphereWrite.pBufferInfo = &sphereBufferInfo;
        samplingWrites.push_back(sphereWrite);

        VkDescriptorBufferInfo uboInfo{};
        uboInfo.buffer = cameraUBOBuffer;
        uboInfo.offset = 0;
        uboInfo.range = sizeof(CameraUBO);

        VkWriteDescriptorSet uboWrite{};
        uboWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        uboWrite.dstSet = gradientSamplingDescriptorSet;
        uboWrite.dstBinding = 8; // 쉐이더와 일치시킴
        uboWrite.descriptorCount = 1;
        uboWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboWrite.pBufferInfo = &uboInfo;
        samplingWrites.push_back(uboWrite);

        vkUpdateDescriptorSets(lveDevice.device(), static_cast<uint32_t>(samplingWrites.size()),
            samplingWrites.data(), 0, nullptr);

        VkDescriptorSetLayout atrousLayouts[2] = { gradientAtrousDescriptorSetLayout, gradientAtrousDescriptorSetLayout };
        VkDescriptorSetAllocateInfo atrousAllocInfo{};
        atrousAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        atrousAllocInfo.descriptorPool = gradientDescriptorPool;
        atrousAllocInfo.descriptorSetCount = 2;
        atrousAllocInfo.pSetLayouts = atrousLayouts;

        if (vkAllocateDescriptorSets(lveDevice.device(), &atrousAllocInfo, gradientAtrousDescriptorSets) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate gradient atrous descriptor sets!");
        }

        VkImageView atrousInputViews[2] = { gradientView[0], gradientView[1] };
        VkImageView atrousOutputViews[2] = { gradientView[1], gradientView[0] };

        for (int set = 0; set < 2; set++) {
            std::vector<VkDescriptorImageInfo> atrousImageInfos = {
                {VK_NULL_HANDLE, atrousInputViews[set], VK_IMAGE_LAYOUT_GENERAL},
                {VK_NULL_HANDLE, atrousOutputViews[set], VK_IMAGE_LAYOUT_GENERAL},
                {VK_NULL_HANDLE, visibilityBufferView, VK_IMAGE_LAYOUT_GENERAL},
            };

            std::vector<VkWriteDescriptorSet> atrousWrites;
            for (int i = 0; i < 3; i++) {
                VkWriteDescriptorSet w{};
                w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                w.dstSet = gradientAtrousDescriptorSets[set];
                w.dstBinding = i;
                w.descriptorCount = 1;
                w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                w.pImageInfo = &atrousImageInfos[i];
                atrousWrites.push_back(w);
            }
            VkWriteDescriptorSet wSphere{};
            wSphere.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            wSphere.dstSet = gradientAtrousDescriptorSets[set];
            wSphere.dstBinding = 3;
            wSphere.descriptorCount = 1;
            wSphere.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            wSphere.pBufferInfo = &sphereBufferInfo;
            atrousWrites.push_back(wSphere);

            VkWriteDescriptorSet wUbo{};
            wUbo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            wUbo.dstSet = gradientAtrousDescriptorSets[set];
            wUbo.dstBinding = 4;
            wUbo.descriptorCount = 1;
            wUbo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            wUbo.pBufferInfo = &uboInfo;
            atrousWrites.push_back(wUbo);

            vkUpdateDescriptorSets(lveDevice.device(), static_cast<uint32_t>(atrousWrites.size()),
                atrousWrites.data(), 0, nullptr);
        }
    }

    void FirstAppRayTracing::createCommandBuffers() {
        commandBuffers.resize(lveSwapChain.imageCount());

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = lveDevice.getCommandPool();
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

        if (vkAllocateCommandBuffers(lveDevice.device(), &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate command buffers!");
        }
    }

    void FirstAppRayTracing::clearForwardProjectionBuffers(VkCommandBuffer commandBuffer) {
        VkClearColorValue clearDepth = {};
        clearDepth.uint32[0] = 0xFFFFFFFF;

        VkClearColorValue clearZero = {};
        clearZero.uint32[0] = 0;
        clearZero.float32[0] = 0.0f;
        clearZero.float32[1] = 0.0f;
        clearZero.float32[2] = 0.0f;
        clearZero.float32[3] = 0.0f;

        VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

        VkImageMemoryBarrier barriers[3] = {};
        barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barriers[0].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[0].image = forwardProjectedDepth;
        barriers[0].subresourceRange = range;
        barriers[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        barriers[1] = barriers[0];
        barriers[1].image = forwardProjectedColor;

        barriers[2] = barriers[0];
        barriers[2].image = forwardProjectedSeed;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 3, barriers);

        vkCmdClearColorImage(commandBuffer, forwardProjectedDepth, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            &clearDepth, 1, &range);
        vkCmdClearColorImage(commandBuffer, forwardProjectedColor, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            &clearZero, 1, &range);
        vkCmdClearColorImage(commandBuffer, forwardProjectedSeed, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            &clearZero, 1, &range);

        for (int i = 0; i < 3; i++) {
            barriers[i].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barriers[i].newLayout = VK_IMAGE_LAYOUT_GENERAL;
            barriers[i].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barriers[i].dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        }

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 3, barriers);
    }

    void FirstAppRayTracing::copyCurrentToPreviousVisibilityBuffer(VkCommandBuffer commandBuffer) {
        std::vector<VkImageMemoryBarrier> barriers(2);

        barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barriers[0].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[0].image = visibilityBuffer;
        barriers[0].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        barriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        barriers[1] = barriers[0];
        barriers[1].image = prevVisibilityBuffer;
        barriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barriers[1].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
            VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 2, barriers.data());

        VkImageCopy copyRegion{};
        copyRegion.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        copyRegion.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        copyRegion.extent = { lveSwapChain.width(), lveSwapChain.height(), 1 };

        vkCmdCopyImage(commandBuffer, visibilityBuffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            prevVisibilityBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        barriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barriers[0].newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barriers[0].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

        barriers[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barriers[1].newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barriers[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barriers[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0,
            0, nullptr, 0, nullptr, 2, barriers.data());
    }

    void FirstAppRayTracing::copyCurrentToPreviousBuffers(VkCommandBuffer commandBuffer) {
        VkImageMemoryBarrier barriers[4] = {};

        barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barriers[0].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[0].image = rtOutputImage;
        barriers[0].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        barriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        barriers[1] = barriers[0];
        barriers[1].image = prevColor;
        barriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barriers[1].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        barriers[2] = barriers[0];
        barriers[2].image = seedImage;

        barriers[3] = barriers[1];
        barriers[3].image = prevSeed;

        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 4, barriers);

        VkImageCopy copyRegion{};
        copyRegion.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        copyRegion.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        copyRegion.extent = { lveSwapChain.width(), lveSwapChain.height(), 1 };

        vkCmdCopyImage(commandBuffer, rtOutputImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            prevColor, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
        vkCmdCopyImage(commandBuffer, seedImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            prevSeed, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        for (int i = 0; i < 4; i++) {
            barriers[i].oldLayout = barriers[i].newLayout;
            barriers[i].newLayout = VK_IMAGE_LAYOUT_GENERAL;
            barriers[i].srcAccessMask = (i % 2 == 0) ? VK_ACCESS_TRANSFER_READ_BIT : VK_ACCESS_TRANSFER_WRITE_BIT;
            barriers[i].dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        }

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0,
            0, nullptr, 0, nullptr, 4, barriers);
    }

    void FirstAppRayTracing::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        uint32_t groupCountX = (lveSwapChain.width() + 15) / 16;
        uint32_t groupCountY = (lveSwapChain.height() + 15) / 16;

        VkMemoryBarrier memBarrier{};
        memBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;

        clearForwardProjectionBuffers(commandBuffer);

        if (frameNumber > 0 && useAdaptiveAlpha) {
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, forwardProjectionPipeline->getPipeline());
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                forwardProjectionPipeline->getPipelineLayout(), 0, 1, &fpDescriptorSet, 0, nullptr);

            ForwardProjectionPushConstants fpPushConstants{};
            fpPushConstants.viewProjMatrix = getProjectionMatrix() * getViewMatrix();
            fpPushConstants.resolution = glm::vec4(
                static_cast<float>(lveSwapChain.width()),
                static_cast<float>(lveSwapChain.height()),
                1.0f / static_cast<float>(lveSwapChain.width()),
                1.0f / static_cast<float>(lveSwapChain.height())
            );
            fpPushConstants.frameNumber = frameNumber;
            fpPushConstants.depthThreshold = depthThreshold;
            fpPushConstants.normalThreshold = normalThreshold;

            vkCmdPushConstants(commandBuffer, forwardProjectionPipeline->getPipelineLayout(),
                VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ForwardProjectionPushConstants), &fpPushConstants);

            vkCmdDispatch(commandBuffer, groupCountX, groupCountY, 1);

            memBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            memBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 1, &memBarrier, 0, nullptr, 0, nullptr);
        }

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rayTracingPipeline->getPipeline());
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
            rayTracingPipeline->getPipelineLayout(), 0, 1, &rtDescriptorSet, 0, nullptr);

        CameraPushConstants rtPushConstants{};
        rtPushConstants.position = cameraPos;
        rtPushConstants.forward = cameraFront;
        rtPushConstants.right = cameraRight;
        rtPushConstants.up = cameraUp;
        rtPushConstants.vfov = vfov;
        rtPushConstants.defocus_angle = defocusAngle;
        rtPushConstants.focus_dist = focusDist;
        rtPushConstants.frameNumber = frameNumber;

        vkCmdPushConstants(commandBuffer, rayTracingPipeline->getPipelineLayout(),
            VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
            0, sizeof(CameraPushConstants), &rtPushConstants);

        VkStridedDeviceAddressRegionKHR raygenRegion = rayTracingPipeline->getRaygenRegion();
        VkStridedDeviceAddressRegionKHR missRegion = rayTracingPipeline->getMissRegion();
        VkStridedDeviceAddressRegionKHR hitRegion = rayTracingPipeline->getHitRegion();
        VkStridedDeviceAddressRegionKHR callableRegion = rayTracingPipeline->getCallableRegion();

        vkCmdTraceRaysKHR(commandBuffer, &raygenRegion, &missRegion, &hitRegion, &callableRegion,
            lveSwapChain.width(), lveSwapChain.height(), 1);

        memBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        memBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memBarrier, 0, nullptr, 0, nullptr);

        if (useAdaptiveAlpha && frameNumber > 0) {
            // Stratum resolution dispatch sizes
            const uint32_t STRATUM_SIZE = 3;
            uint32_t stratumWidth = (lveSwapChain.width() + STRATUM_SIZE - 1) / STRATUM_SIZE;
            uint32_t stratumHeight = (lveSwapChain.height() + STRATUM_SIZE - 1) / STRATUM_SIZE;
            uint32_t stratumGroupX = (stratumWidth + 15) / 16;
            uint32_t stratumGroupY = (stratumHeight + 15) / 16;

            // --- Gradient Sampling (dispatched at stratum resolution) ---
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, gradientSamplingPipeline->getPipeline());
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                gradientSamplingPipeline->getPipelineLayout(), 0, 1, &gradientSamplingDescriptorSet, 0, nullptr);

            GradientSamplingPushConstants gsPushConstants{};
            gsPushConstants.resolution = glm::vec4(
                static_cast<float>(lveSwapChain.width()),
                static_cast<float>(lveSwapChain.height()),
                1.0f / static_cast<float>(lveSwapChain.width()),
                1.0f / static_cast<float>(lveSwapChain.height())
            );
            gsPushConstants.frameNumber = frameNumber;
            gsPushConstants.gradientScale = gradientScale;
            gsPushConstants.depthThreshold = depthThreshold;
            gsPushConstants.normalThreshold = normalThreshold;

            vkCmdPushConstants(commandBuffer, gradientSamplingPipeline->getPipelineLayout(),
                VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(GradientSamplingPushConstants), &gsPushConstants);

            vkCmdDispatch(commandBuffer, stratumGroupX, stratumGroupY, 1);

            memBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            memBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memBarrier, 0, nullptr, 0, nullptr);

            // --- Gradient A-Trous (dispatched at stratum resolution) ---
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, gradientAtrousPipeline->getPipeline());

            for (int pass = 0; pass < 3; pass++) {
                int currentSet = pass % 2;

                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                    gradientAtrousPipeline->getPipelineLayout(), 0, 1, &gradientAtrousDescriptorSets[currentSet], 0, nullptr);

                GradientAtrousPushConstants gaPushConstants{};
                gaPushConstants.fullResolution = glm::vec4(
                    static_cast<float>(lveSwapChain.width()),
                    static_cast<float>(lveSwapChain.height()),
                    1.0f / static_cast<float>(lveSwapChain.width()),
                    1.0f / static_cast<float>(lveSwapChain.height())
                );
                gaPushConstants.stepSize = 1 << pass;
                gaPushConstants.sigmaDepth = sfSigmaDepth;
                gaPushConstants.sigmaNormal = sfSigmaNormal;
                gaPushConstants.frameNumber = frameNumber;

                vkCmdPushConstants(commandBuffer, gradientAtrousPipeline->getPipelineLayout(),
                    VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(GradientAtrousPushConstants), &gaPushConstants);

                vkCmdDispatch(commandBuffer, stratumGroupX, stratumGroupY, 1);

                if (pass < 2) {
                    memBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                    memBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memBarrier, 0, nullptr, 0, nullptr);
                }
            }

            memBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            memBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memBarrier, 0, nullptr, 0, nullptr);
        }

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, temporalAccumulationPipeline->getPipeline());
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
            temporalAccumulationPipeline->getPipelineLayout(), 0, 1, &taDescriptorSets[currentHistoryIndex], 0, nullptr);

        TemporalAccumulationPushConstants taPushConstants{};
        taPushConstants.resolution = glm::vec4(
            static_cast<float>(lveSwapChain.width()),
            static_cast<float>(lveSwapChain.height()),
            1.0f / static_cast<float>(lveSwapChain.width()),
            1.0f / static_cast<float>(lveSwapChain.height())
        );
        taPushConstants.alpha = temporalAlpha;
        taPushConstants.momentsAlpha = momentsAlpha;
        taPushConstants.depthThreshold = depthThreshold;
        taPushConstants.normalThreshold = normalThreshold;
        taPushConstants.frameNumber = frameNumber;
        taPushConstants.useAdaptiveAlpha = useAdaptiveAlpha ? 1 : 0;
        taPushConstants.antilagScale = antilagScale;

        vkCmdPushConstants(commandBuffer, temporalAccumulationPipeline->getPipelineLayout(),
            VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(TemporalAccumulationPushConstants), &taPushConstants);

        vkCmdDispatch(commandBuffer, groupCountX, groupCountY, 1);

        memBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        memBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memBarrier, 0, nullptr, 0, nullptr);

        // ============================================
        // Copy temporal accumulation output to filterPing for spatial filter input
        // ============================================
        {
            VkImageMemoryBarrier copyBarriers[2] = {};

            // denoisedImage: GENERAL -> TRANSFER_SRC
            copyBarriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            copyBarriers[0].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            copyBarriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            copyBarriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            copyBarriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            copyBarriers[0].image = denoisedImage;
            copyBarriers[0].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
            copyBarriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            copyBarriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

            // filterPingImage: GENERAL -> TRANSFER_DST
            copyBarriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            copyBarriers[1].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            copyBarriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            copyBarriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            copyBarriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            copyBarriers[1].image = filterPingImage;
            copyBarriers[1].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
            copyBarriers[1].srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            copyBarriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 2, copyBarriers);

            VkImageCopy copyRegion{};
            copyRegion.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
            copyRegion.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
            copyRegion.extent = { lveSwapChain.width(), lveSwapChain.height(), 1 };

            vkCmdCopyImage(commandBuffer,
                denoisedImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                filterPingImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1, &copyRegion);

            // Transition both back to GENERAL
            copyBarriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            copyBarriers[0].newLayout = VK_IMAGE_LAYOUT_GENERAL;
            copyBarriers[0].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            copyBarriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

            copyBarriers[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            copyBarriers[1].newLayout = VK_IMAGE_LAYOUT_GENERAL;
            copyBarriers[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            copyBarriers[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 2, copyBarriers);
        }

        // ============================================
        // Spatial À-trous Filter (SVGF core denoising)
        // ============================================
        if (sfIterations > 0) {
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, spatialFilterPipeline->getPipeline());

            for (int pass = 0; pass < sfIterations; pass++) {
                int currentSet = pass % 2;

                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                    spatialFilterPipeline->getPipelineLayout(), 0, 1, &sfDescriptorSets[currentSet], 0, nullptr);

                SpatialFilterPushConstants sfPushConstants{};
                sfPushConstants.resolution = glm::vec4(
                    static_cast<float>(lveSwapChain.width()),
                    static_cast<float>(lveSwapChain.height()),
                    1.0f / static_cast<float>(lveSwapChain.width()),
                    1.0f / static_cast<float>(lveSwapChain.height())
                );
                sfPushConstants.stepSize = 1 << pass;
                sfPushConstants.sigmaLuminance = sfSigmaLuminance;
                sfPushConstants.sigmaDepth = sfSigmaDepth;
                sfPushConstants.sigmaNormal = sfSigmaNormal;

                vkCmdPushConstants(commandBuffer, spatialFilterPipeline->getPipelineLayout(),
                    VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(SpatialFilterPushConstants), &sfPushConstants);

                vkCmdDispatch(commandBuffer, groupCountX, groupCountY, 1);

                if (pass < sfIterations - 1) {
                    memBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                    memBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memBarrier, 0, nullptr, 0, nullptr);
                }
            }

            memBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            memBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &memBarrier, 0, nullptr, 0, nullptr);
        }

        // Determine final output: if spatial filter ran, result is in the last output buffer
        // Even iterations (0,2,4) write to filterPong, odd iterations (1,3) write to filterPing
        // After sfIterations passes, last pass index is (sfIterations-1)
        // If (sfIterations-1) % 2 == 0 → output in filterPong, else filterPing
        VkImage finalOutputImage = denoisedImage;
        if (sfIterations > 0) {
            finalOutputImage = ((sfIterations - 1) % 2 == 0) ? filterPongImage : filterPingImage;
        }

        VkImageMemoryBarrier barrier1{};
        barrier1.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier1.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier1.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier1.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier1.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier1.image = finalOutputImage;
        barrier1.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        barrier1.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier1.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier1);

        VkImage swapChainImage = lveSwapChain.getSwapChainImage(imageIndex);

        VkImageMemoryBarrier barrier2{};
        barrier2.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier2.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier2.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier2.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier2.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier2.image = swapChainImage;
        barrier2.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        barrier2.srcAccessMask = 0;
        barrier2.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier2);

        VkImageBlit blitRegion{};
        blitRegion.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        blitRegion.srcOffsets[0] = { 0, 0, 0 };
        blitRegion.srcOffsets[1] = { static_cast<int32_t>(lveSwapChain.width()), static_cast<int32_t>(lveSwapChain.height()), 1 };
        blitRegion.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        blitRegion.dstOffsets[0] = { 0, 0, 0 };
        blitRegion.dstOffsets[1] = { static_cast<int32_t>(lveSwapChain.width()), static_cast<int32_t>(lveSwapChain.height()), 1 };

        vkCmdBlitImage(commandBuffer, finalOutputImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            swapChainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blitRegion, VK_FILTER_NEAREST);

        VkImageMemoryBarrier barrier3{};
        barrier3.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier3.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier3.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrier3.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier3.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier3.image = swapChainImage;
        barrier3.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        barrier3.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier3.dstAccessMask = 0;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier3);

        barrier1.image = finalOutputImage;
        barrier1.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier1.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier1.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier1.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier1);

        copyCurrentToPreviousVisibilityBuffer(commandBuffer);
        copyCurrentToPreviousBuffers(commandBuffer);

        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer!");
        }
    }

    void FirstAppRayTracing::drawFrame() {
        uint32_t imageIndex;
        auto result = lveSwapChain.acquireNextImage(&imageIndex);

        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        updateUniformBuffer();

        vkResetCommandBuffer(commandBuffers[imageIndex], 0);
        recordCommandBuffer(commandBuffers[imageIndex], imageIndex);

        result = lveSwapChain.submitCommandBuffers(&commandBuffers[imageIndex], &imageIndex);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image!");
        }
        vkQueueWaitIdle(lveDevice.presentQueue());

        savePreviousFrameData();

        currentHistoryIndex = 1 - currentHistoryIndex;

        frameNumber++;
        firstFrame = false;
    }

}