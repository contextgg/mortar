#pragma once

#include <string>
#include <functional>
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include "core/types.h"

using ImmediateSubmitFn = std::function<void(std::function<void(VkCommandBuffer)>&&)>;

Texture create_texture_from_data(
    VkDevice device, VmaAllocator allocator, ImmediateSubmitFn submit,
    const uint8_t* pixels, int width, int height);

Texture create_default_white_texture(
    VkDevice device, VmaAllocator allocator, ImmediateSubmitFn submit);

Texture load_texture_from_file(
    VkDevice device, VmaAllocator allocator, ImmediateSubmitFn submit,
    const std::string& path);

void destroy_texture(VkDevice device, VmaAllocator allocator, Texture& tex);
