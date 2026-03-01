#include "ecs/systems/combat_systems.h"
#include "ecs/components.h"
#include "physics/physics_world.h"

#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <iostream>

void register_combat_systems(flecs::world& world, PhysicsWorld& physics) {
    // ShootingSystem — hitscan raycast on fire button
    world.system<const PlayerInput, const Transform, Weapon>("ShootingSystem")
        .kind(flecs::OnUpdate)
        .each([&world, &physics](flecs::entity e, const PlayerInput& pi, const Transform& t, Weapon& w) {
            // Reduce cooldown
            w.cooldown -= world.delta_time();
            if (w.cooldown < 0.0f) w.cooldown = 0.0f;

            if (!pi.fire || w.cooldown > 0.0f || w.ammo <= 0) return;

            // Fire!
            w.cooldown = 1.0f / w.fire_rate;
            w.ammo--;

            // Get camera forward for aiming
            glm::vec3 cam_pos{0.0f};
            glm::vec3 cam_forward{0.0f, 0.0f, -1.0f};
            world.each([&cam_pos, &cam_forward](const Camera& cam, const Transform& ct) {
                cam_pos = ct.position;
                float pitch_rad = glm::radians(cam.pitch);
                float yaw_rad = glm::radians(cam.yaw);
                cam_forward.x = -std::cos(pitch_rad) * std::sin(yaw_rad);
                cam_forward.y = std::sin(pitch_rad);
                cam_forward.z = -std::cos(pitch_rad) * std::cos(yaw_rad);
                cam_forward = glm::normalize(cam_forward);
            });

            auto hit = physics.raycast(cam_pos, cam_forward, w.range);
            if (hit) {
                std::cout << "Hit at distance " << hit->distance << std::endl;

                // Apply damage to hit entity
                if (hit->entity_id != 0) {
                    flecs::entity target = world.entity(static_cast<flecs::entity_t>(hit->entity_id));
                    if (target.is_alive() && target.has<Health>()) {
                        auto& hp = target.ensure<Health>();
                        hp.current -= w.damage;
                        if (hp.current <= 0.0f) {
                            hp.current = 0.0f;
                        }
                    }
                }
            }
        });

    // DamageSystem — check for dead entities
    world.system<const Health>("DamageSystem")
        .kind(flecs::OnUpdate)
        .each([&world](flecs::entity e, const Health& h) {
            if (h.current <= 0.0f && !e.has<Player>()) {
                // Add destroy timer if not already present
                if (!e.has<DestroyAfter>()) {
                    e.set(DestroyAfter{.time_remaining = 0.5f});
                }
            }
        });
}
