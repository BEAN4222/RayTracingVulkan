#include "first_app_raytracing.h"
#include <stdexcept>
#include <array>
#include <random>
#include <iostream>
#include <cmath>
#include <chrono>

namespace lve {

    // Static instance pointer for GLFW callbacks
    FirstAppRayTracing* FirstAppRayTracing::instance = nullptr;

    // Random utility class for scene generation
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

        // Ground sphere
        accelerationStructure->addSphereMesh(
            glm::vec3(0.0f, -1000.0f, 0.0f),
            glm::vec3(0.5f, 0.5f, 0.5f),
            1000.0f,
            0.0f, 0.0f,
            64, 32
        );

        int sphereCount = 0;

        // Random small spheres
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

        // Three big spheres
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
            std::cout << "Camera reset to initial position" << std::endl;
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

        createStorageImage();
        createDescriptorPool();
        createDescriptorSets();
        createCommandBuffers();
    }

    FirstAppRayTracing::~FirstAppRayTracing() {
        vkDestroyImageView(lveDevice.device(), storageImageView, nullptr);
        vkDestroyImage(lveDevice.device(), storageImage, nullptr);
        vkFreeMemory(lveDevice.device(), storageImageMemory, nullptr);
        vkDestroyDescriptorPool(lveDevice.device(), descriptorPool, nullptr);

        vkFreeCommandBuffers(
            lveDevice.device(),
            lveDevice.getCommandPool(),
            static_cast<uint32_t>(commandBuffers.size()),
            commandBuffers.data()
        );

        instance = nullptr;
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

    void FirstAppRayTracing::createStorageImage() {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = lveSwapChain.width();
        imageInfo.extent.height = lveSwapChain.height();
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        lveDevice.createImageWithInfo(
            imageInfo,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            storageImage,
            storageImageMemory
        );

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = storageImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(lveDevice.device(), &viewInfo, nullptr, &storageImageView) != VK_SUCCESS) {
            throw std::runtime_error("failed to create storage image view!");
        }

        VkCommandBuffer commandBuffer = lveDevice.beginSingleTimeCommands();

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = storageImage;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );

        lveDevice.endSingleTimeCommands(commandBuffer);
    }

    void FirstAppRayTracing::createDescriptorPool() {
        // UBO 삭제됨 - 3개만!
        VkDescriptorPoolSize poolSizes[] = {
            {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
        };

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 3;
        poolInfo.pPoolSizes = poolSizes;
        poolInfo.maxSets = 1;

        if (vkCreateDescriptorPool(lveDevice.device(), &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor pool!");
        }
    }

    void FirstAppRayTracing::createDescriptorSets() {
        VkDescriptorSetLayout layout = rayTracingPipeline->getDescriptorSetLayout();

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &layout;

        if (vkAllocateDescriptorSets(lveDevice.device(), &allocInfo, &descriptorSet) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate descriptor sets!");
        }

        // Binding 0: TLAS
        VkAccelerationStructureKHR tlas = accelerationStructure->getTLAS();
        VkWriteDescriptorSetAccelerationStructureKHR asInfo{};
        asInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
        asInfo.accelerationStructureCount = 1;
        asInfo.pAccelerationStructures = &tlas;

        VkWriteDescriptorSet asWrite{};
        asWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        asWrite.dstSet = descriptorSet;
        asWrite.dstBinding = 0;
        asWrite.descriptorCount = 1;
        asWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        asWrite.pNext = &asInfo;

        // Binding 1: Storage Image
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageView = storageImageView;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        VkWriteDescriptorSet imageWrite{};
        imageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        imageWrite.dstSet = descriptorSet;
        imageWrite.dstBinding = 1;
        imageWrite.descriptorCount = 1;
        imageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        imageWrite.pImageInfo = &imageInfo;

        // Binding 2: Sphere Info Buffer
        VkDescriptorBufferInfo sphereBufferInfo{};
        sphereBufferInfo.buffer = accelerationStructure->getSphereInfoBuffer();
        sphereBufferInfo.offset = 0;
        sphereBufferInfo.range = VK_WHOLE_SIZE;

        VkWriteDescriptorSet sphereWrite{};
        sphereWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        sphereWrite.dstSet = descriptorSet;
        sphereWrite.dstBinding = 2;
        sphereWrite.descriptorCount = 1;
        sphereWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        sphereWrite.pBufferInfo = &sphereBufferInfo;

        // Binding 3 (UBO) 삭제됨!
        VkWriteDescriptorSet writes[] = { asWrite, imageWrite, sphereWrite };
        vkUpdateDescriptorSets(lveDevice.device(), 3, writes, 0, nullptr);
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
        // 미리 기록하지 않음! drawFrame에서 매 프레임 기록
    }

    void FirstAppRayTracing::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rayTracingPipeline->getPipeline());
        vkCmdBindDescriptorSets(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
            rayTracingPipeline->getPipelineLayout(),
            0, 1, &descriptorSet, 0, nullptr
        );

        // Push Constants로 카메라 데이터 전송!
        CameraPushConstants pushConstants{};
        pushConstants.position = cameraPos;
        pushConstants.forward = cameraFront;
        pushConstants.right = cameraRight;
        pushConstants.up = cameraUp;
        pushConstants.vfov = vfov;
        pushConstants.defocus_angle = defocusAngle;
        pushConstants.focus_dist = focusDist;

        vkCmdPushConstants(
            commandBuffer,
            rayTracingPipeline->getPipelineLayout(),
            VK_SHADER_STAGE_RAYGEN_BIT_KHR,
            0,
            sizeof(CameraPushConstants),
            &pushConstants
        );

        VkStridedDeviceAddressRegionKHR raygenRegion = rayTracingPipeline->getRaygenRegion();
        VkStridedDeviceAddressRegionKHR missRegion = rayTracingPipeline->getMissRegion();
        VkStridedDeviceAddressRegionKHR hitRegion = rayTracingPipeline->getHitRegion();
        VkStridedDeviceAddressRegionKHR callableRegion = rayTracingPipeline->getCallableRegion();

        vkCmdTraceRaysKHR(
            commandBuffer,
            &raygenRegion,
            &missRegion,
            &hitRegion,
            &callableRegion,
            lveSwapChain.width(),
            lveSwapChain.height(),
            1
        );

        // Storage image → Transfer src
        VkImageMemoryBarrier barrier1{};
        barrier1.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier1.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier1.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier1.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier1.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier1.image = storageImage;
        barrier1.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier1.subresourceRange.baseMipLevel = 0;
        barrier1.subresourceRange.levelCount = 1;
        barrier1.subresourceRange.baseArrayLayer = 0;
        barrier1.subresourceRange.layerCount = 1;
        barrier1.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier1.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
            VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier1);

        VkImage swapChainImage = lveSwapChain.getSwapChainImage(imageIndex);

        // Swap chain image → Transfer dst
        VkImageMemoryBarrier barrier2{};
        barrier2.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier2.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier2.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier2.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier2.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier2.image = swapChainImage;
        barrier2.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier2.subresourceRange.baseMipLevel = 0;
        barrier2.subresourceRange.levelCount = 1;
        barrier2.subresourceRange.baseArrayLayer = 0;
        barrier2.subresourceRange.layerCount = 1;
        barrier2.srcAccessMask = 0;
        barrier2.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier2);

        // Copy
        VkImageCopy copyRegion{};
        copyRegion.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        copyRegion.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        copyRegion.extent = { lveSwapChain.width(), lveSwapChain.height(), 1 };

        vkCmdCopyImage(commandBuffer, storageImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            swapChainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        // Swap chain image → Present
        VkImageMemoryBarrier barrier3{};
        barrier3.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier3.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier3.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrier3.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier3.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier3.image = swapChainImage;
        barrier3.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier3.subresourceRange.baseMipLevel = 0;
        barrier3.subresourceRange.levelCount = 1;
        barrier3.subresourceRange.baseArrayLayer = 0;
        barrier3.subresourceRange.layerCount = 1;
        barrier3.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier3.dstAccessMask = 0;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier3);

        // Storage image → General (다음 프레임용)
        VkImageMemoryBarrier barrier4{};
        barrier4.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier4.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier4.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier4.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier4.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier4.image = storageImage;
        barrier4.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier4.subresourceRange.baseMipLevel = 0;
        barrier4.subresourceRange.levelCount = 1;
        barrier4.subresourceRange.baseArrayLayer = 0;
        barrier4.subresourceRange.layerCount = 1;
        barrier4.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier4.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 0, nullptr, 0, nullptr, 1, &barrier4);

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

        // 매 프레임 Command Buffer 기록!
        vkResetCommandBuffer(commandBuffers[imageIndex], 0);
        recordCommandBuffer(commandBuffers[imageIndex], imageIndex);

        result = lveSwapChain.submitCommandBuffers(&commandBuffers[imageIndex], &imageIndex);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image!");
        }
        vkQueueWaitIdle(lveDevice.presentQueue());
    }

} // namespace lve