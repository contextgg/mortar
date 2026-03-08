#include "server/game_server.h"

#include <iostream>
#include <string>
#include <cstdlib>
#include <csignal>

static GameServer* g_server = nullptr;

void signal_handler(int) {
    if (g_server) g_server->stop();
}

int main(int argc, char* argv[]) {
    uint16_t port = DEFAULT_SERVER_PORT;
    std::string session_id;
    std::string api_base = "https://api.ctx.gg";
    std::string server_secret;

    // Check environment variables first
    if (const char* env = std::getenv("SERVER_SECRET")) server_secret = env;
    if (const char* env = std::getenv("API_BASE")) api_base = env;
    if (const char* env = std::getenv("SESSION_ID")) session_id = env;
    if (const char* env = std::getenv("PORT")) port = static_cast<uint16_t>(std::stoi(env));

    // CLI args override env vars
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if ((arg == "--port" || arg == "-p") && i + 1 < argc) {
            port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--session" && i + 1 < argc) {
            session_id = argv[++i];
        } else if (arg == "--api" && i + 1 < argc) {
            api_base = argv[++i];
        } else if (arg == "--secret" && i + 1 < argc) {
            server_secret = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: mortar-server [options]\n"
                      << "  --port, -p <port>      Listen port (default: " << DEFAULT_SERVER_PORT << ")\n"
                      << "  --session <id>         Game session ID from API\n"
                      << "  --api <url>            API base URL (default: https://api.ctx.gg)\n"
                      << "  --secret <secret>      Server auth secret\n"
                      << "\nEnvironment variables: PORT, SESSION_ID, API_BASE, SERVER_SECRET\n";
            return 0;
        }
    }

    std::cout << "Mortar Dedicated Server" << std::endl;
    if (!session_id.empty()) {
        std::cout << "  Session: " << session_id << std::endl;
    }

    GameServer server;
    g_server = &server;

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    server.init(port, session_id, api_base, server_secret);
    server.run();

    return 0;
}
