#include "server/game_server.h"
#include "net/net_common.h"
#include "ecs/systems.h"
#include "map/map_common.h"

#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include <cstring>
#include <sstream>

// Minimal HTTP GET using raw sockets is too complex — we'll use a simple
// approach: the server is given the map slug and downloads it via the API.
// For now, we shell out to curl. In production, link against a proper HTTP lib.

static std::string http_get(const std::string& url) {
    // Use popen to call curl
    std::string cmd = "curl -sf \"" + url + "\"";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";
    std::string result;
    char buf[4096];
    while (fgets(buf, sizeof(buf), pipe)) {
        result += buf;
    }
    pclose(pipe);
    return result;
}

static std::string http_post(const std::string& url, const std::string& body,
                              const std::string& secret) {
    std::string cmd = "curl -sf -X POST \"" + url + "\" "
                      "-H \"Content-Type: application/json\" "
                      "-H \"X-Server-Secret: " + secret + "\" "
                      "-d '" + body + "'";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";
    std::string result;
    char buf[4096];
    while (fgets(buf, sizeof(buf), pipe)) {
        result += buf;
    }
    pclose(pipe);
    return result;
}

void GameServer::init(uint16_t port, const std::string& session_id,
                       const std::string& api_base, const std::string& server_secret,
                       const std::string& map_path) {
    _port = port;
    _session_id = session_id;
    _api_base = api_base;
    _server_secret = server_secret;

    net_init();
    _physics.init();

    // Set singleton so systems can access physics
    _world.set<PhysicsRef>({&_physics});

    // Register shared systems (movement, physics, combat, AI — no rendering)
    register_shared_systems(_world);

    // If we have a session, fetch session info from the API
    if (!_session_id.empty()) {
        std::string url = _api_base + "/api/matchmaking/sessions/" + _session_id;
        std::string resp = http_get(url + "?X-Server-Secret=" + _server_secret);

        // Actually, we need to pass the secret as a header. Use http_post style.
        // Let's use a GET with header via curl:
        std::string cmd = "curl -sf \"" + url + "\" -H \"X-Server-Secret: " + _server_secret + "\"";
        FILE* pipe = popen(cmd.c_str(), "r");
        std::string session_resp;
        if (pipe) {
            char buf[4096];
            while (fgets(buf, sizeof(buf), pipe)) session_resp += buf;
            pclose(pipe);
        }

        if (!session_resp.empty()) {
            try {
                auto j = nlohmann::json::parse(session_resp);
                _map_slug = j.value("map_slug", "");

                if (j.contains("players") && j["players"].is_array()) {
                    for (const auto& p : j["players"]) {
                        _expected_players.push_back({
                            p.value("user_id", ""),
                            p.value("token", ""),
                        });
                    }
                }

                std::cout << "[SERVER] Session " << _session_id
                          << ": map=" << _map_slug
                          << " expected_players=" << _expected_players.size() << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "[SERVER] Failed to parse session info: " << e.what() << std::endl;
            }
        }

        // Download the map
        if (!_map_slug.empty()) {
            if (!download_map(_map_slug)) {
                std::cerr << "[SERVER] Failed to download map: " << _map_slug << std::endl;
            }
        }

        // Claim the session (tell API our address)
        claim_session();
    } else {
        // Dev mode: load specified map or default
        std::string dev_map = map_path.empty() ? "assets/maps/default.json" : map_path;
        auto map_result = load_map_server(dev_map, _world);
        if (map_result.success) {
            std::cout << "[SERVER] Loaded map: " << dev_map << std::endl;
        } else {
            std::cerr << "[SERVER] Warning: failed to load map (" << map_result.error
                      << "), running with empty world" << std::endl;
        }
    }

    // Wire up networking callbacks
    _net.on_player_connect([this](uint8_t slot, const std::string& token) {
        on_player_connect(slot, token);
    });
    _net.on_player_disconnect([this](uint8_t slot) {
        on_player_disconnect(slot);
    });

    _net.start(_port);
    _running = true;

    std::cout << "[SERVER] Game server initialized on port " << _port << std::endl;
}

