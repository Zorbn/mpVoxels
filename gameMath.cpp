#include "gameMath.hpp"

int32_t hashVector(int32_t x, int32_t y, int32_t z) {
    return x * 73856093 ^ y * 19349663 ^ z * 83492791;
}

glm::ivec3 indexTo3d(int32_t i, int32_t size) {
    return glm::vec3 {
        i % size,
        (i / size) % size,
        i / (size * size)
    };
}

glm::ivec3 floorToInt(glm::vec3 v) {
    return static_cast<glm::ivec3>(glm::floor(v));
}