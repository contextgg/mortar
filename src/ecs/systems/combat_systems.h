#pragma once

#include <flecs.h>

class PhysicsWorld;
class VulkanEngine;

void register_combat_systems(flecs::world& world, PhysicsWorld& physics);
