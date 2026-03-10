#include "game/enemy.h"
#include "ecs/components.h"
#include "physics/physics_world.h"

flecs::entity create_enemy(flecs::world& world, glm::vec3 position,
                            uint32_t mesh, uint32_t material,
                            std::shared_ptr<SkeletonData> skeleton) {
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

    // Add skeletal animation if the enemy has a skeleton
    if (skeleton && !skeleton->clips.empty()) {
        uint32_t idle_clip = 0;
        for (uint32_t i = 0; i < static_cast<uint32_t>(skeleton->clips.size()); i++) {
            const auto& name = skeleton->clips[i].name;
            if (name.find("Idle") != std::string::npos ||
                name.find("idle") != std::string::npos) {
                idle_clip = i;
                break;
            }
        }
        e.set(AnimatedModel{
            .skeleton = skeleton,
            .current_clip = idle_clip,
            .time = 0.0f,
            .speed = 1.0f,
            .looping = true,
            .playing = true,
        });
    }

    // Add a static physics body for raycasting
    auto* physics = world.get<PhysicsRef>().ptr;
    physics->add_box(glm::vec3(0.4f, 0.5f, 0.4f), position,
                     glm::quat(1, 0, 0, 0), true, e.id());

    return e;
}
