#include <random>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vkFrame/renderer.hpp>

#include "GLFW/glfw3.h"

#include "../deps/perlinNoise.hpp"

#include "renderTypes.hpp"
#include "cubeMesh.hpp"
#include "directions.hpp"
#include "world.hpp"
#include "physics.hpp"
#include "player.hpp"
#include "blockInteraction.hpp"
#include "primitiveMeshes.hpp"
#include "frustum.hpp"

constexpr int32_t chunkSize = 32;
constexpr int32_t mapSizeInChunks = 4;
constexpr int32_t chunkCount = mapSizeInChunks * mapSizeInChunks * mapSizeInChunks;

class App {
private:
    Pipeline pipeline;
    Pipeline transparentPipeline;
    Pipeline uiPipeline;
    RenderPass renderPass;

    Image textureImage;
    VkImageView textureImageView;
    VkSampler textureSampler;
    Image uiTextureImage;
    VkImageView uiTextureImageView;
    VkSampler uiTextureSampler;

    UniformBuffer<UniformBufferData> ubo;
    UniformBuffer<UniformBufferData> uiUbo;

    std::vector<VkClearValue> clearValues;

    GLFWwindow* window;
    float windowWidth, windowHeight;
    float currentTime = 0;
    float mouseX = 0, mouseY = 0;

    float mouseSensitivity = 0.1f;

    Frustum frustum;
    World world;
    Player player;
    BlockInteraction blockInteraction;
    Model<VertexData, uint16_t, InstanceData> crosshair;

public:
    App() : world(chunkSize, mapSizeInChunks) {}

    void updateMousePos(float newMouseX, float newMouseY, bool firstUpdate) {
        float dx = 0.0f, dy = 0.0f;

        if (!firstUpdate) {
            dx = (newMouseY - mouseY) * mouseSensitivity;
            dy = (newMouseX - mouseX) * mouseSensitivity;
        }

        mouseX = newMouseX;
        mouseY = newMouseY;

        player.updateRotation(dx, dy);
    }

    void init(VulkanState& vulkanState, GLFWwindow* window, int32_t width, int32_t height) {
        windowWidth = static_cast<float>(width);
        windowHeight = static_cast<float>(height);
        this->window = window;
        glfwSetWindowUserPointer(window, this);

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

        uiTextureImage = Image::createTextureArray("res/uiImg.png", vulkanState.allocator,
                                                 vulkanState.commands, vulkanState.graphicsQueue,
                                                 vulkanState.device, false, 16, 16, 2);
        uiTextureImageView = uiTextureImage.createTextureView(vulkanState.device);
        uiTextureSampler = uiTextureImage.createTextureSampler(
            vulkanState.physicalDevice, vulkanState.device, VK_FILTER_NEAREST, VK_FILTER_NEAREST);

        const VkExtent2D& extent = vulkanState.swapchain.getExtent();
        ubo.create(vulkanState.maxFramesInFlight, vulkanState.allocator);
        uiUbo.create(vulkanState.maxFramesInFlight, vulkanState.allocator);

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
            "res/cubesShader.vert.spv", "res/cubesShader.frag.spv", vulkanState.device, renderPass, false);

