#include <random>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vkFrame/renderer.hpp>

#include "GLFW/glfw3.h"

#include "deps/perlinNoise.hpp"

#include "renderTypes.hpp"
#include "cubeMesh.hpp"
#include "directions.hpp"
#include "world.hpp"
#include "physics.hpp"

class App {
private:
    Pipeline pipeline;
    RenderPass renderPass;

    Image textureImage;
    VkImageView textureImageView;
    VkSampler textureSampler;

    UniformBuffer<UniformBufferData> ubo;

    std::vector<VkClearValue> clearValues;

    GLFWwindow* window;
    float currentTime = 0;
    float mouseX = 0, mouseY = 0;

    float playerSpeed = 3.0f;
    glm::vec3 playerPos{-5.0f, -5.0f, -5.0f};
    glm::vec3 playerSize{0.8f, 0.8f, 0.8f};
    glm::vec3 playerForwardDir;
    glm::vec3 playerRightDir;
    float mouseSensitivity = 0.05f;
    float playerRotationX = 0, playerRotationY = 0;

    World world;

public:
    App() : world(32, 4) {}

    void updateMousePos(float newMouseX, float newMouseY, bool firstUpdate) {
        if (!firstUpdate) {
            float dx = (newMouseY - mouseY) * mouseSensitivity;
            float dy = (newMouseX - mouseX) * mouseSensitivity;

            playerRotationX += dx;
            playerRotationY -= dy;

            if (playerRotationX < -89.0f)
                playerRotationX = -89.0f;

            if (playerRotationX > 89.0f)
                playerRotationX = 89.0f;
        }

        mouseX = newMouseX;
        mouseY = newMouseY;

        glm::mat4 lookMat = glm::rotate(glm::mat4(1.0f), glm::radians(playerRotationY), glm::vec3(0.0f, 1.0f, 0.0f)) *
                            glm::rotate(glm::mat4(1.0f), glm::radians(playerRotationX), glm::vec3(1.0f, 0.0f, 0.0f));
        glm::vec4 forwardVec = lookMat * glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
        playerForwardDir = glm::vec3(forwardVec.x, forwardVec.y, forwardVec.z);
        glm::vec4 rightVec = lookMat * glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
        playerRightDir = glm::vec3(rightVec.x, rightVec.y, rightVec.z);
    }

    void init(VulkanState& vulkanState, GLFWwindow* window, int32_t width, int32_t height) {
        this->window = window;
        glfwSetWindowUserPointer(window, this);
        glfwSetInputMode(window, GLFW_STICKY_KEYS, true);

        if (glfwRawMouseMotionSupported())
            glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);

        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        // First mouse update to initialize the player's rotation.
        double initialMouseX, initialMouseY;
        glfwGetCursorPos(window, &initialMouseX, &initialMouseY);
        updateMousePos(static_cast<float>(initialMouseX), static_cast<float>(initialMouseY), true);

        // Hook into subsequent mouse movements to update the rotation.
        glfwSetCursorPosCallback(window, [](GLFWwindow* window, double mouseX, double mouseY) {
            App* app = reinterpret_cast<App*>(glfwGetWindowUserPointer(window));
            app->updateMousePos(static_cast<float>(mouseX), static_cast<float>(mouseY), false);
        });

        vulkanState.swapchain.create(vulkanState.device, vulkanState.physicalDevice,
                                     vulkanState.surface, width, height);

        vulkanState.commands.createPool(vulkanState.physicalDevice, vulkanState.device,
                                        vulkanState.surface);
        vulkanState.commands.createBuffers(vulkanState.device, vulkanState.maxFramesInFlight);

        textureImage = Image::createTextureArray("res/cubesImg.png", vulkanState.allocator,
                                                 vulkanState.commands, vulkanState.graphicsQueue,
                                                 vulkanState.device, true, 16, 16, 4);
        textureImageView = textureImage.createTextureView(vulkanState.device);
        textureSampler = textureImage.createTextureSampler(
            vulkanState.physicalDevice, vulkanState.device, VK_FILTER_NEAREST, VK_FILTER_NEAREST);

        const VkExtent2D& extent = vulkanState.swapchain.getExtent();
        ubo.create(vulkanState.maxFramesInFlight, vulkanState.allocator);

        renderPass.create(vulkanState.physicalDevice, vulkanState.device, vulkanState.allocator,
                          vulkanState.swapchain, true, false);

