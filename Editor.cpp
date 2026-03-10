#include <fstream>
#include <sstream>
#include "Editor.h"
#include "Render.h"
#include <windows.h>
#include <commdlg.h>
#include "Level.h"
#include "./imgui/imgui.h"
#include "./imgui/imgui_impl_win32.h"
#include "./imgui/imgui_impl_dx11.h"


void UpdateEditor() {

}

void RenderEditor() {
	float clearColor[4] = { 0.0f, 0.0f, 0.2f, 1.0f };
	deviceContext->ClearRenderTargetView(renderTargetView, clearColor);

	if (currentEditorMode == EDITOR_MODE_OBSTACLE) {
		DrawObstaclePreview(0.0f, 0.0f);
	}
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

bool SaveObstacleConfig(const char* filename)
{
	CreateDirectoryA("objects", NULL);
	CreateDirectoryA("objects/OBSTACLE", NULL);

	std::string filepath = "objects/OBSTACLE/";
	filepath += filename;
	filepath += ".json";

	OutputDebugStringA("Salvando em: ");
	OutputDebugStringA(filepath.c_str());
	OutputDebugStringA("\n");

	std::ofstream file(filepath);
	if (!file.is_open()) {
		OutputDebugStringA("ERRO: Nao conseguiu abrir arquivo para escrita!\n");
		return false;
	}

	file << "{\n";
	file << "  \"name\": \"" << editorObstacleConfig.name << "\",\n";
	file << "  \"width\": " << editorObstacleConfig.width << ",\n";
	file << "  \"height\": " << editorObstacleConfig.height << ",\n";
	file << "  \"texture\": \"" << editorObstacleConfig.texturePath << "\",\n";
	file << "  \"color\": {\n";
	file << "    \"r\": " << editorObstacleConfig.colorR << ",\n";
	file << "    \"g\": " << editorObstacleConfig.colorG << ",\n";
	file << "    \"b\": " << editorObstacleConfig.colorB << ",\n";
	file << "    \"a\": " << editorObstacleConfig.colorA << "\n";
	file << "  }\n";
	file << "}\n";

	file.close();
	OutputDebugStringA("Arquivo salvo com sucesso!\n");
	return true;
}

bool LoadObstacleConfig(const char* filename) {
	std::string filepath = "objects/OBSTACLE/";
	filepath += filename;
	filepath += ".json";

	std::ifstream file(filepath);
	if (!file.is_open()) {
		OutputDebugStringA("ERRO: Nao conseguiu abrir arquivo para leitura!\n");
		return false;
	}

	std::string line;
	while (std::getline(file, line)) {
		if (line.find("\"width\":") != std::string::npos) {
			sscanf_s(line.c_str(), " \"width\": %f,", &editorObstacleConfig.width);
		}
		if (line.find("\"height\":") != std::string::npos) {
			sscanf_s(line.c_str(), " \"height\": %f,", &editorObstacleConfig.height);
		}
		if (line.find("\"texture\":") != std::string::npos) {
			size_t start = line.find("\"") + 1;
			size_t end = line.rfind("\"");
			if (start < end) {
				std::string texPath = line.substr(start, end - start);
				strcpy_s(editorObstacleConfig.texturePath, sizeof(editorObstacleConfig.texturePath), texPath.c_str());
			}
		}
		if (line.find("\"r\":") != std::string::npos) {
			sscanf_s(line.c_str(), " \"r\": %f,", &editorObstacleConfig.colorR);
		}
		if (line.find("\"g\":") != std::string::npos) {
			sscanf_s(line.c_str(), " \"g\": %f,", &editorObstacleConfig.colorG);
		}
		if (line.find("\"b\":") != std::string::npos) {
			sscanf_s(line.c_str(), " \"b\": %f,", &editorObstacleConfig.colorB);
		}
		if (line.find("\"a\":") != std::string::npos) {
			sscanf_s(line.c_str(), " \"a\": %f", &editorObstacleConfig.colorA);
		}
	}

	file.close();
	return true;
}

void RenderEditorObstacle()
{
	ImGui::Text("=== EDITOR OBSTACULOS ===");
	ImGui::Separator();

	ImGui::InputText("Nome do Arquivo", editorObstacleNameInput, sizeof(editorObstacleNameInput));
	strcpy_s(editorObstacleConfig.name, sizeof(editorObstacleConfig.name), editorObstacleNameInput);

	ImGui::Separator();

	ImGui::InputText("Largura##obs", editorObstacleWidthInput, sizeof(editorObstacleWidthInput));
	editorObstacleConfig.width = (float)atof(editorObstacleWidthInput);

	ImGui::InputText("Altura##obs", editorObstacleHeightInput, sizeof(editorObstacleHeightInput));
	editorObstacleConfig.height = (float)atof(editorObstacleHeightInput);

	ImGui::Separator();

	ImGui::SliderFloat("Cor R", &editorObstacleConfig.colorR, 0.0f, 1.0f);
	ImGui::SliderFloat("Cor G", &editorObstacleConfig.colorG, 0.0f, 1.0f);
	ImGui::SliderFloat("Cor B", &editorObstacleConfig.colorB, 0.0f, 1.0f);
	ImGui::SliderFloat("Cor A", &editorObstacleConfig.colorA, 0.0f, 1.0f);

	ImGui::Separator();

	ImGui::Text("Textura: %s", editorObstacleConfig.texturePath[0] != '\0' ? editorObstacleConfig.texturePath : "(nenhuma)");

	if (ImGui::Button("Selecionar Textura", ImVec2(200, 0))) {
		if (OpenTextureFileDialog(editorObstacleTexturePathInput, sizeof(editorObstacleTexturePathInput))) {
			strcpy_s(editorObstacleConfig.texturePath, sizeof(editorObstacleConfig.texturePath), editorObstacleTexturePathInput);
			LoadObstacleTexture(editorObstacleConfig.texturePath);
		}
	}

	if (editorObstacleConfig.texturePath[0] != '\0') {
		std::ifstream test(editorObstacleConfig.texturePath);
		bool fileExists = test.good();
		test.close();

		ImGui::TextColored(
			fileExists ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1),
			fileExists ? "Arquivo encontrado" : "Arquivo nao encontrado"
		);
	}

	ImGui::Separator();

	if (ImGui::Button("Salvar Obstaculo", ImVec2(200, 0))) {
		if (SaveObstacleConfig(editorObstacleNameInput)) {
			ImGui::OpenPopup("SaveSuccess");
		}
	}

	if (ImGui::Button("Carregar Obstaculo", ImVec2(200, 0))) {
		if (LoadObstacleConfig(editorObstacleNameInput)) {
			sprintf_s(editorObstacleWidthInput, sizeof(editorObstacleWidthInput), "%.4f", editorObstacleConfig.width);
			sprintf_s(editorObstacleHeightInput, sizeof(editorObstacleHeightInput), "%.4f", editorObstacleConfig.height);
			ImGui::OpenPopup("LoadSuccess");
		}
	}

	if (ImGui::BeginPopupModal("SaveSuccess")) {
		ImGui::Text("Obstaculo salvo com sucesso!");
		if (ImGui::Button("OK")) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	if (ImGui::BeginPopupModal("LoadSuccess")) {
		ImGui::Text("Obstaculo carregado com sucesso!");
		if (ImGui::Button("OK")) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

bool OpenTextureFileDialog(char* outPath, int maxLength)
{
	OPENFILENAMEA ofn = {};
	char szFile[256] = {};

	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = g_hWnd;
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = sizeof(szFile);
	ofn.lpstrFilter = "Bitmap Files (*.bmp)\0*.bmp\0PNG Files (*.png)\0*.png\0All Files (*.*)\0*.*\0";
	ofn.nFilterIndex = 1;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = NULL;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

	if (GetOpenFileNameA(&ofn)) {
		strncpy_s(outPath, maxLength, szFile, maxLength - 1);
		return true;
	}

	return false;
}

bool LoadObstacleTexture(const char* filePath)
{
	if (editorObstacleTexture) {
		editorObstacleTexture->Release();
		editorObstacleTexture = nullptr;
	}
	if (!filePath || filePath[0] == '\0') return true;

	wchar_t wPath[256];
	MultiByteToWideChar(CP_ACP, 0, filePath, -1, wPath, 256);

	HRESULT hr = DirectX::CreateWICTextureFromFile(
		device,
		wPath,
		nullptr,
		&editorObstacleTexture
	);

	if (FAILED(hr)) {
		OutputDebugStringA("ERRO: Falha ao carregar textura WIC!\n");
		return false;
	}

	OutputDebugStringA("Textura carregada com sucesso: ");
	OutputDebugStringA(filePath);
	OutputDebugStringA("\n");
	return true;
}


void RenderEditorPlayer()
{
	ImGui::Text("=== EDITOR JOGADOR (PADDLE) ===");
	ImGui::SliderFloat("Posicao X##paddle", &paddleX, -1.0f, 1.0f);
	ImGui::SliderFloat("Posicao Y##paddle", (float*)&paddleY, -1.0f, 1.0f);
	ImGui::SliderFloat("Largura", &paddleWidth, 0.01f, 0.5f);
	ImGui::SliderFloat("Altura", &paddleHeight, 0.01f, 0.5f);
	ImGui::SliderFloat("Velocidade Dash", &dashSpeed, 0.01f, 0.1f);
	if (ImGui::Button("Reset Paddle")) {
		paddleX = 0.0f;
		paddleWidth = 0.08f;
		paddleHeight = 0.20f;
		dashSpeed = 0.025f;
	}
}

void RenderEditorBall()
{
	ImGui::Text("=== EDITOR BOLA ===");
	ImGui::SliderFloat("Posicao X##ball", &ballX, -1.0f, 1.0f);
	ImGui::SliderFloat("Posicao Y##ball", &ballY, -1.0f, 1.0f);
	ImGui::SliderFloat("Velocidade X", &ballVelX, -0.1f, 0.1f);
	ImGui::SliderFloat("Velocidade Y", &ballVelY, -0.1f, 0.1f);
	ImGui::SliderFloat("Tamanho", &ballSize, 0.01f, 0.1f);
	if (ImGui::Button("Reset Bola")) {
		ballX = 0.0f;
		ballY = -0.5f;
		ballVelX = 0.000001f;
		ballVelY = 0.02f;
		ballSize = 0.03f;
	}
}

void RenderEditorStage()
{
	ImGui::Text("=== EDITOR ESTAGIO ===");
	ImGui::Text("Estagio Atual: %d", stage);
	ImGui::SliderInt("Selecionar Estagio", &stage, 0, 2);
	ImGui::Text("Total de Inimigos: %d", blocksRemaining);
	ImGui::Text("(Drag & Drop: Click esquerdo = adiciona, direito = remove)");
}

void RenderEditorEnemy()
{
	ImGui::Text("=== EDITOR INIMIGOS (BLOCOS) ===");
	ImGui::Text("Total de Blocos/Inimigos: %d", (int)blocks.size());
	static int enemyHits = 1;
	ImGui::SliderInt("Vida do Bloco", &enemyHits, 1, 3);
	static int bulletPatternEnemy = 0;
	ImGui::SliderInt("Padrao de Tiro", &bulletPatternEnemy, 0, 5);
	ImGui::Text("(Drag & Drop: Click esquerdo = adiciona, direito = remove)");
	if (ImGui::Button("Limpar Todos Blocos")) {
		blocks.clear();
	}
}

void RenderEditorBoss()
{
	ImGui::Text("=== EDITOR CHEFES ===");
	ImGui::Text("Chefes: Inimigos com rota customizavel");
	ImGui::Text("(Cada ponto clicado = waypoint da rota)");
	static float bossSpeed = 0.05f;
	ImGui::SliderFloat("Velocidade do Boss", &bossSpeed, 0.01f, 0.2f);
	static int bossBulletPattern = 0;
	ImGui::SliderInt("Padrao de Tiro", &bossBulletPattern, 0, 5);
	static int bossLife = 10;
	ImGui::SliderInt("Vida do Boss", &bossLife, 1, 50);
	ImGui::Text("(Drag & Drop: Click esquerdo = adiciona waypoint, direito = remove)");
}

