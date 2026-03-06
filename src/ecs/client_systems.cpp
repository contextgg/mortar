#include "ecs/client_systems.h"
#include "ecs/systems.h"
#include "ecs/systems/camera_systems.h"
#include "ecs/systems/render_systems.h"
#include "ecs/systems/particle_systems.h"
#include "ecs/systems/hud_systems.h"

void register_client_systems(flecs::world& world, VulkanEngine& engine, Input& input, PhysicsWorld* physics) {
    register_shared_systems(world, physics);
    register_camera_systems(world, input);
    register_particle_systems(world, engine);
    register_render_systems(world, engine);
    register_hud_systems(world);
}
