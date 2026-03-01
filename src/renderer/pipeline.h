#pragma once

#include <string>
#include <vector>
#include <vulkan/vulkan.h>

struct PipelineBuilder {
    std::vector<VkPipelineShaderStageCreateInfo> shader_stages;
    VkPipelineVertexInputStateCreateInfo vertex_input_info{};
    VkPipelineInputAssemblyStateCreateInfo input_assembly{};
    VkViewport viewport{};
    VkRect2D scissor{};
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    VkPipelineMultisampleStateCreateInfo multisampling{};
    VkPipelineDepthStencilStateCreateInfo depth_stencil{};
    VkPipelineColorBlendAttachmentState color_blend_attachment{};
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;

    void set_defaults();
    VkPipeline build(VkDevice device, VkRenderPass render_pass);
};

VkShaderModule load_shader_module(VkDevice device, const std::string& path);
