#pragma once

#include "map/map_common.h"

class VulkanEngine;

// Client-side map loading: loads everything (visuals + physics + gameplay).
// Requires EngineRef and PhysicsRef singletons to be set before calling.
MapLoadResult load_map(const std::filesystem::path& path, flecs::world& world,
                       VulkanEngine& engine);
