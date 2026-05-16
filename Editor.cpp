#include <fstream>
#include <sstream>
#include <cmath>
#include <functional>
#include <filesystem>
#include <string>
#include "Editor.h"
#include "Render.h"
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include "Level.h"
#include "./imgui/imgui.h"
#include "./imgui/imgui_impl_win32.h"
#include "./imgui/imgui_impl_dx11.h"

// Forward declaration - RenderEditorProject definida no final do arquivo
void RenderEditorProject();

// Forward declarations de Level.cpp (evita depender da versao do Level.h no projeto)
bool ExportStageTxt(const char* txtPath);
bool SaveGameProject(const char* fullPath);
bool LoadGameProject(const char* fullPath);

// ==========================================
// UTILITARIOS INTERNOS
// ==========================================

// Visibilidade de cada janela do editor. Cada panel agora e' uma janela
// ImGui independente — usuario abre/fecha via menu Visualizar ou pelo X
// do header. ImGui salva posicao/tamanho automaticamente em imgui.ini.
static bool s_showProjeto   = true;
static bool s_showJogador   = false;
static bool s_showBola      = false;
static bool s_showStage     = true;
static bool s_showObstaculo = false;
static bool s_showInimigo   = false;
static bool s_showBoss      = false;
static bool s_showMenu      = false;
static bool s_showPortal    = false;

// Trigger para exibir a janela "Sobre".
static bool s_showAbout     = false;

static void LoadSRV(const char* path, ID3D11ShaderResourceView** srv)
{
	if (!path || path[0] == '\0') return;
	if (*srv) {
		(*srv)->Release(); *srv = nullptr;
	}
	std::wstring wpath(path, path + strlen(path));
	CreateWICTextureFromFile(device, wpath.c_str(), nullptr, srv);
}

// ==========================================
// CAMINHOS BASE - Documents\TorrouEngine
// ==========================================

static const char* GetEngineBasePath()
{
	static char basePath[MAX_PATH] = {};
	if (basePath[0]) return basePath;
	char docs[MAX_PATH] = {};
	if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_PERSONAL, NULL, 0, docs)))
		sprintf_s(basePath, "%s\\TorrouEngine", docs);
	else
	{
		char* userProfile = nullptr;
		size_t len = 0;
		_dupenv_s(&userProfile, &len, "USERPROFILE");
		sprintf_s(basePath, "%s\\TorrouEngine", userProfile ? userProfile : ".");
		free(userProfile);
	}
	CreateDirectoryA(basePath, NULL);
	return basePath;
}

static const char* GetEngineAssetsPath()
{
	static char assetsPath[MAX_PATH] = {};
	if (assetsPath[0]) return assetsPath;
	sprintf_s(assetsPath, "%s\\Assets", GetEngineBasePath());
	CreateDirectoryA(assetsPath, NULL);
	return assetsPath;
}

// Monta o caminho absoluto de uma subpasta de objects
// Ex: subdir="objects\\PLAYER" -> Documents\TorrouEngine\objects\PLAYER
static void GetEngineObjectsPath(const char* subdir, char* outPath, int maxLen)
{
	char base[MAX_PATH];
	sprintf_s(base, "%s\\objects", GetEngineBasePath());
	CreateDirectoryA(base, NULL);
	const char* sub = subdir ? strrchr(subdir, '\\') : nullptr;
	sub = sub ? sub + 1 : subdir;
	if (sub && sub[0]) {
		sprintf_s(outPath, maxLen, "%s\\%s", base, sub);
		CreateDirectoryA(outPath, NULL);
	}
	else {
		strncpy_s(outPath, maxLen, base, maxLen - 1);
	}
}

// Copia um asset para Documents\TorrouEngine\Assets\ e retorna o novo path.
// Se o arquivo ja estiver em Assets, apenas copia o path.
static bool CopyAssetToEngine(const char* srcPath, char* destPath, int destLen)
{
	if (!srcPath || !srcPath[0]) return false;
	const char* assetsDir = GetEngineAssetsPath();
	if (_strnicmp(srcPath, assetsDir, strlen(assetsDir)) == 0) {
		strncpy_s(destPath, destLen, srcPath, destLen - 1);
		return true;
	}
	const char* fname = strrchr(srcPath, '\\');
	if (!fname) fname = strrchr(srcPath, '/');
	fname = fname ? fname + 1 : srcPath;
	sprintf_s(destPath, destLen, "%s\\%s", assetsDir, fname);
	return CopyFileA(srcPath, destPath, FALSE) != 0;
}

static bool OpenTextureFileDialogInternal(char* outPath, int maxLen)
{
	const char* assetsDir = GetEngineAssetsPath();
	OPENFILENAMEA ofn = {};
	char szFile[MAX_PATH] = {};
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = g_hWnd;
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = sizeof(szFile);
	ofn.lpstrFilter = "Images (*.png;*.jpg;*.bmp)\0*.png;*.jpg;*.bmp\0All Files\0*.*\0";
	ofn.nFilterIndex = 1;
	ofn.lpstrInitialDir = assetsDir;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
	if (GetOpenFileNameA(&ofn)) {
		// Copia o arquivo para Assets e usa o path dentro de Assets
		char dest[MAX_PATH] = {};
		CopyAssetToEngine(szFile, dest, MAX_PATH);
		strncpy_s(outPath, maxLen, dest[0] ? dest : szFile, maxLen - 1);
		return true;
	}
	return false;
}

bool OpenTextureFileDialog(char* outPath, int maxLen) {
	return OpenTextureFileDialogInternal(outPath, maxLen);
}

bool OpenJsonSaveDialog(char* outPath, int maxLen, const char* defaultDir)
{
	char absDir[MAX_PATH] = {};
	GetEngineObjectsPath(defaultDir, absDir, MAX_PATH);
	OPENFILENAMEA ofn = {};
	char szFile[MAX_PATH] = {};
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = g_hWnd;
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = sizeof(szFile);
	ofn.lpstrFilter = "JSON Files (*.json)\0*.json\0All Files\0*.*\0";
	ofn.nFilterIndex = 1;
	ofn.lpstrInitialDir = absDir;
	ofn.lpstrDefExt = "json";
	ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
	if (GetSaveFileNameA(&ofn)) {
		strncpy_s(outPath, maxLen, szFile, maxLen - 1); return true;
	}
	return false;
}

bool OpenJsonLoadDialog(char* outPath, int maxLen, const char* defaultDir)
{
	char absDir[MAX_PATH] = {};
	GetEngineObjectsPath(defaultDir, absDir, MAX_PATH);
	OPENFILENAMEA ofn = {};
	char szFile[MAX_PATH] = {};
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = g_hWnd;
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = sizeof(szFile);
	ofn.lpstrFilter = "JSON Files (*.json)\0*.json\0All Files\0*.*\0";
	ofn.nFilterIndex = 1;
	ofn.lpstrInitialDir = absDir;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
	if (GetOpenFileNameA(&ofn)) {
		strncpy_s(outPath, maxLen, szFile, maxLen - 1); return true;
	}
	return false;
}

static bool JsonSaveLoadButtons(const char* dir, char* sp, int ssl, char* lp, int lsl, bool* ds, bool* dl)
{
	*ds = *dl = false;
	if (ImGui::Button("Salvar", ImVec2(90, 0))) {
		char t[MAX_PATH] = {}; if (OpenJsonSaveDialog(t, sizeof(t), dir)) {
			strncpy_s(sp, ssl, t, ssl - 1); *ds = true;
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Carregar", ImVec2(90, 0))) {
		char t[MAX_PATH] = {}; if (OpenJsonLoadDialog(t, sizeof(t), dir)) {
			strncpy_s(lp, lsl, t, lsl - 1); *dl = true;
		}
	}
	return *ds || *dl;
}

static bool TextureButton(const char* label, char* pathBuf, int pathBufSize, const char* uid)
{
	ImGui::PushID(uid);
	bool changed = false;
	ImGui::Text("%s: %s", label, pathBuf[0] ? pathBuf : "(nenhuma)");
	char btnLabel[64]; sprintf_s(btnLabel, "Selecionar##%s", uid);
	if (ImGui::Button(btnLabel, ImVec2(180, 0))) {
		char tmp[256] = {};
		if (OpenTextureFileDialogInternal(tmp, sizeof(tmp))) {
			strcpy_s(pathBuf, pathBufSize, tmp); changed = true;
		}
	}
	if (pathBuf[0]) {
		std::ifstream t(pathBuf); bool ok = t.good(); t.close();
		ImGui::SameLine();
		ImGui::TextColored(ok ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1), ok ? "[OK]" : "[nao encontrado]");
	}
	ImGui::PopID();
	return changed;
}

static void DropTableEditor(DropTable& dt)
{
	ImGui::Checkbox("Tem Drop?", &dt.hasDrop);
	if (!dt.hasDrop) return;
	ImGui::Indent();
	const char* labels[] = { "Vida", "Shield", "(Reservado)", "Pontos x2" };
	float totalW = 0.0f; for (int i = 0; i < 4; i++) if (dt.entryEnabled[i]) totalW += dt.entryWeight[i];
	for (int i = 0; i < 4; i++) {
		char chkId[32]; sprintf_s(chkId, "##chk%d", i);
		ImGui::Checkbox(chkId, &dt.entryEnabled[i]); ImGui::SameLine();
		char sldId[32]; sprintf_s(sldId, "%s##w%d", labels[i], i);
		ImGui::BeginDisabled(!dt.entryEnabled[i]);
		ImGui::SliderFloat(sldId, &dt.entryWeight[i], 0.0f, 100.0f, "%.1f");
		ImGui::EndDisabled();
		if (dt.entryEnabled[i] && totalW > 0.0f)
			ImGui::SameLine(), ImGui::TextDisabled("(%.0f%%)", dt.entryWeight[i] / totalW * 100.0f);
	}
	ImGui::Unindent();
}

// ==========================================
// UPDATE DO EDITOR (input de mouse no viewport)
// ==========================================

// Drag state para objetos no stage editor
static int   s_dragObjectIdx = -1;     // indice em stageObjects (-1 = nenhum, -2 = bola spawn)
static bool  s_dragging = false;
static float s_dragOffsetX = 0.0f;
static float s_dragOffsetY = 0.0f;

// Click-to-set state for boss viewport interaction (used by UpdateEditor + RenderEditorBoss)
enum BossPickTarget { BOSS_PICK_NONE = 0, BOSS_PICK_XY };
static BossPickTarget s_bossPickTarget = BOSS_PICK_NONE;
static float* s_bossPickX = nullptr;
static float* s_bossPickY = nullptr;

static void ScreenToNDC(POINT pt, float& ndcX, float& ndcY)
{
	float halfW = (g_currentWidth  > 0) ? (g_currentWidth  * 0.5f) : 400.0f;
	float halfH = (g_currentHeight > 0) ? (g_currentHeight * 0.5f) : 300.0f;
	ndcX = (pt.x / halfW) - 1.0f;
	ndcY = -(pt.y / halfH) + 1.0f;
}

void UpdateEditor()
{
	static bool s_bossPickWasDown = false; // track LMB for boss pick (single-click, not drag)

	if (!ImGui::GetIO().WantCaptureMouse) {
		if (currentEditorMode == EDITOR_MODE_STAGE) {
			POINT pt; GetCursorPos(&pt); ScreenToClient(g_hWnd, &pt);
			float ndcX, ndcY;
			ScreenToNDC(pt, ndcX, ndcY);

			bool lmbDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

			if (lmbDown && !s_dragging) {
				// Inicio de drag — verifica se clicou em cima de algum objeto
				s_dragObjectIdx = -1;
				s_dragging = true;

				// Verifica bola spawn primeiro
				float bdx = ndcX - editorStageConfig.ballStartX;
				float bdy = ndcY - editorStageConfig.ballStartY;
				if (bdx * bdx + bdy * bdy < 0.01f) {
					s_dragObjectIdx = -2; // bola spawn
					s_dragOffsetX = bdx;
					s_dragOffsetY = bdy;
				}

				// Verifica stageObjects (ultimo adicionado tem prioridade)
				if (s_dragObjectIdx == -1) {
					for (int i = (int)stageObjects.size() - 1; i >= 0; i--) {
						auto& o = stageObjects[i];
						float hw = 0.05f, hh = 0.03f; // tamanho default para click test
						if (o.type == PLACED_BLOCK) {
							hw = editorBlockConfig.width / 2.0f;
							hh = editorBlockConfig.height / 2.0f;
						}
						else if (o.type == PLACED_OBSTACLE) {
							hw = editorObstacleConfig.width / 2.0f;
							hh = editorObstacleConfig.height / 2.0f;
						}
						else if (o.type == PLACED_BOSS) {
							hw = (editorBossConfig.width > 0 ? editorBossConfig.width : 0.2f) / 2.0f;
							hh = (editorBossConfig.height > 0 ? editorBossConfig.height : 0.2f) / 2.0f;
						}
						else if (o.type == PLACED_PORTAL) {
							hw = (editorPortalConfig.width  > 0 ? editorPortalConfig.width  : 0.10f) / 2.0f;
							hh = (editorPortalConfig.height > 0 ? editorPortalConfig.height : 0.15f) / 2.0f;
						}
						if (ndcX >= o.x - hw && ndcX <= o.x + hw &&
							ndcY >= o.y - hh && ndcY <= o.y + hh) {
							s_dragObjectIdx = i;
							s_dragOffsetX = ndcX - o.x;
							s_dragOffsetY = ndcY - o.y;
							break;
						}
					}
				}
			}
			else if (lmbDown && s_dragging) {
				// Continuacao do drag — move o objeto
				if (s_dragObjectIdx == -2) {
					editorStageConfig.ballStartX = ndcX - s_dragOffsetX;
					editorStageConfig.ballStartY = ndcY - s_dragOffsetY;
					ballX = editorStageConfig.ballStartX;
					ballY = editorStageConfig.ballStartY;
				}
				else if (s_dragObjectIdx >= 0 && s_dragObjectIdx < (int)stageObjects.size()) {
					stageObjects[s_dragObjectIdx].x = ndcX - s_dragOffsetX;
					stageObjects[s_dragObjectIdx].y = ndcY - s_dragOffsetY;
				}
			}
			else if (!lmbDown) {
				// Fim do drag
				s_dragging = false;
				s_dragObjectIdx = -1;
			}
		}
		// Boss tab: click-to-set position picking
		else if (currentEditorMode == EDITOR_MODE_BOSS && s_bossPickTarget == BOSS_PICK_XY && s_bossPickX && s_bossPickY) {
			bool lmbDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
			if (lmbDown && !s_bossPickWasDown) {
				POINT pt; GetCursorPos(&pt); ScreenToClient(g_hWnd, &pt);
				float ndcX, ndcY;
				ScreenToNDC(pt, ndcX, ndcY);
				*s_bossPickX = ndcX;
				*s_bossPickY = ndcY;
				s_bossPickTarget = BOSS_PICK_NONE;
				s_bossPickX = s_bossPickY = nullptr;
			}
			s_bossPickWasDown = lmbDown;
		}
	}
	else {
		// ImGui capturou o mouse — cancelar drag
		if (s_dragging) {
			s_dragging = false;
			s_dragObjectIdx = -1;
		}
		s_bossPickWasDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
	}
}

// ==========================================
// SAVE / LOAD -PLAYER
// ==========================================

