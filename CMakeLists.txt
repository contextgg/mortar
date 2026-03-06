cmake_minimum_required(VERSION 3.24)
project(mortar LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Dependencies via vcpkg
find_package(flecs CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(vk-bootstrap CONFIG REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(Jolt CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(GameNetworkingSockets CONFIG REQUIRED)

# stb (header-only, provides stb_image.h)
find_path(STB_INCLUDE_DIRS "stb_image.h")

# cgltf (header-only)
find_path(CGLTF_INCLUDE_DIRS "cgltf.h")

# miniaudio (header-only)
find_path(MINIAUDIO_INCLUDE_DIRS "miniaudio.h")

# Shader compilation
add_subdirectory(shaders)

# ============================================================
# Shared game logic (used by both client and server)
# ============================================================
add_library(mortar_shared STATIC
    src/ecs/systems.cpp
    src/ecs/systems/movement_systems.cpp
    src/ecs/systems/physics_systems.cpp
    src/ecs/systems/combat_systems.cpp
    src/ecs/systems/ai_systems.cpp
    src/physics/physics_world.cpp
    src/game/player.cpp
    src/game/enemy.cpp
    src/game/spawner.cpp
    src/net/net_common.cpp
)

target_include_directories(mortar_shared PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${STB_INCLUDE_DIRS}
    ${CGLTF_INCLUDE_DIRS}
    ${MINIAUDIO_INCLUDE_DIRS}
)

target_link_libraries(mortar_shared PUBLIC
    $<IF:$<TARGET_EXISTS:flecs::flecs>,flecs::flecs,flecs::flecs_static>
    glm::glm
    Jolt::Jolt
    nlohmann_json::nlohmann_json
    GameNetworkingSockets::shared
)

# ============================================================
# Client (mortar) — full engine with rendering, audio, input
# ============================================================
add_executable(mortar
    src/main.cpp
    src/core/engine.cpp
    src/core/input.cpp
    src/ecs/client_systems.cpp
    src/ecs/systems/camera_systems.cpp
    src/ecs/systems/render_systems.cpp
    src/ecs/systems/particle_systems.cpp
    src/ecs/systems/hud_systems.cpp
    src/renderer/mesh.cpp
    src/renderer/pipeline.cpp
    src/renderer/descriptors.cpp
    src/renderer/texture.cpp
    src/renderer/material.cpp
    src/renderer/model_loader.cpp
    src/renderer/particle_renderer.cpp
    src/renderer/imgui_renderer.cpp
    src/audio/audio_engine.cpp
    src/map/map_loader.cpp
    src/net/net_client.cpp
)

target_link_libraries(mortar PRIVATE
    mortar_shared
    glfw
    vk-bootstrap::vk-bootstrap
    GPUOpen::VulkanMemoryAllocator
    Vulkan::Vulkan
    imgui::imgui
)

if(UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
    target_link_libraries(mortar PRIVATE
        X11::X11
        X11::Xrandr
        X11::Xinerama
        X11::Xcursor
        X11::Xi
    )
endif()

# Shaders must be compiled before the client
add_dependencies(mortar shaders)

# Copy shader SPIR-V to build directory
add_custom_command(TARGET mortar POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:mortar>/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_BINARY_DIR}/shaders
        $<TARGET_FILE_DIR:mortar>/shaders
)

# Copy assets to build directory
add_custom_command(TARGET mortar POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:mortar>/assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:mortar>/assets
)

# ============================================================
# Server (mortar-server) — headless, no renderer/audio/input
# ============================================================
add_executable(mortar-server
    src/server_main.cpp
    src/server/game_server.cpp
    src/net/net_server.cpp
)

target_link_libraries(mortar-server PRIVATE
    mortar_shared
)

# Copy assets for server (maps)
add_custom_command(TARGET mortar-server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:mortar-server>/assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:mortar-server>/assets
)
