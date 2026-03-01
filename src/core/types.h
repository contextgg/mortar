#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <glm/glm.hpp>

struct AllocatedBuffer {
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = nullptr;
};

struct AllocatedImage {
    VkImage image = VK_NULL_HANDLE;
    VmaAllocation allocation = nullptr;
    VkImageView view = VK_NULL_HANDLE;
};

// GPU-side scene uniform buffer (std140 layout, set 0 binding 0)
struct SceneUBO {
    glm::mat4 view;
    glm::mat4 projection;
    glm::mat4 view_projection;
    glm::vec4 camera_position;
    glm::vec4 ambient_color;       // rgb + intensity in w
    glm::vec4 sun_direction;       // xyz = direction, w unused
    glm::vec4 sun_color;           // rgb = color, a = intensity
    glm::vec4 point_positions[4];  // xyz = position, w = radius
    glm::vec4 point_colors[4];     // rgb = color, a = intensity
    int num_point_lights = 0;
    int _pad[3]{};
};

// GPU-side material uniform buffer (std140 layout, set 1 binding 0)
struct MaterialUBO {
    glm::vec4 base_color{1.0f};
    glm::vec4 emissive{0.0f};
    float metallic = 0.0f;
    float roughness = 0.5f;
    float _pad[2]{};
};

struct Texture {
    AllocatedImage image;
    VkSampler sampler = VK_NULL_HANDLE;
};

struct GPUMaterial {
    MaterialUBO properties;
    uint32_t albedo_texture = 0;
    AllocatedBuffer ubo_buffer;
    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
};