bool SavePlayerConfig(const char* fullPath)
{
	std::ofstream f(fullPath); if (!f.is_open()) return false;
	const DamageEffectConfig& fx = editorPlayerConfig.damageEffect;
	f << "{\n";
	f << "  \"paddleWidth\": " << paddleEditWidth << ",\n";
	f << "  \"paddleHeight\": " << paddleEditHeight << ",\n";
	f << "  \"moveSpeed\": " << paddleEditMoveSpeed << ",\n";
	f << "  \"dashSpeed\": " << dashSpeed << ",\n";
	f << "  \"texturePath\": \"" << editorPlayerConfig.texturePath << "\",\n";
	f << "  \"runRightTexturePath\": \"" << editorPlayerConfig.runRightTexturePath << "\",\n";
	f << "  \"runLeftTexturePath\": \"" << editorPlayerConfig.runLeftTexturePath << "\",\n";
	f << "  \"projectileTexturePath\": \"" << editorPlayerConfig.projectileTexturePath << "\",\n";
	f << "  \"shieldTexturePath\": \"" << editorPlayerConfig.shieldTexturePath << "\",\n";
	f << "  \"dashRightTexturePath\": \"" << editorPlayerConfig.dashRightTexturePath << "\",\n";
	f << "  \"dashLeftTexturePath\": \"" << editorPlayerConfig.dashLeftTexturePath << "\",\n";
	f << "  \"dmgEffectType\": " << (int)fx.type << ",\n";
	f << "  \"dmgDuration\": " << fx.durationFrames << ",\n";
	f << "  \"dmgBlinkInterval\": " << fx.blinkIntervalFrames << ",\n";
	f << "  \"dmgTintR\": " << fx.tintR << ",\n";
	f << "  \"dmgTintG\": " << fx.tintG << ",\n";
	f << "  \"dmgTintB\": " << fx.tintB << ",\n";
	f << "  \"dmgTintA\": " << fx.tintA << ",\n";
	f << "  \"dmgSprite\": \"" << fx.damageSpritePath << "\"\n";
	f << "}\n"; f.close(); return true;
}

bool LoadPlayerConfig(const char* fullPath)
{
	std::ifstream f(fullPath); if (!f.is_open()) return false;
	std::string line;
	DamageEffectConfig& fx = editorPlayerConfig.damageEffect;
	while (std::getline(f, line)) {
		if (line.find("\"paddleWidth\"") != std::string::npos) sscanf_s(line.c_str(), " \"paddleWidth\": %f,", &paddleEditWidth);
		if (line.find("\"paddleHeight\"") != std::string::npos) sscanf_s(line.c_str(), " \"paddleHeight\": %f,", &paddleEditHeight);
		if (line.find("\"moveSpeed\"") != std::string::npos) sscanf_s(line.c_str(), " \"moveSpeed\": %f,", &paddleEditMoveSpeed);
		if (line.find("\"dashSpeed\"") != std::string::npos) sscanf_s(line.c_str(), " \"dashSpeed\": %f,", &dashSpeed);
		if (line.find("\"dmgEffectType\"") != std::string::npos) {
			int v; sscanf_s(line.c_str(), " \"dmgEffectType\": %d,", &v); fx.type = (DamageEffectType)v;
		}
		if (line.find("\"dmgDuration\"") != std::string::npos) sscanf_s(line.c_str(), " \"dmgDuration\": %d,", &fx.durationFrames);
		if (line.find("\"dmgBlinkInterval\"") != std::string::npos) sscanf_s(line.c_str(), " \"dmgBlinkInterval\": %d,", &fx.blinkIntervalFrames);
		if (line.find("\"dmgTintR\"") != std::string::npos) sscanf_s(line.c_str(), " \"dmgTintR\": %f,", &fx.tintR);
		if (line.find("\"dmgTintG\"") != std::string::npos) sscanf_s(line.c_str(), " \"dmgTintG\": %f,", &fx.tintG);
		if (line.find("\"dmgTintB\"") != std::string::npos) sscanf_s(line.c_str(), " \"dmgTintB\": %f,", &fx.tintB);
		if (line.find("\"dmgTintA\"") != std::string::npos) sscanf_s(line.c_str(), " \"dmgTintA\": %f", &fx.tintA);
		auto readStr = [&](const char* key, char* dest, int sz) {
			if (line.find(key) != std::string::npos) {
				size_t s = line.find(": \"") + 3, e = line.rfind("\"");
				if (s < e) {
					std::string v = line.substr(s, e - s); strcpy_s(dest, sz, v.c_str());
				}
			}
			};
		readStr("\"texturePath\"", editorPlayerConfig.texturePath, 256);
		readStr("\"runRightTexturePath\"", editorPlayerConfig.runRightTexturePath, 256);
		readStr("\"runLeftTexturePath\"", editorPlayerConfig.runLeftTexturePath, 256);
		readStr("\"projectileTexturePath\"", editorPlayerConfig.projectileTexturePath, 256);
		readStr("\"shieldTexturePath\"", editorPlayerConfig.shieldTexturePath, 256);
		readStr("\"dashRightTexturePath\"", editorPlayerConfig.dashRightTexturePath, 256);
		readStr("\"dashLeftTexturePath\"", editorPlayerConfig.dashLeftTexturePath, 256);
		readStr("\"dmgSprite\"", fx.damageSpritePath, 256);
	}
	f.close();
	paddleWidth = paddleEditWidth;
	paddleHeight = paddleHeightNormal;
	LoadSRV(editorPlayerConfig.texturePath,           &editorPlayerTexture);
	LoadSRV(editorPlayerConfig.runRightTexturePath,   &editorPlayerRunRightTexture);
	LoadSRV(editorPlayerConfig.runLeftTexturePath,    &editorPlayerRunLeftTexture);
	LoadSRV(editorPlayerConfig.dashRightTexturePath,  &editorPlayerDashRightTexture);
	LoadSRV(editorPlayerConfig.dashLeftTexturePath,   &editorPlayerDashLeftTexture);
	LoadSRV(editorPlayerConfig.projectileTexturePath, &editorProjectileTexture);
	return true;
}

// ==========================================
// SAVE / LOAD -BALL
// ==========================================

bool SaveBallConfig(const char* fullPath)
{
	std::ofstream f(fullPath); if (!f.is_open()) return false;
	f << "{\n";
	f << "  \"ballSize\": " << ballSize << ",\n";
	f << "  \"initialSpeed\": " << editorBallConfig.initialSpeed << ",\n";
	f << "  \"texturePath\": \"" << editorBallConfig.texturePath << "\"\n";
	f << "}\n"; f.close(); return true;
}

bool LoadBallConfig(const char* fullPath)
{
	std::ifstream f(fullPath); if (!f.is_open()) return false;
	std::string line;
	while (std::getline(f, line)) {
		if (line.find("\"ballSize\"") != std::string::npos) sscanf_s(line.c_str(), " \"ballSize\": %f,", &ballSize);
		if (line.find("\"initialSpeed\"") != std::string::npos) sscanf_s(line.c_str(), " \"initialSpeed\": %f,", &editorBallConfig.initialSpeed);
		if (line.find("\"texturePath\"") != std::string::npos) {
			size_t s = line.find(": \"") + 3, e = line.rfind("\"");
			if (s < e) {
				std::string v = line.substr(s, e - s); strcpy_s(editorBallConfig.texturePath, 256, v.c_str());
			}
		}
	}
	f.close();
	LoadSRV(editorBallConfig.texturePath, &editorBallTexture);
	return true;
}

// ==========================================
// SAVE / LOAD -STAGE
// ==========================================

bool SaveStageConfig(const char* fullPath) {
	return SaveStageJSON(fullPath);
}
bool LoadStageConfig(const char* fullPath) {
	return LoadStageJSON(fullPath);
}

// ==========================================
// SAVE / LOAD -OBSTACLE
// ==========================================

bool SaveObstacleConfig(const char* fullPath)
{
	std::ofstream f(fullPath); if (!f.is_open()) return false;
	f << "{\n";
	f << "  \"name\": \"" << editorObstacleConfig.name << "\",\n";
	f << "  \"width\": " << editorObstacleConfig.width << ",\n";
	f << "  \"height\": " << editorObstacleConfig.height << ",\n";
	f << "  \"useTexture\": " << (editorObstacleConfig.useTexture ? "true" : "false") << ",\n";
	f << "  \"texture\": \"" << editorObstacleConfig.texturePath << "\",\n";
	f << "  \"color\": { \"r\": " << editorObstacleConfig.colorR
		<< ", \"g\": " << editorObstacleConfig.colorG
		<< ", \"b\": " << editorObstacleConfig.colorB
		<< ", \"a\": " << editorObstacleConfig.colorA << " }\n";
	f << "}\n"; f.close(); return true;
}

bool LoadObstacleConfig(const char* fullPath)
{
	std::ifstream f(fullPath); if (!f.is_open()) return false;
	std::string line;
	while (std::getline(f, line)) {
		if (line.find("\"width\"") != std::string::npos) sscanf_s(line.c_str(), " \"width\": %f,", &editorObstacleConfig.width);
		if (line.find("\"height\"") != std::string::npos) sscanf_s(line.c_str(), " \"height\": %f,", &editorObstacleConfig.height);
		if (line.find("\"useTexture\": true") != std::string::npos) editorObstacleConfig.useTexture = true;
		if (line.find("\"useTexture\": false") != std::string::npos) editorObstacleConfig.useTexture = false;
		if (line.find("\"r\":") != std::string::npos) sscanf_s(line.c_str(), " \"color\": { \"r\": %f , \"g\": %f , \"b\": %f , \"a\": %f }",
			&editorObstacleConfig.colorR, &editorObstacleConfig.colorG, &editorObstacleConfig.colorB, &editorObstacleConfig.colorA);
		if (line.find("\"texture\"") != std::string::npos) {
			size_t s = line.find(": \"") + 3, e = line.rfind("\"");
			if (s < e) {
				std::string v = line.substr(s, e - s); strcpy_s(editorObstacleConfig.texturePath, 256, v.c_str());
			}
		}
	}
	f.close(); return true;
}

// ==========================================
// SAVE / LOAD -BLOCK (ENEMY)
// ==========================================

bool SaveBlockConfig(const char* fullPath)
{
	const DropTable& dt = editorBlockConfig.dropTable;
	std::ofstream f(fullPath); if (!f.is_open()) return false;
	f << "{\n";
	f << "  \"name\": \"" << editorBlockConfig.name << "\",\n";
	f << "  \"texturePath\": \"" << editorBlockConfig.texturePath << "\",\n";
	f << "  \"width\": " << editorBlockConfig.width << ",\n";
	f << "  \"height\": " << editorBlockConfig.height << ",\n";
	f << "  \"useTexture\": " << (editorBlockConfig.useTexture ? "true" : "false") << ",\n";
	f << "  \"colorR\": " << editorBlockConfig.colorR << ",\n";
	f << "  \"colorG\": " << editorBlockConfig.colorG << ",\n";
	f << "  \"colorB\": " << editorBlockConfig.colorB << ",\n";
	f << "  \"colorA\": " << editorBlockConfig.colorA << ",\n";
	f << "  \"maxHits\": " << editorBlockConfig.maxHits << ",\n";
	f << "  \"bulletPattern\": " << editorBlockConfig.bulletPattern << ",\n";
	f << "  \"bulletCount\": " << editorBlockConfig.bulletCount << ",\n";
	f << "  \"bulletSpeed\": " << editorBlockConfig.bulletSpeed << ",\n";
	f << "  \"shootInterval\": " << editorBlockConfig.shootIntervalFrames << ",\n";
	f << "  \"burstStep\": " << editorBlockConfig.burstStepFrames << ",\n";
	f << "  \"invulnerable\": " << (editorBlockConfig.invulnerable ? "true" : "false") << ",\n";
	f << "  \"movType\": " << (int)editorBlockConfig.movType << ",\n";
	f << "  \"movSpeed\": " << editorBlockConfig.movSpeed << ",\n";
	f << "  \"movAmplitude\": " << editorBlockConfig.movAmplitude << ",\n";
	f << "  \"movRadius\": " << editorBlockConfig.movRadius << ",\n";
	f << "  \"hasDrop\": " << (dt.hasDrop ? "true" : "false") << ",\n";
	for (int i = 0; i < 4; i++)
		f << "  \"dropEnabled" << i << "\": " << (dt.entryEnabled[i] ? "true" : "false") << ",\n"
		<< "  \"dropWeight" << i << "\": " << dt.entryWeight[i] << (i < 3 ? "," : "") << "\n";
	f << "}\n"; f.close(); return true;
}

bool LoadBlockConfig(const char* fullPath)
{
	std::ifstream f(fullPath); if (!f.is_open()) return false;
	std::string line; DropTable& dt = editorBlockConfig.dropTable;
	while (std::getline(f, line)) {
		if (line.find("\"width\"") != std::string::npos) sscanf_s(line.c_str(), " \"width\": %f,", &editorBlockConfig.width);
		if (line.find("\"height\"") != std::string::npos) sscanf_s(line.c_str(), " \"height\": %f,", &editorBlockConfig.height);
		if (line.find("\"useTexture\": true") != std::string::npos) editorBlockConfig.useTexture = true;
		if (line.find("\"useTexture\": false") != std::string::npos) editorBlockConfig.useTexture = false;
		if (line.find("\"colorR\"") != std::string::npos) sscanf_s(line.c_str(), " \"colorR\": %f,", &editorBlockConfig.colorR);
		if (line.find("\"colorG\"") != std::string::npos) sscanf_s(line.c_str(), " \"colorG\": %f,", &editorBlockConfig.colorG);
		if (line.find("\"colorB\"") != std::string::npos) sscanf_s(line.c_str(), " \"colorB\": %f,", &editorBlockConfig.colorB);
		if (line.find("\"colorA\"") != std::string::npos) sscanf_s(line.c_str(), " \"colorA\": %f,", &editorBlockConfig.colorA);
		if (line.find("\"maxHits\"") != std::string::npos) sscanf_s(line.c_str(), " \"maxHits\": %d,", &editorBlockConfig.maxHits);
		if (line.find("\"bulletPattern\"") != std::string::npos) sscanf_s(line.c_str(), " \"bulletPattern\": %d,", &editorBlockConfig.bulletPattern);
		if (line.find("\"bulletCount\"") != std::string::npos) sscanf_s(line.c_str(), " \"bulletCount\": %d,", &editorBlockConfig.bulletCount);
		if (line.find("\"bulletSpeed\"") != std::string::npos) sscanf_s(line.c_str(), " \"bulletSpeed\": %f,", &editorBlockConfig.bulletSpeed);
		if (line.find("\"shootInterval\"") != std::string::npos) sscanf_s(line.c_str(), " \"shootInterval\": %d,", &editorBlockConfig.shootIntervalFrames);
		if (line.find("\"burstStep\"") != std::string::npos) sscanf_s(line.c_str(), " \"burstStep\": %d,", &editorBlockConfig.burstStepFrames);
		if (line.find("\"invulnerable\": true") != std::string::npos) editorBlockConfig.invulnerable = true;
		if (line.find("\"invulnerable\": false") != std::string::npos) editorBlockConfig.invulnerable = false;
		if (line.find("\"movType\"") != std::string::npos) {
			int v; sscanf_s(line.c_str(), " \"movType\": %d,", &v); editorBlockConfig.movType = (EnemyMovType)v;
		}
		if (line.find("\"movSpeed\"") != std::string::npos) sscanf_s(line.c_str(), " \"movSpeed\": %f,", &editorBlockConfig.movSpeed);
		if (line.find("\"movAmplitude\"") != std::string::npos) sscanf_s(line.c_str(), " \"movAmplitude\": %f,", &editorBlockConfig.movAmplitude);
		if (line.find("\"movRadius\"") != std::string::npos) sscanf_s(line.c_str(), " \"movRadius\": %f,", &editorBlockConfig.movRadius);
		if (line.find("\"hasDrop\": true") != std::string::npos) dt.hasDrop = true;
		if (line.find("\"hasDrop\": false") != std::string::npos) dt.hasDrop = false;
		for (int i = 0; i < 4; i++) {
			char key[32]; sprintf_s(key, "\"dropEnabled%d\"", i);
			if (line.find(key) != std::string::npos) {
				if (line.find("true") != std::string::npos) dt.entryEnabled[i] = true;
				if (line.find("false") != std::string::npos) dt.entryEnabled[i] = false;
			}
			sprintf_s(key, "\"dropWeight%d\"", i);
			if (line.find(key) != std::string::npos) sscanf_s(line.c_str(), " \"%*[^\"]\": %f,", &dt.entryWeight[i]);
		}
		if (line.find("\"texturePath\"") != std::string::npos) {
			size_t s = line.find(": \"") + 3, e = line.rfind("\"");
			if (s < e) {
				std::string v = line.substr(s, e - s); strcpy_s(editorBlockConfig.texturePath, 256, v.c_str());
			}
		}
	}
	f.close();
	LoadSRV(editorBlockConfig.texturePath, &editorBlockTexture);
	return true;
}

