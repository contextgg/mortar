#pragma once

#include <string>
#include <vector>
#include "renderer/mesh.h"

struct LoadedMesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::string name;
};

// Load meshes from a GLTF/GLB file. Returns empty vector on failure.
std::vector<LoadedMesh> load_gltf(const std::string& path);
