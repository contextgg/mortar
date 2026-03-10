#pragma once

#include <vector>
#include <string>
#include <functional>

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <glm/glm.hpp>
#include <GLFW/glfw3.h>
#include <VkBootstrap.h>

#include "core/types.h"
#include "renderer/mesh.h"
#include "renderer/descriptors.h"

struct Renderable {
    glm::mat4 model;
    uint32_t mesh_index;
    uint32_t material_index;
    uint32_t bone_offset = 0;
    uint32_t bone_count = 0;
};

struct PushConstants {
    glm::mat4 model;
    uint32_t bone_offset = 0;
    uint32_t bone_count = 0;
};

class VulkanEngine {
public:
    void init(int width = 1280, int height = 720, const std::string& title = "Mortar");
    void begin_frame();
    void draw_frame();
    void cleanup();

    bool should_close() const;
    void poll_events() const;

    void push_renderable(const Renderable& r);
    void clear_renderables();

    uint32_t upload_mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);

    // Bone SSBO — upload bone matrices for skinned meshes, returns offset into SSBO
    uint32_t upload_bones(const glm::mat4* matrices, uint32_t count);
    void reset_bone_data();

    // Scene UBO — updated each frame by the SceneUBOUpdateSystem
    void update_scene_ubo(const SceneUBO& ubo);

    // Material creation — returns material index
    uint32_t create_material(const MaterialUBO& props, uint32_t albedo_texture = 0);

    // Texture upload — returns texture index
    uint32_t upload_texture(const uint8_t* pixels, int width, int height);
    uint32_t load_texture(const std::string& path);
    uint32_t default_texture_index() const { return 0; }

    // Immediate GPU command submission (for uploads)
    void immediate_submit(std::function<void(VkCommandBuffer)>&& function);

    GLFWwindow* window = nullptr;

    int width() const { return _width; }
    int height() const { return _height; }

    // Vulkan handle accessors (for ImGui init)
    VkInstance instance() const { return _instance; }
    VkPhysicalDevice physical_device() const { return _physical_device; }
    VkDevice device() const { return _device; }
    uint32_t graphics_queue_family() const { return _graphics_queue_family; }
    VkQueue graphics_queue() const { return _graphics_queue; }
    VkRenderPass render_pass() const { return _render_pass; }
    uint32_t swapchain_image_count() const { return static_cast<uint32_t>(_swapchain_images.size()); }

    // ImGui integration
    void set_imgui_enabled(bool enabled) { _imgui_enabled = enabled; }
    using ImGuiRenderCallback = std::function<void(VkCommandBuffer)>;
    void set_imgui_render_callback(ImGuiRenderCallback cb) { _imgui_render_cb = std::move(cb); }

private:
    void init_vulkan();
    void init_swapchain();
    void init_commands();
    void init_sync_objects();
    void init_render_pass();
    void init_framebuffers();
    void init_descriptors();
    void init_pipeline();
    void init_default_resources();

    AllocatedBuffer create_buffer(size_t size, VkBufferUsageFlags usage, VmaMemoryUsage memory_usage);

    int _width = 1280;
    int _height = 720;

    // Vulkan core
    VkInstance _instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT _debug_messenger = VK_NULL_HANDLE;
    VkPhysicalDevice _physical_device = VK_NULL_HANDLE;
    VkDevice _device = VK_NULL_HANDLE;
    VkSurfaceKHR _surface = VK_NULL_HANDLE;
    VkQueue _graphics_queue = VK_NULL_HANDLE;
    uint32_t _graphics_queue_family = 0;
    VmaAllocator _allocator = nullptr;

    // Swapchain
    VkSwapchainKHR _swapchain = VK_NULL_HANDLE;
    VkFormat _swapchain_format{};
    std::vector<VkImage> _swapchain_images;
    std::vector<VkImageView> _swapchain_image_views;

    // Depth buffer
    AllocatedImage _depth_image;
    VkFormat _depth_format = VK_FORMAT_D32_SFLOAT;

    // Render pass + framebuffers
    VkRenderPass _render_pass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> _framebuffers;

    // Pipeline
    VkPipelineLayout _pipeline_layout = VK_NULL_HANDLE;
    VkPipeline _pipeline = VK_NULL_HANDLE;

    // Descriptor infrastructure
    DescriptorAllocator _descriptor_allocator;
    VkDescriptorSetLayout _scene_set_layout = VK_NULL_HANDLE;
    VkDescriptorSetLayout _material_set_layout = VK_NULL_HANDLE;
    VkDescriptorSetLayout _bone_set_layout = VK_NULL_HANDLE;
    VkDescriptorSet _scene_descriptor_set = VK_NULL_HANDLE;
    VkDescriptorSet _bone_descriptor_set = VK_NULL_HANDLE;
    AllocatedBuffer _scene_ubo_buffer;

    // Bone SSBO for skeletal animation
    static constexpr uint32_t MAX_BONE_MATRICES = 8192;
    AllocatedBuffer _bone_ssbo;
    void* _bone_ssbo_mapped = nullptr;
    uint32_t _bone_write_offset = 0;

    // Commands
    VkCommandPool _command_pool = VK_NULL_HANDLE;
    VkCommandBuffer _command_buffer = VK_NULL_HANDLE;

    // Sync
    VkFence _render_fence = VK_NULL_HANDLE;
    VkSemaphore _present_semaphore = VK_NULL_HANDLE;
    VkSemaphore _render_semaphore = VK_NULL_HANDLE;

    // Mesh registry
    std::vector<Mesh> _meshes;

    // Texture registry
    std::vector<Texture> _textures;

    // Material registry
    std::vector<GPUMaterial> _materials;

    // Per-frame draw list
    std::vector<Renderable> _renderables;

    // ImGui
    bool _imgui_enabled = false;
    ImGuiRenderCallback _imgui_render_cb;
};
