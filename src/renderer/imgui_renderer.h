#pragma once

#include <vulkan/vulkan.h>

struct GLFWwindow;

class ImGuiRenderer {
public:
    void init(GLFWwindow* window, VkInstance instance, VkPhysicalDevice physical_device,
              VkDevice device, uint32_t queue_family, VkQueue graphics_queue,
              VkRenderPass render_pass, uint32_t image_count);
    void shutdown();

    void new_frame();
    void render(VkCommandBuffer cmd);

private:
    VkDevice _device = VK_NULL_HANDLE;
    VkDescriptorPool _imgui_pool = VK_NULL_HANDLE;
};
