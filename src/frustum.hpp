#pragma once

#include <array>
#include <cmath>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Frustum {
public:
    void calculate(glm::mat4 viewMatrix);
    bool shouldBeCulled(glm::vec3 pos, glm::vec3 size);

private:
    std::array<glm::vec4, 6> planes;
};