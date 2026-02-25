#include <windows.h>
#include "Globals.h"
#include "Game.h"
#include "Render.h"
#include "Editor.h"

#include "./imgui/imgui.h"
#include "./imgui/imgui_impl_win32.h"
#include "./imgui/imgui_impl_dx11.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam)) return true;
    switch (message) { case WM_DESTROY: PostQuitMessage(0); return 0; }
                                      return DefWindowProc(hWnd, message, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WindowProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, L"TorrouDX", NULL };
    RegisterClassEx(&wc);
    HWND hWnd = CreateWindow(L"TorrouDX", L"Torrou Engine - Bullet Hell Creator", WS_OVERLAPPEDWINDOW, 100, 100, 800, 600, NULL, NULL, wc.hInstance, NULL);
    ShowWindow(hWnd, nCmdShow);
    g_hWnd = hWnd;

    if (!InitD3D(hWnd)) return 0;

    IMGUI_CHECKVERSION(); ImGui::CreateContext(); ImGuiIO& io = ImGui::GetIO(); (void)io; io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark(); ImGui_ImplWin32_Init(g_hWnd); ImGui_ImplDX11_Init(device, deviceContext);

    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessage(&msg); }
        else {
            switch (currentState) {
            case GameState::STATE_START_MENU: UpdateMenu(); RenderMenu(); break;
            case GameState::STATE_DIFFICULTY_SELECT: UpdateDiffSelect(); RenderDiffSelect(); break;
            case GameState::STATE_GAMEPLAY:
                UpdateGameplay(); RenderGameplay();
                DrawScore(g_hWnd, score); DrawBlocksRemaining(g_hWnd, blocksRemaining); DrawLives(g_hWnd, life); DrawStage(g_hWnd, stage);
                break;
            }
            RenderImGuiDebugWindow(); swapChain->Present(1, 0);
        }
    }

    ImGui_ImplDX11_Shutdown(); ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext();
    CleanD3D(); UnregisterClass(L"TorrouDX", wc.hInstance);
    return 0;
}