#include "renderer/descriptors.h"
#include <stdexcept>

DescriptorLayoutBuilder& DescriptorLayoutBuilder::add_binding(uint32_t binding, VkDescriptorType type, VkShaderStageFlags stages) {
    VkDescriptorSetLayoutBinding b{};
    b.binding = binding;
    b.descriptorType = type;
    b.descriptorCount = 1;
    b.stageFlags = stages;
    _bindings.push_back(b);
    return *this;
}

VkDescriptorSetLayout DescriptorLayoutBuilder::build(VkDevice device) {
    VkDescriptorSetLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = static_cast<uint32_t>(_bindings.size());
    info.pBindings = _bindings.data();

    VkDescriptorSetLayout layout;
    if (vkCreateDescriptorSetLayout(device, &info, nullptr, &layout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor set layout");
    }
    return layout;
}

void DescriptorAllocator::init(VkDevice device, uint32_t max_sets, const std::vector<VkDescriptorPoolSize>& pool_sizes) {
    _device = device;

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.maxSets = max_sets;
    pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
    pool_info.pPoolSizes = pool_sizes.data();

    if (vkCreateDescriptorPool(_device, &pool_info, nullptr, &_pool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool");
    }
}

VkDescriptorSet DescriptorAllocator::allocate(VkDescriptorSetLayout layout) {
    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = _pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &layout;

    VkDescriptorSet set;
    if (vkAllocateDescriptorSets(_device, &alloc_info, &set) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor set");
    }
    return set;
}

void DescriptorAllocator::cleanup() {
    if (_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(_device, _pool, nullptr);
        _pool = VK_NULL_HANDLE;
    }
}
