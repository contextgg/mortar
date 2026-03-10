#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include "renderer/model_loader.h"

#include <stb_image.h>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <unordered_map>

// Find an accessor for a given attribute type in a primitive
static cgltf_accessor* find_accessor(cgltf_primitive* prim, cgltf_attribute_type type, int index = 0) {
    for (cgltf_size i = 0; i < prim->attributes_count; i++) {
        if (prim->attributes[i].type == type && prim->attributes[i].index == index)
            return prim->attributes[i].data;
    }
    return nullptr;
}

// Read a mat4 from a cgltf accessor at the given index
static glm::mat4 read_mat4(cgltf_accessor* accessor, cgltf_size index) {
    float m[16];
    cgltf_accessor_read_float(accessor, index, m, 16);
    return glm::make_mat4(m);
}

// Build node-to-joint-index mapping for a skin
static std::unordered_map<cgltf_node*, int> build_joint_map(cgltf_skin* skin) {
    std::unordered_map<cgltf_node*, int> map;
    for (cgltf_size i = 0; i < skin->joints_count; i++) {
        map[skin->joints[i]] = static_cast<int>(i);
    }
    return map;
}

// Find parent joint index for a node within the skin's joint set
static int find_parent_joint(cgltf_node* node, const std::unordered_map<cgltf_node*, int>& joint_map) {
    if (!node->parent) return -1;
    auto it = joint_map.find(node->parent);
    if (it != joint_map.end()) return it->second;
    // Parent is not a joint — recurse up (handles intermediate non-joint nodes)
    return find_parent_joint(node->parent, joint_map);
}

// Extract local TRS from a cgltf_node
static void read_node_trs(cgltf_node* node, glm::vec3& pos, glm::quat& rot, glm::vec3& scale) {
    pos = glm::vec3(0.0f);
    rot = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    scale = glm::vec3(1.0f);

    if (node->has_translation) {
        pos = {node->translation[0], node->translation[1], node->translation[2]};
    }
    if (node->has_rotation) {
        // cgltf: [x, y, z, w], glm::quat constructor: (w, x, y, z)
        rot = glm::quat(node->rotation[3], node->rotation[0], node->rotation[1], node->rotation[2]);
    }
    if (node->has_scale) {
        scale = {node->scale[0], node->scale[1], node->scale[2]};
    }

    if (node->has_matrix) {
        // If the node uses a matrix instead of TRS, decompose it
        glm::mat4 mat = glm::make_mat4(node->matrix);
        pos = glm::vec3(mat[3]);
        scale = glm::vec3(
            glm::length(glm::vec3(mat[0])),
            glm::length(glm::vec3(mat[1])),
            glm::length(glm::vec3(mat[2]))
        );
        glm::mat3 rot_mat(
            glm::vec3(mat[0]) / scale.x,
            glm::vec3(mat[1]) / scale.y,
            glm::vec3(mat[2]) / scale.z
        );
        rot = glm::quat_cast(rot_mat);
    }
}

