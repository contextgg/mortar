#pragma once

#include <memory>
#include <string>
#include <vector>

#include "renderer/mesh.h"
#include "animation/skeleton.h"

struct EmbeddedImage {
    std::vector<uint8_t> pixels; // RGBA
    int width = 0;
    int height = 0;
};

struct LoadedModel {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::shared_ptr<SkeletonData> skeleton; // null if no skeleton
    std::vector<EmbeddedImage> textures;    // extracted from glTF
};

// Load a glTF/GLB file. Returns a LoadedModel with mesh data, skeleton, and animations.
// skeleton will be null for static meshes (no skin).
LoadedModel load_gltf(const std::string& path);

// Load animation clips from a separate GLB file and append them to an existing skeleton.
// Remaps joint indices by name to handle different joint orderings.
void load_gltf_animations(const std::string& path, SkeletonData& target_skeleton);
