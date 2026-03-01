#include "renderer/material.h"
#include <cstring>

void write_material_descriptor(
    VkDevice device,
    const GPUMaterial& mat,
    const Texture& albedo_tex,
    VkDescriptorSetLayout /*layout*/)
{
    // Write descriptor set bindings
    VkDescriptorBufferInfo buf_info{};
    buf_info.buffer = mat.ubo_buffer.buffer;
    buf_info.offset = 0;
    buf_info.range = sizeof(MaterialUBO);

    VkDescriptorImageInfo img_info{};
    img_info.sampler = albedo_tex.sampler;
    img_info.imageView = albedo_tex.image.view;
    img_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet writes[2]{};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = mat.descriptor_set;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].pBufferInfo = &buf_info;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = mat.descriptor_set;
    writes[1].dstBinding = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].pImageInfo = &img_info;

    vkUpdateDescriptorSets(device, 2, writes, 0, nullptr);
}