// ==========================================
// SAVE / LOAD — BOSS (completo: fases HP, scripts, familiares, nos multi-part)
// ==========================================

static void SaveBossAction(std::ofstream& f, const char* pre, const BossAction& act)
{
	f << "  \"" << pre << "_type\": " << (int)act.type << ",\n";
	f << "  \"" << pre << "_targetX\": " << act.targetX << ",\n";
	f << "  \"" << pre << "_targetY\": " << act.targetY << ",\n";
	f << "  \"" << pre << "_speed\": " << act.speed << ",\n";
	f << "  \"" << pre << "_bulletPattern\": " << act.bulletPattern << ",\n";
	f << "  \"" << pre << "_bulletCount\": " << act.bulletCount << ",\n";
	f << "  \"" << pre << "_bulletSpeed\": " << act.bulletSpeed << ",\n";
	f << "  \"" << pre << "_duration\": " << act.duration << ",\n";
	f << "  \"" << pre << "_fixedPtCount\": " << act.fixedPointCount << ",\n";
	for (int fp = 0; fp < act.fixedPointCount; fp++) {
		f << "  \"" << pre << "_fp" << fp << "x\": " << act.fixedPtsX[fp] << ",\n";
		f << "  \"" << pre << "_fp" << fp << "y\": " << act.fixedPtsY[fp] << ",\n";
	}
	f << "  \"" << pre << "_spawnX\": " << act.spawnX << ",\n";
	f << "  \"" << pre << "_spawnY\": " << act.spawnY << ",\n";
	f << "  \"" << pre << "_minionIdx\": " << act.minionPatternIndex << ",\n";
	f << "  \"" << pre << "_spritePath\": \"" << act.spritePath << "\",\n";
	f << "  \"" << pre << "_invincible\": " << (act.invincibleOn ? 1 : 0) << ",\n";
}

static void SaveBossScript(std::ofstream& f, const char* pre, const BossScript& sc)
{
	f << "  \"" << pre << "_actionCount\": " << sc.actionCount << ",\n";
	f << "  \"" << pre << "_loopFromStep\": " << sc.loopFromStep << ",\n";
	for (int a = 0; a < sc.actionCount; a++) {
		char apre[96]; sprintf_s(apre, "%s_a%d", pre, a);
		SaveBossAction(f, apre, sc.actions[a]);
	}
}

bool SaveBossConfig(const char* fullPath)
{
	std::ofstream f(fullPath); if (!f.is_open()) return false;
	auto& bc = editorBossConfig;
	f << "{\n";
	f << "  \"name\": \"" << bc.name << "\",\n";
	f << "  \"texturePath\": \"" << bc.texturePath << "\",\n";
	f << "  \"maxHP\": " << bc.maxHP << ",\n";
	f << "  \"width\": " << bc.width << ",\n";
	f << "  \"height\": " << bc.height << ",\n";
	f << "  \"archetype\": " << (int)bc.archetype << ",\n";
	f << "  \"startX\": " << bc.startX << ",\n";
	f << "  \"startY\": " << bc.startY << ",\n";
	f << "  \"hpPhaseCount\": " << bc.hpPhaseCount << ",\n";
	f << "  \"familiarCount\": " << bc.familiarCount << ",\n";
	f << "  \"nodeCount\": " << bc.nodeCount << ",\n";

	// HP Phases + Scripts
	for (int p = 0; p < bc.hpPhaseCount; p++) {
		char pre[32]; sprintf_s(pre, "hp%d", p);
		f << "  \"" << pre << "_threshold\": " << bc.hpPhases[p].hpThresholdPct << ",\n";
		char scPre[48]; sprintf_s(scPre, "hp%d", p);
		SaveBossScript(f, scPre, bc.hpPhases[p].script);
	}

	// Familiars
	for (int i = 0; i < bc.familiarCount; i++) {
		auto& fam = bc.familiars[i];
		char pre[32]; sprintf_s(pre, "fam%d", i);
		f << "  \"" << pre << "_offsetX\": " << fam.relOffsetX << ",\n";
		f << "  \"" << pre << "_offsetY\": " << fam.relOffsetY << ",\n";
		f << "  \"" << pre << "_orbitRadius\": " << fam.orbitRadius << ",\n";
		f << "  \"" << pre << "_orbitSpeed\": " << fam.orbitSpeed << ",\n";
		f << "  \"" << pre << "_bulletPattern\": " << fam.bulletPattern << ",\n";
		f << "  \"" << pre << "_bulletCount\": " << fam.bulletCount << ",\n";
		f << "  \"" << pre << "_bulletSpeed\": " << fam.bulletSpeed << ",\n";
		f << "  \"" << pre << "_shootInterval\": " << fam.shootIntervalSec << ",\n";
		f << "  \"" << pre << "_texture\": \"" << fam.texturePath << "\",\n";
	}

	// Multi-part Nodes + Scripts
	for (int i = 0; i < bc.nodeCount; i++) {
		auto& node = bc.nodes[i];
		char pre[32]; sprintf_s(pre, "node%d", i);
		f << "  \"" << pre << "_texture\": \"" << node.texturePath << "\",\n";
		f << "  \"" << pre << "_startX\": " << node.startX << ",\n";
		f << "  \"" << pre << "_startY\": " << node.startY << ",\n";
		SaveBossScript(f, pre, node.script);
	}

	f << "  \"__end\": 0\n";
	f << "}\n";
	f.close(); return true;
}

// Helper: parse "prefix<int>_rest" from a key, returns index and rest. -1 if no match.
static bool ParseKeyPrefix(const std::string& key, const char* prefix, int& idx, std::string& rest)
{
	size_t plen = strlen(prefix);
	if (key.compare(0, plen, prefix) != 0) return false;
	size_t numEnd = key.find('_', plen);
	if (numEnd == std::string::npos) return false;
	try { idx = std::stoi(key.substr(plen, numEnd - plen)); } catch (...) { return false; }
	rest = key.substr(numEnd + 1);
	return true;
}

static void LoadBossActionField(BossAction& act, const std::string& field,
	const std::function<float()>& valF, const std::function<int()>& valI,
	const std::function<void(char*, int)>& valStr)
{
	if (field == "type") act.type = (BossActionType)valI();
	else if (field == "targetX") act.targetX = valF();
	else if (field == "targetY") act.targetY = valF();
	else if (field == "speed") act.speed = valF();
	else if (field == "bulletPattern") act.bulletPattern = valI();
	else if (field == "bulletCount") act.bulletCount = valI();
	else if (field == "bulletSpeed") act.bulletSpeed = valF();
	else if (field == "duration") act.duration = valF();
	else if (field == "fixedPtCount") act.fixedPointCount = valI();
	else if (field == "spawnX") act.spawnX = valF();
	else if (field == "spawnY") act.spawnY = valF();
	else if (field == "minionIdx") act.minionPatternIndex = valI();
	else if (field == "spritePath") valStr(act.spritePath, 256);
	else if (field == "invincible") act.invincibleOn = (valI() != 0);
	else {
		// Fixed points: fp0x, fp0y, etc.
		int fpIdx = -1; char axis = 0;
		if (sscanf_s(field.c_str(), "fp%d%c", &fpIdx, &axis, 1) == 2 && fpIdx >= 0 && fpIdx < 8) {
			if (axis == 'x') act.fixedPtsX[fpIdx] = valF();
			else if (axis == 'y') act.fixedPtsY[fpIdx] = valF();
		}
	}
}

bool LoadBossConfig(const char* fullPath)
{
	std::ifstream f(fullPath); if (!f.is_open()) return false;
	editorBossConfig = {};
	auto& bc = editorBossConfig;

	std::string line;
	while (std::getline(f, line)) {
		// Extract key between first pair of quotes
		size_t q1 = line.find('"'); if (q1 == std::string::npos) continue;
		size_t q2 = line.find('"', q1 + 1); if (q2 == std::string::npos) continue;
		std::string key = line.substr(q1 + 1, q2 - q1 - 1);

		// Extract value string after ": "
		size_t colon = line.find(':', q2); if (colon == std::string::npos) continue;
		std::string vstr = line.substr(colon + 1);
		while (!vstr.empty() && (vstr.front() == ' ' || vstr.front() == '\t')) vstr.erase(vstr.begin());
		while (!vstr.empty() && (vstr.back() == ',' || vstr.back() == ' ' || vstr.back() == '\n' || vstr.back() == '\r')) vstr.pop_back();

		auto valF = [&]() -> float { try { return std::stof(vstr); } catch (...) { return 0.0f; } };
		auto valI = [&]() -> int { try { return std::stoi(vstr); } catch (...) { return 0; } };
		auto valStr = [&](char* dest, int sz) {
			size_t s = vstr.find('"'), e = vstr.rfind('"');
			if (s != std::string::npos && e != std::string::npos && s < e) {
				std::string v = vstr.substr(s + 1, e - s - 1);
				strcpy_s(dest, sz, v.c_str());
			}
		};

		// Simple keys
		if (key == "name") { valStr(bc.name, 64); continue; }
		if (key == "texturePath") { valStr(bc.texturePath, 256); continue; }
		if (key == "maxHP") { bc.maxHP = valI(); continue; }
		if (key == "width") { bc.width = valF(); continue; }
		if (key == "height") { bc.height = valF(); continue; }
		if (key == "archetype") { bc.archetype = (BossArchetype)valI(); continue; }
		if (key == "startX") { bc.startX = valF(); continue; }
		if (key == "startY") { bc.startY = valF(); continue; }
		if (key == "hpPhaseCount") { bc.hpPhaseCount = valI(); continue; }
		if (key == "familiarCount") { bc.familiarCount = valI(); continue; }
		if (key == "nodeCount") { bc.nodeCount = valI(); continue; }

		int idx = -1; std::string rest;

		// HP Phase keys: "hp<p>_..."
		if (ParseKeyPrefix(key, "hp", idx, rest) && idx >= 0 && idx < BOSS_MAX_HP_PHASES) {
			if (rest == "threshold") { bc.hpPhases[idx].hpThresholdPct = valF(); continue; }
			if (rest == "actionCount") { bc.hpPhases[idx].script.actionCount = valI(); continue; }
			if (rest == "loopFromStep") { bc.hpPhases[idx].script.loopFromStep = valI(); continue; }
			// Action fields: "a<a>_field"
			int aIdx = -1; std::string aField;
			if (ParseKeyPrefix(rest, "a", aIdx, aField) && aIdx >= 0 && aIdx < BOSS_SCRIPT_MAX_ACTIONS) {
				LoadBossActionField(bc.hpPhases[idx].script.actions[aIdx], aField, valF, valI, valStr);
			}
			continue;
		}

		// Familiar keys: "fam<i>_..."
		if (ParseKeyPrefix(key, "fam", idx, rest) && idx >= 0 && idx < BOSS_MAX_FAMILIARS) {
			auto& fam = bc.familiars[idx];
			if (rest == "offsetX") fam.relOffsetX = valF();
			else if (rest == "offsetY") fam.relOffsetY = valF();
			else if (rest == "orbitRadius") fam.orbitRadius = valF();
			else if (rest == "orbitSpeed") fam.orbitSpeed = valF();
			else if (rest == "bulletPattern") fam.bulletPattern = valI();
			else if (rest == "bulletCount") fam.bulletCount = valI();
			else if (rest == "bulletSpeed") fam.bulletSpeed = valF();
			else if (rest == "shootInterval") fam.shootIntervalSec = valF();
			else if (rest == "texture") valStr(fam.texturePath, 256);
			continue;
		}

		// Node keys: "node<i>_..."
		if (ParseKeyPrefix(key, "node", idx, rest) && idx >= 0 && idx < BOSS_MAX_NODES) {
			auto& node = bc.nodes[idx];
			if (rest == "texture") { valStr(node.texturePath, 256); continue; }
			if (rest == "startX") { node.startX = valF(); continue; }
			if (rest == "startY") { node.startY = valF(); continue; }
			if (rest == "actionCount") { node.script.actionCount = valI(); continue; }
			if (rest == "loopFromStep") { node.script.loopFromStep = valI(); continue; }
			int aIdx = -1; std::string aField;
			if (ParseKeyPrefix(rest, "a", aIdx, aField) && aIdx >= 0 && aIdx < BOSS_SCRIPT_MAX_ACTIONS) {
				LoadBossActionField(node.script.actions[aIdx], aField, valF, valI, valStr);
			}
			continue;
		}
	}

	f.close();
	LoadSRV(bc.texturePath, &editorBossTexture);
	return true;
}

// ==========================================
// SAVE / LOAD -MENU
// ==========================================

bool SaveMenuConfig(const char* fullPath)
{
	std::ofstream f(fullPath); if (!f.is_open()) return false;
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
	f << "  \"logoHeight\": " << editorMenuConfig.logoHeight << ",\n";
	// Tipografia
	f << "  \"txtR\": " << editorMenuConfig.textColorR << ", \"txtG\": " << editorMenuConfig.textColorG << ", \"txtB\": " << editorMenuConfig.textColorB << ", \"txtA\": " << editorMenuConfig.textColorA << ",\n";
	f << "  \"txtSelR\": " << editorMenuConfig.textSelectedColorR << ", \"txtSelG\": " << editorMenuConfig.textSelectedColorG << ", \"txtSelB\": " << editorMenuConfig.textSelectedColorB << ", \"txtSelA\": " << editorMenuConfig.textSelectedColorA << ",\n";
	f << "  \"textSize\": " << editorMenuConfig.textSize << ",\n";
	f << "  \"fontPath\": \"" << editorMenuConfig.fontPath << "\"\n";
	f << "}\n"; f.close(); return true;
}

