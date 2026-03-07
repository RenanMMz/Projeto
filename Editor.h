#pragma once
#include "Globals.h"

void UpdateEditor();
void RenderEditor();
void RenderEditorUI();

void RenderEditorObstacle();
bool SaveObstacleConfig(const char* filename);
bool LoadObstacleConfig(const char* filename);

void RenderEditorPlayer();
void RenderEditorBall();
void RenderEditorStage();
void RenderEditorEnemy();
void RenderEditorBoss();