        pipeline.createDescriptorSetLayout(
            vulkanState.device, [&](std::vector<VkDescriptorSetLayoutBinding>& bindings) {
                VkDescriptorSetLayoutBinding uboLayoutBinding{};
                uboLayoutBinding.binding = 0;
                uboLayoutBinding.descriptorCount = 1;
                uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                uboLayoutBinding.pImmutableSamplers = nullptr;
                uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

                VkDescriptorSetLayoutBinding samplerLayoutBinding{};
                samplerLayoutBinding.binding = 1;
                samplerLayoutBinding.descriptorCount = 1;
                samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                samplerLayoutBinding.pImmutableSamplers = nullptr;
                samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

                bindings.push_back(uboLayoutBinding);
                bindings.push_back(samplerLayoutBinding);
            });
        pipeline.createDescriptorPool(
            vulkanState.maxFramesInFlight, vulkanState.device,
            [&](std::vector<VkDescriptorPoolSize> poolSizes) {
                poolSizes.resize(2);
                poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                poolSizes[0].descriptorCount = static_cast<uint32_t>(vulkanState.maxFramesInFlight);
                poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                poolSizes[1].descriptorCount = static_cast<uint32_t>(vulkanState.maxFramesInFlight);
            });
        pipeline.createDescriptorSets(
            vulkanState.maxFramesInFlight, vulkanState.device,
            [&](std::vector<VkWriteDescriptorSet>& descriptorWrites, VkDescriptorSet descriptorSet,
                uint32_t i) {
                VkDescriptorBufferInfo bufferInfo{};
                bufferInfo.buffer = ubo.getBuffer(i);
                bufferInfo.offset = 0;
                bufferInfo.range = ubo.getDataSize();

                VkDescriptorImageInfo imageInfo{};
                imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                imageInfo.imageView = textureImageView;
                imageInfo.sampler = textureSampler;

                descriptorWrites.resize(2);

                descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptorWrites[0].dstSet = descriptorSet;
                descriptorWrites[0].dstBinding = 0;
                descriptorWrites[0].dstArrayElement = 0;
                descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                descriptorWrites[0].descriptorCount = 1;
                descriptorWrites[0].pBufferInfo = &bufferInfo;

                descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptorWrites[1].dstSet = descriptorSet;
                descriptorWrites[1].dstBinding = 1;
                descriptorWrites[1].dstArrayElement = 0;
                descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                descriptorWrites[1].descriptorCount = 1;
                descriptorWrites[1].pImageInfo = &imageInfo;

                vkUpdateDescriptorSets(vulkanState.device,
                                       static_cast<uint32_t>(descriptorWrites.size()),
                                       descriptorWrites.data(), 0, nullptr);
            });
        pipeline.create<VertexData, InstanceData>(
            "res/cubesShader.vert.spv", "res/cubesShader.frag.spv", vulkanState.device, renderPass);

        clearValues.resize(2);
        clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        clearValues[1].depthStencil = {1.0f, 0};

        const int32_t seed = 123;

        std::mt19937 rng{seed};
        siv::BasicPerlinNoise<float> noise{seed};