        transparentPipeline.createDescriptorSetLayout(
            vulkanState.device, [&](std::vector<VkDescriptorSetLayoutBinding>& bindings) {
                VkDescriptorSetLayoutBinding uboLayoutBinding{};
                uboLayoutBinding.binding = 0;
                uboLayoutBinding.descriptorCount = 1;
                uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                uboLayoutBinding.pImmutableSamplers = nullptr;
                uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

                bindings.push_back(uboLayoutBinding);
            });
        transparentPipeline.createDescriptorPool(
            vulkanState.maxFramesInFlight, vulkanState.device,
            [&](std::vector<VkDescriptorPoolSize> poolSizes) {
                poolSizes.resize(1);
                poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                poolSizes[0].descriptorCount = static_cast<uint32_t>(vulkanState.maxFramesInFlight);
            });
        transparentPipeline.createDescriptorSets(
            vulkanState.maxFramesInFlight, vulkanState.device,
            [&](std::vector<VkWriteDescriptorSet>& descriptorWrites, VkDescriptorSet descriptorSet,
                uint32_t i) {
                VkDescriptorBufferInfo bufferInfo{};
                bufferInfo.buffer = ubo.getBuffer(i);
                bufferInfo.offset = 0;
                bufferInfo.range = ubo.getDataSize();

                descriptorWrites.resize(1);

                descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptorWrites[0].dstSet = descriptorSet;
                descriptorWrites[0].dstBinding = 0;
                descriptorWrites[0].dstArrayElement = 0;
                descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                descriptorWrites[0].descriptorCount = 1;
                descriptorWrites[0].pBufferInfo = &bufferInfo;

                vkUpdateDescriptorSets(vulkanState.device,
                                       static_cast<uint32_t>(descriptorWrites.size()),
                                       descriptorWrites.data(), 0, nullptr);
            });
        transparentPipeline.create<TransparentVertexData, InstanceData>(
            "res/transparentShader.vert.spv", "res/transparentShader.frag.spv", vulkanState.device, renderPass, true);

