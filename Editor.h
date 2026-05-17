#pragma once
#include "Globals.h"

// Inicializacao
bool OpenTextureFileDialog(char* outPath, int maxLen);
void UpdateEditor();

// Save/Load por painel
bool SavePlayerConfig(const char* fullPath);
bool LoadPlayerConfig(const char* fullPath);
bool SaveBallConfig(const char* fullPath);
bool LoadBallConfig(const char* fullPath);
bool SaveStageConfig(const char* fullPath);
bool LoadStageConfig(const char* fullPath);
bool SaveObstacleConfig(const char* fullPath);
bool LoadObstacleConfig(const char* fullPath);
bool SaveBlockConfig(const char* fullPath);
bool LoadBlockConfig(const char* fullPath);
bool SaveBossConfig(const char* fullPath);
bool LoadBossConfig(const char* fullPath);
bool SavePortalConfig(const char* fullPath);
bool LoadPortalConfig(const char* fullPath);
bool SaveMenuConfig(const char* fullPath);
bool LoadMenuConfig(const char* fullPath);

// Paineis
void RenderEditorPlayer();
void RenderEditorBall();
void RenderEditorStage();
void RenderEditorObstacle();
void RenderEditorEnemy();
void RenderEditorBoss();
void RenderEditorMenu();
void RenderEditorPortal();
void RenderEditorUI();
void RenderEditor();