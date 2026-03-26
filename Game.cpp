#include "Game.h"
#include "Math.h"
#include "Level.h"
#include "./imgui/imgui.h"

void ActivateDash()
{
	dashActive = true; dashTimer = 15;
}
void ActivateforceField()
{
	forceFieldActive = true; forceFieldX = paddleX; forceFieldY = paddleY + paddleHeight * 0.7f; forceFieldTimer = 10;
}

void SpawnEnemyBulletAngle(float startX, float startY, float angleRadian, float speed)
{
	EnemyBullet b; b.x = startX; b.y = startY; b.size = 0.01f; b.active = true;
	b.vx = cosf(angleRadian) * speed; b.vy = sinf(angleRadian) * speed;
	enemyBullets.push_back(b);
}

void InitGameplay(int selectedDifficulty, int selectedLives)
{
	life = selectedLives; difficulty = selectedDifficulty; stage = 0;
	iFrame = false; iFrameTimer = 0; paddleVisible = true; dashActive = false; forceFieldActive = false;
	combo = 0; score = 0; paddleX = 0.0f; paddleHeight = paddleHeightNormal;
	ballX = 0.75f; ballY = -0.5f; ballSize = 0.03f; ballVelY = 0.02f;
	InitStage(stage);
}

void UpdateDiffSelect()
{
	bool isUpPressed = (GetAsyncKeyState(VK_UP) & 0x8000); bool isDownPressed = (GetAsyncKeyState(VK_DOWN) & 0x8000);
	if (isUpPressed && !g_wasUpPressed) selectedMenuIndex = max(0, selectedMenuIndex - 1);
	if (isDownPressed && !g_wasDownPressed) selectedMenuIndex = min(difficultyCount - 1, selectedMenuIndex + 1);
	bool isZPressed = (GetAsyncKeyState('Z') & 0x8000); bool isXPressed = (GetAsyncKeyState('X') & 0x8000);
	if (isZPressed && !g_wasZPressed)
	{
		difficulty = selectedMenuIndex; InitGameplay(difficulty, cfgLife); currentState = GameState::STATE_GAMEPLAY;
	}
	if (isXPressed)
	{
		selectedMenuIndex = 0; currentState = GameState::STATE_START_MENU;
	}
	g_wasUpPressed = isUpPressed; g_wasDownPressed = isDownPressed; g_wasZPressed = isZPressed;
}

void UpdateIFrame()
{
	if (iFrame)
	{
		paddleVisible = !paddleVisible; iFrameTimer -= 1; if (iFrameTimer <= 0)
		{
			iFrame = false; paddleVisible = true;
		}
	}
}

void UpdateMenu()
{
	bool isUpPressed = (GetAsyncKeyState(VK_UP) & 0x8000); bool isDownPressed = (GetAsyncKeyState(VK_DOWN) & 0x8000);
	if (isUpPressed && !g_wasUpPressed) selectedMenuIndex = max(0, selectedMenuIndex - 1);
	if (isDownPressed && !g_wasDownPressed) selectedMenuIndex = min(mainMenuCount - 1, selectedMenuIndex + 1);
	bool isZPressed = (GetAsyncKeyState('Z') & 0x8000);
	if (isZPressed && !g_wasZPressed)
	{
		if (selectedMenuIndex == 0)
		{
			selectedMenuIndex = 1; currentState = GameState::STATE_DIFFICULTY_SELECT;
		}
		else if (selectedMenuIndex == 2)
		{
			PostQuitMessage(0);
		}
	}
	g_wasUpPressed = isUpPressed; g_wasDownPressed = isDownPressed; g_wasZPressed = isZPressed;
}

void UpdatePaddle()
{
	if (paddleVisible)
	{
		Vertex vertices[] = {
			{paddleX - paddleWidth / 2, paddleY + paddleHeight, 0.0f}, {paddleX - paddleWidth / 2, paddleY, 0.0f}, {paddleX + paddleWidth / 2, paddleY, 0.0f},
			{paddleX - paddleWidth / 2, paddleY + paddleHeight, 0.0f}, {paddleX + paddleWidth / 2, paddleY, 0.0f}, {paddleX + paddleWidth / 2, paddleY + paddleHeight, 0.0f}
		};
		deviceContext->UpdateSubresource(paddleVertexBuffer, 0, nullptr, vertices, 0, 0);
	}
}

