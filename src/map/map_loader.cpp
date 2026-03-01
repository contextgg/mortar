#include "map/map_loader.h"

#include <fstream>
#include <iostream>
#include <unordered_map>

#include <nlohmann/json.hpp>
#include <glm/gtc/quaternion.hpp>

#include "core/engine.h"
#include "core/types.h"
#include "ecs/components.h"
#include "physics/physics_world.h"
#include "renderer/mesh.h"
#include "game/spawner.h"

using json = nlohmann::json;

// Helper: read a vec3 from a JSON array [x, y, z]
static glm::vec3 read_vec3(const json& j, glm::vec3 fallback = glm::vec3(0.0f)) {
    if (!j.is_array() || j.size() < 3) return fallback;
    return glm::vec3(j[0].get<float>(), j[1].get<float>(), j[2].get<float>());
}

// Helper: read a vec4 from a JSON array [x, y, z, w]
static glm::vec4 read_vec4(const json& j, glm::vec4 fallback = glm::vec4(0.0f)) {
    if (!j.is_array() || j.size() < 4) return fallback;
    return glm::vec4(j[0].get<float>(), j[1].get<float>(), j[2].get<float>(), j[3].get<float>());
}

// Helper: read a quaternion from JSON array [x, y, z, w] (JSON convention)
// glm::quat constructor is (w, x, y, z)
static glm::quat read_quat(const json& j, glm::quat fallback = glm::quat(1, 0, 0, 0)) {
    if (!j.is_array() || j.size() < 4) return fallback;
    float x = j[0].get<float>();
    float y = j[1].get<float>();
    float z = j[2].get<float>();
    float w = j[3].get<float>();
    return glm::quat(w, x, y, z);
}

// Upload a mesh by name, caching to avoid duplicate uploads
static uint32_t get_or_upload_mesh(const std::string& mesh_type, VulkanEngine& engine,
                                   std::unordered_map<std::string, uint32_t>& cache) {
    auto it = cache.find(mesh_type);
    if (it != cache.end()) return it->second;

    uint32_t index = 0;
    if (mesh_type == "cube") {
        index = engine.upload_mesh(cube_vertices(), cube_indices());
    } else if (mesh_type == "plane") {
        index = engine.upload_mesh(plane_vertices(50.0f, 10.0f), plane_indices());
    } else if (mesh_type == "humanoid") {
        index = engine.upload_mesh(humanoid_vertices(), humanoid_indices());
    } else {
        // Unknown mesh type — default to cube
        std::cerr << "[map_loader] Unknown mesh type '" << mesh_type << "', defaulting to cube\n";
        index = engine.upload_mesh(cube_vertices(), cube_indices());
    }

    cache[mesh_type] = index;
    return index;
}

// Create a material from JSON data
static uint32_t create_material_from_json(const json& mat, VulkanEngine& engine) {
    MaterialUBO props{};
    if (mat.contains("base_color"))
        props.base_color = read_vec4(mat["base_color"], glm::vec4(1.0f));
    if (mat.contains("emissive"))
        props.emissive = read_vec4(mat["emissive"], glm::vec4(0.0f));
    if (mat.contains("metallic"))
        props.metallic = mat["metallic"].get<float>();
    if (mat.contains("roughness"))
        props.roughness = mat["roughness"].get<float>();

    return engine.create_material(props);
}

