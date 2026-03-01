#pragma once

#include "ecs/components.h"

inline Weapon make_pistol() {
    return Weapon{
        .fire_rate = 3.0f,
        .damage = 25.0f,
        .range = 50.0f,
        .ammo = 12,
        .max_ammo = 12,
    };
}

inline Weapon make_rifle() {
    return Weapon{
        .fire_rate = 8.0f,
        .damage = 15.0f,
        .range = 100.0f,
        .ammo = 30,
        .max_ammo = 30,
    };
}
