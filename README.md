# mortar

C++ game engine for the [ctx.gg](https://ctx.gg) platform.

**Vulkan renderer** | **flecs ECS** | **Jolt physics** | **miniaudio**

## Prerequisites

- CMake 3.24+
- C++20 compiler (GCC 12+, Clang 15+, MSVC 2022+)
- Vulkan SDK
- [vcpkg](https://github.com/microsoft/vcpkg)

On Ubuntu/Debian, run the setup script:

```bash
./setup.sh
```

## Build

```bash
export VCPKG_ROOT=/path/to/vcpkg
cmake --preset default
cmake --build --preset default
```

## Run

```bash
cd build
./mortar                          # default map
./mortar --map path/to/map.json   # custom map
```

## Project Structure

```
src/
  core/       Engine core (window, input, main loop)
  renderer/   Vulkan rendering pipeline
  ecs/        Entity Component System (flecs)
  physics/    Jolt physics integration
  audio/      miniaudio audio engine
  map/        JSON map loader
  game/       Game-specific logic (player, enemies, spawners)
shaders/      GLSL shaders (compiled to SPIR-V)
assets/       Models, textures, audio
```

## Map Format

Maps are JSON files compatible with the [map editor](https://github.com/contextgg/map-editor). See the map editor repo for the schema.

## License

MIT