bool GameServer::download_map(const std::string& map_slug) {
    std::string url = _api_base + "/api/maps/" + map_slug + "/download";
    std::string map_data = http_get(url);

    if (map_data.empty()) {
        std::cerr << "[SERVER] Empty response downloading map " << map_slug << std::endl;
        return false;
    }

    // Save to a temp file so the map loader can read it
    std::string map_path = "assets/maps/" + map_slug + ".json";

    // Ensure directory exists
    std::string dir = "assets/maps";
    std::string mkdir_cmd = "mkdir -p " + dir;
    system(mkdir_cmd.c_str());

    std::ofstream out(map_path);
    if (!out.is_open()) {
        std::cerr << "[SERVER] Failed to write map file: " << map_path << std::endl;
        return false;
    }
    out << map_data;
    out.close();

    std::cout << "[SERVER] Downloaded map: " << map_slug << " -> " << map_path << std::endl;

    // Load map entities (physics, health, AI, spawners) into the ECS world
    auto map_result = load_map_server(map_path, _world);
    if (!map_result.success) {
        std::cerr << "[SERVER] Failed to load map entities: " << map_result.error << std::endl;
        return false;
    }
    std::cout << "[SERVER] Map entities loaded" << std::endl;
    return true;
}

bool GameServer::claim_session() {
    if (_session_id.empty() || _server_secret.empty()) return false;

    nlohmann::json body;
    body["server_addr"] = "0.0.0.0"; // Will be resolved by the API/Fly.io
    body["server_port"] = _port;

    std::string url = _api_base + "/api/matchmaking/sessions/" + _session_id + "/claim";
    std::string resp = http_post(url, body.dump(), _server_secret);

    if (resp.empty()) {
        std::cerr << "[SERVER] Failed to claim session" << std::endl;
        return false;
    }

    std::cout << "[SERVER] Session claimed" << std::endl;
    return true;
}

void GameServer::finish_session() {
    if (_session_id.empty() || _server_secret.empty()) return;

    std::string url = _api_base + "/api/matchmaking/sessions/" + _session_id + "/finish";
    http_post(url, "{}", _server_secret);
    std::cout << "[SERVER] Session finished" << std::endl;
}

void GameServer::run() {
    using clock = std::chrono::steady_clock;
    using namespace std::chrono;

    auto next_tick = clock::now();

    while (_running) {
        _net.poll();

        if (!_game_started) {
            check_game_start();
        }

        tick(SERVER_TICK_DELTA);

        next_tick += microseconds(static_cast<int64_t>(SERVER_TICK_DELTA * 1'000'000));
        auto now = clock::now();
        if (next_tick > now) {
            std::this_thread::sleep_until(next_tick);
        } else {
            next_tick = now;
        }
    }
}

void GameServer::stop() {
    finish_session();
    _running = false;
    _net.stop();
    _physics.shutdown();
    net_shutdown();
    std::cout << "[SERVER] Stopped" << std::endl;
}

void GameServer::check_game_start() {
    if (_expected_players.empty()) {
        // No session — start immediately (local/dev mode)
        _game_started = true;
        return;
    }

    // Check if all expected players have connected
    int connected = 0;
    for (const auto& slot : _slots) {
        if (slot.occupied) connected++;
    }

    if (connected >= static_cast<int>(_expected_players.size())) {
        _game_started = true;

        // Send GameStart to all clients
        GameStartMsg msg{};
        strncpy(msg.map_slug, _map_slug.c_str(), sizeof(msg.map_slug) - 1);
        msg.player_count = static_cast<uint8_t>(connected);
        msg.countdown_seconds = 3;

        _net.broadcast(MessageType::ServerGameStart, msg.serialize());
        std::cout << "[SERVER] Game starting! Map: " << _map_slug
                  << " Players: " << connected << std::endl;
    }
}

