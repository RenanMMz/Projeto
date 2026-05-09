#include "Game.h"
#include "Math.h"
#include "Level.h"
#include "./imgui/imgui.h"
#include <algorithm>

// ==========================================
// DROPS
// ==========================================

int RollDropType(const float weights[4])
{
    float total = 0.0f;
    for (int i = 0; i < 4; i++) total += weights[i];
    if (total <= 0.0f) return 0;
    float r = (float)rand() / (float)RAND_MAX * total;
    float acc = 0.0f;
    for (int i = 0; i < 4; i++) {
        acc += weights[i];
        if (r <= acc) return i;
    }
    return 0;
}

void SpawnDrop(float x, float y, const float weights[4])
{
    DroppedItem d;
    d.x = x;
    d.y = y;
    d.type = RollDropType(weights);
    d.active = true;
    droppedItems.push_back(d);
}

// ==========================================
// DASH / FORCEFIELD
// ==========================================

void ActivateDash()
{
    dashActive = true; dashTimer = 15;
}

void ActivateforceField()
{
    forceFieldActive = true;
    forceFieldX = paddleX;
    forceFieldY = paddleY + paddleHeight * 0.7f;
    forceFieldTimer = 10;
}

void SpawnEnemyBulletAngle(float startX, float startY, float angleRadian, float speed)
{
    EnemyBullet b;
    b.x = startX; b.y = startY; b.size = 0.01f; b.active = true;
    b.vx = cosf(angleRadian) * speed;
    b.vy = sinf(angleRadian) * speed;
    enemyBullets.push_back(b);
}

// ==========================================
// INICIALIZACAO
// ==========================================

void InitGameplay(int selectedDifficulty, int selectedLives)
{
    life = selectedLives; difficulty = selectedDifficulty; stage = 0;
    iFrame = false; iFrameTimer = 0;
    damageEffectTimer = 0; damageBlinkCounter = 0;
    paddleVisible = true; dashActive = false; forceFieldActive = false;
    combo = 0; score = 0;
    paddleX = 0.0f; paddleWidth = paddleEditWidth; paddleHeight = paddleHeightNormal;

    InitStage(stage);  // InitStage define ballVelX/Y a partir do editorStageConfig

    // Aplica velocidade inicial configurada pelo editor APÓS o InitStage
    if (editorBallConfig.initialSpeed > 0.0f) {
        float mag = sqrtf(ballVelX * ballVelX + ballVelY * ballVelY);
        if (mag > 0.0f) {
            ballVelX = ballVelX / mag * editorBallConfig.initialSpeed;
            ballVelY = ballVelY / mag * editorBallConfig.initialSpeed;
        }
        else {
            // Fallback: velocidade padrão para cima e levemente diagonal
            ballVelX = 0.000001f;
            ballVelY = editorBallConfig.initialSpeed;
        }
    }
}

// ==========================================
// MENUS
// ==========================================

void UpdateDiffSelect()
{
    bool isUpPressed = (GetAsyncKeyState(VK_UP) & 0x8000);
    bool isDownPressed = (GetAsyncKeyState(VK_DOWN) & 0x8000);
    if (isUpPressed && !g_wasUpPressed)   selectedMenuIndex = max(0, selectedMenuIndex - 1);
    if (isDownPressed && !g_wasDownPressed) selectedMenuIndex = min(difficultyCount - 1, selectedMenuIndex + 1);
    bool isZPressed = (GetAsyncKeyState('Z') & 0x8000);
    bool isXPressed = (GetAsyncKeyState('X') & 0x8000);
    if (isZPressed && !g_wasZPressed) {
        difficulty = selectedMenuIndex;
        InitGameplay(difficulty, cfgLife);
        currentState = GameState::STATE_GAMEPLAY;
    }
    if (isXPressed) {
        selectedMenuIndex = 0;
        currentState = GameState::STATE_START_MENU;
    }
    g_wasUpPressed = isUpPressed; g_wasDownPressed = isDownPressed; g_wasZPressed = isZPressed;
}

