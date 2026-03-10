#include "map/map_loader.h"

#include <iostream>
#include <unordered_map>

#include "core/engine.h"
#include "core/types.h"
#include "renderer/mesh.h"

using json = nlohmann::json;

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
    json map_data;

    if (!parse_map_file(path, map_data, result))
        return result;

    // Load shared gameplay entities (physics, health, AI, spawners)
    load_map_entities_shared(map_data["entities"], world, physics, result);

    // Now load visual-only data: meshes, materials, lights, particles
    std::unordered_map<std::string, uint32_t> mesh_cache;

    for (const auto& ent_data : map_data["entities"]) {
        std::string name = ent_data.value("name", "unnamed");
        std::string type = ent_data.value("type", "geometry");

        if (type == "player_spawn")
            continue;

        // Find or create the entity (shared loader already created entities with
        // physics/health/AI, so entity() with the same name returns the existing one)
        auto entity = world.entity(name.c_str());

        // Transform — set for visual-only entities that shared loader skipped
        bool has_physics = ent_data.contains("physics");
        bool has_health = ent_data.contains("health");
        bool has_ai = ent_data.contains("ai_state");
        bool has_spawner = ent_data.contains("spawner_config");
        bool is_enemy = (type == "enemy_spawn");
        bool was_loaded_by_shared = has_physics || has_health || has_ai || has_spawner || is_enemy;

        if (!was_loaded_by_shared && ent_data.contains("transform")) {
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

        // AngularVelocity (visual-only, but harmless on server too)
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

        // SpawnerConfig — client needs mesh/material indices
        if (has_spawner && entity.has<SpawnerConfig>()) {
            const auto& sc = ent_data["spawner_config"];
            auto& config = entity.get_mut<SpawnerConfig>();
            if (sc.contains("enemy_mesh")) {
                std::string mesh_type = sc["enemy_mesh"].get<std::string>();
                config.enemy_mesh = get_or_upload_mesh(mesh_type, engine, mesh_cache);
            }
            if (sc.contains("enemy_material")) {
                config.enemy_material = create_material_from_json(sc["enemy_material"], engine);
            }
        }
    }

    result.success = true;
    return result;
}