        world.generate(rng, noise);
    }

    void update(VulkanState& vulkanState) {
        float newTime = static_cast<float>(glfwGetTime());
        float deltaTime = newTime - currentTime;
        currentTime = newTime;

        world.update(vulkanState.allocator, vulkanState.commands, vulkanState.graphicsQueue, vulkanState.device);

        glm::vec2 horizontalForwardDir = glm::normalize(glm::vec2(playerForwardDir.x, playerForwardDir.z));
        glm::vec2 horizontalRightDir = glm::normalize(glm::vec2(playerRightDir.x, playerRightDir.z));

        glm::vec3 newPlayerPos = playerPos;

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
            newPlayerPos.x += horizontalForwardDir.x * playerSpeed * deltaTime;
            newPlayerPos.z += horizontalForwardDir.y * playerSpeed * deltaTime;
        }

        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
            newPlayerPos.x -= horizontalForwardDir.x * playerSpeed * deltaTime;
            newPlayerPos.z -= horizontalForwardDir.y * playerSpeed * deltaTime;
        }

        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
            newPlayerPos.x -= horizontalRightDir.x * playerSpeed * deltaTime;
            newPlayerPos.z -= horizontalRightDir.y * playerSpeed * deltaTime;
        }

        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
            newPlayerPos.x += horizontalRightDir.x * playerSpeed * deltaTime;
            newPlayerPos.z += horizontalRightDir.y * playerSpeed * deltaTime;
        }

        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
            newPlayerPos.y += playerSpeed * deltaTime;
        }

        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
            newPlayerPos.y -= playerSpeed * deltaTime;
        }

        if (glfwGetKey(window, GLFW_KEY_N) != GLFW_PRESS) {
            if (isCollidingWithBlock(world, glm::vec3{newPlayerPos.x, playerPos.y, playerPos.z}, playerSize)) {
                newPlayerPos.x = playerPos.x;
            }

            if (isCollidingWithBlock(world, glm::vec3{playerPos.x, newPlayerPos.y, playerPos.z}, playerSize)) {
                newPlayerPos.y = playerPos.y;
            }

            if (isCollidingWithBlock(world, glm::vec3{playerPos.x, playerPos.y, newPlayerPos.z}, playerSize)) {
                newPlayerPos.z = playerPos.z;
            }
        }

        playerPos = newPlayerPos;
    }

    void render(VulkanState& vulkanState, VkCommandBuffer commandBuffer, uint32_t imageIndex,
                uint32_t currentFrame) {
        const VkExtent2D& extent = vulkanState.swapchain.getExtent();

        UniformBufferData uboData{};
        uboData.model = glm::mat4(1.0f);
        uboData.view = glm::lookAt(playerPos, playerPos + playerForwardDir, glm::vec3(0.0f, 1.0f, 0.0f));
        uboData.proj = glm::perspective(glm::radians(90.0f), extent.width / (float)extent.height, 0.1f, 128.0f);
        uboData.proj[1][1] *= -1;

        ubo.update(uboData);

        vulkanState.commands.beginBuffer(currentFrame);

        renderPass.begin(imageIndex, commandBuffer, extent, clearValues);
        pipeline.bind(commandBuffer, currentFrame);

        world.draw(commandBuffer);

        renderPass.end(commandBuffer);

        vulkanState.commands.endBuffer(currentFrame);
    }

    void resize(VulkanState& vulkanState, int32_t width, int32_t height) {
        renderPass.recreate(vulkanState.physicalDevice, vulkanState.device, vulkanState.allocator,
                            vulkanState.swapchain);
    }

    void cleanup(VulkanState& vulkanState) {
        pipeline.cleanup(vulkanState.device);
        renderPass.cleanup(vulkanState.allocator, vulkanState.device);

        ubo.destroy(vulkanState.allocator);

        vkDestroySampler(vulkanState.device, textureSampler, nullptr);
        vkDestroyImageView(vulkanState.device, textureImageView, nullptr);
        textureImage.destroy(vulkanState.allocator);

        world.destroy(vulkanState.allocator);
    }

    int run() {
        Renderer renderer;

        std::function<void(VulkanState&, GLFWwindow*, int32_t, int32_t)> initCallback =
            [&](VulkanState& vulkanState, GLFWwindow* window, int32_t width, int32_t height) {
                this->init(vulkanState, window, width, height);
            };

        std::function<void(VulkanState&)> updateCallback = [&](VulkanState vulkanState) {
            this->update(vulkanState);
        };

        std::function<void(VulkanState&, VkCommandBuffer, uint32_t, uint32_t)> renderCallback =
            [&](VulkanState& vulkanState, VkCommandBuffer commandBuffer, uint32_t imageIndex,
                uint32_t currentFrame) {
                this->render(vulkanState, commandBuffer, imageIndex, currentFrame);
            };

        std::function<void(VulkanState&, int32_t, int32_t)> resizeCallback =
            [&](VulkanState& vulkanState, int32_t width, int32_t height) {
                this->resize(vulkanState, width, height);
            };

        std::function<void(VulkanState&)> cleanupCallback = [&](VulkanState& vulkanState) {
            this->cleanup(vulkanState);
        };

        try {
            renderer.run("mpVoxels", 640, 480, 2, initCallback, updateCallback, renderCallback,
                         resizeCallback, cleanupCallback);
        } catch (const std::exception& e) {
            std::cerr << e.what() << std::endl;
            return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
    }
};

int main() {
    App app;
    return app.run();
}