#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>

struct Joint {
    int parent = -1; // -1 for root joints
    glm::mat4 inverse_bind_matrix{1.0f};
    glm::vec3 local_position{0.0f};
    glm::quat local_rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 local_scale{1.0f};
    std::string name;
};

struct PositionKey {
    float time;
    glm::vec3 value;
};

struct RotationKey {
    float time;
    glm::quat value;
};

struct ScaleKey {
    float time;
    glm::vec3 value;
};

struct JointAnimation {
    int joint_index = -1;
    std::vector<PositionKey> positions;
    std::vector<RotationKey> rotations;
    std::vector<ScaleKey> scales;
};

struct AnimationClip {
    std::string name;
    float duration = 0.0f;
    std::vector<JointAnimation> channels;
};

struct SkeletonData {
    std::vector<Joint> joints;
    std::vector<AnimationClip> clips;
};
