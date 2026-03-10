#include "ecs/systems/physics_systems.h"
#include "ecs/components.h"
#include "physics/physics_world.h"

#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

// Component to link an entity to a Jolt rigid body
struct RigidBodyHandle {
    uint32_t body_id = UINT32_MAX;
};

void register_physics_systems(flecs::world& world) {
    auto* physics = world.get<PhysicsRef>().ptr;

    // CharacterControllerSystem — drives the character from PlayerInput
    world.system<const PlayerInput, MovementState, const CharacterControllerTag>("CharacterControllerSystem")
        .kind(flecs::OnUpdate)
        .each([&world, physics](const PlayerInput& pi, MovementState& ms, const CharacterControllerTag&) {
            float dt = world.delta_time();

            // Look up camera yaw
            float cam_yaw = 0.0f;
            auto cam_entity = world.lookup("MainCamera");
            if (cam_entity.is_alive()) {
                const auto* cam = cam_entity.try_get<Camera>();
                if (cam) cam_yaw = cam->yaw;
            }

            float yaw_rad = glm::radians(cam_yaw);
            glm::vec3 forward{std::sin(yaw_rad), 0.0f, std::cos(yaw_rad)};
            glm::vec3 right{-std::cos(yaw_rad), 0.0f, std::sin(yaw_rad)};

            bool moving = glm::length(pi.move_dir) > 0.1f;
            bool grounded = physics->is_character_grounded();

            // Sprint: requires holding shift, moving, on ground, and having stamina
            bool wants_sprint = pi.sprint && moving && grounded && ms.stamina > 0.0f;
            ms.sprinting = wants_sprint;

            if (ms.sprinting) {
                ms.stamina -= 30.0f * dt; // drains over ~3.3 seconds
                if (ms.stamina < 0.0f) ms.stamina = 0.0f;
            } else {
                ms.stamina += 20.0f * dt; // regens over ~5 seconds
                if (ms.stamina > ms.max_stamina) ms.stamina = ms.max_stamina;
            }

            float speed = ms.sprinting ? 9.0f : 5.0f;
            glm::vec3 move = forward * pi.move_dir.y + right * pi.move_dir.x;
            if (glm::length(move) > 0.0f) {
                move = glm::normalize(move) * speed;
            }

            if (grounded) {
                ms.jumps_remaining = 2;
                ms.vertical_velocity = 0.0f;
            } else {
                ms.vertical_velocity -= 9.81f * dt;
            }

            if (pi.jump && ms.jumps_remaining > 0) {
                ms.vertical_velocity = 7.0f;
                ms.jumps_remaining--;
            }

            if (ms.dodge_cooldown > 0.0f) ms.dodge_cooldown -= dt;
            if (ms.dodge_timer > 0.0f) ms.dodge_timer -= dt;

            if ((pi.dodge_left || pi.dodge_right) && ms.dodge_cooldown <= 0.0f) {
                glm::vec3 dir = pi.dodge_left ? -right : right;
                ms.dodge_velocity = dir * 18.0f;
                ms.dodge_timer = 0.2f;
                ms.dodge_cooldown = 0.8f;
                ms.vertical_velocity = 3.0f;
            }

            glm::vec3 velocity = move;
            if (ms.dodge_timer > 0.0f) {
                velocity += ms.dodge_velocity;
            }
            velocity.y = ms.vertical_velocity;

            physics->update_character(dt, velocity);
        });

    // PhysicsStepSystem — advances the physics simulation
    world.system("PhysicsStepSystem")
        .kind(flecs::OnUpdate)
        .run([&world, physics](flecs::iter&) {
            physics->step(world.delta_time());
        });

    // PhysicsSyncSystem — sync character position back to ECS
    world.system<Transform, const CharacterControllerTag>("PhysicsSyncSystem")
        .kind(flecs::OnUpdate)
        .each([physics](Transform& t, const CharacterControllerTag&) {
            t.position = physics->get_character_position();
        });

    // Sync rigid body positions back to ECS
    world.system<Transform, const RigidBodyHandle>("RigidBodySyncSystem")
        .kind(flecs::OnUpdate)
        .each([physics](Transform& t, const RigidBodyHandle& rb) {
            if (rb.body_id != UINT32_MAX) {
                t.position = physics->get_position(rb.body_id);
                t.rotation = physics->get_rotation(rb.body_id);
            }
        });

    // Cleanup observer — remove physics bodies when entities are destroyed
    world.observer<const RigidBodyHandle>("RigidBodyCleanup")
        .event(flecs::OnRemove)
        .each([physics](const RigidBodyHandle& rb) {
            if (rb.body_id != UINT32_MAX) {
                physics->remove_body(rb.body_id);
            }
        });
}
