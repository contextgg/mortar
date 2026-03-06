#pragma once

#include <flecs.h>

class VulkanEngine;
class Input;
class PhysicsWorld;

// Register all systems for the client (shared + rendering + camera + particles + HUD)
void register_client_systems(flecs::world& world, VulkanEngine& engine, Input& input, PhysicsWorld* physics = nullptr);
