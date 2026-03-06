#include "net/net_common.h"
#include <iostream>
#include <cassert>

static bool g_net_initialized = false;

static void debug_output(ESteamNetworkingSocketsDebugOutputType type, const char* msg) {
    if (type == k_ESteamNetworkingSocketsDebugOutputType_Bug ||
        type == k_ESteamNetworkingSocketsDebugOutputType_Error) {
        std::cerr << "[NET ERROR] " << msg << std::endl;
    }
}

void net_init() {
    if (g_net_initialized) return;

    SteamDatagramErrMsg err;
    if (!GameNetworkingSockets_Init(nullptr, err)) {
        std::cerr << "GameNetworkingSockets_Init failed: " << err << std::endl;
        return;
    }

    SteamNetworkingUtils()->SetDebugOutputFunction(
        k_ESteamNetworkingSocketsDebugOutputType_Msg, debug_output);

    g_net_initialized = true;
}

void net_shutdown() {
    if (!g_net_initialized) return;
    GameNetworkingSockets_Kill();
    g_net_initialized = false;
}

bool net_send(HSteamNetConnection conn, MessageType type, const std::vector<uint8_t>& payload,
              int send_flags) {
    auto packet = build_packet(type, payload);
    auto* iface = SteamNetworkingSockets();
    EResult result = iface->SendMessageToConnection(
        conn, packet.data(), static_cast<uint32_t>(packet.size()), send_flags, nullptr);
    return result == k_EResultOK;
}
