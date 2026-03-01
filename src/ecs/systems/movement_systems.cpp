#include "ecs/systems/movement_systems.h"
#include "ecs/components.h"

#include <glm/gtc/quaternion.hpp>

void register_movement_systems(flecs::world& world) {
    world.system<Transform, const Velocity>("MovementSystem")
        .kind(flecs::OnUpdate)
        .each([&world](Transform& t, const Velocity& v) {
            t.position += v.linear * world.delta_time();
        });

    world.system<Transform, const AngularVelocity>("RotationSystem")
        .kind(flecs::OnUpdate)
        .each([&world](Transform& t, const AngularVelocity& av) {
            float angle = av.speed * world.delta_time();
            glm::quat delta = glm::angleAxis(angle, glm::normalize(av.axis));
            t.rotation = delta * t.rotation;
        });

    world.system<DestroyAfter>("DestroyAfterSystem")
        .kind(flecs::OnUpdate)
        .each([&world](flecs::entity e, DestroyAfter& d) {
            d.time_remaining -= world.delta_time();
            if (d.time_remaining <= 0.0f) {
                e.destruct();
            }
        });
}
