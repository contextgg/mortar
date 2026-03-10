#include "ecs/systems/camera_systems.h"
#include "ecs/components.h"
#include "core/input.h"

#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

void register_camera_systems(flecs::world& world) {
    // InputSystem — reads GLFW input into the PlayerInput component
    world.system<PlayerInput>("InputSystem")
        .kind(flecs::PreUpdate)
        .each([&world](PlayerInput& pi) {
            auto* input = world.get<InputRef>().ptr;

            glm::vec2 dir{0.0f};
            if (input->key_down(GLFW_KEY_W)) dir.y += 1.0f;
            if (input->key_down(GLFW_KEY_S)) dir.y -= 1.0f;
            if (input->key_down(GLFW_KEY_A)) dir.x -= 1.0f;
            if (input->key_down(GLFW_KEY_D)) dir.x += 1.0f;

            if (glm::length(dir) > 0.0f) {
                dir = glm::normalize(dir);
            }
            pi.move_dir = dir;

            pi.mouse_delta = input->cursor_captured() ? input->mouse_delta() : glm::vec2{0.0f};

            pi.fire = input->mouse_down(GLFW_MOUSE_BUTTON_LEFT);
            pi.jump = input->key_pressed(GLFW_KEY_SPACE);
            pi.sprint = input->key_down(GLFW_KEY_LEFT_SHIFT);

            // Double-tap dodge detection
            static double last_a_press = 0.0, last_d_press = 0.0;
            const double dodge_window = 0.3;

            pi.dodge_left = false;
            pi.dodge_right = false;

            if (input->key_pressed(GLFW_KEY_A)) {
                double now = glfwGetTime();
                if ((now - last_a_press) < dodge_window) {
                    pi.dodge_left = true;
                }
                last_a_press = now;
            }
            if (input->key_pressed(GLFW_KEY_D)) {
                double now = glfwGetTime();
                if ((now - last_d_press) < dodge_window) {
                    pi.dodge_right = true;
                }
                last_d_press = now;
            }
        });

    // ThirdPersonCameraSystem — locked behind player, over-the-shoulder
    world.system<Transform, Camera>("ThirdPersonCameraSystem")
        .kind(flecs::PreUpdate)
        .each([&world](Transform& cam_t, Camera& cam) {
            // Look up player entity by name
            auto player = world.lookup("Player");
            if (!player.is_alive()) return;

            const auto* pt = player.try_get<Transform>();
            const auto* pi = player.try_get<PlayerInput>();
            if (!pt) return;

            glm::vec3 target_pos = pt->position;
            glm::vec2 mouse_delta = pi ? pi->mouse_delta : glm::vec2{0.0f};

            // Update yaw and pitch from mouse
            cam.yaw -= mouse_delta.x * cam.sensitivity;
            cam.pitch -= mouse_delta.y * cam.sensitivity;

            cam.yaw = std::fmod(cam.yaw, 360.0f);
            if (cam.yaw > 180.0f) cam.yaw -= 360.0f;
            if (cam.yaw < -180.0f) cam.yaw += 360.0f;

            if (cam.pitch < cam.min_pitch) cam.pitch = cam.min_pitch;
            if (cam.pitch > cam.max_pitch) cam.pitch = cam.max_pitch;

            float pitch_rad = glm::radians(cam.pitch);
            float yaw_rad = glm::radians(cam.yaw);

            glm::vec3 forward{std::sin(yaw_rad), 0.0f, std::cos(yaw_rad)};
            glm::vec3 right{std::cos(yaw_rad), 0.0f, -std::sin(yaw_rad)};

            glm::vec3 look_target = target_pos
                + glm::vec3(0.0f, cam.target_offset.y, 0.0f)
                + right * cam.target_offset.x;

            glm::vec3 behind = -forward * cam.distance;
            behind.y = cam.distance * -std::sin(pitch_rad);
            float horiz_scale = std::cos(pitch_rad);
            behind.x *= horiz_scale;
            behind.z *= horiz_scale;

            cam_t.position = look_target + behind;

            // Rotate the player model to face the camera's forward direction
            if (player.is_alive()) {
                auto& player_t = player.ensure<Transform>();
                player_t.rotation = glm::angleAxis(yaw_rad, glm::vec3(0.0f, 1.0f, 0.0f));
            }
        });
}
