#include "player.hpp"

void Player::updateRotation(float dx, float dy) {
    rotationX += dx;
    rotationY -= dy;

    if (rotationX < -89.0f)
        rotationX = -89.0f;

    if (rotationX > 89.0f)
        rotationX = 89.0f;

    glm::mat4 lookMat = glm::rotate(glm::mat4(1.0f), glm::radians(rotationY), glm::vec3(0.0f, 1.0f, 0.0f)) *
                        glm::rotate(glm::mat4(1.0f), glm::radians(rotationX), glm::vec3(1.0f, 0.0f, 0.0f));
    glm::vec4 forwardVec = lookMat * glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
    forwardDir = glm::vec3(forwardVec.x, forwardVec.y, forwardVec.z);
    glm::vec4 rightVec = lookMat * glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
    rightDir = glm::vec3(rightVec.x, rightVec.y, rightVec.z);
}

glm::mat4 Player::getViewMatrix() {
    return glm::lookAt(viewPos, viewPos + forwardDir, glm::vec3(0.0f, 1.0f, 0.0f));
}

void Player::updateInteraction(GLFWwindow* window, World& world, BlockInteraction& blockInteraction, float deltaTime) {
    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) {
        RaycastHit hit = raycast(world, viewPos, forwardDir, 10.0f);

        if (hit.hit) {
            blockInteraction.mineBlock(world, hit.pos.x, hit.pos.y, hit.pos.z, deltaTime);
        }
    }
}

void Player::updateMovement(GLFWwindow* window, World& world, float deltaTime) {
    bool isGrounded = isOnGround(world, pos, size);

    glm::vec2 horizontalForwardDir = glm::normalize(glm::vec2(forwardDir.x, forwardDir.z));
    glm::vec2 horizontalRightDir = glm::normalize(glm::vec2(rightDir.x, rightDir.z));

    glm::vec3 newPos = pos;

    // y is forward/backward movement, x is right/left movement.
    glm::vec2 moveDir{0.0f, 0.0f};

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        moveDir.y += 1.0f;
    }

    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        moveDir.y -= 1.0f;
    }

    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        moveDir.x -= 1.0f;
    }

    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        moveDir.x += 1.0f;
    }

    moveDir = glm::normalize(moveDir);

    newPos.x += moveDir.y * horizontalForwardDir.x * speed * deltaTime;
    newPos.z += moveDir.y * horizontalForwardDir.y * speed * deltaTime;

    newPos.x += moveDir.x * horizontalRightDir.x * speed * deltaTime;
    newPos.z += moveDir.x * horizontalRightDir.y * speed * deltaTime;

    // if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
    //     newPos.y += speed * deltaTime;
    // }

    // if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
    //     newPos.y -= speed * deltaTime;
    // }

    yVelocity -= gravity * deltaTime;

    if (isGrounded) {
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
            yVelocity = jumpForce;
        } else {
            yVelocity = 0;
        }
    }

    newPos.y += yVelocity * deltaTime;

    if (glfwGetKey(window, GLFW_KEY_N) != GLFW_PRESS) {
        if (isCollidingWithBlock(world, glm::vec3{newPos.x, pos.y, pos.z}, size)) {
            newPos.x = pos.x;
        }

        if (isCollidingWithBlock(world, glm::vec3{pos.x, newPos.y, pos.z}, size)) {
            yVelocity = 0;
            newPos.y = pos.y;
        }

        if (isCollidingWithBlock(world, glm::vec3{pos.x, pos.y, newPos.z}, size)) {
            newPos.z = pos.z;
        }

        // TODO: Step up if player is colliding, block at feet is filled, and three blocks above that one are empty
    }

    setPos(newPos);
}

void Player::setPos(glm::vec3 newPos) {
    pos = newPos;
    viewPos = pos;
    viewPos.y += viewHeightOffset;
}