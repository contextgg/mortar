#pragma once

#include <flecs.h>

class PhysicsWorld;

// Register shared systems (movement, physics, combat, AI) — used by both client and server
void register_shared_systems(flecs::world& world, PhysicsWorld* physics = nullptr);
