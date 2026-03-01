#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec4 inColor;

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

layout(push_constant) uniform PushConstants {
    mat4 model;
} pc;

layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragUV;
layout(location = 3) out vec4 fragColor;

void main() {
    vec4 worldPos = pc.model * vec4(inPosition, 1.0);
    gl_Position = scene.view_projection * worldPos;
    fragWorldPos = worldPos.xyz;
    fragNormal = normalize(mat3(pc.model) * inNormal);
    fragUV = inUV;
    fragColor = inColor;
}
