#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

struct Transform {
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};
};

struct Velocity {
    glm::vec3 linear{0.0f};
};

struct AngularVelocity {
    glm::vec3 axis{0.0f, 1.0f, 0.0f};
    float speed = 1.0f; // radians per second
};

struct MeshHandle {
    uint32_t index = 0;
};

struct MaterialHandle {
    uint32_t index = 0;
};

// Camera with 3rd-person orbit parameters
struct Camera {
    float fov = 70.0f;
    float near_plane = 0.1f;
    float far_plane = 200.0f;
    float distance = 8.0f;
    float pitch = -20.0f;       // degrees
    float yaw = 0.0f;           // degrees
    float sensitivity = 0.03f;
    glm::vec3 target_offset{0.6f, 1.6f, 0.0f};
    float min_pitch = -45.0f;
    float max_pitch = 20.0f;
};

struct DirectionalLight {
    glm::vec3 direction{-0.5f, -1.0f, -0.3f};
    glm::vec3 color{1.0f};
    float intensity = 1.0f;
};

struct PointLight {
    glm::vec3 color{1.0f};
    float intensity = 1.0f;
    float radius = 10.0f;
};

// Tags and input
struct Player {};

struct PlayerInput {
    glm::vec2 move_dir{0.0f};
    glm::vec2 mouse_delta{0.0f};
    bool fire = false;
    bool jump = false;
    bool dodge_left = false;   // triggered by double-tap A
    bool dodge_right = false;  // triggered by double-tap D
};

struct MovementState {
    float vertical_velocity = 0.0f;   // accumulated Y velocity (gravity + jumps)
    int jumps_remaining = 2;          // reset to 2 on ground, decrement on each jump
    float dodge_cooldown = 0.0f;      // time until next dodge allowed
    float dodge_timer = 0.0f;         // remaining dodge burst time
    glm::vec3 dodge_velocity{0.0f};   // direction+speed of active dodge
};

struct CharacterControllerTag {};

// Combat (Phase 2+)
struct Health {
    float current = 100.0f;
    float max = 100.0f;
};

struct Weapon {
    float fire_rate = 5.0f;     // shots per second
    float damage = 25.0f;
    float range = 100.0f;
    int ammo = 30;
    int max_ammo = 30;
    float cooldown = 0.0f;
};

struct Projectile {
    glm::vec3 direction{0.0f, 0.0f, -1.0f};
    float speed = 50.0f;
    float damage = 25.0f;
};

struct DestroyAfter {
    float time_remaining = 1.0f;
};

// AI (Phase 3+)
enum class AIStateType { Idle, Chase, Attack, Dead };

struct Enemy {};

struct AIState {
    AIStateType state = AIStateType::Idle;
    float detect_range = 20.0f;
    float attack_range = 3.0f;
    float move_speed = 4.0f;
    float attack_cooldown = 1.0f;
    float cooldown_timer = 0.0f;
};

// Particles (Phase 4+)
struct ParticleEmitter {
    int burst_count = 10;
    float emit_timer = 0.0f;
    float emit_interval = 0.0f; // 0 = one-shot
    glm::vec4 start_color{1.0f, 0.8f, 0.2f, 1.0f};
    glm::vec4 end_color{1.0f, 0.2f, 0.0f, 0.0f};
    float start_size = 0.3f;
    float end_size = 0.0f;
    float lifetime = 0.5f;
    float speed = 5.0f;
    bool active = true;
};

struct Particle {
    glm::vec4 start_color{1.0f};
    glm::vec4 end_color{0.0f};
    float start_size = 0.3f;
    float end_size = 0.0f;
    float lifetime = 0.5f;
    float age = 0.0f;
};

// Audio (Phase 4+)
struct AudioSource {
    uint32_t sound_id = 0;
    float volume = 1.0f;
    bool loop = false;
    bool playing = false;
};

// HUD singleton (Phase 4+)
struct HUDState {
    bool show_crosshair = true;
    bool show_health = true;
    bool show_ammo = true;
};