bool LoadMenuConfig(const char* fullPath)
{
	std::ifstream f(fullPath); if (!f.is_open()) return false;
	std::string line;
	while (std::getline(f, line)) {
		if (line.find("\"bgR\"") != std::string::npos) sscanf_s(line.c_str(), " \"bgR\": %f , \"bgG\": %f , \"bgB\": %f , \"bgA\": %f ,", &editorMenuConfig.bgColorR, &editorMenuConfig.bgColorG, &editorMenuConfig.bgColorB, &editorMenuConfig.bgColorA);
		if (line.find("\"btnR\"") != std::string::npos) sscanf_s(line.c_str(), " \"btnR\": %f , \"btnG\": %f , \"btnB\": %f , \"btnA\": %f ,", &editorMenuConfig.buttonColorR, &editorMenuConfig.buttonColorG, &editorMenuConfig.buttonColorB, &editorMenuConfig.buttonColorA);
		if (line.find("\"selR\"") != std::string::npos) sscanf_s(line.c_str(), " \"selR\": %f , \"selG\": %f , \"selB\": %f , \"selA\": %f ,", &editorMenuConfig.selectedColorR, &editorMenuConfig.selectedColorG, &editorMenuConfig.selectedColorB, &editorMenuConfig.selectedColorA);
		if (line.find("\"logoX\"") != std::string::npos) sscanf_s(line.c_str(), " \"logoX\": %f ,", &editorMenuConfig.logoX);
		if (line.find("\"logoY\"") != std::string::npos) sscanf_s(line.c_str(), " \"logoY\": %f ,", &editorMenuConfig.logoY);
		if (line.find("\"logoWidth\"") != std::string::npos) sscanf_s(line.c_str(), " \"logoWidth\": %f ,", &editorMenuConfig.logoWidth);
		if (line.find("\"logoHeight\"") != std::string::npos) sscanf_s(line.c_str(), " \"logoHeight\": %f", &editorMenuConfig.logoHeight);
		// Tipografia (campos opcionais — defaults preservados se ausentes)
		if (line.find("\"txtR\"") != std::string::npos) sscanf_s(line.c_str(), " \"txtR\": %f , \"txtG\": %f , \"txtB\": %f , \"txtA\": %f ,", &editorMenuConfig.textColorR, &editorMenuConfig.textColorG, &editorMenuConfig.textColorB, &editorMenuConfig.textColorA);
		if (line.find("\"txtSelR\"") != std::string::npos) sscanf_s(line.c_str(), " \"txtSelR\": %f , \"txtSelG\": %f , \"txtSelB\": %f , \"txtSelA\": %f ,", &editorMenuConfig.textSelectedColorR, &editorMenuConfig.textSelectedColorG, &editorMenuConfig.textSelectedColorB, &editorMenuConfig.textSelectedColorA);
		if (line.find("\"textSize\"") != std::string::npos) sscanf_s(line.c_str(), " \"textSize\": %f ,", &editorMenuConfig.textSize);
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
		readStr("\"fontPath\"", editorMenuConfig.fontPath, 256);
	}
	f.close();
	LoadSRV(editorMenuConfig.bgTexturePath, &menuBgTexture);
	LoadSRV(editorMenuConfig.titleTexturePath, &menuTitleTexture);
	LoadSRV(editorMenuConfig.selectorTexturePath, &menuSelectorTexture);
	return true;
}

// ==========================================
// PAINEL -JOGADOR
// ==========================================

static void BulletPatternCombo(const char* label, int& pattern)
{
	const char* patterns[] = {
		"0 - Leque (direcao jogador)", "1 - Rajada", "2 - Radial", "3 - Espiral", "4 - Aleatorio", "5 - Fixo para baixo"
	};
	ImGui::Combo(label, &pattern, patterns, IM_ARRAYSIZE(patterns));
}

