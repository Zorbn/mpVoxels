#include "input.hpp"

void Input::updateButton(int32_t button, int32_t action, int32_t mods) {
    switch (action) {
    case GLFW_PRESS:
        if (pressedButtons.find(button) == pressedButtons.end()) {
            buttonWasPressed.insert(button);
        }

        pressedButtons.insert(button);
        break;
    case GLFW_RELEASE:
        pressedButtons.erase(button);
        break;
    default:
        break;
    }
}

void Input::updateMousePos(float newMouseX, float newMouseY) {
    if (!mousePosOutOfDate) {
        dx = (newMouseY - mouseY) * mouseSensitivity;
        dy = (newMouseX - mouseX) * mouseSensitivity;
    }

    mouseX = newMouseX;
    mouseY = newMouseY;

    mousePosOutOfDate = false;
}

void Input::update(GLFWwindow* window) {
    if (isFocused && wasButtonPressed(GLFW_KEY_ESCAPE)) {
        // Unfocus the window.
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        isFocused = false;
    } else if (!isFocused && wasButtonPressedNoFocus(GLFW_MOUSE_BUTTON_LEFT)) {
        // Focus the window.
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        mousePosOutOfDate = true;
        isFocused = true;
    }

    buttonWasPressed.clear();
}

bool Input::isButtonPressed(int32_t button) {
    if (!isFocused) return false;

    return pressedButtons.find(button) != pressedButtons.end();
}

bool Input::wasButtonPressed(int32_t button) {
    if (!isFocused) return false;

    return wasButtonPressedNoFocus(button);
}

bool Input::wasButtonPressedNoFocus(int32_t button) {
    return buttonWasPressed.find(button) != buttonWasPressed.end();
}

void Input::setMouseSensitivity(float newSensitivity) {
    mouseSensitivity = newSensitivity;
}

glm::vec2 Input::getMouseDelta() {
    if (!isFocused) return glm::vec2(0.0f);

    return glm::vec2(dx, dy);
}