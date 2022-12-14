#pragma once

#include <cmath>
#include <cinttypes>
#include <vector>
#include <random>
#include <array>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vkFrame/renderer.hpp>

#include "cubeMesh.hpp"
#include "blocks.hpp"
#include "renderTypes.hpp"
#include "directions.hpp"
#include "gameMath.hpp"
#include "vertexNeighbors.hpp"
#include "../deps/perlinNoise.hpp"

class World;

class Chunk {
public:
    Chunk(int32_t size, int32_t x, int32_t y, int32_t z);
    int32_t getBlockIndex(int32_t x, int32_t y, int32_t z);
    bool setBlock(World& world, int32_t x, int32_t y, int32_t z, Blocks type);
    Blocks getBlock(int32_t x, int32_t y, int32_t z);
    bool isBlockOccupied(int32_t x, int32_t y, int32_t z);
    void setLitColumn(World& world, int32_t x, int32_t topY, int32_t z, bool lit);
    void setLit(int32_t x, int32_t y, int32_t z, bool lit);
    bool getLit(int32_t x, int32_t y, int32_t z);
    bool update(World& world, VmaAllocator allocator, Commands& commands, VkQueue graphicsQueue, VkDevice device);
    bool upload(VmaAllocator allocator, Commands& commands, VkQueue graphicsQueue, VkDevice device);
    void updateMesh(World& world, VmaAllocator allocator, Commands& commands, VkQueue graphicsQueue, VkDevice device);
    void uploadMesh(VmaAllocator allocator, Commands& commands, VkQueue graphicsQueue, VkDevice device);
    void generate(World& world, std::mt19937& rng, siv::BasicPerlinNoise<float>& noise);
    void draw(VkCommandBuffer commandBuffer);
    glm::vec3 getPos();
    glm::vec3 getSize();
    void destroy(VmaAllocator allocator);
    int32_t calculateAoLevel(VertexNeighbors neighbors);
    VertexNeighbors checkVertexNeighbors(World& world, glm::ivec3 worldPos, glm::ivec3 vertexPos, int32_t direction);
    void orientLastFace();

    bool firstUpdate = true;
    bool needsUpdate = true;
    bool needsUpload = false;

private:
    bool shouldGenerateSolid(siv::BasicPerlinNoise<float>& noise, int32_t worldX, int32_t worldY, int32_t worldZ);

    int32_t chunkX, chunkY, chunkZ;
    int32_t size;

    std::vector<Blocks> data;
    std::vector<bool> lightMap;
    std::vector<float> shadeNoise;

    std::vector<VertexData> vertices;
    std::vector<uint32_t> indices;
    std::array<int32_t, 4> aoBuffer;

    std::vector<InstanceData> instances;
    Model<VertexData, uint32_t, InstanceData> model;
};