#pragma once

#include <flecs.h>

// Register all client systems (shared + rendering + camera + particles + HUD + animation + effects).
// Requires InputRef, PhysicsRef, EngineRef, AudioRef singletons to be set before calling.
void register_client_systems(flecs::world& world);
