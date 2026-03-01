#include "game/spawner.h"
#include "game/enemy.h"
#include "ecs/components.h"
#include "physics/physics_world.h"

#include <cmath>
#include <cstdlib>

void register_spawner_system(flecs::world& world, PhysicsWorld& physics) {
    world.system<SpawnerConfig, SpawnerState>("SpawnerSystem")
        .kind(flecs::OnUpdate)
        .each([&world, &physics](SpawnerConfig& cfg, SpawnerState& state) {
            state.timer -= world.delta_time();
            if (state.timer > 0.0f) return;

            state.timer = cfg.spawn_interval;

            // Count current enemies
            int enemy_count = 0;
            world.each([&enemy_count](const Enemy&) {
                enemy_count++;
            });

            if (enemy_count >= cfg.max_enemies) return;

            // Find player position for spawn reference
            glm::vec3 player_pos{0.0f};
            world.each([&player_pos](const Player&, const Transform& t) {
                player_pos = t.position;
            });

            // Spawn at random position around player
            float angle = static_cast<float>(rand()) / RAND_MAX * 6.28318f;
            float dist = cfg.spawn_radius + static_cast<float>(rand()) / RAND_MAX * 5.0f;
            glm::vec3 spawn_pos{
                player_pos.x + std::cos(angle) * dist,
                0.5f,
                player_pos.z + std::sin(angle) * dist,
            };

            create_enemy(world, physics, spawn_pos, cfg.enemy_mesh, cfg.enemy_material);
        });
}
