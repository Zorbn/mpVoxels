#pragma once

#include <cinttypes>
#include <vector>
#include <random>
#include <array>

#include <vkFrame/renderer.hpp>

#include "cubeMesh.hpp"
#include "blocks.hpp"
#include "renderTypes.hpp"
#include "directions.hpp"
#include "gameMath.hpp"
#include "deps/perlinNoise.hpp"

class World;

class Chunk {
public:
    Chunk(int32_t size, int32_t x, int32_t y, int32_t z);
    int32_t getBlockIndex(int32_t x, int32_t y, int32_t z);
    bool setBlock(int32_t x, int32_t y, int32_t z, Blocks type);
    Blocks getBlock(int32_t x, int32_t y, int32_t z);
    void update(World& world, VmaAllocator allocator, Commands& commands, VkQueue graphicsQueue, VkDevice device);
    void updateMesh(World& world, VmaAllocator allocator, Commands& commands, VkQueue graphicsQueue, VkDevice device);
    void generate(std::mt19937& rng, siv::BasicPerlinNoise<float>& noise);
    void draw(VkCommandBuffer commandBuffer);
    void destroy(VmaAllocator allocator);

    bool firstUpdate = true;
    bool needsUpdate = true;

private:
    bool shouldGenerateSolid(siv::BasicPerlinNoise<float>& noise, int32_t worldX, int32_t worldY, int32_t worldZ);

    int32_t chunkX, chunkY, chunkZ;
    int32_t size;

    std::vector<Blocks> data;
    std::vector<float> shadeNoise;

    std::vector<VertexData> vertices;
    std::vector<uint32_t> indices;

    std::vector<InstanceData> instances;
    Model<VertexData, uint32_t, InstanceData> model;
};