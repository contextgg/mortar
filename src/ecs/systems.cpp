#include "ecs/systems.h"
#include "ecs/systems/movement_systems.h"
#include "ecs/systems/camera_systems.h"
#include "ecs/systems/render_systems.h"
#include "ecs/systems/physics_systems.h"
#include "ecs/systems/combat_systems.h"
#include "ecs/systems/ai_systems.h"
#include "ecs/systems/particle_systems.h"
#include "ecs/systems/hud_systems.h"

void register_systems(flecs::world& world, VulkanEngine& engine, Input& input, PhysicsWorld* physics) {
    register_movement_systems(world);
    register_camera_systems(world, input);

    if (physics) {
        register_physics_systems(world, *physics);
        register_combat_systems(world, *physics);
    }

    register_ai_systems(world);
    register_particle_systems(world, engine);
    register_render_systems(world, engine);
    register_hud_systems(world);
}