void RenderEditorPlayer()
{
	ImGui::Text("=== EDITOR DE JOGADOR ===");
	ImGui::Separator();

	if (ImGui::CollapsingHeader("Dimensoes e Velocidade", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::SliderFloat("Largura", &paddleEditWidth, 0.02f, 0.5f);
		ImGui::SliderFloat("Altura", &paddleEditHeight, 0.01f, 0.5f);
		ImGui::SliderFloat("Vel. Movimento", &paddleEditMoveSpeed, 0.001f, 0.05f, "%.4f");
		ImGui::SliderFloat("Vel. Dash", &dashSpeed, 0.005f, 0.1f, "%.4f");
		// Aplica imediatamente no paddleWidth/Height
		paddleWidth = paddleEditWidth;
	}

	if (ImGui::CollapsingHeader("Sprites", ImGuiTreeNodeFlags_DefaultOpen)) {
		if (TextureButton("Parado", editorPlayerConfig.texturePath, 256, "spr_idle"))
			LoadSRV(editorPlayerConfig.texturePath, &editorPlayerTexture);
		if (TextureButton("Correndo Direita", editorPlayerConfig.runRightTexturePath, 256, "spr_run_r"))
			LoadSRV(editorPlayerConfig.runRightTexturePath, &editorPlayerRunRightTexture);
		if (TextureButton("Correndo Esquerda", editorPlayerConfig.runLeftTexturePath, 256, "spr_run_l"))
			LoadSRV(editorPlayerConfig.runLeftTexturePath, &editorPlayerRunLeftTexture);
		TextureButton("Projetil", editorPlayerConfig.projectileTexturePath, 256, "spr_proj");
		TextureButton("Escudo", editorPlayerConfig.shieldTexturePath, 256, "spr_shield");
		if (TextureButton("Dash Direita", editorPlayerConfig.dashRightTexturePath, 256, "spr_dash_r"))
			LoadSRV(editorPlayerConfig.dashRightTexturePath, &editorPlayerDashRightTexture);
		if (TextureButton("Dash Esquerda", editorPlayerConfig.dashLeftTexturePath, 256, "spr_dash_l"))
			LoadSRV(editorPlayerConfig.dashLeftTexturePath, &editorPlayerDashLeftTexture);
	}

	if (ImGui::CollapsingHeader("Efeito ao Receber Dano")) {
		DamageEffectConfig& fx = editorPlayerConfig.damageEffect;
		const char* fxNames[] = { "Piscar", "Mudar de cor (tint)", "Mudar de sprite" };
		int fxType = (int)fx.type;
		ImGui::Text("Tipo de efeito (exclusivo):");
		for (int i = 0; i < 3; i++) {
			if (ImGui::RadioButton(fxNames[i], fxType == i)) fxType = i;
		}
		fx.type = (DamageEffectType)fxType;
		ImGui::SliderInt("Duracao (frames)", &fx.durationFrames, 30, 300);

		if (fx.type == DMG_BLINK) {
			ImGui::SliderInt("Intervalo piscar (frames)", &fx.blinkIntervalFrames, 1, 20);
		}
		else if (fx.type == DMG_TINT) {
			ImGui::ColorEdit4("Cor do tint", &fx.tintR, ImGuiColorEditFlags_NoInputs);
		}
		else if (fx.type == DMG_SPRITE) {
			TextureButton("Sprite de dano", fx.damageSpritePath, 256, "spr_dmg");
		}
	}

	ImGui::Separator();
	{
		char sp[MAX_PATH] = {}, lp[MAX_PATH] = {}; bool ds, dl;
		if (JsonSaveLoadButtons("objects\\PLAYER", sp, MAX_PATH, lp, MAX_PATH, &ds, &dl)) {
			if (ds) SavePlayerConfig(sp);
			if (dl) LoadPlayerConfig(lp);
		}
	}
}

// ==========================================
// PAINEL -BOLA
// ==========================================

void RenderEditorBall()
{
	ImGui::Text("=== EDITOR DE BOLA ===");
	ImGui::Separator();

	ImGui::SliderFloat("Tamanho", &ballSize, 0.01f, 0.05f);
	ImGui::SliderFloat("Velocidade Ini.", &editorBallConfig.initialSpeed, 0.005f, 0.04f, "%.4f");
	if (TextureButton("Sprite da Bola", editorBallConfig.texturePath, 256, "spr_ball"))
		LoadSRV(editorBallConfig.texturePath, &editorBallTexture);

	ImGui::Separator();
	{
		char sp[MAX_PATH] = {}, lp[MAX_PATH] = {}; bool ds, dl;
		if (JsonSaveLoadButtons("objects\\BALL", sp, MAX_PATH, lp, MAX_PATH, &ds, &dl)) {
			if (ds) SaveBallConfig(sp);
			if (dl) LoadBallConfig(lp);
		}
	}
}

// ==========================================
// PAINEL -STAGE EDITOR
// ==========================================

void RenderEditorStage()
{
	ImGui::Text("=== EDITOR DE STAGE ===");
	ImGui::Separator();

	// Modo do stage
	if (ImGui::CollapsingHeader("Modo do Estagio", ImGuiTreeNodeFlags_DefaultOpen)) {
		int mode = (int)editorStageEditorConfig.mode;
		ImGui::RadioButton("Normal (blocos chegam a 0)", &mode, 0);
		ImGui::RadioButton("Boss (HP do boss chega a 0)", &mode, 1);
		editorStageEditorConfig.mode = (StageMode)mode;
	}

	// Background
	if (ImGui::CollapsingHeader("Background")) {
		ImGui::RadioButton("Cor solida##bg", (int*)&editorStageEditorConfig.useTextureBg, 0);
		ImGui::SameLine();
		ImGui::RadioButton("Textura##bg", (int*)&editorStageEditorConfig.useTextureBg, 1);
		if (!editorStageEditorConfig.useTextureBg) {
			ImGui::ColorEdit4("Cor de fundo", &editorStageEditorConfig.bgColorR);
		}
		else {
			if (TextureButton("Textura de Fundo", editorStageEditorConfig.bgTexturePath, 256, "spr_stage_bg"))
				LoadSRV(editorStageEditorConfig.bgTexturePath, &stageBgTexture);
		}
	}

	// Posicao inicial da bola
	if (ImGui::CollapsingHeader("Spawn da Bola", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::TextDisabled("Clique no viewport para posicionar a bola");
		ImGui::SliderFloat("X##ball_spawn", &editorStageConfig.ballStartX, -1.0f, 1.0f);
		ImGui::SliderFloat("Y##ball_spawn", &editorStageConfig.ballStartY, -1.0f, 1.0f);
		ImGui::SliderFloat("Vel X##ball", &editorStageConfig.ballStartVelX, -0.05f, 0.05f, "%.5f");
		ImGui::SliderFloat("Vel Y##ball", &editorStageConfig.ballStartVelY, -0.05f, 0.05f, "%.5f");
		// Aplica posicao imediatamente no preview
		ballX = editorStageConfig.ballStartX;
		ballY = editorStageConfig.ballStartY;
	}

	// Lista de objetos colocados
	if (ImGui::CollapsingHeader("Objetos no Estagio")) {
		// ATENCAO: a ordem deve corresponder exatamente ao enum PlacedObjectType
		// (Globals.h). Tamanho minimo == numero de valores do enum + 1, sob pena de
		// out-of-bounds read em typeNames[o.type] linhas abaixo (causou crash
		// 0xC0000005 em ucrtbased.dll quando PLACED_PORTAL=4 foi adicionado).
		const char* typeNames[] = { "Inimigo", "Obstaculo", "Boss", "Spawn Bola", "Portal" };

		// Botoes de adicionar
		if (ImGui::Button("+ Inimigo (JSON)")) {
			char p[MAX_PATH] = {};
			if (OpenJsonLoadDialog(p, MAX_PATH, "objects\\BLOCK")) {
				PlacedObject po = {}; po.type = PLACED_BLOCK; po.x = 0.0f; po.y = 0.5f;
				strncpy_s(po.configFile, 256, p, 255);
				// Extrai nome do arquivo como displayName
				const char* slash = strrchr(p, '\\');
				const char* name = slash ? slash + 1 : p;
				strncpy_s(po.displayName, 64, name, 63);
				stageObjects.push_back(po);
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("+ Obstaculo (JSON)")) {
			char p[MAX_PATH] = {};
			if (OpenJsonLoadDialog(p, MAX_PATH, "objects\\OBSTACLE")) {
				PlacedObject po = {}; po.type = PLACED_OBSTACLE; po.x = 0.0f; po.y = 0.0f;
				strncpy_s(po.configFile, 256, p, 255);
				const char* slash = strrchr(p, '\\'); const char* name = slash ? slash + 1 : p;
				strncpy_s(po.displayName, 64, name, 63);
				stageObjects.push_back(po);
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("+ Boss (JSON)")) {
			char p[MAX_PATH] = {};
			if (OpenJsonLoadDialog(p, MAX_PATH, "objects\\BOSS")) {
				PlacedObject po = {}; po.type = PLACED_BOSS; po.x = 0.0f; po.y = 0.7f;
				strncpy_s(po.configFile, 256, p, 255);
				const char* slash = strrchr(p, '\\'); const char* name = slash ? slash + 1 : p;
				strncpy_s(po.displayName, 64, name, 63);
				stageObjects.push_back(po);
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("+ Portal (JSON)")) {
			char p[MAX_PATH] = {};
			if (OpenJsonLoadDialog(p, MAX_PATH, "objects\\PORTAL")) {
				PlacedObject po = {}; po.type = PLACED_PORTAL; po.x = 0.0f; po.y = 0.0f;
				strncpy_s(po.configFile, 256, p, 255);
				const char* slash = strrchr(p, '\\'); const char* name = slash ? slash + 1 : p;
				strncpy_s(po.displayName, 64, name, 63);
				stageObjects.push_back(po);
			}
		}

		ImGui::Separator();
		ImGui::TextDisabled("Arraste X/Y para reposicionar. Del = remover.");
		ImGui::Separator();

		const int kTypeCount = (int)(sizeof(typeNames) / sizeof(typeNames[0]));
		// Filtro de tipo (facilita encontrar entidades em estagios cheios).
		static int s_filterType = -1; // -1 = todos
		ImGui::SetNextItemWidth(140);
		const char* filterItems[] = { "Todos", "Inimigo", "Obstaculo", "Boss", "Spawn Bola", "Portal" };
		int filterIdx = s_filterType + 1;
		if (ImGui::Combo("Filtro##stageobjs", &filterIdx, filterItems, IM_ARRAYSIZE(filterItems)))
			s_filterType = filterIdx - 1;
		ImGui::TextDisabled("Total: %d objetos", (int)stageObjects.size());

		// Lista scrollavel com altura limitada: a aba Stage tem muitos
		// controles globais (cores, spawn, save/load); manter a lista
		// confinada evita empurrar os botoes de save para fora da tela
		// quando o estagio tem dezenas de entidades.
		ImGui::BeginChild("##objlist", ImVec2(0, 240), true);
		for (int i = 0; i < (int)stageObjects.size(); i++) {
			auto& o = stageObjects[i];
			if (s_filterType >= 0 && (int)o.type != s_filterType) continue;
			ImGui::PushID(i);
			// Clamp defensivo do indice: evita out-of-bounds caso o enum
			// PlacedObjectType cresca sem atualizacao desta tabela.
			int ti = (int)o.type;
			const char* tname = (ti >= 0 && ti < kTypeCount) ? typeNames[ti] : "?";
			ImGui::Text("[%s] %s", tname, o.displayName);
			ImGui::SameLine();
			ImGui::SetNextItemWidth(70);
			ImGui::DragFloat("X##ox", &o.x, 0.01f, -1.0f, 1.0f, "%.2f");
			ImGui::SameLine();
			ImGui::SetNextItemWidth(70);
			ImGui::DragFloat("Y##oy", &o.y, 0.01f, -1.0f, 1.0f, "%.2f");
			ImGui::SameLine();
			// Duplicar: clona o objeto reaproveitando o mesmo configFile,
			// evitando que o autor tenha que reimportar o JSON cada vez
			// que precisa de outra copia da mesma entidade no estagio.
			// O clone aparece deslocado em Y para nao ficar coberto pelo
			// original e ja' fica selecionado para arrasto imediato.
			if (ImGui::SmallButton("Dup##dup")) {
				PlacedObject copy = o;
				copy.y = o.y - 0.10f;       // empilha logo abaixo do original
				copy.selected = false;
				stageObjects.push_back(copy);
				ImGui::PopID();
				break;                       // a lista cresceu; reinicia no proximo frame
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("X##rem")) {
				stageObjects.erase(stageObjects.begin() + i); ImGui::PopID(); break;
			}
			ImGui::PopID();
		}
		ImGui::EndChild();
	}

	ImGui::Separator();
	{
		char sp[MAX_PATH] = {}, lp[MAX_PATH] = {}; bool ds, dl;
		if (JsonSaveLoadButtons("objects\\STAGE", sp, MAX_PATH, lp, MAX_PATH, &ds, &dl)) {
			if (ds) {
				SaveStageConfig(sp);
				// Exporta stage.txt no mesmo diretorio para o gameplay carregar
				char txtPath[MAX_PATH];
				strncpy_s(txtPath, sp, MAX_PATH - 1);
				// Troca extensao .json por .txt
				char* ext = strrchr(txtPath, '.');
				if (ext) strcpy_s(ext, MAX_PATH - (ext - txtPath), ".txt");
				else strncat_s(txtPath, MAX_PATH, ".txt", 4);
				ExportStageTxt(txtPath);
			}
			if (dl) LoadStageConfig(lp);
		}
	}
}

// ==========================================
// PAINEL -OBSTACULOS
// ==========================================

void RenderEditorObstacle()
{
	ImGui::Text("=== EDITOR DE OBSTACULO ===");
	ImGui::Separator();

	ImGui::InputText("Nome", editorObstacleConfig.name, sizeof(editorObstacleConfig.name));

	if (ImGui::CollapsingHeader("Dimensoes", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::SliderFloat("Largura##obs", &editorObstacleConfig.width, 0.01f, 1.0f);
		ImGui::SliderFloat("Altura##obs", &editorObstacleConfig.height, 0.01f, 0.5f);
	}

	if (ImGui::CollapsingHeader("Aparencia", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::RadioButton("Cor solida##obs_app", (int*)&editorObstacleConfig.useTexture, 0);
		ImGui::SameLine();
		ImGui::RadioButton("Sprite##obs_app", (int*)&editorObstacleConfig.useTexture, 1);
		if (!editorObstacleConfig.useTexture) {
			ImGui::ColorEdit4("Cor##obs", &editorObstacleConfig.colorR, ImGuiColorEditFlags_NoInputs);
		}
		else {
			if (TextureButton("Sprite", editorObstacleConfig.texturePath, 256, "spr_obs"))
				LoadSRV(editorObstacleConfig.texturePath, &editorObstacleTexture);
		}
	}

	// Info do diretorio atual
	{
		char cwd[MAX_PATH]; GetCurrentDirectoryA(MAX_PATH, cwd); ImGui::TextDisabled("Dir: %s", cwd);
	}

	ImGui::Separator();
	{
		char sp[MAX_PATH] = {}, lp[MAX_PATH] = {}; bool ds, dl;
		if (JsonSaveLoadButtons("objects\\OBSTACLE", sp, MAX_PATH, lp, MAX_PATH, &ds, &dl)) {
			if (ds) SaveObstacleConfig(sp);
			if (dl) {
				LoadObstacleConfig(lp);
				sprintf_s(editorObstacleWidthInput, sizeof(editorObstacleWidthInput), "%.4f", editorObstacleConfig.width);
				sprintf_s(editorObstacleHeightInput, sizeof(editorObstacleHeightInput), "%.4f", editorObstacleConfig.height);
			}
		}
	}
}

// ==========================================
// PAINEL -INIMIGOS (BLOCOS)
// ==========================================

void RenderEditorEnemy()
{
	ImGui::Text("=== EDITOR DE INIMIGOS ===");
	// Botao de demonstracao
	if (editorDemoActive) {
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
		if (ImGui::Button("[ DEMO ON  ] Parar", ImVec2(-1, 0))) editorDemoActive = false;
		ImGui::PopStyleColor();
	}
	else {
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
		if (ImGui::Button("[ DEMO OFF ] Demonstrar", ImVec2(-1, 0))) {
			editorDemoActive = true;
		}
		ImGui::PopStyleColor();
	}
	ImGui::Separator();

	ImGui::InputText("Nome##blk", editorBlockConfig.name, sizeof(editorBlockConfig.name));

	if (ImGui::CollapsingHeader("Dimensoes", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::SliderFloat("Largura##blk", &editorBlockConfig.width, 0.02f, 0.5f);
		ImGui::SliderFloat("Altura##blk", &editorBlockConfig.height, 0.01f, 0.3f);
	}

	if (ImGui::CollapsingHeader("Aparencia", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::RadioButton("Cor solida##blk_app", (int*)&editorBlockConfig.useTexture, 0);
		ImGui::SameLine();
		ImGui::RadioButton("Sprite##blk_app", (int*)&editorBlockConfig.useTexture, 1);
		if (!editorBlockConfig.useTexture) {
			ImGui::ColorEdit4("Cor##blk", &editorBlockConfig.colorR, ImGuiColorEditFlags_NoInputs);
		}
		else {
			if (TextureButton("Sprite", editorBlockConfig.texturePath, 256, "spr_blk"))
				LoadSRV(editorBlockConfig.texturePath, &editorBlockTexture);
		}
	}

	if (ImGui::CollapsingHeader("Propriedades", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::SliderInt("Max Hits##blk", &editorBlockConfig.maxHits, 1, 20);
		ImGui::Checkbox("Invulneravel (turret)", &editorBlockConfig.invulnerable);
		if (editorBlockConfig.invulnerable)
			ImGui::TextDisabled("Bola passa sem dano. So projeteis inimigos atiram.");
	}

	if (ImGui::CollapsingHeader("Padrao de Tiro")) {
		BulletPatternCombo("Padrao##blk", editorBlockConfig.bulletPattern);
		ImGui::SliderInt("Qtd. Projeteis##blk", &editorBlockConfig.bulletCount, 1, 20);
		ImGui::SliderFloat("Velocidade Projetil##blk", &editorBlockConfig.bulletSpeed, 0.001f, 0.03f, "%.4f");
		ImGui::SliderInt("Intervalo de Tiro (frames)", &editorBlockConfig.shootIntervalFrames, 0, 300);
		ImGui::TextDisabled("0 = somente atira ao ser atingido");
		ImGui::SliderInt("Espacamento da Rajada (frames)", &editorBlockConfig.burstStepFrames, 1, 30);
		ImGui::TextDisabled("Frames entre tiros consecutivos da rajada");
	}

	if (ImGui::CollapsingHeader("Movimentacao")) {
		const char* movNames[] = { "Nenhuma", "Vertical", "Horizontal", "Circular" };
		int mt = (int)editorBlockConfig.movType;
		ImGui::Combo("Tipo##mov", &mt, movNames, IM_ARRAYSIZE(movNames));
		editorBlockConfig.movType = (EnemyMovType)mt;

		if (editorBlockConfig.movType != MOV_NONE) {
			ImGui::SliderFloat("Velocidade##mov", &editorBlockConfig.movSpeed, 0.001f, 0.02f, "%.4f");
			if (editorBlockConfig.movType == MOV_VERTICAL || editorBlockConfig.movType == MOV_HORIZONTAL) {
				ImGui::SliderFloat("Amplitude (distancia max)##mov", &editorBlockConfig.movAmplitude, 0.05f, 1.0f);
			}
			if (editorBlockConfig.movType == MOV_CIRCULAR) {
				ImGui::SliderFloat("Raio##mov", &editorBlockConfig.movRadius, 0.05f, 0.8f);
			}
			ImGui::TextDisabled("Movimento e tiro coexistem (independentes).");
		}
	}

	if (ImGui::CollapsingHeader("Drops")) {
		DropTableEditor(editorBlockConfig.dropTable);
	}

	ImGui::Separator();
	{
		char sp[MAX_PATH] = {}, lp[MAX_PATH] = {}; bool ds, dl;
		if (JsonSaveLoadButtons("objects\\BLOCK", sp, MAX_PATH, lp, MAX_PATH, &ds, &dl)) {
			if (ds) SaveBlockConfig(sp);
			if (dl) LoadBlockConfig(lp);
		}
	}
}

// ==========================================
// PAINEL -BOSS
// ==========================================

static bool PickPosButton(const char* label, float* px, float* py)
{
	bool active = (s_bossPickTarget == BOSS_PICK_XY && s_bossPickX == px && s_bossPickY == py);
	if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.9f, 0.6f, 0.1f, 1.0f));
	bool clicked = ImGui::SmallButton(label);
	if (active) ImGui::PopStyleColor();
	if (clicked) {
		if (active) { s_bossPickTarget = BOSS_PICK_NONE; s_bossPickX = s_bossPickY = nullptr; }
		else { s_bossPickTarget = BOSS_PICK_XY; s_bossPickX = px; s_bossPickY = py; }
	}
	return clicked;
}

static void BossActionEditor(BossAction& a, int idx)
{
	ImGui::PushID(idx);
	const char* actNames[] = {
		"Mover ate ponto", "Teleportar", "Atirar (tempo)",
		"Atirar (pontos fixos)", "Avancar no jogador",
		"Esperar", "Spawnar minion", "Trocar sprite", "Invulneravel"
	};
	int t = (int)a.type;
	ImGui::SetNextItemWidth(200);
	if (ImGui::Combo("Tipo##act", &t, actNames, IM_ARRAYSIZE(actNames))) a.type = (BossActionType)t;

	switch (a.type) {
	case BOSS_ACT_MOVE_TO:
	case BOSS_ACT_TELEPORT:
		ImGui::DragFloat("Alvo X##act", &a.targetX, 0.01f, -1.0f, 1.0f);
		ImGui::DragFloat("Alvo Y##act", &a.targetY, 0.01f, -1.0f, 1.0f);
		PickPosButton("Clicar no viewport##act_tgt", &a.targetX, &a.targetY);
		if (a.type == BOSS_ACT_MOVE_TO) ImGui::SliderFloat("Velocidade##act", &a.speed, 0.001f, 0.05f);
		break;
	case BOSS_ACT_SHOOT_TIMED:
		BulletPatternCombo("Padrao##act", a.bulletPattern);
		ImGui::SliderInt("Qtd.##act", &a.bulletCount, 1, 24);
		ImGui::SliderFloat("Vel.##act_bs", &a.bulletSpeed, 0.001f, 0.03f);
		ImGui::SliderFloat("Duracao (s)##act", &a.duration, 0.5f, 10.0f);
		break;
	case BOSS_ACT_SHOOT_FIXED_PTS:
		BulletPatternCombo("Padrao##actfp", a.bulletPattern);
		ImGui::SliderInt("Qtd. pts##actfp", &a.fixedPointCount, 1, 8);
		for (int i = 0; i < a.fixedPointCount; i++) {
			char lx[16], ly[16]; sprintf_s(lx, "Pt%d X", i + 1); sprintf_s(ly, "Pt%d Y", i + 1);
			ImGui::DragFloat(lx, &a.fixedPtsX[i], 0.01f, -1.0f, 1.0f);
			ImGui::SameLine();
			ImGui::DragFloat(ly, &a.fixedPtsY[i], 0.01f, -1.0f, 1.0f);
			ImGui::SameLine();
			char pickLabel[32]; sprintf_s(pickLabel, "Pick##fp%d", i);
			PickPosButton(pickLabel, &a.fixedPtsX[i], &a.fixedPtsY[i]);
		}
		break;
	case BOSS_ACT_CHARGE_PLAYER:
		ImGui::SliderFloat("Velocidade##actcp", &a.speed, 0.005f, 0.1f);
		break;
	case BOSS_ACT_WAIT:
		ImGui::SliderFloat("Duracao (s)##actwait", &a.duration, 0.1f, 10.0f);
		break;
	case BOSS_ACT_SPAWN_MINION:
		ImGui::DragFloat("Spawn X##actsm", &a.spawnX, 0.01f, -1.0f, 1.0f);
		ImGui::DragFloat("Spawn Y##actsm", &a.spawnY, 0.01f, -1.0f, 1.0f);
		PickPosButton("Clicar no viewport##act_spawn", &a.spawnX, &a.spawnY);
		ImGui::SliderInt("Template IDX##actsm", &a.minionPatternIndex, -1, 7);
		ImGui::TextDisabled("-1 = template padrao do BossConfig");
		break;
	case BOSS_ACT_CHANGE_SPRITE:
		TextureButton("Novo sprite##actcs", a.spritePath, 256, "act_spr");
		break;
	case BOSS_ACT_INVINCIBLE:
		ImGui::Checkbox("Ativar invulnerabilidade##actinv", &a.invincibleOn);
		ImGui::SliderFloat("Duracao (s)##actinv", &a.duration, 0.1f, 10.0f);
		break;
	default: break;
	}
	ImGui::PopID();
}

static void BossScriptEditor(BossScript& script, const char* uid)
{
	ImGui::PushID(uid);
	ImGui::Text("Acoes (%d/%d)", script.actionCount, BOSS_SCRIPT_MAX_ACTIONS);
	ImGui::SliderInt("Loop a partir da acao##sc", &script.loopFromStep, 0, max(0, script.actionCount - 1));
	ImGui::TextDisabled("0 = reinicia do inicio; N = pula para a acao N");

	for (int i = 0; i < script.actionCount; i++) {
		char label[32]; sprintf_s(label, "Acao %d", i + 1);
		bool open = ImGui::TreeNode((void*)(intptr_t)i, "%s", label);
		ImGui::SameLine();
		char remId[32]; sprintf_s(remId, "X##remact%d", i);
		if (ImGui::SmallButton(remId)) {
			for (int j = i; j < script.actionCount - 1; j++) script.actions[j] = script.actions[j + 1];
			script.actionCount--;
			if (open) ImGui::TreePop();
			break;
		}
		if (open) {
			BossActionEditor(script.actions[i], i);
			ImGui::TreePop();
		}
	}
	if (script.actionCount < BOSS_SCRIPT_MAX_ACTIONS) {
		if (ImGui::Button("+ Adicionar Acao")) {
			script.actions[script.actionCount] = {};
			script.actionCount++;
		}
	}
	ImGui::PopID();
}

void RenderEditorBoss()
{
	ImGui::Text("=== EDITOR DE BOSS ===");
	ImGui::TextDisabled("Feche outras janelas para usar o stage inteiro");
	ImGui::Separator();
	// Demo toggle
	if (editorDemoActive) {
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
		if (ImGui::Button("[ DEMO ON  ] Parar##boss", ImVec2(-1, 0))) editorDemoActive = false;
		ImGui::PopStyleColor();
	}
	else {
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
		if (ImGui::Button("[ DEMO OFF ] Demonstrar##boss", ImVec2(-1, 0))) editorDemoActive = true;
		ImGui::PopStyleColor();
	}
	if (editorDemoActive) {
		ImGui::SliderFloat("HP Simulado%%##bss_hp", &editorDemoBossHPPct, 0.0f, 1.0f, "%.2f");
		ImGui::TextDisabled("Ajuste para simular troca de fases.");
	}
	ImGui::Separator();

	ImGui::InputText("Nome##boss", editorBossConfig.name, sizeof(editorBossConfig.name));
	if (TextureButton("Sprite", editorBossConfig.texturePath, 256, "spr_boss"))
		LoadSRV(editorBossConfig.texturePath, &editorBossTexture);
	ImGui::SliderInt("HP Maximo", &editorBossConfig.maxHP, 10, 1000);
	ImGui::SliderFloat("Largura##bss", &editorBossConfig.width, 0.05f, 0.6f);
	ImGui::SliderFloat("Altura##bss", &editorBossConfig.height, 0.05f, 0.6f);

	// Posicao inicial
	ImGui::TextDisabled("Posicao inicial (NDC):");
	ImGui::DragFloat("Start X##bss", &editorBossConfig.startX, 0.01f, -1.0f, 1.0f);
	ImGui::DragFloat("Start Y##bss", &editorBossConfig.startY, 0.01f, -1.0f, 1.0f);
	PickPosButton("Clicar no viewport##boss_start", &editorBossConfig.startX, &editorBossConfig.startY);
	if (s_bossPickTarget != BOSS_PICK_NONE) {
		ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), ">> Clique no viewport para definir posicao <<");
	}

	// Arquetipo
	const char* archNames[] = { "Scripted", "Estatico", "Estatico + Familiares", "Multi-Part" };
	int arch = (int)editorBossConfig.archetype;
	ImGui::Combo("Arquetipo", &arch, archNames, IM_ARRAYSIZE(archNames));
	editorBossConfig.archetype = (BossArchetype)arch;

	// Fases de HP com scripts
	if (ImGui::CollapsingHeader("Fases de HP + Scripts")) {
		ImGui::SliderInt("Qtd. Fases##bss", &editorBossConfig.hpPhaseCount, 1, BOSS_MAX_HP_PHASES);
		for (int i = 0; i < editorBossConfig.hpPhaseCount; i++) {
			char phLabel[64]; sprintf_s(phLabel, "Fase %d (HP <= %.0f%%)", i + 1, editorBossConfig.hpPhases[i].hpThresholdPct * 100.0f);
			if (ImGui::TreeNode((void*)(intptr_t)i, "%s", phLabel)) {
				ImGui::SliderFloat("Limiar HP%%##ph", &editorBossConfig.hpPhases[i].hpThresholdPct, 0.0f, 1.0f, "%.2f");
				char scriptUid[32]; sprintf_s(scriptUid, "phase_script_%d", i);
				BossScriptEditor(editorBossConfig.hpPhases[i].script, scriptUid);
				ImGui::TreePop();
			}
		}
	}

	// Familiares (se STATIC_FAMILIARS)
	if (editorBossConfig.archetype == BOSS_ARCH_STATIC_FAMILIARS) {
		if (ImGui::CollapsingHeader("Familiares")) {
			ImGui::SliderInt("Qtd. Familiares", &editorBossConfig.familiarCount, 1, BOSS_MAX_FAMILIARS);
			for (int i = 0; i < editorBossConfig.familiarCount; i++) {
				auto& fam = editorBossConfig.familiars[i];
				char flabel[32]; sprintf_s(flabel, "Familiar %d", i + 1);
				if (ImGui::TreeNode((void*)(intptr_t)(100 + i), "%s", flabel)) {
					ImGui::DragFloat("Offset X##fam", &fam.relOffsetX, 0.01f, -1.0f, 1.0f);
					ImGui::DragFloat("Offset Y##fam", &fam.relOffsetY, 0.01f, -1.0f, 1.0f);
					ImGui::SliderFloat("Raio Orbita##fam", &fam.orbitRadius, 0.0f, 0.5f);
					// orbitSpeed e' aplicado diretamente como radianos por frame
					// no gameplay (g_boss.familiarAngles[i] += fam.orbitSpeed),
					// nao como graus/s. A escala antiga (0-360) interpretava o
					// numero como se fosse degrees-per-second e produzia rotacao
					// completamente irrealista (60+ revolucoes por frame).
					ImGui::SliderFloat("Vel. Orbita (rad/frame)##fam", &fam.orbitSpeed, 0.0f, 0.20f, "%.3f");
					BulletPatternCombo("Padrao##fam", fam.bulletPattern);
					ImGui::SliderInt("Qtd.##fam", &fam.bulletCount, 1, 16);
					ImGui::SliderFloat("Vel.##fam_bs", &fam.bulletSpeed, 0.001f, 0.03f);
					ImGui::SliderFloat("Intervalo Tiro (s)##fam", &fam.shootIntervalSec, 0.1f, 10.0f);
					TextureButton("Sprite##fam", fam.texturePath, 256, "spr_fam");
					ImGui::TreePop();
				}
			}
		}
	}

	// Nos multi-part (se MULTIPART)
	if (editorBossConfig.archetype == BOSS_ARCH_MULTIPART) {
		if (ImGui::CollapsingHeader("Nos Multi-Part")) {
			ImGui::SliderInt("Qtd. Nos", &editorBossConfig.nodeCount, 1, BOSS_MAX_NODES);
			for (int i = 0; i < editorBossConfig.nodeCount; i++) {
				auto& node = editorBossConfig.nodes[i];
				char nlabel[32]; sprintf_s(nlabel, "No %d", i + 1);
				if (ImGui::TreeNode((void*)(intptr_t)(200 + i), "%s", nlabel)) {
					TextureButton("Sprite##node", node.texturePath, 256, "spr_node");
					ImGui::DragFloat("Start X##node", &node.startX, 0.01f, -1.0f, 1.0f);
					ImGui::DragFloat("Start Y##node", &node.startY, 0.01f, -1.0f, 1.0f);
					{ char pk[32]; sprintf_s(pk, "Pick##nd%d", i); PickPosButton(pk, &node.startX, &node.startY); }
					char nodeUid[32]; sprintf_s(nodeUid, "node_script_%d", i);
					if (ImGui::TreeNode("Script do No")) {
						BossScriptEditor(node.script, nodeUid);
						ImGui::TreePop();
					}
					ImGui::TreePop();
				}
			}
		}
	}

	ImGui::Separator();
	{
		char sp[MAX_PATH] = {}, lp[MAX_PATH] = {}; bool ds, dl;
		if (JsonSaveLoadButtons("objects\\BOSS", sp, MAX_PATH, lp, MAX_PATH, &ds, &dl)) {
			if (ds) SaveBossConfig(sp);
			if (dl) LoadBossConfig(lp);
		}
	}
}

// ==========================================
// PAINEL -MENU
// ==========================================

void RenderEditorMenu()
{
	ImGui::Text("=== EDITOR DE MENU ===");
	ImGui::Separator();

	if (ImGui::CollapsingHeader("Cores", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::ColorEdit4("Fundo##menu", &editorMenuConfig.bgColorR, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_PickerHueBar);
		ImGui::ColorEdit4("Botao Normal", &editorMenuConfig.buttonColorR, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_PickerHueBar);
		ImGui::ColorEdit4("Botao Selecionado", &editorMenuConfig.selectedColorR, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_PickerHueBar);
	}

	if (ImGui::CollapsingHeader("Icone de Selecao")) {
		if (TextureButton("Textura do Seletor", editorMenuConfig.selectorTexturePath, 256, "spr_sel"))
			LoadSRV(editorMenuConfig.selectorTexturePath, &menuSelectorTexture);
		ImGui::TextDisabled("Aparece dentro do botao ativo, lado esquerdo.");
	}

	if (ImGui::CollapsingHeader("Logo / Titulo", ImGuiTreeNodeFlags_DefaultOpen)) {
		if (TextureButton("Imagem da Logo", editorMenuConfig.titleTexturePath, 256, "spr_title"))
			LoadSRV(editorMenuConfig.titleTexturePath, &menuTitleTexture);
		ImGui::Separator();
		ImGui::SliderFloat("Centro X##logo", &editorMenuConfig.logoX, -1.0f, 1.0f);
		ImGui::SliderFloat("Centro Y##logo", &editorMenuConfig.logoY, -1.0f, 1.0f);
		ImGui::SliderFloat("Largura##logo", &editorMenuConfig.logoWidth, 0.05f, 1.5f);
		ImGui::SliderFloat("Altura##logo", &editorMenuConfig.logoHeight, 0.02f, 0.6f);
		if (ImGui::Button("Reset Logo")) {
			editorMenuConfig.logoX = 0.0f; editorMenuConfig.logoY = 0.75f; editorMenuConfig.logoWidth = 0.8f; editorMenuConfig.logoHeight = 0.15f;
		}
	}

	if (ImGui::CollapsingHeader("Fundo do Menu")) {
		if (TextureButton("Textura de Fundo", editorMenuConfig.bgTexturePath, 256, "spr_menubg"))
			LoadSRV(editorMenuConfig.bgTexturePath, &menuBgTexture);
	}

	if (ImGui::CollapsingHeader("Fonte do Texto", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::ColorEdit4("Cor (Normal)",      &editorMenuConfig.textColorR,
			ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_PickerHueBar);
		ImGui::ColorEdit4("Cor (Selecionado)", &editorMenuConfig.textSelectedColorR,
			ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_PickerHueBar);

		ImGui::SliderFloat("Tamanho (px)", &editorMenuConfig.textSize, 10.0f, 96.0f, "%.0f");

		ImGui::Separator();
		ImGui::TextDisabled("Familia (arquivo .ttf):");
		ImGui::PushItemWidth(-90);
		ImGui::InputText("##fontpath", editorMenuConfig.fontPath,
			sizeof(editorMenuConfig.fontPath));
		ImGui::PopItemWidth();
		ImGui::SameLine();
		if (ImGui::Button("Procurar...##fnt", ImVec2(80, 0))) {
			// Reaproveita o dialogo de textura, filtrando manualmente .ttf
			// (o OpenFileName trata extensoes pelo lpstrFilter; o filtro de
			// imagens existente aceita "All Files (*.*)" como fallback).
			char tmp[MAX_PATH] = {};
			OPENFILENAMEA ofn = {};
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = g_hWnd;
			ofn.lpstrFile = tmp;
			ofn.nMaxFile = MAX_PATH;
			ofn.lpstrFilter = "TrueType Font (*.ttf)\0*.ttf\0All Files\0*.*\0";
			ofn.nFilterIndex = 1;
			ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
			char winFonts[MAX_PATH] = {};
			if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_FONTS, NULL, 0, winFonts)))
				ofn.lpstrInitialDir = winFonts;
			if (GetOpenFileNameA(&ofn)) {
				strncpy_s(editorMenuConfig.fontPath, tmp,
					sizeof(editorMenuConfig.fontPath) - 1);
			}
		}
		ImGui::TextDisabled("Vazio = fonte padrao do ImGui.");
	}

	ImGui::Separator();
	{
		char sp[MAX_PATH] = {}, lp[MAX_PATH] = {}; bool ds, dl;
		if (JsonSaveLoadButtons("objects\\MENU", sp, MAX_PATH, lp, MAX_PATH, &ds, &dl)) {
			if (ds) SaveMenuConfig(sp);
			if (dl) LoadMenuConfig(lp);
		}
	}
}

// ==========================================
// SAVE / LOAD — PORTAL
// ==========================================

bool SavePortalConfig(const char* fullPath)
{
	std::ofstream f(fullPath); if (!f.is_open()) return false;
	f << "{\n";
	f << "  \"texturePath\": \"" << editorPortalConfig.spritePath << "\",\n";
	f << "  \"width\": "  << editorPortalConfig.width  << ",\n";
	f << "  \"height\": " << editorPortalConfig.height << "\n";
	f << "}\n";
	f.close();
	return true;
}

bool LoadPortalConfig(const char* fullPath)
{
	std::ifstream f(fullPath); if (!f.is_open()) return false;
	std::string line;
	while (std::getline(f, line)) {
		if (line.find("\"width\"")  != std::string::npos) sscanf_s(line.c_str(), " \"width\": %f,",  &editorPortalConfig.width);
		if (line.find("\"height\"") != std::string::npos) sscanf_s(line.c_str(), " \"height\": %f,", &editorPortalConfig.height);
		if (line.find("\"texturePath\"") != std::string::npos) {
			size_t s = line.find(": \"") + 3, e = line.rfind("\"");
			if (s < e) {
				std::string v = line.substr(s, e - s);
				strcpy_s(editorPortalConfig.spritePath, 256, v.c_str());
			}
		}
	}
	f.close();
	// Recarrega o SRV de preview com o sprite recem carregado (se houver).
	LoadSRV(editorPortalConfig.spritePath, &editorPortalTexture);
	return true;
}

// ==========================================
// PAINEL — PORTAL
// Edicao do template do portal (sprite + dimensoes) e persistencia em JSON.
// A insercao de portais no estagio e' feita na aba "Stage", por meio do
// botao "+ Portal (JSON)", que aceita um arquivo de configuracao previamente
// salvo aqui. Esta separacao centraliza a montagem do estagio na aba Stage,
// em paralelo as demais entidades (Inimigo, Obstaculo, Boss).
// ==========================================
void RenderEditorPortal()
{
	ImGui::Text("=== EDITOR DE PORTAL ===");
	ImGui::Separator();

	// Ao trocar o sprite, recarrega o SRV imediatamente para que a area
	// de preview a' direita reflita a escolha.
	if (TextureButton("Sprite", editorPortalConfig.spritePath, 256, "spr_portal"))
		LoadSRV(editorPortalConfig.spritePath, &editorPortalTexture);

	ImGui::SliderFloat("Largura##portal", &editorPortalConfig.width,  0.02f, 0.50f);
	ImGui::SliderFloat("Altura##portal",  &editorPortalConfig.height, 0.02f, 0.80f);

	ImGui::Separator();
	{
		char sp[MAX_PATH] = {}, lp[MAX_PATH] = {}; bool ds, dl;
		if (JsonSaveLoadButtons("objects\\PORTAL", sp, MAX_PATH, lp, MAX_PATH, &ds, &dl)) {
			if (ds) SavePortalConfig(sp);
			if (dl) LoadPortalConfig(lp);
		}
	}
}

// ==========================================
// RENDER DO EDITOR (tabs + viewport)
// ==========================================

// Helper: abre uma janela do editor, dispara mudanca de EditorMode quando
// ganha foco (preview-tracking) e desativa o demo na troca de painel ativo.
// Retorna true se o conteudo da janela deve ser desenhado (false = janela
// colapsada). O caller deve SEMPRE chamar ImGui::End() em seguida — esta e'
// a convencao Begin/End do ImGui.
// `visiblePtr` recebe o flag de visibilidade (NULL = sem botao X).
static bool BeginEditorWindow(const char* title, bool* visiblePtr,
                              EditorMode modeOnFocus,
                              const ImVec2& initialPos,
                              const ImVec2& initialSize)
{
	ImGui::SetNextWindowPos(initialPos, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(initialSize, ImGuiCond_FirstUseEver);
	const bool open = ImGui::Begin(title, visiblePtr);
	// Preview-tracking: a janela focada controla o que o viewport mostra.
	// Trocar de painel ativo zera o demo (assim como o sistema antigo
	// de tabs fazia, evitando overlay de bala/movimento ao mudar de tela).
	if (open && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
		if (currentEditorMode != modeOnFocus) {
			currentEditorMode = modeOnFocus;
			editorDemoActive  = false;
		}
	}
	return open;
}

void RenderEditorUI_NoNewFrame()
{
	// ---------------------------------------------------------------
	// MAIN MENU BAR — ancorada no topo do viewport principal.
	// Cobre acoes globais (Salvar/Aplicar/Exportar/Sair) e toggles de
	// visibilidade de cada painel.
	// ---------------------------------------------------------------
	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("Arquivo")) {
			if (ImGui::MenuItem("Novo Projeto")) {
				gameProject = {};
				gameProjectPath[0] = '\0';
			}
			if (ImGui::MenuItem("Carregar Projeto...")) {
				char tmp[MAX_PATH] = {};
				if (OpenJsonLoadDialog(tmp, MAX_PATH, ""))
					LoadGameProject(tmp);
			}
			if (ImGui::MenuItem("Salvar Projeto...")) {
				char tmp[MAX_PATH] = {};
				if (OpenJsonSaveDialog(tmp, MAX_PATH, ""))
					SaveGameProject(tmp);
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Aplicar ao Jogo (recarregar configs)")) {
				if (gameProject.playerConfigPath[0]) LoadPlayerConfig(gameProject.playerConfigPath);
				if (gameProject.ballConfigPath[0])   LoadBallConfig(gameProject.ballConfigPath);
				if (gameProject.menuConfigPath[0])   LoadMenuConfig(gameProject.menuConfigPath);
				if (gameProject.stageCount > 0 && gameProject.stagePaths[0][0])
					LoadStageConfig(gameProject.stagePaths[0]);
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Sair")) {
				PostMessageW(g_hWnd, WM_CLOSE, 0, 0);
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Visualizar")) {
			ImGui::MenuItem("Projeto",   nullptr, &s_showProjeto);
			ImGui::MenuItem("Jogador",   nullptr, &s_showJogador);
			ImGui::MenuItem("Bola",      nullptr, &s_showBola);
			ImGui::MenuItem("Stage",     nullptr, &s_showStage);
			ImGui::MenuItem("Obstaculo", nullptr, &s_showObstaculo);
			ImGui::MenuItem("Inimigo",   nullptr, &s_showInimigo);
			ImGui::MenuItem("Boss",      nullptr, &s_showBoss);
			ImGui::MenuItem("Menu",      nullptr, &s_showMenu);
			ImGui::MenuItem("Portal",    nullptr, &s_showPortal);
			ImGui::Separator();
			if (ImGui::MenuItem("Mostrar tudo")) {
				s_showProjeto = s_showJogador = s_showBola = s_showStage =
				s_showObstaculo = s_showInimigo = s_showBoss = s_showMenu =
				s_showPortal = true;
			}
			if (ImGui::MenuItem("Ocultar tudo")) {
				s_showProjeto = s_showJogador = s_showBola = s_showStage =
				s_showObstaculo = s_showInimigo = s_showBoss = s_showMenu =
				s_showPortal = false;
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Ajuda")) {
			ImGui::MenuItem("Sobre", nullptr, &s_showAbout);
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}

	// ---------------------------------------------------------------
	// PAINEIS — cada um e' uma janela independente, livremente movivel
	// e redimensionavel. Posicao inicial cascateada do canto superior
	// esquerdo; ImGui persiste posicao/tamanho em imgui.ini.
	// ---------------------------------------------------------------
	const ImVec2 sz(380.0f, 520.0f);
	if (s_showProjeto) {
		if (BeginEditorWindow("Projeto", &s_showProjeto, EDITOR_MODE_PLAYER,
			ImVec2(20.0f, 40.0f), sz)) RenderEditorProject();
		ImGui::End();
	}
	if (s_showJogador) {
		if (BeginEditorWindow("Jogador", &s_showJogador, EDITOR_MODE_PLAYER,
			ImVec2(40.0f, 60.0f), sz)) RenderEditorPlayer();
		ImGui::End();
	}
	if (s_showBola) {
		if (BeginEditorWindow("Bola", &s_showBola, EDITOR_MODE_BALL,
			ImVec2(60.0f, 80.0f), sz)) RenderEditorBall();
		ImGui::End();
	}
	if (s_showStage) {
		if (BeginEditorWindow("Stage", &s_showStage, EDITOR_MODE_STAGE,
			ImVec2(80.0f, 100.0f), sz)) RenderEditorStage();
		ImGui::End();
	}
	if (s_showObstaculo) {
		if (BeginEditorWindow("Obstaculo", &s_showObstaculo, EDITOR_MODE_OBSTACLE,
			ImVec2(100.0f, 120.0f), sz)) RenderEditorObstacle();
		ImGui::End();
	}
	if (s_showInimigo) {
		if (BeginEditorWindow("Inimigo", &s_showInimigo, EDITOR_MODE_ENEMY,
			ImVec2(120.0f, 140.0f), sz)) RenderEditorEnemy();
		ImGui::End();
	}
	if (s_showBoss) {
		if (BeginEditorWindow("Boss", &s_showBoss, EDITOR_MODE_BOSS,
			ImVec2(140.0f, 160.0f), sz)) RenderEditorBoss();
		ImGui::End();
	}
	if (s_showMenu) {
		if (BeginEditorWindow("Menu", &s_showMenu, EDITOR_MODE_MENU,
			ImVec2(160.0f, 180.0f), sz)) RenderEditorMenu();
		ImGui::End();
	}
	if (s_showPortal) {
		if (BeginEditorWindow("Portal", &s_showPortal, EDITOR_MODE_PORTAL,
			ImVec2(180.0f, 200.0f), sz)) RenderEditorPortal();
		ImGui::End();
	}

	// Janela "Sobre" (toggleavel pelo menu Ajuda).
	if (s_showAbout) {
		ImGui::SetNextWindowSize(ImVec2(360.0f, 160.0f), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Sobre", &s_showAbout, ImGuiWindowFlags_NoCollapse)) {
			ImGui::Text("TorrouDX Editor");
			ImGui::Separator();
			ImGui::TextWrapped("Editor integrado do jogo. Cada painel pode ser "
				"movido, redimensionado ou fechado independentemente. Use o "
				"menu Visualizar para abrir os paineis que voce precisar.");
			ImGui::Separator();
			if (ImGui::Button("OK", ImVec2(120, 0))) s_showAbout = false;
		}
		ImGui::End();
	}

	ImGui::Render(); ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

// Versao legada com NewFrame proprio (mantida por compatibilidade)
void RenderEditorUI()
{
	ImGui_ImplDX11_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame();
	RenderEditorUI_NoNewFrame();
}

// ==========================================
// PAINEL -PROJETO
// ==========================================

// ==========================================
// EXPORTACAO STANDALONE
// ----------------------------------------------------------------------
// Empacota o jogo como executavel distribuivel: copia o binario, as DLLs
// do CRT, as texturas (Assets/) e todos os configs (objects/), e reescreve
// os caminhos absolutos dos JSONs para caminhos relativos a' raiz do
// pacote (formato "Assets\\..." em vez de "C:\\Users\\...\\TorrouEngine\\Assets\\...").
// ==========================================

namespace fs = std::filesystem;

// Mensagem persistente exibida apos a exportacao (sucesso ou erro).
static std::string s_exportStatus;

// Abre o dialogo nativo de selecao de pasta e devolve o caminho escolhido.
// IMPORTANTE: o motor inicializa COM em modo MTA (COINIT_MULTITHREADED em
// main.cpp), enquanto a Shell exige STA para o dialogo moderno (BIF_NEWDIALOGSTYLE).
// Por isso usa-se o dialogo classico, sem BIF_NEWDIALOGSTYLE — compativel com
// MTA, ainda que visualmente menos polido.
static bool PickFolderDialog(char* outPath, int maxLen)
{
	if (!outPath || maxLen <= 0) return false;
	outPath[0] = '\0';

	BROWSEINFOA bi = {};
	bi.hwndOwner = g_hWnd;
	bi.pszDisplayName = nullptr;
	bi.lpszTitle = "Selecione a pasta de destino do pacote standalone";
	bi.ulFlags = BIF_RETURNONLYFSDIRS; // sem BIF_NEWDIALOGSTYLE para evitar conflito MTA

	LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
	if (!pidl) return false; // usuario cancelou

	bool ok = (SHGetPathFromIDListA(pidl, outPath) != FALSE);

	// Liberacao do PIDL: em modo MTA evita-se SHGetMalloc (que faz QueryInterface
	// e pode interferir com o apartment atual). Usa-se CoTaskMemFree, que e' o
	// alocador compativel com itens retornados pela Shell.
	CoTaskMemFree(pidl);

	return ok && outPath[0] != '\0' && (int)strlen(outPath) < maxLen;
}

// Substitui todas as ocorrencias de `from` por `to` num arquivo de texto.
static bool ReplaceAllInTextFile(const fs::path& path,
                                 const std::string& from,
                                 const std::string& to)
{
	std::ifstream in(path, std::ios::binary);
	if (!in.is_open()) return false;
	std::stringstream ss; ss << in.rdbuf();
	in.close();
	std::string content = ss.str();
	if (from.empty()) return true;
	size_t pos = 0;
	while ((pos = content.find(from, pos)) != std::string::npos) {
		content.replace(pos, from.size(), to);
		pos += to.size();
	}
	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	if (!out.is_open()) return false;
	out << content;
	return true;
}

// Constroi o prefixo do basePath ja escapado para JSON (cada '\' -> "\\\\").
static std::string BuildJsonEscapedPrefix(const std::string& path)
{
	std::string out;
	for (char c : path) {
		if (c == '\\') out += "\\\\"; // dois bytes literais
		else out += c;
	}
	return out;
}

// ==========================================
// APLICACAO DE ICONE (.ico) AO EXECUTAVEL EXPORTADO
// ----------------------------------------------------------------------
// Le um arquivo .ico, monta o recurso RT_GROUP_ICON e os recursos RT_ICON
// correspondentes, e os grava no PE de destino via BeginUpdateResource.
// Recursos antigos com mesmos IDs sao sobrescritos; quaisquer icones
// com IDs mais altos permanecem orfaos, mas o icone exibido pelo Windows
// e' sempre o do grupo com o menor identificador numerico (id=1).
// ==========================================

#pragma pack(push, 1)
struct IconDirHeaderDisk {
	WORD reserved;
	WORD type;       // 1 = icone
	WORD count;
};
struct IconDirEntryDisk {
	BYTE  width;
	BYTE  height;
	BYTE  colorCount;
	BYTE  reserved;
	WORD  planes;
	WORD  bitCount;
	DWORD bytesInRes;
	DWORD imageOffset;
};
struct GrpIconDirEntryRes {
	BYTE  width;
	BYTE  height;
	BYTE  colorCount;
	BYTE  reserved;
	WORD  planes;
	WORD  bitCount;
	DWORD bytesInRes;
	WORD  id;          // id do recurso RT_ICON correspondente
};
#pragma pack(pop)

// Retorna mensagem vazia em caso de sucesso; mensagem nao-vazia descreve o
// erro encontrado (a exportacao prossegue mesmo sem icone).
static std::string ApplyIconToExe(const fs::path& exePath, const fs::path& icoPath)
{
	std::ifstream ico(icoPath, std::ios::binary);
	if (!ico.is_open()) return "icone: nao foi possivel abrir o arquivo .ico";
	std::vector<BYTE> data((std::istreambuf_iterator<char>(ico)),
	                       std::istreambuf_iterator<char>());
	ico.close();
	if (data.size() < sizeof(IconDirHeaderDisk))
		return "icone: arquivo invalido (header truncado)";

	IconDirHeaderDisk* hdr = (IconDirHeaderDisk*)data.data();
	if (hdr->type != 1) return "icone: tipo de arquivo nao e' .ico";
	const int count = hdr->count;
	if (count <= 0) return "icone: nenhuma imagem no .ico";
	const size_t needed = sizeof(IconDirHeaderDisk) + (size_t)count * sizeof(IconDirEntryDisk);
	if (data.size() < needed) return "icone: tabela de entradas truncada";

	IconDirEntryDisk* entries = (IconDirEntryDisk*)(data.data() + sizeof(IconDirHeaderDisk));

	// Valida offsets antes de tocar no PE.
	for (int i = 0; i < count; i++) {
		size_t end = (size_t)entries[i].imageOffset + (size_t)entries[i].bytesInRes;
		if (end > data.size())
			return "icone: imagem alem do fim do arquivo";
	}

	// Monta GRPICONDIR: mesmo header + entradas com wId em vez de offset.
	const size_t grpSize = sizeof(IconDirHeaderDisk) + (size_t)count * sizeof(GrpIconDirEntryRes);
	std::vector<BYTE> grp(grpSize);
	memcpy(grp.data(), hdr, sizeof(IconDirHeaderDisk));
	GrpIconDirEntryRes* g = (GrpIconDirEntryRes*)(grp.data() + sizeof(IconDirHeaderDisk));
	for (int i = 0; i < count; i++) {
		g[i].width      = entries[i].width;
		g[i].height     = entries[i].height;
		g[i].colorCount = entries[i].colorCount;
		g[i].reserved   = entries[i].reserved;
		g[i].planes     = entries[i].planes;
		g[i].bitCount   = entries[i].bitCount;
		g[i].bytesInRes = entries[i].bytesInRes;
		g[i].id         = (WORD)(i + 1);
	}

	HANDLE hUpd = BeginUpdateResourceW(exePath.wstring().c_str(), FALSE);
	if (!hUpd) return "icone: BeginUpdateResource falhou";

	WORD lang = MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL);
	bool ok = true;

	if (!UpdateResourceW(hUpd, RT_GROUP_ICON, MAKEINTRESOURCEW(1), lang,
	                     grp.data(), (DWORD)grp.size())) {
		ok = false;
	}
	for (int i = 0; i < count && ok; i++) {
		const BYTE* imgData = data.data() + entries[i].imageOffset;
		DWORD       imgSize = entries[i].bytesInRes;
		if (!UpdateResourceW(hUpd, RT_ICON, MAKEINTRESOURCEW(i + 1), lang,
		                     (LPVOID)imgData, imgSize)) {
			ok = false;
		}
	}

	// `fDiscard=TRUE` descarta as mudancas em caso de erro durante a sequencia.
	if (!EndUpdateResourceW(hUpd, ok ? FALSE : TRUE))
		return "icone: EndUpdateResource falhou";
	if (!ok) return "icone: UpdateResource falhou";
	return {}; // sucesso
}

// Sanitiza o nome do jogo para uso simultaneo como nome de pasta e de
// executavel: remove os caracteres reservados pelo Windows ('<>:"/\\|?*'),
// caracteres de controle, e apara espacos/pontos finais (que Windows trata
// como invalidos em nomes de arquivo). Devolve string vazia se nada
// restou — o chamador deve aplicar um default.
static std::string SanitizeGameName(const char* input)
{
	std::string out;
	for (const char* p = input; p && *p; p++) {
		unsigned char c = (unsigned char)*p;
		if (c < 32) continue;
		switch (c) {
		case '<': case '>': case ':': case '"':
		case '/': case '\\': case '|': case '?': case '*':
			continue;
		}
		out += (char)c;
	}
	while (!out.empty() && (out.back() == ' ' || out.back() == '.'))
		out.pop_back();
	return out;
}

// Funcao principal: empacota o jogo no diretorio destino. Cria uma
// subpasta com o nome do jogo dentro de `destFolder`, copia os arquivos
// para essa subpasta e renomeia o executavel para `<gameName>.exe`.
// `iconFile` e' opcional; quando preenchido com um caminho para um .ico
// valido, o icone e' embedded no PE do executavel copiado.
// Retorna mensagem de status (vazio em caso de cancelamento).
static std::string ExportStandalone(const std::string& destFolder,
                                    const std::string& gameNameInput,
                                    const std::string& iconFile)
{
	if (destFolder.empty()) return "";

	// Resolve o nome final do jogo. Sem entrada valida, cai em "MeuJogo"
	// para que a exportacao nunca grave arquivos soltos no destino
	// selecionado pelo usuario.
	std::string gameName = SanitizeGameName(gameNameInput.c_str());
	if (gameName.empty()) gameName = "MeuJogo";

	try {
		fs::path baseDst(destFolder);
		if (!fs::exists(baseDst)) fs::create_directories(baseDst);

		// Subpasta dentro do destino: dst = <destFolder>/<gameName>
		fs::path dst = baseDst / gameName;
		if (!fs::exists(dst)) fs::create_directories(dst);
		const fs::path dstAbs = fs::absolute(dst).lexically_normal();

		// 1) Binario atual (TG.exe) e diretorio que o contem.
		char exeBuf[MAX_PATH] = {};
		GetModuleFileNameA(nullptr, exeBuf, MAX_PATH);
		fs::path exePath(exeBuf);
		fs::path exeDir = exePath.parent_path();

		// Nome final do executavel exportado: <gameName>.exe
		const fs::path exeOutName = fs::path(gameName + ".exe");
		const fs::path exeOutPath = dst / exeOutName;

		// Guarda anti-recursao: o destino nao pode ser, nem estar contido em,
		// nem conter, o diretorio do exe ou o engineBase (Documents\TorrouEngine).
		// Sem isto, fs::copy recursive entraria em loop ao copiar Assets/objects.
		auto isPrefix = [](const fs::path& parent, const fs::path& child) {
			auto p = parent.lexically_normal();
			auto c = child.lexically_normal();
			auto mismatch = std::mismatch(p.begin(), p.end(), c.begin(), c.end());
			return mismatch.first == p.end();
		};
		const fs::path engineBaseAbs = fs::absolute(fs::path(GetEngineBasePath())).lexically_normal();
		const fs::path exeDirAbs     = fs::absolute(exeDir).lexically_normal();
		if (isPrefix(exeDirAbs, dstAbs) || isPrefix(dstAbs, exeDirAbs) ||
			isPrefix(engineBaseAbs, dstAbs) || isPrefix(dstAbs, engineBaseAbs)) {
			return "Falha: a pasta de destino nao pode coincidir, conter ou estar contida "
			       "na pasta do executavel ou em Documents\\TorrouEngine. "
			       "Escolha um diretorio independente (ex: C:\\Distribuicao\\MeuJogo).";
		}

		// 2) Copia o executavel ja' com o nome final do jogo.
		fs::copy_file(exePath, exeOutPath,
			fs::copy_options::overwrite_existing);

		// 3) DLLs do CRT (Debug). Em Release algumas dessas nem existem;
		//    a ausencia e' silenciosa para nao bloquear a exportacao.
		char sysDir[MAX_PATH] = {};
		GetSystemDirectoryA(sysDir, MAX_PATH);
		const char* dlls[] = {
			"vcruntime140d.dll", "vcruntime140_1d.dll",
			"msvcp140d.dll",     "ucrtbased.dll",
			"vcruntime140.dll",  "vcruntime140_1.dll",
			"msvcp140.dll",      "ucrtbase.dll"
		};
		int dllsCopiadas = 0;
		for (const char* d : dlls) {
			fs::path s = fs::path(sysDir) / d;
			if (fs::exists(s)) {
				fs::copy_file(s, dst / d, fs::copy_options::overwrite_existing);
				dllsCopiadas++;
			}
		}

		// 4) Replicar Assets/ e objects/ a partir de Documents\TorrouEngine.
		fs::path engineBase(GetEngineBasePath());
		const fs::path assetsSrc  = engineBase / "Assets";
		const fs::path objectsSrc = engineBase / "objects";
		if (fs::exists(assetsSrc)) {
			fs::copy(assetsSrc, dst / "Assets",
				fs::copy_options::recursive | fs::copy_options::overwrite_existing);
		}
		if (fs::exists(objectsSrc)) {
			fs::copy(objectsSrc, dst / "objects",
				fs::copy_options::recursive | fs::copy_options::overwrite_existing);
		}

		// 5) Copia o game_config.json ativo (na CWD do .exe atual).
		fs::path gameCfgSrc = exeDir / "game_config.json";
		if (!fs::exists(gameCfgSrc)) {
			// fallback: usa a variavel global gameProjectPath, que pode ser
			// um caminho absoluto definido pela ultima sessao de edicao.
			gameCfgSrc = fs::absolute(gameProjectPath);
		}
		if (fs::exists(gameCfgSrc)) {
			fs::copy_file(gameCfgSrc, dst / "game_config.json",
				fs::copy_options::overwrite_existing);
		}

		// 6) Gera config.json sinalizando modo standalone.
		{
			std::ofstream f((dst / "config.json").string(),
				std::ios::binary | std::ios::trunc);
			f << "{\n  \"standalone\": true\n}\n";
		}

		// 7) Reescreve paths absolutos em todos os .json copiados.
		// Substitui o prefixo do engineBase por nada — os paths viram
		// relativos a' raiz do pacote (ex: "Assets\\sprite.png").
		const std::string baseStr = engineBase.string();
		const std::string prefixEscaped = BuildJsonEscapedPrefix(baseStr) + "\\\\";
		const std::string prefixForward = baseStr + "/";
		// Tambem cobre o caso em que a barra final aparece como '\\' (1 byte
		// no JSON, raro mas possivel em saves manuais).
		const std::string prefixRaw = baseStr + "\\";

		int reescritos = 0;
		for (auto& entry : fs::recursive_directory_iterator(dst)) {
			if (!entry.is_regular_file()) continue;
			if (entry.path().extension() != ".json") continue;
			ReplaceAllInTextFile(entry.path(), prefixEscaped, "");
			ReplaceAllInTextFile(entry.path(), prefixForward, "");
			ReplaceAllInTextFile(entry.path(), prefixRaw, "");
			reescritos++;
		}

		// 8) Aplica o icone, se fornecido. Falhas aqui sao reportadas como
		// aviso, sem invalidar o resto da exportacao.
		std::string iconStatus = "(nao alterado)";
		if (!iconFile.empty()) {
			fs::path icoSrc(iconFile);
			if (!fs::exists(icoSrc)) {
				iconStatus = "FALHOU: arquivo .ico nao encontrado";
			}
			else {
				std::string err = ApplyIconToExe(exeOutPath, icoSrc);
				iconStatus = err.empty() ? "aplicado" : ("FALHOU: " + err);
			}
		}

		char msg[1024];
		sprintf_s(msg, sizeof(msg),
			"Exportacao concluida.\n"
			"  Jogo: %s\n"
			"  Pasta criada: %s\n"
			"  Executavel: %s\n"
			"  DLLs CRT copiadas: %d\n"
			"  JSONs reescritos: %d\n"
			"  Icone do executavel: %s\n"
			"Abra %s dentro da pasta para iniciar o jogo.",
			gameName.c_str(),
			dst.string().c_str(),
			exeOutName.string().c_str(),
			dllsCopiadas, reescritos, iconStatus.c_str(),
			exeOutName.string().c_str());
		return msg;
	}
	catch (const std::exception& ex) {
		std::string m = "Falha na exportacao: ";
		m += ex.what();
		return m;
	}
}

void RenderEditorProject()
{
	ImGui::Text("=== PROJETO DO JOGO ===");
	ImGui::Separator();

	ImGui::TextColored(ImVec4(0.7f, 0.9f, 0.7f, 1.f), "Arquivo: %s",
		gameProjectPath[0] ? gameProjectPath : "(nenhum)");

	if (ImGui::Button("Novo Projeto", ImVec2(-1, 0))) {
		gameProject = {};
		gameProjectPath[0] = '\0';
	}

	ImGui::Separator();
	if (ImGui::CollapsingHeader("Configs Ativas", ImGuiTreeNodeFlags_DefaultOpen)) {

		// Helper para selecionar arquivo e atualizar path do projeto
		auto ProjectFileButton = [&](const char* label, char* path, const char* dir) {
			ImGui::Text("%s:", label);
			ImGui::SameLine();
			ImGui::TextDisabled("%s", path[0] ? path : "(nenhum)");
			char btnId[64]; sprintf_s(btnId, "Selecionar##%s", label);
			if (ImGui::Button(btnId, ImVec2(130, 0))) {
				char tmp[MAX_PATH] = {};
				if (OpenJsonLoadDialog(tmp, MAX_PATH, dir))
					strncpy_s(path, MAX_PATH, tmp, MAX_PATH - 1);
			}
			ImGui::SameLine();
			char clrId[64]; sprintf_s(clrId, "X##clr%s", label);
			if (ImGui::SmallButton(clrId)) path[0] = '\0';
			};

		ProjectFileButton("Jogador", gameProject.playerConfigPath, "objects\\PLAYER");
		ProjectFileButton("Bola", gameProject.ballConfigPath, "objects\\BALL");
		ProjectFileButton("Menu", gameProject.menuConfigPath, "objects\\MENU");

		ImGui::Separator();
		ImGui::Text("Stages (ordem de progressao):");
		ImGui::TextDisabled("O stage 0 e' carregado ao iniciar; os demais avancam em sequencia.");

		for (int i = 0; i < gameProject.stageCount; i++) {
			ImGui::PushID(i);
			char stLabel[32]; sprintf_s(stLabel, "Stage %d", i);
			ImGui::Text("%s:", stLabel);
			ImGui::SameLine();
			ImGui::TextDisabled("%s", gameProject.stagePaths[i][0] ? gameProject.stagePaths[i] : "(vazio)");
			if (ImGui::Button("Selecionar##st", ImVec2(100, 0))) {
				char tmp[MAX_PATH] = {};
				if (OpenJsonLoadDialog(tmp, MAX_PATH, "objects\\STAGE"))
					strncpy_s(gameProject.stagePaths[i], MAX_PATH, tmp, MAX_PATH - 1);
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("X##strem") && gameProject.stageCount > 0) {
				for (int j = i; j < gameProject.stageCount - 1; j++)
					memcpy(gameProject.stagePaths[j], gameProject.stagePaths[j + 1], MAX_PATH);
				gameProject.stagePaths[gameProject.stageCount - 1][0] = '\0';
				gameProject.stageCount--;
				ImGui::PopID(); break;
			}
			ImGui::PopID();
		}
		if (gameProject.stageCount < PROJECT_MAX_STAGES) {
			if (ImGui::Button("+ Adicionar Stage", ImVec2(-1, 0))) {
				char tmp[MAX_PATH] = {};
				if (OpenJsonLoadDialog(tmp, MAX_PATH, "objects\\STAGE")) {
					strncpy_s(gameProject.stagePaths[gameProject.stageCount], MAX_PATH, tmp, MAX_PATH - 1);
					gameProject.stageCount++;
				}
			}
		}
	}

	ImGui::Separator();

	// Aplicar projeto ao jogo (recarrega tudo na memoria)
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.9f, 1.0f));
	if (ImGui::Button("Aplicar ao Jogo (recarregar configs)", ImVec2(-1, 0))) {
		if (gameProject.playerConfigPath[0]) LoadPlayerConfig(gameProject.playerConfigPath);
		if (gameProject.ballConfigPath[0])   LoadBallConfig(gameProject.ballConfigPath);
		if (gameProject.menuConfigPath[0])   LoadMenuConfig(gameProject.menuConfigPath);
		if (gameProject.stageCount > 0 && gameProject.stagePaths[0][0])
			LoadStageConfig(gameProject.stagePaths[0]);
	}
	ImGui::PopStyleColor();

	ImGui::Separator();

	// Save / Load do game_config.json
	if (ImGui::Button("Salvar Projeto", ImVec2(130, 0))) {
		char tmp[MAX_PATH] = {};
		if (OpenJsonSaveDialog(tmp, MAX_PATH, "")) {
			SaveGameProject(tmp);
			ImGui::OpenPopup("OK_Proj");
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Carregar Projeto", ImVec2(130, 0))) {
		char tmp[MAX_PATH] = {};
		if (OpenJsonLoadDialog(tmp, MAX_PATH, "")) {
			LoadGameProject(tmp);
		}
	}
	if (ImGui::BeginPopupModal("OK_Proj", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("Projeto salvo em:\n%s", gameProjectPath);
		if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}

	// ==========================================
	// EXPORTACAO STANDALONE
	// ==========================================
	ImGui::Separator();
	ImGui::TextDisabled("Distribuicao");

	// Buffer persistente para o campo de path. Permite ao usuario digitar
	// manualmente ou aproveitar o ultimo destino escolhido entre cliques.
	static char s_exportDest[MAX_PATH] = {};
	// Nome do jogo: usado como nome da subpasta criada dentro do destino
	// e como nome final do executavel exportado (<gameName>.exe).
	static char s_exportName[128] = "MeuJogo";
	// Caminho opcional para o .ico que sera embarcado no executavel.
	static char s_exportIcon[MAX_PATH] = {};

	ImGui::TextDisabled("Nome do jogo:");
	ImGui::PushItemWidth(-90);
	ImGui::InputText("##expname", s_exportName, sizeof(s_exportName));
	ImGui::PopItemWidth();
	// Pre-visualiza o nome sanitizado para que o usuario saiba qual
	// nome de pasta/executavel sera efetivamente criado.
	{
		std::string preview = SanitizeGameName(s_exportName);
		if (preview.empty()) preview = "MeuJogo";
		ImGui::TextDisabled("-> pasta/.exe: %s / %s.exe",
			preview.c_str(), preview.c_str());
	}

	ImGui::TextDisabled("Pasta de destino:");
	ImGui::PushItemWidth(-90);
	ImGui::InputText("##expdest", s_exportDest, sizeof(s_exportDest));
	ImGui::PopItemWidth();
	ImGui::SameLine();
	if (ImGui::Button("Procurar...", ImVec2(80, 0))) {
		char picked[MAX_PATH] = {};
		if (PickFolderDialog(picked, MAX_PATH)) {
			strncpy_s(s_exportDest, picked, sizeof(s_exportDest) - 1);
		}
	}

	// Selecao de icone do executavel — opcional. Se vazio, o icone do
	// binario original e' preservado.
	ImGui::TextDisabled("Icone do executavel (.ico, opcional):");
	ImGui::PushItemWidth(-130);
	ImGui::InputText("##expicon", s_exportIcon, sizeof(s_exportIcon));
	ImGui::PopItemWidth();
	ImGui::SameLine();
	if (ImGui::Button("Procurar...##ico", ImVec2(80, 0))) {
		char tmp[MAX_PATH] = {};
		OPENFILENAMEA ofn = {};
		ofn.lStructSize  = sizeof(ofn);
		ofn.hwndOwner    = g_hWnd;
		ofn.lpstrFile    = tmp;
		ofn.nMaxFile     = MAX_PATH;
		ofn.lpstrFilter  = "Icone (*.ico)\0*.ico\0Todos arquivos\0*.*\0";
		ofn.nFilterIndex = 1;
		ofn.Flags        = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
		if (GetOpenFileNameA(&ofn))
			strncpy_s(s_exportIcon, tmp, sizeof(s_exportIcon) - 1);
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("X##icoclr")) s_exportIcon[0] = '\0';

	if (s_exportIcon[0]) {
		std::ifstream t(s_exportIcon); bool ok = t.good(); t.close();
		ImGui::TextColored(ok ? ImVec4(0.4f, 0.9f, 0.4f, 1.0f)
		                      : ImVec4(0.9f, 0.4f, 0.4f, 1.0f),
		                   ok ? "Icone OK" : "Arquivo nao encontrado");
	}

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.55f, 0.25f, 1.0f));
	const bool canExport = (s_exportDest[0] != '\0');
	if (!canExport) ImGui::BeginDisabled();
	if (ImGui::Button("Exportar para Standalone", ImVec2(-1, 0))) {
		s_exportStatus = ExportStandalone(s_exportDest, s_exportName, s_exportIcon);
		if (!s_exportStatus.empty()) ImGui::OpenPopup("Resultado_Export");
	}
	if (!canExport) ImGui::EndDisabled();
	ImGui::PopStyleColor();

	if (!canExport) {
		ImGui::TextDisabled("Informe ou selecione uma pasta para habilitar.");
	}

	if (ImGui::BeginPopupModal("Resultado_Export", nullptr,
		ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::TextUnformatted(s_exportStatus.c_str());
		ImGui::Separator();
		if (ImGui::Button("OK", ImVec2(120, 0))) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}
}