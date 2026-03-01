#pragma once

#include <flecs.h>
#include "ecs/components.h"
#include "physics/physics_world.h"

class VulkanEngine;

flecs::entity create_player(flecs::world& world, VulkanEngine& engine,
                             PhysicsWorld& physics, uint32_t mesh, uint32_t material);
