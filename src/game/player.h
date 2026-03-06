#pragma once

#include <flecs.h>
#include "ecs/components.h"
#include "physics/physics_world.h"

// Create a player entity with physics, mesh, material, weapon, etc.
flecs::entity create_player(flecs::world& world, PhysicsWorld& physics,
                             uint32_t mesh, uint32_t material);
