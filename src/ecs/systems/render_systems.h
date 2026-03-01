#pragma once

#include <flecs.h>

class VulkanEngine;

void register_render_systems(flecs::world& world, VulkanEngine& engine);
