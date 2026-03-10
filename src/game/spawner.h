#pragma once

#include <flecs.h>
#include <glm/glm.hpp>
#include <memory>

#include "animation/skeleton.h"

struct SpawnerConfig {
    float spawn_interval = 5.0f;
    float spawn_radius = 15.0f;
    int max_enemies = 10;
    uint32_t enemy_mesh = 0;
    uint32_t enemy_material = 0;
    std::shared_ptr<SkeletonData> enemy_skeleton; // optional, for animated enemies
};

struct SpawnerState {
    float timer = 0.0f;
};

void register_spawner_system(flecs::world& world);
