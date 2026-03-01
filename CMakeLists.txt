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

# stb (header-only, provides stb_image.h)
find_path(STB_INCLUDE_DIRS "stb_image.h")

# cgltf (header-only)
find_path(CGLTF_INCLUDE_DIRS "cgltf.h")

# miniaudio (header-only)
find_path(MINIAUDIO_INCLUDE_DIRS "miniaudio.h")

# Shader compilation
add_subdirectory(shaders)

# Source files
add_executable(mortar
    src/main.cpp
    src/core/engine.cpp
    src/core/input.cpp
    src/ecs/systems.cpp
    src/ecs/systems/movement_systems.cpp
    src/ecs/systems/camera_systems.cpp
    src/ecs/systems/render_systems.cpp
    src/ecs/systems/physics_systems.cpp
    src/ecs/systems/combat_systems.cpp
    src/ecs/systems/ai_systems.cpp
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
    src/physics/physics_world.cpp
    src/audio/audio_engine.cpp
    src/game/player.cpp
    src/game/enemy.cpp
    src/game/spawner.cpp
    src/map/map_loader.cpp
)

target_include_directories(mortar PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${STB_INCLUDE_DIRS}
    ${CGLTF_INCLUDE_DIRS}
    ${MINIAUDIO_INCLUDE_DIRS}
)

target_link_libraries(mortar PRIVATE
    $<IF:$<TARGET_EXISTS:flecs::flecs>,flecs::flecs,flecs::flecs_static>
    glfw
    glm::glm
    vk-bootstrap::vk-bootstrap
    GPUOpen::VulkanMemoryAllocator
    Vulkan::Vulkan
    Jolt::Jolt
    imgui::imgui
    nlohmann_json::nlohmann_json
)

# Shaders must be compiled before the executable runs
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
