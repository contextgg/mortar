#include "animation/animation_system.h"
#include "ecs/components.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cmath>

template <typename Key>
static size_t find_keyframe(const std::vector<Key>& keys, float time) {
    for (size_t i = keys.size() - 1; i > 0; i--) {
        if (keys[i].time <= time) return i;
    }
    return 0;
}

static glm::vec3 sample_position(const std::vector<PositionKey>& keys, float time) {
    if (keys.empty()) return glm::vec3(0.0f);
    if (keys.size() == 1) return keys[0].value;
    size_t i = find_keyframe(keys, time);
    size_t next = (i + 1 < keys.size()) ? i + 1 : i;
    if (i == next) return keys[i].value;
    float dt = keys[next].time - keys[i].time;
    float t = (dt > 0.0f) ? (time - keys[i].time) / dt : 0.0f;
    return glm::mix(keys[i].value, keys[next].value, glm::clamp(t, 0.0f, 1.0f));
}

static glm::quat sample_rotation(const std::vector<RotationKey>& keys, float time) {
    if (keys.empty()) return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    if (keys.size() == 1) return keys[0].value;
    size_t i = find_keyframe(keys, time);
    size_t next = (i + 1 < keys.size()) ? i + 1 : i;
    if (i == next) return keys[i].value;
    float dt = keys[next].time - keys[i].time;
    float t = (dt > 0.0f) ? (time - keys[i].time) / dt : 0.0f;
    return glm::slerp(keys[i].value, keys[next].value, glm::clamp(t, 0.0f, 1.0f));
}

static glm::vec3 sample_scale(const std::vector<ScaleKey>& keys, float time) {
    if (keys.empty()) return glm::vec3(1.0f);
    if (keys.size() == 1) return keys[0].value;
    size_t i = find_keyframe(keys, time);
    size_t next = (i + 1 < keys.size()) ? i + 1 : i;
    if (i == next) return keys[i].value;
    float dt = keys[next].time - keys[i].time;
    float t = (dt > 0.0f) ? (time - keys[i].time) / dt : 0.0f;
    return glm::mix(keys[i].value, keys[next].value, glm::clamp(t, 0.0f, 1.0f));
}

// Find a clip index by searching for a substring in the clip name
static uint32_t find_clip(const SkeletonData& skel, const std::string& search, uint32_t fallback = 0) {
    for (uint32_t i = 0; i < static_cast<uint32_t>(skel.clips.size()); i++) {
        if (skel.clips[i].name.find(search) != std::string::npos) return i;
    }
    return fallback;
}

void register_animation_systems(flecs::world& world) {
    // PlayerAnimationControllerSystem — switches clips based on movement
    world.system<const PlayerInput, const MovementState, AnimatedModel, PlayerAnimClips>("PlayerAnimControllerSystem")
        .kind(flecs::PreStore)
        .each([](const PlayerInput& pi, const MovementState& ms, AnimatedModel& anim, PlayerAnimClips& clips) {
            uint32_t desired_clip = clips.idle;

            bool moving = glm::length(pi.move_dir) > 0.1f;
            bool in_air = ms.vertical_velocity != 0.0f || ms.jumps_remaining < 2;

            if (in_air) {
                desired_clip = clips.jump_start;
            } else if (moving && ms.sprinting) {
                desired_clip = clips.sprint;
            } else if (moving) {
                desired_clip = clips.run;
            }

            if (anim.current_clip != desired_clip) {
                anim.current_clip = desired_clip;
                anim.time = 0.0f;
                anim.looping = (desired_clip != clips.jump_start);
            }
        });

    world.system<AnimatedModel>("AnimationUpdateSystem")
        .kind(flecs::PreStore)
        .each([](flecs::entity e, AnimatedModel& anim) {
            if (!anim.skeleton || !anim.playing) return;

            const auto& skel = *anim.skeleton;
            if (skel.clips.empty()) return;

            uint32_t clip_idx = anim.current_clip % static_cast<uint32_t>(skel.clips.size());
            const auto& clip = skel.clips[clip_idx];

            float dt = e.world().delta_time();
            anim.time += dt * anim.speed;

            if (clip.duration > 0.0f) {
                if (anim.looping) {
                    anim.time = std::fmod(anim.time, clip.duration);
                    if (anim.time < 0.0f) anim.time += clip.duration;
                } else {
                    anim.time = glm::clamp(anim.time, 0.0f, clip.duration);
                }
            }

            size_t joint_count = skel.joints.size();
            if (joint_count == 0) return;

            std::vector<glm::vec3> local_pos(joint_count);
            std::vector<glm::quat> local_rot(joint_count);
            std::vector<glm::vec3> local_scl(joint_count);

            for (size_t j = 0; j < joint_count; j++) {
                local_pos[j] = skel.joints[j].local_position;
                local_rot[j] = skel.joints[j].local_rotation;
                local_scl[j] = skel.joints[j].local_scale;
            }

            for (const auto& ch : clip.channels) {
                if (ch.joint_index < 0 || ch.joint_index >= static_cast<int>(joint_count))
                    continue;
                auto ji = static_cast<size_t>(ch.joint_index);
                if (!ch.positions.empty()) local_pos[ji] = sample_position(ch.positions, anim.time);
                if (!ch.rotations.empty()) local_rot[ji] = sample_rotation(ch.rotations, anim.time);
                if (!ch.scales.empty()) local_scl[ji] = sample_scale(ch.scales, anim.time);
            }

            std::vector<glm::mat4> global(joint_count);
            for (size_t j = 0; j < joint_count; j++) {
                glm::mat4 local = glm::translate(glm::mat4(1.0f), local_pos[j])
                                * glm::mat4_cast(local_rot[j])
                                * glm::scale(glm::mat4(1.0f), local_scl[j]);

                int parent = skel.joints[j].parent;
                if (parent >= 0 && parent < static_cast<int>(joint_count)) {
                    global[j] = global[static_cast<size_t>(parent)] * local;
                } else {
                    global[j] = local;
                }
            }

            anim.bone_matrices.resize(joint_count);
            for (size_t j = 0; j < joint_count; j++) {
                anim.bone_matrices[j] = global[j] * skel.joints[j].inverse_bind_matrix;
            }
        });
}