void UpdateBall()
{
	ballVelY -= 0.0007f;
	if (ballX - ballSize < -0.9f)
	{
		ballX = -0.9f + ballSize; ballVelX *= -1;
	}
	if (ballX + ballSize > 0.9f)
	{
		ballX = 0.9f - ballSize; ballVelX *= -1;
	}
	if (ballY + ballSize > 1.0f)
	{
		ballY = 1.0f - ballSize; ballVelY *= -1;
	}
	if (ballY - ballSize < -0.72f)
	{
		ballY = -0.72f + ballSize; ballVelY *= -0.80f; combo = 0;
	}

	float paddleHitOffset = (ballX - paddleX) / paddleWidth;
	if (ballY - ballSize <= paddleY + paddleHeight && ballX >= paddleX - paddleWidth / 2 && ballX <= paddleX + paddleWidth / 2 && ballY > paddleY)
	{
		ballVelY *= -1; ballY = paddleY + paddleHeight + ballSize; ballVelX += paddleHitOffset * 0.015f;
		if (!iFrame)
		{
			iFrame = true; iFrameTimer = 60 * 5; life -= 1;
		}
	}

	for (auto& block : blocks)
	{
		if (!block.active) continue;
		bool hitX = ballX + ballSize > block.x - block.width / 2 && ballX - ballSize < block.x + block.width / 2;
		bool hitY = ballY + ballSize > block.y && ballY - ballSize < block.y + block.height;
		if (hitX && hitY)
		{
			if (!block.iFrameBlock)
			{
				block.hits -= 1; combo++; score += 10 * (combo); block.iFrameBlock = true; block.iFrameBlockTimer = 60 * 2;
				float pAngle = atan2f(paddleY - block.y, paddleX - block.x);
				for (int i = 0; i < block.bulletCount; i++)
				{
					if (block.bulletPattern == 0)
					{
						float spread = 1.0f; float startAngle = pAngle - (spread / 2.0f); float a = (block.bulletCount > 1) ? startAngle + (spread * i / (block.bulletCount - 1)) : pAngle; SpawnEnemyBulletAngle(block.x, block.y, a, 0.007f);
					}
					else if (block.bulletPattern == 1)
					{
						float spd = 0.005f + (i * 0.002f); SpawnEnemyBulletAngle(block.x, block.y, pAngle, spd);
					}
					else if (block.bulletPattern == 2)
					{
						float a = (2.0f * 3.14159265f * i) / block.bulletCount; SpawnEnemyBulletAngle(block.x, block.y, a, 0.006f);
					}
					else if (block.bulletPattern == 3)
					{
						float a = i * 0.5f; float spd = 0.003f + (i * 0.0003f); SpawnEnemyBulletAngle(block.x, block.y, a, spd);
					}
				}
				break;
			}
		}
	}

	for (auto& p : portals)
	{
		if (!p.active) continue;

		float pLeft = p.x - p.width / 2.0f;
		float pRight = p.x + p.width / 2.0f;
		float pBottom = p.y;
		float pTop = p.y + p.height;

		if (ballX + ballSize > pLeft && ballX - ballSize < pRight &&
			ballY + ballSize > pBottom && ballY - ballSize < pTop)
		{

			// Timer
			ballInTransit = true;
			portalTimer = 60;

			break;
		}
	}

	for (auto& enemyBullet : enemyBullets)
	{
		if (!enemyBullet.active) continue;
		bool hitX = ballX + ballSize > enemyBullet.x - enemyBullet.size / 2 && ballX - ballSize < enemyBullet.x + enemyBullet.size / 2;
		bool hitY = ballY + ballSize > enemyBullet.y && ballY - ballSize < enemyBullet.y + enemyBullet.size;
		if (hitX && hitY)
		{
			enemyBullet.active = false; break;
		}
	}

	float nearestT = 1.0f; float normalX = 0.0f, normalY = 0.0f; bool collisionFound = false;
	for (auto& obstacle : obstacles)
	{
		if (!obstacle.active) continue;
		SweepResult res = SweptAABB(ballX - ballSize, ballY - ballSize, ballSize * 2, ballSize * 2, ballVelX, ballVelY, obstacle.x - obstacle.width / 2, obstacle.y, obstacle.width, obstacle.height);
		if (res.t < nearestT)
		{
			nearestT = res.t;
			normalX = res.nx;
			normalY = res.ny;
			collisionFound = true;
		}
	}

	if (collisionFound)
	{
		ballX += ballVelX * (nearestT - 0.00001f);
		ballY += ballVelY * (nearestT - 0.00001f);

		if (normalX != 0.0f) ballVelX = -ballVelX;
		if (normalY != 0.0f) ballVelY = -ballVelY;
	}
	else
	{
		ballX += ballVelX;
		ballY += ballVelY;
	}


	Vertex ballVertices[] = { {ballX - ballSize, ballY + ballSize, 0.0f}, {ballX - ballSize, ballY - ballSize, 0.0f}, {ballX + ballSize, ballY - ballSize, 0.0f}, {ballX - ballSize, ballY + ballSize, 0.0f}, {ballX + ballSize, ballY - ballSize, 0.0f}, {ballX + ballSize, ballY + ballSize, 0.0f} };
	if (ballVelX > 0.03f)
	{
		ballVelX = 0.03f;
	};
	deviceContext->UpdateSubresource(ballVertexBuffer, 0, nullptr, ballVertices, 0, 0);
}

