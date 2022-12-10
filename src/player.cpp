#include "player.hpp"

Player::Player() {
    updateRotation(0.0f, 0.0f);
}

void Player::updateRotation(float dx, float dy) {
    rotationX = glm::clamp(rotationX + dx, -maxPitch, maxPitch);
    rotationY -= dy;

    glm::mat4 lookMat = glm::rotate(glm::mat4(1.0f), glm::radians(rotationY), glm::vec3(0.0f, 1.0f, 0.0f)) *
                        glm::rotate(glm::mat4(1.0f), glm::radians(rotationX), glm::vec3(1.0f, 0.0f, 0.0f));
    glm::vec4 forwardVec = lookMat * glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
    forwardDir = glm::vec3(forwardVec.x, forwardVec.y, forwardVec.z);
    glm::vec4 rightVec = lookMat * glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
    rightDir = glm::vec3(rightVec.x, rightVec.y, rightVec.z);
}

glm::mat4 Player::getViewMatrix() {
    glm::vec4 upVec = glm::rotate(glm::mat4(1.0f), glm::radians(viewTilt.x), forwardDir) * glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
    glm::vec4 forwardVec = glm::rotate(glm::mat4(1.0f), glm::radians(glm::clamp(viewTilt.y, -maxPitch - rotationX, maxPitch - rotationX)), rightDir) *
                           glm::vec4(forwardDir.x, forwardDir.y, forwardDir.z, 1.0f);
    return glm::lookAt(viewPos, viewPos + glm::vec3(forwardVec.x, forwardVec.y, forwardVec.z), glm::vec3(upVec.x, upVec.y, upVec.z));
}

float Player::getFov() {
    return fov;
}

void Player::updateInteraction(Input& input, World& world, BlockInteraction& blockInteraction, float deltaTime) {
    if (input.isButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) {
        RaycastHit hit = raycast(world, viewPos, forwardDir, range);

        if (hit.hit) {
            blockInteraction.mineBlock(world, hit.pos.x, hit.pos.y, hit.pos.z, deltaTime);
        }
    } else if (input.wasButtonPressed(GLFW_MOUSE_BUTTON_RIGHT)) {
        RaycastHit hit = raycast(world, viewPos, forwardDir, range);

        if (hit.hit) {
            blockInteraction.placeBlock(world, hit.lastPos.x, hit.lastPos.y, hit.lastPos.z, Blocks::Stone);
        }
    }
}

// Setup up a single block if there is space for the place at the target position. (The target position
// is where the player was headed when it collided with a wall)
bool Player::tryStepUp(World& world, glm::vec3 targetPos, glm::ivec3 hitBlock, bool isGrounded) {
    glm::ivec3 feetPos(hitBlock.x, floorToInt(pos.y - size.y * 0.5f), hitBlock.z);

    if (isGrounded &&
        world.isBlockOccupied(feetPos.x, feetPos.y, feetPos.z) &&
        !isCollidingWithBlock(world, glm::vec3(targetPos.x, targetPos.y + 1.0f, targetPos.z), size)) {

        increaseViewInterp();
        setPos(glm::vec3(pos.x, pos.y + 1.0f, pos.z));

        return true;
    }

    return false;
}

bool Player::canStep(World& world, glm::vec3 newPos, int32_t axis, bool isCrouching, bool isGrounded) {
    glm::vec3 axisNewPos = pos;
    axisNewPos[axis] = newPos[axis];
    bool snagLedge = isCrouching && isGrounded && !isOnGround(world, axisNewPos, size);
    if (snagLedge) {
        return false;
    } else {
        auto collision = getBlockCollision(world, axisNewPos, size);
        if (collision.has_value()) {
            glm::ivec3 hitBlock = collision.value();
            if (!tryStepUp(world, newPos, hitBlock, isGrounded)) {
                return false;
            }
        }
    }

    return true;
}

void Player::updateMovement(Input& input, World& world, float deltaTime) {
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

    if (input.isButtonPressed(GLFW_KEY_W)) {
        moveDir.y += 1.0f;
    }

    if (input.isButtonPressed(GLFW_KEY_S)) {
        moveDir.y -= 1.0f;
    }

    if (input.isButtonPressed(GLFW_KEY_D)) {
        moveDir.x -= 1.0f;
    }

    if (input.isButtonPressed(GLFW_KEY_A)) {
        moveDir.x += 1.0f;
    }

    viewTargetTilt = {
        moveDir.x * -viewMaxTilt,
        moveDir.y * viewMaxTilt,
    };

    if (glm::length(moveDir) != 0.0f) {
        moveDir = glm::normalize(moveDir);
    }

    float modifiedSpeed = speed * deltaTime;

    bool isSprinting = input.isButtonPressed(GLFW_KEY_LEFT_SHIFT);
    bool isCrouching = input.isButtonPressed(GLFW_KEY_C);

    targetFov = defaultFov;

    if (isCrouching) {
        modifiedSpeed *= crouchMultiplier;
    } else if (isSprinting) {
        modifiedSpeed *= sprintMultiplier;
        targetFov = defaultFov * fovSprintMultiplier;
    }

    float xVelocity = moveDir.y * horizontalForwardDir.x * modifiedSpeed +
                      moveDir.x * horizontalRightDir.x * modifiedSpeed;
    float zVelocity = moveDir.y * horizontalForwardDir.y * modifiedSpeed +
                      moveDir.x * horizontalRightDir.y * modifiedSpeed;

    yVelocity -= gravity * deltaTime;

    if (isGrounded) {
        if (input.isButtonPressed(GLFW_KEY_SPACE)) {
            yVelocity = jumpForce;
        } else {
            yVelocity = 0;
        }
    }

    glm::vec3 newPos = pos;

    bool noClip = input.isButtonPressed(GLFW_KEY_N);

    if (!noClip) {
        newPos.x += xVelocity;

        if (!canStep(world, newPos, 0, isCrouching, isGrounded)) {
            newPos.x = pos.x;
        }

        newPos.z += zVelocity;

        if (!canStep(world, newPos, 2, isCrouching, isGrounded)) {
            newPos.z = pos.z;
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
    fov += (targetFov - fov) * deltaTime * fovInterpSpeed;
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