void UpdateMenu()
{
    bool isUpPressed = (GetAsyncKeyState(VK_UP) & 0x8000);
    bool isDownPressed = (GetAsyncKeyState(VK_DOWN) & 0x8000);
    if (isUpPressed && !g_wasUpPressed)   selectedMenuIndex = max(0, selectedMenuIndex - 1);
    if (isDownPressed && !g_wasDownPressed) selectedMenuIndex = min(mainMenuCount - 1, selectedMenuIndex + 1);
    bool isZPressed = (GetAsyncKeyState('Z') & 0x8000);
    if (isZPressed && !g_wasZPressed) {
        if (selectedMenuIndex == 0) {
            selectedMenuIndex = 0;
            currentState = GameState::STATE_DIFFICULTY_SELECT;
        }
        else if (selectedMenuIndex == 1) {
            currentState = GameState::STATE_OPTIONS;
        }
        else if (selectedMenuIndex == 2) {
            PostQuitMessage(0);
        }
    }
    g_wasUpPressed = isUpPressed; g_wasDownPressed = isDownPressed; g_wasZPressed = isZPressed;
}

void UpdateOptions()
{
    // Saida via ESC volta para o menu principal. Demais inputs sao tratados pelo ImGui em RenderOptionsUI.
    static bool s_wasEscPressed = false;
    bool isEscPressed = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
    if (isEscPressed && !s_wasEscPressed) {
        selectedMenuIndex = 1;
        currentState = GameState::STATE_START_MENU;
    }
    s_wasEscPressed = isEscPressed;
}

// ==========================================
// IFRAME + EFEITO DE DANO
// ==========================================

void UpdateIFrame()
{
    if (!iFrame) return;

    const DamageEffectConfig& fx = editorPlayerConfig.damageEffect;

    if (fx.type == DMG_BLINK) {
        // Piscar: alterna visibilidade a cada blinkIntervalFrames
        damageBlinkCounter++;
        if (damageBlinkCounter >= fx.blinkIntervalFrames) {
            damageBlinkCounter = 0;
            paddleVisible = !paddleVisible;
        }
    }
    // DMG_TINT e DMG_SPRITE: o efeito é aplicado no render (shader tint)
    // por enquanto apenas controla o timer

    damageEffectTimer++;
    iFrameTimer--;
    if (iFrameTimer <= 0) {
        iFrame = false;
        paddleVisible = true;
        damageEffectTimer = 0;
        damageBlinkCounter = 0;
    }
}

// ==========================================
// PADDLE
// ==========================================

void UpdatePaddle()
{
    if (!paddleVisible) return;
    Vertex vertices[] = {
        {paddleX - paddleWidth / 2, paddleY + paddleHeight, 0.0f},
        {paddleX - paddleWidth / 2, paddleY,                0.0f},
        {paddleX + paddleWidth / 2, paddleY,                0.0f},
        {paddleX - paddleWidth / 2, paddleY + paddleHeight, 0.0f},
        {paddleX + paddleWidth / 2, paddleY,                0.0f},
        {paddleX + paddleWidth / 2, paddleY + paddleHeight, 0.0f}
    };
    deviceContext->UpdateSubresource(paddleVertexBuffer, 0, nullptr, vertices, 0, 0);
}

// ==========================================
// BOLA
// ==========================================

