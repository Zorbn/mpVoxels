#pragma once

#include <cinttypes>
#include <unordered_map>
#include <utility>

#include <glm/glm.hpp>

#include "blocks.hpp"
#include "gameMath.hpp"
#include "world.hpp"

const float blockBreakTime = 0.5f;

struct BreakingBlock {
    float progress;
    bool updatedThisFrame;
    Blocks block;
    glm::ivec3 pos;
};

class BlockInteraction {
public:
    Blocks mineBlock(World& world, int32_t x, int32_t y, int32_t z, float deltaTime);
    void placeBlock(World& world, int32_t x, int32_t y, int32_t z, Blocks block);
    void preUpdate();
    void postUpdate();

private:
    std::unordered_map<int32_t, BreakingBlock> breakingBlocks;
};