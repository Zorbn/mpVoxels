#pragma once

#include <vector>
#include <random>
#include <optional>

#include <glm/glm.hpp>

#include "chunk.hpp"
#include "frustum.hpp"

class World {
public:
    World(int32_t chunkSize, int32_t mapSizeInChunks);
    Chunk& getChunk(int32_t x, int32_t y, int32_t z);
    void updateChunk(int32_t x, int32_t y, int32_t z);
    void setBlock(int32_t x, int32_t y, int32_t z, Blocks block);
    Blocks getBlock(int32_t x, int32_t y, int32_t z);
    bool getLit(int32_t x, int32_t y, int32_t z);
    bool isBlockOccupied(int32_t x, int32_t y, int32_t z);
    bool isBlockSupported(int32_t x, int32_t y, int32_t z);
    std::optional<glm::vec3> getSpawnPos(int32_t spawnChunkX, int32_t spawnChunkY, int32_t spawnChunkZ, bool force);
    void generate(std::mt19937& rng, siv::BasicPerlinNoise<float>& noise);
    void draw(Frustum& frustum, VkCommandBuffer commandBuffer);
    void destroy(VmaAllocator allocator);
    void update(VmaAllocator allocator, Commands& commands, VkQueue graphicsQueue, VkDevice device);

private:
    int32_t chunkSize;
    int32_t mapSizeInChunks;
    int32_t mapSize;
    std::vector<Chunk> chunks;
};