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
#include "renderer/model_loader.h"
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

    // Set up ECS world with singletons
    flecs::world world;
    world.set<InputRef>({&input});
    world.set<PhysicsRef>({&physics});
    world.set<EngineRef>({&engine});
    world.set<AudioRef>({&audio});
    world.set<AudioSounds>({.shoot = shoot_sound, .hit = hit_sound});

    // Register all systems (shared + client)
    register_client_systems(world);
    register_spawner_system(world);

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
        });

        net_client.on_disconnect([]() {
            std::cerr << "Disconnected from server" << std::endl;
        });

        net_client.connect(host, port, args.token);
    }

    // Load map (use default if none specified)
    std::string map_path = args.map_path.empty() ? "assets/maps/default.json" : args.map_path;
    auto map_result = load_map(map_path, world, engine);
    if (!map_result.success) {
        std::cerr << "Failed to load map: " << map_result.error << std::endl;
        return 1;
    }
    std::cout << "Loaded map: " << map_path << std::endl;
    glm::vec3 player_spawn_pos = map_result.player_spawn_position;

    // Player mesh + material — try glTF model, fall back to procedural humanoid
    uint32_t player_mesh_idx = 0;
    uint32_t player_mat_idx = 0;
    std::shared_ptr<SkeletonData> player_skeleton;

    auto player_model = load_gltf("assets/models/characters/Ranger.glb");
    if (!player_model.vertices.empty()) {
        player_mesh_idx = engine.upload_mesh(player_model.vertices, player_model.indices);
        uint32_t tex_idx = 0; // default white
        if (!player_model.textures.empty() && !player_model.textures[0].pixels.empty()) {
            const auto& tex = player_model.textures[0];
            tex_idx = engine.upload_texture(tex.pixels.data(), tex.width, tex.height);
        }
        MaterialUBO player_props{};
        player_props.base_color = glm::vec4(1.0f);
        player_props.roughness = 0.7f;
        player_mat_idx = engine.create_material(player_props, tex_idx);
        player_skeleton = player_model.skeleton;

        // Load animations from separate KayKit animation packs
        if (player_skeleton) {
            load_gltf_animations("assets/models/characters/Rig_Medium_General.glb", *player_skeleton);
            load_gltf_animations("assets/models/characters/Rig_Medium_MovementBasic.glb", *player_skeleton);
        }
    } else {
        player_mesh_idx = engine.upload_mesh(humanoid_vertices(), humanoid_indices());
        MaterialUBO player_props{};
        player_props.base_color = glm::vec4(0.8f, 0.8f, 0.2f, 1.0f);
        player_props.roughness = 0.5f;
        player_mat_idx = engine.create_material(player_props);
    }

    // Player entity
    auto player = create_player(world, player_mesh_idx, player_mat_idx);
    player.set(Transform{.position = player_spawn_pos});

    // Attach skeletal animation if the model has a skeleton
    if (player_skeleton && !player_skeleton->clips.empty()) {
        const auto& clips = player_skeleton->clips;

        // Resolve clip indices by name
        auto find_clip = [&](const std::string& search, uint32_t fallback = 0) -> uint32_t {
            for (uint32_t i = 0; i < static_cast<uint32_t>(clips.size()); i++) {
                if (clips[i].name.find(search) != std::string::npos) return i;
            }
            return fallback;
        };

        uint32_t idle_clip = find_clip("Idle");
        uint32_t run_clip = find_clip("Running_A", idle_clip);
        uint32_t sprint_clip = find_clip("Running_B", run_clip);
        uint32_t jump_clip = find_clip("Jump_Start", idle_clip);
        uint32_t attack_clip = find_clip("Interact", idle_clip);

        player.set(AnimatedModel{
            .skeleton = player_skeleton,
            .current_clip = idle_clip,
            .time = 0.0f,
            .speed = 1.0f,
            .looping = true,
            .playing = true,
        });

        player.set(PlayerAnimClips{
            .idle = idle_clip,
            .run = run_clip,
            .sprint = sprint_clip,
            .jump_start = jump_clip,
            .attack = attack_clip,
        });
    }

    // Camera
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

    while (!engine.should_close()) {
        engine.poll_events();
        input.update();

        // Poll network
        if (multiplayer) {
            net_client.poll();

            if (net_client.is_connected()) {
                InputSnapshot snap{};
                snap.tick = net_client.server_tick();
                auto p = world.lookup("Player");
                if (p.is_alive()) {
                    const auto* pi = p.try_get<PlayerInput>();
                    if (pi) {
                        snap.move_dir = pi->move_dir;
                        snap.look_delta = pi->mouse_delta;
                        snap.buttons = 0;
                        if (pi->fire) snap.buttons |= InputSnapshot::BTN_FIRE;
                        if (pi->jump) snap.buttons |= InputSnapshot::BTN_JUMP;
                        if (pi->dodge_left) snap.buttons |= InputSnapshot::BTN_DODGE_LEFT;
                        if (pi->dodge_right) snap.buttons |= InputSnapshot::BTN_DODGE_RIGHT;
                    }
                }
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