MapLoadResult load_map(const std::filesystem::path& path, flecs::world& world,
                       VulkanEngine& engine, PhysicsWorld& physics) {
    MapLoadResult result;

    // Read and parse JSON file
    std::ifstream file(path);
    if (!file.is_open()) {
        result.error = "Failed to open map file: " + path.string();
        return result;
    }

    json map_data;
    try {
        map_data = json::parse(file);
    } catch (const json::parse_error& e) {
        result.error = std::string("JSON parse error: ") + e.what();
        return result;
    }

    // Validate version
    if (!map_data.contains("version") || map_data["version"].get<int>() != 1) {
        result.error = "Unsupported or missing map version (expected 1)";
        return result;
    }

    // Read settings
    if (map_data.contains("settings")) {
        const auto& settings = map_data["settings"];
        // ambient_color and gravity are available for future use;
        // the scene UBO and physics world handle these through their own systems
        (void)settings;
    }

    // Mesh cache to avoid duplicate uploads
    std::unordered_map<std::string, uint32_t> mesh_cache;

    // Process entities
    if (!map_data.contains("entities") || !map_data["entities"].is_array()) {
        result.error = "Map file missing 'entities' array";
        return result;
    }

    for (const auto& ent_data : map_data["entities"]) {
        std::string name = ent_data.value("name", "unnamed");
        std::string type = ent_data.value("type", "geometry");

        // Check for player_spawn — extract position but don't create an entity
        if (type == "player_spawn") {
            if (ent_data.contains("transform") && ent_data["transform"].contains("position")) {
                result.player_spawn_position = read_vec3(ent_data["transform"]["position"],
                                                         glm::vec3(0.0f, 2.0f, 0.0f));
            }
            continue;
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

        // Mesh
        if (ent_data.contains("mesh")) {
            std::string mesh_type = ent_data["mesh"].get<std::string>();
            uint32_t mesh_index = get_or_upload_mesh(mesh_type, engine, mesh_cache);
            entity.set(MeshHandle{.index = mesh_index});
        }

        // Material
        if (ent_data.contains("material")) {
            uint32_t mat_index = create_material_from_json(ent_data["material"], engine);
            entity.set(MaterialHandle{.index = mat_index});
        }

        // Physics
        if (ent_data.contains("physics")) {
            const auto& phys = ent_data["physics"];
            std::string shape = phys.value("shape", "box");
            std::string phys_type = phys.value("type", "static");
            bool is_static = (phys_type == "static");

            // Get position and rotation from transform for physics body placement
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
            // Also pass entity id for static bodies with health (targets)
            if (is_static && ent_data.contains("health"))
                eid = entity.id();

            if (shape == "box") {
                glm::vec3 half_extents = read_vec3(phys["half_extents"], glm::vec3(0.5f));
                physics.add_box(half_extents, pos, rot, is_static, eid);
            } else if (shape == "sphere") {
                float radius = phys.value("radius", 0.5f);
                physics.add_sphere(radius, pos, is_static, eid);
            }
        }

        // DirectionalLight
        if (ent_data.contains("directional_light")) {
            const auto& dl = ent_data["directional_light"];
            DirectionalLight light;
            if (dl.contains("direction"))
                light.direction = read_vec3(dl["direction"], light.direction);
            if (dl.contains("color"))
                light.color = read_vec3(dl["color"], light.color);
            if (dl.contains("intensity"))
                light.intensity = dl["intensity"].get<float>();
            entity.set(light);
        }

        // PointLight
        if (ent_data.contains("point_light")) {
            const auto& pl = ent_data["point_light"];
            PointLight light;
            if (pl.contains("color"))
                light.color = read_vec3(pl["color"], light.color);
            if (pl.contains("intensity"))
                light.intensity = pl["intensity"].get<float>();
            if (pl.contains("radius"))
                light.radius = pl["radius"].get<float>();
            entity.set(light);
        }

        // Health
        if (ent_data.contains("health")) {
            const auto& h = ent_data["health"];
            Health health;
            if (h.contains("current"))
                health.current = h["current"].get<float>();
            if (h.contains("max"))
                health.max = h["max"].get<float>();
            entity.set(health);
        }

        // AIState
        if (ent_data.contains("ai_state")) {
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

        // Enemy tag — set for enemy_spawn entities or those with ai_state
        if (type == "enemy_spawn" || ent_data.contains("ai_state")) {
            entity.add<Enemy>();
        }

        // AngularVelocity
        if (ent_data.contains("angular_velocity")) {
            const auto& av = ent_data["angular_velocity"];
            AngularVelocity angular;
            if (av.contains("axis"))
                angular.axis = read_vec3(av["axis"], angular.axis);
            if (av.contains("speed"))
                angular.speed = av["speed"].get<float>();
            entity.set(angular);
        }

        // ParticleEmitter
        if (ent_data.contains("particle_emitter")) {
            const auto& pe = ent_data["particle_emitter"];
            ParticleEmitter emitter;
            if (pe.contains("burst_count"))
                emitter.burst_count = pe["burst_count"].get<int>();
            if (pe.contains("emit_timer"))
                emitter.emit_timer = pe["emit_timer"].get<float>();
            if (pe.contains("emit_interval"))
                emitter.emit_interval = pe["emit_interval"].get<float>();
            if (pe.contains("start_color"))
                emitter.start_color = read_vec4(pe["start_color"], emitter.start_color);
            if (pe.contains("end_color"))
                emitter.end_color = read_vec4(pe["end_color"], emitter.end_color);
            if (pe.contains("start_size"))
                emitter.start_size = pe["start_size"].get<float>();
            if (pe.contains("end_size"))
                emitter.end_size = pe["end_size"].get<float>();
            if (pe.contains("lifetime"))
                emitter.lifetime = pe["lifetime"].get<float>();
            if (pe.contains("speed"))
                emitter.speed = pe["speed"].get<float>();
            if (pe.contains("active"))
                emitter.active = pe["active"].get<bool>();
            entity.set(emitter);
        }

        // SpawnerConfig
        if (ent_data.contains("spawner_config")) {
            const auto& sc = ent_data["spawner_config"];
            SpawnerConfig config;
            if (sc.contains("spawn_interval"))
                config.spawn_interval = sc["spawn_interval"].get<float>();
            if (sc.contains("spawn_radius"))
                config.spawn_radius = sc["spawn_radius"].get<float>();
            if (sc.contains("max_count"))
                config.max_enemies = sc["max_count"].get<int>();
            // enemy_mesh and enemy_material need mesh/material indices
            if (sc.contains("enemy_mesh")) {
                std::string mesh_type = sc["enemy_mesh"].get<std::string>();
                config.enemy_mesh = get_or_upload_mesh(mesh_type, engine, mesh_cache);
            }
            if (sc.contains("enemy_material")) {
                config.enemy_material = create_material_from_json(sc["enemy_material"], engine);
            }
            entity.set(config);
            entity.set(SpawnerState{.timer = config.spawn_interval * 0.5f});
        }
    }

    result.success = true;
    return result;
}
