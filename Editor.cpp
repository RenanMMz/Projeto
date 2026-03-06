#include "Editor.h"
#include "Level.h"
#include "./imgui/imgui.h"
#include "./imgui/imgui_impl_win32.h"
#include "./imgui/imgui_impl_dx11.h"


void UpdateEditor() {

}

void RenderEditor() {
	float clearColor[4] = { 0.0f, 0.0f, 0.2f, 1.0f };
	deviceContext->ClearRenderTargetView(renderTargetView, clearColor);
}

void RenderDebugUI()
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	ImGui::Begin("Game Engine - TorrouDX");
	ImGui::Text("STATUS");
	ImGui::Text("Blocos: %d | Score: %d", blocksRemaining, score);
	ImGui::Separator();

	bool check = (currentState == STATE_EDITOR);
	if (ImGui::Checkbox("Habilitar Modo de Edicao", &check))
	{
		if (check) {
			currentState = STATE_EDITOR;
		}
		else {
			currentState = STATE_START_MENU;
		}
	}

	ImGui::Separator();

	ImGui::Text("TWEAKS AO VIVO (Cheat)");
	ImGui::SliderFloat("Velocidade Y Bola", &ballVelY, 0.0f, 0.1f);
	static bool godMode = false;
	if (ImGui::Checkbox("God Mode", &godMode)) {
		life = godMode ? 999 : 3;
	}
	ImGui::SameLine();
	if (ImGui::Button("Resetar Bola")) {
		ballX = 0.0f; ballY = 0.0f; ballVelX = 0.0f; ballVelY = 0.02f;
	}
	ImGui::Separator();

	ImGui::End();
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}
