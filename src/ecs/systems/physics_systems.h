#pragma once

#include <flecs.h>

class PhysicsWorld;

void register_physics_systems(flecs::world& world, PhysicsWorld& physics);