// Load skeleton data from a glTF skin
static std::shared_ptr<SkeletonData> load_skeleton(cgltf_data* data, cgltf_skin* skin) {
    auto skel = std::make_shared<SkeletonData>();
    auto joint_map = build_joint_map(skin);

    skel->joints.resize(skin->joints_count);

    for (cgltf_size i = 0; i < skin->joints_count; i++) {
        Joint& joint = skel->joints[i];
        cgltf_node* node = skin->joints[i];

        joint.name = node->name ? node->name : "";
        joint.parent = find_parent_joint(node, joint_map);
        read_node_trs(node, joint.local_position, joint.local_rotation, joint.local_scale);

        if (skin->inverse_bind_matrices) {
            joint.inverse_bind_matrix = read_mat4(skin->inverse_bind_matrices, i);
        }
    }

    // Load all animations
    for (cgltf_size a = 0; a < data->animations_count; a++) {
        cgltf_animation* anim = &data->animations[a];
        AnimationClip clip;
        clip.name = anim->name ? anim->name : "anim_" + std::to_string(a);
        clip.duration = 0.0f;

        for (cgltf_size c = 0; c < anim->channels_count; c++) {
            cgltf_animation_channel* channel = &anim->channels[c];
            cgltf_animation_sampler* sampler = channel->sampler;

            if (!channel->target_node) continue;

            // Find which joint this channel targets
            auto it = joint_map.find(channel->target_node);
            if (it == joint_map.end()) continue;
            int joint_idx = it->second;

            // Find or create JointAnimation for this joint
            JointAnimation* ja = nullptr;
            for (auto& existing : clip.channels) {
                if (existing.joint_index == joint_idx) {
                    ja = &existing;
                    break;
                }
            }
            if (!ja) {
                clip.channels.push_back({});
                ja = &clip.channels.back();
                ja->joint_index = joint_idx;
            }

            cgltf_accessor* times = sampler->input;
            cgltf_accessor* values = sampler->output;

            for (cgltf_size k = 0; k < times->count; k++) {
                float t;
                cgltf_accessor_read_float(times, k, &t, 1);
                if (t > clip.duration) clip.duration = t;

                switch (channel->target_path) {
                case cgltf_animation_path_type_translation: {
                    float v[3];
                    cgltf_accessor_read_float(values, k, v, 3);
                    ja->positions.push_back({t, {v[0], v[1], v[2]}});
                    break;
                }
                case cgltf_animation_path_type_rotation: {
                    float v[4];
                    cgltf_accessor_read_float(values, k, v, 4);
                    // glTF: [x,y,z,w], glm: (w,x,y,z)
                    ja->rotations.push_back({t, glm::quat(v[3], v[0], v[1], v[2])});
                    break;
                }
                case cgltf_animation_path_type_scale: {
                    float v[3];
                    cgltf_accessor_read_float(values, k, v, 3);
                    ja->scales.push_back({t, {v[0], v[1], v[2]}});
                    break;
                }
                default:
                    break;
                }
            }
        }

        skel->clips.push_back(std::move(clip));
    }

    return skel;
}

