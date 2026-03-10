#include "map/map_common.h"

#include <fstream>
#include <iostream>

using json = nlohmann::json;

glm::vec3 read_vec3(const json& j, glm::vec3 fallback) {
    if (!j.is_array() || j.size() < 3) return fallback;
    return glm::vec3(j[0].get<float>(), j[1].get<float>(), j[2].get<float>());
}

glm::vec4 read_vec4(const json& j, glm::vec4 fallback) {
    if (!j.is_array() || j.size() < 4) return fallback;
    return glm::vec4(j[0].get<float>(), j[1].get<float>(), j[2].get<float>(), j[3].get<float>());
}

glm::quat read_quat(const json& j, glm::quat fallback) {
    if (!j.is_array() || j.size() < 4) return fallback;
    float x = j[0].get<float>();
    float y = j[1].get<float>();
    float z = j[2].get<float>();
    float w = j[3].get<float>();
    return glm::quat(w, x, y, z);
}

bool parse_map_file(const std::filesystem::path& path, json& out, MapLoadResult& result) {
    std::ifstream file(path);
    if (!file.is_open()) {
        result.error = "Failed to open map file: " + path.string();
        return false;
    }

    try {
        out = json::parse(file);
    } catch (const json::parse_error& e) {
        result.error = std::string("JSON parse error: ") + e.what();
        return false;
    }

    if (!out.contains("version") || out["version"].get<int>() != 1) {
        result.error = "Unsupported or missing map version (expected 1)";
        return false;
    }

    if (!out.contains("entities") || !out["entities"].is_array()) {
        result.error = "Map file missing 'entities' array";
        return false;
    }

    return true;
}

void load_map_entities_shared(const json& entities, flecs::world& world,
                              PhysicsWorld& physics, MapLoadResult& result) {
    for (const auto& ent_data : entities) {
        std::string name = ent_data.value("name", "unnamed");
        std::string type = ent_data.value("type", "geometry");

        // Player spawn — extract position, don't create entity
        if (type == "player_spawn") {
            if (ent_data.contains("transform") && ent_data["transform"].contains("position")) {
                result.player_spawn_position = read_vec3(ent_data["transform"]["position"],
                                                         glm::vec3(0.0f, 2.0f, 0.0f));
            }
            continue;
        }

        // Skip purely visual entities (no physics, no gameplay components)
        bool has_physics = ent_data.contains("physics");
        bool has_health = ent_data.contains("health");
        bool has_ai = ent_data.contains("ai_state");
        bool has_spawner = ent_data.contains("spawner_config");
        bool is_enemy = (type == "enemy_spawn");

        if (!has_physics && !has_health && !has_ai && !has_spawner && !is_enemy) {
            continue; // Visual-only entity (grass, lights, decorations)
        }

        auto entity = world.entity(name.c_str());

        // Transform
        if (ent_data.contains("transform")) {
            const auto& t = ent_data["transform"];
            Transform transform;
            if (t.contains("position"))
                transform.position = read_vec3(t["position"]);
            if (t.contains("rotation"))
                transform.rotation = read_quat(t["rotation"]);
            if (t.contains("scale"))
                transform.scale = read_vec3(t["scale"], glm::vec3(1.0f));
            entity.set(transform);
        }

        // Physics
        if (has_physics) {
            const auto& phys = ent_data["physics"];
            std::string shape = phys.value("shape", "box");
            std::string phys_type = phys.value("type", "static");
            bool is_static = (phys_type == "static");

            glm::vec3 pos(0.0f);
            glm::quat rot(1, 0, 0, 0);
            if (ent_data.contains("transform")) {
                const auto& t = ent_data["transform"];
                if (t.contains("position"))
                    pos = read_vec3(t["position"]);
                if (t.contains("rotation"))
                    rot = read_quat(t["rotation"]);
            }

            uint64_t eid = is_static ? 0 : entity.id();
            if (is_static && has_health)
                eid = entity.id();

            if (shape == "box") {
                glm::vec3 half_extents = read_vec3(phys["half_extents"], glm::vec3(0.5f));
                physics.add_box(half_extents, pos, rot, is_static, eid);
            } else if (shape == "sphere") {
                float radius = phys.value("radius", 0.5f);
                physics.add_sphere(radius, pos, is_static, eid);
            }
        }

        // Health
        if (has_health) {
            const auto& h = ent_data["health"];
            Health health;
            if (h.contains("current"))
                health.current = h["current"].get<float>();
            if (h.contains("max"))
                health.max = h["max"].get<float>();
            entity.set(health);
        }

        // AIState
        if (has_ai) {
            const auto& ai = ent_data["ai_state"];
            AIState state;
            if (ai.contains("state")) {
                std::string s = ai["state"].get<std::string>();
                if (s == "idle") state.state = AIStateType::Idle;
                else if (s == "chase") state.state = AIStateType::Chase;
                else if (s == "attack") state.state = AIStateType::Attack;
                else if (s == "dead") state.state = AIStateType::Dead;
            }
            if (ai.contains("detect_range"))
                state.detect_range = ai["detect_range"].get<float>();
            if (ai.contains("attack_range"))
                state.attack_range = ai["attack_range"].get<float>();
            if (ai.contains("move_speed"))
                state.move_speed = ai["move_speed"].get<float>();
            if (ai.contains("attack_cooldown"))
                state.attack_cooldown = ai["attack_cooldown"].get<float>();
            if (ai.contains("cooldown_timer"))
                state.cooldown_timer = ai["cooldown_timer"].get<float>();
            entity.set(state);
        }

        // Enemy tag
        if (is_enemy || has_ai) {
            entity.add<Enemy>();
        }

        // SpawnerConfig (server doesn't need mesh/material indices)
        if (has_spawner) {
            const auto& sc = ent_data["spawner_config"];
            SpawnerConfig config;
            if (sc.contains("spawn_interval"))
                config.spawn_interval = sc["spawn_interval"].get<float>();
            if (sc.contains("spawn_radius"))
                config.spawn_radius = sc["spawn_radius"].get<float>();
            if (sc.contains("max_count"))
                config.max_enemies = sc["max_count"].get<int>();
            // mesh/material left at 0 — server doesn't render
            entity.set(config);
            entity.set(SpawnerState{.timer = config.spawn_interval * 0.5f});
        }
    }
}

MapLoadResult load_map_server(const std::filesystem::path& path, flecs::world& world,
                              PhysicsWorld& physics) {
    MapLoadResult result;
    json map_data;

    if (!parse_map_file(path, map_data, result))
        return result;

    load_map_entities_shared(map_data["entities"], world, physics, result);
    result.success = true;
    return result;
}
