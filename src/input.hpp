#pragma once

#include <cinttypes>
#include <unordered_set>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

class Input {
public:
    void updateButton(int32_t button, int32_t action, int32_t mods);
    void updateMousePos(float newMouseX, float newMouseY);
    void update(GLFWwindow* window);
    bool isButtonPressed(int32_t button);
    bool wasButtonPressed(int32_t button);
    void setMouseSensitivity(float newSensitivity);
    glm::vec2 getMouseDelta();

private:
    bool wasButtonPressedNoFocus(int32_t button);

    std::unordered_set<int32_t> pressedButtons;
    std::unordered_set<int32_t> buttonWasPressed;
    float mouseSensitivity = 1.0f;
    float mouseX = 0, mouseY = 0;
    float dx = 0.0f, dy = 0.0f;
    bool mousePosOutOfDate = true;
    bool isFocused = false;
};