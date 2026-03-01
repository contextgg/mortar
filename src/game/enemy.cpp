#include "game/enemy.h"
#include "ecs/components.h"
#include "physics/physics_world.h"

flecs::entity create_enemy(flecs::world& world, PhysicsWorld& physics,
                            glm::vec3 position, uint32_t mesh, uint32_t material) {
    auto e = world.entity()
        .add<Enemy>()
        .set(Transform{.position = position})
        .set(MeshHandle{.index = mesh})
        .set(MaterialHandle{.index = material})
        .set(Health{.current = 50.0f, .max = 50.0f})
        .set(AIState{
            .state = AIStateType::Idle,
            .detect_range = 20.0f,
            .attack_range = 2.5f,
            .move_speed = 3.5f,
            .attack_cooldown = 1.5f,
        });

    // Add a static physics body for raycasting
    physics.add_box(glm::vec3(0.4f, 0.5f, 0.4f), position,
                    glm::quat(1, 0, 0, 0), true, e.id());

    return e;
}
