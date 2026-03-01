#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cstdint>
#include <optional>

// Forward declare Jolt types to keep the header light
namespace JPH {
    class PhysicsSystem;
    class TempAllocatorImpl;
    class JobSystemThreadPool;
    class BodyInterface;
    class CharacterVirtual;
}

struct RaycastResult {
    glm::vec3 hit_position;
    glm::vec3 hit_normal;
    float distance;
    uint64_t entity_id; // 0 if no entity mapping
};

class PhysicsWorld {
public:
    void init();
    void shutdown();
    void step(float dt);

    // Body creation — returns Jolt body ID (as uint32_t)
    uint32_t add_box(glm::vec3 half_extents, glm::vec3 position, glm::quat rotation,
                     bool is_static, uint64_t entity_id = 0);
    uint32_t add_sphere(float radius, glm::vec3 position, bool is_static, uint64_t entity_id = 0);

    void remove_body(uint32_t body_id);

    // Body state
    glm::vec3 get_position(uint32_t body_id) const;
    glm::quat get_rotation(uint32_t body_id) const;
    void set_linear_velocity(uint32_t body_id, glm::vec3 velocity);

    // Character controller
    void create_character(glm::vec3 position, float radius, float height);
    void update_character(float dt, glm::vec3 desired_velocity);
    glm::vec3 get_character_position() const;
    bool is_character_grounded() const;
    void destroy_character();

    // Raycasting
    std::optional<RaycastResult> raycast(glm::vec3 origin, glm::vec3 direction, float max_distance) const;

private:
    JPH::PhysicsSystem* _physics_system = nullptr;
    JPH::TempAllocatorImpl* _temp_allocator = nullptr;
    JPH::JobSystemThreadPool* _job_system = nullptr;
    JPH::CharacterVirtual* _character = nullptr;
};