void UpdateBall()
{
    if (portalCooldown > 0) portalCooldown--;
    // Em caso de a bola estar no teleporte todas as outras físicas são ignoradas
    if (ballInTransit)
    {
        portalTimer--;
        if (portalTimer <= 0)
        {

            // Portal aleatório dentre os existentes
            if (!portals.empty())
            {
                int index = rand() % portals.size();
                ballX = portals[index].x;
                ballY = portals[index].y;

            }
            //angulo aleatório
            float randomAngle = ((rand() % 360)) * (2 * 3.14159f / 180.0f);

            //velocidade de saída
            float speed = 0.02f;
            ballVelY = cosf(randomAngle) * speed;
            ballVelX = sinf(randomAngle) * speed;

            ballInTransit = false;
            portalCooldown = 20;

        }
        return;
    }

    for (auto& p : portals)
    {
        if (!p.active) continue;
        if (p.active && portalCooldown <= 0)
        {
            // aabb para colisão com portal

            float pLeft = p.x - p.width / 2.0f;
            float pRight = p.x + p.width / 2.0f;
            float pBottom = p.y;
            float pTop = p.y + p.height;

            if (ballX + ballSize > pLeft && ballX - ballSize < pRight &&
                ballY + ballSize > pBottom && ballY - ballSize < pTop)
            {
                std::vector<Portal*> activeTargets;
                for (auto& target : portals)
                {
                    if (target.active)
                    {
                        activeTargets.push_back(&target);
                    }
                }

                ballInTransit = true;
                portalTimer = 60;
                ballVelX = 0.0f;
                ballVelY = 0.0f;
                float halfTimer = portalTimer / 2;
                ballX = (pLeft + pRight) / 2;
                ballY = (pTop + pBottom) / 2;
                break;

            }
        }
    }
    ballVelY -= 0.0007f;

    if (ballX - ballSize < -0.9f) {
        ballX = -0.9f + ballSize; ballVelX *= -1;
    }
    if (ballX + ballSize > 0.9f) {
        ballX = 0.9f - ballSize; ballVelX *= -1;
    }
    if (ballY + ballSize > 1.0f) {
        ballY = 1.0f - ballSize; ballVelY *= -1;
    }
    if (ballY - ballSize < -0.72f) {
        ballY = -0.72f + ballSize; ballVelY *= -0.80f; combo = 0;
    }

    // Colisao paddle
    float paddleHitOffset = (ballX - paddleX) / paddleWidth;
    if (ballY - ballSize <= paddleY + paddleHeight &&
        ballX >= paddleX - paddleWidth / 2 &&
        ballX <= paddleX + paddleWidth / 2 &&
        ballY > paddleY)
    {
        ballVelY *= -1;
        ballY = paddleY + paddleHeight + ballSize;
        ballVelX += paddleHitOffset * 0.015f;
        if (!iFrame) {
            iFrame = true; iFrameTimer = 60 * 5; life -= 1;
        }
    }

    // Colisao com blocos
    for (auto& block : blocks)
    {
        if (!block.active) continue;
        bool hitX = ballX + ballSize > block.x - block.width / 2 &&
            ballX - ballSize < block.x + block.width / 2;
        bool hitY = ballY + ballSize > block.y &&
            ballY - ballSize < block.y + block.height;
        if (hitX && hitY)
        {
            if (block.invulnerable) continue; // turret — bola passa, nao perde hit
            if (!block.iFrameBlock)
            {
                block.hits--;
                combo++;
                score += 10 * combo;
                block.iFrameBlock = true;
                block.iFrameBlockTimer = 60 * 2;
                // Tiro ao ser atingido
                float pAngle = atan2f(paddleY - block.y, paddleX - block.x);
                float spd = (block.bulletSpeed > 0.0f) ? block.bulletSpeed : 0.007f;
                for (int i = 0; i < block.bulletCount; i++) {
                    if (block.bulletPattern == 0) {
                        float spread = 1.0f, startAngle = pAngle - spread / 2.0f;
                        float a = (block.bulletCount > 1)
                            ? startAngle + (spread * i / (block.bulletCount - 1))
                            : pAngle;
                        SpawnEnemyBulletAngle(block.x, block.y, a, spd);
                    }
                    else if (block.bulletPattern == 1) {
                        SpawnEnemyBulletAngle(block.x, block.y, pAngle, 0.005f + i * 0.002f);
                    }
                    else if (block.bulletPattern == 2) {
                        float a = (2.0f * 3.14159265f * i) / block.bulletCount;
                        SpawnEnemyBulletAngle(block.x, block.y, a, spd);
                    }
                    else if (block.bulletPattern == 3) {
                        SpawnEnemyBulletAngle(block.x, block.y, i * 0.5f, 0.003f + i * 0.0003f);
                    }
                }
                break;
            }
        }
    }

    // Bala vs bola
    for (auto& eb : enemyBullets) {
        if (!eb.active) continue;
        bool hitX = ballX + ballSize > eb.x - eb.size / 2 &&
            ballX - ballSize < eb.x + eb.size / 2;
        bool hitY = ballY + ballSize > eb.y &&
            ballY - ballSize < eb.y + eb.size;
        if (hitX && hitY) {
            eb.active = false; break;
        }
    }

    // Colisao swept com obstaculos
    float nearestT = 1.0f, normalX = 0.0f, normalY = 0.0f;
    bool  collisionFound = false;
    for (auto& obstacle : obstacles) {
        if (!obstacle.active) continue;
        SweepResult res = SweptAABB(
            ballX - ballSize, ballY - ballSize, ballSize * 2, ballSize * 2,
            ballVelX, ballVelY,
            obstacle.x - obstacle.width / 2, obstacle.y, obstacle.width, obstacle.height);
        if (res.t < nearestT) {
            nearestT = res.t; normalX = res.nx; normalY = res.ny; collisionFound = true;
        }
    }

    if (collisionFound) {
        ballX += ballVelX * (nearestT - 0.00001f);
        ballY += ballVelY * (nearestT - 0.00001f);
        if (normalX != 0.0f) ballVelX = -ballVelX;
        if (normalY != 0.0f) ballVelY = -ballVelY;
    }
    else {
        ballX += ballVelX;
        ballY += ballVelY;
    }

    if (ballVelX > 0.03f) ballVelX = 0.03f;
    if (ballVelX < -0.03f) ballVelX = -0.03f;

    Vertex ballVertices[] = {
        {ballX - ballSize, ballY + ballSize, 0.0f},
        {ballX - ballSize, ballY - ballSize, 0.0f},
        {ballX + ballSize, ballY - ballSize, 0.0f},
        {ballX - ballSize, ballY + ballSize, 0.0f},
        {ballX + ballSize, ballY - ballSize, 0.0f},
        {ballX + ballSize, ballY + ballSize, 0.0f}
    };
    deviceContext->UpdateSubresource(ballVertexBuffer, 0, nullptr, ballVertices, 0, 0);
}

