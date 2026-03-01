#include "ecs/systems/ai_systems.h"
#include "ecs/components.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cmath>

void register_ai_systems(flecs::world& world) {
    // AISystem — Enemy FSM: Idle → Chase → Attack → Dead
    world.system<Transform, AIState, const Enemy>("AISystem")
        .kind(flecs::OnUpdate)
        .each([&world](flecs::entity e, Transform& t, AIState& ai, const Enemy&) {
            // Check if dead
            if (e.has<Health>()) {
                const Health& h = e.get<Health>();
                if (h.current <= 0.0f) {
                    ai.state = AIStateType::Dead;
                }
            }

            if (ai.state == AIStateType::Dead) {
                // Sink into ground then destroy
                t.position.y -= world.delta_time() * 2.0f;
                if (t.position.y < -2.0f) {
                    e.destruct();
                }
                return;
            }

            // Find player position
            glm::vec3 player_pos{0.0f};
            bool player_found = false;
            world.each([&](const Player&, const Transform& pt) {
                player_pos = pt.position;
                player_found = true;
            });

            if (!player_found) return;

            glm::vec3 to_player = player_pos - t.position;
            to_player.y = 0.0f; // Horizontal only
            float dist = glm::length(to_player);

            // Reduce attack cooldown
            ai.cooldown_timer -= world.delta_time();
            if (ai.cooldown_timer < 0.0f) ai.cooldown_timer = 0.0f;

            // State transitions
            switch (ai.state) {
            case AIStateType::Idle:
                if (dist < ai.detect_range) {
                    ai.state = AIStateType::Chase;
                }
                break;

            case AIStateType::Chase:
                if (dist < ai.attack_range) {
                    ai.state = AIStateType::Attack;
                } else if (dist > ai.detect_range * 1.5f) {
                    ai.state = AIStateType::Idle;
                } else {
                    // Move toward player
                    glm::vec3 dir = glm::normalize(to_player);
                    t.position += dir * ai.move_speed * world.delta_time();

                    // Face player
                    float angle = std::atan2(dir.x, dir.z);
                    t.rotation = glm::angleAxis(angle, glm::vec3(0, 1, 0));
                }
                break;

            case AIStateType::Attack:
                if (dist > ai.attack_range * 1.5f) {
                    ai.state = AIStateType::Chase;
                } else if (ai.cooldown_timer <= 0.0f) {
                    // Deal damage to player
                    ai.cooldown_timer = ai.attack_cooldown;
                    world.each([](const Player&, Health& h) {
                        h.current -= 10.0f;
                        if (h.current < 0.0f) h.current = 0.0f;
                    });
                }
                break;

            default:
                break;
            }
        });
}
