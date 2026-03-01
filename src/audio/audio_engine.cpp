#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include "audio/audio_engine.h"
#include <iostream>

void AudioEngine::init() {
    _engine = new ma_engine();
    ma_result result = ma_engine_init(nullptr, _engine);
    if (result != MA_SUCCESS) {
        std::cerr << "Failed to initialize audio engine" << std::endl;
        delete _engine;
        _engine = nullptr;
        return;
    }
    _initialized = true;
}

void AudioEngine::shutdown() {
    if (_engine) {
        ma_engine_uninit(_engine);
        delete _engine;
        _engine = nullptr;
    }
    _initialized = false;
}

uint32_t AudioEngine::load_sound(const std::string& path) {
    uint32_t id = static_cast<uint32_t>(_sound_paths.size());
    _sound_paths.push_back(path);
    return id;
}

void AudioEngine::play(uint32_t sound_id, float volume) {
    if (!_initialized || sound_id >= _sound_paths.size()) return;
    // Fire and forget playback
    ma_engine_play_sound(_engine, _sound_paths[sound_id].c_str(), nullptr);
    (void)volume; // Volume applied via group in full implementation
}

void AudioEngine::play_at(uint32_t sound_id, float x, float y, float z, float volume) {
    // Spatial audio — simplified to regular play for now
    play(sound_id, volume);
    (void)x; (void)y; (void)z;
}

void AudioEngine::set_listener_position(float x, float y, float z) {
    if (!_initialized) return;
    ma_engine_listener_set_position(_engine, 0, x, y, z);
}
