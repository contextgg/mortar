#include "game/spawner.h"
#include "game/enemy.h"
#include "ecs/components.h"

#include <cmath>
#include <cstdlib>

void register_spawner_system(flecs::world& world) {
    world.system<SpawnerConfig, SpawnerState>("SpawnerSystem")
        .kind(flecs::OnUpdate)
        .each([](flecs::entity e, SpawnerConfig& cfg, SpawnerState& state) {
            auto world = e.world();
            float dt = world.delta_time();

            state.timer -= dt;
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
            auto player = world.lookup("Player");
            if (player.is_alive()) {
                const auto* pt = player.try_get<Transform>();
                if (pt) player_pos = pt->position;
            }

            // Spawn at random position around player
            float angle = static_cast<float>(rand()) / RAND_MAX * 6.28318f;
            float dist = cfg.spawn_radius + static_cast<float>(rand()) / RAND_MAX * 5.0f;
            glm::vec3 spawn_pos{
                player_pos.x + std::cos(angle) * dist,
                0.5f,
                player_pos.z + std::sin(angle) * dist,
            };

            create_enemy(world, spawn_pos, cfg.enemy_mesh, cfg.enemy_material, cfg.enemy_skeleton);
        });
}
