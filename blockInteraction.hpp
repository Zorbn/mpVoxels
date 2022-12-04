#pragma once

#include <cinttypes>
#include <unordered_map>
#include <utility>

#include <glm/glm.hpp>

#include "blocks.hpp"
#include "gameMath.hpp"
#include "world.hpp"
#include "renderTypes.hpp"
#include "cubeMesh.hpp"

const float blockBreakTime = 0.5f;

struct BreakingBlock {
    float progress;
    bool updatedThisFrame;
    Blocks block;
    glm::ivec3 pos;
};

class BlockInteraction {
public:
    void init(VmaAllocator allocator, Commands& commands, VkQueue graphicsQueue, VkDevice device);
    Blocks mineBlock(World& world, int32_t x, int32_t y, int32_t z, float deltaTime);
    void placeBlock(World& world, int32_t x, int32_t y, int32_t z, Blocks block);
    void preUpdate();
    void postUpdate(Commands& commands, VmaAllocator allocator, VkQueue graphicsQueue, VkDevice device);
    void draw(VkCommandBuffer commandBuffer);
    void destroy(VmaAllocator allocator);

private:
    std::unordered_map<int32_t, BreakingBlock> breakingBlocks;
    Model<TransparentVertexData, uint32_t, InstanceData> model;

    std::vector<TransparentVertexData> vertices;
    std::vector<uint32_t> indices;
    std::vector<InstanceData> instances;
};