#pragma once

#include <cinttypes>
#include <array>

const std::array<std::array<int32_t, 3>, 6> directions = {{
    {0, 0, -1}, // Forward
    {0, 0, 1},  // Backward
    {1, 0, 0},  // Right
    {-1, 0, 0}, // Left
    {0, 1, 0},  // Up
    {0, -1, 0}, // Down
}};