// ==========================================
// BALAS INIMIGAS
// ==========================================

void UpdateEnemyBullet()
{
    for (auto& bullet : enemyBullets) {
        if (!bullet.active) continue;
        bullet.x += bullet.vx; bullet.y += bullet.vy;
        if (bullet.x < -1.0f || bullet.x > 1.0f ||
            bullet.y < -1.0f || bullet.y > 1.0f) {
            bullet.active = false; continue;
        }
        // Colisao com paddle
        if (bullet.y - bullet.size < paddleY + paddleHeight &&
            bullet.x > paddleX - paddleWidth / 2 &&
            bullet.x < paddleX + paddleWidth / 2 &&
            bullet.y > paddleY)
        {
            bullet.active = false;
            if (!iFrame) {
                iFrame = true; iFrameTimer = 60 * 5; life -= 1; combo = 0;
            }
        }
    }
}

// ==========================================
// DROPPED ITEMS — queda + coleta pelo jogador
// ==========================================

void UpdateDroppedItems()
{
    const float dropSpeed = 0.005f;
    const float dropSize = 0.02f;

    for (auto& d : droppedItems) {
        if (!d.active) continue;
        d.y -= dropSpeed; // cai para baixo

        // Fora da tela
        if (d.y < -1.1f) { d.active = false; continue; }

        // Colisao com paddle
        if (d.y - dropSize < paddleY + paddleHeight &&
            d.y + dropSize > paddleY &&
            d.x + dropSize > paddleX - paddleWidth / 2 &&
            d.x - dropSize < paddleX + paddleWidth / 2)
        {
            d.active = false;
            switch (d.type) {
            case 0: life++; break;         // vida
            case 1: break;                 // shield (placeholder)
            case 3: score += 50; break;    // pontos
            }
        }
    }

    droppedItems.erase(
        std::remove_if(droppedItems.begin(), droppedItems.end(),
            [](const DroppedItem& d) { return !d.active; }),
        droppedItems.end());
}

// ==========================================
// BLOCOS — UPDATE (iFrame + destruicao + drops)
// ==========================================

