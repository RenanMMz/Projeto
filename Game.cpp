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
// RAJADAS DE TIRO (BURST)
// Em vez de spawnar todos os tiros do padrao no mesmo frame, agendamos uma
// rajada que emite um tiro a cada `stepFrames` frames. Isso preserva a
// forma do padrao visualmente — ex: o circulo (pattern 2) gira em torno
// do inimigo conforme os projeteis sao emitidos.
// ==========================================

static void StartBurst(BulletBurst& brst, int pattern, int count, float speed,
                       float originX, float originY, float angle, int stepFrames)
{
    brst.pattern        = pattern;
    brst.count          = (count > 0) ? count : 1;
    brst.speed          = (speed > 0.0f) ? speed : 0.007f;
    brst.originX        = originX;
    brst.originY        = originY;
    brst.angle          = angle;
    brst.stepFrames     = (stepFrames > 0) ? stepFrames : 4;
    brst.idx            = 0;

    // Padroes leque (0) e radial (2) sao "simultaneos" por design: a forma
    // do padrao so existe se todos os projeteis sairem no mesmo instante.
    // Para esses casos, emitimos tudo aqui mesmo e zeramos shotsRemaining
    // para que TickBurst nao tente continuar a rajada.
    if (pattern == 0 || pattern == 2) {
        for (int i = 0; i < brst.count; i++) {
            float a = brst.angle;
            if (pattern == 0) {
                const float spread = 1.0f;
                a = (brst.count > 1)
                    ? (brst.angle - spread * 0.5f + spread * i / (brst.count - 1))
                    : brst.angle;
            } else { // pattern == 2
                a = (2.0f * 3.14159265f * i) / (float)brst.count;
            }
            SpawnEnemyBulletAngle(originX, originY, a, brst.speed);
        }
        brst.shotsRemaining = 0;
        return;
    }

    // Padroes sequenciais (1, 3, 5, ...): subTimer comeca em stepFrames
    // para que o primeiro tiro saia imediatamente no proximo TickBurst.
    brst.shotsRemaining = brst.count;
    brst.subTimer       = brst.stepFrames;
}

