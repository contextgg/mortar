#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include "core/types.h"

struct ParticleVertex {
    glm::vec3 position;
    glm::vec4 color;
    float size;
};

struct ParticleInstance {
    glm::vec3 position;
    glm::vec4 color;
    float size;
};

class ParticleRenderer {
public:
    void init(VkDevice device, VkRenderPass render_pass, VkDescriptorSetLayout scene_set_layout,
              VkPipelineLayout& out_layout, VkPipeline& out_pipeline);
    void cleanup(VkDevice device);

    void push_particle(const ParticleInstance& p);
    void clear();
    const std::vector<ParticleInstance>& particles() const { return _particles; }

private:
    std::vector<ParticleInstance> _particles;
};
