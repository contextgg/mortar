#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include "core/types.h"

// Update a material's descriptor set with its UBO buffer and albedo texture
void write_material_descriptor(
    VkDevice device,
    const GPUMaterial& mat,
    const Texture& albedo_tex,
    VkDescriptorSetLayout layout);
