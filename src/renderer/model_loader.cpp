#include "renderer/model_loader.h"
#include <iostream>

// GLTF loading will be fully implemented when models are needed (Phase 2+).
// For now, procedural geometry (cubes, planes) is used.

std::vector<LoadedMesh> load_gltf(const std::string& path) {
    std::cerr << "GLTF loading not yet implemented: " << path << std::endl;
    return {};
}
