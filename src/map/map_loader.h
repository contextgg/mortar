#pragma once

#include "map/map_common.h"

class VulkanEngine;

// Client-side map loading: loads everything (visuals + physics + gameplay).
MapLoadResult load_map(const std::filesystem::path& path, flecs::world& world,
                       VulkanEngine& engine, PhysicsWorld& physics);
