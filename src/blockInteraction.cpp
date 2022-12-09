#include "blockInteraction.hpp"

constexpr float blockModelPadding = 0.0025f;
constexpr float blockModelScale = 1.0f + blockModelPadding * 2.0f;

void BlockInteraction::init(VmaAllocator allocator, Commands& commands, VkQueue graphicsQueue, VkDevice device) {
    model = Model<TransparentVertexData, uint32_t, InstanceData>::create(1, allocator, commands, graphicsQueue, device);
    instances.push_back(InstanceData{glm::vec3(0.0f, 0.0f, 0.0f)});
    model.updateInstances(instances, commands, allocator, graphicsQueue, device);
}

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

void BlockInteraction::postUpdate(Commands& commands, VmaAllocator allocator, VkQueue graphicsQueue, VkDevice device) {
    vertices.clear();
    indices.clear();

    for (auto it = breakingBlocks.cbegin(); it != breakingBlocks.cend();) {
        if (!it->second.updatedThisFrame) {
            breakingBlocks.erase(it++);
            continue;
        }

        int32_t x = it->second.pos.x;
        int32_t y = it->second.pos.y;
        int32_t z = it->second.pos.z;

        for (int32_t face = 0; face < 6; face++) {
            size_t vertexCount = vertices.size();
            for (uint32_t index : cubeIndices[face]) {
                indices.push_back(index + static_cast<uint32_t>(vertexCount));
            }

            for (size_t i = 0; i < 4; i++) {
                glm::vec3 vertex = cubeVertices[face][i];
                glm::vec2 uv = cubeUvs[face][i];

                vertices.push_back(TransparentVertexData {
                    vertex * blockModelScale + glm::vec3(x, y, z) - blockModelPadding,
                    glm::vec4(0.0f, 0.0f, 0.0f, it->second.progress),
                });
            }
        }

        it++;
    }

    model.update(vertices, indices, commands, allocator, graphicsQueue, device);
}

void BlockInteraction::draw(VkCommandBuffer commandBuffer) {
    model.draw(commandBuffer);
}

void BlockInteraction::destroy(VmaAllocator allocator) {
    model.destroy(allocator);
}