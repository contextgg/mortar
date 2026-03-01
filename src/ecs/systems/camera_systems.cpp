#include "ecs/systems/camera_systems.h"
#include "ecs/components.h"
#include "core/input.h"

#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

void register_camera_systems(flecs::world& world, Input& input) {
    // InputSystem — reads GLFW input into the PlayerInput component
    world.system<PlayerInput>("InputSystem")
        .kind(flecs::PreUpdate)
        .each([&input](PlayerInput& pi) {
            // Movement direction from WASD
            glm::vec2 dir{0.0f};
            if (input.key_down(GLFW_KEY_W)) dir.y += 1.0f;
            if (input.key_down(GLFW_KEY_S)) dir.y -= 1.0f;
            if (input.key_down(GLFW_KEY_A)) dir.x -= 1.0f;
            if (input.key_down(GLFW_KEY_D)) dir.x += 1.0f;

            if (glm::length(dir) > 0.0f) {
                dir = glm::normalize(dir);
            }
            pi.move_dir = dir;

            // Mouse delta (only when cursor is captured)
            pi.mouse_delta = input.cursor_captured() ? input.mouse_delta() : glm::vec2{0.0f};

            // Buttons
            pi.fire = input.mouse_down(GLFW_MOUSE_BUTTON_LEFT);
            pi.jump = input.key_pressed(GLFW_KEY_SPACE);

            // Double-tap dodge detection
            static double last_a_press = 0.0, last_d_press = 0.0;
            const double dodge_window = 0.3;

            pi.dodge_left = false;
            pi.dodge_right = false;

            if (input.key_pressed(GLFW_KEY_A)) {
                double now = glfwGetTime();
                if ((now - last_a_press) < dodge_window) {
                    pi.dodge_left = true;
                }
                last_a_press = now;
            }
            if (input.key_pressed(GLFW_KEY_D)) {
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
            // Find the player entity to get target position
            glm::vec3 target_pos{0.0f};
            world.each([&target_pos](const Player&, const Transform& pt) {
                target_pos = pt.position;
            });

            // Get mouse delta from player input
            glm::vec2 mouse_delta{0.0f};
            world.each([&mouse_delta](const Player&, const PlayerInput& pi) {
                mouse_delta = pi.mouse_delta;
            });

            // Update yaw and pitch from mouse
            cam.yaw -= mouse_delta.x * cam.sensitivity;
            cam.pitch -= mouse_delta.y * cam.sensitivity;

            // Wrap yaw to [-180, 180] to prevent float precision drift
            cam.yaw = std::fmod(cam.yaw, 360.0f);
            if (cam.yaw > 180.0f) cam.yaw -= 360.0f;
            if (cam.yaw < -180.0f) cam.yaw += 360.0f;

            // Clamp pitch
            if (cam.pitch < cam.min_pitch) cam.pitch = cam.min_pitch;
            if (cam.pitch > cam.max_pitch) cam.pitch = cam.max_pitch;

            float pitch_rad = glm::radians(cam.pitch);
            float yaw_rad = glm::radians(cam.yaw);

            // Camera sits behind the player along the yaw direction
            glm::vec3 forward{std::sin(yaw_rad), 0.0f, std::cos(yaw_rad)};
            glm::vec3 right{std::cos(yaw_rad), 0.0f, -std::sin(yaw_rad)};

            // Shoulder target: offset right and up from the player
            glm::vec3 look_target = target_pos
                + glm::vec3(0.0f, cam.target_offset.y, 0.0f)
                + right * cam.target_offset.x;

            // Position behind and above the player
            glm::vec3 behind = -forward * cam.distance;
            behind.y = cam.distance * -std::sin(pitch_rad);
            // Shorten horizontal distance as pitch increases
            float horiz_scale = std::cos(pitch_rad);
            behind.x *= horiz_scale;
            behind.z *= horiz_scale;

            cam_t.position = look_target + behind;

            // Rotate the player model to face the camera's forward direction
            world.each([yaw_rad](const Player&, Transform& pt) {
                pt.rotation = glm::angleAxis(yaw_rad, glm::vec3(0.0f, 1.0f, 0.0f));
            });
        });
}
