#pragma once

#include <flecs.h>

// Register shared systems (movement, physics, combat, AI) — used by both client and server.
// Requires PhysicsRef singleton to be set before calling.
void register_shared_systems(flecs::world& world);
