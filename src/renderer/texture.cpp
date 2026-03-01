#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "renderer/texture.h"
#include <stdexcept>
#include <cstring>

static AllocatedBuffer create_staging_buffer(VmaAllocator allocator, VkDeviceSize size) {
    VkBufferCreateInfo buf_info{};
    buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buf_info.size = size;
    buf_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage = VMA_MEMORY_USAGE_CPU_ONLY;

    AllocatedBuffer buffer;
    vmaCreateBuffer(allocator, &buf_info, &alloc_info, &buffer.buffer, &buffer.allocation, nullptr);
    return buffer;
}

static VkSampler create_default_sampler(VkDevice device) {
    VkSamplerCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    info.magFilter = VK_FILTER_LINEAR;
    info.minFilter = VK_FILTER_LINEAR;
    info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    info.maxLod = 1.0f;

    VkSampler sampler;
    if (vkCreateSampler(device, &info, nullptr, &sampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create sampler");
    }
    return sampler;
}

Texture create_texture_from_data(
    VkDevice device, VmaAllocator allocator, ImmediateSubmitFn submit,
    const uint8_t* pixels, int width, int height)
{
    VkDeviceSize image_size = width * height * 4;

    // Staging buffer
    AllocatedBuffer staging = create_staging_buffer(allocator, image_size);
    void* data;
    vmaMapMemory(allocator, staging.allocation, &data);
    memcpy(data, pixels, image_size);
    vmaUnmapMemory(allocator, staging.allocation);

    // Create image
    VkImageCreateInfo img_info{};
    img_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    img_info.imageType = VK_IMAGE_TYPE_2D;
    img_info.format = VK_FORMAT_R8G8B8A8_SRGB;
    img_info.extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};
    img_info.mipLevels = 1;
    img_info.arrayLayers = 1;
    img_info.samples = VK_SAMPLE_COUNT_1_BIT;
    img_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    img_info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    VmaAllocationCreateInfo img_alloc{};
    img_alloc.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    Texture tex{};
    vmaCreateImage(allocator, &img_info, &img_alloc,
                   &tex.image.image, &tex.image.allocation, nullptr);

    // Upload via immediate submit
    submit([&](VkCommandBuffer cmd) {
        // Transition to TRANSFER_DST
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = tex.image.image;
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);

        // Copy buffer to image
        VkBufferImageCopy region{};
        region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        region.imageExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};

        vkCmdCopyBufferToImage(cmd, staging.buffer, tex.image.image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        // Transition to SHADER_READ_ONLY
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);
    });

    // Destroy staging buffer
    vmaDestroyBuffer(allocator, staging.buffer, staging.allocation);

    // Image view
    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = tex.image.image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = VK_FORMAT_R8G8B8A8_SRGB;
    view_info.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    vkCreateImageView(device, &view_info, nullptr, &tex.image.view);

    // Sampler
    tex.sampler = create_default_sampler(device);

    return tex;
}

Texture create_default_white_texture(
    VkDevice device, VmaAllocator allocator, ImmediateSubmitFn submit)
{
    uint8_t white[] = {255, 255, 255, 255};
    return create_texture_from_data(device, allocator, submit, white, 1, 1);
}

Texture load_texture_from_file(
    VkDevice device, VmaAllocator allocator, ImmediateSubmitFn submit,
    const std::string& path)
{
    int w, h, channels;
    stbi_uc* pixels = stbi_load(path.c_str(), &w, &h, &channels, STBI_rgb_alpha);
    if (!pixels) {
        throw std::runtime_error("Failed to load texture: " + path);
    }

    Texture tex = create_texture_from_data(device, allocator, submit, pixels, w, h);
    stbi_image_free(pixels);
    return tex;
}

void destroy_texture(VkDevice device, VmaAllocator allocator, Texture& tex) {
    vkDestroySampler(device, tex.sampler, nullptr);
    vkDestroyImageView(device, tex.image.view, nullptr);
    vmaDestroyImage(allocator, tex.image.image, tex.image.allocation);
}
