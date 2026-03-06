#pragma once

#include <flecs.h>
#include <array>

#include "net/net_server.h"
#include "net/protocol.h"
#include "physics/physics_world.h"
#include "ecs/components.h"

// Authoritative game server: runs the simulation headlessly
// and sends snapshots to connected clients.
class GameServer {
public:
    void init(uint16_t port, const std::string& map_path = "");
    void run();  // Blocking tick loop
    void stop();

    bool is_running() const { return _running; }

private:
    void tick(float dt);
    void apply_player_input(uint8_t slot);
    void broadcast_snapshot();

    void on_player_connect(uint8_t slot, const std::string& user_id);
    void on_player_disconnect(uint8_t slot);

    NetServer _net;
    PhysicsWorld _physics;
    flecs::world _world;

    uint32_t _tick = 0;
    bool _running = false;

    // Per-slot ECS entities for players
    struct PlayerSlot {
        flecs::entity entity;
        bool occupied = false;
    };
    std::array<PlayerSlot, MAX_PLAYERS> _slots{};
};