LoadedModel load_gltf(const std::string& path) {
    LoadedModel model;

    cgltf_options options = {};
    cgltf_data* data = nullptr;

    cgltf_result result = cgltf_parse_file(&options, path.c_str(), &data);
    if (result != cgltf_result_success) {
        std::cerr << "[model_loader] Failed to parse: " << path << std::endl;
        return model;
    }

    result = cgltf_load_buffers(&options, data, path.c_str());
    if (result != cgltf_result_success) {
        std::cerr << "[model_loader] Failed to load buffers: " << path << std::endl;
        cgltf_free(data);
        return model;
    }

    if (data->meshes_count == 0) {
        std::cerr << "[model_loader] No meshes found: " << path << std::endl;
        cgltf_free(data);
        return model;
    }

    // Determine which skin to use (first mesh node's skin, or first skin)
    cgltf_skin* skin = nullptr;
    for (cgltf_size i = 0; i < data->nodes_count && !skin; i++) {
        if (data->nodes[i].mesh == &data->meshes[0] && data->nodes[i].skin) {
            skin = data->nodes[i].skin;
        }
    }
    if (!skin && data->skins_count > 0) {
        skin = &data->skins[0];
    }

    // Load all primitives from all meshes into a single vertex/index buffer
    for (cgltf_size m = 0; m < data->meshes_count; m++) {
        cgltf_mesh* mesh = &data->meshes[m];

        for (cgltf_size p = 0; p < mesh->primitives_count; p++) {
            cgltf_primitive* prim = &mesh->primitives[p];

            if (prim->type != cgltf_primitive_type_triangles) continue;

            cgltf_accessor* pos_acc = find_accessor(prim, cgltf_attribute_type_position);
            if (!pos_acc) continue;

            cgltf_accessor* norm_acc = find_accessor(prim, cgltf_attribute_type_normal);
            cgltf_accessor* uv_acc = find_accessor(prim, cgltf_attribute_type_texcoord);
            cgltf_accessor* joints_acc = find_accessor(prim, cgltf_attribute_type_joints);
            cgltf_accessor* weights_acc = find_accessor(prim, cgltf_attribute_type_weights);
            cgltf_accessor* color_acc = find_accessor(prim, cgltf_attribute_type_color);

            uint32_t prim_vertex_base = static_cast<uint32_t>(model.vertices.size());

            for (cgltf_size v = 0; v < pos_acc->count; v++) {
                Vertex vert{};

                float pos[3];
                cgltf_accessor_read_float(pos_acc, v, pos, 3);
                vert.position = {pos[0], pos[1], pos[2]};

                if (norm_acc) {
                    float n[3];
                    cgltf_accessor_read_float(norm_acc, v, n, 3);
                    vert.normal = {n[0], n[1], n[2]};
                }

                if (uv_acc) {
                    float uv[2];
                    cgltf_accessor_read_float(uv_acc, v, uv, 2);
                    vert.uv = {uv[0], uv[1]};
                }

                if (color_acc) {
                    float c[4] = {1, 1, 1, 1};
                    cgltf_accessor_read_float(color_acc, v, c, 4);
                    vert.color = {c[0], c[1], c[2], c[3]};
                } else {
                    vert.color = glm::vec4(1.0f);
                }

                if (joints_acc) {
                    cgltf_uint j[4] = {0, 0, 0, 0};
                    cgltf_accessor_read_uint(joints_acc, v, j, 4);
                    vert.bone_indices = {j[0], j[1], j[2], j[3]};
                }

                if (weights_acc) {
                    float w[4] = {0, 0, 0, 0};
                    cgltf_accessor_read_float(weights_acc, v, w, 4);
                    vert.bone_weights = {w[0], w[1], w[2], w[3]};
                }

                model.vertices.push_back(vert);
            }

            // Read indices
            if (prim->indices) {
                for (cgltf_size i = 0; i < prim->indices->count; i++) {
                    cgltf_uint idx;
                    cgltf_accessor_read_uint(prim->indices, i, &idx, 1);
                    model.indices.push_back(prim_vertex_base + idx);
                }
            } else {
                // No indices — generate sequential
                for (cgltf_size i = 0; i < pos_acc->count; i++) {
                    model.indices.push_back(prim_vertex_base + static_cast<uint32_t>(i));
                }
            }
        }
    }

    // Load skeleton and animations
    if (skin) {
        model.skeleton = load_skeleton(data, skin);
    }

    // Extract embedded textures
    for (cgltf_size i = 0; i < data->images_count; i++) {
        cgltf_image* img = &data->images[i];
        EmbeddedImage tex{};

        const uint8_t* img_data = nullptr;
        size_t img_size = 0;

        if (img->buffer_view) {
            // Embedded in GLB binary chunk
            img_data = static_cast<const uint8_t*>(img->buffer_view->buffer->data)
                     + img->buffer_view->offset;
            img_size = img->buffer_view->size;
        }

        if (img_data && img_size > 0) {
            int w, h, channels;
            stbi_uc* pixels = stbi_load_from_memory(img_data, static_cast<int>(img_size),
                                                     &w, &h, &channels, STBI_rgb_alpha);
            if (pixels) {
                tex.width = w;
                tex.height = h;
                tex.pixels.assign(pixels, pixels + w * h * 4);
                stbi_image_free(pixels);
            }
        }

        model.textures.push_back(std::move(tex));
    }

    std::cout << "[model_loader] Loaded " << path
              << ": " << model.vertices.size() << " verts, "
              << model.indices.size() / 3 << " tris";
    if (model.skeleton) {
        std::cout << ", " << model.skeleton->joints.size() << " joints, "
                  << model.skeleton->clips.size() << " animations";
    }
    std::cout << ", " << model.textures.size() << " textures" << std::endl;

    cgltf_free(data);
    return model;
}

