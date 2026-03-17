#include <fstream>
#include <sstream>
#include <cmath>
#include "Editor.h"
#include "Render.h"
#include <windows.h>
#include <commdlg.h>
#include "Level.h"
#include "./imgui/imgui.h"
#include "./imgui/imgui_impl_win32.h"
#include "./imgui/imgui_impl_dx11.h"

// ==========================================
// UTILITÁRIOS INTERNOS
// ==========================================

static bool TextureButton(const char* label, char* pathBuf, int pathBufSize, const char* uid)
{
	ImGui::PushID(uid);
	bool changed = false;
	ImGui::Text("%s: %s", label, pathBuf[0] != '\0' ? pathBuf : "(nenhuma)");
	char btnLabel[64];
	sprintf_s(btnLabel, "Selecionar##%s", uid);
	if (ImGui::Button(btnLabel, ImVec2(180, 0))) {
		char tmp[256] = {};
		if (OpenTextureFileDialog(tmp, sizeof(tmp))) {
			strcpy_s(pathBuf, pathBufSize, tmp);
			changed = true;
		}
	}
	if (pathBuf[0] != '\0') {
		std::ifstream t(pathBuf);
		bool ok = t.good();
		t.close();
		ImGui::SameLine();
		ImGui::TextColored(ok ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1), ok ? "[OK]" : "[nao encontrado]");
	}
	ImGui::PopID();
	return changed;
}

static void DropEditor(DropConfig& drop)
{
	ImGui::Checkbox("Tem Drop?", &drop.hasDrop);
	if (drop.hasDrop) {
		const char* dropTypes[] = { "Vida", "Shield", "Bomba/Especial", "Pontos Bonus" };
		ImGui::Combo("Tipo de Drop", &drop.dropType, dropTypes, IM_ARRAYSIZE(dropTypes));
		ImGui::SliderFloat("Chance de Drop", &drop.dropChance, 0.0f, 1.0f, "%.2f");
	}
}

static void BulletPatternCombo(const char* label, int& pattern)
{
	const char* patterns[] = {
		"0 - Leque em direcao ao jogador",
		"1 - Rajada (velocidades crescentes)",
		"2 - Radial (todas direcoes)",
		"3 - Espiral",
		"4 - Aleatório",
		"5 - Fixo para baixo"
	};
	ImGui::Combo(label, &pattern, patterns, IM_ARRAYSIZE(patterns));
}

// ==========================================
// LOOP DO EDITOR
// ==========================================

void UpdateEditor()
{
	// Drag & drop da bola no modo Stage com o mouse
	if (currentEditorMode == EDITOR_MODE_STAGE) {
		ImGuiIO& io = ImGui::GetIO();
		if (!io.WantCaptureMouse && (GetAsyncKeyState(VK_LBUTTON) & 0x8000)) {
			POINT pt;
			GetCursorPos(&pt);
			ScreenToClient(g_hWnd, &pt);
			// Converte pixel -> NDC  (janela 800x600)
			float ndcX = (pt.x / 400.0f) - 1.0f;
			float ndcY = -(pt.y / 300.0f) + 1.0f;
			ballX = ndcX;
			ballY = ndcY;
			// Atualiza buffer da bola em tempo real
			Vertex bv[] = {
				{ballX - ballSize, ballY + ballSize, 0.0f},
				{ballX - ballSize, ballY - ballSize, 0.0f},
				{ballX + ballSize, ballY - ballSize, 0.0f},
				{ballX - ballSize, ballY + ballSize, 0.0f},
				{ballX + ballSize, ballY - ballSize, 0.0f},
				{ballX + ballSize, ballY + ballSize, 0.0f}
			};
			deviceContext->UpdateSubresource(ballVertexBuffer, 0, nullptr, bv, 0, 0);
		}
	}
}

void RenderEditor()
{
	float clearColor[4] = { 0.0f, 0.0f, 0.2f, 1.0f };
	deviceContext->ClearRenderTargetView(renderTargetView, clearColor);

	if (currentEditorMode == EDITOR_MODE_OBSTACLE) {
		DrawObstaclePreview(0.0f, 0.0f);
	}
	// No modo Stage a bola é arrastada pelo mouse; renderizar via RenderGameplay seria pesado,
	// então apenas renderizamos a bola atual via buffer já atualizado no UpdateEditor.
}

// ==========================================
// UI DE DEBUG (gameplay)
// ==========================================

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
		currentState = check ? STATE_EDITOR : STATE_START_MENU;

	ImGui::Separator();
	ImGui::Text("TWEAKS AO VIVO (Cheat)");
	ImGui::SliderFloat("Velocidade Y Bola", &ballVelY, 0.0f, 0.1f);
	static bool godMode = false;
	if (ImGui::Checkbox("God Mode", &godMode)) life = godMode ? 999 : 3;
	ImGui::SameLine();
	if (ImGui::Button("Resetar Bola")) {
		ballX = 0.0f; ballY = 0.0f; ballVelX = 0.0f; ballVelY = 0.02f;
	}
	ImGui::Separator();
	ImGui::End();

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

// ==========================================
// UI PRINCIPAL DO EDITOR
// ==========================================

