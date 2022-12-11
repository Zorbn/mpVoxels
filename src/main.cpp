#include <random>
#include <thread>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vkFrame/renderer.hpp>

#include <GLFW/glfw3.h>

#include "../deps/perlinNoise.hpp"
#include "../deps/tiny_obj_loader.h"

#include "renderTypes.hpp"
#include "cubeMesh.hpp"
#include "directions.hpp"
#include "world.hpp"
#include "physics.hpp"
#include "player.hpp"
#include "blockInteraction.hpp"
#include "primitiveMeshes.hpp"
#include "frustum.hpp"
#include "input.hpp"

constexpr int32_t chunkSize = 32;
constexpr int32_t mapSizeInChunks = 4;
constexpr int32_t chunkCount = mapSizeInChunks * mapSizeInChunks * mapSizeInChunks;
constexpr float fogMaxDistance = 64.0f;
constexpr float mouseSensitivity = 0.1f;

class App {
private:
    Pipeline pipeline;
    Pipeline transparentPipeline;
    Pipeline uiPipeline;
    Pipeline modelPipeline; // TODO: Make an easy way to create simple VertexData/PerspectiveUbo pipelines
    RenderPass renderPass;

    Image textureImage;
    VkImageView textureImageView;
    VkSampler textureSampler;
    Image modelTextureImage;
    VkImageView modelTextureImageView;
    VkSampler modelTextureSampler;
    Image uiTextureImage;
    VkImageView uiTextureImageView;
    VkSampler uiTextureSampler;

    UniformBuffer<UniformBufferData> ubo;
    UniformBuffer<UniformBufferData> uiUbo;
    UniformBuffer<FogUniformData> fogUbo;

    std::vector<VkClearValue> clearValues;

    GLFWwindow* window;
    float windowWidth, windowHeight;
    float currentTime = 0;

    Frustum frustum;
    World world;
    Player player;
    BlockInteraction blockInteraction;
    Model<VertexData, uint16_t, InstanceData> crosshair;
    Model<VertexData, uint32_t, InstanceData> model;

    Input input;

    bool updateWorld = true;
    std::thread worldUpdateThread;

public:
    App() : world(chunkSize, mapSizeInChunks) {}

    void loadObjData(const std::string& path, const std::string& file, std::vector<VertexData>& vertices,
        std::vector<uint32_t>& indices, std::vector<std::string>& textures) {

        tinyobj::ObjReaderConfig readerConfig;
        readerConfig.mtl_search_path = path;
        tinyobj::ObjReader reader;

        if (!reader.ParseFromFile(path + file, readerConfig)) {
            if (!reader.Error().empty()) {
                throw std::runtime_error(reader.Error());
            }

            throw std::runtime_error("Failed to load mtl/obj file!");
        }

        if (!reader.Warning().empty()) {
            std::cout << "TinyObjReader: " << reader.Warning();
        }

        auto& attrib = reader.GetAttrib();
        auto& shapes = reader.GetShapes();
        auto& materials = reader.GetMaterials();

        for (size_t s = 0; s < shapes.size(); s++) {
            size_t indexOff = 0;
            
            for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
                size_t faceVerts = static_cast<size_t>(shapes[s].mesh.num_face_vertices[f]);

                for (size_t v = 0; v < faceVerts; v++) {
                    tinyobj::index_t idx = shapes[s].mesh.indices[indexOff + v];
                    tinyobj::real_t vx = attrib.vertices[3 * static_cast<size_t>(idx.vertex_index) + 0];
                    tinyobj::real_t vy = attrib.vertices[3 * static_cast<size_t>(idx.vertex_index) + 1];
                    tinyobj::real_t vz = attrib.vertices[3 * static_cast<size_t>(idx.vertex_index) + 2];

                    tinyobj::real_t tx = 0.0f;
                    tinyobj::real_t ty = 0.0f;

                    if (idx.texcoord_index >= 0) {
                        tx = attrib.texcoords[2 * static_cast<size_t>(idx.texcoord_index) + 0];
                        ty = attrib.texcoords[2 * static_cast<size_t>(idx.texcoord_index) + 1];
                    }

                    indices.push_back(vertices.size());

                    vertices.push_back(VertexData{
                        glm::vec3(vx, vy, vz),
                        glm::vec3(1.0f),
                        glm::vec3(tx, -ty, 0.0f),
                    });
                }

                indexOff += faceVerts;
            }
        }

