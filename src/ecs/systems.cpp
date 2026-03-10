#include "ecs/systems.h"
#include "ecs/systems/movement_systems.h"
#include "ecs/systems/physics_systems.h"
#include "ecs/systems/combat_systems.h"
#include "ecs/systems/ai_systems.h"
#include "ecs/components.h"

void register_shared_systems(flecs::world& world) {
    register_movement_systems(world);

    // Physics and combat systems require PhysicsRef singleton
    if (world.has<PhysicsRef>()) {
        register_physics_systems(world);
        register_combat_systems(world);
    }

    register_ai_systems(world);
}
