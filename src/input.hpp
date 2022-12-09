#pragma once

#include <cinttypes>
#include <unordered_set>

#include <GLFW/glfw3.h>

class Input {
public:
    void updateButton(int32_t button, int32_t action, int32_t mods);
    void update();
    bool isButtonPressed(int32_t button);
    bool wasButtonPressed(int32_t button);

private:
    std::unordered_set<int32_t> pressedButtons;
    std::unordered_set<int32_t> buttonWasPressed;
};