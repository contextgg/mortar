#pragma once

#include <glm/glm.hpp>
#include <GLFW/glfw3.h>

class Input {
public:
    void init(GLFWwindow* window);
    void update(); // call once at start of each frame

    bool key_down(int key) const;
    bool key_pressed(int key) const; // true only on the frame the key was first pressed
    bool mouse_down(int button) const;
    bool mouse_pressed(int button) const;
    glm::vec2 mouse_delta() const;

    void capture_cursor(bool capture);
    bool cursor_captured() const;

private:
    GLFWwindow* _window = nullptr;
    bool _captured = false;
    bool _first_update = true;

    glm::dvec2 _mouse_pos{};
    glm::dvec2 _prev_mouse_pos{};

    bool _keys[GLFW_KEY_LAST + 1]{};
    bool _prev_keys[GLFW_KEY_LAST + 1]{};
    bool _mouse_buttons[GLFW_MOUSE_BUTTON_LAST + 1]{};
    bool _prev_mouse_buttons[GLFW_MOUSE_BUTTON_LAST + 1]{};
};