void UpdateBlocks()
{
    for (auto& b : blocks) {
        if (!b.active) continue;

        // iFrame de hit
        if (b.iFrameBlock) {
            b.iFrameBlockTimer--;
            if (b.iFrameBlockTimer <= 0) b.iFrameBlock = false;
        }

        // Tiro periodico (blocos que atiram com intervalo proprio)
        if (b.shootIntervalFrames > 0) {
            b.shootTimer++;
            if (b.shootTimer >= b.shootIntervalFrames) {
                b.shootTimer = 0;
                float pAngle = atan2f(paddleY - b.y, paddleX - b.x);
                float spd = (b.bulletSpeed > 0.0f) ? b.bulletSpeed : 0.007f;
                for (int i = 0; i < b.bulletCount; i++) {
                    if (b.bulletPattern == 0) {
                        float spread = 1.0f, sa = pAngle - spread / 2.0f;
                        float a = (b.bulletCount > 1)
                            ? sa + (spread * i / (b.bulletCount - 1)) : pAngle;
                        SpawnEnemyBulletAngle(b.x, b.y, a, spd);
                    }
                    else if (b.bulletPattern == 1) {
                        SpawnEnemyBulletAngle(b.x, b.y, pAngle, 0.005f + i * 0.002f);
                    }
                    else if (b.bulletPattern == 2) {
                        float a = (2.0f * 3.14159265f * i) / b.bulletCount;
                        SpawnEnemyBulletAngle(b.x, b.y, a, spd);
                    }
                    else if (b.bulletPattern == 3) {
                        SpawnEnemyBulletAngle(b.x, b.y, i * 0.5f, 0.003f + i * 0.0003f);
                    }
                    else if (b.bulletPattern == 5) {
                        SpawnEnemyBulletAngle(b.x, b.y, -1.5708f, spd); // fixo para baixo
                    }
                }
            }
        }

        // Destruicao
        if (!b.invulnerable && b.hits <= 0) {
            if (b.hasDrop) SpawnDrop(b.x, b.y, b.dropWeights);
            b.active = false;
            blocksRemaining--;
        }
    }
}

// ==========================================
// MOVIMENTACAO DOS INIMIGOS
// ==========================================

void UpdateEnemyMovement()
{
    for (auto& b : blocks) {
        if (!b.active) continue;
        if (b.movType == MOV_NONE) continue;

        switch (b.movType) {
        case MOV_VERTICAL:
            b.y += b.movSpeed * b.movDir;
            if (fabsf(b.y - b.movOriginY) >= b.movAmplitude) {
                b.movDir *= -1.0f;
                b.y = b.movOriginY + b.movAmplitude * b.movDir * -1.0f;
            }
            break;

        case MOV_HORIZONTAL:
            b.x += b.movSpeed * b.movDir;
            if (fabsf(b.x - b.movOriginX) >= b.movAmplitude) {
                b.movDir *= -1.0f;
                b.x = b.movOriginX + b.movAmplitude * b.movDir * -1.0f;
            }
            break;

        case MOV_CIRCULAR:
            b.movAngle += b.movSpeed;
            b.x = b.movOriginX + cosf(b.movAngle) * b.movRadius;
            b.y = b.movOriginY + sinf(b.movAngle) * b.movRadius;
            break;

        default: break;
        }
    }
}

// ==========================================
// PROJÉTEIS DO JOGADOR
// ==========================================

void UpdateProjectiles()
{
    for (auto& p : projectiles) {
        if (!p.active) continue;
        p.y += projectileSpeed;
        float expandedSize = ballSize * 2.0f;
        if (p.x >= ballX - expandedSize && p.x <= ballX + expandedSize &&
            p.y >= ballY - expandedSize && p.y <= ballY + expandedSize)
        {
            float hitOffset = (p.x - ballX) / expandedSize;
            ballVelX += hitOffset * -0.01f;
            ballVelY = 0.030f;
            p.active = false;
        }
        if (p.y > 1.0f) p.active = false;
    }
    projectiles.erase(
        std::remove_if(projectiles.begin(), projectiles.end(),
            [](const Projectile& p) { return !p.active; }),
        projectiles.end());
}

// ==========================================
// FORCEFIELD / DASH
// ==========================================

void UpdateForceField()
{
    if (!forceFieldActive) return;
    forceFieldX = paddleX;
    forceFieldY = paddleY + paddleHeight / 2;
    forceFieldTimer--;
    if (forceFieldTimer <= 0) {
        forceFieldActive = false; return;
    }

    float dx = ballX - forceFieldX, dy = ballY - forceFieldY;
    float distSq = dx * dx + dy * dy, minDist = forceFieldRadius + ballSize;
    if (distSq < minDist * minDist) {
        float dist = sqrtf(distSq);
        if (dist == 0.0f) dist = 0.00001f;
        float nx = dx / dist, ny = dy / dist;
        ballX = forceFieldX + nx * minDist;
        ballY = forceFieldY + ny * minDist;
        float dot = ballVelX * nx + ballVelY * ny;
        ballVelX -= 2 * dot * nx; ballVelY -= 2 * dot * ny;
        ballVelX += nx * 0.01f;  ballVelY += ny * 0.01f;
    }
}

