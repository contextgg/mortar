#include "server/game_server.h"

#include <iostream>
#include <string>
#include <csignal>

static GameServer* g_server = nullptr;

void signal_handler(int) {
    if (g_server) g_server->stop();
}

int main(int argc, char* argv[]) {
    uint16_t port = DEFAULT_SERVER_PORT;
    std::string map_path;

    // Simple arg parsing
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if ((arg == "--port" || arg == "-p") && i + 1 < argc) {
            port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if ((arg == "--map" || arg == "-m") && i + 1 < argc) {
            map_path = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: mortar-server [options]\n"
                      << "  --port, -p <port>  Listen port (default: " << DEFAULT_SERVER_PORT << ")\n"
                      << "  --map, -m <path>   Map file to load\n";
            return 0;
        }
    }

    std::cout << "Mortar Dedicated Server" << std::endl;

    GameServer server;
    g_server = &server;

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    server.init(port, map_path);
    server.run();

    return 0;
}
