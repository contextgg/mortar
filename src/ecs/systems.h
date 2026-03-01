#pragma once

#include <flecs.h>

class VulkanEngine;
class Input;
class PhysicsWorld;

void register_systems(flecs::world& world, VulkanEngine& engine, Input& input, PhysicsWorld* physics = nullptr);