void GameServer::tick(float dt) {
    // Apply latest input from each connected player
    for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
        if (_slots[i].occupied)
            apply_player_input(i);
    }

    // Step the ECS world (runs physics, movement, AI, combat)
    _world.progress(dt);
    _tick++;

    // Broadcast snapshot to all clients
    broadcast_snapshot();
}

void GameServer::apply_player_input(uint8_t slot) {
    const auto& net_player = _net.player(slot);
    if (!net_player.active || !_slots[slot].occupied) return;

    const auto& input = net_player.last_input;
    auto entity = _slots[slot].entity;

    entity.set(PlayerInput{
        .move_dir = input.move_dir,
        .mouse_delta = input.look_delta,
        .fire = (input.buttons & InputSnapshot::BTN_FIRE) != 0,
        .jump = (input.buttons & InputSnapshot::BTN_JUMP) != 0,
        .dodge_left = (input.buttons & InputSnapshot::BTN_DODGE_LEFT) != 0,
        .dodge_right = (input.buttons & InputSnapshot::BTN_DODGE_RIGHT) != 0,
    });
}

void GameServer::broadcast_snapshot() {
    SnapshotMsg snapshot{};
    snapshot.tick = _tick;
    snapshot.player_count = 0;

    for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
        if (!_slots[i].occupied) continue;

        auto& ps = snapshot.players[snapshot.player_count];
        ps.slot = i;

        const auto* transform = _slots[i].entity.try_get<Transform>();
        const auto* health = _slots[i].entity.try_get<Health>();

        if (transform) {
            ps.position = transform->position;
            ps.rotation = transform->rotation;
        }
        ps.velocity = {};
        ps.health = health ? health->current : 0.0f;
        ps.flags = (health && health->current > 0) ? PlayerState::FLAG_ALIVE : 0;

        snapshot.player_count++;
    }

    _net.broadcast_unreliable(MessageType::ServerSnapshot, snapshot.serialize());
}

void GameServer::on_player_connect(uint8_t slot, const std::string& token) {
    // Validate token against expected players if we have a session
    std::string user_id;
    if (!_expected_players.empty()) {
        bool valid = false;
        for (const auto& ep : _expected_players) {
            if (ep.token == token) {
                valid = true;
                user_id = ep.user_id;
                break;
            }
        }
        if (!valid) {
            std::cerr << "[SERVER] Rejected player: invalid token" << std::endl;
            _net.kick(slot, "Invalid session token");
            return;
        }
    } else {
        user_id = token; // Dev mode: use token as user ID
    }

    std::cout << "[SERVER] Player " << user_id << " joined slot " << (int)slot << std::endl;

    // Create a player entity in the ECS world
    glm::vec3 spawn_pos{static_cast<float>(slot) * 3.0f, 2.0f, 0.0f};

    auto entity = _world.entity()
        .set(Transform{.position = spawn_pos})
        .add<Player>()
        .set(PlayerInput{})
        .set(MovementState{})
        .add<CharacterControllerTag>()
        .set(Health{.current = 100.0f, .max = 100.0f});

    _slots[slot].entity = entity;
    _slots[slot].occupied = true;
    _slots[slot].user_id = user_id;
}

void GameServer::on_player_disconnect(uint8_t slot) {
    std::cout << "[SERVER] Player left slot " << (int)slot << std::endl;

    if (_slots[slot].occupied && _slots[slot].entity.is_alive()) {
        _slots[slot].entity.destruct();
    }
    _slots[slot].occupied = false;
    _slots[slot].user_id.clear();

    // If all players disconnected and we have a session, finish it
    if (!_session_id.empty() && _net.player_count() == 0 && _game_started) {
        std::cout << "[SERVER] All players left, finishing session" << std::endl;
        stop();
    }
}
