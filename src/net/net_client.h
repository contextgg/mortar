#pragma once

#include "net/net_common.h"
#include <string>

// Client-side networking: connects to a game server
class NetClient {
public:
    void connect(const std::string& server_addr, uint16_t port, const std::string& auth_token);
    void disconnect();
    void poll();

    bool is_connected() const { return _connected; }
    uint8_t local_slot() const { return _local_slot; }
    uint32_t server_tick() const { return _server_tick; }

    void send_input(const InputSnapshot& input);

    // Set callbacks
    void on_welcome(std::function<void(uint8_t slot, uint32_t tick)> cb) { _on_welcome = std::move(cb); }
    void on_snapshot(std::function<void(const SnapshotMsg&)> cb) { _on_snapshot = std::move(cb); }
    void on_player_join(std::function<void(uint8_t slot)> cb) { _on_player_join = std::move(cb); }
    void on_player_leave(std::function<void(uint8_t slot)> cb) { _on_player_leave = std::move(cb); }
    void on_disconnect(std::function<void()> cb) { _on_disconnect_cb = std::move(cb); }

private:
    void on_connection_status_changed(SteamNetConnectionStatusChangedCallback_t* info);
    void process_message(const uint8_t* data, size_t len);

    static void connection_status_callback(SteamNetConnectionStatusChangedCallback_t* info);

    HSteamNetConnection _connection = k_HSteamNetConnection_Invalid;
    bool _connected = false;
    uint8_t _local_slot = 0;
    uint32_t _server_tick = 0;
    std::string _auth_token;

    std::function<void(uint8_t, uint32_t)> _on_welcome;
    std::function<void(const SnapshotMsg&)> _on_snapshot;
    std::function<void(uint8_t)> _on_player_join;
    std::function<void(uint8_t)> _on_player_leave;
    std::function<void()> _on_disconnect_cb;
};
