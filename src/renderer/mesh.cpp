#include "renderer/mesh.h"

std::vector<VkVertexInputBindingDescription> Vertex::get_binding_descriptions() {
    return {{
        .binding = 0,
        .stride = sizeof(Vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    }};
}

std::vector<VkVertexInputAttributeDescription> Vertex::get_attribute_descriptions() {
    return {
        {
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(Vertex, position),
        },
        {
            .location = 1,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(Vertex, normal),
        },
        {
            .location = 2,
            .binding = 0,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = offsetof(Vertex, uv),
        },
        {
            .location = 3,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .offset = offsetof(Vertex, color),
        },
        {
            .location = 4,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32A32_UINT,
            .offset = offsetof(Vertex, bone_indices),
        },
        {
            .location = 5,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .offset = offsetof(Vertex, bone_weights),
        },
    };
}

std::vector<Vertex> cube_vertices() {
    const glm::vec4 white{1.0f};

    return {
        // Front face (+Z), normal (0,0,1)
        {{-0.5f, -0.5f,  0.5f}, { 0, 0, 1}, {0, 0}, white},
        {{ 0.5f, -0.5f,  0.5f}, { 0, 0, 1}, {1, 0}, white},
        {{ 0.5f,  0.5f,  0.5f}, { 0, 0, 1}, {1, 1}, white},
        {{-0.5f,  0.5f,  0.5f}, { 0, 0, 1}, {0, 1}, white},

        // Back face (-Z), normal (0,0,-1)
        {{ 0.5f, -0.5f, -0.5f}, { 0, 0,-1}, {0, 0}, white},
        {{-0.5f, -0.5f, -0.5f}, { 0, 0,-1}, {1, 0}, white},
        {{-0.5f,  0.5f, -0.5f}, { 0, 0,-1}, {1, 1}, white},
        {{ 0.5f,  0.5f, -0.5f}, { 0, 0,-1}, {0, 1}, white},

        // Top face (+Y), normal (0,1,0)
        {{-0.5f,  0.5f,  0.5f}, { 0, 1, 0}, {0, 0}, white},
        {{ 0.5f,  0.5f,  0.5f}, { 0, 1, 0}, {1, 0}, white},
        {{ 0.5f,  0.5f, -0.5f}, { 0, 1, 0}, {1, 1}, white},
        {{-0.5f,  0.5f, -0.5f}, { 0, 1, 0}, {0, 1}, white},

        // Bottom face (-Y), normal (0,-1,0)
        {{-0.5f, -0.5f, -0.5f}, { 0,-1, 0}, {0, 0}, white},
        {{ 0.5f, -0.5f, -0.5f}, { 0,-1, 0}, {1, 0}, white},
        {{ 0.5f, -0.5f,  0.5f}, { 0,-1, 0}, {1, 1}, white},
        {{-0.5f, -0.5f,  0.5f}, { 0,-1, 0}, {0, 1}, white},

        // Right face (+X), normal (1,0,0)
        {{ 0.5f, -0.5f,  0.5f}, { 1, 0, 0}, {0, 0}, white},
        {{ 0.5f, -0.5f, -0.5f}, { 1, 0, 0}, {1, 0}, white},
        {{ 0.5f,  0.5f, -0.5f}, { 1, 0, 0}, {1, 1}, white},
        {{ 0.5f,  0.5f,  0.5f}, { 1, 0, 0}, {0, 1}, white},

        // Left face (-X), normal (-1,0,0)
        {{-0.5f, -0.5f, -0.5f}, {-1, 0, 0}, {0, 0}, white},
        {{-0.5f, -0.5f,  0.5f}, {-1, 0, 0}, {1, 0}, white},
        {{-0.5f,  0.5f,  0.5f}, {-1, 0, 0}, {1, 1}, white},
        {{-0.5f,  0.5f, -0.5f}, {-1, 0, 0}, {0, 1}, white},
    };
}

std::vector<uint32_t> cube_indices() {
    return {
         0,  1,  2,  2,  3,  0,  // Front
         4,  5,  6,  6,  7,  4,  // Back
         8,  9, 10, 10, 11,  8,  // Top
        12, 13, 14, 14, 15, 12,  // Bottom
        16, 17, 18, 18, 19, 16,  // Right
        20, 21, 22, 22, 23, 20,  // Left
    };
}

std::vector<Vertex> plane_vertices(float half_extent, float uv_scale) {
    const glm::vec3 up{0, 1, 0};
    const glm::vec4 white{1.0f};

    return {
        {{-half_extent, 0, -half_extent}, up, {0,        0},        white},
        {{ half_extent, 0, -half_extent}, up, {uv_scale, 0},        white},
        {{ half_extent, 0,  half_extent}, up, {uv_scale, uv_scale}, white},
        {{-half_extent, 0,  half_extent}, up, {0,        uv_scale}, white},
    };
}

std::vector<uint32_t> plane_indices() {
    return {0, 1, 2, 2, 3, 0};
}

// Helper: append a box with given half-extents and center offset to vertex/index vectors
static void add_box(std::vector<Vertex>& verts, std::vector<uint32_t>& idxs,
                    glm::vec3 half, glm::vec3 center, glm::vec4 color) {
    uint32_t base = static_cast<uint32_t>(verts.size());
    float hx = half.x, hy = half.y, hz = half.z;
    float cx = center.x, cy = center.y, cz = center.z;

    // Front (+Z)
    verts.push_back({{cx - hx, cy - hy, cz + hz}, { 0, 0, 1}, {0, 0}, color});
    verts.push_back({{cx + hx, cy - hy, cz + hz}, { 0, 0, 1}, {1, 0}, color});
    verts.push_back({{cx + hx, cy + hy, cz + hz}, { 0, 0, 1}, {1, 1}, color});
    verts.push_back({{cx - hx, cy + hy, cz + hz}, { 0, 0, 1}, {0, 1}, color});
    // Back (-Z)
    verts.push_back({{cx + hx, cy - hy, cz - hz}, { 0, 0,-1}, {0, 0}, color});
    verts.push_back({{cx - hx, cy - hy, cz - hz}, { 0, 0,-1}, {1, 0}, color});
    verts.push_back({{cx - hx, cy + hy, cz - hz}, { 0, 0,-1}, {1, 1}, color});
    verts.push_back({{cx + hx, cy + hy, cz - hz}, { 0, 0,-1}, {0, 1}, color});
    // Top (+Y)
    verts.push_back({{cx - hx, cy + hy, cz + hz}, { 0, 1, 0}, {0, 0}, color});
    verts.push_back({{cx + hx, cy + hy, cz + hz}, { 0, 1, 0}, {1, 0}, color});
    verts.push_back({{cx + hx, cy + hy, cz - hz}, { 0, 1, 0}, {1, 1}, color});
    verts.push_back({{cx - hx, cy + hy, cz - hz}, { 0, 1, 0}, {0, 1}, color});
    // Bottom (-Y)
    verts.push_back({{cx - hx, cy - hy, cz - hz}, { 0,-1, 0}, {0, 0}, color});
    verts.push_back({{cx + hx, cy - hy, cz - hz}, { 0,-1, 0}, {1, 0}, color});
    verts.push_back({{cx + hx, cy - hy, cz + hz}, { 0,-1, 0}, {1, 1}, color});
    verts.push_back({{cx - hx, cy - hy, cz + hz}, { 0,-1, 0}, {0, 1}, color});
    // Right (+X)
    verts.push_back({{cx + hx, cy - hy, cz + hz}, { 1, 0, 0}, {0, 0}, color});
    verts.push_back({{cx + hx, cy - hy, cz - hz}, { 1, 0, 0}, {1, 0}, color});
    verts.push_back({{cx + hx, cy + hy, cz - hz}, { 1, 0, 0}, {1, 1}, color});
    verts.push_back({{cx + hx, cy + hy, cz + hz}, { 1, 0, 0}, {0, 1}, color});
    // Left (-X)
    verts.push_back({{cx - hx, cy - hy, cz - hz}, {-1, 0, 0}, {0, 0}, color});
    verts.push_back({{cx - hx, cy - hy, cz + hz}, {-1, 0, 0}, {1, 0}, color});
    verts.push_back({{cx - hx, cy + hy, cz + hz}, {-1, 0, 0}, {1, 1}, color});
    verts.push_back({{cx - hx, cy + hy, cz - hz}, {-1, 0, 0}, {0, 1}, color});

    for (uint32_t face = 0; face < 6; face++) {
        uint32_t f = base + face * 4;
        idxs.push_back(f); idxs.push_back(f + 1); idxs.push_back(f + 2);
        idxs.push_back(f + 2); idxs.push_back(f + 3); idxs.push_back(f);
    }
}

std::vector<Vertex> humanoid_vertices() {
    std::vector<Vertex> verts;
    std::vector<uint32_t> idxs; // unused here, but add_box needs both
    const glm::vec4 skin{0.9f, 0.75f, 0.6f, 1.0f};
    const glm::vec4 shirt{0.3f, 0.5f, 0.8f, 1.0f};
    const glm::vec4 pants{0.25f, 0.25f, 0.35f, 1.0f};

    add_box(verts, idxs, {0.125f, 0.125f, 0.125f}, {0.0f, 1.575f, 0.0f}, skin);   // Head
    add_box(verts, idxs, {0.2f,   0.25f,  0.1f},   {0.0f, 1.2f,   0.0f}, shirt);  // Torso
    add_box(verts, idxs, {0.06f,  0.225f, 0.06f},   {-0.26f, 1.175f, 0.0f}, shirt); // Left arm
    add_box(verts, idxs, {0.06f,  0.225f, 0.06f},   {0.26f,  1.175f, 0.0f}, shirt); // Right arm
    add_box(verts, idxs, {0.075f, 0.225f, 0.075f},  {-0.1f,  0.225f, 0.0f}, pants); // Left leg
    add_box(verts, idxs, {0.075f, 0.225f, 0.075f},  {0.1f,   0.225f, 0.0f}, pants); // Right leg

    const glm::vec4 gun_metal{0.15f, 0.15f, 0.15f, 1.0f};
    const glm::vec4 gun_grip{0.3f, 0.2f, 0.1f, 1.0f};
    // Gun barrel — long thin box extending forward from right hand
    add_box(verts, idxs, {0.025f, 0.025f, 0.2f},   {0.26f,  1.0f,  0.25f}, gun_metal);  // Barrel
    // Gun body/receiver
    add_box(verts, idxs, {0.035f, 0.05f,  0.1f},    {0.26f,  1.0f,  0.05f}, gun_metal);  // Receiver
    // Grip
    add_box(verts, idxs, {0.025f, 0.06f,  0.025f},  {0.26f,  0.935f, 0.0f}, gun_grip);   // Grip

    return verts;
}

std::vector<uint32_t> humanoid_indices() {
    std::vector<Vertex> verts;
    std::vector<uint32_t> idxs;
    const glm::vec4 dummy{1.0f};

    // Generate indices by running the same box sequence
    add_box(verts, idxs, {0.125f, 0.125f, 0.125f}, {0.0f, 1.575f, 0.0f}, dummy);
    add_box(verts, idxs, {0.2f,   0.25f,  0.1f},   {0.0f, 1.2f,   0.0f}, dummy);
    add_box(verts, idxs, {0.06f,  0.225f, 0.06f},   {-0.26f, 1.175f, 0.0f}, dummy);
    add_box(verts, idxs, {0.06f,  0.225f, 0.06f},   {0.26f,  1.175f, 0.0f}, dummy);
    add_box(verts, idxs, {0.075f, 0.225f, 0.075f},  {-0.1f,  0.225f, 0.0f}, dummy);
    add_box(verts, idxs, {0.075f, 0.225f, 0.075f},  {0.1f,   0.225f, 0.0f}, dummy);
    // Gun parts (must match humanoid_vertices)
    add_box(verts, idxs, {0.025f, 0.025f, 0.2f},   {0.26f,  1.0f,  0.25f}, dummy);
    add_box(verts, idxs, {0.035f, 0.05f,  0.1f},    {0.26f,  1.0f,  0.05f}, dummy);
    add_box(verts, idxs, {0.025f, 0.06f,  0.025f},  {0.26f,  0.935f, 0.0f}, dummy);

    return idxs;
}