void UpdateEnemyBullet()
{
	for (auto& bullet : enemyBullets)
	{
		if (!bullet.active) continue;
		bullet.x += bullet.vx; bullet.y += bullet.vy;
		if (bullet.x < -1.0f || bullet.x > 1.0f || bullet.y < -1.0f || bullet.y > 1.0f) bullet.active = false;
		if (bullet.y - bullet.size < paddleY + paddleHeight && bullet.x > paddleX - paddleWidth / 2 && bullet.x < paddleX + paddleWidth / 2 && bullet.y > paddleY)
		{
			bullet.active = false; if (!iFrame)
			{
				iFrame = true; iFrameTimer = 60 * 5; life -= 1; combo = 0;
			}
		}
	}
}

void UpdateBlocks()
{
	for (auto& b : blocks)
	{
		if (!b.active) continue;
		if (b.hits <= 0)
		{
			b.active = false; blocksRemaining--;
		}
		if (b.iFrameBlock)
		{
			b.iFrameBlockTimer -= 1; if (b.iFrameBlockTimer <= 0)
			{
				b.iFrameBlock = false;
			}
		}
	}
}

void UpdateProjectiles()
{
	for (auto& p : projectiles)
	{
		if (!p.active) continue; p.y += projectileSpeed; float hitboxScale = 2.0f; float expandedSize = ballSize * hitboxScale;
		if (p.x >= ballX - expandedSize && p.x <= ballX + expandedSize && p.y >= ballY - expandedSize && p.y <= ballY + expandedSize)
		{
			float hitOffset = (p.x - ballX) / expandedSize; ballVelX += hitOffset * -0.01f; ballVelY = 0.030f; p.active = false;
		}
		if (p.y > 1.0f) p.active = false;
	}
	projectiles.erase(std::remove_if(projectiles.begin(), projectiles.end(), [](const Projectile& p)
		{
			return !p.active;
		}), projectiles.end());
}

void UpdateForceField()
{
	if (forceFieldActive)
	{
		forceFieldX = paddleX; forceFieldY = paddleY + (paddleHeight / 2); forceFieldTimer -= 1;
		if (forceFieldTimer <= 0)
		{
			forceFieldActive = false;
		}
		float dx = ballX - forceFieldX; float dy = ballY - forceFieldY; float distSq = dx * dx + dy * dy; float minDist = forceFieldRadius + ballSize;
		if (distSq < minDist * minDist)
		{
			float angle = atan2f(dy, dx);
			if (angle >= -3.14159265f && angle <= 3.14159265f)
			{
				float dist = sqrtf(distSq); if (dist == 0.0f) dist = 0.00001f; float nx = dx / dist; float ny = dy / dist;
				ballX = forceFieldX + nx * (minDist); ballY = forceFieldY + ny * (minDist);
				float dot = ballVelX * nx + ballVelY * ny; ballVelX -= 2 * dot * nx; ballVelY -= 2 * dot * ny; ballVelX += nx * 0.01f; ballVelY += ny * 0.01f;
			}
		}
	}
}

void UpdateDash()
{
	if (dashActive)
	{
		float shieldWidth = 0.25f; float shieldHeight = 0.15f; float shieldY = paddleY; float rx = paddleX - shieldWidth / 2.0f; float ry = shieldY; float rw = shieldWidth; float rh = shieldHeight;
		if (CircleRectCollision(ballX, ballY, ballSize, rx, ry, rw, rh))
		{
			if (fabs(ballVelY * 1.2f) <= 0.03f)
			{
				ballVelY = 0.03f;
			}
			else
			{
				ballVelY = fabs(ballVelY * 1.2f);
			}
			float hitOffset = dashDir * -1.0f; ballVelX += hitOffset * -0.02f;
		}
		paddleHeight = paddleHeightDash; dashTimer -= 1; if (dashTimer <= 0)
		{
			dashActive = false; paddleHeight = paddleHeightNormal;
		} paddleX += (dashDir * dashSpeed);
	}
}

