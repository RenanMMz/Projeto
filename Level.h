#pragma once
#include "Globals.h"

void ClearLevel();
void SaveLevel(const char* filename);
void LoadLevel(const char* filename);
void AddBlocks(float x, float y, float width, float height, int hits, int pattern, int count);
void AddBlockFromConfig(float x, float y, const BlockConfig& cfg);
void AddObstacles(float x, float y, float width, float height);
void InitStage(int stageSelected);
bool SaveStageJSON(const char* fullPath);
bool LoadStageJSON(const char* fullPath);