#pragma once

#include <flecs.h>

class Input;

void register_camera_systems(flecs::world& world, Input& input);