void HandlePortals()
{
	if (ballInTransit)
	{
		portalTimer--;

		// Para a bolinha no portal de entrada até a hora de teleportar no portal de saída
		ballVelX = 0;
		ballVelY = 0;
		ballX = portalEntranceX;
		ballY = portalEntranceY;

		if (portalTimer <= 0)
		{
			ballInTransit = false;
			ballX = portalExitX;
			ballY = portalExitY;
			ballVelX = portalExitVelX;
			ballVelY = portalExitVelY;
		}
		return;
	}

	// para cada portal
	for (auto& p : portals)
	{
		if (!p.active) continue;

		// aabb para colisão com portal
		if (ballX + ballSize > p.x && ballX - ballSize < p.x + p.width &&
			ballY + ballSize > p.y && ballY - ballSize < p.y + p.height)
		{
			ballInTransit = true;
			portalTimer = 60;

			std::vector<Portal*> activeTargets;
			for (auto& target : portals)
			{
				if (target.active)
				{
					activeTargets.push_back(&target);
				}
			}

			// portal de saída aleatório
			int randomIndex = rand() % activeTargets.size();
			Portal* exitPortal = activeTargets[randomIndex];

			// velocidade aleatória - Ajustar aqui futuramente pq eu zerei a velocidade acima, pensar em outra fórmula
			float currentSpeed = sqrt(ballVelX * ballVelX + ballVelY * ballVelY);
			if (currentSpeed < 0.01f) currentSpeed = 0.02f; // velocidade mínima - Adicionar velocidade máxima

			// ângulo aleatório
			float randomAngle = (float)(rand() % 360) * (3.14159f / 180.0f);

			portalExitVelX = cos(randomAngle) * currentSpeed;
			portalExitVelY = sin(randomAngle) * currentSpeed;

			// posição de saída (meio do portal de saída)
			portalExitX = exitPortal->x + (exitPortal->width / 2.0f);
			portalExitY = exitPortal->y + (exitPortal->height / 2.0f);

			// empurra a bola para fora do portal para evitar imediatamente entrar no portal novamente
			portalExitX += (portalExitVelX * 2.0f);
			portalExitY += (portalExitVelY * 2.0f);

			break;
		}
	}
}

void UpdateGameplay()
{
	if (stageTransitionTimer > 0)
	{
		stageTransitionTimer--; return;
	}
	if (life < 0)
	{
		selectedMenuIndex = 0; currentState = GameState::STATE_START_MENU; return;
	}

	if (blocksRemaining <= 0 && !modoEditor)
	{
		blocksRemaining = 0; char nextFilename[64]; snprintf(nextFilename, sizeof(nextFilename), "stage%d.txt", stage + 1);
		std::ifstream nextFile(nextFilename);
		if (nextFile.is_open())
		{
			nextFile.close(); stage++; InitStage(stage);
		}
		else
		{
			selectedMenuIndex = 0; currentState = GameState::STATE_START_MENU; return;
		}
	}

	static bool zWasPressed = false; static bool xWasPressed = false;
	if (GetAsyncKeyState('X') & 0x8000)
	{
		if (!xWasPressed)
		{
			if (!forceFieldActive)
			{
				if (GetAsyncKeyState(VK_LEFT) & 0x8000)
				{
					dashDir = -1.0f; ActivateDash();
				}
				else if (GetAsyncKeyState(VK_RIGHT) & 0x8000)
				{
					dashDir = 1.0f; ActivateDash();
				}
				else if (!dashActive)
				{
					ActivateforceField();
				}
			}
		} xWasPressed = true;
	}
	else
	{
		xWasPressed = false;
	}

	if (GetAsyncKeyState('Z') & 0x8000)
	{
		if (!zWasPressed)
		{
			Projectile p; p.x = paddleX; p.y = paddleY + paddleHeight + 0.003f; p.active = true; projectiles.push_back(p);
		} zWasPressed = true;
	}
	else
	{
		zWasPressed = false;
	}

	ImGuiIO& io = ImGui::GetIO();
	if (!io.WantCaptureKeyboard)
	{
		if (!forceFieldActive && !dashActive)
		{
			if (GetAsyncKeyState(VK_LEFT) & 0x8000) paddleX -= 0.01f; if (GetAsyncKeyState(VK_RIGHT) & 0x8000) paddleX += 0.01f;
		}
	}

	if (paddleX - paddleWidth / 2 < -0.90f) paddleX = -0.90f + paddleWidth / 2; if (paddleX + paddleWidth / 2 > 0.90f) paddleX = 0.90f - paddleWidth / 2;
	UpdatePaddle(); UpdateBall(); UpdateProjectiles(); UpdateBlocks(); UpdateForceField(); UpdateDash(); UpdateIFrame(); UpdateEnemyBullet();
}