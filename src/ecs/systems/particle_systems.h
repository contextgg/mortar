#pragma once

#include <flecs.h>

class VulkanEngine;

void register_particle_systems(flecs::world& world, VulkanEngine& engine);
