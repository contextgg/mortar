#pragma once

#include <filesystem>
#include <string>
#include <glm/glm.hpp>
#include <flecs.h>

class VulkanEngine;
class PhysicsWorld;

struct MapLoadResult {
    bool success = false;
    std::string error;
    glm::vec3 player_spawn_position{0.0f, 2.0f, 0.0f};
};

MapLoadResult load_map(const std::filesystem::path& path, flecs::world& world,
                       VulkanEngine& engine, PhysicsWorld& physics);