        uiPipeline.createDescriptorSetLayout(
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
        uiPipeline.createDescriptorPool(
            vulkanState.maxFramesInFlight, vulkanState.device,
            [&](std::vector<VkDescriptorPoolSize> poolSizes) {
                poolSizes.resize(2);
                poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                poolSizes[0].descriptorCount = static_cast<uint32_t>(vulkanState.maxFramesInFlight);
                poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                poolSizes[1].descriptorCount = static_cast<uint32_t>(vulkanState.maxFramesInFlight);
            });
        uiPipeline.createDescriptorSets(
            vulkanState.maxFramesInFlight, vulkanState.device,
            [&](std::vector<VkWriteDescriptorSet>& descriptorWrites, VkDescriptorSet descriptorSet,
                uint32_t i) {
                VkDescriptorBufferInfo bufferInfo{};
                bufferInfo.buffer = uiUbo.getBuffer(i);
                bufferInfo.offset = 0;
                bufferInfo.range = uiUbo.getDataSize();

                VkDescriptorImageInfo imageInfo{};
                imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                imageInfo.imageView = uiTextureImageView;
                imageInfo.sampler = uiTextureSampler;

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
        uiPipeline.create<VertexData, InstanceData>(
            "res/uiShader.vert.spv", "res/uiShader.frag.spv", vulkanState.device, renderPass, true);

        clearValues.resize(2);
        clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        clearValues[1].depthStencil = {1.0f, 0};

        const int32_t seed = 123;

        std::mt19937 rng{seed};
        siv::BasicPerlinNoise<float> noise{seed};

        world.generate(rng, noise);

        int32_t playerSpawnI = rng() % chunkCount;
        glm::ivec3 playerSpawnChunk = indexTo3d(playerSpawnI, mapSizeInChunks);
        glm::vec3 playerSpawnPos = world.getSpawnPos(playerSpawnChunk.x, playerSpawnChunk.y, playerSpawnChunk.z, true).value();
        player.setPos(playerSpawnPos);

        blockInteraction.init(vulkanState.allocator, vulkanState.commands, vulkanState.graphicsQueue, vulkanState.device);

        currentTime = static_cast<float>(glfwGetTime());

        std::vector<VertexData> crosshairVertices;
        size_t vertexCount = crosshairVertices.size();

        for (size_t i = 0; i < 4; i++) {
            glm::vec3 vertex = centeredSquareVertices[i];
            glm::vec2 uv = cubeUvs[1][i];

            crosshairVertices.push_back(VertexData {
                vertex * 16.0f,
                glm::vec3(1.0f, 1.0f, 1.0f),
                glm::vec3(uv.x, uv.y, 0.0f),
            });
        }

        crosshair = Model<VertexData, uint16_t, InstanceData>::fromVerticesAndIndices(crosshairVertices, centeredSquareIndices, 1,
            vulkanState.allocator, vulkanState.commands, vulkanState.graphicsQueue, vulkanState.device);
        std::vector<InstanceData> crosshairInstances;
        crosshairInstances.push_back(InstanceData{glm::vec3(0.0f)});
        crosshair.updateInstances(crosshairInstances, vulkanState.commands, vulkanState.allocator, vulkanState.graphicsQueue, vulkanState.device);
    }

    void update(VulkanState& vulkanState) {
        float newTime = static_cast<float>(glfwGetTime());
        float deltaTime = newTime - currentTime;
        currentTime = newTime;

        // Ignore outlier deltaTime values to prevent the simulation from moving too fast.
        if (deltaTime > 0.1f) {
            return;
        }

        blockInteraction.preUpdate();

        world.update(vulkanState.allocator, vulkanState.commands, vulkanState.graphicsQueue, vulkanState.device);

        player.updateMovement(window, world, deltaTime);
        player.updateInteraction(window, world, blockInteraction, deltaTime);

        blockInteraction.postUpdate(vulkanState.commands, vulkanState.allocator, vulkanState.graphicsQueue, vulkanState.device);
    }

    void render(VulkanState& vulkanState, VkCommandBuffer commandBuffer, uint32_t imageIndex,
                uint32_t currentFrame) {
        const VkExtent2D& extent = vulkanState.swapchain.getExtent();

        UniformBufferData uboData{};
        uboData.model = glm::mat4(1.0f);
        uboData.view = player.getViewMatrix();
        uboData.proj = glm::perspective(glm::radians(player.getFov()), extent.width / (float)extent.height, 0.1f, 128.0f);
        uboData.proj[1][1] *= -1;

        ubo.update(uboData);

        // uboData.proj = glm::perspective(glm::radians(15.0f), extent.width / (float)extent.height, 0.1f, 128.0f);
        // uboData.proj[1][1] *= -1;
        frustum.calculate(uboData.proj * uboData.view);

        uboData.proj = glm::ortho(-windowWidth * 0.5f, windowWidth * 0.5f, -windowHeight * 0.5f, windowHeight * 0.5f, -10.0f, 10.0f);
        uboData.proj[1][1] *= -1;

        uiUbo.update(uboData);

        vulkanState.commands.beginBuffer(currentFrame);

        renderPass.begin(imageIndex, commandBuffer, extent, clearValues);
        pipeline.bind(commandBuffer, currentFrame);

        world.draw(frustum, commandBuffer);

        transparentPipeline.bind(commandBuffer, currentFrame);

        blockInteraction.draw(commandBuffer);

        uiPipeline.bind(commandBuffer, currentFrame);

        crosshair.draw(commandBuffer);

        renderPass.end(commandBuffer);

        vulkanState.commands.endBuffer(currentFrame);
    }

    void resize(VulkanState& vulkanState, int32_t width, int32_t height) {
        windowWidth = static_cast<float>(width);
        windowHeight = static_cast<float>(height);

        renderPass.recreate(vulkanState.physicalDevice, vulkanState.device, vulkanState.allocator,
                            vulkanState.swapchain);
    }

    void cleanup(VulkanState& vulkanState) {
        pipeline.cleanup(vulkanState.device);
        transparentPipeline.cleanup(vulkanState.device);
        uiPipeline.cleanup(vulkanState.device);
        renderPass.cleanup(vulkanState.allocator, vulkanState.device);

        ubo.destroy(vulkanState.allocator);
        uiUbo.destroy(vulkanState.allocator);

        vkDestroySampler(vulkanState.device, textureSampler, nullptr);
        vkDestroyImageView(vulkanState.device, textureImageView, nullptr);
        textureImage.destroy(vulkanState.allocator);
        vkDestroySampler(vulkanState.device, uiTextureSampler, nullptr);
        vkDestroyImageView(vulkanState.device, uiTextureImageView, nullptr);
        uiTextureImage.destroy(vulkanState.allocator);

        world.destroy(vulkanState.allocator);
        blockInteraction.destroy(vulkanState.allocator);
        crosshair.destroy(vulkanState.allocator);
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