// Emite no maximo um tiro da rajada por chamada (frame). Sai cedo se nao
// houver rajada ativa. A posicao de spawn (brst.originX/Y) deve ser
// atualizada pelo chamador a cada frame para que o tiro saia da posicao
// atual do inimigo — caso contrario inimigos em movimento "deixam tiros
// para tras" presos ao local onde a rajada comecou.
static void TickBurst(BulletBurst& brst)
{
    if (brst.shotsRemaining <= 0) return;
    brst.subTimer++;
    if (brst.subTimer < brst.stepFrames) return;
    brst.subTimer = 0;

    const int   i      = brst.idx;
    const float pAngle = brst.angle;
    float       spd    = brst.speed;
    float       a      = pAngle;

    // Padroes 0 (leque) e 2 (radial) sao tratados em StartBurst (instantaneo).
    // Aqui processamos apenas os padroes que justificam emissao gradual.
    switch (brst.pattern) {
    case 1: // mesma direcao, velocidades crescentes
        spd = 0.005f + i * 0.002f;
        break;
    case 3: // espiral
        a = i * 0.5f;
        spd = 0.003f + i * 0.0003f;
        break;
    case 5: // reto para baixo
        a = -1.5708f;
        break;
    default:
        break;
    }

    SpawnEnemyBulletAngle(brst.originX, brst.originY, a, spd);
    brst.idx++;
    brst.shotsRemaining--;
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

    // Garantia defensiva: limpa todas as colecoes de entidades antes da
    // reinicializacao do estagio. InitStage chamara o pipeline de populacao,
    // que tambem invoca ClearLevel, mas esta chamada extra elimina qualquer
    // estado residual de sessoes anteriores (ex: retorno ao menu apos game over).
    ClearLevel();

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

void UpdateMenu()
{
    bool isUpPressed = (GetAsyncKeyState(VK_UP) & 0x8000);
    bool isDownPressed = (GetAsyncKeyState(VK_DOWN) & 0x8000);
    if (isUpPressed && !g_wasUpPressed)   selectedMenuIndex = max(0, selectedMenuIndex - 1);
    if (isDownPressed && !g_wasDownPressed) selectedMenuIndex = min(mainMenuCount - 1, selectedMenuIndex + 1);
    bool isZPressed = (GetAsyncKeyState('Z') & 0x8000);
    if (isZPressed && !g_wasZPressed) {
        if (selectedMenuIndex == 0) {
            // "Jogar" — entrada direta no gameplay com dificuldade padrao.
            // A selecao de dificuldade foi removida do fluxo atual.
            InitGameplay(0, cfgLife);
            currentState = GameState::STATE_GAMEPLAY;
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
                // Tiro ao ser atingido — agenda rajada gradual (um tiro por
                // `stepFrames` frames). UpdateBlocks emitira os projeteis.
                float pAngle = atan2f(paddleY - block.y, paddleX - block.x);
                StartBurst(block.burst, block.bulletPattern, block.bulletCount,
                           block.bulletSpeed, block.x, block.y, pAngle,
                           block.burst.stepFrames);
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

        // Continuacao de rajada em andamento — emite no maximo um tiro por
        // frame, independente de o bloco ter intervalo periodico ou estar
        // reagindo a um hit (caso burst foi iniciado em UpdateBall).
        // Atualiza a origem para a posicao atual do bloco a cada frame: se
        // o inimigo se move enquanto a rajada esta ativa (espiral, etc.),
        // os tiros sairao da posicao corrente, nao do local de inicio.
        b.burst.originX = b.x;
        b.burst.originY = b.y;
        TickBurst(b.burst);

        // Tiro periodico (blocos que atiram com intervalo proprio): so inicia
        // nova rajada quando a anterior terminou, para evitar acumulo.
        if (b.shootIntervalFrames > 0 && b.burst.shotsRemaining <= 0) {
            b.shootTimer++;
            if (b.shootTimer >= b.shootIntervalFrames) {
                b.shootTimer = 0;
                float pAngle = atan2f(paddleY - b.y, paddleX - b.x);
                StartBurst(b.burst, b.bulletPattern, b.bulletCount,
                           b.bulletSpeed, b.x, b.y, pAngle,
                           b.burst.stepFrames);
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

        //AABB do projétil do player
        float pLeft = p.x - projectileSize;
        float pRight = p.x + projectileSize;
        float pTop = p.y + projectileSize;
        float pBottom = p.y - projectileSize;

        //Loop de colisão com projéteis inimigos
        for (auto& b : enemyBullets)
        {
            if (!b.active) continue;

            //AABB do projétil inimigo
            float bLeft = b.x - b.size;
            float bRight = b.x + b.size;
            float bTop = b.y + b.size;
            float bBottom = b.y - b.size;

            //Interseção AABB entre tiros de player e inimigos
            if (pLeft < bRight && pRight > bLeft && pBottom < bTop && pTop > bBottom)
            {
                p.active = false; // Destrói o tiro do jogador
                b.active = false; // Destrói a bala inimiga
                break;
            }
        }

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

    for (auto& bullet : enemyBullets)
    {
        if (!bullet.active) continue;

        //Distância vetorial entre o centro do campo e a bala
        float dx = bullet.x - forceFieldX;
        float dy = bullet.y - forceFieldY;

        //Distância ao quadrado
        float distanceSq = (dx * dx) + (dy * dy);

        //Raio ao quadrado
        float radiusSq = forceFieldRadius * forceFieldRadius;

        if (distanceSq <= radiusSq)
        {
            bullet.active = false;
        }
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
// BOSS
// ==========================================

void UpdateBoss()
{
    if (!g_boss.active) return;
    const BossConfig& cfg = g_boss.config;
    const float hw = cfg.width  * 0.5f;
    const float hh = cfg.height;

    // Ball vs Boss collision — main body (SCRIPTED / STATIC / STATIC_FAMILIARS)
    if (cfg.archetype != BOSS_ARCH_MULTIPART) {
        bool bHitX = ballX + ballSize > g_boss.x - hw && ballX - ballSize < g_boss.x + hw;
        bool bHitY = ballY + ballSize > g_boss.y      && ballY - ballSize < g_boss.y + hh;
        if (bHitX && bHitY) {
            ballVelY *= -1.0f;
            if (!g_boss.invincible) {
                g_boss.hp--; bossHP = g_boss.hp;
                if (g_boss.hp <= 0) { g_boss.active = false; return; }
            }
        }
    }

    // Ball vs multipart nodes
    if (cfg.archetype == BOSS_ARCH_MULTIPART) {
        for (int ni = 0; ni < cfg.nodeCount && ni < BOSS_MAX_NODES; ni++) {
            if (!g_boss.nodeActive[ni]) continue;
            bool nhitX = ballX + ballSize > g_boss.nodeX[ni] - hw &&
                         ballX - ballSize < g_boss.nodeX[ni] + hw;
            bool nhitY = ballY + ballSize > g_boss.nodeY[ni] &&
                         ballY - ballSize < g_boss.nodeY[ni] + hh;
            if (nhitX && nhitY) {
                ballVelY *= -1.0f;
                if (!g_boss.invincible) {
                    g_boss.hp--; bossHP = g_boss.hp;
                    if (g_boss.hp <= 0) { g_boss.active = false; return; }
                }
            }
        }
    }

    // Phase transition — advance-only (highest-indexed phase whose threshold >= current HP%)
    if (cfg.hpPhaseCount > 0 && cfg.maxHP > 0) {
        float hpPct = (float)g_boss.hp / (float)cfg.maxHP;
        for (int i = cfg.hpPhaseCount - 1; i > g_boss.currentPhase; i--) {
            if (hpPct <= cfg.hpPhases[i].hpThresholdPct) {
                g_boss.currentPhase = i;
                g_boss.actionIdx    = 0;
                g_boss.actionTimer  = 0.0f;
                break;
            }
        }
    }

    // Continua qualquer rajada principal do boss em andamento (uma bala
    // por TickBurst). Esta chamada e' independente do script — assim, mesmo
    // que a acao SHOOT_TIMED ja' tenha avancado, os tiros pendentes saem.
    // Atualiza a origem com a posicao atual do boss para acompanhar
    // movimento durante a rajada.
    g_boss.burst.originX = g_boss.x;
    g_boss.burst.originY = g_boss.y;
    TickBurst(g_boss.burst);

    // Script execution — SCRIPTED / STATIC / STATIC_FAMILIARS all drive through hpPhases
    if (cfg.archetype != BOSS_ARCH_MULTIPART && cfg.hpPhaseCount > 0) {
        const BossScript& sc = cfg.hpPhases[g_boss.currentPhase].script;
        if (sc.actionCount > 0) {
            const int idx = g_boss.actionIdx % sc.actionCount;
            const BossAction& act = sc.actions[idx];
            g_boss.actionTimer++;
            bool done = false;
            switch (act.type) {
            case BOSS_ACT_MOVE_TO: {
                float dx = act.targetX - g_boss.x, dy = act.targetY - g_boss.y;
                float len = sqrtf(dx * dx + dy * dy);
                float sp = (act.speed > 0.0f) ? act.speed : 0.01f;
                // Snap-to-target quando o proximo passo ultrapassaria o
                // destino. Sem isto, com sp > distancia restante o boss
                // pula por cima do alvo e a logica `dx/len` inverte de
                // sinal no proximo frame -> oscilacao infinita (flicker).
                // O bug so se "desfaz" quando a fase muda e actionIdx zera.
                if (len <= sp || len < 0.005f) {
                    g_boss.x = act.targetX;
                    g_boss.y = act.targetY;
                    done = true;
                    break;
                }
                g_boss.x += (dx / len) * sp;
                g_boss.y += (dy / len) * sp;
                break;
            }
            case BOSS_ACT_TELEPORT:
                g_boss.x = act.targetX; g_boss.y = act.targetY; done = true; break;
            case BOSS_ACT_WAIT:
                done = g_boss.actionTimer >= act.duration * 60.0f; break;
            case BOSS_ACT_SHOOT_TIMED: {
                // Inicia a rajada no primeiro frame da acao; o TickBurst no
                // topo de UpdateBoss emite um tiro por frame ate' acabar.
                if (g_boss.actionTimer == 1.0f) {
                    float pAngle = atan2f(paddleY - g_boss.y, paddleX - g_boss.x);
                    StartBurst(g_boss.burst, act.bulletPattern, act.bulletCount,
                               act.bulletSpeed, g_boss.x, g_boss.y, pAngle, 4);
                }
                done = g_boss.actionTimer >= act.duration * 60.0f; break;
            }
            case BOSS_ACT_SHOOT_FIXED_PTS: {
                if (g_boss.actionTimer == 1.0f) {
                    float spd = (act.bulletSpeed > 0.0f) ? act.bulletSpeed : 0.007f;
                    for (int fp = 0; fp < act.fixedPointCount; fp++) {
                        float a = atan2f(act.fixedPtsY[fp] - g_boss.y,
                                        act.fixedPtsX[fp] - g_boss.x);
                        SpawnEnemyBulletAngle(g_boss.x, g_boss.y, a, spd);
                    }
                }
                done = g_boss.actionTimer >= act.duration * 60.0f; break;
            }
            case BOSS_ACT_CHARGE_PLAYER: {
                float dx = paddleX - g_boss.x, dy = paddleY - g_boss.y;
                float len = sqrtf(dx * dx + dy * dy);
                if (len > 0.02f) {
                    float sp = (act.speed > 0.0f) ? act.speed : 0.02f;
                    g_boss.x += (dx / len) * sp;
                    g_boss.y += (dy / len) * sp;
                }
                done = g_boss.actionTimer >= act.duration * 60.0f; break;
            }
            case BOSS_ACT_INVINCIBLE:
                g_boss.invincible = act.invincibleOn;
                done = g_boss.actionTimer >= act.duration * 60.0f; break;
            default:
                done = true; break; // SPAWN_MINION, CHANGE_SPRITE — not yet implemented, skip
            }

            if (done) {
                if (act.type == BOSS_ACT_INVINCIBLE && act.invincibleOn)
                    g_boss.invincible = false;
                g_boss.actionIdx++;
                const int loopTo = (sc.loopFromStep >= 0 && sc.loopFromStep < sc.actionCount)
                    ? sc.loopFromStep : 0;
                if (g_boss.actionIdx >= sc.actionCount)
                    g_boss.actionIdx = loopTo;
                g_boss.actionTimer = 0.0f;
            }
        }
    }

    // Multipart: each node drives its own BossScript
    if (cfg.archetype == BOSS_ARCH_MULTIPART) {
        bool anyAlive = false;
        for (int ni = 0; ni < cfg.nodeCount && ni < BOSS_MAX_NODES; ni++) {
            if (!g_boss.nodeActive[ni]) continue;
            anyAlive = true;
            // Tick da rajada do node (independente do script atual).
            // Origem segue a posicao corrente do node.
            g_boss.nodeBursts[ni].originX = g_boss.nodeX[ni];
            g_boss.nodeBursts[ni].originY = g_boss.nodeY[ni];
            TickBurst(g_boss.nodeBursts[ni]);
            const BossScript& sc = cfg.nodes[ni].script;
            if (sc.actionCount == 0) continue;
            const int idx = g_boss.nodeActionIdx[ni] % sc.actionCount;
            const BossAction& act = sc.actions[idx];
            g_boss.nodeActionTimer[ni]++;
            float& nx = g_boss.nodeX[ni];
            float& ny = g_boss.nodeY[ni];
            const float nt = g_boss.nodeActionTimer[ni];
            bool ndone = false;
            switch (act.type) {
            case BOSS_ACT_MOVE_TO: {
                float dx = act.targetX - nx, dy = act.targetY - ny;
                float len = sqrtf(dx * dx + dy * dy);
                float sp = (act.speed > 0.0f) ? act.speed : 0.01f;
                // Mesmo snap-to-target do boss principal — evita oscilacao
                // quando sp excede a distancia residual ate o alvo.
                if (len <= sp || len < 0.005f) {
                    nx = act.targetX; ny = act.targetY;
                    ndone = true; break;
                }
                nx += (dx / len) * sp; ny += (dy / len) * sp; break;
            }
            case BOSS_ACT_TELEPORT: nx = act.targetX; ny = act.targetY; ndone = true; break;
            case BOSS_ACT_WAIT:     ndone = nt >= act.duration * 60.0f; break;
            case BOSS_ACT_SHOOT_TIMED:
                // Inicia rajada gradual para o node; ticks aplicados abaixo.
                if (nt == 1.0f) {
                    float pAngle = atan2f(paddleY - ny, paddleX - nx);
                    StartBurst(g_boss.nodeBursts[ni], act.bulletPattern,
                               act.bulletCount, act.bulletSpeed, nx, ny, pAngle, 4);
                }
                ndone = nt >= act.duration * 60.0f; break;
            default: ndone = true; break;
            }
            if (ndone) {
                g_boss.nodeActionIdx[ni]++;
                const int loopTo = (sc.loopFromStep >= 0 && sc.loopFromStep < sc.actionCount)
                    ? sc.loopFromStep : 0;
                if (g_boss.nodeActionIdx[ni] >= sc.actionCount)
                    g_boss.nodeActionIdx[ni] = loopTo;
                g_boss.nodeActionTimer[ni] = 0.0f;
            }
        }
        if (!anyAlive) { g_boss.active = false; return; }
    }

    // Familiars orbit and shoot (STATIC_FAMILIARS)
    if (cfg.archetype == BOSS_ARCH_STATIC_FAMILIARS) {
        for (int i = 0; i < cfg.familiarCount && i < BOSS_MAX_FAMILIARS; i++) {
            const FamiliarConfig& fam = cfg.familiars[i];
            g_boss.familiarAngles[i] += fam.orbitSpeed;
            const float fx = g_boss.x + fam.relOffsetX +
                             cosf(g_boss.familiarAngles[i]) * fam.orbitRadius;
            const float fy = g_boss.y + fam.relOffsetY +
                             sinf(g_boss.familiarAngles[i]) * fam.orbitRadius;
            const float interval = (fam.shootIntervalSec > 0.0f)
                ? fam.shootIntervalSec * 60.0f : 120.0f;

            // Continua rajada do familiar (gradual)
            // Atualiza a origem em curso para acompanhar a orbita.
            g_boss.familiarBursts[i].originX = fx;
            g_boss.familiarBursts[i].originY = fy;
            TickBurst(g_boss.familiarBursts[i]);

            // Inicia nova rajada apenas quando a anterior terminou.
            if (g_boss.familiarBursts[i].shotsRemaining <= 0) {
                g_boss.familiarShootTimers[i]++;
                if (g_boss.familiarShootTimers[i] >= interval) {
                    g_boss.familiarShootTimers[i] = 0.0f;
                    float pAngle = atan2f(paddleY - fy, paddleX - fx);
                    StartBurst(g_boss.familiarBursts[i], fam.bulletPattern,
                               fam.bulletCount, fam.bulletSpeed, fx, fy, pAngle, 4);
                }
            }
        }
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

    // Condição de vitória: blocos normais destruídos OU boss derrotado (IE-02)
    bool stageCleared = (currentStageMode == STAGE_BOSS)
        ? !g_boss.active
        : (blocksRemaining <= 0 && blocksInitialCount > 0);
    if (stageCleared && !modoEditor) {
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

    enemyBullets.erase(std::remove_if(enemyBullets.begin(), enemyBullets.end(),
        [](const EnemyBullet& b)
        {
            return !b.active;
        }), enemyBullets.end());

    droppedItems.erase(std::remove_if(droppedItems.begin(), droppedItems.end(),
        [](const DroppedItem& d)
        {
            return !d.active;
        }), droppedItems.end());

    blocks.erase(std::remove_if(blocks.begin(), blocks.end(),
        [](const Block& b)
        {
            return !b.active;
        }), blocks.end());

    projectiles.erase(std::remove_if(projectiles.begin(), projectiles.end(),
        [](const Projectile& p)
        {
            return !p.active;
        }), projectiles.end());

    portals.erase(std::remove_if(portals.begin(), portals.end(),
        [](const Portal& tp)
        {
            return !tp.active;
        }), portals.end());

    obstacles.erase(std::remove_if(obstacles.begin(),obstacles.end(),
        [](const Obstacle& o)
        {
            return !o.active;
        }), obstacles.end());

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
    if (currentStageMode == STAGE_BOSS) UpdateBoss();
}