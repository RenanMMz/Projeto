#pragma once
#include "Globals.h"

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
void UpdateEnemyMovement();   // movimentacao dos blocos inimigos
void SpawnDrop(float x, float y, const float weights[4]); // spawna drop na posicao
int  RollDropType(const float weights[4]);                 // sorteia tipo de drop pelos pesos