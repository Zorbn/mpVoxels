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
    glm::vec4 upVec = glm::rotate(glm::mat4(1.0f), glm::radians(viewTilt.x), forwardDir) * glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
    glm::vec4 forwardVec = glm::rotate(glm::mat4(1.0f), glm::radians(viewTilt.y), rightDir) * glm::vec4(forwardDir.x, forwardDir.y, forwardDir.z, 1.0f);
    return glm::lookAt(viewPos, viewPos + glm::vec3(forwardVec.x, forwardVec.y, forwardVec.z), glm::vec3(upVec.x, upVec.y, upVec.z));
}

void Player::updateInteraction(GLFWwindow* window, World& world, BlockInteraction& blockInteraction, float deltaTime) {
    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) {
        RaycastHit hit = raycast(world, viewPos, forwardDir, range);

        if (hit.hit) {
            blockInteraction.mineBlock(world, hit.pos.x, hit.pos.y, hit.pos.z, deltaTime);
        }
    }
    // } else if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
    //     RaycastHit hit = raycast(world, viewPos, forwardDir, range);

    //     if (hit.hit) {
    //         blockInteraction.placeBlock(world, hit.lastPos.x, hit.lastPos.y, hit.lastPos.z, Blocks::Stone);
    //     }
    // }
}

// Setup up a single block if there is space for the place at the target position. (The target position
// is where the player was headed when it collided with a wall)
bool Player::tryStepUp(World& world, glm::vec3 targetPos, glm::ivec3 hitBlock, bool isGrounded) {
    glm::ivec3 feetPos(hitBlock.x, floorToInt(pos.y - size.y * 0.5f), hitBlock.z);

    if (isGrounded &&
        world.getBlock(feetPos.x, feetPos.y, feetPos.z) != Blocks::Air &&
        !isCollidingWithBlock(world, glm::vec3(targetPos.x, targetPos.y + 1.0f, targetPos.z), size)) {

        increaseViewInterp();
        setPos(glm::vec3(pos.x, pos.y + 1.0f, pos.z));

        return true;
    }

    return false;
}

void Player::updateMovement(GLFWwindow* window, World& world, float deltaTime) {
    bool isGrounded = isOnGround(world, pos, size);

    glm::vec2 horizontalForwardDir = glm::vec2(forwardDir.x, forwardDir.z);
    glm::vec2 horizontalRightDir = glm::vec2(rightDir.x, rightDir.z);

    if (glm::length(horizontalForwardDir) != 0.0f) {
        horizontalForwardDir = glm::normalize(horizontalForwardDir);
    }

    if (glm::length(horizontalRightDir) != 0.0f) {
        horizontalRightDir = glm::normalize(horizontalRightDir);
    }

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

    viewTargetTilt = {
        moveDir.x * -viewMaxTilt,
        moveDir.y * viewMaxTilt,
    };

    if (glm::length(moveDir) != 0.0f) {
        moveDir = glm::normalize(moveDir);
    }

    float xVelocity = moveDir.y * horizontalForwardDir.x * speed * deltaTime +
                      moveDir.x * horizontalRightDir.x * speed * deltaTime;
    float zVelocity = moveDir.y * horizontalForwardDir.y * speed * deltaTime +
                      moveDir.x * horizontalRightDir.y * speed * deltaTime;

    yVelocity -= gravity * deltaTime;

    if (isGrounded) {
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
            yVelocity = jumpForce;
        } else {
            yVelocity = 0;
        }
    }

    glm::vec3 newPos = pos;

    bool noClip = glfwGetKey(window, GLFW_KEY_N) == GLFW_PRESS;

    if (!noClip) {
        newPos.x += xVelocity;

        auto collision = getBlockCollision(world, glm::vec3{newPos.x, pos.y, pos.z}, size);
        if (collision.has_value()) {
            glm::ivec3 hitBlock = collision.value();
            if (!tryStepUp(world, newPos, hitBlock, isGrounded)) {
                newPos.x = pos.x;
            }
        }

        newPos.z += zVelocity;

        collision = getBlockCollision(world, glm::vec3{pos.x, pos.y, newPos.z}, size);
        if (isCollidingWithBlock(world, glm::vec3{pos.x, pos.y, newPos.z}, size)) {
            glm::ivec3 hitBlock = collision.value();
            if (!tryStepUp(world, newPos, hitBlock, isGrounded)) {
                newPos.z = pos.z;
            }
        }

        newPos.y = pos.y + yVelocity * deltaTime;

        if (isCollidingWithBlock(world, glm::vec3{pos.x, newPos.y, pos.z}, size)) {
            yVelocity = 0;
            newPos.y = pos.y;
        }
    }

    updateView(deltaTime);
    setPos(newPos);
}

void Player::increaseViewInterp() {
    viewHeightInterp++;
}

void Player::updateView(float deltaTime) {
    viewTilt += (viewTargetTilt - viewTilt) * deltaTime * viewTiltInterpSpeed;

    viewHeightInterp -= deltaTime * viewHeightInterpSpeed;

    if (viewHeightInterp < 0.0f) {
        viewHeightInterp = 0.0f;
    }
}

void Player::updateViewPos() {
    viewPos = pos;
    viewPos.y += viewHeightOffset - viewHeightInterp;
}

void Player::setPos(glm::vec3 newPos) {
    pos = newPos;
    updateViewPos();
}