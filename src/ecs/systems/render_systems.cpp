#include "ecs/systems/render_systems.h"
#include "ecs/components.h"
#include "core/engine.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

void register_render_systems(flecs::world& world) {
    // SceneUBOUpdateSystem — builds SceneUBO from camera + lights
    world.system<const Transform, const Camera>("SceneUBOUpdateSystem")
        .kind(flecs::OnStore)
        .each([&world](const Transform& cam_t, const Camera& cam) {
            auto* engine = world.get<EngineRef>().ptr;

            // Look up player position for the look-at target
            glm::vec3 target_pos{0.0f};
            auto player = world.lookup("Player");
            if (player.is_alive()) {
                const auto* pt = player.try_get<Transform>();
                if (pt) target_pos = pt->position;
            }
            glm::vec3 look_target = target_pos + cam.target_offset;

            float aspect = static_cast<float>(engine->width()) / static_cast<float>(engine->height());

            glm::mat4 view = glm::lookAt(cam_t.position, look_target, glm::vec3(0, 1, 0));
            glm::mat4 projection = glm::perspective(glm::radians(cam.fov), aspect, cam.near_plane, cam.far_plane);
            projection[1][1] *= -1;

            SceneUBO ubo{};
            ubo.view = view;
            ubo.projection = projection;
            ubo.view_projection = projection * view;
            ubo.camera_position = glm::vec4(cam_t.position, 1.0f);
            ubo.ambient_color = glm::vec4(0.15f, 0.15f, 0.2f, 1.0f);

            // Gather directional light
            world.each([&ubo](const DirectionalLight& dl) {
                ubo.sun_direction = glm::vec4(glm::normalize(dl.direction), 0.0f);
                ubo.sun_color = glm::vec4(dl.color, dl.intensity);
            });

            // Gather point lights
            int count = 0;
            world.each([&ubo, &count](const Transform& t, const PointLight& pl) {
                if (count >= 4) return;
                ubo.point_positions[count] = glm::vec4(t.position, pl.radius);
                ubo.point_colors[count] = glm::vec4(pl.color, pl.intensity);
                count++;
            });
            ubo.num_point_lights = count;

            engine->update_scene_ubo(ubo);
        });

    // RenderCollectSystem — pushes renderables to engine, handles bone upload
    world.system<const Transform, const MeshHandle, const MaterialHandle>("RenderCollectSystem")
        .kind(flecs::OnStore)
        .each([&world](flecs::entity e, const Transform& t, const MeshHandle& mh, const MaterialHandle& mat) {
            auto* engine = world.get<EngineRef>().ptr;

            glm::mat4 model = glm::translate(glm::mat4(1.0f), t.position)
                            * glm::mat4_cast(t.rotation)
                            * glm::scale(glm::mat4(1.0f), t.scale);

            uint32_t bone_offset = 0;
            uint32_t bone_count = 0;

            const auto* anim = e.try_get<AnimatedModel>();
            if (anim && !anim->bone_matrices.empty()) {
                bone_count = static_cast<uint32_t>(anim->bone_matrices.size());
                bone_offset = engine->upload_bones(anim->bone_matrices.data(), bone_count);
            }

            engine->push_renderable({
                .model = model,
                .mesh_index = mh.index,
                .material_index = mat.index,
                .bone_offset = bone_offset,
                .bone_count = bone_count,
            });
        });
}