void RenderEditorUI()
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	ImGui::Begin("TorrouDX - EDITOR", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

	bool check = (currentState == STATE_EDITOR);
	if (ImGui::Checkbox("Modo Editor Ativo", &check))
		currentState = check ? STATE_EDITOR : STATE_START_MENU;

	ImGui::Separator();
	ImGui::Text("PAINEL:");

	int mode = (int)currentEditorMode;
	ImGui::RadioButton("Jogador", &mode, EDITOR_MODE_PLAYER);   ImGui::SameLine();
	ImGui::RadioButton("Bola", &mode, EDITOR_MODE_BALL);     ImGui::SameLine();
	ImGui::RadioButton("Estagio", &mode, EDITOR_MODE_STAGE);
	ImGui::RadioButton("Obstaculos", &mode, EDITOR_MODE_OBSTACLE); ImGui::SameLine();
	ImGui::RadioButton("Inimigos", &mode, EDITOR_MODE_ENEMY);    ImGui::SameLine();
	ImGui::RadioButton("Boss", &mode, EDITOR_MODE_BOSS);
	ImGui::RadioButton("Bomba/Especial", &mode, EDITOR_MODE_BOMB);     ImGui::SameLine();
	ImGui::RadioButton("Menu", &mode, EDITOR_MODE_MENU);
	currentEditorMode = (EditorMode)mode;

	ImGui::Separator();

	switch (currentEditorMode) {
	case EDITOR_MODE_PLAYER:   RenderEditorPlayer();   break;
	case EDITOR_MODE_BALL:     RenderEditorBall();     break;
	case EDITOR_MODE_STAGE:    RenderEditorStage();    break;
	case EDITOR_MODE_OBSTACLE: RenderEditorObstacle(); break;
	case EDITOR_MODE_ENEMY:    RenderEditorEnemy();    break;
	case EDITOR_MODE_BOSS:     RenderEditorBoss();     break;
	case EDITOR_MODE_BOMB:     RenderEditorBomb();     break;
	case EDITOR_MODE_MENU:     RenderEditorMenu();     break;
	}

	ImGui::End();
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

// ==========================================
// EDITOR DE JOGADOR
// ==========================================

bool SavePlayerConfig(const char* filename)
{
	CreateDirectoryA("objects", NULL);
	CreateDirectoryA("objects/PLAYER", NULL);
	std::string fp = std::string("objects/PLAYER/") + filename + ".json";
	std::ofstream f(fp);
	if (!f.is_open()) return false;
	f << "{\n";
	f << "  \"paddleWidth\": " << paddleWidth << ",\n";
	f << "  \"paddleHeight\": " << paddleHeight << ",\n";
	f << "  \"dashSpeed\": " << dashSpeed << ",\n";
	f << "  \"texturePath\": \"" << editorPlayerConfig.texturePath << "\",\n";
	f << "  \"projectileTexturePath\": \"" << editorPlayerConfig.projectileTexturePath << "\",\n";
	f << "  \"shieldTexturePath\": \"" << editorPlayerConfig.shieldTexturePath << "\",\n";
	f << "  \"dashTexturePath\": \"" << editorPlayerConfig.dashTexturePath << "\"\n";
	f << "}\n";
	f.close();
	return true;
}

bool LoadPlayerConfig(const char* filename)
{
	std::string fp = std::string("objects/PLAYER/") + filename + ".json";
	std::ifstream f(fp);
	if (!f.is_open()) return false;
	std::string line;
	while (std::getline(f, line)) {
		if (line.find("\"paddleWidth\"") != std::string::npos) sscanf_s(line.c_str(), " \"paddleWidth\": %f,", &paddleWidth);
		if (line.find("\"paddleHeight\"") != std::string::npos) sscanf_s(line.c_str(), " \"paddleHeight\": %f,", &paddleHeight);
		if (line.find("\"dashSpeed\"") != std::string::npos) sscanf_s(line.c_str(), " \"dashSpeed\": %f,", &dashSpeed);
		auto readStr = [&](const char* key, char* dest, int sz) {
			if (line.find(key) != std::string::npos) {
				size_t s = line.find(": \"") + 3, e = line.rfind("\"");
				if (s < e) {
					std::string v = line.substr(s, e - s); strcpy_s(dest, sz, v.c_str());
				}
			}
			};
		readStr("\"texturePath\"", editorPlayerConfig.texturePath, 256);
		readStr("\"projectileTexturePath\"", editorPlayerConfig.projectileTexturePath, 256);
		readStr("\"shieldTexturePath\"", editorPlayerConfig.shieldTexturePath, 256);
		readStr("\"dashTexturePath\"", editorPlayerConfig.dashTexturePath, 256);
	}
	f.close();
	return true;
}

void RenderEditorPlayer()
{
	ImGui::Text("=== EDITOR JOGADOR (PADDLE) ===");

	if (ImGui::CollapsingHeader("Fisica & Dimensoes", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::SliderFloat("Posicao X##paddle", &paddleX, -1.0f, 1.0f);
		ImGui::SliderFloat("Largura", &paddleWidth, 0.01f, 0.5f);
		ImGui::SliderFloat("Altura Normal", &paddleHeight, 0.01f, 0.5f);
		ImGui::SliderFloat("Velocidade Dash", &dashSpeed, 0.01f, 0.1f);
		if (ImGui::Button("Reset Paddle")) {
			paddleX = 0.0f; paddleWidth = 0.08f; paddleHeight = 0.20f; dashSpeed = 0.025f;
		}
	}

	if (ImGui::CollapsingHeader("Sprites")) {
		TextureButton("Corpo do Jogador", editorPlayerConfig.texturePath, 256, "spr_player");
		TextureButton("Projétil (tiro)", editorPlayerConfig.projectileTexturePath, 256, "spr_proj");
		TextureButton("Shield", editorPlayerConfig.shieldTexturePath, 256, "spr_shield");
		TextureButton("Dash", editorPlayerConfig.dashTexturePath, 256, "spr_dash");
	}

	ImGui::Separator();
	static char nameInput[64] = "player_default";
	ImGui::InputText("Arquivo##player", nameInput, sizeof(nameInput));
	if (ImGui::Button("Salvar##player")) {
		if (SavePlayerConfig(nameInput)) ImGui::OpenPopup("OK_Player");
	}
	ImGui::SameLine();
	if (ImGui::Button("Carregar##player")) {
		LoadPlayerConfig(nameInput);
	}
	if (ImGui::BeginPopupModal("OK_Player")) {
		ImGui::Text("Jogador salvo!");
		if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}
}

// ==========================================
// EDITOR DE BOLA
// ==========================================

bool SaveBallConfig(const char* filename)
{
	CreateDirectoryA("objects", NULL);
	CreateDirectoryA("objects/BALL", NULL);
	std::string fp = std::string("objects/BALL/") + filename + ".json";
	std::ofstream f(fp);
	if (!f.is_open()) return false;
	f << "{\n";
	f << "  \"ballSize\": " << ballSize << ",\n";
	f << "  \"texturePath\": \"" << editorBallConfig.texturePath << "\"\n";
	f << "}\n";
	f.close();
	return true;
}

bool LoadBallConfig(const char* filename)
{
	std::string fp = std::string("objects/BALL/") + filename + ".json";
	std::ifstream f(fp);
	if (!f.is_open()) return false;
	std::string line;
	while (std::getline(f, line)) {
		if (line.find("\"ballSize\"") != std::string::npos) sscanf_s(line.c_str(), " \"ballSize\": %f,", &ballSize);
		if (line.find("\"texturePath\"") != std::string::npos) {
			size_t s = line.find(": \"") + 3, e = line.rfind("\"");
			if (s < e) {
				std::string v = line.substr(s, e - s); strcpy_s(editorBallConfig.texturePath, 256, v.c_str());
			}
		}
	}
	f.close();
	return true;
}

void RenderEditorBall()
{
	ImGui::Text("=== EDITOR BOLA ===");

	if (ImGui::CollapsingHeader("Fisica", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::SliderFloat("Posicao X##ball", &ballX, -1.0f, 1.0f);
		ImGui::SliderFloat("Posicao Y##ball", &ballY, -1.0f, 1.0f);
		ImGui::SliderFloat("Velocidade X", &ballVelX, -0.1f, 0.1f);
		ImGui::SliderFloat("Velocidade Y", &ballVelY, -0.1f, 0.1f);
		ImGui::SliderFloat("Tamanho", &ballSize, 0.01f, 0.1f);
		if (ImGui::Button("Reset Bola")) {
			ballX = 0.0f; ballY = -0.5f; ballVelX = 0.000001f; ballVelY = 0.02f; ballSize = 0.03f;
		}
	}

	if (ImGui::CollapsingHeader("Sprite")) {
		TextureButton("Textura da Bola", editorBallConfig.texturePath, 256, "spr_ball");
	}

	ImGui::Separator();
	static char nameInput[64] = "ball_default";
	ImGui::InputText("Arquivo##ball", nameInput, sizeof(nameInput));
	if (ImGui::Button("Salvar##ball")) {
		if (SaveBallConfig(nameInput)) ImGui::OpenPopup("OK_Ball");
	}
	ImGui::SameLine();
	if (ImGui::Button("Carregar##ball")) {
		LoadBallConfig(nameInput);
	}
	if (ImGui::BeginPopupModal("OK_Ball")) {
		ImGui::Text("Bola salva!");
		if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}
}

// ==========================================
// EDITOR DE ESTÁGIO
// ==========================================

bool SaveStageConfig(const char* filename)
{
	CreateDirectoryA("objects", NULL);
	CreateDirectoryA("objects/STAGE", NULL);
	std::string fp = std::string("objects/STAGE/") + filename + ".json";
	std::ofstream f(fp);
	if (!f.is_open()) return false;
	f << "{\n";
	f << "  \"stageNumber\": " << editorStageConfig.stageNumber << ",\n";
	f << "  \"ballStartX\": " << editorStageConfig.ballStartX << ",\n";
	f << "  \"ballStartY\": " << editorStageConfig.ballStartY << ",\n";
	f << "  \"ballStartVelX\": " << editorStageConfig.ballStartVelX << ",\n";
	f << "  \"ballStartVelY\": " << editorStageConfig.ballStartVelY << "\n";
	f << "}\n";
	f.close();
	return true;
}

bool LoadStageConfig(const char* filename)
{
	std::string fp = std::string("objects/STAGE/") + filename + ".json";
	std::ifstream f(fp);
	if (!f.is_open()) return false;
	std::string line;
	while (std::getline(f, line)) {
		if (line.find("\"stageNumber\"") != std::string::npos) sscanf_s(line.c_str(), " \"stageNumber\": %d,", &editorStageConfig.stageNumber);
		if (line.find("\"ballStartX\"") != std::string::npos) sscanf_s(line.c_str(), " \"ballStartX\": %f,", &editorStageConfig.ballStartX);
		if (line.find("\"ballStartY\"") != std::string::npos) sscanf_s(line.c_str(), " \"ballStartY\": %f,", &editorStageConfig.ballStartY);
		if (line.find("\"ballStartVelX\"") != std::string::npos) sscanf_s(line.c_str(), " \"ballStartVelX\": %f,", &editorStageConfig.ballStartVelX);
		if (line.find("\"ballStartVelY\"") != std::string::npos) sscanf_s(line.c_str(), " \"ballStartVelY\": %f", &editorStageConfig.ballStartVelY);
	}
	f.close();
	ballX = editorStageConfig.ballStartX;
	ballY = editorStageConfig.ballStartY;
	ballVelX = editorStageConfig.ballStartVelX;
	ballVelY = editorStageConfig.ballStartVelY;
	return true;
}

void RenderEditorStage()
{
	ImGui::Text("=== EDITOR ESTAGIO ===");
	ImGui::Separator();

	ImGui::SliderInt("Numero do Estagio", &editorStageConfig.stageNumber, 0, 9);
	ImGui::Separator();

	ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.3f, 1.0f),
		"[ Clique com o BOTAO ESQUERDO na tela de jogo para");
	ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.3f, 1.0f),
		"  posicionar a bola. O painel ImGui nao captura o clique. ]");
	ImGui::Separator();

	ImGui::Text("Posicao atual da bola:");
	ImGui::Text("  X = %.4f   Y = %.4f", ballX, ballY);

	if (ImGui::Button("Capturar posicao atual como start")) {
		editorStageConfig.ballStartX = ballX;
		editorStageConfig.ballStartY = ballY;
	}
	ImGui::Separator();

	ImGui::Text("Velocidade inicial:");
	ImGui::SliderFloat("Vel X##stage", &editorStageConfig.ballStartVelX, -0.05f, 0.05f);
	ImGui::SliderFloat("Vel Y##stage", &editorStageConfig.ballStartVelY, 0.001f, 0.05f);

	ImGui::Separator();
	static char nameInput[64] = "stage0";
	ImGui::InputText("Arquivo##stage", nameInput, sizeof(nameInput));
	if (ImGui::Button("Salvar##stage")) {
		if (SaveStageConfig(nameInput)) ImGui::OpenPopup("OK_Stage");
	}
	ImGui::SameLine();
	if (ImGui::Button("Carregar##stage")) {
		LoadStageConfig(nameInput);
	}
	if (ImGui::BeginPopupModal("OK_Stage")) {
		ImGui::Text("Estagio salvo!");
		if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}
}

