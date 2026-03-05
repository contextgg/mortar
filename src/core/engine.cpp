#define VMA_IMPLEMENTATION
#include "core/engine.h"
#include "renderer/pipeline.h"
#include "renderer/texture.h"
#include "renderer/material.h"

#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <iostream>

void VulkanEngine::init(int width, int height, const std::string& title) {
    _width = width;
    _height = height;

    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    window = glfwCreateWindow(_width, _height, title.c_str(), nullptr, nullptr);
    if (!window) {
        throw std::runtime_error("Failed to create GLFW window");
    }

    init_vulkan();
    init_swapchain();
    init_commands();
    init_sync_objects();
    init_render_pass();
    init_framebuffers();
    init_descriptors();
    init_pipeline();
    init_default_resources();
}

void VulkanEngine::init_vulkan() {
    uint32_t glfw_ext_count = 0;
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_ext_count);

    vkb::InstanceBuilder builder;
    builder.set_app_name("Mortar")
        .request_validation_layers(true)
        .use_default_debug_messenger()
        .require_api_version(1, 1, 0)
        .enable_extensions(glfw_ext_count, glfw_extensions);

    auto inst_ret = builder.build();

    if (!inst_ret) {
        throw std::runtime_error("Failed to create Vulkan instance: " + inst_ret.error().message());
    }

    vkb::Instance vkb_inst = inst_ret.value();
    _instance = vkb_inst.instance;
    _debug_messenger = vkb_inst.debug_messenger;

    if (glfwCreateWindowSurface(_instance, window, nullptr, &_surface) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface");
    }

    vkb::PhysicalDeviceSelector selector{vkb_inst};
    auto phys_ret = selector
        .set_minimum_version(1, 1)
        .set_surface(_surface)
        .select();

    if (!phys_ret) {
        throw std::runtime_error("Failed to select physical device: " + phys_ret.error().message());
    }

    vkb::PhysicalDevice vkb_phys = phys_ret.value();
    _physical_device = vkb_phys.physical_device;

    vkb::DeviceBuilder device_builder{vkb_phys};
    auto dev_ret = device_builder.build();
    if (!dev_ret) {
        throw std::runtime_error("Failed to create logical device: " + dev_ret.error().message());
    }

    vkb::Device vkb_device = dev_ret.value();
    _device = vkb_device.device;

    auto queue_ret = vkb_device.get_queue(vkb::QueueType::graphics);
    if (!queue_ret) {
        throw std::runtime_error("Failed to get graphics queue");
    }
    _graphics_queue = queue_ret.value();
    _graphics_queue_family = vkb_device.get_queue_index(vkb::QueueType::graphics).value();

    VmaAllocatorCreateInfo alloc_info{};
    alloc_info.physicalDevice = _physical_device;
    alloc_info.device = _device;
    alloc_info.instance = _instance;
    if (vmaCreateAllocator(&alloc_info, &_allocator) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create VMA allocator");
    }
}