        for (const auto& material : materials) {
            textures.push_back(path + material.diffuse_texname);
        }
    }

    void updateMousePos(float newMouseX, float newMouseY) {
        input.updateMousePos(newMouseX, newMouseY);
        glm::vec2 delta = input.getMouseDelta();
        player.updateRotation(delta.x, delta.y);
    }

    void updateKey(int32_t key, int32_t scancode, int32_t action, int32_t mods) {
        input.updateButton(key, action, mods);
    }

    void updateMouseButton(int32_t button, int32_t action, int32_t mods) {
        input.updateButton(button, action, mods);
    }

    void init(VulkanState& vulkanState, GLFWwindow* window, int32_t width, int32_t height) {
        windowWidth = static_cast<float>(width);
        windowHeight = static_cast<float>(height);
        this->window = window;
        glfwSetWindowUserPointer(window, this);

        if (glfwRawMouseMotionSupported())
            glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);

        // Hook into mouse movements to update the player's rotation.
        glfwSetCursorPosCallback(window, [](GLFWwindow* window, double mouseX, double mouseY) {
            App* app = reinterpret_cast<App*>(glfwGetWindowUserPointer(window));
            app->updateMousePos(static_cast<float>(mouseX), static_cast<float>(mouseY));
        });

        // Keep track of inputs and pass them to the input manager.
        glfwSetKeyCallback(window, [](GLFWwindow* window, int32_t key, int32_t scancode, int32_t action, int32_t mods) {
            App* app = reinterpret_cast<App*>(glfwGetWindowUserPointer(window));
            app->updateKey(key, scancode, action, mods);
        });

        glfwSetMouseButtonCallback(window, [](GLFWwindow* window, int32_t button, int32_t action, int32_t mods) {
            App* app = reinterpret_cast<App*>(glfwGetWindowUserPointer(window));
            app->updateMouseButton(button, action, mods);
        });

        input.setMouseSensitivity(mouseSensitivity);

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
        fogUbo.create(vulkanState.maxFramesInFlight, vulkanState.allocator);

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

                VkDescriptorSetLayoutBinding fogLayoutBinding{};
                fogLayoutBinding.binding = 2;
                fogLayoutBinding.descriptorCount = 1;
                fogLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                fogLayoutBinding.pImmutableSamplers = nullptr;
                fogLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

                bindings.push_back(uboLayoutBinding);
                bindings.push_back(samplerLayoutBinding);
                bindings.push_back(fogLayoutBinding);
            });
        pipeline.createDescriptorPool(
            vulkanState.maxFramesInFlight, vulkanState.device,
            [&](std::vector<VkDescriptorPoolSize> poolSizes) {
                poolSizes.resize(3);
                poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                poolSizes[0].descriptorCount = static_cast<uint32_t>(vulkanState.maxFramesInFlight);
                poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                poolSizes[1].descriptorCount = static_cast<uint32_t>(vulkanState.maxFramesInFlight);
                poolSizes[2].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                poolSizes[2].descriptorCount = static_cast<uint32_t>(vulkanState.maxFramesInFlight);
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

                VkDescriptorBufferInfo fogInfo{};
                fogInfo.buffer = fogUbo.getBuffer(i);
                fogInfo.offset = 0;
                fogInfo.range = fogUbo.getDataSize();

                descriptorWrites.resize(3);

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

                descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptorWrites[2].dstSet = descriptorSet;
                descriptorWrites[2].dstBinding = 2;
                descriptorWrites[2].dstArrayElement = 0;
                descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                descriptorWrites[2].descriptorCount = 1;
                descriptorWrites[2].pBufferInfo = &fogInfo;

                vkUpdateDescriptorSets(vulkanState.device,
                                       static_cast<uint32_t>(descriptorWrites.size()),
                                       descriptorWrites.data(), 0, nullptr);
            });
        pipeline.create<VertexData, InstanceData>(
            "res/cubesShader.vert.spv", "res/cubesShader.frag.spv", vulkanState.device, renderPass, false);

        std::vector<VertexData> modelVertices;
        std::vector<uint32_t> modelIndices;
        std::vector<std::string> modelTextures;
        loadObjData("res/testModel/", "testModel.obj", modelVertices, modelIndices, modelTextures);
        model = Model<VertexData, uint32_t, InstanceData>::fromVerticesAndIndices(modelVertices, modelIndices, 1, vulkanState.allocator,
            vulkanState.commands, vulkanState.graphicsQueue, vulkanState.device);
        std::vector<InstanceData> modelInstances;
        modelInstances.push_back(InstanceData{glm::vec3(80.0f)});
        model.updateInstances(modelInstances, vulkanState.commands, vulkanState.allocator, vulkanState.graphicsQueue, vulkanState.device);

        modelTextureImage = Image::createTexture(modelTextures[0], vulkanState.allocator,
                                                 vulkanState.commands, vulkanState.graphicsQueue,
                                                 vulkanState.device, false);
        modelTextureImageView = modelTextureImage.createTextureView(vulkanState.device);
        modelTextureSampler = modelTextureImage.createTextureSampler(
            vulkanState.physicalDevice, vulkanState.device, VK_FILTER_NEAREST, VK_FILTER_NEAREST);

        modelPipeline.createDescriptorSetLayout(
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

                VkDescriptorSetLayoutBinding fogLayoutBinding{};
                fogLayoutBinding.binding = 2;
                fogLayoutBinding.descriptorCount = 1;
                fogLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                fogLayoutBinding.pImmutableSamplers = nullptr;
                fogLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

                bindings.push_back(uboLayoutBinding);
                bindings.push_back(samplerLayoutBinding);
                bindings.push_back(fogLayoutBinding);
            });
        modelPipeline.createDescriptorPool(
            vulkanState.maxFramesInFlight, vulkanState.device,
            [&](std::vector<VkDescriptorPoolSize> poolSizes) {
                poolSizes.resize(3);
                poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                poolSizes[0].descriptorCount = static_cast<uint32_t>(vulkanState.maxFramesInFlight);
                poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                poolSizes[1].descriptorCount = static_cast<uint32_t>(vulkanState.maxFramesInFlight);
                poolSizes[2].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                poolSizes[2].descriptorCount = static_cast<uint32_t>(vulkanState.maxFramesInFlight);
            });
        modelPipeline.createDescriptorSets(
            vulkanState.maxFramesInFlight, vulkanState.device,
            [&](std::vector<VkWriteDescriptorSet>& descriptorWrites, VkDescriptorSet descriptorSet,
                uint32_t i) {
                VkDescriptorBufferInfo bufferInfo{};
                bufferInfo.buffer = ubo.getBuffer(i);
                bufferInfo.offset = 0;
                bufferInfo.range = ubo.getDataSize();

                VkDescriptorImageInfo imageInfo{};
                imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                imageInfo.imageView = modelTextureImageView;
                imageInfo.sampler = modelTextureSampler;

                VkDescriptorBufferInfo fogInfo{};
                fogInfo.buffer = fogUbo.getBuffer(i);
                fogInfo.offset = 0;
                fogInfo.range = fogUbo.getDataSize();

                descriptorWrites.resize(3);

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

                descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptorWrites[2].dstSet = descriptorSet;
                descriptorWrites[2].dstBinding = 2;
                descriptorWrites[2].dstArrayElement = 0;
                descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                descriptorWrites[2].descriptorCount = 1;
                descriptorWrites[2].pBufferInfo = &fogInfo;

                vkUpdateDescriptorSets(vulkanState.device,
                                       static_cast<uint32_t>(descriptorWrites.size()),
                                       descriptorWrites.data(), 0, nullptr);
            });
        modelPipeline.create<VertexData, InstanceData>(
            "res/modelShader.vert.spv", "res/modelShader.frag.spv", vulkanState.device, renderPass, false);

        transparentPipeline.createDescriptorSetLayout(
            vulkanState.device, [&](std::vector<VkDescriptorSetLayoutBinding>& bindings) {
                VkDescriptorSetLayoutBinding uboLayoutBinding{};
                uboLayoutBinding.binding = 0;
                uboLayoutBinding.descriptorCount = 1;
                uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                uboLayoutBinding.pImmutableSamplers = nullptr;
                uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

                VkDescriptorSetLayoutBinding fogLayoutBinding{};
                fogLayoutBinding.binding = 1;
                fogLayoutBinding.descriptorCount = 1;
                fogLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                fogLayoutBinding.pImmutableSamplers = nullptr;
                fogLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

                bindings.push_back(uboLayoutBinding);
                bindings.push_back(fogLayoutBinding);
            });
        transparentPipeline.createDescriptorPool(
            vulkanState.maxFramesInFlight, vulkanState.device,
            [&](std::vector<VkDescriptorPoolSize> poolSizes) {
                poolSizes.resize(2);
                poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                poolSizes[0].descriptorCount = static_cast<uint32_t>(vulkanState.maxFramesInFlight);
                poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                poolSizes[1].descriptorCount = static_cast<uint32_t>(vulkanState.maxFramesInFlight);
            });
        transparentPipeline.createDescriptorSets(
            vulkanState.maxFramesInFlight, vulkanState.device,
            [&](std::vector<VkWriteDescriptorSet>& descriptorWrites, VkDescriptorSet descriptorSet,
                uint32_t i) {
                VkDescriptorBufferInfo bufferInfo{};
                bufferInfo.buffer = ubo.getBuffer(i);
                bufferInfo.offset = 0;
                bufferInfo.range = ubo.getDataSize();

                VkDescriptorBufferInfo fogInfo{};
                fogInfo.buffer = fogUbo.getBuffer(i);
                fogInfo.offset = 0;
                fogInfo.range = fogUbo.getDataSize();

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
                descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                descriptorWrites[1].descriptorCount = 1;
                descriptorWrites[1].pBufferInfo = &fogInfo;

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

        fogUbo.update(FogUniformData{glm::vec4(0.0f, 0.0f, 0.0f, 1.0f), fogMaxDistance});

        worldUpdateThread = std::thread([&]() {
            while (updateWorld) {
                world.update(vulkanState.allocator, vulkanState.commands, vulkanState.graphicsQueue, vulkanState.device);
            }
        });
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

        world.upload(vulkanState.allocator, vulkanState.commands, vulkanState.graphicsQueue, vulkanState.device);

        player.updateMovement(input, world, deltaTime);
        player.updateInteraction(input, world, blockInteraction, deltaTime);

        blockInteraction.postUpdate(vulkanState.commands, vulkanState.allocator, vulkanState.graphicsQueue, vulkanState.device);

        input.update(window);
    }

    void render(VulkanState& vulkanState, VkCommandBuffer commandBuffer, uint32_t imageIndex,
                uint32_t currentFrame) {
        const VkExtent2D& extent = vulkanState.swapchain.getExtent();

        UniformBufferData uboData{};
        uboData.model = glm::mat4(1.0f);
        uboData.view = player.getViewMatrix();
        uboData.proj = glm::perspective(glm::radians(player.getFov()), extent.width / (float)extent.height, 0.1f, fogMaxDistance);
        uboData.proj[1][1] *= -1;

        ubo.update(uboData);

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

        modelPipeline.bind(commandBuffer, currentFrame);
        model.draw(commandBuffer);

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
        updateWorld = false;
        worldUpdateThread.join();

        pipeline.cleanup(vulkanState.device);
        modelPipeline.cleanup(vulkanState.device);
        transparentPipeline.cleanup(vulkanState.device);
        uiPipeline.cleanup(vulkanState.device);
        renderPass.cleanup(vulkanState.allocator, vulkanState.device);

        ubo.destroy(vulkanState.allocator);
        uiUbo.destroy(vulkanState.allocator);
        fogUbo.destroy(vulkanState.allocator);

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