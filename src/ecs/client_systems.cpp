#include "ecs/client_systems.h"
#include "ecs/systems.h"
#include "ecs/systems/camera_systems.h"
#include "ecs/systems/render_systems.h"
#include "ecs/systems/particle_systems.h"
#include "ecs/systems/hud_systems.h"
#include "animation/animation_system.h"
#include "ecs/components.h"
#include "audio/audio_engine.h"
#include "core/engine.h"

void register_client_systems(flecs::world& world) {
    register_shared_systems(world);
    register_camera_systems(world);
    register_animation_systems(world);
    register_particle_systems(world);
    register_render_systems(world);
    register_hud_systems(world);

    // ShootEffectsSystem — spawn muzzle flash particles + play sound on weapon fire
    world.system<const Player, const Transform, Weapon>("ShootEffectsSystem")
        .kind(flecs::OnUpdate)
        .each([](flecs::entity e, const Player&, const Transform& t, Weapon& w) {
            if (!w.fired_this_frame) return;
            w.fired_this_frame = false;

            auto world = e.world();

            // Play shoot sound
            if (world.has<AudioRef>() && world.has<AudioSounds>()) {
                auto* audio = world.get<AudioRef>().ptr;
                const auto& sounds = world.get<AudioSounds>();
                audio->play(sounds.shoot);
            }

            // Spawn muzzle flash particle emitter
            world.entity()
                .set(Transform{.position = t.position + glm::vec3(0, 0.5f, 0)})
                .set(ParticleEmitter{
                    .burst_count = 5,
                    .start_color = glm::vec4(1.0f, 0.8f, 0.2f, 1.0f),
                    .end_color = glm::vec4(1.0f, 0.2f, 0.0f, 0.0f),
                    .start_size = 0.15f,
                    .end_size = 0.0f,
                    .lifetime = 0.3f,
                    .speed = 3.0f,
                });
        });

    // AudioListenerSystem — update spatial audio listener position from camera
    world.system<const Camera, const Transform>("AudioListenerSystem")
        .kind(flecs::OnStore)
        .each([](flecs::entity e, const Camera&, const Transform& t) {
            auto world = e.world();
            if (!world.has<AudioRef>()) return;
            auto* audio = world.get<AudioRef>().ptr;
            audio->set_listener_position(t.position.x, t.position.y, t.position.z);
        });
}
