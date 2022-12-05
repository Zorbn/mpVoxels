#pragma once

#include "GLFW/glfw3.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "world.hpp"
#include "physics.hpp"
#include "blockInteraction.hpp"

class Player {
public:
    void updateRotation(float dx, float dy);
    glm::mat4 Player::getViewMatrix();
    bool tryStepUp(World& world, glm::vec3 targetPos, glm::ivec3 hitBlock, bool isGrounded);
    void updateMovement(GLFWwindow* window, World& world, float deltaTime);
    void updateInteraction(GLFWwindow* window, World& world, BlockInteraction& blockInteraction, float deltaTime);
    void increaseViewInterp();
    void updateViewInterp(float deltaTime);
    void updateViewPos();
    void setPos(glm::vec3 newPos);

private:
    float speed = 5.0f;
    float jumpForce = 9.0f;
    float yVelocity = 0.0f;
    float viewHeightOffset = 1.0f;
    float viewHeightInterp = 0.0f;
    float viewHeightInterpSpeed = 8.0f;
    float range = 10.0f;
    glm::vec3 pos;
    glm::vec3 viewPos = pos;
    glm::vec3 size{0.8f, 2.8f, 0.8f};
    glm::vec3 forwardDir;
    glm::vec3 rightDir;
    float rotationX = 0, rotationY = 0;
};