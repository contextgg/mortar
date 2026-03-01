#include "ecs/systems/hud_systems.h"
#include "ecs/components.h"

#include <string>
#include <imgui.h>

void register_hud_systems(flecs::world& world) {
    world.system("HUDSystem")
        .kind(flecs::OnStore)
        .run([&world](flecs::iter&) {
            // Get player health and weapon info
            float health = 100.0f;
            float max_health = 100.0f;
            int ammo = 0;
            int max_ammo = 0;

            world.each([&](const Player&, const Health& h) {
                health = h.current;
                max_health = h.max;
            });

            world.each([&](const Player&, const Weapon& w) {
                ammo = w.ammo;
                max_ammo = w.max_ammo;
            });

            // Crosshair
            ImDrawList* draw = ImGui::GetBackgroundDrawList();
            ImVec2 center = ImGui::GetMainViewport()->GetCenter();
            float cross_size = 10.0f;
            float cross_thick = 2.0f;
            ImU32 cross_color = IM_COL32(255, 255, 255, 200);

            draw->AddLine(ImVec2(center.x - cross_size, center.y),
                          ImVec2(center.x + cross_size, center.y),
                          cross_color, cross_thick);
            draw->AddLine(ImVec2(center.x, center.y - cross_size),
                          ImVec2(center.x, center.y + cross_size),
                          cross_color, cross_thick);

            // Health bar
            ImGui::SetNextWindowPos(ImVec2(20, ImGui::GetIO().DisplaySize.y - 60));
            ImGui::SetNextWindowSize(ImVec2(250, 50));
            ImGui::Begin("##Health", nullptr,
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoBackground);

            float health_frac = health / max_health;
            ImVec4 health_color = health_frac > 0.5f
                ? ImVec4(0.2f, 0.8f, 0.2f, 1.0f)
                : (health_frac > 0.25f
                    ? ImVec4(0.9f, 0.7f, 0.1f, 1.0f)
                    : ImVec4(0.9f, 0.15f, 0.1f, 1.0f));

            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, health_color);
            ImGui::ProgressBar(health_frac, ImVec2(200, 20),
                ("HP: " + std::to_string(static_cast<int>(health)) + "/" +
                 std::to_string(static_cast<int>(max_health))).c_str());
            ImGui::PopStyleColor();
            ImGui::End();

            // Ammo counter
            ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 170,
                                            ImGui::GetIO().DisplaySize.y - 60));
            ImGui::SetNextWindowSize(ImVec2(150, 50));
            ImGui::Begin("##Ammo", nullptr,
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoBackground);

            ImGui::SetWindowFontScale(1.5f);
            ImGui::TextColored(ImVec4(1, 1, 1, 0.9f), "%d / %d", ammo, max_ammo);
            ImGui::End();

            // Enemy count
            int enemy_count = 0;
            world.each([&enemy_count](const Enemy&) {
                enemy_count++;
            });

            ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 170, 10));
            ImGui::SetNextWindowSize(ImVec2(150, 40));
            ImGui::Begin("##Enemies", nullptr,
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoBackground);

            ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 0.9f), "Enemies: %d", enemy_count);
            ImGui::End();
        });
}
