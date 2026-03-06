#pragma once

#include "net/net_common.h"
#include <array>
#include <string>

struct ConnectedPlayer {
    HSteamNetConnection connection = k_HSteamNetConnection_Invalid;
    std::string user_id;
    bool active = false;
    InputSnapshot last_input{};
};

// Server-side networking: listens for client connections
class NetServer {
public:
    void start(uint16_t port);
    void stop();
    void poll();

    bool is_running() const { return _running; }

    void send_to(uint8_t slot, MessageType type, const std::vector<uint8_t>& payload);
    void broadcast(MessageType type, const std::vector<uint8_t>& payload);
    void broadcast_unreliable(MessageType type, const std::vector<uint8_t>& payload);
    void kick(uint8_t slot, const std::string& reason);

    const ConnectedPlayer& player(uint8_t slot) const { return _players[slot]; }
    int player_count() const;

    // Callbacks
    void on_player_connect(std::function<void(uint8_t slot, const std::string& user_id)> cb) {
        _on_player_connect = std::move(cb);
    }
    void on_player_disconnect(std::function<void(uint8_t slot)> cb) {
        _on_player_disconnect = std::move(cb);
    }
    void on_player_input(std::function<void(uint8_t slot, const InputSnapshot& input)> cb) {
        _on_player_input = std::move(cb);
    }

private:
    void on_connection_status_changed(SteamNetConnectionStatusChangedCallback_t* info);
    void process_message(HSteamNetConnection conn, const uint8_t* data, size_t len);
    int find_slot(HSteamNetConnection conn) const;
    int allocate_slot();

    static void connection_status_callback(SteamNetConnectionStatusChangedCallback_t* info);

    HSteamListenSocket _listen_socket = k_HSteamListenSocket_Invalid;
    HSteamNetPollGroup _poll_group = k_HSteamNetPollGroup_Invalid;
    bool _running = false;

    std::array<ConnectedPlayer, MAX_PLAYERS> _players{};

    std::function<void(uint8_t, const std::string&)> _on_player_connect;
    std::function<void(uint8_t)> _on_player_disconnect;
    std::function<void(uint8_t, const InputSnapshot&)> _on_player_input;
};
