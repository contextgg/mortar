#pragma once

#include <flecs.h>

class PhysicsWorld;

void register_combat_systems(flecs::world& world, PhysicsWorld& physics);
