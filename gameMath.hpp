#pragma once

#include <cinttypes>

#include <glm/glm.hpp>

int32_t hashVector(int32_t x, int32_t y, int32_t z);
glm::ivec3 indexTo3d(int32_t i, int32_t size);