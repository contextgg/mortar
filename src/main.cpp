#include <iostream>
#include <cmath>

#include <flecs.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>

#include "core/engine.h"
#include "core/input.h"
#include "ecs/components.h"
#include "ecs/systems.h"
#include "physics/physics_world.h"
#include "audio/audio_engine.h"
#include "renderer/imgui_renderer.h"
#include "game/player.h"
#include "game/weapon.h"
#include "game/enemy.h"
#include "game/spawner.h"
#include "map/map_loader.h"

int main(int argc, char* argv[]) {
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
    register_systems(world, engine, input, &physics);
    register_spawner_system(world, physics);

    glm::vec3 player_spawn_pos{0.0f, 2.0f, 0.0f};
    bool map_loaded = false;

    // Load map from CLI argument if provided
    if (argc > 1) {
        auto map_result = load_map(argv[1], world, engine, physics);
        if (map_result.success) {
            player_spawn_pos = map_result.player_spawn_position;
            map_loaded = true;
            std::cout << "Loaded map: " << argv[1] << std::endl;
        } else {
            std::cerr << "Failed to load map: " << map_result.error << std::endl;
            std::cerr << "Falling back to default scene." << std::endl;
        }
    }

    if (!map_loaded) {
        // Upload meshes
        uint32_t cube_mesh = engine.upload_mesh(cube_vertices(), cube_indices());
        uint32_t plane_mesh = engine.upload_mesh(plane_vertices(50.0f, 10.0f), plane_indices());

        // Create materials
        MaterialUBO ground_props{};
        ground_props.base_color = glm::vec4(0.35f, 0.55f, 0.3f, 1.0f);
        ground_props.roughness = 0.9f;
        uint32_t ground_mat = engine.create_material(ground_props);

        MaterialUBO red_props{};
        red_props.base_color = glm::vec4(0.9f, 0.2f, 0.2f, 1.0f);
        red_props.roughness = 0.4f;
        red_props.metallic = 0.3f;
        uint32_t red_mat = engine.create_material(red_props);

        MaterialUBO blue_props{};
        blue_props.base_color = glm::vec4(0.2f, 0.3f, 0.9f, 1.0f);
        blue_props.roughness = 0.3f;
        blue_props.metallic = 0.5f;
        uint32_t blue_mat = engine.create_material(blue_props);

        MaterialUBO enemy_props{};
        enemy_props.base_color = glm::vec4(0.8f, 0.15f, 0.1f, 1.0f);
        enemy_props.roughness = 0.6f;
        enemy_props.metallic = 0.1f;
        uint32_t enemy_mat = engine.create_material(enemy_props);

        // Sun light
        world.entity("Sun")
            .set(DirectionalLight{
                .direction = glm::vec3(-0.4f, -0.8f, -0.4f),
                .color = glm::vec3(1.0f, 0.95f, 0.85f),
                .intensity = 1.2f,
            });

        // Point light
        world.entity("PointLight1")
            .set(Transform{.position = glm::vec3(3.0f, 3.0f, 3.0f)})
            .set(PointLight{
                .color = glm::vec3(0.4f, 0.6f, 1.0f),
                .intensity = 2.0f,
                .radius = 15.0f,
            });

        // Ground plane (with physics)
        physics.add_box(glm::vec3(50.0f, 0.1f, 50.0f), glm::vec3(0, -0.1f, 0),
                        glm::quat(1, 0, 0, 0), true);

        world.entity("Ground")
            .set(Transform{.position = glm::vec3(0.0f)})
            .set(MeshHandle{.index = plane_mesh})
            .set(MaterialHandle{.index = ground_mat});

        // Decorative cubes with physics
        auto spawn_cube = [&](const char* name, glm::vec3 pos, uint32_t mat, float spin = 1.0f) {
            uint64_t eid = world.entity(name)
                .set(Transform{.position = pos})
                .set(AngularVelocity{.axis = glm::vec3(0, 1, 0), .speed = spin})
                .set(MeshHandle{.index = cube_mesh})
                .set(MaterialHandle{.index = mat})
                .set(Health{.current = 50.0f, .max = 50.0f})
                .id();

            physics.add_box(glm::vec3(0.5f), pos, glm::quat(1, 0, 0, 0), true, eid);
        };

        spawn_cube("RedCube", glm::vec3(-3, 0.5f, -3), red_mat, 1.5f);
        spawn_cube("BlueCube", glm::vec3(4, 0.5f, -2), blue_mat, 1.0f);

        // Scattered target cubes
        for (int i = 0; i < 5; i++) {
            float x = -8.0f + i * 4.0f;
            float z = -8.0f;
            MaterialUBO props{};
            props.base_color = glm::vec4(0.5f + i * 0.1f, 0.3f, 0.7f - i * 0.1f, 1.0f);
            props.roughness = 0.2f + i * 0.15f;
            props.metallic = 0.1f * i;
            uint32_t mat = engine.create_material(props);

            std::string cube_name = "TargetCube_" + std::to_string(i);
            uint64_t eid = world.entity(cube_name.c_str())
                .set(Transform{
                    .position = glm::vec3(x, 0.5f, z),
                    .scale = glm::vec3(0.8f),
                })
                .set(AngularVelocity{
                    .axis = glm::vec3(0.3f, 1.0f, 0.2f),
                    .speed = 0.5f + i * 0.3f,
                })
                .set(MeshHandle{.index = cube_mesh})
                .set(MaterialHandle{.index = mat})
                .set(Health{.current = 30.0f, .max = 30.0f})
                .id();

            physics.add_box(glm::vec3(0.4f), glm::vec3(x, 0.5f, z),
                            glm::quat(1, 0, 0, 0), true, eid);
        }

        // Enemy spawner
        world.entity("EnemySpawner")
            .set(SpawnerConfig{
                .spawn_interval = 4.0f,
                .spawn_radius = 15.0f,
                .max_enemies = 10,
                .enemy_mesh = cube_mesh,
                .enemy_material = enemy_mat,
            })
            .set(SpawnerState{.timer = 2.0f});

        // Spawn a few initial enemies
        create_enemy(world, physics, glm::vec3(8, 0.5f, -5), cube_mesh, enemy_mat);
        create_enemy(world, physics, glm::vec3(-6, 0.5f, -8), cube_mesh, enemy_mat);
        create_enemy(world, physics, glm::vec3(3, 0.5f, -12), cube_mesh, enemy_mat);
    }

    // Player mesh + material (always needed)
    uint32_t humanoid_mesh = engine.upload_mesh(humanoid_vertices(), humanoid_indices());
    MaterialUBO player_props{};
    player_props.base_color = glm::vec4(0.8f, 0.8f, 0.2f, 1.0f);
    player_props.roughness = 0.5f;
    uint32_t player_mat = engine.create_material(player_props);

    // Player entity (with character controller) — uses spawn position from map or default
    auto player = create_player(world, engine, physics, humanoid_mesh, player_mat);
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

    imgui_renderer.shutdown();
    audio.shutdown();
    physics.shutdown();
    engine.cleanup();
    return 0;
}
