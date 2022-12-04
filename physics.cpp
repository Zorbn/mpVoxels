#include "physics.hpp"

bool isCollidingWithBlock(World& world, glm::vec3 pos, glm::vec3 size) {
    return getBlockCollision(world, pos, size).has_value();
}

std::optional<glm::ivec3> getBlockCollision(World& world, glm::vec3 pos, glm::vec3 size) {
    // The position supplied is the center of the bounding box.
    glm::vec3 pos0 = pos - size * 0.5f;
    glm::vec3 pos1 = pos + size * 0.5f;

    glm::ivec3 steps = floorToInt(size) + 1;
    glm::vec3 interp;

    // Interpolate between pos0 and pos1, checking each block between them.
    for (int32_t x = 0; x <= steps.x; x++) {
        interp.x = pos0.x + (static_cast<float>(x) / steps.x) * size.x;

        for (int32_t y = 0; y <= steps.y; y++) {
            interp.y = pos0.y + (static_cast<float>(y) / steps.y) * size.y;

            for (int32_t z = 0; z <= steps.z; z++) {
                interp.z = pos0.z + (static_cast<float>(z) / steps.z) * size.z;

                glm::ivec3 interpBlock = floorToInt(interp);

                if (world.isBlockOccupied(interpBlock.x, interpBlock.y, interpBlock.z)) {
                    return std::optional<glm::ivec3>{interpBlock};
                }
            }
        }
    }

    return std::nullopt;
}

bool overlapsBlock(float x, glm::vec3 pos, glm::vec3 size, glm::ivec3 blockPos) {
    glm::vec3 halfSize = size * 0.5f;

    return pos.x + halfSize.x > blockPos.x && x - halfSize.x < blockPos.x + 1 &&
        pos.y + blockPos.y > blockPos.y && pos.y - halfSize.y < blockPos.y + 1 &&
        pos.z + blockPos.z > blockPos.z && pos.z - halfSize.z < blockPos.z + 1;
}

bool isOnGround(World& world, glm::vec3 pos, glm::vec3 size) {
    float feetHitboxSize = 0.1f;
    glm::vec3 feetPos = pos;
    feetPos.y = pos.y - (size.y + feetHitboxSize) * 0.5f;
    glm::vec3 feetSize = size;
    feetSize.y = feetHitboxSize;
    return isCollidingWithBlock(world, feetPos, feetSize);
}

RaycastHit raycast(World& world, glm::vec3 start, glm::vec3 dir, float range) {
    glm::ivec3 tileDir = glm::sign(dir);
    glm::vec3 step = glm::abs(1.0f / dir);
    glm::vec3 initialStep;

    if (dir.x > 0) {
        initialStep.x = (glm::ceil(start.x) - start.x) * step.x;
    } else {
        initialStep.x = (start.x - glm::floor(start.x)) * step.x;
    }

    if (dir.y > 0) {
        initialStep.y = (glm::ceil(start.y) - start.y) * step.y;
    } else {
        initialStep.y = (start.y - glm::floor(start.y)) * step.y;
    }

    if (dir.z > 0) {
        initialStep.z = (glm::ceil(start.z) - start.z) * step.z;
    } else {
        initialStep.z = (start.z - glm::floor(start.z)) * step.z;
    }

    glm::vec3 distToNext = initialStep;
    glm::ivec3 blockPos = glm::floor(start);
    glm::ivec3 lastPos = blockPos;

    float lastDistToNext = 0.0f;

    Blocks hitBlock = world.getBlock(blockPos.x, blockPos.y, blockPos.z);
    while (hitBlock == Blocks::Air && lastDistToNext < range) {
        lastPos = blockPos;

        if (distToNext.x < distToNext.y && distToNext.x < distToNext.z) {
            lastDistToNext = distToNext.x;
            distToNext.x += step.x;
            blockPos.x += tileDir.x;
        } else if (distToNext.y < distToNext.x && distToNext.y < distToNext.z) {
            lastDistToNext = distToNext.y;
            distToNext.y += step.y;
            blockPos.y += tileDir.y;
        } else {
            lastDistToNext = distToNext.z;
            distToNext.z += step.z;
            blockPos.z += tileDir.z;
        }

        hitBlock = world.getBlock(blockPos.x, blockPos.y, blockPos.z);
    }

    return RaycastHit{
        hitBlock != Blocks::Air,
        hitBlock,
        lastDistToNext,
        blockPos,
        lastPos,
    };
}