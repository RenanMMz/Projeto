#pragma once
#include "Globals.h"

bool InitD3D(HWND hWnd);
void CleanD3D();
void DrawRectButton(float x1, float y1, float x2, float y2, const float color[4]);
void DrawLives(HWND hwnd, int life);
void DrawStage(HWND hwnd, int stage);
void DrawScore(HWND hwnd, int score);
void DrawBlocksRemaining(HWND hwnd, int blocksRemaining);
void RenderMenu();
void RenderDiffSelect();
void RenderGameplay();
void RenderDebugUI();
void RenderEditor();
void DrawObstaclePreview(float centerX, float centerY);