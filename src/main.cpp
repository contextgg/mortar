#include <iostream>
#include <string>

#include <flecs.h>
#include <glm/glm.hpp>
#include <GLFW/glfw3.h>

#include "core/engine.h"
#include "core/input.h"
#include "ecs/components.h"
#include "ecs/client_systems.h"
#include "physics/physics_world.h"
#include "audio/audio_engine.h"
#include "renderer/imgui_renderer.h"
#include "game/player.h"
#include "game/weapon.h"
#include "game/spawner.h"
#include "map/map_loader.h"
#include "net/net_common.h"
#include "net/net_client.h"

struct ClientArgs {
    std::string map_path;
    std::string connect_addr;  // "host:port"
    std::string token;
};

static ClientArgs parse_args(int argc, char* argv[]) {
    ClientArgs args;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if ((arg == "--map" || arg == "-m") && i + 1 < argc) {
            args.map_path = argv[++i];
        } else if (arg == "--connect" && i + 1 < argc) {
            args.connect_addr = argv[++i];
        } else if (arg == "--token" && i + 1 < argc) {
            args.token = argv[++i];
        } else if (!arg.starts_with("-") && args.map_path.empty()) {
            args.map_path = arg; // positional arg = map path
        }
    }
    return args;
}

int main(int argc, char* argv[]) {
    auto args = parse_args(argc, argv);
    bool multiplayer = !args.connect_addr.empty();

    VulkanEngine engine;

    try {
        engine.init(1920, 1080, "Mortar");
    } catch (const std::exception& e) {
        std::cerr << "Engine init failed: " << e.what() << std::endl;
        return 1;
    }

    // Input system
    Input input;
    input.init(engine.window);
    input.capture_cursor(true);

    // Physics
    PhysicsWorld physics;
    physics.init();

    // Audio
    AudioEngine audio;
    audio.init();
    uint32_t shoot_sound = audio.load_sound("assets/audio/shoot.wav");
    uint32_t hit_sound = audio.load_sound("assets/audio/hit.wav");

    // ImGui
    ImGuiRenderer imgui_renderer;
    imgui_renderer.init(engine.window, engine.instance(), engine.physical_device(),
                         engine.device(), engine.graphics_queue_family(),
                         engine.graphics_queue(), engine.render_pass(),
                         engine.swapchain_image_count());
    engine.set_imgui_enabled(true);
    engine.set_imgui_render_callback([&imgui_renderer](VkCommandBuffer cmd) {
        imgui_renderer.render(cmd);
    });

    // Set up ECS world
    flecs::world world;
    register_client_systems(world, engine, input, &physics);
    register_spawner_system(world, physics);

    // Networking (multiplayer mode)
    NetClient net_client;
    if (multiplayer) {
        net_init();

        // Parse host:port
        std::string host = "127.0.0.1";
        uint16_t port = DEFAULT_SERVER_PORT;
        auto colon = args.connect_addr.rfind(':');
        if (colon != std::string::npos) {
            host = args.connect_addr.substr(0, colon);
            port = static_cast<uint16_t>(std::stoi(args.connect_addr.substr(colon + 1)));
        } else {
            host = args.connect_addr;
        }

        std::cout << "Connecting to " << host << ":" << port << std::endl;

        net_client.on_snapshot([&world](const SnapshotMsg& snap) {
            // TODO: interpolate remote player positions
            // For now, just update transforms of remote player entities
        });

        net_client.on_disconnect([]() {
            std::cerr << "Disconnected from server" << std::endl;
        });

        net_client.connect(host, port, args.token);
    }

    // Load map (use default if none specified)
    std::string map_path = args.map_path.empty() ? "assets/maps/default.json" : args.map_path;
    auto map_result = load_map(map_path, world, engine, physics);
    if (!map_result.success) {
        std::cerr << "Failed to load map: " << map_result.error << std::endl;
        return 1;
    }
    std::cout << "Loaded map: " << map_path << std::endl;
    glm::vec3 player_spawn_pos = map_result.player_spawn_position;

    // Player mesh + material (always needed)
    uint32_t humanoid_mesh = engine.upload_mesh(humanoid_vertices(), humanoid_indices());
    MaterialUBO player_props{};
    player_props.base_color = glm::vec4(0.8f, 0.8f, 0.2f, 1.0f);
    player_props.roughness = 0.5f;
    uint32_t player_mat = engine.create_material(player_props);

    // Player entity (with character controller) — uses spawn position from map or default
    auto player = create_player(world, physics, humanoid_mesh, player_mat);
    player.set(Transform{.position = player_spawn_pos});

    // Camera (always needed)
    world.entity("MainCamera")
        .set(Transform{.position = glm::vec3(0.0f, 5.0f, 10.0f)})
        .set(Camera{
            .fov = 65.0f,
            .near_plane = 0.1f,
            .far_plane = 200.0f,
            .distance = 4.0f,
            .pitch = -10.0f,
        });

    // Main loop
    double last_time = glfwGetTime();
    bool was_firing = false;

    while (!engine.should_close()) {
        engine.poll_events();
        input.update();

        // Poll network
        if (multiplayer) {
            net_client.poll();

            // Send input to server
            if (net_client.is_connected()) {
                InputSnapshot snap{};
                snap.tick = net_client.server_tick();
                world.each([&snap](const Player&, const PlayerInput& pi) {
                    snap.move_dir = pi.move_dir;
                    snap.look_delta = pi.mouse_delta;
                    snap.buttons = 0;
                    if (pi.fire) snap.buttons |= InputSnapshot::BTN_FIRE;
                    if (pi.jump) snap.buttons |= InputSnapshot::BTN_JUMP;
                    if (pi.dodge_left) snap.buttons |= InputSnapshot::BTN_DODGE_LEFT;
                    if (pi.dodge_right) snap.buttons |= InputSnapshot::BTN_DODGE_RIGHT;
                });
                net_client.send_input(snap);
            }
        }

        // ESC to release cursor / close
        if (input.key_pressed(GLFW_KEY_ESCAPE)) {
            if (input.cursor_captured()) {
                input.capture_cursor(false);
            } else {
                glfwSetWindowShouldClose(engine.window, GLFW_TRUE);
            }
        }

        // Click to re-capture cursor
        if (input.mouse_pressed(GLFW_MOUSE_BUTTON_LEFT) && !input.cursor_captured()) {
            input.capture_cursor(true);
        }

        double current_time = glfwGetTime();
        float delta = static_cast<float>(current_time - last_time);
        last_time = current_time;
        if (delta > 0.05f) delta = 0.05f;

        // Detect shooting for audio/particle feedback
        bool firing_now = false;
        world.each([&](const Player&, const PlayerInput& pi, const Weapon& w) {
            if (pi.fire && w.cooldown <= 0.0f && w.ammo > 0) {
                firing_now = true;
            }
        });
        if (firing_now && !was_firing) {
            audio.play(shoot_sound);

            // Spawn muzzle flash particle emitter at player position
            glm::vec3 player_pos{0.0f};
            world.each([&player_pos](const Player&, const Transform& t) {
                player_pos = t.position;
            });
            world.entity()
                .set(Transform{.position = player_pos + glm::vec3(0, 0.5f, 0)})
                .set(ParticleEmitter{
                    .burst_count = 5,
                    .start_color = glm::vec4(1.0f, 0.8f, 0.2f, 1.0f),
                    .end_color = glm::vec4(1.0f, 0.2f, 0.0f, 0.0f),
                    .start_size = 0.15f,
                    .end_size = 0.0f,
                    .lifetime = 0.3f,
                    .speed = 3.0f,
                });
        }
        was_firing = firing_now;

        // Update listener position for spatial audio
        world.each([&audio](const Camera&, const Transform& t) {
            audio.set_listener_position(t.position.x, t.position.y, t.position.z);
        });

        // ImGui new frame
        imgui_renderer.new_frame();

        engine.begin_frame();
        engine.clear_renderables();
        world.progress(delta);
        engine.draw_frame();
    }

    if (multiplayer) {
        net_client.disconnect();
        net_shutdown();
    }

    imgui_renderer.shutdown();
    audio.shutdown();
    physics.shutdown();
    engine.cleanup();
    return 0;
}