// ==========================================
// EDITOR DE OBSTÁCULO
// ==========================================

bool SaveObstacleConfig(const char* filename)
{
	CreateDirectoryA("objects", NULL);
	CreateDirectoryA("objects/OBSTACLE", NULL);
	std::string fp = std::string("objects/OBSTACLE/") + filename + ".json";
	std::ofstream f(fp);
	if (!f.is_open()) return false;
	f << "{\n";
	f << "  \"name\": \"" << editorObstacleConfig.name << "\",\n";
	f << "  \"width\": " << editorObstacleConfig.width << ",\n";
	f << "  \"height\": " << editorObstacleConfig.height << ",\n";
	f << "  \"texture\": \"" << editorObstacleConfig.texturePath << "\",\n";
	// Cores em linhas separadas — LoadObstacleConfig lê linha por linha com sscanf_s
	f << "  \"color\": {\n";
	f << "    \"r\": " << editorObstacleConfig.colorR << ",\n";
	f << "    \"g\": " << editorObstacleConfig.colorG << ",\n";
	f << "    \"b\": " << editorObstacleConfig.colorB << ",\n";
	f << "    \"a\": " << editorObstacleConfig.colorA << "\n";
	f << "  }\n";
	f << "}\n";
	f.close();
	return true;
}

bool LoadObstacleConfig(const char* filename)
{
	std::string fp = std::string("objects/OBSTACLE/") + filename + ".json";
	std::ifstream f(fp);
	if (!f.is_open()) return false;
	std::string line;
	while (std::getline(f, line)) {
		if (line.find("\"width\"") != std::string::npos) sscanf_s(line.c_str(), " \"width\": %f,", &editorObstacleConfig.width);
		if (line.find("\"height\"") != std::string::npos) sscanf_s(line.c_str(), " \"height\": %f,", &editorObstacleConfig.height);
		if (line.find("\"r\":") != std::string::npos) sscanf_s(line.c_str(), " \"r\": %f,", &editorObstacleConfig.colorR);
		if (line.find("\"g\":") != std::string::npos) sscanf_s(line.c_str(), " \"g\": %f,", &editorObstacleConfig.colorG);
		if (line.find("\"b\":") != std::string::npos) sscanf_s(line.c_str(), " \"b\": %f,", &editorObstacleConfig.colorB);
		if (line.find("\"a\":") != std::string::npos) sscanf_s(line.c_str(), " \"a\": %f", &editorObstacleConfig.colorA);
		if (line.find("\"texture\"") != std::string::npos) {
			size_t s = line.find(": \"") + 3, e = line.rfind("\"");
			if (s < e) {
				std::string v = line.substr(s, e - s); strcpy_s(editorObstacleConfig.texturePath, 256, v.c_str());
			}
		}
	}
	f.close();
	return true;
}

