#include "net/net_server.h"
#include <iostream>
#include <cstring>

static thread_local NetServer* g_server_instance = nullptr;

void NetServer::start(uint16_t port) {
    g_server_instance = this;

    SteamNetworkingIPAddr addr{};
    addr.Clear();
    addr.m_port = port;

    SteamNetworkingConfigValue_t opt{};
    opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged,
               reinterpret_cast<void*>(&NetServer::connection_status_callback));

    auto* iface = SteamNetworkingSockets();
    _listen_socket = iface->CreateListenSocketIP(addr, 1, &opt);

    if (_listen_socket == k_HSteamListenSocket_Invalid) {
        std::cerr << "[NET] Failed to listen on port " << port << std::endl;
        return;
    }

    _poll_group = iface->CreatePollGroup();
    _running = true;
    std::cout << "[NET] Server listening on port " << port << std::endl;
}

void NetServer::stop() {
    if (!_running) return;

    auto* iface = SteamNetworkingSockets();

    for (auto& p : _players) {
        if (p.active) {
            iface->CloseConnection(p.connection, 0, "Server shutting down", true);
            p.active = false;
        }
    }

    iface->CloseListenSocket(_listen_socket);
    iface->DestroyPollGroup(_poll_group);
    _listen_socket = k_HSteamListenSocket_Invalid;
    _poll_group = k_HSteamNetPollGroup_Invalid;
    _running = false;
}

void NetServer::poll() {
    if (!_running) return;

    g_server_instance = this;
    auto* iface = SteamNetworkingSockets();

    iface->RunCallbacks();

    // Receive messages from poll group
    ISteamNetworkingMessage* msgs[128];
    int count = iface->ReceiveMessagesOnPollGroup(_poll_group, msgs, 128);
    for (int i = 0; i < count; i++) {
        process_message(msgs[i]->m_conn,
                        static_cast<const uint8_t*>(msgs[i]->m_pData),
                        static_cast<size_t>(msgs[i]->m_cbSize));
        msgs[i]->Release();
    }
}

void NetServer::send_to(uint8_t slot, MessageType type, const std::vector<uint8_t>& payload) {
    if (slot >= MAX_PLAYERS || !_players[slot].active) return;
    net_send(_players[slot].connection, type, payload);
}

void NetServer::broadcast(MessageType type, const std::vector<uint8_t>& payload) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (_players[i].active)
            net_send(_players[i].connection, type, payload);
    }
}

void NetServer::broadcast_unreliable(MessageType type, const std::vector<uint8_t>& payload) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (_players[i].active)
            net_send_unreliable(_players[i].connection, type, payload);
    }
}

void NetServer::kick(uint8_t slot, const std::string& reason) {
    if (slot >= MAX_PLAYERS || !_players[slot].active) return;

    auto* iface = SteamNetworkingSockets();
    iface->CloseConnection(_players[slot].connection, 0, reason.c_str(), true);
    _players[slot].active = false;
    _players[slot].connection = k_HSteamNetConnection_Invalid;
}

int NetServer::player_count() const {
    int count = 0;
    for (const auto& p : _players)
        if (p.active) count++;
    return count;
}

int NetServer::find_slot(HSteamNetConnection conn) const {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (_players[i].active && _players[i].connection == conn)
            return i;
    }
    return -1;
}

int NetServer::allocate_slot() {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!_players[i].active)
            return i;
    }
    return -1;
}

void NetServer::connection_status_callback(SteamNetConnectionStatusChangedCallback_t* info) {
    if (g_server_instance)
        g_server_instance->on_connection_status_changed(info);
}

void NetServer::on_connection_status_changed(SteamNetConnectionStatusChangedCallback_t* info) {
    auto* iface = SteamNetworkingSockets();

    switch (info->m_info.m_eState) {
        case k_ESteamNetworkingConnectionState_Connecting: {
            // Accept the connection and add to poll group
            if (iface->AcceptConnection(info->m_hConn) != k_EResultOK) {
                iface->CloseConnection(info->m_hConn, 0, nullptr, false);
                break;
            }
            iface->SetConnectionPollGroup(info->m_hConn, _poll_group);
            break;
        }

        case k_ESteamNetworkingConnectionState_ClosedByPeer:
        case k_ESteamNetworkingConnectionState_ProblemDetectedLocally: {
            int slot = find_slot(info->m_hConn);
            if (slot >= 0) {
                std::cout << "[NET] Player slot " << slot << " disconnected" << std::endl;
                _players[slot].active = false;
                _players[slot].connection = k_HSteamNetConnection_Invalid;
                if (_on_player_disconnect) _on_player_disconnect(static_cast<uint8_t>(slot));

                // Notify other players
                std::vector<uint8_t> leave_payload = {static_cast<uint8_t>(slot)};
                broadcast(MessageType::ServerPlayerLeave, leave_payload);
            }
            iface->CloseConnection(info->m_hConn, 0, nullptr, false);
            break;
        }

        default:
            break;
    }
}

void NetServer::process_message(HSteamNetConnection conn, const uint8_t* data, size_t len) {
    PacketHeader header{};
    if (!parse_header(data, len, header)) return;

    const uint8_t* payload = data + PACKET_HEADER_SIZE;
    size_t payload_len = header.payload_length;

    switch (header.type) {
        case MessageType::ClientHello: {
            auto hello = ClientHelloMsg::deserialize(payload, payload_len);

            if (hello.protocol_version != PROTOCOL_VERSION) {
                std::string reason = "Protocol version mismatch";
                std::vector<uint8_t> deny(reason.begin(), reason.end());
                net_send(conn, MessageType::ServerDenied, deny);
                SteamNetworkingSockets()->CloseConnection(conn, 0, reason.c_str(), true);
                break;
            }

            int slot = allocate_slot();
            if (slot < 0) {
                std::string reason = "Server full";
                std::vector<uint8_t> deny(reason.begin(), reason.end());
                net_send(conn, MessageType::ServerDenied, deny);
                SteamNetworkingSockets()->CloseConnection(conn, 0, reason.c_str(), true);
                break;
            }

            _players[slot].connection = conn;
            _players[slot].user_id = std::string(hello.auth_token);
            _players[slot].active = true;
            _players[slot].last_input = {};

            // Send welcome
            ServerWelcomeMsg welcome{};
            welcome.your_slot = static_cast<uint8_t>(slot);
            welcome.server_tick = 0; // Will be set by game server
            net_send(conn, MessageType::ServerWelcome, welcome.serialize());

            std::cout << "[NET] Player connected: slot " << slot << std::endl;

            // Notify other players
            std::vector<uint8_t> join_payload = {static_cast<uint8_t>(slot)};
            broadcast(MessageType::ServerPlayerJoin, join_payload);

            if (_on_player_connect)
                _on_player_connect(static_cast<uint8_t>(slot), _players[slot].user_id);
            break;
        }

        case MessageType::ClientInput: {
            int slot = find_slot(conn);
            if (slot < 0) break;
            auto input = InputSnapshot::deserialize(payload, payload_len);
            _players[slot].last_input = input;
            if (_on_player_input)
                _on_player_input(static_cast<uint8_t>(slot), input);
            break;
        }

        case MessageType::ClientDisconnect: {
            int slot = find_slot(conn);
            if (slot >= 0) {
                _players[slot].active = false;
                _players[slot].connection = k_HSteamNetConnection_Invalid;
                if (_on_player_disconnect)
                    _on_player_disconnect(static_cast<uint8_t>(slot));
            }
            SteamNetworkingSockets()->CloseConnection(conn, 0, "Client requested", false);
            break;
        }

        default:
            break;
    }
}
