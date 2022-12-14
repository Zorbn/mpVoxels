#pragma once

#include <optional>

#include "world.hpp"
#include "gameMath.hpp"

const float gravity = 30.0f;

struct RaycastHit {
    bool hit;
    Blocks block;
    float distance;
    glm::ivec3 pos;
    glm::ivec3 lastPos;
};

bool isCollidingWithBlock(World& world, glm::vec3 pos, glm::vec3 size);
std::optional<glm::ivec3> getBlockCollision(World& world, glm::vec3 pos, glm::vec3 size);
bool overlapsBlock(float x, glm::vec3 pos, glm::vec3 size, glm::ivec3 blockPos);
bool isOnGround(World& world, glm::vec3 pos, glm::vec3 size);
RaycastHit raycast(World& world, glm::vec3 start, glm::vec3 dir, float range);