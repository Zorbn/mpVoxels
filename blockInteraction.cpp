#include "blockInteraction.hpp"

Blocks BlockInteraction::mineBlock(World& world, int32_t x, int32_t y, int32_t z, float deltaTime) {
    Blocks block = world.getBlock(x, y, z);
    int32_t key = hashVector(x, y, z);

    auto keyPos = breakingBlocks.find(key);
    if (keyPos == breakingBlocks.end()) {
        breakingBlocks.insert(std::make_pair(key, BreakingBlock{
            0.0f,
            true,
            block,
            glm::ivec3(x, y, z),
        }));
    } else {
        BreakingBlock& breakingBlock = breakingBlocks.at(key);
        breakingBlock.updatedThisFrame = true;

        if (block != breakingBlock.block) {
            breakingBlocks.erase(key);
            return breakingBlock.block;
        }
    }

    BreakingBlock& breakingBlock = breakingBlocks.at(key);
    breakingBlock.progress += deltaTime;

    if (breakingBlock.progress >= blockBreakTime) {
        world.setBlock(x, y, z, Blocks::Air);
        breakingBlocks.erase(key);
        return breakingBlock.block;
    }

    return Blocks::Air;
}

void BlockInteraction::placeBlock(World& world, int32_t x, int32_t y, int32_t z, Blocks block) {
    world.setBlock(x, y, z, block);
}

void BlockInteraction::preUpdate() {
    for (auto& [_, breakingBlock] : breakingBlocks) {
        breakingBlock.updatedThisFrame = false;
    }
}

void BlockInteraction::postUpdate() {
    for (auto& it = breakingBlocks.cbegin(); it != breakingBlocks.cend();) {
        if (!it->second.updatedThisFrame) {
            breakingBlocks.erase(it++);
        } else {
            it++;
        }
    }
}