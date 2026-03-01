#include "renderer/particle_renderer.h"
#include "renderer/pipeline.h"

void ParticleRenderer::init(VkDevice device, VkRenderPass render_pass,
                             VkDescriptorSetLayout scene_set_layout,
                             VkPipelineLayout& out_layout, VkPipeline& out_pipeline) {
    // Particle pipeline will use the mesh pipeline for now.
    // Full billboard particle pipeline with geometry shader or instancing
    // is a future enhancement. Particles are rendered as small cubes.
    (void)device; (void)render_pass; (void)scene_set_layout;
    out_layout = VK_NULL_HANDLE;
    out_pipeline = VK_NULL_HANDLE;
}

void ParticleRenderer::cleanup(VkDevice /*device*/) {
    _particles.clear();
}

void ParticleRenderer::push_particle(const ParticleInstance& p) {
    _particles.push_back(p);
}

void ParticleRenderer::clear() {
    _particles.clear();
}
