#pragma once

#include <flecs.h>
#include <array>
#include <string>
#include <vector>

#include "net/net_server.h"
#include "net/protocol.h"
#include "physics/physics_world.h"
#include "ecs/components.h"

// Authoritative game server: runs the simulation headlessly
// and sends snapshots to connected clients.
class GameServer {
public:
    void init(uint16_t port, const std::string& session_id = "",
              const std::string& api_base = "https://api.ctx.gg",
              const std::string& server_secret = "");
    void run();  // Blocking tick loop
    void stop();

    bool is_running() const { return _running; }

private:
    void tick(float dt);
    void apply_player_input(uint8_t slot);
    void broadcast_snapshot();
    void check_game_start();

    void on_player_connect(uint8_t slot, const std::string& token);
    void on_player_disconnect(uint8_t slot);

    bool download_map(const std::string& map_slug);
    bool claim_session();
    void finish_session();

    NetServer _net;
    PhysicsWorld _physics;
    flecs::world _world;

    uint32_t _tick = 0;
    bool _running = false;
    bool _game_started = false;

    std::string _session_id;
    std::string _api_base;
    std::string _server_secret;
    std::string _map_slug;
    uint16_t _port = DEFAULT_SERVER_PORT;

    // Expected player tokens (from API session)
    struct ExpectedPlayer {
        std::string user_id;
        std::string token;
    };
    std::vector<ExpectedPlayer> _expected_players;

    // Per-slot ECS entities for players
    struct PlayerSlot {
        flecs::entity entity;
        bool occupied = false;
        std::string user_id;
    };
    std::array<PlayerSlot, MAX_PLAYERS> _slots{};
};
