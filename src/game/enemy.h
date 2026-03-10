#pragma once

#include <flecs.h>
#include <glm/glm.hpp>
#include <memory>

#include "animation/skeleton.h"

// Requires PhysicsRef singleton to be set.
flecs::entity create_enemy(flecs::world& world, glm::vec3 position,
                            uint32_t mesh, uint32_t material,
                            std::shared_ptr<SkeletonData> skeleton = nullptr);
