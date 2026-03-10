#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include "core/types.h"

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
    glm::vec4 color{1.0f};
    glm::uvec4 bone_indices{0};
    glm::vec4 bone_weights{0.0f};

    static std::vector<VkVertexInputBindingDescription> get_binding_descriptions();
    static std::vector<VkVertexInputAttributeDescription> get_attribute_descriptions();
};

struct Mesh {
    AllocatedBuffer vertex_buffer;
    AllocatedBuffer index_buffer;
    uint32_t index_count = 0;
};

// Generate unit cube with normals and UVs
std::vector<Vertex> cube_vertices();
std::vector<uint32_t> cube_indices();

// Generate ground plane centered at origin
std::vector<Vertex> plane_vertices(float half_extent = 50.0f, float uv_scale = 10.0f);
std::vector<uint32_t> plane_indices();

// Generate blocky humanoid figure (origin at feet, ~1.8m tall)
std::vector<Vertex> humanoid_vertices();
std::vector<uint32_t> humanoid_indices();
