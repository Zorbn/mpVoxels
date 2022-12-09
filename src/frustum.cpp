#include "frustum.hpp"

void Frustum::calculate(glm::mat4 viewProjMatrix) {
    // Left plane.
    planes[0].x = viewProjMatrix[0].w + viewProjMatrix[0].x;
    planes[0].y = viewProjMatrix[1].w + viewProjMatrix[1].x;
    planes[0].z = viewProjMatrix[2].w + viewProjMatrix[2].x;
    planes[0].w = viewProjMatrix[3].w + viewProjMatrix[3].x;

    // Right plane.
    planes[1].x = viewProjMatrix[0].w - viewProjMatrix[0].x;
    planes[1].y = viewProjMatrix[1].w - viewProjMatrix[1].x;
    planes[1].z = viewProjMatrix[2].w - viewProjMatrix[2].x;
    planes[1].w = viewProjMatrix[3].w - viewProjMatrix[3].x;

    // Top plane.
    planes[2].x = viewProjMatrix[0].w - viewProjMatrix[0].y;
    planes[2].y = viewProjMatrix[1].w - viewProjMatrix[1].y;
    planes[2].z = viewProjMatrix[2].w - viewProjMatrix[2].y;
    planes[2].w = viewProjMatrix[3].w - viewProjMatrix[3].y;

    // Bottom plane.
    planes[3].x = viewProjMatrix[0].w + viewProjMatrix[0].y;
    planes[3].y = viewProjMatrix[1].w + viewProjMatrix[1].y;
    planes[3].z = viewProjMatrix[2].w + viewProjMatrix[2].y;
    planes[3].w = viewProjMatrix[3].w + viewProjMatrix[3].y;

    // Back plane.
    planes[4].x = viewProjMatrix[0].w + viewProjMatrix[0].z;
    planes[4].y = viewProjMatrix[1].w + viewProjMatrix[1].z;
    planes[4].z = viewProjMatrix[2].w + viewProjMatrix[2].z;
    planes[4].w = viewProjMatrix[3].w + viewProjMatrix[3].z;

    // Front plane.
    planes[5].x = viewProjMatrix[0].w - viewProjMatrix[0].z;
    planes[5].y = viewProjMatrix[1].w - viewProjMatrix[1].z;
    planes[5].z = viewProjMatrix[2].w - viewProjMatrix[2].z;
    planes[5].w = viewProjMatrix[3].w - viewProjMatrix[3].z;

    // Normalize the planes, the normals point inward.
    for (int32_t i = 0; i < 6; i++) {
        float length = std::sqrt(planes[i].x * planes[i].x + planes[i].y * planes[i].y + planes[i].z * planes[i].z);
        planes[i] /= length;
    }
}

bool Frustum::shouldBeCulled(glm::vec3 pos, glm::vec3 size) {
    for (int32_t i = 0; i < 6; i++) {
        glm::vec3 planeNormal = glm::vec3(planes[i].x, planes[i].y, planes[i].z);
        float planeConstant = planes[i].w;
        glm::vec3 axisVert;

        if (planes[i].x < 0.0f) {
            axisVert.x = pos.x;
        } else {
            axisVert.x = pos.x + size.x;
        }

        if (planes[i].y < 0.0f) {
            axisVert.y = pos.y;
        } else {
            axisVert.y = pos.y + size.y;
        }

        if (planes[i].z < 0.0f) {
            axisVert.z = pos.z;
        } else {
            axisVert.z = pos.z + size.z;
        }

        if (glm::dot(planeNormal, axisVert) + planeConstant < 0.0f) {
            return true;
        }
    }

    return false;
}