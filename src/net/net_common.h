#pragma once

#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>
#include <functional>
#include <string>
#include <vector>
#include <cstdint>

#include "net/protocol.h"

// Callback types for received messages
using OnMessageCallback = std::function<void(HSteamNetConnection conn, MessageType type,
                                              const uint8_t* payload, size_t len)>;
using OnConnectCallback = std::function<void(HSteamNetConnection conn)>;
using OnDisconnectCallback = std::function<void(HSteamNetConnection conn)>;

// Initialize/shutdown the GameNetworkingSockets library (call once)
void net_init();
void net_shutdown();

// Send a typed message over a connection
bool net_send(HSteamNetConnection conn, MessageType type, const std::vector<uint8_t>& payload,
              int send_flags = k_nSteamNetworkingSend_Reliable);

// Send unreliable (for input snapshots, state updates)
inline bool net_send_unreliable(HSteamNetConnection conn, MessageType type,
                                 const std::vector<uint8_t>& payload) {
    return net_send(conn, type, payload, k_nSteamNetworkingSend_Unreliable);
}
