#include "ecs/systems/hud_systems.h"
#include "ecs/components.h"

#include <string>
#include <imgui.h>

void register_hud_systems(flecs::world& world) {
    world.system("HUDSystem")
        .kind(flecs::OnStore)
        .run([&world](flecs::iter&) {
            // Look up player by name
            auto player = world.lookup("Player");

            float health = 100.0f;
            float max_health = 100.0f;
            int ammo = 0;
            int max_ammo = 0;
            float stamina = 100.0f;
            float max_stamina = 100.0f;

            if (player.is_alive()) {
                const auto* h = player.try_get<Health>();
                if (h) { health = h->current; max_health = h->max; }

                const auto* w = player.try_get<Weapon>();
                if (w) { ammo = w->ammo; max_ammo = w->max_ammo; }

                const auto* ms = player.try_get<MovementState>();
                if (ms) { stamina = ms->stamina; max_stamina = ms->max_stamina; }
            }

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

            // Health + Stamina bar
            ImGui::SetNextWindowPos(ImVec2(20, ImGui::GetIO().DisplaySize.y - 75));
            ImGui::SetNextWindowSize(ImVec2(250, 65));
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

            // Stamina bar
            float stamina_frac = stamina / max_stamina;
            ImVec4 stamina_color = stamina_frac > 0.3f
                ? ImVec4(0.2f, 0.6f, 0.9f, 1.0f)
                : ImVec4(0.9f, 0.4f, 0.1f, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, stamina_color);
            ImGui::ProgressBar(stamina_frac, ImVec2(200, 12), "");
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
