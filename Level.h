#pragma once
#include "Globals.h"

// Declaração das funções de manipulação de Fases (Levels)
void ClearLevel();
void SaveLevel(const char* filename);
void AddBlocks(float x, float y, float width, float height, int hits, int pattern, int count);
void AddObstacles(float x, float y, float width, float height);
void LoadLevel(const char* filename);
void InitStage(int stageSelected);