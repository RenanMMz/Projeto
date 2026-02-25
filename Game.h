#pragma once
#include "Globals.h"

// Declaração das funções de lógica e atualização
void ActivateDash();
void ActivateforceField();
void SpawnEnemyBulletAngle(float startX, float startY, float angleRadian, float speed);
void InitGameplay(int selectedDifficulty, int selectedLives);
void UpdateDiffSelect();
void UpdateIFrame();
void UpdateMenu();
void UpdatePaddle();
void UpdateBall();
void UpdateEnemyBullet();
void UpdateBlocks();
void UpdateProjectiles();
void UpdateForceField();
void UpdateDash();
void UpdateGameplay();