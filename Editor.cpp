#include "Editor.h"
#include "Level.h"
#include "./imgui/imgui.h"
#include "./imgui/imgui_impl_win32.h"
#include "./imgui/imgui_impl_dx11.h"

void RenderImGuiDebugWindow()
{
    ImGui_ImplDX11_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame();
    ImGui::Begin("Game Engine - TorrouDX");
    ImGui::Text("STATUS"); ImGui::Text("Blocos: %d | Score: %d", blocksRemaining, score);
    ImGui::Separator();

    ImGui::Text("TWEAKS AO VIVO (Cheat)");
    ImGui::SliderFloat("Velocidade Y Bola", &ballVelY, 0.0f, 0.1f);
    static bool godMode = false; if (ImGui::Checkbox("God Mode", &godMode)) { life = godMode ? 999 : 3; } ImGui::SameLine();
    if (ImGui::Button("Resetar Bola")) { ballX = 0.0f; ballY = 0.0f; ballVelX = 0.0f; ballVelY = 0.02f; }
    ImGui::Separator();

    ImGui::Text("EDITOR DE FASES (MOUSE)");
    ImGui::Checkbox("Habilitar Modo de Edicao", &modoEditor);

    if (modoEditor)
    {
        static int ferramentaAtual = 0;
        ImGui::RadioButton("Colocar Blocos (Inimigos)", &ferramentaAtual, 0); ImGui::SameLine();
        ImGui::RadioButton("Colocar Paredes", &ferramentaAtual, 1);

        static int editorHits = 1; static float obsW = 0.3f; static float obsH = 0.02f;
        static int editorBulletCount = 3;
        static int editorBulletPattern = 0;

        if (ferramentaAtual == 0) {
            ImGui::SliderInt("Vida do Bloco (1=Cinz, 3=Prt)", &editorHits, 1, 3);
            ImGui::SliderInt("Qtd de Tiros", &editorBulletCount, 0, 40);
            const char* patternNames[] = { "0: Disperso (Cone)", "1: Focado (Linha)", "2: Circular (Ring)", "3: Espiral (Swirl)" };
            ImGui::Combo("Padrao de Tiro", &editorBulletPattern, patternNames, 4);
        }
        else {
            ImGui::SliderFloat("Largura Parede", &obsW, 0.05f, 1.0f); ImGui::SliderFloat("Altura Parede", &obsH, 0.01f, 0.5f);
        }

        static int editorStage = 0; ImGui::InputInt("ID da Fase (Salvar)", &editorStage); if (editorStage < 0) editorStage = 0;
        char filename[64]; snprintf(filename, sizeof(filename), "stage%d.txt", editorStage);

        if (ImGui::Button("Salvar Fase")) { SaveLevel(filename); } ImGui::SameLine();
        if (ImGui::Button("Carregar Fase")) { std::ifstream file(filename); if (file.is_open()) { file.close(); LoadLevel(filename); } else { ClearLevel(); } }
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button("Deletar Tudo da Tela")) { ClearLevel(); } ImGui::PopStyleColor();

        ImGuiIO& io = ImGui::GetIO();
        if (ImGui::IsMouseClicked(0) && !io.WantCaptureMouse) {
            float dxX = (io.MousePos.x / 800.0f) * 2.0f - 1.0f; float dxY = -((io.MousePos.y / 600.0f) * 2.0f - 1.0f);
            if (ferramentaAtual == 0) { AddBlocks(dxX, dxY, 0.1f, 0.1f, editorHits, editorBulletPattern, editorBulletCount); }
            else { AddObstacles(dxX, dxY, obsW, obsH); }
        }
        if (ImGui::IsMouseClicked(1) && !io.WantCaptureMouse) {
            float dxX = (io.MousePos.x / 800.0f) * 2.0f - 1.0f; float dxY = -((io.MousePos.y / 600.0f) * 2.0f - 1.0f);
            for (auto& b : blocks) { if (!b.active) continue; if (dxX > b.x - b.width / 2 && dxX < b.x + b.width / 2 && dxY > b.y && dxY < b.y + b.height) { b.active = false; blocksRemaining--; } }
            for (auto& o : obstacles) { if (!o.active) continue; if (dxX > o.x - o.width / 2 && dxX < o.x + o.width / 2 && dxY > o.y && dxY < o.y + o.height) { o.active = false; } }
        }
    }
    ImGui::End(); ImGui::Render(); ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}