#include <windows.h>
#include "Globals.h"
#include "Game.h"
#include "Render.h"
#include "Editor.h"
#include "Level.h"
#include "timeapi.h"
#pragma comment (lib, "winmm.lib")
#include "./imgui/imgui.h"
#include "./imgui/imgui_impl_win32.h"
#include "./imgui/imgui_impl_dx11.h"

#include <fstream>
#include <string>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam)) return true;
	switch (message) {
	case WM_SIZE: {
		if (wParam != SIZE_MINIMIZED) {
			int newW = LOWORD(lParam);
			int newH = HIWORD(lParam);
			if (newW > 0 && newH > 0) {
				g_currentWidth  = newW;
				g_currentHeight = newH;
				if (device) ResizeViewport(newW, newH);
			}
		}
		return 0;
	}
	case WM_DESTROY: PostQuitMessage(0); return 0;
	}
	return DefWindowProcW(hWnd, message, wParam, lParam);
}

// ==========================================
// CONFIG.JSON loader (modo standalone)
// Formato minimo aceito:
//   { "standalone": true }
// Quando true, desabilita-se o editor (tecla E inerte, sem overlay ImGui).
// Procura o arquivo no diretorio de trabalho corrente (ao lado do executavel).
// Falha silenciosa preserva o comportamento default (g_isEditorEnabled = true).
// ==========================================
static void LoadAppConfig(const char* path)
{
	std::ifstream f(path);
	if (!f.is_open()) return;
	std::string line;
	while (std::getline(f, line)) {
		if (line.find("\"standalone\"") != std::string::npos) {
			if (line.find("true") != std::string::npos) {
				g_isEditorEnabled = false;
			}
			else if (line.find("false") != std::string::npos) {
				g_isEditorEnabled = true;
			}
		}
	}
	f.close();
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
	CoInitializeEx(nullptr, COINIT_MULTITHREADED);

	// 1) Le config.json antes de qualquer subsistema, para que o modo standalone
	//    possa influenciar a inicializacao (ex: nao instanciar ImGui em release puro).
	LoadAppConfig("config.json");

	WNDCLASSEXW wc = {};
	wc.cbSize = sizeof(WNDCLASSEXW);
	wc.style = CS_CLASSDC;
	wc.lpfnWndProc = WindowProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = GetModuleHandleW(NULL);
	wc.hIcon = NULL;
	wc.hCursor = NULL;
	wc.hbrBackground = NULL;
	wc.lpszMenuName = NULL;
	wc.lpszClassName = L"TorrouDX";
	wc.hIconSm = NULL;
	RegisterClassExW(&wc);

	const wchar_t* windowTitle = g_isEditorEnabled
		? L"Torrou Engine — Bullet Hell Creator"
		: L"TorrouDX";

	HWND hWnd = CreateWindowExW(
		0,
		L"TorrouDX",
		windowTitle,
		WS_OVERLAPPEDWINDOW,
		100, 100, 800, 600,
		NULL, NULL, wc.hInstance, NULL);
	ShowWindow(hWnd, nCmdShow);
	g_hWnd = hWnd;

	if (!InitD3D(hWnd)) return 0;

	IMGUI_CHECKVERSION(); ImGui::CreateContext(); ImGuiIO& io = ImGui::GetIO(); (void)io; io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	ImGui::StyleColorsDark(); ImGui_ImplWin32_Init(g_hWnd); ImGui_ImplDX11_Init(device, deviceContext);

	// Auto-load do projeto ao iniciar (falha silenciosamente se nao existir).
	// Procura-se primeiro o caminho relativo padrao (./game_config.json),
	// facilitando a distribuicao da pasta do jogo como pacote autocontido.
	LoadGameProject(gameProjectPath);

	// No modo standalone, garante-se entrada direta no menu principal,
	// suprimindo qualquer estado residual (ex: ultima sessao do editor).
	if (!g_isEditorEnabled) {
		currentState = GameState::STATE_START_MENU;
	}

	//Controle de FPS
	timeBeginPeriod(1);
	LARGE_INTEGER frequency;
	LARGE_INTEGER lastTime;
	QueryPerformanceFrequency(&frequency);
	QueryPerformanceCounter(&lastTime);

	const double targetFPS = 60.0;
	const double targetSecondsPerFrame = 1.0 / targetFPS;

	MSG msg = {};
	while (msg.message != WM_QUIT) {
		if (PeekMessageW(&msg, NULL, 0U, 0U, PM_REMOVE)) {
			TranslateMessage(&msg); DispatchMessageW(&msg);
		}
		else {
			//calcula tempo passado
			LARGE_INTEGER currentTime;
			QueryPerformanceCounter(&currentTime);
			double elapsedSeconds = (double)(currentTime.QuadPart - lastTime.QuadPart) / frequency.QuadPart;

			//continua apenas se 1/60 de um segundo passaram-se.
			if (elapsedSeconds >= targetSecondsPerFrame) {
				lastTime = currentTime;

				// Toggle do editor pela tecla E — somente quando habilitado.
				if (g_isEditorEnabled) {
					static bool eWasPressed = false;
					bool ePressed = (GetAsyncKeyState('E') & 0x8000) != 0;
					if (ePressed && !eWasPressed) {
						if (currentState == GameState::STATE_EDITOR)
							currentState = GameState::STATE_START_MENU;
						else
							currentState = GameState::STATE_EDITOR;
					}
					eWasPressed = ePressed;
				}

				switch (currentState) {
				case GameState::STATE_START_MENU:
					UpdateMenu(); RenderMenu();
					DrawMenuText(g_hWnd);
					break;
				case GameState::STATE_DIFFICULTY_SELECT: UpdateDiffSelect(); RenderDiffSelect(); break;
				case GameState::STATE_OPTIONS:
					UpdateOptions(); RenderOptions();
					break;
				case GameState::STATE_GAMEPLAY:
					UpdateGameplay(); RenderGameplay();
					DrawScore(g_hWnd, score); DrawBlocksRemaining(g_hWnd, blocksRemaining); DrawLives(g_hWnd, life); DrawStage(g_hWnd, stage);
					break;
				case GameState::STATE_EDITOR:
					if (g_isEditorEnabled) {
						UpdateEditor(); RenderEditor();
					}
					else {
						// Em standalone, qualquer tentativa de entrar no editor cai no menu.
						currentState = GameState::STATE_START_MENU;
					}
					break;
				}

				// Overlay ImGui — totalmente suprimido em standalone.
				if (g_isEditorEnabled) {
					if (currentState == GameState::STATE_EDITOR) {
						RenderEditorUI();
					}
					else if (currentState == GameState::STATE_OPTIONS) {
						RenderOptionsUI();
					}
					else {
						RenderDebugUI();
					}
				}
				else if (currentState == GameState::STATE_OPTIONS) {
					// A tela de opcoes precisa do ImGui mesmo em standalone
					// (combo de resolucao + botoes), por ser parte do menu do jogador.
					RenderOptionsUI();
				}

				swapChain->Present(0, 0);
			}
			else {
				if (targetSecondsPerFrame - elapsedSeconds > 0.002) {
					Sleep(1);
				}
			}
		}
	}
	timeEndPeriod(1);
	ImGui_ImplDX11_Shutdown(); ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext();
	CleanD3D(); UnregisterClassW(L"TorrouDX", wc.hInstance);
	CoUninitialize();
	return 0;
}
