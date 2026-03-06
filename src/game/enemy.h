#pragma once

#include <flecs.h>
#include <glm/glm.hpp>

class PhysicsWorld;

flecs::entity create_enemy(flecs::world& world, PhysicsWorld& physics,
                            glm::vec3 position, uint32_t mesh, uint32_t material);
