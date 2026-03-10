#include "game/player.h"
#include "game/weapon.h"
#include "ecs/components.h"
#include "physics/physics_world.h"

flecs::entity create_player(flecs::world& world, uint32_t mesh, uint32_t material) {
    glm::vec3 spawn_pos{0.0f, 2.0f, 0.0f};

    auto* physics = world.get<PhysicsRef>().ptr;
    physics->create_character(spawn_pos, 0.35f, 1.8f);

    return world.entity("Player")
        .add<Player>()
        .add<CharacterControllerTag>()
        .set(Transform{.position = spawn_pos})
        .set(PlayerInput{})
        .set(MovementState{})
        .set(MeshHandle{.index = mesh})
        .set(MaterialHandle{.index = material})
        .set(Health{.current = 100.0f, .max = 100.0f})
        .set(make_bow());
}
