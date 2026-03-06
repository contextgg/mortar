#include "ecs/systems.h"
#include "ecs/systems/movement_systems.h"
#include "ecs/systems/physics_systems.h"
#include "ecs/systems/combat_systems.h"
#include "ecs/systems/ai_systems.h"

void register_shared_systems(flecs::world& world, PhysicsWorld* physics) {
    register_movement_systems(world);

    if (physics) {
        register_physics_systems(world, *physics);
        register_combat_systems(world, *physics);
    }

    register_ai_systems(world);
}
