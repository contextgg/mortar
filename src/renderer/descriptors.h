#pragma once

#include <vector>
#include <vulkan/vulkan.h>

class DescriptorLayoutBuilder {
public:
    DescriptorLayoutBuilder& add_binding(uint32_t binding, VkDescriptorType type, VkShaderStageFlags stages);
    VkDescriptorSetLayout build(VkDevice device);

private:
    std::vector<VkDescriptorSetLayoutBinding> _bindings;
};

class DescriptorAllocator {
public:
    void init(VkDevice device, uint32_t max_sets, const std::vector<VkDescriptorPoolSize>& pool_sizes);
    VkDescriptorSet allocate(VkDescriptorSetLayout layout);
    void cleanup();

private:
    VkDevice _device = VK_NULL_HANDLE;
    VkDescriptorPool _pool = VK_NULL_HANDLE;
};
