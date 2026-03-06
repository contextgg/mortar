#include "net/net_client.h"
#include <iostream>
#include <cstring>

// Thread-local pointer so the static callback can find our instance
static thread_local NetClient* g_client_instance = nullptr;

void NetClient::connect(const std::string& server_addr, uint16_t port,
                         const std::string& auth_token) {
    _auth_token = auth_token;
    g_client_instance = this;

    SteamNetworkingIPAddr addr{};
    addr.Clear();
    addr.ParseString(server_addr.c_str());
    addr.m_port = port;

    SteamNetworkingConfigValue_t opt{};
    opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged,
               reinterpret_cast<void*>(&NetClient::connection_status_callback));

    auto* iface = SteamNetworkingSockets();
    _connection = iface->ConnectByIPAddress(addr, 1, &opt);

    if (_connection == k_HSteamNetConnection_Invalid) {
        std::cerr << "[NET] Failed to create connection" << std::endl;
    }
}

void NetClient::disconnect() {
    if (_connection != k_HSteamNetConnection_Invalid) {
        auto* iface = SteamNetworkingSockets();
        iface->CloseConnection(_connection, 0, "Client disconnect", true);
        _connection = k_HSteamNetConnection_Invalid;
    }
    _connected = false;
}

void NetClient::poll() {
    if (_connection == k_HSteamNetConnection_Invalid) return;

    g_client_instance = this;
    auto* iface = SteamNetworkingSockets();

    iface->RunCallbacks();

    // Receive messages
    ISteamNetworkingMessage* msgs[64];
    int count = iface->ReceiveMessagesOnConnection(_connection, msgs, 64);
    for (int i = 0; i < count; i++) {
        process_message(static_cast<const uint8_t*>(msgs[i]->m_pData),
                        static_cast<size_t>(msgs[i]->m_cbSize));
        msgs[i]->Release();
    }
}

void NetClient::send_input(const InputSnapshot& input) {
    if (!_connected) return;
    net_send_unreliable(_connection, MessageType::ClientInput, input.serialize());
}

void NetClient::connection_status_callback(SteamNetConnectionStatusChangedCallback_t* info) {
    if (g_client_instance)
        g_client_instance->on_connection_status_changed(info);
}

void NetClient::on_connection_status_changed(SteamNetConnectionStatusChangedCallback_t* info) {
    switch (info->m_info.m_eState) {
        case k_ESteamNetworkingConnectionState_Connected: {
            // Send hello with auth token
            ClientHelloMsg hello{};
            hello.protocol_version = PROTOCOL_VERSION;
            strncpy(hello.auth_token, _auth_token.c_str(), sizeof(hello.auth_token) - 1);
            net_send(_connection, MessageType::ClientHello, hello.serialize());
            break;
        }

        case k_ESteamNetworkingConnectionState_ClosedByPeer:
        case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
            std::cerr << "[NET] Disconnected: " << info->m_info.m_szEndDebug << std::endl;
            SteamNetworkingSockets()->CloseConnection(info->m_hConn, 0, nullptr, false);
            _connection = k_HSteamNetConnection_Invalid;
            _connected = false;
            if (_on_disconnect_cb) _on_disconnect_cb();
            break;

        default:
            break;
    }
}

void NetClient::process_message(const uint8_t* data, size_t len) {
    PacketHeader header{};
    if (!parse_header(data, len, header)) return;

    const uint8_t* payload = data + PACKET_HEADER_SIZE;
    size_t payload_len = header.payload_length;

    switch (header.type) {
        case MessageType::ServerWelcome: {
            auto msg = ServerWelcomeMsg::deserialize(payload, payload_len);
            _local_slot = msg.your_slot;
            _server_tick = msg.server_tick;
            _connected = true;
            std::cout << "[NET] Connected as slot " << (int)_local_slot << std::endl;
            if (_on_welcome) _on_welcome(msg.your_slot, msg.server_tick);
            break;
        }

        case MessageType::ServerSnapshot: {
            auto msg = SnapshotMsg::deserialize(payload, payload_len);
            _server_tick = msg.tick;
            if (_on_snapshot) _on_snapshot(msg);
            break;
        }

        case MessageType::ServerPlayerJoin: {
            if (payload_len >= 1 && _on_player_join)
                _on_player_join(payload[0]);
            break;
        }

        case MessageType::ServerPlayerLeave: {
            if (payload_len >= 1 && _on_player_leave)
                _on_player_leave(payload[0]);
            break;
        }

        case MessageType::ServerDenied: {
            std::string reason(reinterpret_cast<const char*>(payload), payload_len);
            std::cerr << "[NET] Connection denied: " << reason << std::endl;
            disconnect();
            break;
        }

        default:
            break;
    }
}