bool LoadObstacleTexture(const char* filePath)
{
	if (editorObstacleTexture) {
		editorObstacleTexture->Release(); editorObstacleTexture = nullptr;
	}
	if (!filePath || filePath[0] == '\0') return true;
	wchar_t wPath[256];
	MultiByteToWideChar(CP_ACP, 0, filePath, -1, wPath, 256);
	HRESULT hr = DirectX::CreateWICTextureFromFile(device, wPath, nullptr, &editorObstacleTexture);
	if (FAILED(hr)) {
		OutputDebugStringA("ERRO: Falha ao carregar textura WIC!\n"); return false;
	}
	return true;
}

static bool LoadSRV(const char* filePath, ID3D11ShaderResourceView** outSRV)
{
	if (*outSRV) {
		(*outSRV)->Release(); *outSRV = nullptr;
	}
	if (!filePath || filePath[0] == '\0') return true;
	wchar_t wPath[256];
	MultiByteToWideChar(CP_ACP, 0, filePath, -1, wPath, 256);
	HRESULT hr = DirectX::CreateWICTextureFromFile(device, wPath, nullptr, outSRV);
	if (FAILED(hr)) {
		OutputDebugStringA("ERRO: Falha ao carregar textura de menu!\n"); return false;
	}
	return true;
}

