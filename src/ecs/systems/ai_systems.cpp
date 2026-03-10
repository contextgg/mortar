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
            if (e.has<Health>()) {
                const Health& h = e.get<Health>();
                if (h.current <= 0.0f) {
                    ai.state = AIStateType::Dead;
                }
            }

            if (ai.state == AIStateType::Dead) {
                t.position.y -= world.delta_time() * 2.0f;
                if (t.position.y < -2.0f) {
                    e.destruct();
                }
                return;
            }

            // Look up player by name
            auto player = world.lookup("Player");
            if (!player.is_alive()) return;
            const auto* player_t = player.try_get<Transform>();
            if (!player_t) return;

            glm::vec3 to_player = player_t->position - t.position;
            to_player.y = 0.0f;
            float dist = glm::length(to_player);

            ai.cooldown_timer -= world.delta_time();
            if (ai.cooldown_timer < 0.0f) ai.cooldown_timer = 0.0f;

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
                    glm::vec3 dir = glm::normalize(to_player);
                    t.position += dir * ai.move_speed * world.delta_time();
                    float angle = std::atan2(dir.x, dir.z);
                    t.rotation = glm::angleAxis(angle, glm::vec3(0, 1, 0));
                }
                break;

            case AIStateType::Attack:
                if (dist > ai.attack_range * 1.5f) {
                    ai.state = AIStateType::Chase;
                } else if (ai.cooldown_timer <= 0.0f) {
                    ai.cooldown_timer = ai.attack_cooldown;
                    // Damage the player
                    if (player.has<Health>()) {
                        auto& h = player.ensure<Health>();
                        h.current -= 10.0f;
                        if (h.current < 0.0f) h.current = 0.0f;
                    }
                }
                break;

            default:
                break;
            }
        });
}
