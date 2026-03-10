#include "map/map_loader.h"

#include <iostream>
#include <unordered_map>

#include "core/engine.h"
#include "core/types.h"
#include "renderer/mesh.h"
#include "renderer/model_loader.h"

using json = nlohmann::json;

// Cache for loaded glTF models (mesh index + texture material + optional skeleton)
struct CachedModel {
    uint32_t mesh_index = 0;
    uint32_t material_index = 0; // textured material (0 = default)
    std::shared_ptr<SkeletonData> skeleton;
};

static std::unordered_map<std::string, CachedModel> s_model_cache;

// Upload a mesh by name, caching to avoid duplicate uploads.
// If the mesh_type ends in .glb/.gltf, loads via glTF loader.
static CachedModel get_or_upload_model(const std::string& mesh_type, VulkanEngine& engine,
                                        std::unordered_map<std::string, uint32_t>& mesh_cache) {
    // Check model cache first (for glTF models with skeleton data)
    auto model_it = s_model_cache.find(mesh_type);
    if (model_it != s_model_cache.end()) return model_it->second;

    // Check mesh-only cache
    auto it = mesh_cache.find(mesh_type);
    if (it != mesh_cache.end()) return {it->second, 0, nullptr};

    CachedModel result;

    bool is_gltf = mesh_type.ends_with(".glb") || mesh_type.ends_with(".gltf");
    if (is_gltf) {
        auto model = load_gltf(mesh_type);
        if (!model.vertices.empty()) {
            result.mesh_index = engine.upload_mesh(model.vertices, model.indices);
            result.skeleton = model.skeleton;
            // Upload embedded texture and create a textured material
            if (!model.textures.empty() && !model.textures[0].pixels.empty()) {
                const auto& tex = model.textures[0];
                uint32_t tex_idx = engine.upload_texture(tex.pixels.data(), tex.width, tex.height);
                MaterialUBO props{};
                props.base_color = glm::vec4(1.0f);
                props.roughness = 0.5f;
                result.material_index = engine.create_material(props, tex_idx);
            }
        } else {
            std::cerr << "[map_loader] Failed to load model '" << mesh_type << "', defaulting to cube\n";
            result.mesh_index = engine.upload_mesh(cube_vertices(), cube_indices());
        }
        s_model_cache[mesh_type] = result;
    } else if (mesh_type == "cube") {
        result.mesh_index = engine.upload_mesh(cube_vertices(), cube_indices());
    } else if (mesh_type == "plane") {
        result.mesh_index = engine.upload_mesh(plane_vertices(50.0f, 10.0f), plane_indices());
    } else if (mesh_type == "humanoid") {
        result.mesh_index = engine.upload_mesh(humanoid_vertices(), humanoid_indices());
    } else {
        std::cerr << "[map_loader] Unknown mesh type '" << mesh_type << "', defaulting to cube\n";
        result.mesh_index = engine.upload_mesh(cube_vertices(), cube_indices());
    }

    mesh_cache[mesh_type] = result.mesh_index;
    return result;
}

// Legacy helper — returns just the mesh index
static uint32_t get_or_upload_mesh(const std::string& mesh_type, VulkanEngine& engine,
                                   std::unordered_map<std::string, uint32_t>& cache) {
    return get_or_upload_model(mesh_type, engine, cache).mesh_index;
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
                       VulkanEngine& engine) {
    MapLoadResult result;
    json map_data;

    if (!parse_map_file(path, map_data, result))
        return result;

    // Load shared gameplay entities (physics, health, AI, spawners)
    load_map_entities_shared(map_data["entities"], world, result);

    // Now load visual-only data: meshes, materials, lights, particles
    std::unordered_map<std::string, uint32_t> mesh_cache;
    s_model_cache.clear();

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

        // SpawnerConfig — client needs mesh/material indices + optional skeleton
        if (has_spawner && entity.has<SpawnerConfig>()) {
            const auto& sc = ent_data["spawner_config"];
            auto& config = entity.get_mut<SpawnerConfig>();
            if (sc.contains("enemy_mesh")) {
                std::string mesh_type = sc["enemy_mesh"].get<std::string>();
                auto model = get_or_upload_model(mesh_type, engine, mesh_cache);
                config.enemy_mesh = model.mesh_index;
                config.enemy_skeleton = model.skeleton;
                // Use embedded texture material if no explicit material in map JSON
                if (!sc.contains("enemy_material") && model.material_index != 0) {
                    config.enemy_material = model.material_index;
                }
            }
            if (sc.contains("enemy_material")) {
                config.enemy_material = create_material_from_json(sc["enemy_material"], engine);
            }
        }
    }

    result.success = true;
    return result;
}
