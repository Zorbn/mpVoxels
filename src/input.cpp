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

void Input::update() {
    buttonWasPressed.clear();
}

bool Input::isButtonPressed(int32_t button) {
    return pressedButtons.find(button) != pressedButtons.end();
}

bool Input::wasButtonPressed(int32_t button) {
    return buttonWasPressed.find(button) != buttonWasPressed.end();
}