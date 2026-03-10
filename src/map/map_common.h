#pragma once

#include <filesystem>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <nlohmann/json.hpp>
#include <flecs.h>

#include "ecs/components.h"
#include "physics/physics_world.h"
#include "game/spawner.h"

struct MapLoadResult {
    bool success = false;
    std::string error;
    glm::vec3 player_spawn_position{0.0f, 2.0f, 0.0f};
};

// JSON reading helpers
glm::vec3 read_vec3(const nlohmann::json& j, glm::vec3 fallback = glm::vec3(0.0f));
glm::vec4 read_vec4(const nlohmann::json& j, glm::vec4 fallback = glm::vec4(0.0f));
glm::quat read_quat(const nlohmann::json& j, glm::quat fallback = glm::quat(1, 0, 0, 0));

// Parse a map JSON file into a json object, validating version.
// On failure, result.error is set and result.success remains false.
bool parse_map_file(const std::filesystem::path& path, nlohmann::json& out, MapLoadResult& result);

// Load shared entity data (transform, physics, health, AI, spawner, enemy tags)
// into the ECS world. Skips visual-only data (mesh, material, lights, particles).
// This is used by the server. The client calls this plus additional visual loading.
void load_map_entities_shared(const nlohmann::json& entities, flecs::world& world,
                              PhysicsWorld& physics, MapLoadResult& result);

// Server-side map loading: parses file and loads physics + gameplay entities.
MapLoadResult load_map_server(const std::filesystem::path& path, flecs::world& world,
                              PhysicsWorld& physics);