void UpdateDash()
{
    // Encerramento atrasado: se o timer expirou no ciclo anterior, desativa agora.
    // Isso garante que o renderizador exiba o sprite de dash no frame em que dashTimer chega a 0.
    if (dashTimer <= 0 && dashActive) {
        dashActive = false; paddleHeight = paddleHeightNormal;
        return;
    }
    if (!dashActive) return;
    float sw = 0.25f, sh = 0.15f;
    float rx = paddleX - sw / 2.0f, ry = paddleY;
    if (CircleRectCollision(ballX, ballY, ballSize, rx, ry, sw, sh)) {
        ballVelY = (fabs(ballVelY * 1.2f) <= 0.03f) ? 0.03f : fabs(ballVelY * 1.2f);
        ballVelX += (dashDir * -1.0f) * -0.02f;
    }
    paddleHeight = paddleHeightDash;
    dashTimer--;
    paddleX += dashDir * dashSpeed;
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

}

// ==========================================
// GAME LOOP PRINCIPAL
// ==========================================

void UpdateGameplay()
{
    if (stageTransitionTimer > 0) {
        stageTransitionTimer--; return;
    }
    if (life < 0) {
        selectedMenuIndex = 0; currentState = GameState::STATE_START_MENU; return;
    }

    // Condição de vitória: só verifica se o stage tinha blocos para começar
    if (blocksRemaining <= 0 && blocksInitialCount > 0 && !modoEditor) {
        int nextStage = stage + 1;
        // Verifica se existe proximo stage no projeto
        bool hasNext = false;
        if (nextStage < gameProject.stageCount && gameProject.stagePaths[nextStage][0]) {
            hasNext = true;
        } else {
            // Fallback: tenta stage%d.txt no diretorio de trabalho
            char nextFilename[64];
            snprintf(nextFilename, sizeof(nextFilename), "stage%d.txt", nextStage);
            std::ifstream nextFile(nextFilename);
            if (nextFile.is_open()) {
                nextFile.close();
                hasNext = true;
            }
        }
        if (hasNext) {
            stage = nextStage;
            InitStage(stage);
        }
        else {
            selectedMenuIndex = 0; currentState = GameState::STATE_START_MENU; return;
        }
    }

    static bool zWasPressed = false, xWasPressed = false;
    if (GetAsyncKeyState('X') & 0x8000) {
        if (!xWasPressed) {
            if (!forceFieldActive) {
                if (GetAsyncKeyState(VK_LEFT) & 0x8000) {
                    dashDir = -1.0f; ActivateDash();
                }
                else if (GetAsyncKeyState(VK_RIGHT) & 0x8000) {
                    dashDir = 1.0f; ActivateDash();
                }
                else if (!dashActive) ActivateforceField();
            }
        }
        xWasPressed = true;
    }
    else xWasPressed = false;

    if (GetAsyncKeyState('Z') & 0x8000) {
        if (!zWasPressed) {
            Projectile p; p.x = paddleX; p.y = paddleY + paddleHeight + 0.003f; p.active = true;
            projectiles.push_back(p);
        }
        zWasPressed = true;
    }
    else zWasPressed = false;

    // Verifica captura de teclado pelo ImGui (seguro mesmo antes do NewFrame)
    ImGuiIO& io = ImGui::GetIO();
    bool imguiWantsKeys = io.WantCaptureKeyboard;
    paddleMoveDir = 0;
    if (!imguiWantsKeys && !forceFieldActive && !dashActive) {
        float spd = (paddleEditMoveSpeed > 0.0f) ? paddleEditMoveSpeed : 0.01f;
        if (GetAsyncKeyState(VK_LEFT) & 0x8000) { paddleX -= spd; paddleMoveDir = -1; }
        if (GetAsyncKeyState(VK_RIGHT) & 0x8000) { paddleX += spd; paddleMoveDir = 1; }
    }
    if (paddleX - paddleWidth / 2 < -0.90f) paddleX = -0.90f + paddleWidth / 2;
    if (paddleX + paddleWidth / 2 > 0.90f) paddleX = 0.90f - paddleWidth / 2;

    UpdatePaddle();
    UpdateEnemyMovement();
    UpdateBall();
    UpdateProjectiles();
    UpdateBlocks();
    UpdateForceField();
    UpdateDash();
    UpdateIFrame();
    UpdateEnemyBullet();
    UpdateDroppedItems();
}