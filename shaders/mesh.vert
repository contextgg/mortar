#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec4 inColor;
layout(location = 4) in uvec4 inBoneIndices;
layout(location = 5) in vec4 inBoneWeights;

layout(std140, set = 0, binding = 0) uniform SceneUBO {
    mat4 view;
    mat4 projection;
    mat4 view_projection;
    vec4 camera_position;
    vec4 ambient_color;
    vec4 sun_direction;
    vec4 sun_color;
    vec4 point_positions[4];
    vec4 point_colors[4];
    int num_point_lights;
} scene;

layout(std430, set = 2, binding = 0) readonly buffer BoneSSBO {
    mat4 bones[];
} bone_data;

layout(push_constant) uniform PushConstants {
    mat4 model;
    uint bone_offset;
    uint bone_count;
} pc;

layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragUV;
layout(location = 3) out vec4 fragColor;

void main() {
    vec3 localPos = inPosition;
    vec3 localNormal = inNormal;

    if (pc.bone_count > 0) {
        mat4 skin = inBoneWeights.x * bone_data.bones[pc.bone_offset + inBoneIndices.x]
                  + inBoneWeights.y * bone_data.bones[pc.bone_offset + inBoneIndices.y]
                  + inBoneWeights.z * bone_data.bones[pc.bone_offset + inBoneIndices.z]
                  + inBoneWeights.w * bone_data.bones[pc.bone_offset + inBoneIndices.w];

        localPos = (skin * vec4(inPosition, 1.0)).xyz;
        localNormal = mat3(skin) * inNormal;
    }

    vec4 worldPos = pc.model * vec4(localPos, 1.0);
    gl_Position = scene.view_projection * worldPos;
    fragWorldPos = worldPos.xyz;
    fragNormal = normalize(mat3(pc.model) * localNormal);
    fragUV = inUV;
    fragColor = inColor;
}
