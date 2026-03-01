#include "ecs/systems/particle_systems.h"
#include "ecs/components.h"
#include "core/engine.h"

#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <cstdlib>

void register_particle_systems(flecs::world& world, VulkanEngine& engine) {
    // ParticleEmitSystem — spawn particles from emitters
    world.system<const Transform, ParticleEmitter>("ParticleEmitSystem")
        .kind(flecs::OnStore)
        .each([&world](flecs::entity e, const Transform& t, ParticleEmitter& emitter) {
            if (!emitter.active) return;

            bool should_emit = false;
            if (emitter.emit_interval <= 0.0f) {
                // One-shot: emit once then deactivate
                should_emit = true;
                emitter.active = false;
            } else {
                emitter.emit_timer -= world.delta_time();
                if (emitter.emit_timer <= 0.0f) {
                    emitter.emit_timer = emitter.emit_interval;
                    should_emit = true;
                }
            }

            if (!should_emit) return;

            for (int i = 0; i < emitter.burst_count; i++) {
                // Random direction
                float theta = (static_cast<float>(rand()) / RAND_MAX) * 6.28318f;
                float phi = (static_cast<float>(rand()) / RAND_MAX) * 3.14159f * 0.5f;
                float speed = emitter.speed * (0.5f + 0.5f * static_cast<float>(rand()) / RAND_MAX);

                glm::vec3 dir{
                    std::cos(theta) * std::sin(phi),
                    std::cos(phi),
                    std::sin(theta) * std::sin(phi),
                };

                world.entity()
                    .set(Transform{.position = t.position, .scale = glm::vec3(emitter.start_size)})
                    .set(Velocity{.linear = dir * speed})
                    .set(Particle{
                        .start_color = emitter.start_color,
                        .end_color = emitter.end_color,
                        .start_size = emitter.start_size,
                        .end_size = emitter.end_size,
                        .lifetime = emitter.lifetime,
                    })
                    .set(DestroyAfter{.time_remaining = emitter.lifetime});
            }
        });

    // ParticleUpdateSystem — interpolate color/size over lifetime
    world.system<Transform, Particle>("ParticleUpdateSystem")
        .kind(flecs::OnStore)
        .each([&world](Transform& t, Particle& p) {
            p.age += world.delta_time();
            float frac = p.age / p.lifetime;
            if (frac > 1.0f) frac = 1.0f;

            float size = glm::mix(p.start_size, p.end_size, frac);
            t.scale = glm::vec3(size);
        });

    // ParticleRenderCollectSystem — render particles as small cubes
    // Particles use mesh index 0 (cube) and a special particle material
    world.system<const Transform, const Particle>("ParticleRenderCollectSystem")
        .kind(flecs::OnStore)
        .each([&engine, &world](const Transform& t, const Particle& p) {
            float frac = p.age / p.lifetime;
            if (frac > 1.0f) frac = 1.0f;

            glm::vec4 color = glm::mix(p.start_color, p.end_color, frac);

            // Use default material (index 0) — particles get their color from vertex color
            // but since we can't easily change vertex color per-instance with current setup,
            // we just render them as small cubes with the default material
            if (t.scale.x > 0.01f) {
                glm::mat4 model = glm::translate(glm::mat4(1.0f), t.position)
                                * glm::scale(glm::mat4(1.0f), t.scale);

                engine.push_renderable({
                    .model = model,
                    .mesh_index = 0, // cube mesh (must be uploaded as index 0)
                    .material_index = 0, // default material
                });
            }
        });
}
