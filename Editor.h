#pragma once
#include "Globals.h"

// ==========================================
// LOOP DO EDITOR
// ==========================================
void UpdateEditor();   // mouse drag para bola no modo Stage
void RenderEditor();   // renderiza viewport do editor
void RenderDebugUI();  // painel ImGui de debug (gameplay)
void RenderEditorUI(); // painel ImGui principal do editor

// ==========================================
// PAINEIS DO EDITOR
// ==========================================

// Jogador
void RenderEditorPlayer();
bool SavePlayerConfig(const char* filename);
bool LoadPlayerConfig(const char* filename);

// Bola
void RenderEditorBall();
bool SaveBallConfig(const char* filename);
bool LoadBallConfig(const char* filename);

// Estagio
void RenderEditorStage();
bool SaveStageConfig(const char* filename);
bool LoadStageConfig(const char* filename);

// Obstáculo
void RenderEditorObstacle();
bool SaveObstacleConfig(const char* filename);
bool LoadObstacleConfig(const char* filename);
bool LoadObstacleTexture(const char* filePath);

// Inimigos / Blocos
void RenderEditorEnemy();
bool SaveBlockConfig(const char* filename);
bool LoadBlockConfig(const char* filename);

// Boss
void RenderEditorBoss();
bool SaveBossConfig(const char* filename);
bool LoadBossConfig(const char* filename);

// Bomba / Especial  (NOVO)
void RenderEditorBomb();
bool SaveBombConfig(const char* filename);
bool LoadBombConfig(const char* filename);

// Menu               (NOVO)
void RenderEditorMenu();
bool SaveMenuConfig(const char* filename);
bool LoadMenuConfig(const char* filename);

// ==========================================
// UTILITÁRIOS
// ==========================================
bool OpenTextureFileDialog(char* outPath, int maxLength);