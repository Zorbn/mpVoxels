#pragma once

#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

std::vector<glm::vec3> centeredSquareVertices = {
    glm::vec3(-0.5f, -0.5f, 0.5f),
    glm::vec3(-0.5, 0.5f, 0.5f),
    glm::vec3(0.5f, 0.5f, 0.5f),
    glm::vec3(0.5f, -0.5f, 0.5f),
};

std::vector<uint16_t> centeredSquareIndices = {0, 2, 1, 0, 3, 2};