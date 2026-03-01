#version 450

layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragUV;
layout(location = 3) in vec4 fragColor;

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

layout(std140, set = 1, binding = 0) uniform MaterialUBO {
    vec4 base_color;
    vec4 emissive;
    float metallic;
    float roughness;
} material;

layout(set = 1, binding = 1) uniform sampler2D albedoTex;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 N = normalize(fragNormal);
    vec3 V = normalize(scene.camera_position.xyz - fragWorldPos);

    // Sample albedo
    vec3 albedo = texture(albedoTex, fragUV).rgb * material.base_color.rgb * fragColor.rgb;

    // Ambient
    vec3 ambient = scene.ambient_color.rgb * scene.ambient_color.a * albedo;

    // Blinn-Phong shininess from roughness
    float shininess = mix(128.0, 8.0, material.roughness);

    // Directional light (sun)
    vec3 L = normalize(-scene.sun_direction.xyz);
    float NdotL = max(dot(N, L), 0.0);
    vec3 H = normalize(L + V);
    float NdotH = max(dot(N, H), 0.0);
    float spec = pow(NdotH, shininess);

    vec3 diffuse = NdotL * scene.sun_color.rgb * scene.sun_color.a * albedo;
    vec3 specular = spec * scene.sun_color.rgb * scene.sun_color.a * material.metallic;

    // Point lights
    for (int i = 0; i < scene.num_point_lights; i++) {
        vec3 light_pos = scene.point_positions[i].xyz;
        float radius = scene.point_positions[i].w;
        vec3 light_color = scene.point_colors[i].rgb;
        float intensity = scene.point_colors[i].a;

        vec3 PL = light_pos - fragWorldPos;
        float dist = length(PL);
        vec3 PL_dir = PL / max(dist, 0.001);
        float attenuation = max(1.0 - dist / max(radius, 0.001), 0.0);
        attenuation *= attenuation;

        float pNdotL = max(dot(N, PL_dir), 0.0);
        vec3 pH = normalize(PL_dir + V);
        float pNdotH = max(dot(N, pH), 0.0);
        float pSpec = pow(pNdotH, shininess);

        diffuse += pNdotL * light_color * intensity * attenuation * albedo;
        specular += pSpec * light_color * intensity * attenuation * material.metallic;
    }

    vec3 color = ambient + diffuse + specular + material.emissive.rgb;
    outColor = vec4(color, material.base_color.a * fragColor.a);
}
