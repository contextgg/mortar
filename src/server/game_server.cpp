#include "server/game_server.h"
#include "net/net_common.h"
#include "ecs/systems.h"

#include <iostream>
#include <chrono>
#include <thread>

void GameServer::init(uint16_t port, const std::string& /*map_path*/) {
    net_init();

    _physics.init();

    // Register shared systems (movement, physics, combat, AI — no rendering)
    register_shared_systems(_world, &_physics);

    // Set up physics ground plane
    _physics.add_box(glm::vec3(50.0f, 0.1f, 50.0f), glm::vec3(0, -0.1f, 0),
                     glm::quat(1, 0, 0, 0), true);

    // TODO: load map entities here when map_path is provided

    // Wire up networking callbacks
    _net.on_player_connect([this](uint8_t slot, const std::string& user_id) {
        on_player_connect(slot, user_id);
    });
    _net.on_player_disconnect([this](uint8_t slot) {
        on_player_disconnect(slot);
    });

    _net.start(port);
    _running = true;

    std::cout << "[SERVER] Game server initialized" << std::endl;
}

void GameServer::run() {
    using clock = std::chrono::steady_clock;
    using namespace std::chrono;

    auto next_tick = clock::now();

    while (_running) {
        _net.poll();
        tick(SERVER_TICK_DELTA);

        next_tick += microseconds(static_cast<int64_t>(SERVER_TICK_DELTA * 1'000'000));
        auto now = clock::now();
        if (next_tick > now) {
            std::this_thread::sleep_until(next_tick);
        } else {
            // Falling behind — skip to now
            next_tick = now;
        }
    }
}

void GameServer::stop() {
    _running = false;
    _net.stop();
    _physics.shutdown();
    net_shutdown();
    std::cout << "[SERVER] Stopped" << std::endl;
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

    // Write input into ECS PlayerInput component
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

        const auto* transform = _slots[i].entity.get<Transform>();
        const auto* health = _slots[i].entity.get<Health>();

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

void GameServer::on_player_connect(uint8_t slot, const std::string& user_id) {
    std::cout << "[SERVER] Player " << user_id << " joined slot " << (int)slot << std::endl;

    // Create a player entity in the ECS world
    glm::vec3 spawn_pos{static_cast<float>(slot) * 3.0f, 2.0f, 0.0f};

    auto entity = _world.entity()
        .set(Transform{.position = spawn_pos})
        .set(Player{})
        .set(PlayerInput{})
        .set(MovementState{})
        .set(CharacterControllerTag{})
        .set(Health{.current = 100.0f, .max = 100.0f});

    _slots[slot].entity = entity;
    _slots[slot].occupied = true;
}

void GameServer::on_player_disconnect(uint8_t slot) {
    std::cout << "[SERVER] Player left slot " << (int)slot << std::endl;

    if (_slots[slot].occupied && _slots[slot].entity.is_alive()) {
        _slots[slot].entity.destruct();
    }
    _slots[slot].occupied = false;
}