void RenderEditorObstacle()
{
	ImGui::Text("=== EDITOR OBSTACULOS ===");

	// Mostra o diretório de trabalho atual para referência
	char cwd[256] = {};
	GetCurrentDirectoryA(sizeof(cwd), cwd);
	ImGui::TextDisabled("Salvando em: %s\\objects\\OBSTACLE\\", cwd);
	ImGui::Separator();

	ImGui::InputText("Nome##obs", editorObstacleNameInput, sizeof(editorObstacleNameInput));
	strcpy_s(editorObstacleConfig.name, sizeof(editorObstacleConfig.name), editorObstacleNameInput);

	if (ImGui::CollapsingHeader("Dimensoes & Cor", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::InputText("Largura##obs", editorObstacleWidthInput, sizeof(editorObstacleWidthInput));
		editorObstacleConfig.width = (float)atof(editorObstacleWidthInput);
		ImGui::InputText("Altura##obs", editorObstacleHeightInput, sizeof(editorObstacleHeightInput));
		editorObstacleConfig.height = (float)atof(editorObstacleHeightInput);
		ImGui::SliderFloat("R##obs", &editorObstacleConfig.colorR, 0.0f, 1.0f);
		ImGui::SliderFloat("G##obs", &editorObstacleConfig.colorG, 0.0f, 1.0f);
		ImGui::SliderFloat("B##obs", &editorObstacleConfig.colorB, 0.0f, 1.0f);
		ImGui::SliderFloat("A##obs", &editorObstacleConfig.colorA, 0.0f, 1.0f);
	}

	if (ImGui::CollapsingHeader("Sprite")) {
		if (TextureButton("Textura", editorObstacleConfig.texturePath, 256, "spr_obs"))
			LoadObstacleTexture(editorObstacleConfig.texturePath);
	}

	ImGui::Separator();
	if (ImGui::Button("Salvar Obstaculo")) {
		if (SaveObstacleConfig(editorObstacleNameInput)) ImGui::OpenPopup("OK_Obs");
	}
	ImGui::SameLine();
	if (ImGui::Button("Carregar Obstaculo")) {
		if (LoadObstacleConfig(editorObstacleNameInput)) {
			sprintf_s(editorObstacleWidthInput, sizeof(editorObstacleWidthInput), "%.4f", editorObstacleConfig.width);
			sprintf_s(editorObstacleHeightInput, sizeof(editorObstacleHeightInput), "%.4f", editorObstacleConfig.height);
			ImGui::OpenPopup("OK_Obs");
		}
	}
	if (ImGui::BeginPopupModal("OK_Obs")) {
		ImGui::Text("Operacao concluida!");
		if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}
}

// ==========================================
// EDITOR DE INIMIGOS / BLOCOS
// ==========================================

bool SaveBlockConfig(const char* filename)
{
	CreateDirectoryA("objects", NULL);
	CreateDirectoryA("objects/BLOCK", NULL);
	std::string fp = std::string("objects/BLOCK/") + filename + ".json";
	std::ofstream f(fp);
	if (!f.is_open()) return false;
	f << "{\n";
	f << "  \"name\": \"" << editorBlockConfig.name << "\",\n";
	f << "  \"texturePath\": \"" << editorBlockConfig.texturePath << "\",\n";
	f << "  \"width\": " << editorBlockConfig.width << ",\n";
	f << "  \"height\": " << editorBlockConfig.height << ",\n";
	f << "  \"colorR\": " << editorBlockConfig.colorR << ",\n";
	f << "  \"colorG\": " << editorBlockConfig.colorG << ",\n";
	f << "  \"colorB\": " << editorBlockConfig.colorB << ",\n";
	f << "  \"colorA\": " << editorBlockConfig.colorA << ",\n";
	f << "  \"maxHits\": " << editorBlockConfig.maxHits << ",\n";
	f << "  \"bulletPattern\": " << editorBlockConfig.bulletPattern << ",\n";
	f << "  \"bulletCount\": " << editorBlockConfig.bulletCount << ",\n";
	f << "  \"hasDrop\": " << (editorBlockConfig.drop.hasDrop ? "true" : "false") << ",\n";
	f << "  \"dropType\": " << editorBlockConfig.drop.dropType << ",\n";
	f << "  \"dropChance\": " << editorBlockConfig.drop.dropChance << "\n";
	f << "}\n";
	f.close();
	return true;
}

bool LoadBlockConfig(const char* filename)
{
	std::string fp = std::string("objects/BLOCK/") + filename + ".json";
	std::ifstream f(fp);
	if (!f.is_open()) return false;
	std::string line;
	while (std::getline(f, line)) {
		if (line.find("\"width\"") != std::string::npos) sscanf_s(line.c_str(), " \"width\": %f,", &editorBlockConfig.width);
		if (line.find("\"height\"") != std::string::npos) sscanf_s(line.c_str(), " \"height\": %f,", &editorBlockConfig.height);
		if (line.find("\"colorR\"") != std::string::npos) sscanf_s(line.c_str(), " \"colorR\": %f,", &editorBlockConfig.colorR);
		if (line.find("\"colorG\"") != std::string::npos) sscanf_s(line.c_str(), " \"colorG\": %f,", &editorBlockConfig.colorG);
		if (line.find("\"colorB\"") != std::string::npos) sscanf_s(line.c_str(), " \"colorB\": %f,", &editorBlockConfig.colorB);
		if (line.find("\"colorA\"") != std::string::npos) sscanf_s(line.c_str(), " \"colorA\": %f,", &editorBlockConfig.colorA);
		if (line.find("\"maxHits\"") != std::string::npos) sscanf_s(line.c_str(), " \"maxHits\": %d,", &editorBlockConfig.maxHits);
		if (line.find("\"bulletPattern\"") != std::string::npos) sscanf_s(line.c_str(), " \"bulletPattern\": %d,", &editorBlockConfig.bulletPattern);
		if (line.find("\"bulletCount\"") != std::string::npos) sscanf_s(line.c_str(), " \"bulletCount\": %d,", &editorBlockConfig.bulletCount);
		if (line.find("\"dropType\"") != std::string::npos) sscanf_s(line.c_str(), " \"dropType\": %d,", &editorBlockConfig.drop.dropType);
		if (line.find("\"dropChance\"") != std::string::npos) sscanf_s(line.c_str(), " \"dropChance\": %f", &editorBlockConfig.drop.dropChance);
		if (line.find("\"hasDrop\": true") != std::string::npos) editorBlockConfig.drop.hasDrop = true;
		if (line.find("\"hasDrop\": false") != std::string::npos) editorBlockConfig.drop.hasDrop = false;
		if (line.find("\"texturePath\"") != std::string::npos) {
			size_t s = line.find(": \"") + 3, e = line.rfind("\"");
			if (s < e) {
				std::string v = line.substr(s, e - s); strcpy_s(editorBlockConfig.texturePath, 256, v.c_str());
			}
		}
	}
	f.close();
	return true;
}

void RenderEditorEnemy()
{
	ImGui::Text("=== EDITOR INIMIGOS / BLOCOS ===");
	ImGui::Text("Blocos na cena: %d", (int)blocks.size());
	ImGui::Separator();

	if (ImGui::CollapsingHeader("Template do Bloco", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::InputText("Nome##blk", editorBlockConfig.name, sizeof(editorBlockConfig.name));
		ImGui::SliderFloat("Largura##blk", &editorBlockConfig.width, 0.01f, 0.5f);
		ImGui::SliderFloat("Altura##blk", &editorBlockConfig.height, 0.01f, 0.3f);
		ImGui::SliderInt("Vida (hits)", &editorBlockConfig.maxHits, 1, 5);
		ImGui::ColorEdit4("Cor##blk",
			&editorBlockConfig.colorR, ImGuiColorEditFlags_NoInputs);
	}

	if (ImGui::CollapsingHeader("Padrao de Tiro")) {
		BulletPatternCombo("Padrao##blk", editorBlockConfig.bulletPattern);
		ImGui::SliderInt("Qtd Projeteis##blk", &editorBlockConfig.bulletCount, 1, 16);
	}

	if (ImGui::CollapsingHeader("Sprite")) {
		TextureButton("Textura do Bloco", editorBlockConfig.texturePath, 256, "spr_blk");
	}

	if (ImGui::CollapsingHeader("Drop")) {
		DropEditor(editorBlockConfig.drop);
	}

	ImGui::Separator();
	ImGui::Text("Blocos existentes:");
	for (int i = 0; i < (int)blocks.size(); i++) {
		ImGui::PushID(i);
		ImGui::Text("[%d] x=%.2f y=%.2f hits=%d", i, blocks[i].x, blocks[i].y, blocks[i].hits);
		ImGui::SameLine();
		if (ImGui::SmallButton("X")) blocks[i].active = false;
		ImGui::PopID();
	}
	if (ImGui::Button("Limpar Todos")) blocks.clear();

	ImGui::Separator();
	static char nameInput[64] = "block_default";
	ImGui::InputText("Arquivo##blk", nameInput, sizeof(nameInput));
	if (ImGui::Button("Salvar##blk")) {
		if (SaveBlockConfig(nameInput)) ImGui::OpenPopup("OK_Blk");
	}
	ImGui::SameLine();
	if (ImGui::Button("Carregar##blk")) {
		LoadBlockConfig(nameInput);
	}
	if (ImGui::BeginPopupModal("OK_Blk")) {
		ImGui::Text("Bloco salvo!");
		if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}
}

// ==========================================
// EDITOR DE BOSS
// ==========================================

bool SaveBossConfig(const char* filename)
{
	CreateDirectoryA("objects", NULL);
	CreateDirectoryA("objects/BOSS", NULL);
	std::string fp = std::string("objects/BOSS/") + filename + ".json";
	std::ofstream f(fp);
	if (!f.is_open()) return false;
	f << "{\n";
	f << "  \"name\": \"" << editorBossConfig.name << "\",\n";
	f << "  \"texturePath\": \"" << editorBossConfig.texturePath << "\",\n";
	f << "  \"maxHP\": " << editorBossConfig.maxHP << ",\n";
	f << "  \"width\": " << editorBossConfig.width << ",\n";
	f << "  \"height\": " << editorBossConfig.height << ",\n";
	f << "  \"phaseCount\": " << editorBossConfig.phaseCount << ",\n";
	f << "  \"phases\": [\n";
	for (int i = 0; i < editorBossConfig.phaseCount; i++) {
		BossPhase& p = editorBossConfig.phases[i];
		f << "    { \"hpThreshold\": " << p.hpThreshold
			<< ", \"bulletPattern\": " << p.bulletPattern
			<< ", \"bulletCount\": " << p.bulletCount
			<< ", \"bulletSpeed\": " << p.bulletSpeed
			<< ", \"movementPattern\": " << p.movementPattern
			<< ", \"movementSpeed\": " << p.movementSpeed
			<< ", \"amplitude\": " << p.amplitude
			<< ", \"frequency\": " << p.frequency << " }";
		if (i < editorBossConfig.phaseCount - 1) f << ",";
		f << "\n";
	}
	f << "  ],\n";
	f << "  \"waypointCount\": " << editorBossConfig.waypointCount << ",\n";
	f << "  \"waypoints\": [\n";
	for (int i = 0; i < editorBossConfig.waypointCount; i++) {
		f << "    { \"x\": " << editorBossConfig.waypointX[i]
			<< ", \"y\": " << editorBossConfig.waypointY[i] << " }";
		if (i < editorBossConfig.waypointCount - 1) f << ",";
		f << "\n";
	}
	f << "  ]\n}\n";
	f.close();
	return true;
}

bool LoadBossConfig(const char* filename)
{
	std::string fp = std::string("objects/BOSS/") + filename + ".json";
	std::ifstream f(fp);
	if (!f.is_open()) return false;
	// Leitura simplificada linha por linha
	std::string line;
	int phaseIdx = -1;
	int wpIdx = -1;
	editorBossConfig.phaseCount = 0;
	editorBossConfig.waypointCount = 0;
	while (std::getline(f, line)) {
		if (line.find("\"maxHP\"") != std::string::npos) sscanf_s(line.c_str(), " \"maxHP\": %d,", &editorBossConfig.maxHP);
		if (line.find("\"width\"") != std::string::npos) sscanf_s(line.c_str(), " \"width\": %f,", &editorBossConfig.width);
		if (line.find("\"height\"") != std::string::npos) sscanf_s(line.c_str(), " \"height\": %f,", &editorBossConfig.height);
		if (line.find("\"phaseCount\"") != std::string::npos) sscanf_s(line.c_str(), " \"phaseCount\": %d,", &editorBossConfig.phaseCount);
		if (line.find("\"hpThreshold\"") != std::string::npos) {
			phaseIdx++;
			if (phaseIdx < 4) {
				BossPhase& p = editorBossConfig.phases[phaseIdx];
				sscanf_s(line.c_str(),
					" { \"hpThreshold\": %d , \"bulletPattern\": %d , \"bulletCount\": %d , \"bulletSpeed\": %f , \"movementPattern\": %d , \"movementSpeed\": %f , \"amplitude\": %f , \"frequency\": %f }",
					&p.hpThreshold, &p.bulletPattern, &p.bulletCount, &p.bulletSpeed,
					&p.movementPattern, &p.movementSpeed, &p.amplitude, &p.frequency);
			}
		}
		if (line.find("\"x\":") != std::string::npos && line.find("\"y\":") != std::string::npos && line.find("hpThreshold") == std::string::npos) {
			wpIdx++;
			if (wpIdx < 16) {
				sscanf_s(line.c_str(), " { \"x\": %f , \"y\": %f }",
					&editorBossConfig.waypointX[wpIdx],
					&editorBossConfig.waypointY[wpIdx]);
				editorBossConfig.waypointCount = wpIdx + 1;
			}
		}
		if (line.find("\"texturePath\"") != std::string::npos) {
			size_t s = line.find(": \"") + 3, e = line.rfind("\"");
			if (s < e) {
				std::string v = line.substr(s, e - s); strcpy_s(editorBossConfig.texturePath, 256, v.c_str());
			}
		}
		if (line.find("\"name\"") != std::string::npos) {
			size_t s = line.find(": \"") + 3, e = line.rfind("\"");
			if (s < e) {
				std::string v = line.substr(s, e - s); strcpy_s(editorBossConfig.name, 64, v.c_str());
			}
		}
	}
	f.close();
	return true;
}

void RenderEditorBoss()
{
	ImGui::Text("=== EDITOR DE BOSS ===");
	ImGui::Separator();

	// --- Identidade ---
	if (ImGui::CollapsingHeader("Identidade", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::InputText("Nome##boss", editorBossConfig.name, sizeof(editorBossConfig.name));
		ImGui::SliderInt("HP Maximo", &editorBossConfig.maxHP, 1, 200);
		ImGui::SliderFloat("Largura##boss", &editorBossConfig.width, 0.05f, 0.8f);
		ImGui::SliderFloat("Altura##boss", &editorBossConfig.height, 0.05f, 0.8f);
		TextureButton("Sprite do Boss", editorBossConfig.texturePath, 256, "spr_boss");
	}

	// --- Fases ---
	if (ImGui::CollapsingHeader("Fases (ate 4)")) {
		ImGui::SliderInt("Numero de Fases", &editorBossConfig.phaseCount, 1, 4);

		const char* movPatterns[] = { "Waypoints", "Senoidal", "Circular", "Dash ao jogador" };

		for (int i = 0; i < editorBossConfig.phaseCount; i++) {
			ImGui::PushID(i);
			char phLabel[32]; sprintf_s(phLabel, "Fase %d", i + 1);
			if (ImGui::TreeNode(phLabel)) {
				BossPhase& ph = editorBossConfig.phases[i];
				ImGui::SliderInt("HP% de ativacao", &ph.hpThreshold, 1, 100);
				ImGui::Text("  (ativa quando HP cai abaixo de %d%%)", ph.hpThreshold);

				ImGui::Separator();
				ImGui::Text("Movimento:");
				ImGui::Combo("Padrao##mov", &ph.movementPattern, movPatterns, IM_ARRAYSIZE(movPatterns));
				ImGui::SliderFloat("Velocidade##mov", &ph.movementSpeed, 0.001f, 0.05f);
				if (ph.movementPattern == 1) { // senoidal
					ImGui::SliderFloat("Amplitude", &ph.amplitude, 0.01f, 1.0f);
					ImGui::SliderFloat("Frequencia", &ph.frequency, 0.01f, 0.2f);
				}
				if (ph.movementPattern == 2) { // circular
					ImGui::SliderFloat("Raio##circ", &ph.amplitude, 0.05f, 1.0f);
				}

				ImGui::Separator();
				ImGui::Text("Tiro:");
				BulletPatternCombo("Padrao##blt", ph.bulletPattern);
				ImGui::SliderInt("Qtd Projeteis##blt", &ph.bulletCount, 1, 24);
				ImGui::SliderFloat("Velocidade Projetil", &ph.bulletSpeed, 0.001f, 0.03f);

				ImGui::TreePop();
			}
			ImGui::PopID();
		}
	}

	// --- Waypoints ---
	if (ImGui::CollapsingHeader("Waypoints")) {
		ImGui::Text("Waypoints definidos: %d / 16", editorBossConfig.waypointCount);
		ImGui::TextColored(ImVec4(1, 1, 0.3f, 1), "Dica: use 'Capturar posicao atual' para");
		ImGui::TextColored(ImVec4(1, 1, 0.3f, 1), "adicionar o ponto onde a bola/boss esta.");

		if (editorBossConfig.waypointCount < 16) {
			if (ImGui::Button("Adicionar posicao atual da bola")) {
				int idx = editorBossConfig.waypointCount;
				editorBossConfig.waypointX[idx] = ballX;
				editorBossConfig.waypointY[idx] = ballY;
				editorBossConfig.waypointCount++;
			}
		}

		for (int i = 0; i < editorBossConfig.waypointCount; i++) {
			ImGui::PushID(i);
			ImGui::Text("[%d] x=%.3f  y=%.3f", i, editorBossConfig.waypointX[i], editorBossConfig.waypointY[i]);
			ImGui::SameLine();
			if (ImGui::SmallButton("X##wp")) {
				// Remove: desloca array
				for (int j = i; j < editorBossConfig.waypointCount - 1; j++) {
					editorBossConfig.waypointX[j] = editorBossConfig.waypointX[j + 1];
					editorBossConfig.waypointY[j] = editorBossConfig.waypointY[j + 1];
				}
				editorBossConfig.waypointCount--;
			}
			ImGui::PopID();
		}
		if (ImGui::Button("Limpar Waypoints")) editorBossConfig.waypointCount = 0;
	}

	// --- Save / Load ---
	ImGui::Separator();
	static char nameInput[64] = "boss_default";
	ImGui::InputText("Arquivo##boss", nameInput, sizeof(nameInput));
	if (ImGui::Button("Salvar Boss")) {
		if (SaveBossConfig(nameInput)) ImGui::OpenPopup("OK_Boss");
	}
	ImGui::SameLine();
	if (ImGui::Button("Carregar Boss")) {
		LoadBossConfig(nameInput);
	}
	if (ImGui::BeginPopupModal("OK_Boss")) {
		ImGui::Text("Boss salvo!");
		if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}
}

// ==========================================
// EDITOR DE BOMBA / ESPECIAL
// ==========================================

bool SaveBombConfig(const char* filename)
{
	CreateDirectoryA("objects", NULL);
	CreateDirectoryA("objects/BOMB", NULL);
	std::string fp = std::string("objects/BOMB/") + filename + ".json";
	std::ofstream f(fp);
	if (!f.is_open()) return false;
	f << "{\n";
	f << "  \"name\": \"" << editorBombConfig.name << "\",\n";
	f << "  \"texturePath\": \"" << editorBombConfig.texturePath << "\",\n";
	f << "  \"type\": " << editorBombConfig.type << ",\n";
	f << "  \"radius\": " << editorBombConfig.radius << ",\n";
	f << "  \"damage\": " << editorBombConfig.damage << ",\n";
	f << "  \"duration\": " << editorBombConfig.duration << ",\n";
	f << "  \"bulletPattern\": " << editorBombConfig.bulletPattern << ",\n";
	f << "  \"bulletCount\": " << editorBombConfig.bulletCount << ",\n";
	f << "  \"bulletSpeed\": " << editorBombConfig.bulletSpeed << "\n";
	f << "}\n";
	f.close();
	return true;
}

bool LoadBombConfig(const char* filename)
{
	std::string fp = std::string("objects/BOMB/") + filename + ".json";
	std::ifstream f(fp);
	if (!f.is_open()) return false;
	std::string line;
	while (std::getline(f, line)) {
		if (line.find("\"type\"") != std::string::npos) sscanf_s(line.c_str(), " \"type\": %d,", &editorBombConfig.type);
		if (line.find("\"radius\"") != std::string::npos) sscanf_s(line.c_str(), " \"radius\": %f,", &editorBombConfig.radius);
		if (line.find("\"damage\"") != std::string::npos) sscanf_s(line.c_str(), " \"damage\": %f,", &editorBombConfig.damage);
		if (line.find("\"duration\"") != std::string::npos) sscanf_s(line.c_str(), " \"duration\": %d,", &editorBombConfig.duration);
		if (line.find("\"bulletPattern\"") != std::string::npos) sscanf_s(line.c_str(), " \"bulletPattern\": %d,", &editorBombConfig.bulletPattern);
		if (line.find("\"bulletCount\"") != std::string::npos) sscanf_s(line.c_str(), " \"bulletCount\": %d,", &editorBombConfig.bulletCount);
		if (line.find("\"bulletSpeed\"") != std::string::npos) sscanf_s(line.c_str(), " \"bulletSpeed\": %f", &editorBombConfig.bulletSpeed);
		if (line.find("\"texturePath\"") != std::string::npos) {
			size_t s = line.find(": \"") + 3, e = line.rfind("\"");
			if (s < e) {
				std::string v = line.substr(s, e - s); strcpy_s(editorBombConfig.texturePath, 256, v.c_str());
			}
		}
		if (line.find("\"name\"") != std::string::npos) {
			size_t s = line.find(": \"") + 3, e = line.rfind("\"");
			if (s < e) {
				std::string v = line.substr(s, e - s); strcpy_s(editorBombConfig.name, 64, v.c_str());
			}
		}
	}
	f.close();
	return true;
}

void RenderEditorBomb()
{
	ImGui::Text("=== EDITOR BOMBA / ESPECIAL ===");
	ImGui::Separator();

	ImGui::InputText("Nome##bomb", editorBombConfig.name, sizeof(editorBombConfig.name));

	if (ImGui::CollapsingHeader("Tipo & Propriedades", ImGuiTreeNodeFlags_DefaultOpen)) {
		const char* types[] = { "Habilidade do jogador", "Explosivo lancavel", "Ambos" };
		ImGui::Combo("Tipo##bomb", &editorBombConfig.type, types, IM_ARRAYSIZE(types));

		ImGui::SliderFloat("Raio de Efeito", &editorBombConfig.radius, 0.05f, 1.5f);
		ImGui::SliderFloat("Dano", &editorBombConfig.damage, 0.1f, 10.0f);
		ImGui::SliderInt("Duracao (frames)", &editorBombConfig.duration, 1, 600);
	}

	if (ImGui::CollapsingHeader("Explosao - Projeteis")) {
		BulletPatternCombo("Padrao##bomb", editorBombConfig.bulletPattern);
		ImGui::SliderInt("Qtd Projeteis##bomb", &editorBombConfig.bulletCount, 0, 32);
		ImGui::SliderFloat("Velocidade Projetil##b", &editorBombConfig.bulletSpeed, 0.001f, 0.03f);
		if (editorBombConfig.bulletCount == 0)
			ImGui::TextDisabled("(sem projeteis na explosao)");
	}

	if (ImGui::CollapsingHeader("Sprite")) {
		TextureButton("Textura da Bomba", editorBombConfig.texturePath, 256, "spr_bomb");
	}

	ImGui::Separator();
	static char nameInput[64] = "bomb_default";
	ImGui::InputText("Arquivo##bomb", nameInput, sizeof(nameInput));
	if (ImGui::Button("Salvar Bomba")) {
		if (SaveBombConfig(nameInput)) ImGui::OpenPopup("OK_Bomb");
	}
	ImGui::SameLine();
	if (ImGui::Button("Carregar##bomb")) {
		LoadBombConfig(nameInput);
	}
	if (ImGui::BeginPopupModal("OK_Bomb")) {
		ImGui::Text("Bomba salva!");
		if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}
}

// ==========================================
// EDITOR DE MENU
// ==========================================

bool SaveMenuConfig(const char* filename)
{
	CreateDirectoryA("objects", NULL);
	CreateDirectoryA("objects/MENU", NULL);
	std::string fp = std::string("objects/MENU/") + filename + ".json";
	std::ofstream f(fp);
	if (!f.is_open()) return false;
	f << "{\n";
	f << "  \"bgR\": " << editorMenuConfig.bgColorR << ", \"bgG\": " << editorMenuConfig.bgColorG << ", \"bgB\": " << editorMenuConfig.bgColorB << ", \"bgA\": " << editorMenuConfig.bgColorA << ",\n";
	f << "  \"btnR\": " << editorMenuConfig.buttonColorR << ", \"btnG\": " << editorMenuConfig.buttonColorG << ", \"btnB\": " << editorMenuConfig.buttonColorB << ", \"btnA\": " << editorMenuConfig.buttonColorA << ",\n";
	f << "  \"selR\": " << editorMenuConfig.selectedColorR << ", \"selG\": " << editorMenuConfig.selectedColorG << ", \"selB\": " << editorMenuConfig.selectedColorB << ", \"selA\": " << editorMenuConfig.selectedColorA << ",\n";
	f << "  \"bgTexture\": \"" << editorMenuConfig.bgTexturePath << "\",\n";
	f << "  \"titleTexture\": \"" << editorMenuConfig.titleTexturePath << "\",\n";
	f << "  \"selectorTexture\": \"" << editorMenuConfig.selectorTexturePath << "\",\n";
	f << "  \"logoX\": " << editorMenuConfig.logoX << ",\n";
	f << "  \"logoY\": " << editorMenuConfig.logoY << ",\n";
	f << "  \"logoWidth\": " << editorMenuConfig.logoWidth << ",\n";
	f << "  \"logoHeight\": " << editorMenuConfig.logoHeight << "\n";
	f << "}\n";
	f.close();
	return true;
}

bool LoadMenuConfig(const char* filename)
{
	std::string fp = std::string("objects/MENU/") + filename + ".json";
	std::ifstream f(fp);
	if (!f.is_open()) return false;
	std::string line;
	while (std::getline(f, line)) {
		if (line.find("\"bgR\"") != std::string::npos)
			sscanf_s(line.c_str(), " \"bgR\": %f , \"bgG\": %f , \"bgB\": %f , \"bgA\": %f ,",
				&editorMenuConfig.bgColorR, &editorMenuConfig.bgColorG,
				&editorMenuConfig.bgColorB, &editorMenuConfig.bgColorA);
		if (line.find("\"btnR\"") != std::string::npos)
			sscanf_s(line.c_str(), " \"btnR\": %f , \"btnG\": %f , \"btnB\": %f , \"btnA\": %f ,",
				&editorMenuConfig.buttonColorR, &editorMenuConfig.buttonColorG,
				&editorMenuConfig.buttonColorB, &editorMenuConfig.buttonColorA);
		if (line.find("\"selR\"") != std::string::npos)
			sscanf_s(line.c_str(), " \"selR\": %f , \"selG\": %f , \"selB\": %f , \"selA\": %f ,",
				&editorMenuConfig.selectedColorR, &editorMenuConfig.selectedColorG,
				&editorMenuConfig.selectedColorB, &editorMenuConfig.selectedColorA);
		if (line.find("\"logoX\"") != std::string::npos) sscanf_s(line.c_str(), " \"logoX\": %f ,", &editorMenuConfig.logoX);
		if (line.find("\"logoY\"") != std::string::npos) sscanf_s(line.c_str(), " \"logoY\": %f ,", &editorMenuConfig.logoY);
		if (line.find("\"logoWidth\"") != std::string::npos) sscanf_s(line.c_str(), " \"logoWidth\": %f ,", &editorMenuConfig.logoWidth);
		if (line.find("\"logoHeight\"") != std::string::npos) sscanf_s(line.c_str(), " \"logoHeight\": %f", &editorMenuConfig.logoHeight);

		auto readStr = [&](const char* key, char* dest, int sz) {
			if (line.find(key) != std::string::npos) {
				size_t s = line.find(": \"") + 3, e = line.rfind("\"");
				if (s < e) {
					std::string v = line.substr(s, e - s); strcpy_s(dest, sz, v.c_str());
				}
			}
			};
		readStr("\"bgTexture\"", editorMenuConfig.bgTexturePath, 256);
		readStr("\"titleTexture\"", editorMenuConfig.titleTexturePath, 256);
		readStr("\"selectorTexture\"", editorMenuConfig.selectorTexturePath, 256);
	}
	f.close();
	// Recarrega texturas na GPU
	LoadSRV(editorMenuConfig.bgTexturePath, &menuBgTexture);
	LoadSRV(editorMenuConfig.titleTexturePath, &menuTitleTexture);
	LoadSRV(editorMenuConfig.selectorTexturePath, &menuSelectorTexture);
	return true;
}

void RenderEditorMenu()
{
	ImGui::Text("=== EDITOR DE MENU ===");
	ImGui::Separator();

	if (ImGui::CollapsingHeader("Cores", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::ColorEdit4("Fundo##menu",
			&editorMenuConfig.bgColorR,
			ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_PickerHueBar);
		ImGui::ColorEdit4("Botao Normal",
			&editorMenuConfig.buttonColorR,
			ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_PickerHueBar);
		ImGui::ColorEdit4("Botao Selecionado",
			&editorMenuConfig.selectedColorR,
			ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_PickerHueBar);
	}

	if (ImGui::CollapsingHeader("Preview das Cores")) {
		ImVec2 sz(ImGui::GetContentRegionAvail().x, 30);
		ImGui::TextDisabled("Fundo:");
		ImGui::ColorButton("##prevBG",
			ImVec4(editorMenuConfig.bgColorR, editorMenuConfig.bgColorG,
				editorMenuConfig.bgColorB, editorMenuConfig.bgColorA),
			ImGuiColorEditFlags_NoTooltip, sz);
		ImGui::TextDisabled("Botao normal:");
		ImGui::ColorButton("##prevBtn",
			ImVec4(editorMenuConfig.buttonColorR, editorMenuConfig.buttonColorG,
				editorMenuConfig.buttonColorB, editorMenuConfig.buttonColorA),
			ImGuiColorEditFlags_NoTooltip, sz);
		ImGui::TextDisabled("Botao selecionado:");
		ImGui::ColorButton("##prevSel",
			ImVec4(editorMenuConfig.selectedColorR, editorMenuConfig.selectedColorG,
				editorMenuConfig.selectedColorB, editorMenuConfig.selectedColorA),
			ImGuiColorEditFlags_NoTooltip, sz);
	}

	if (ImGui::CollapsingHeader("Logo / Titulo", ImGuiTreeNodeFlags_DefaultOpen)) {
		if (TextureButton("Imagem da Logo", editorMenuConfig.titleTexturePath, 256, "spr_title"))
			LoadSRV(editorMenuConfig.titleTexturePath, &menuTitleTexture);

		ImGui::Separator();
		ImGui::Text("Posicao e tamanho (NDC: -1 a 1):");
		ImGui::SliderFloat("Centro X##logo", &editorMenuConfig.logoX, -1.0f, 1.0f);
		ImGui::SliderFloat("Centro Y##logo", &editorMenuConfig.logoY, -1.0f, 1.0f);
		ImGui::SliderFloat("Largura##logo", &editorMenuConfig.logoWidth, 0.05f, 2.0f);
		ImGui::SliderFloat("Altura##logo", &editorMenuConfig.logoHeight, 0.02f, 1.5f);
		if (ImGui::Button("Reset Logo")) {
			editorMenuConfig.logoX = 0.0f; editorMenuConfig.logoY = 0.75f;
			editorMenuConfig.logoWidth = 0.8f; editorMenuConfig.logoHeight = 0.15f;
		}
		ImGui::TextDisabled("Dica: ative o menu (STATE_START_MENU) para");
		ImGui::TextDisabled("ver o resultado em tempo real.");
	}

	if (ImGui::CollapsingHeader("Icone de Selecao (seta)")) {
		if (TextureButton("Textura do Seletor", editorMenuConfig.selectorTexturePath, 256, "spr_sel"))
			LoadSRV(editorMenuConfig.selectorTexturePath, &menuSelectorTexture);
		ImGui::TextDisabled("Aparece dentro do botao ativo, lado esquerdo.");
		ImGui::TextDisabled("Tamanho: 70%% da altura do botao (automatico).");
	}

	if (ImGui::CollapsingHeader("Fundo do Menu")) {
		if (TextureButton("Textura de Fundo", editorMenuConfig.bgTexturePath, 256, "spr_menubg"))
			LoadSRV(editorMenuConfig.bgTexturePath, &menuBgTexture);
		ImGui::TextDisabled("Cobre a cor de fundo quando definida.");
	}

	ImGui::Separator();
	static char nameInput[64] = "menu_default";
	ImGui::InputText("Arquivo##menu", nameInput, sizeof(nameInput));
	if (ImGui::Button("Salvar Menu")) {
		if (SaveMenuConfig(nameInput)) ImGui::OpenPopup("OK_Menu");
	}
	ImGui::SameLine();
	if (ImGui::Button("Carregar##menu")) {
		LoadMenuConfig(nameInput);
	}
	if (ImGui::BeginPopupModal("OK_Menu")) {
		ImGui::Text("Config de menu salva!");
		if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}
}

// ==========================================
// UTILITÁRIOS
// ==========================================

bool OpenTextureFileDialog(char* outPath, int maxLength)
{
	OPENFILENAMEA ofn = {};
	char szFile[256] = {};
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = g_hWnd;
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = sizeof(szFile);
	ofn.lpstrFilter = "Imagens (*.bmp;*.png;*.jpg)\0*.bmp;*.png;*.jpg\0Todos (*.*)\0*.*\0";
	ofn.nFilterIndex = 1;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
	if (GetOpenFileNameA(&ofn)) {
		strncpy_s(outPath, maxLength, szFile, maxLength - 1);
		return true;
	}
	return false;
}