void load_gltf_animations(const std::string& path, SkeletonData& target_skeleton) {
    cgltf_options options = {};
    cgltf_data* data = nullptr;

    cgltf_result result = cgltf_parse_file(&options, path.c_str(), &data);
    if (result != cgltf_result_success) {
        std::cerr << "[model_loader] Failed to parse animations: " << path << std::endl;
        return;
    }

    result = cgltf_load_buffers(&options, data, path.c_str());
    if (result != cgltf_result_success) {
        std::cerr << "[model_loader] Failed to load anim buffers: " << path << std::endl;
        cgltf_free(data);
        return;
    }

    if (data->skins_count == 0 || data->animations_count == 0) {
        cgltf_free(data);
        return;
    }

    cgltf_skin* skin = &data->skins[0];
    auto source_joint_map = build_joint_map(skin);

    // Build name → target joint index mapping for remapping
    std::unordered_map<std::string, int> target_name_to_idx;
    for (size_t i = 0; i < target_skeleton.joints.size(); i++) {
        if (!target_skeleton.joints[i].name.empty()) {
            target_name_to_idx[target_skeleton.joints[i].name] = static_cast<int>(i);
        }
    }

    // Build source index → target index remap table
    std::vector<int> remap(skin->joints_count, -1);
    for (cgltf_size i = 0; i < skin->joints_count; i++) {
        const char* name = skin->joints[i]->name;
        if (name) {
            auto it = target_name_to_idx.find(name);
            if (it != target_name_to_idx.end()) {
                remap[i] = it->second;
            }
        }
    }

    // Load animations with remapped joint indices
    for (cgltf_size a = 0; a < data->animations_count; a++) {
        cgltf_animation* anim = &data->animations[a];
        AnimationClip clip;
        clip.name = anim->name ? anim->name : "anim_" + std::to_string(a);
        clip.duration = 0.0f;

        for (cgltf_size c = 0; c < anim->channels_count; c++) {
            cgltf_animation_channel* channel = &anim->channels[c];
            cgltf_animation_sampler* sampler = channel->sampler;

            if (!channel->target_node) continue;

            auto it = source_joint_map.find(channel->target_node);
            if (it == source_joint_map.end()) continue;
            int source_idx = it->second;

            // Remap to target skeleton
            int target_idx = (source_idx >= 0 && source_idx < static_cast<int>(remap.size()))
                           ? remap[source_idx] : -1;
            if (target_idx < 0) continue;

            JointAnimation* ja = nullptr;
            for (auto& existing : clip.channels) {
                if (existing.joint_index == target_idx) {
                    ja = &existing;
                    break;
                }
            }
            if (!ja) {
                clip.channels.push_back({});
                ja = &clip.channels.back();
                ja->joint_index = target_idx;
            }

            cgltf_accessor* times = sampler->input;
            cgltf_accessor* values = sampler->output;

            for (cgltf_size k = 0; k < times->count; k++) {
                float t;
                cgltf_accessor_read_float(times, k, &t, 1);
                if (t > clip.duration) clip.duration = t;

                switch (channel->target_path) {
                case cgltf_animation_path_type_translation: {
                    float v[3];
                    cgltf_accessor_read_float(values, k, v, 3);
                    ja->positions.push_back({t, {v[0], v[1], v[2]}});
                    break;
                }
                case cgltf_animation_path_type_rotation: {
                    float v[4];
                    cgltf_accessor_read_float(values, k, v, 4);
                    ja->rotations.push_back({t, glm::quat(v[3], v[0], v[1], v[2])});
                    break;
                }
                case cgltf_animation_path_type_scale: {
                    float v[3];
                    cgltf_accessor_read_float(values, k, v, 3);
                    ja->scales.push_back({t, {v[0], v[1], v[2]}});
                    break;
                }
                default:
                    break;
                }
            }
        }

        target_skeleton.clips.push_back(std::move(clip));
    }

    std::cout << "[model_loader] Loaded " << data->animations_count
              << " animations from " << path << std::endl;

    cgltf_free(data);
}
