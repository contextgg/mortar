#include "core/input.h"
#include <cstring>

void Input::init(GLFWwindow* window) {
    _window = window;
    memset(_keys, 0, sizeof(_keys));
    memset(_prev_keys, 0, sizeof(_prev_keys));
    memset(_mouse_buttons, 0, sizeof(_mouse_buttons));
    memset(_prev_mouse_buttons, 0, sizeof(_prev_mouse_buttons));
    glfwGetCursorPos(_window, &_mouse_pos.x, &_mouse_pos.y);
    _prev_mouse_pos = _mouse_pos;
}

void Input::update() {
    // Save previous state
    memcpy(_prev_keys, _keys, sizeof(_keys));
    memcpy(_prev_mouse_buttons, _mouse_buttons, sizeof(_mouse_buttons));

    // Poll current key states
    for (int k = 0; k <= GLFW_KEY_LAST; k++) {
        _keys[k] = glfwGetKey(_window, k) == GLFW_PRESS;
    }

    // Poll mouse buttons
    for (int b = 0; b <= GLFW_MOUSE_BUTTON_LAST; b++) {
        _mouse_buttons[b] = glfwGetMouseButton(_window, b) == GLFW_PRESS;
    }

    // Mouse position
    _prev_mouse_pos = _mouse_pos;
    glfwGetCursorPos(_window, &_mouse_pos.x, &_mouse_pos.y);

    if (_first_update) {
        _prev_mouse_pos = _mouse_pos;
        _first_update = false;
    }
}

bool Input::key_down(int key) const {
    if (key < 0 || key > GLFW_KEY_LAST) return false;
    return _keys[key];
}

bool Input::key_pressed(int key) const {
    if (key < 0 || key > GLFW_KEY_LAST) return false;
    return _keys[key] && !_prev_keys[key];
}

bool Input::mouse_down(int button) const {
    if (button < 0 || button > GLFW_MOUSE_BUTTON_LAST) return false;
    return _mouse_buttons[button];
}

bool Input::mouse_pressed(int button) const {
    if (button < 0 || button > GLFW_MOUSE_BUTTON_LAST) return false;
    return _mouse_buttons[button] && !_prev_mouse_buttons[button];
}

glm::vec2 Input::mouse_delta() const {
    return glm::vec2(_mouse_pos - _prev_mouse_pos);
}

void Input::capture_cursor(bool capture) {
    _captured = capture;
    glfwSetInputMode(_window, GLFW_CURSOR,
        capture ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    if (capture) {
        // Reset delta on capture to avoid jump
        _first_update = true;
    }
}

bool Input::cursor_captured() const {
    return _captured;
}
