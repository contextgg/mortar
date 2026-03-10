#pragma once

#include <flecs.h>
#include <cstdint>

// Create a player entity with physics, mesh, material, weapon, etc.
// Requires PhysicsRef singleton to be set.
flecs::entity create_player(flecs::world& world, uint32_t mesh, uint32_t material);
