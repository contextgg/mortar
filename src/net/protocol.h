#pragma once

#include <cstdint>
#include <cstring>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// Protocol version — bump when wire format changes
constexpr uint16_t PROTOCOL_VERSION = 1;

// Max players per game server
constexpr int MAX_PLAYERS = 16;

// Tick rate (server simulation Hz)
constexpr int SERVER_TICK_RATE = 60;
constexpr float SERVER_TICK_DELTA = 1.0f / SERVER_TICK_RATE;

// Network ports
constexpr uint16_t DEFAULT_SERVER_PORT = 27015;

enum class MessageType : uint8_t {
    // Client -> Server
    ClientHello       = 0x01,  // Initial connection with auth token
    ClientInput       = 0x02,  // Player input snapshot
    ClientDisconnect  = 0x03,

    // Server -> Client
    ServerWelcome     = 0x10,  // Connection accepted, assigned player slot
    ServerDenied      = 0x11,  // Connection rejected (reason string)
    ServerSnapshot    = 0x12,  // Full world state snapshot
    ServerPlayerJoin  = 0x13,  // A player joined
    ServerPlayerLeave = 0x14,  // A player left
    ServerGameStart   = 0x15,  // Game is starting (includes map slug)
    ServerGameOver    = 0x16,  // Game ended (results)
    ServerWaiting     = 0x17,  // Waiting for players (count/total)
};

// Wire format: [uint8_t type][uint16_t payload_len][payload bytes]
// All multi-byte values are little-endian.

struct PacketHeader {
    MessageType type;
    uint16_t payload_length;
};

constexpr size_t PACKET_HEADER_SIZE = 3;
constexpr size_t MAX_PACKET_SIZE = 1200; // Stay under MTU

// --- Message Payloads ---

struct ClientHelloMsg {
    uint16_t protocol_version;
    char auth_token[256]; // JWT or session token from API

    std::vector<uint8_t> serialize() const {
        std::vector<uint8_t> buf(sizeof(protocol_version) + sizeof(auth_token));
        memcpy(buf.data(), &protocol_version, sizeof(protocol_version));
        memcpy(buf.data() + sizeof(protocol_version), auth_token, sizeof(auth_token));
        return buf;
    }

    static ClientHelloMsg deserialize(const uint8_t* data, size_t len) {
        ClientHelloMsg msg{};
        if (len >= sizeof(msg.protocol_version))
            memcpy(&msg.protocol_version, data, sizeof(msg.protocol_version));
        if (len >= sizeof(msg.protocol_version) + sizeof(msg.auth_token))
            memcpy(msg.auth_token, data + sizeof(msg.protocol_version), sizeof(msg.auth_token));
        return msg;
    }
};

struct InputSnapshot {
    uint32_t tick;          // Server tick this input is for
    glm::vec2 move_dir;    // WASD input
    glm::vec2 look_delta;  // Mouse delta
    uint8_t buttons;       // Bitfield: fire=0x01, jump=0x02, dodge_left=0x04, dodge_right=0x08

    static constexpr uint8_t BTN_FIRE        = 0x01;
    static constexpr uint8_t BTN_JUMP        = 0x02;
    static constexpr uint8_t BTN_DODGE_LEFT  = 0x04;
    static constexpr uint8_t BTN_DODGE_RIGHT = 0x08;

    std::vector<uint8_t> serialize() const {
        std::vector<uint8_t> buf(sizeof(InputSnapshot));
        memcpy(buf.data(), this, sizeof(InputSnapshot));
        return buf;
    }

    static InputSnapshot deserialize(const uint8_t* data, size_t) {
        InputSnapshot s{};
        memcpy(&s, data, sizeof(InputSnapshot));
        return s;
    }
};

struct PlayerState {
    uint8_t slot;
    glm::vec3 position;
    glm::quat rotation;
    glm::vec3 velocity;
    float health;
    uint8_t flags; // alive=0x01

    static constexpr uint8_t FLAG_ALIVE = 0x01;
};

struct ServerWelcomeMsg {
    uint8_t your_slot;
    uint32_t server_tick;

    std::vector<uint8_t> serialize() const {
        std::vector<uint8_t> buf(sizeof(your_slot) + sizeof(server_tick));
        buf[0] = your_slot;
        memcpy(buf.data() + 1, &server_tick, sizeof(server_tick));
        return buf;
    }

    static ServerWelcomeMsg deserialize(const uint8_t* data, size_t) {
        ServerWelcomeMsg msg{};
        msg.your_slot = data[0];
        memcpy(&msg.server_tick, data + 1, sizeof(msg.server_tick));
        return msg;
    }
};

struct SnapshotMsg {
    uint32_t tick;
    uint8_t player_count;
    PlayerState players[MAX_PLAYERS];

    std::vector<uint8_t> serialize() const {
        size_t size = sizeof(tick) + sizeof(player_count) +
                      player_count * sizeof(PlayerState);
        std::vector<uint8_t> buf(size);
        size_t offset = 0;
        memcpy(buf.data() + offset, &tick, sizeof(tick)); offset += sizeof(tick);
        buf[offset++] = player_count;
        memcpy(buf.data() + offset, players, player_count * sizeof(PlayerState));
        return buf;
    }

    static SnapshotMsg deserialize(const uint8_t* data, size_t len) {
        SnapshotMsg msg{};
        size_t offset = 0;
        memcpy(&msg.tick, data + offset, sizeof(msg.tick)); offset += sizeof(msg.tick);
        msg.player_count = data[offset++];
        size_t players_size = msg.player_count * sizeof(PlayerState);
        if (offset + players_size <= len) {
            memcpy(msg.players, data + offset, players_size);
        }
        return msg;
    }
};

struct GameStartMsg {
    char map_slug[64];
    uint8_t player_count;
    uint8_t countdown_seconds;

    std::vector<uint8_t> serialize() const {
        std::vector<uint8_t> buf(sizeof(map_slug) + 2);
        memcpy(buf.data(), map_slug, sizeof(map_slug));
        buf[sizeof(map_slug)] = player_count;
        buf[sizeof(map_slug) + 1] = countdown_seconds;
        return buf;
    }

    static GameStartMsg deserialize(const uint8_t* data, size_t len) {
        GameStartMsg msg{};
        if (len >= sizeof(msg.map_slug))
            memcpy(msg.map_slug, data, sizeof(msg.map_slug));
        if (len >= sizeof(msg.map_slug) + 1)
            msg.player_count = data[sizeof(msg.map_slug)];
        if (len >= sizeof(msg.map_slug) + 2)
            msg.countdown_seconds = data[sizeof(msg.map_slug) + 1];
        return msg;
    }
};

// Utility: build a packet from a message type + payload
inline std::vector<uint8_t> build_packet(MessageType type, const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> packet(PACKET_HEADER_SIZE + payload.size());
    packet[0] = static_cast<uint8_t>(type);
    uint16_t len = static_cast<uint16_t>(payload.size());
    memcpy(packet.data() + 1, &len, sizeof(len));
    memcpy(packet.data() + PACKET_HEADER_SIZE, payload.data(), payload.size());
    return packet;
}

// Utility: parse header from raw bytes
inline bool parse_header(const uint8_t* data, size_t len, PacketHeader& header) {
    if (len < PACKET_HEADER_SIZE) return false;
    header.type = static_cast<MessageType>(data[0]);
    memcpy(&header.payload_length, data + 1, sizeof(header.payload_length));
    return true;
}
