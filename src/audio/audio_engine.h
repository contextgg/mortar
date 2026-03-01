#pragma once

#include <string>
#include <vector>
#include <cstdint>

struct ma_engine;

class AudioEngine {
public:
    void init();
    void shutdown();

    // Load a sound file, returns sound ID
    uint32_t load_sound(const std::string& path);

    // Play a loaded sound (fire-and-forget)
    void play(uint32_t sound_id, float volume = 1.0f);

    // Play a sound at a 3D position
    void play_at(uint32_t sound_id, float x, float y, float z, float volume = 1.0f);

    // Set listener position (usually camera)
    void set_listener_position(float x, float y, float z);

    bool is_initialized() const { return _initialized; }

private:
    ma_engine* _engine = nullptr;
    std::vector<std::string> _sound_paths;
    bool _initialized = false;
};