void VulkanEngine::init_swapchain() {
    vkb::SwapchainBuilder builder{_physical_device, _device, _surface};
    auto swap_ret = builder
        .set_desired_format({VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
        .set_desired_extent(_width, _height)
        .build();

    if (!swap_ret) {
        throw std::runtime_error("Failed to create swapchain: " + swap_ret.error().message());
    }

    vkb::Swapchain vkb_swap = swap_ret.value();
    _swapchain = vkb_swap.swapchain;
    _swapchain_format = vkb_swap.image_format;
    _swapchain_images = vkb_swap.get_images().value();
    _swapchain_image_views = vkb_swap.get_image_views().value();

    VkExtent3D depth_extent = {
        static_cast<uint32_t>(_width),
        static_cast<uint32_t>(_height),
        1
    };

    VkImageCreateInfo depth_img_info{};
    depth_img_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    depth_img_info.imageType = VK_IMAGE_TYPE_2D;
    depth_img_info.format = _depth_format;
    depth_img_info.extent = depth_extent;
    depth_img_info.mipLevels = 1;
    depth_img_info.arrayLayers = 1;
    depth_img_info.samples = VK_SAMPLE_COUNT_1_BIT;
    depth_img_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    depth_img_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    VmaAllocationCreateInfo depth_alloc_info{};
    depth_alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    depth_alloc_info.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    vmaCreateImage(_allocator, &depth_img_info, &depth_alloc_info,
                   &_depth_image.image, &_depth_image.allocation, nullptr);

    VkImageViewCreateInfo depth_view_info{};
    depth_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    depth_view_info.image = _depth_image.image;
    depth_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    depth_view_info.format = _depth_format;
    depth_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    depth_view_info.subresourceRange.baseMipLevel = 0;
    depth_view_info.subresourceRange.levelCount = 1;
    depth_view_info.subresourceRange.baseArrayLayer = 0;
    depth_view_info.subresourceRange.layerCount = 1;

    vkCreateImageView(_device, &depth_view_info, nullptr, &_depth_image.view);
}

void VulkanEngine::init_commands() {
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = _graphics_queue_family;

    if (vkCreateCommandPool(_device, &pool_info, nullptr, &_command_pool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool");
    }

    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = _command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(_device, &alloc_info, &_command_buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffer");
    }
}

void VulkanEngine::init_sync_objects() {
    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkSemaphoreCreateInfo sem_info{};
    sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    vkCreateFence(_device, &fence_info, nullptr, &_render_fence);
    vkCreateSemaphore(_device, &sem_info, nullptr, &_present_semaphore);
    vkCreateSemaphore(_device, &sem_info, nullptr, &_render_semaphore);
}

void VulkanEngine::init_render_pass() {
    VkAttachmentDescription color_attachment{};
    color_attachment.format = _swapchain_format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depth_attachment{};
    depth_attachment.format = _depth_format;
    depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference color_ref{};
    color_ref.attachment = 0;
    color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depth_ref{};
    depth_ref.attachment = 1;
    depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_ref;
    subpass.pDepthStencilAttachment = &depth_ref;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkAttachmentDescription attachments[] = {color_attachment, depth_attachment};

    VkRenderPassCreateInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = 2;
    render_pass_info.pAttachments = attachments;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = 1;
    render_pass_info.pDependencies = &dependency;

    if (vkCreateRenderPass(_device, &render_pass_info, nullptr, &_render_pass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass");
    }
}

void VulkanEngine::init_framebuffers() {
    _framebuffers.resize(_swapchain_image_views.size());

    for (size_t i = 0; i < _swapchain_image_views.size(); i++) {
        VkImageView attachments[] = {_swapchain_image_views[i], _depth_image.view};

        VkFramebufferCreateInfo fb_info{};
        fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fb_info.renderPass = _render_pass;
        fb_info.attachmentCount = 2;
        fb_info.pAttachments = attachments;
        fb_info.width = _width;
        fb_info.height = _height;
        fb_info.layers = 1;

        if (vkCreateFramebuffer(_device, &fb_info, nullptr, &_framebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create framebuffer");
        }
    }
}

void VulkanEngine::init_descriptors() {
    // Scene set layout (set 0): SceneUBO at binding 0
    _scene_set_layout = DescriptorLayoutBuilder()
        .add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                     VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
        .build(_device);

    // Material set layout (set 1): MaterialUBO at binding 0, albedo sampler at binding 1
    _material_set_layout = DescriptorLayoutBuilder()
        .add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
        .add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
        .build(_device);

    // Descriptor pool — enough for scene + many materials
    std::vector<VkDescriptorPoolSize> pool_sizes = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100},
    };
    _descriptor_allocator.init(_device, 100, pool_sizes);

    // Allocate scene descriptor set
    _scene_descriptor_set = _descriptor_allocator.allocate(_scene_set_layout);

    // Create scene UBO buffer
    _scene_ubo_buffer = create_buffer(sizeof(SceneUBO),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    // Write scene descriptor
    VkDescriptorBufferInfo scene_buf_info{};
    scene_buf_info.buffer = _scene_ubo_buffer.buffer;
    scene_buf_info.offset = 0;
    scene_buf_info.range = sizeof(SceneUBO);

    VkWriteDescriptorSet scene_write{};
    scene_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    scene_write.dstSet = _scene_descriptor_set;
    scene_write.dstBinding = 0;
    scene_write.descriptorCount = 1;
    scene_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    scene_write.pBufferInfo = &scene_buf_info;

    vkUpdateDescriptorSets(_device, 1, &scene_write, 0, nullptr);
}

void VulkanEngine::init_pipeline() {
    VkShaderModule vert_module = load_shader_module(_device, "shaders/mesh.vert.spv");
    VkShaderModule frag_module = load_shader_module(_device, "shaders/mesh.frag.spv");

    VkPipelineShaderStageCreateInfo vert_stage{};
    vert_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_stage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_stage.module = vert_module;
    vert_stage.pName = "main";

    VkPipelineShaderStageCreateInfo frag_stage{};
    frag_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_stage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_stage.module = frag_module;
    frag_stage.pName = "main";

    // Push constants: model matrix only (64 bytes), used in vertex stage
    VkPushConstantRange push_range{};
    push_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    push_range.offset = 0;
    push_range.size = sizeof(PushConstants);

    VkDescriptorSetLayout set_layouts[] = {_scene_set_layout, _material_set_layout};

    VkPipelineLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.setLayoutCount = 2;
    layout_info.pSetLayouts = set_layouts;
    layout_info.pushConstantRangeCount = 1;
    layout_info.pPushConstantRanges = &push_range;

    if (vkCreatePipelineLayout(_device, &layout_info, nullptr, &_pipeline_layout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout");
    }

    auto bindings = Vertex::get_binding_descriptions();
    auto attributes = Vertex::get_attribute_descriptions();

    PipelineBuilder builder;
    builder.set_defaults();
    builder.shader_stages = {vert_stage, frag_stage};
    builder.vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    builder.vertex_input_info.vertexBindingDescriptionCount = static_cast<uint32_t>(bindings.size());
    builder.vertex_input_info.pVertexBindingDescriptions = bindings.data();
    builder.vertex_input_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size());
    builder.vertex_input_info.pVertexAttributeDescriptions = attributes.data();
    builder.viewport = {0.0f, 0.0f, static_cast<float>(_width), static_cast<float>(_height), 0.0f, 1.0f};
    builder.scissor = {{0, 0}, {static_cast<uint32_t>(_width), static_cast<uint32_t>(_height)}};
    builder.pipeline_layout = _pipeline_layout;

    _pipeline = builder.build(_device, _render_pass);

    vkDestroyShaderModule(_device, vert_module, nullptr);
    vkDestroyShaderModule(_device, frag_module, nullptr);
}

void VulkanEngine::init_default_resources() {
    // Create default white 1x1 texture (index 0)
    auto submit_fn = [this](std::function<void(VkCommandBuffer)>&& fn) {
        immediate_submit(std::move(fn));
    };
    _textures.push_back(create_default_white_texture(_device, _allocator, submit_fn));

    // Create default material (index 0) — white, no texture
    create_material(MaterialUBO{}, 0);
}

void VulkanEngine::immediate_submit(std::function<void(VkCommandBuffer)>&& function) {
    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = _command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;
    vkAllocateCommandBuffers(_device, &alloc_info, &cmd);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin_info);

    function(cmd);

    vkEndCommandBuffer(cmd);

    VkFence fence;
    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    vkCreateFence(_device, &fence_info, nullptr, &fence);

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;

    vkQueueSubmit(_graphics_queue, 1, &submit, fence);
    vkWaitForFences(_device, 1, &fence, VK_TRUE, UINT64_MAX);

    vkDestroyFence(_device, fence, nullptr);
    vkFreeCommandBuffers(_device, _command_pool, 1, &cmd);
}

AllocatedBuffer VulkanEngine::create_buffer(size_t size, VkBufferUsageFlags usage, VmaMemoryUsage memory_usage) {
    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = usage;

    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage = memory_usage;

    AllocatedBuffer buffer;
    vmaCreateBuffer(_allocator, &buffer_info, &alloc_info, &buffer.buffer, &buffer.allocation, nullptr);
    return buffer;
}

uint32_t VulkanEngine::upload_mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) {
    Mesh mesh;
    mesh.index_count = static_cast<uint32_t>(indices.size());

    size_t vb_size = vertices.size() * sizeof(Vertex);
    mesh.vertex_buffer = create_buffer(vb_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    void* data;
    vmaMapMemory(_allocator, mesh.vertex_buffer.allocation, &data);
    memcpy(data, vertices.data(), vb_size);
    vmaUnmapMemory(_allocator, mesh.vertex_buffer.allocation);

    size_t ib_size = indices.size() * sizeof(uint32_t);
    mesh.index_buffer = create_buffer(ib_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    vmaMapMemory(_allocator, mesh.index_buffer.allocation, &data);
    memcpy(data, indices.data(), ib_size);
    vmaUnmapMemory(_allocator, mesh.index_buffer.allocation);

    uint32_t index = static_cast<uint32_t>(_meshes.size());
    _meshes.push_back(mesh);
    return index;
}

void VulkanEngine::update_scene_ubo(const SceneUBO& ubo) {
    void* data;
    vmaMapMemory(_allocator, _scene_ubo_buffer.allocation, &data);
    memcpy(data, &ubo, sizeof(SceneUBO));
    vmaUnmapMemory(_allocator, _scene_ubo_buffer.allocation);
}

uint32_t VulkanEngine::create_material(const MaterialUBO& props, uint32_t albedo_texture) {
    GPUMaterial mat;
    mat.properties = props;
    mat.albedo_texture = albedo_texture;

    // Create UBO buffer
    mat.ubo_buffer = create_buffer(sizeof(MaterialUBO),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    // Upload UBO data
    void* data;
    vmaMapMemory(_allocator, mat.ubo_buffer.allocation, &data);
    memcpy(data, &props, sizeof(MaterialUBO));
    vmaUnmapMemory(_allocator, mat.ubo_buffer.allocation);

    // Allocate descriptor set
    mat.descriptor_set = _descriptor_allocator.allocate(_material_set_layout);

    // Write descriptor
    write_material_descriptor(_device, mat, _textures[albedo_texture], _material_set_layout);

    uint32_t index = static_cast<uint32_t>(_materials.size());
    _materials.push_back(mat);
    return index;
}

void VulkanEngine::push_renderable(const Renderable& r) {
    _renderables.push_back(r);
}

void VulkanEngine::clear_renderables() {
    _renderables.clear();
}

void VulkanEngine::begin_frame() {
    vkWaitForFences(_device, 1, &_render_fence, VK_TRUE, UINT64_MAX);
    vkResetFences(_device, 1, &_render_fence);
}

void VulkanEngine::draw_frame() {
    uint32_t image_index;
    vkAcquireNextImageKHR(_device, _swapchain, UINT64_MAX, _present_semaphore, VK_NULL_HANDLE, &image_index);

    vkResetCommandBuffer(_command_buffer, 0);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(_command_buffer, &begin_info);

    VkClearValue clear_values[2];
    clear_values[0].color = {{0.05f, 0.05f, 0.08f, 1.0f}};
    clear_values[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo rp_info{};
    rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp_info.renderPass = _render_pass;
    rp_info.framebuffer = _framebuffers[image_index];
    rp_info.renderArea.offset = {0, 0};
    rp_info.renderArea.extent = {static_cast<uint32_t>(_width), static_cast<uint32_t>(_height)};
    rp_info.clearValueCount = 2;
    rp_info.pClearValues = clear_values;

    vkCmdBeginRenderPass(_command_buffer, &rp_info, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline);

    // Bind scene descriptor set (set 0) — once per frame
    vkCmdBindDescriptorSets(_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        _pipeline_layout, 0, 1, &_scene_descriptor_set, 0, nullptr);

    // Sort renderables by material to minimize descriptor switches
    std::sort(_renderables.begin(), _renderables.end(),
        [](const Renderable& a, const Renderable& b) {
            return a.material_index < b.material_index;
        });

    uint32_t bound_material = UINT32_MAX;

    for (const auto& renderable : _renderables) {
        // Bind material descriptor set (set 1) when material changes
        if (renderable.material_index != bound_material) {
            bound_material = renderable.material_index;
            const GPUMaterial& mat = _materials[bound_material];
            vkCmdBindDescriptorSets(_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                _pipeline_layout, 1, 1, &mat.descriptor_set, 0, nullptr);
        }

        const Mesh& mesh = _meshes[renderable.mesh_index];

        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(_command_buffer, 0, 1, &mesh.vertex_buffer.buffer, &offset);
        vkCmdBindIndexBuffer(_command_buffer, mesh.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);

        PushConstants pc{};
        pc.model = renderable.model;

        vkCmdPushConstants(_command_buffer, _pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT,
                           0, sizeof(PushConstants), &pc);
        vkCmdDrawIndexed(_command_buffer, mesh.index_count, 1, 0, 0, 0);
    }

    // ImGui rendering (inside render pass)
    if (_imgui_enabled && _imgui_render_cb) {
        _imgui_render_cb(_command_buffer);
    }

    vkCmdEndRenderPass(_command_buffer);
    vkEndCommandBuffer(_command_buffer);

    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &_present_semaphore;
    submit.pWaitDstStageMask = &wait_stage;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &_command_buffer;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &_render_semaphore;

    vkQueueSubmit(_graphics_queue, 1, &submit, _render_fence);

    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &_render_semaphore;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &_swapchain;
    present_info.pImageIndices = &image_index;

    vkQueuePresentKHR(_graphics_queue, &present_info);
}

bool VulkanEngine::should_close() const {
    return glfwWindowShouldClose(window);
}

void VulkanEngine::poll_events() const {
    glfwPollEvents();
}

void VulkanEngine::cleanup() {
    vkDeviceWaitIdle(_device);

    // Materials
    for (auto& mat : _materials) {
        vmaDestroyBuffer(_allocator, mat.ubo_buffer.buffer, mat.ubo_buffer.allocation);
    }

    // Textures
    for (auto& tex : _textures) {
        destroy_texture(_device, _allocator, tex);
    }

    // Meshes
    for (auto& mesh : _meshes) {
        vmaDestroyBuffer(_allocator, mesh.vertex_buffer.buffer, mesh.vertex_buffer.allocation);
        vmaDestroyBuffer(_allocator, mesh.index_buffer.buffer, mesh.index_buffer.allocation);
    }

    // Scene UBO
    vmaDestroyBuffer(_allocator, _scene_ubo_buffer.buffer, _scene_ubo_buffer.allocation);

    // Descriptors
    _descriptor_allocator.cleanup();
    vkDestroyDescriptorSetLayout(_device, _scene_set_layout, nullptr);
    vkDestroyDescriptorSetLayout(_device, _material_set_layout, nullptr);

    vkDestroyPipeline(_device, _pipeline, nullptr);
    vkDestroyPipelineLayout(_device, _pipeline_layout, nullptr);

    for (auto fb : _framebuffers) {
        vkDestroyFramebuffer(_device, fb, nullptr);
    }

    vkDestroyRenderPass(_device, _render_pass, nullptr);

    vkDestroyImageView(_device, _depth_image.view, nullptr);
    vmaDestroyImage(_allocator, _depth_image.image, _depth_image.allocation);

    for (auto view : _swapchain_image_views) {
        vkDestroyImageView(_device, view, nullptr);
    }

    vkDestroySwapchainKHR(_device, _swapchain, nullptr);

    vkDestroyFence(_device, _render_fence, nullptr);
    vkDestroySemaphore(_device, _present_semaphore, nullptr);
    vkDestroySemaphore(_device, _render_semaphore, nullptr);

    vkDestroyCommandPool(_device, _command_pool, nullptr);

    vmaDestroyAllocator(_allocator);
    vkDestroySurfaceKHR(_instance, _surface, nullptr);
    vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);
    vkDestroyDevice(_device, nullptr);
    vkDestroyInstance(_instance, nullptr);

    glfwDestroyWindow(window);
    glfwTerminate();
}
