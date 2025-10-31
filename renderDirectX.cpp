#define UNICODE
#define _UNICODE
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <vector>
#include <DirectXMath.h>
using namespace DirectX;
#pragma comment(lib, "D3DCompiler.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

// Variáveis globais DirectX

HWND g_hWnd = nullptr;

IDXGISwapChain *swapChain = nullptr;
ID3D11Device *device = nullptr;
ID3D11DeviceContext *deviceContext = nullptr;
ID3D11RenderTargetView *renderTargetView = nullptr;

ID3D11VertexShader *vertexShader = nullptr;

ID3D11InputLayout *inputLayout = nullptr;
ID3D11RasterizerState *rasterState = nullptr;

ID3D11ShaderResourceView *forceFieldTexture = nullptr;

// buffers
ID3D11Buffer *obstacleBuffer = nullptr;
ID3D11Buffer *vertexBuffer = nullptr;
ID3D11Buffer *blockVertexBuffer = nullptr;
ID3D11Buffer *ballVertexBuffer = nullptr;
ID3D11Buffer *projectileBuffer = nullptr;
ID3D11Buffer *forceFieldBuffer = nullptr;
ID3D11Buffer *dashShieldBuffer = nullptr;
ID3D11Buffer *blockColorBuffer = nullptr;
ID3D11Buffer *enemyBulletBuffer = nullptr;

// pixel shaders
ID3D11PixelShader *pixelShaderObstacle = nullptr;
ID3D11PixelShader *pixelShader = nullptr;
ID3D11PixelShader *pixelShaderBlock = nullptr;
ID3D11PixelShader *pixelShaderPaddle = nullptr;
ID3D11PixelShader *pixelShaderBall = nullptr;
ID3D11PixelShader *pixelShaderProjectile = nullptr;
ID3D11PixelShader *pixelShaderEnemyBullet = nullptr;
ID3D11PixelShader *pixelShaderMenu = nullptr;
ID3D11PixelShader *pixelShaderButton = nullptr;

int selectedMenuIndex = 0;
const char *mainMenuItems[] = {
    "Start",
    "Options",
    "Close"};

const int mainMenuCount = 3;

bool g_wasUpPressed = false;
bool g_wasDownPressed = false;
bool g_wasZPressed = false;

enum GameState
{
    STATE_START_MENU,
    STATE_DIFFICULTY_SELECT,
    STATE_OPTIONS,
    STATE_GAMEPLAY,
    STATE_GAMEOVER,
    STATE_PAUSE
};
GameState currentState = STATE_START_MENU; // Inicia no menu inicial
int difficulty = 0;                        // Dificuldades? Obviamente uma delas terá que se chamar "Lunatic"
const char *difficulties[] = {
    "Easy",
    "Normal",
    "Hard",
    "Lunatic"};

const int difficultyCount = 4;

bool timeout = false;         // tempo acaba = "desperation"
int stage = 0;                // seletor de stage
int stageTransitionTimer = 0; // variável que fará a contagem do load
int life = 0;                 // vidas, = 0 skill issue
int cfgLife = 3;
int timer = 0;                // timer de cada stage, começa em X e vai a 0 onde a variável vira True
int bossHP = 0;               //
int blocksRemaining = 0;      // blocos restantes, 0 = next.
int timeCount = 0;            // conta o tempo total
int menuOption = 0;           // Não sei se será utilizado, mas tecnicamente pode ser utilizado para definir qual opção da lista será selecionada
int score = 0;                //
int highScore = 0;            // Ainda não sei se isso vai resetar o valor de HS sempre que reabrir. Devo salvar em um arquivo separado e buscar o valor?
int combo = 0;                // multiplicador de score, reseta quando a bolinha cai no chão sem ser rebatida
bool iFrame = false;
int iFrameTimer = 0;

// tirinho
bool projectileActive = false;
float projectileX = 0.0f;
float projectileY = 0.0f;
float projectileSize = 0.02f;
float projectileSpeed = 0.05f;

// barrinha
float paddleX = 0.0f;            // posição horizontal (começa no meio)
const float paddleY = -0.75f;    // posição fixa vertical
const float paddleWidth = 0.08f; // largura (0.04 esquerda + 0.04 direita)
float paddleHeight = 0.20f;
float paddleHeightNormal = 0.20f;
float paddleHeightDash = 0.08f;
bool paddleVisible = true;

// bolinha
float ballX = 0.75f;
float ballY = -0.5f;
float ballVelX = 0.0000000000000000000000000000000000000000001f; // erro de divisão por 0 no cálculo de colisão (possivelmente por causa do AABB) se a velocidade da bola for igual a 0, que faz com que conte colisão com obstáculos em qualquer posição horizontal
float ballVelY = 0.02f;
float ballSize = 0.03f;

// shield
bool forceFieldActive = false;
float forceFieldRadius = 0.20f;
float forceFieldTimer = 0.00f;
float forceFieldY = 0.00f;
float forceFieldX = 0.00f;

// rasteira
bool dashActive = false;
int dashTimer = 0;
float dashDir = 0.0f;     // -1 para esquerda, +1 para direita
float dashSpeed = 0.025f; // velocidade durante a rasteira, só um pouco mais rápido do que a velocidade normal

struct ColorConstantBuffer
{
    DirectX::XMFLOAT4 color; // R, G, B, A
};

struct Projectile
{
    float x, y;
    bool active;
};

std::vector<Projectile> projectiles;

struct Block
{
    float x, y;
    float width, height;
    bool active;
    int hits;
    bool iFrameBlock;
    int iFrameBlockTimer;
};

std::vector<Block> blocks;

struct Obstacle
{
    float x, y, width, height;
    bool active;
};

std::vector<Obstacle> obstacles;

struct Vertex
{
    float x, y, z;
};

struct VertexMenu
{
    float x, y, z;
    float r, g, b, a;
};

struct EnemyBullet
{
    float x, y;   // posição
    float vx, vy; // velocidade
    float size;   // tamanho
    bool active;  // desligar os tiros quando saírem da tela, e não esquecer de desligar os tiros também no final de cada stage/boss para não acontecer o clássico bug da Shinki.
};

std::vector<EnemyBullet> enemyBullets;

struct SweepResult // Struct auxiliar para cálculo de AABB Swept.
{
    float t;      // tempo normalizado de colisão (0 a 1)
    float nx, ny; // normal, é a direção para o qual a aresta de um objeto está virada
};

SweepResult SweptAABB( // Retorna um valor entre 0 e 1 que indica quando a colisão ocorreu, valor de 0 indica que ocorreu no começo do movimento, valores PRÓXIMOS de 1 indicam que ocorreu no final do movimento e valor de 1 indica que não ocorreu nenhuma colisão.
    float bx, float by, float bw, float bh,
    float vx, float vy,
    float ox, float oy, float ow, float oh)
{
    float xInvEntry, yInvEntry;
    float xInvExit, yInvExit;

    // distância de entrada e saída
    if (vx > 0.0f)
    {
        xInvEntry = (ox - (bx + bw)) - 0.0f;
        xInvExit = ((ox + ow) - bx);
    }
    else
    {
        xInvEntry = ((ox + ow) - bx);
        xInvExit = (ox - (bx + bw));
    }

    if (vy > 0.0f)
    {
        yInvEntry = (oy - (by + bh));
        yInvExit = ((oy + oh) - by);
    }
    else
    {
        yInvEntry = ((oy + oh) - by);
        yInvExit = (oy - (by + bh));
    }

    float xEntry, yEntry;
    float xExit, yExit;

    if (vx == 0.0f)
    {
        xEntry = -INFINITY;
        xExit = INFINITY;
    }
    else
    {
        xEntry = xInvEntry / vx;
        xExit = xInvExit / vx;
    }

    if (vy == 0.0f)
    {
        yEntry = -INFINITY;
        yExit = INFINITY;
    }
    else
    {
        yEntry = yInvEntry / vy;
        yExit = yInvExit / vy;
    }

    float entryTime = max(xEntry, yEntry);
    float exitTime = min(xExit, yExit);

    SweepResult result;
    result.t = 1.0f;
    result.nx = result.ny = 0.0f;

    if (entryTime > exitTime || (xEntry < 0.0f && yEntry < 0.0f) || xEntry > 1.0f || yEntry > 1.0f)
        return result;

    result.t = entryTime;

    if (xEntry > yEntry)
    {
        result.nx = (vx < 0.0f) ? 1.0f : -1.0f;
        result.ny = 0.0f;
    }
    else
    {
        result.nx = 0.0f;
        result.ny = (vy < 0.0f) ? 1.0f : -1.0f;
    }

    return result;
}

void ActivateDash()
{
    dashActive = true;
    dashTimer = 15;
}

void ActivateforceField()
{
    forceFieldActive = true;
    forceFieldX = paddleX;
    forceFieldY = paddleY + paddleHeight * 0.7f;
    forceFieldTimer = 10; // Em frames
}

void DrawRectButton(float x1, float y1, float x2, float y2, const float color[4])
{
    Vertex vertices[] = {
        {x1, y1, 0.0f},
        {x1, y2, 0.0f},
        {x2, y2, 0.0f},

        {x1, y1, 0.0f},
        {x2, y2, 0.0f},
        {x2, y1, 0.0f},
    };

    // Atualiza buffer de vértices
    deviceContext->UpdateSubresource(vertexBuffer, 0, nullptr, vertices, 0, 0);

    // Atualiza o constant buffer com a cor recebida para o botão, que varia dependendo de seu estado de seleção
    ColorConstantBuffer cb = {
        DirectX::XMFLOAT4(color[0], color[1], color[2], color[3])};

    // reutilizando o blockcolorbuffer para isso, já que blocos e menus não aparecerão na mesma tela
    deviceContext->UpdateSubresource(blockColorBuffer, 0, nullptr, &cb, 0, 0);

    // Setar Input Layout e Vertex Shader
    deviceContext->IASetInputLayout(inputLayout);
    deviceContext->VSSetShader(vertexShader, nullptr, 0);
    deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Define cor via constante no pixel shader, se quiser
    deviceContext->PSSetShader(pixelShaderBlock, nullptr, 0);
    deviceContext->PSSetConstantBuffers(0, 1, &blockColorBuffer); // Associa o buffer de cor ao slot 0

    // Desenhar
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    deviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
    deviceContext->Draw(6, 0);
}

// Colisão de círculo com um retângulo para calcular o ângulo do shield
bool CircleRectCollision(float cx, float cy, float radius,
                         float rx, float ry, float rw, float rh)
{
    // ponto mais próximo dentro do retângulo ao centro do círculo
    float closestX = max(rx, min(cx, rx + rw));
    float closestY = max(ry, min(cy, ry + rh));

    // distância até esse ponto
    float dx = cx - closestX;
    float dy = cy - closestY;

    return (dx * dx + dy * dy) < (radius * radius);
}

void RenderDiffSelect()
{
    // Limpa a tela (cor de fundo)
    float clearColor[4] = {0.05f, 0.05f, 0.1f, 1.0f}; // azul escuro
    deviceContext->ClearRenderTargetView(renderTargetView, clearColor);

    float startY = 0.5f;  // onde começam os botões
    float spacing = 0.3f; // espaço vertical entre eles
    float buttonWidth = 0.8f;
    float buttonHeight = 0.2f;

    // Cores
    XMFLOAT4 colorNormal = XMFLOAT4(0.3f, 0.3f, 0.8f, 1.0f);
    XMFLOAT4 colorSelected = XMFLOAT4(1.0f, 1.0f, 0.3f, 1.0f);

    // Desenha os botões
    for (int i = 0; i < difficultyCount; i++)
    {
        float yCenter = startY - i * spacing; // Posição central vertical
        XMFLOAT4 color = (selectedMenuIndex == i) ? colorSelected : colorNormal;

        float x1 = -buttonWidth / 2.0f;
        float y1 = yCenter + buttonHeight / 2.0f;
        float x2 = buttonWidth / 2.0f;
        float y2 = yCenter - buttonHeight / 2.0f;

        DrawRectButton(x1, y1, x2, y2, &color.x); // &color.x passa o float[4] da cor
    }
}

void AddBlocks(float x, float y, float width, float height, int hits) // É o "construtor" dos blocos, vou chamar múltiplos AddBlocks com valores diferentes para cada stage.
{
    blocksRemaining++;
    Block b;
    b.x = x;
    b.y = y;
    b.width = width;
    b.height = height;
    b.hits = hits;
    b.active = true;
    b.iFrameBlock = false;
    b.iFrameBlockTimer = 0;
    blocks.push_back(b);
}

void PlaceBlocks(int stageSelected)
{
    blocks.clear();
    float width = 0.1f;
    float height = 0.1f;

    switch (stageSelected)
    {
    case 0:
        blocksRemaining = 0;
        AddBlocks(-0.85f, 0.8f, width, height, 1);
        AddBlocks(-0.7f, 0.8f, width, height, 1);
        AddBlocks(-0.55f, 0.8f, width, height, 1);
        AddBlocks(-0.4f, 0.8f, width, height, 1);
        AddBlocks(-0.25f, 0.8f, width, height, 1);
        AddBlocks(-0.1f, 0.8f, width, height, 1);
        AddBlocks(0.05f, 0.8f, width, height, 1);
        AddBlocks(0.2f, 0.8f, width, height, 1);
        AddBlocks(0.35f, 0.8f, width, height, 1);
        AddBlocks(0.5f, 0.8f, width, height, 1);
        AddBlocks(0.65f, 0.8f, width, height, 1);
        AddBlocks(0.8f, 0.8f, width, height, 1);

        AddBlocks(-0.80f, 0.65f, width, height, 1);
        AddBlocks(-0.65f, 0.65f, width, height, 1);
        AddBlocks(-0.50f, 0.65f, width, height, 1);
        AddBlocks(-0.35f, 0.65f, width, height, 1);
        AddBlocks(-0.20f, 0.65f, width, height, 1);
        AddBlocks(-0.05f, 0.65f, width, height, 1);
        AddBlocks(0.10f, 0.65f, width, height, 1);
        AddBlocks(0.25f, 0.65f, width, height, 1);
        AddBlocks(0.40f, 0.65f, width, height, 1);
        AddBlocks(0.55f, 0.65f, width, height, 1);
        AddBlocks(0.70f, 0.65f, width, height, 1);
        AddBlocks(0.85f, 0.65f, width, height, 1);

        AddBlocks(-0.85f, 0.5f, width, height, 1);
        AddBlocks(-0.7f, 0.5f, width, height, 1);
        AddBlocks(-0.55f, 0.5f, width, height, 1);
        AddBlocks(-0.4f, 0.5f, width, height, 1);
        AddBlocks(-0.25f, 0.5f, width, height, 1);
        AddBlocks(-0.1f, 0.5f, width, height, 1);
        AddBlocks(0.05f, 0.5f, width, height, 1);
        AddBlocks(0.2f, 0.5f, width, height, 1);
        AddBlocks(0.35f, 0.5f, width, height, 1);
        AddBlocks(0.5f, 0.5f, width, height, 1);
        AddBlocks(0.65f, 0.5f, width, height, 1);
        AddBlocks(0.8f, 0.5f, width, height, 1);
        break;

    case 1:
        blocksRemaining = 0;
        AddBlocks(-0.85f, 0.8f, width, height, 2);
        AddBlocks(-0.7f, 0.8f, width, height, 2);
        AddBlocks(-0.55f, 0.8f, width, height, 2);
        AddBlocks(-0.4f, 0.8f, width, height, 2);
        AddBlocks(-0.25f, 0.8f, width, height, 2);
        AddBlocks(-0.1f, 0.8f, width, height, 2);
        AddBlocks(0.05f, 0.8f, width, height, 2);
        AddBlocks(0.2f, 0.8f, width, height, 2);
        AddBlocks(0.35f, 0.8f, width, height, 2);
        AddBlocks(0.5f, 0.8f, width, height, 2);
        AddBlocks(0.65f, 0.8f, width, height, 2);
        AddBlocks(0.8f, 0.8f, width, height, 2);

        AddBlocks(-0.80f, 0.65f, width, height, 2);
        AddBlocks(-0.65f, 0.65f, width, height, 2);
        AddBlocks(-0.50f, 0.65f, width, height, 2);
        AddBlocks(-0.35f, 0.65f, width, height, 2);
        AddBlocks(-0.20f, 0.65f, width, height, 2);
        AddBlocks(-0.05f, 0.65f, width, height, 2);
        AddBlocks(0.10f, 0.65f, width, height, 2);
        AddBlocks(0.25f, 0.65f, width, height, 2);
        AddBlocks(0.40f, 0.65f, width, height, 2);
        AddBlocks(0.55f, 0.65f, width, height, 2);
        AddBlocks(0.70f, 0.65f, width, height, 2);
        AddBlocks(0.85f, 0.65f, width, height, 2);

        AddBlocks(-0.85f, 0.5f, width, height, 2);
        AddBlocks(-0.7f, 0.5f, width, height, 2);
        AddBlocks(-0.55f, 0.5f, width, height, 2);
        AddBlocks(-0.4f, 0.5f, width, height, 2);
        AddBlocks(-0.25f, 0.5f, width, height, 2);
        AddBlocks(-0.1f, 0.5f, width, height, 2);
        AddBlocks(0.05f, 0.5f, width, height, 2);
        AddBlocks(0.2f, 0.5f, width, height, 2);
        AddBlocks(0.35f, 0.5f, width, height, 2);
        AddBlocks(0.5f, 0.5f, width, height, 2);
        AddBlocks(0.65f, 0.5f, width, height, 2);
        AddBlocks(0.8f, 0.5f, width, height, 2);
        break;
    }
}

void AddObstacles(float x, float y, float width, float height) // É o "construtor" dos obstáculos, vou chamar múltiplos AddObstacles com valores diferentes para cada stage.
{
    Obstacle o;
    o.x = x;
    o.y = y;
    o.width = width;
    o.height = height;
    o.active = true;
    obstacles.push_back(o);
}

void PlaceObstacles(int stageSelected)
{
    obstacles.clear();

    switch (stageSelected)
    {
    case 0:
        AddObstacles(0.0f, -0.3f, 0.7f, 0.01f);
        break;
    case 1:
        AddObstacles(0.0f, 0.5f, 0.9f, 0.01f);
        break;
    }
};

void InitStage(int stageSelected)
{
    paddleX = 0.0f;
    ballX = 0.75f;
    ballY = -0.5f;
    ballVelX = 0.0000000000000000000000000000000000000000001f; // erro de divisão por 0 no cálculo de colisão (possivelmente por causa do AABB) se a velocidade da bola for igual a 0, que faz com que conte colisão com obstáculos em qualquer posição horizontal
    ballVelY = 0.02f;
    projectiles.clear();
    enemyBullets.clear();

    PlaceBlocks(stageSelected);
    PlaceObstacles(stageSelected);
    stageTransitionTimer = 60;
}

void InitGameplay(int selectedDifficulty, int selectedLives)
{

    // Variáveis alteráveis com opções, valores recebidos na função
    life = selectedLives;
    difficulty = selectedDifficulty;
    stage = 0;

    // Reset de variáveis player
    iFrame = false;
    iFrameTimer = 0;
    dashActive = false;
    forceFieldActive = false;
    combo = 0;
    score = 0;
    paddleX = 0.0f;
    paddleHeight = paddleHeightNormal;
    dashActive = false;
    forceFieldActive = false;

    // Reset de variáveis da bola
    ballX = 0.75f;
    ballY = -0.5f;
    ballSize = 0.03f;
    ballVelX = 0.0000000000000000000000000000000000000000001f; // erro de divisão por 0 no cálculo de colisão (possivelmente por causa do AABB) se a velocidade da bola for igual a 0, que faz com que conte colisão com obstáculos em qualquer posição horizontal
    ballVelY = 0.02f;

    InitStage(stage);
    projectiles.clear();
    enemyBullets.clear();
}

void UpdateDiffSelect()
{

    bool isUpPressed = (GetAsyncKeyState(VK_UP) & 0x8000);
    bool isDownPressed = (GetAsyncKeyState(VK_DOWN) & 0x8000);

    if (isUpPressed && !g_wasUpPressed)
        selectedMenuIndex = max(0, selectedMenuIndex - 1);
    if (isDownPressed && !g_wasDownPressed)
        selectedMenuIndex = min(difficultyCount - 1, selectedMenuIndex + 1);

    bool isZPressed = (GetAsyncKeyState('Z') & 0x8000);

    bool isXPressed = (GetAsyncKeyState('X') & 0x8000);

    if (isZPressed && !g_wasZPressed)
    {
        switch (selectedMenuIndex)
        {
        case 0: // Easy
            difficulty = 0;
            stage = 0;
            InitGameplay(difficulty, cfgLife);
            currentState = GameState::STATE_GAMEPLAY;
            break;
        case 1: // Normal
            difficulty = 1;
            stage = 0;
            InitGameplay(difficulty, cfgLife);
            currentState = GameState::STATE_GAMEPLAY;
            break;
        case 2: // Hard
            difficulty = 2;
            stage = 0;
            InitGameplay(difficulty, cfgLife);
            currentState = GameState::STATE_GAMEPLAY;
            break;
        case 3: // Lunatic
            difficulty = 3;
            stage = 0;
            InitGameplay(difficulty, cfgLife);
            currentState = GameState::STATE_GAMEPLAY;
            break;
        }
    }

    if (isXPressed)
    {
        selectedMenuIndex = 0;
        currentState = GameState::STATE_START_MENU; // X volta para o menu inicial
    }

    // marca que os botões foram apertados para que não repita o clique em todos os frames
    g_wasUpPressed = isUpPressed;
    g_wasDownPressed = isDownPressed;
    g_wasZPressed = isZPressed;
}

void SpawnEnemyBullet(float startX, float startY, float targetX, float targetY)
{
    EnemyBullet b;
    b.x = startX;
    b.y = startY;
    b.size = 0.01f;
    b.active = true;

    float dx = targetX - startX;
    float dy = targetY - startY;
    float len = sqrt(dx * dx + dy * dy);
    if (len == 0)
        len = 0.001f;

    dx /= len;
    dy /= len;

    float speed = 0.007f;
    b.vx = dx * speed;
    b.vy = dy * speed;

    enemyBullets.push_back(b);
};

void DrawLives(HWND hwnd, int life)
{
    HDC hdc = GetDC(hwnd);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));

    wchar_t buffer[32];
    swprintf(buffer, 32, L"Lives: %d", life);

    TextOutW(hdc, 10, 10, buffer, wcslen(buffer));
    ReleaseDC(hwnd, hdc);
}

void DrawStage(HWND hwnd, int stage)
{
    HDC hdc = GetDC(hwnd);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));

    wchar_t buffer[32];
    swprintf(buffer, 32, L"Stage: %d", stage + 1);

    TextOutW(hdc, 400, 10, buffer, wcslen(buffer));
    ReleaseDC(hwnd, hdc);
}

void DrawScore(HWND hwnd, int score)
{
    HDC hdc = GetDC(hwnd);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));

    wchar_t buffer[32];
    swprintf(buffer, 32, L"Score: %d", score*10);

    TextOutW(hdc, 10, 30, buffer, wcslen(buffer));
    ReleaseDC(hwnd, hdc);
}

void DrawBlocksRemaining(HWND hwnd, int blocksRemaining)
{
    HDC hdc = GetDC(hwnd);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));

    wchar_t buffer[32];
    swprintf(buffer, 32, L"Blocks Remaining: %d", blocksRemaining);

    TextOutW(hdc, 200, 10, buffer, wcslen(buffer));
    ReleaseDC(hwnd, hdc);
}

void UpdateIFrame()
{
    if (iFrame)
    {
        paddleVisible = !paddleVisible;
        iFrameTimer -= 1;
        if (iFrameTimer <= 0)
        {
            iFrame = false;
            paddleVisible = true;
        }
    }
}

void UpdateMenu()
{

    bool isUpPressed = (GetAsyncKeyState(VK_UP) & 0x8000);
    bool isDownPressed = (GetAsyncKeyState(VK_DOWN) & 0x8000);

    if (isUpPressed && !g_wasUpPressed)
        selectedMenuIndex = max(0, selectedMenuIndex - 1);
    if (isDownPressed && !g_wasDownPressed)
        selectedMenuIndex = min(mainMenuCount - 1, selectedMenuIndex + 1);

    bool isZPressed = (GetAsyncKeyState('Z') & 0x8000);

    if (isZPressed && !g_wasZPressed)
    {
        switch (selectedMenuIndex)
        {
        case 0: // Start
            selectedMenuIndex = 1;
            currentState = GameState::STATE_DIFFICULTY_SELECT;
            break;
        case 1:
            currentState = GameState::STATE_START_MENU;
            break;
        case 2: // Fecha
            PostQuitMessage(0);
            break;
        }
    }

    // marca que os botões foram apertados para que não repita o clique em todos os frames
    g_wasUpPressed = isUpPressed;
    g_wasDownPressed = isDownPressed;
    g_wasZPressed = isZPressed;
}

void RenderMenu()
{
    // Limpa a tela (cor de fundo)
    float clearColor[4] = {0.05f, 0.05f, 0.1f, 1.0f}; // azul escuro
    deviceContext->ClearRenderTargetView(renderTargetView, clearColor);

    float startY = 0.2f;  // onde começam os botões
    float spacing = 0.3f; // espaço vertical entre eles
    float buttonWidth = 0.8f;
    float buttonHeight = 0.2f;

    // Cores
    XMFLOAT4 colorNormal = XMFLOAT4(0.3f, 0.3f, 0.8f, 1.0f);
    XMFLOAT4 colorSelected = XMFLOAT4(1.0f, 1.0f, 0.3f, 1.0f);

    // Desenha os botões
    for (int i = 0; i < mainMenuCount; i++)
    {
        float yCenter = startY - i * spacing; // Posição central vertical
        XMFLOAT4 color = (selectedMenuIndex == i) ? colorSelected : colorNormal;

        float x1 = -buttonWidth / 2.0f;
        float y1 = yCenter + buttonHeight / 2.0f;
        float x2 = buttonWidth / 2.0f;
        float y2 = yCenter - buttonHeight / 2.0f;

        DrawRectButton(x1, y1, x2, y2, &color.x); // &color.x passa o float[4] da cor
    }
}

void UpdatePaddle()
{
    if (paddleVisible == true)
    {
        Vertex vertices[] = {
            // Triângulo 1
            {paddleX - paddleWidth / 2, paddleY + paddleHeight, 0.0f}, // esquerda cima
            {paddleX - paddleWidth / 2, paddleY, 0.0f},                // esquerda baixo
            {paddleX + paddleWidth / 2, paddleY, 0.0f},                // direita baixo

            // Triângulo 2
            {paddleX - paddleWidth / 2, paddleY + paddleHeight, 0.0f}, // esquerda cima
            {paddleX + paddleWidth / 2, paddleY, 0.0f},                // direita baixo
            {paddleX + paddleWidth / 2, paddleY + paddleHeight, 0.0f}  // direita cima
        };

        deviceContext->UpdateSubresource(vertexBuffer, 0, nullptr, vertices, 0, 0);
    }
}

void UpdateBall()
{
    ballX += ballVelX;
    ballY += ballVelY;
    ballVelY -= 0.0007f;

    // colisão com paredes
    if (ballX - ballSize < -0.9f) // esquerda
    {
        ballX = -0.9f + ballSize;
        ballVelX *= -1; // inverte velocidade horizontal
    }

    if (ballX + ballSize > 0.9f) // direita
    {
        ballX = 0.9f - ballSize;
        ballVelX *= -1;
    }

    // colisão com teto
    if (ballY + ballSize > 1.0f)
    {
        ballY = 1.0f - ballSize;
        ballVelY *= -1; // inverte velocidade vertical
    }

    // Bola caiu no chão
    if (ballY - ballSize < -0.72f)
    {
        ballY = -0.72f + ballSize;
        ballVelY *= -0.80f;
        combo = 0;
    }

    // colisão com a barra

    float paddleHitOffset = (ballX - paddleX) / paddleWidth; // Local da barrinha

    if (ballY - ballSize <= paddleY + paddleHeight &&
        ballX >= paddleX - paddleWidth / 2 &&
        ballX <= paddleX + paddleWidth / 2 &&
        ballY > paddleY)
    {
        ballVelY *= -1;
        ballY = paddleY + paddleHeight + ballSize; // corrigir posição

        ballVelX += paddleHitOffset * 0.015f;

        if (!iFrame)
        {
            iFrame = true;
            iFrameTimer = 60 * 5;
            life -= 1;
        }
    }

    // Colisão com blocos, não tem efeito na bola mas quebra os blocos
    for (auto &block : blocks)
    {
        if (!block.active)
            continue;
        bool hitX = ballX + ballSize > block.x - block.width / 2 &&
                    ballX - ballSize < block.x + block.width / 2;
        bool hitY = ballY + ballSize > block.y &&
                    ballY - ballSize < block.y + block.height;

        if (hitX && hitY)
        {
            if (!block.iFrameBlock)
            {
                block.hits -= 1;
                combo++;
                score += 10 * (combo);
                block.iFrameBlock = true;
                block.iFrameBlockTimer = 60 * 2;
                SpawnEnemyBullet(block.x, block.y, paddleX, paddleY);

                break;
            }
        }
    }

    for (auto &enemyBullet : enemyBullets)
    {
        if (!enemyBullet.active)
            continue;
        bool hitX = ballX + ballSize > enemyBullet.x - enemyBullet.size / 2 &&
                    ballX - ballSize < enemyBullet.x + enemyBullet.size / 2;
        bool hitY = ballY + ballSize > enemyBullet.y &&
                    ballY - ballSize < enemyBullet.y + enemyBullet.size;

        if (hitX && hitY)
        {
            enemyBullet.active = false;
            break;
        }
    }


    // Colisão com obstáculos com cálculo de AABB

    float dt = 1.0f; // Delta Time

    float nearestT = 1.0f;
    float normalX = 0.0f, normalY = 0.0f;

    for (auto &obstacle : obstacles)
    {
        if (!obstacle.active)
            continue;

        SweepResult res = SweptAABB(
            ballX - ballSize, ballY - ballSize, ballSize * 2, ballSize * 2,
            ballVelX * dt, ballVelY * dt,
            obstacle.x - obstacle.width / 2, obstacle.y,
            obstacle.width, obstacle.height);

        if (res.t < nearestT)
        {
            nearestT = res.t;
            normalX = res.nx;
            normalY = res.ny;
        }

        // Se houver colisão dentro do próximo passo
        if (nearestT < 1.0f)
        {
            // Move até o ponto exato da colisão
            ballX += ballVelX * dt * nearestT;
            ballY += ballVelY * dt * nearestT;

            // Reflete a velocidade conforme a normal
            if (normalX != 0.0f)
                ballVelX = -ballVelX;
            if (normalY != 0.0f)
                ballVelY = -ballVelY;
        }
    }

    // atualizar geometria
    Vertex ballVertices[] = {
        {ballX - ballSize, ballY + ballSize, 0.0f},
        {ballX - ballSize, ballY - ballSize, 0.0f},
        {ballX + ballSize, ballY - ballSize, 0.0f},

        {ballX - ballSize, ballY + ballSize, 0.0f},
        {ballX + ballSize, ballY - ballSize, 0.0f},
        {ballX + ballSize, ballY + ballSize, 0.0f},
    };

    if (ballVelX > 0.03f)
    {
        ballVelX = 0.03f; // Limita a velocidade horizontal da bolinha para ela não ficar rápida demais, ajustar valor conforme necessário
    };

    deviceContext->UpdateSubresource(ballVertexBuffer, 0, nullptr, ballVertices, 0, 0);
}

void UpdateEnemyBullet()
{
    for (auto &bullet : enemyBullets)
    {
        if (!bullet.active)
            continue;
        bullet.x += bullet.vx;
        bullet.y += bullet.vy;

        // desativa o tiro se sair da tela
        if (bullet.x < -1.0f || bullet.x > 1.0f || bullet.y < -1.0f || bullet.y > 1.0f)
            bullet.active = false;

        if (bullet.y - bullet.size < paddleY + paddleHeight &&
            bullet.x > paddleX - paddleWidth / 2 &&
            bullet.x < paddleX + paddleWidth / 2 &&
            bullet.y > paddleY)
        {
            bullet.active = false;

            if (!iFrame)
            {
                iFrame = true;
                iFrameTimer = 60 * 5;
                life -= 1;
                combo = 0;
            }
        }
    };
};

void UpdateBlocks()
{
    for (auto &b : blocks)
    {
        if (!b.active)
            continue;
        if (b.hits <= 0)
        {

            b.active = false;
            blocksRemaining--;
        }
        if (b.iFrameBlock)
        {
            b.iFrameBlockTimer -= 1;

            if (b.iFrameBlockTimer <= 0)
            {
                b.iFrameBlock = false;
            }
        }
    }
}

void UpdateProjectiles()
{
    for (auto &p : projectiles)
    {
        if (!p.active)
            continue;

        p.y += projectileSpeed;

        // colisão com a bolinha
        float hitboxScale = 2.0f;
        float expandedSize = ballSize * hitboxScale;
        if (p.x >= ballX - expandedSize &&
            p.x <= ballX + expandedSize &&
            p.y >= ballY - expandedSize &&
            p.y <= ballY + expandedSize)
        {
            float hitOffset = (p.x - ballX) / expandedSize; // Local onde a bolinha foi atingida pelo proj

            ballVelX += hitOffset * -0.01f; // impulso horizontal dependendo de onde a bolinha foi atingida.

            ballVelY = 0.030f; // impulso vertical ao acertar a bolinha

            p.active = false;
        }

        // saiu da tela
        if (p.y > 1.0f)
            p.active = false;
    }

    // remove projéteis inativos da lista
    projectiles.erase(
        std::remove_if(projectiles.begin(), projectiles.end(),
                       [](const Projectile &p)
                       { return !p.active; }),
        projectiles.end());
}

void UpdateForceField()
{
    if (forceFieldActive)
    {
        forceFieldX = paddleX;
        forceFieldY = paddleY + (paddleHeight / 2);
        forceFieldTimer -= 1;
        if (forceFieldTimer <= 0)
        {
            forceFieldActive = false;
        }

        // colisão com a bolinha
        float dx = ballX - forceFieldX;
        float dy = ballY - forceFieldY;
        float distSq = dx * dx + dy * dy;
        float minDist = forceFieldRadius + ballSize;

        if (distSq < minDist * minDist) // colisão ocorreu
        {
            float angle = atan2f(dy, dx);
            if (angle >= -3.14159265f && angle <= 3.14159265f) // ">=" é a parte inferior do círculo, "<=" é a parte superior. Igualar um dos valores a 0 faz o cálculo desconsiderar aquela parte do círculo
            {
                float dist = sqrtf(distSq);
                if (dist == 0.0f)
                    dist = 0.00001f; // evita divisão por zero

                float nx = dx / dist;
                float ny = dy / dist;

                // Reposiciona a bola para fora do shield
                ballX = forceFieldX + nx * (minDist);
                ballY = forceFieldY + ny * (minDist);

                // Reflete a velocidade da bolinha
                float dot = ballVelX * nx + ballVelY * ny;
                ballVelX -= 2 * dot * nx;
                ballVelY -= 2 * dot * ny;

                // Impulso extra (get parried idiot)
                ballVelX += nx * 0.01f;
                ballVelY += ny * 0.01f;
            }
        }
    }
}

void UpdateDash()
{
    if (dashActive)
    {
        float shieldWidth = 0.25f;
        float shieldHeight = 0.15f;

        float shieldY = paddleY;
        float rx = paddleX - shieldWidth / 2.0f;
        float ry = shieldY;
        float rw = shieldWidth;
        float rh = shieldHeight;

        if (CircleRectCollision(ballX, ballY, ballSize, rx, ry, rw, rh))
        {
            /* // reposiciona a bolinha para fora do dashShield
            ballY = ry + rh + ballSize + 0.001f;*/

            // rebote vertical (sempre para cima, a rasteira serve para levantar a bola)
            if (fabs(ballVelY * 1.2f) <= 0.03f)
            {
                ballVelY = 0.03f;
            }
            else
            {
                ballVelY = fabs(ballVelY * 1.2f);
            }

            // variação horizontal conforme a direção do dash
            float hitOffset = dashDir * -1.0f;
            ballVelX += hitOffset * -0.02f;
        }

        paddleHeight = paddleHeightDash;
        dashTimer -= 1;
        if (dashTimer <= 0)
        {
            dashActive = false;
            paddleHeight = paddleHeightNormal;
        }

        paddleX += (dashDir * dashSpeed);
    }
}

const char *g_VS =
    "struct VS_INPUT { float3 pos : POSITION; }; \
    struct PS_INPUT { float4 pos : SV_POSITION; }; \
    PS_INPUT VSMain(VS_INPUT input) { PS_INPUT output; output.pos = float4(input.pos,1.0f); return output; }";

const char *g_PS =
    "struct PS_INPUT { float4 pos : SV_POSITION; }; \
    float4 PSMain(PS_INPUT input) : SV_TARGET { return float4(1.0f,0.0f,0.0f,1.0f); }";

const char *g_PS_Ball =
    "struct PS_INPUT { float4 pos : SV_POSITION; }; \
     float4 PSMain(PS_INPUT input) : SV_TARGET { return float4(0.0f,1.0f,0.0f,1.0f); }"; // verde

const char *g_PS_Projectile =
    "struct PS_INPUT { float4 pos : SV_POSITION; }; \
     float4 PSMain(PS_INPUT input) : SV_TARGET { return float4(1.0f,1.0f,1.0f,1.0f); }"; // branco

const char *g_PS_Obstacle =
    "struct PS_INPUT { float4 pos : SV_POSITION; }; \
     float4 PSMain(PS_INPUT input) : SV_TARGET { return float4(1.0f,1.0f,0.0f,1.0f); }"; // amarelo

const char *g_PS_Block =
    "cbuffer ColorBuffer : register(b0) { float4 blockColor; }; \
    struct PS_INPUT { float4 pos : SV_POSITION; }; \
     float4 PSMain(PS_INPUT input) : SV_TARGET { return blockColor; }"; // cor definida pela quantidade de hits, que será um valor definido no renderFrame

const char *g_PS_Bullet =
    "struct PS_INPUT { float4 pos : SV_POSITION; }; \
     float4 PSMain(PS_INPUT input) : SV_TARGET { return float4(0.900f,0.2500f,0.950f,1.0f); }"; // roxo

const char *g_PS_Menu =
    "struct PS_INPUT { float4 pos : SV_POSITION; }; \
     float4 PSMain(PS_INPUT input) : SV_TARGET { return float4(1.0f,1.0f,1.0f,1.0f); }"; // roxo

// Inicializa DirectX
bool InitD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount = 1;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = hWnd;
    scd.SampleDesc.Count = 1;
    scd.Windowed = TRUE;

    if (FAILED(D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
            D3D11_SDK_VERSION, &scd, &swapChain, &device, nullptr, &deviceContext)))
    {
        return false;
    }

    // Back buffer
    ID3D11Texture2D *backBuffer = nullptr;
    swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID *)&backBuffer);
    device->CreateRenderTargetView(backBuffer, nullptr, &renderTargetView);
    backBuffer->Release();

    deviceContext->OMSetRenderTargets(1, &renderTargetView, nullptr);

    D3D11_VIEWPORT viewport = {};

    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = 800;
    viewport.Height = 600;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    deviceContext->RSSetViewports(1, &viewport);

    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode = D3D11_FILL_SOLID; // sólido
    rd.CullMode = D3D11_CULL_NONE;  // desativa backface culling
    rd.FrontCounterClockwise = false;

    HRESULT hr;
    hr = device->CreateRasterizerState(&rd, &rasterState);
    if (FAILED(hr))
        return false;

    // Ativar rasterizer state
    deviceContext->RSSetState(rasterState);

    // Compilar shaders
    ID3DBlob *vsBlob = nullptr;
    ID3DBlob *psBlob = nullptr;
    ID3DBlob *psBlobBall = nullptr;
    ID3DBlob *errorBlob = nullptr;
    ID3DBlob *psBlobBlock = nullptr;
    ID3DBlob *psBlobProjectile = nullptr;
    ID3DBlob *psBlobObstacle = nullptr;
    ID3DBlob *psBlobBullet = nullptr;
    ID3DBlob *psBlobMenu = nullptr;

    if (FAILED(D3DCompile(g_PS_Block, strlen(g_PS_Block), nullptr, nullptr, nullptr,
                          "PSMain", "ps_4_0", 0, 0, &psBlobBlock, &errorBlob)))
    {
        if (errorBlob)
        {
            OutputDebugStringA((char *)errorBlob->GetBufferPointer());
            errorBlob->Release();
        }
        return false;
    }

    if (FAILED(D3DCompile(g_PS_Bullet, strlen(g_PS_Bullet), nullptr, nullptr, nullptr,
                          "PSMain", "ps_4_0", 0, 0, &psBlobBullet, &errorBlob)))
    {
        if (errorBlob)
        {
            OutputDebugStringA((char *)errorBlob->GetBufferPointer());
            errorBlob->Release();
        }
        return false;
    }

    if (FAILED(D3DCompile(g_PS_Projectile, strlen(g_PS_Projectile), nullptr, nullptr, nullptr,
                          "PSMain", "ps_4_0", 0, 0, &psBlobProjectile, &errorBlob)))
    {
        if (errorBlob)
        {
            OutputDebugStringA((char *)errorBlob->GetBufferPointer());
            errorBlob->Release();
        }
        return false;
    }

    hr = device->CreatePixelShader(psBlobProjectile->GetBufferPointer(), psBlobProjectile->GetBufferSize(), nullptr, &pixelShaderProjectile);
    if (FAILED(hr))
        return false;

    psBlobProjectile->Release();

    // Criar buffer do projétil
    Vertex projectileVertices[6] = {};
    D3D11_BUFFER_DESC bdProj = {};
    bdProj.Usage = D3D11_USAGE_DEFAULT;
    bdProj.ByteWidth = sizeof(Vertex) * _countof(projectileVertices);
    bdProj.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA initProj = {};
    initProj.pSysMem = projectileVertices;

    hr = device->CreateBuffer(&bdProj, &initProj, &projectileBuffer);
    if (FAILED(hr))
        return false;

    if (FAILED(D3DCompile(g_VS, strlen(g_VS), nullptr, nullptr, nullptr,
                          "VSMain", "vs_4_0", 0, 0, &vsBlob, &errorBlob)))
    {
        if (errorBlob)
        {
            OutputDebugStringA((char *)errorBlob->GetBufferPointer());
            errorBlob->Release();
        }
        return false;
    }
    if (FAILED(D3DCompile(g_PS, strlen(g_PS), nullptr, nullptr, nullptr,
                          "PSMain", "ps_4_0", 0, 0, &psBlob, &errorBlob)))
    {
        if (errorBlob)
        {
            OutputDebugStringA((char *)errorBlob->GetBufferPointer());
            errorBlob->Release();
        }
        return false;
    }
    if (FAILED(D3DCompile(g_PS_Ball, strlen(g_PS_Ball), nullptr, nullptr, nullptr,
                          "PSMain", "ps_4_0", 0, 0, &psBlobBall, &errorBlob)))
    {
        if (errorBlob)
        {
            OutputDebugStringA((char *)errorBlob->GetBufferPointer());
            errorBlob->Release();
        }
        return false;
    }

    if (FAILED(D3DCompile(g_PS_Obstacle, strlen(g_PS_Obstacle), nullptr, nullptr, nullptr,
                          "PSMain", "ps_4_0", 0, 0, &psBlobObstacle, &errorBlob)))
    {
        if (errorBlob)
        {
            OutputDebugStringA((char *)errorBlob->GetBufferPointer());
            errorBlob->Release();
        }
        return false;
    }

    hr = device->CreatePixelShader(psBlobBlock->GetBufferPointer(), psBlobBlock->GetBufferSize(), nullptr, &pixelShaderMenu);
    if (FAILED(hr))
        return false;

    hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vertexShader);
    if (FAILED(hr))
        return false;

    hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pixelShaderPaddle);
    if (FAILED(hr))
        return false;
    hr = device->CreatePixelShader(psBlobBall->GetBufferPointer(), psBlobBall->GetBufferSize(), nullptr, &pixelShaderBall);
    if (FAILED(hr))
        return false;

    hr = device->CreatePixelShader(psBlobBlock->GetBufferPointer(), psBlobBlock->GetBufferSize(), nullptr, &pixelShaderBlock);
    if (FAILED(hr))
        return false;

    hr = device->CreatePixelShader(psBlobObstacle->GetBufferPointer(), psBlobObstacle->GetBufferSize(), nullptr, &pixelShaderObstacle);
    if (FAILED(hr))
        return false;

    hr = device->CreatePixelShader(psBlobBullet->GetBufferPointer(), psBlobBullet->GetBufferSize(), nullptr, &pixelShaderEnemyBullet);
    if (FAILED(hr))
        return false;

    // Layout dos vértices
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
         D3D11_INPUT_PER_VERTEX_DATA, 0}};
    hr = device->CreateInputLayout(layout, 1, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &inputLayout);
    if (FAILED(hr))
        return false;

    vsBlob->Release();
    psBlob->Release();
    psBlobBall->Release();
    psBlobBlock->Release();
    psBlobBullet->Release();

    // Vértices de um retângulo (2 triângulos)
    Vertex vertices[] = {
        // Triângulo 1
        {-0.12f, -0.7f, 0.0f},  // esquerda cima
        {-0.12f, -0.75f, 0.0f}, // esquerda baixo
        {0.12f, -0.75f, 0.0f},  // direita baixo

        // Triângulo 2
        {-0.12f, -0.7f, 0.0f}, // esquerda cima
        {0.12f, -0.75f, 0.0f}, // direita baixo
        {0.12f, -0.7f, 0.0f}   // direita cima
    };

    Vertex ballVertices[6];

    // Criar vertex buffer da bolinha
    D3D11_BUFFER_DESC bdBall = {};
    bdBall.Usage = D3D11_USAGE_DEFAULT;
    bdBall.ByteWidth = sizeof(Vertex) * _countof(ballVertices);
    bdBall.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA initBall = {};
    initBall.pSysMem = ballVertices;

    hr = device->CreateBuffer(&bdBall, &initBall, &ballVertexBuffer);
    if (FAILED(hr))
        return false;

    // Criar vertex buffer da barrinha
    D3D11_BUFFER_DESC bdPaddle = {};
    bdPaddle.Usage = D3D11_USAGE_DEFAULT;
    bdPaddle.ByteWidth = sizeof(Vertex) * _countof(vertices);
    bdPaddle.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA initPaddle = {};
    initPaddle.pSysMem = vertices;

    hr = device->CreateBuffer(&bdPaddle, &initPaddle, &vertexBuffer);
    if (FAILED(hr))
        return false;

    // vertex buffer do shield
    D3D11_BUFFER_DESC bdShield = {};
    bdShield.Usage = D3D11_USAGE_DEFAULT;
    bdShield.ByteWidth = sizeof(Vertex) * (32 + 2) * 3; // número suficiente de vértices para o círculo
    bdShield.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    hr = device->CreateBuffer(&bdShield, nullptr, &forceFieldBuffer);
    if (FAILED(hr))
        return false;

    // vertex buffer do shield horizontal
    D3D11_BUFFER_DESC bdDashShield = {};
    bdDashShield.Usage = D3D11_USAGE_DEFAULT;
    bdDashShield.ByteWidth = sizeof(Vertex) * _countof(vertices);
    bdDashShield.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    hr = device->CreateBuffer(&bdDashShield, nullptr, &dashShieldBuffer);
    if (FAILED(hr))
        return false;

    // vertex buffer dos blocos
    D3D11_BUFFER_DESC bdBlock = {};
    bdBlock.Usage = D3D11_USAGE_DEFAULT;
    bdBlock.ByteWidth = sizeof(Vertex) * 6;
    bdBlock.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    hr = device->CreateBuffer(&bdBlock, nullptr, &blockVertexBuffer);
    if (FAILED(hr))
        return false;

    // Constant Buffer da cor dos blocos
    D3D11_BUFFER_DESC bdBlockColor = {};
    bdBlockColor.Usage = D3D11_USAGE_DEFAULT;
    bdBlockColor.ByteWidth = sizeof(XMFLOAT4);
    bdBlockColor.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    hr = device->CreateBuffer(&bdBlockColor, nullptr, &blockColorBuffer);
    if (FAILED(hr))
        return false;

    // vertex dos obstáculos
    D3D11_BUFFER_DESC bdObstacle = {};
    bdObstacle.Usage = D3D11_USAGE_DEFAULT;
    bdObstacle.ByteWidth = sizeof(Vertex) * 6;
    bdObstacle.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    hr = device->CreateBuffer(&bdObstacle, nullptr, &obstacleBuffer);
    if (FAILED(hr))
        return false;

    life = 3;

    // vertex dos tiros inimigos
    D3D11_BUFFER_DESC bdBullet = {};
    bdBullet.Usage = D3D11_USAGE_DEFAULT;
    bdBullet.ByteWidth = sizeof(Vertex) * 6;
    bdBullet.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    hr = device->CreateBuffer(&bdBullet, nullptr, &enemyBulletBuffer);
    if (FAILED(hr))
        return false;
    return true;
}

void UpdateGameplay()
{
    if (stageTransitionTimer > 0)
    {
        stageTransitionTimer--;
        return;
    }

    if (blocksRemaining <= 0)
    {
        blocksRemaining = 0;
        if (stage < 1)
        {
            stage++;
            projectiles.clear();
            enemyBullets.clear();
            InitStage(stage);
        }
        else if (stage >= 1)
        {
            currentState = GameState::STATE_START_MENU;
        }
    }

    static bool zWasPressed = false;
    static bool xWasPressed = false;

    if (GetAsyncKeyState('X') & 0x8000) // Deve priorizar o dash acima do Shield
    {
        if (!xWasPressed)
        {
            if (!forceFieldActive) // Shield não deve estar ativo
            {
                if (GetAsyncKeyState(VK_LEFT) & 0x8000)
                {
                    // dashActive = true;
                    dashDir = -1.0f; // esquerda
                    // dashTimer = 20;
                    // paddleHeight = paddleHeightDash;
                    ActivateDash();
                }
                else if (GetAsyncKeyState(VK_RIGHT) & 0x8000)
                {
                    // dashActive = true;
                    dashDir = 1.0f; // direita
                    // dashTimer = 20;
                    // paddleHeight = paddleHeightDash;
                    ActivateDash();
                }
                else if (!dashActive)
                {
                    // sem direção = shield
                    ActivateforceField();
                }
            }
        }
        xWasPressed = true;
    }
    else
    {
        xWasPressed = false;
    }
    /*if (GetAsyncKeyState('X') & 0x8000) // X para criar o shield
    {
        if (!xWasPressed && !forceFieldActive)
        {
            ActivateforceField();
        }
        xWasPressed = true;
    }
    else
    {
        xWasPressed = false;
    }*/
    if (GetAsyncKeyState('Z') & 0x8000) // Z para atirar
    {

        if (!zWasPressed)
        {
            Projectile p;
            p.x = paddleX;
            p.y = paddleY + paddleHeight + 0.003f;
            p.active = true;
            projectiles.push_back(p);
        }
        zWasPressed = true;
    }
    else
    {
        zWasPressed = false;
    }
    if (!forceFieldActive && !dashActive) // movimentação é bloqueada enquanto o escudo estiver ativo
    {
        if (GetAsyncKeyState(VK_LEFT) & 0x8000)
        {
            paddleX -= 0.01f; // velocidade para a esquerda
        }
        if (GetAsyncKeyState(VK_RIGHT) & 0x8000)
        {
            paddleX += 0.01f; // velocidade para a direita
        }
    }
    // Limite para não sair da tela
    if (paddleX - paddleWidth / 2 < -0.90f)
        paddleX = -0.90f + paddleWidth / 2;
    if (paddleX + paddleWidth / 2 > 0.90f)
        paddleX = 0.90f - paddleWidth / 2;

    UpdatePaddle();
    UpdateBall();
    UpdateProjectiles();
    UpdateBlocks();
    UpdateForceField();
    UpdateDash();
    UpdateIFrame();
    UpdateEnemyBullet();
}

void RenderGameplay()
{

    float clearColor[4] = {0.2f, 0.2f, 0.6f, 1.0f}; // cor azulada
    deviceContext->ClearRenderTargetView(renderTargetView, clearColor);

    // Configurar pipeline
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    deviceContext->IASetInputLayout(inputLayout);
    deviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
    deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    deviceContext->VSSetShader(vertexShader, nullptr, 0);
    deviceContext->PSSetShader(pixelShader, nullptr, 0);

    // Desenhar a barrinha
    if (paddleVisible)
    {
        deviceContext->PSSetShader(pixelShaderPaddle, nullptr, 0);
        deviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
        deviceContext->Draw(6, 0);
    }

    // Desenhar bolinha
    deviceContext->PSSetShader(pixelShaderBall, nullptr, 0);
    deviceContext->IASetVertexBuffers(0, 1, &ballVertexBuffer, &stride, &offset);
    deviceContext->Draw(6, 0);

    // Desenhar projétil
    deviceContext->PSSetShader(pixelShaderProjectile, nullptr, 0);
    for (auto &p : projectiles)
    {
        if (!p.active)
            continue;

        Vertex projVertices[] =
            {
                {p.x - (projectileSize * 0.8f), p.y + (projectileSize * 0.8f), 0.0f},
                {p.x - (projectileSize * 0.8f), p.y - (projectileSize * 0.8f), 0.0f},
                {p.x + (projectileSize * 0.8f), p.y - (projectileSize * 0.8f), 0.0f},

                {p.x - (projectileSize * 0.8f), p.y + (projectileSize * 0.8f), 0.0f},
                {p.x + (projectileSize * 0.8f), p.y - (projectileSize * 0.8f), 0.0f},
                {p.x + (projectileSize * 0.8f), p.y + (projectileSize * 0.8f), 0.0f},
            };

        deviceContext->UpdateSubresource(projectileBuffer, 0, nullptr, projVertices, 0, 0);
        deviceContext->IASetVertexBuffers(0, 1, &projectileBuffer, &stride, &offset);
        deviceContext->Draw(6, 0);
    }

    // Desenhar obstáculos
    deviceContext->PSSetShader(pixelShaderObstacle, nullptr, 0);
    deviceContext->IASetVertexBuffers(0, 1, &obstacleBuffer, &stride, &offset);

    for (auto &obstacle : obstacles)
    {
        if (!obstacle.active)
            continue;

        Vertex vertices[] = {
            {obstacle.x - obstacle.width / 2, obstacle.y + obstacle.height, 0.0f},
            {obstacle.x - obstacle.width / 2, obstacle.y, 0.0f},
            {obstacle.x + obstacle.width / 2, obstacle.y, 0.0f},
            {obstacle.x - obstacle.width / 2, obstacle.y + obstacle.height, 0.0f},
            {obstacle.x + obstacle.width / 2, obstacle.y, 0.0f},
            {obstacle.x + obstacle.width / 2, obstacle.y + obstacle.height, 0.0f},
        };
        deviceContext->UpdateSubresource(obstacleBuffer, 0, nullptr, vertices, 0, 0);
        deviceContext->Draw(6, 0);
    }

    // Desenhar bloco(s)
    deviceContext->PSSetShader(pixelShaderBlock, nullptr, 0);
    deviceContext->IASetVertexBuffers(0, 1, &blockVertexBuffer, &stride, &offset);

    for (auto &block : blocks)
    {
        if (!block.active)
            continue;

        XMFLOAT4 color;

        if (block.hits >= 3)
            color = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
        else if (block.hits == 2)
            color = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
        else if (block.hits == 1)
            color = XMFLOAT4(0.4f, 0.4f, 0.4f, 1.0f);

        deviceContext->PSSetConstantBuffers(0, 1, &blockColorBuffer);
        deviceContext->UpdateSubresource(blockColorBuffer, 0, nullptr, &color, 0, 0);

        Vertex vertices[] = {
            {block.x - block.width / 2, block.y + block.height, 0.0f},
            {block.x - block.width / 2, block.y, 0.0f},
            {block.x + block.width / 2, block.y, 0.0f},
            {block.x - block.width / 2, block.y + block.height, 0.0f},
            {block.x + block.width / 2, block.y, 0.0f},
            {block.x + block.width / 2, block.y + block.height, 0.0f},
        };
        deviceContext->UpdateSubresource(blockVertexBuffer, 0, nullptr, vertices, 0, 0);
        deviceContext->Draw(6, 0);
    }

    // desenhar tiros inimigos
    deviceContext->PSSetShader(pixelShaderEnemyBullet, nullptr, 0);

    for (auto &bullet : enemyBullets)
    {
        if (!bullet.active)
            continue;

        Vertex verticesBullet[] = {
            {bullet.x - bullet.size, bullet.y + bullet.size, 0.0f},
            {bullet.x - bullet.size, bullet.y - bullet.size, 0.0f},
            {bullet.x + bullet.size, bullet.y - bullet.size, 0.0f},

            {bullet.x - bullet.size, bullet.y + bullet.size, 0.0f},
            {bullet.x + bullet.size, bullet.y - bullet.size, 0.0f},
            {bullet.x + bullet.size, bullet.y + bullet.size, 0.0f},
        };
        UINT stride = sizeof(Vertex);
        UINT offset = 0;
        deviceContext->IASetVertexBuffers(0, 1, &enemyBulletBuffer, &stride, &offset);
        deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        deviceContext->UpdateSubresource(enemyBulletBuffer, 0, nullptr, verticesBullet, 0, 0);
        deviceContext->Draw(6, 0);
    }

    // desenhar shield
    if (forceFieldActive)
    {
        deviceContext->PSSetShader(pixelShaderProjectile, nullptr, 0);

        const int segments = 32; // Segmentos para formar o círculo

        std::vector<Vertex> circleVerts;

        // Vértice central
        circleVerts.push_back({forceFieldX, forceFieldY, 0.0f});

        for (int i = 0; i <= segments; i++)
        {
            float theta = (2 * 3.14159265f * i) / segments; // Círculo inteiro pois o PI está sendo multiplicado por 2, para um meio-círculo não multiplicar por 2
            float x = forceFieldX + cosf(theta) * forceFieldRadius;
            float y = forceFieldY + sinf(theta) * forceFieldRadius;
            circleVerts.push_back({x, y, 0.0f});
        }

        // Loop para formar o círculo
        std::vector<Vertex> fanVerts;
        for (int i = 1; i < circleVerts.size() - 1; i++) // .size() -1 para o último passo não ficar OOB
        {
            fanVerts.push_back(circleVerts[0]);     // centro
            fanVerts.push_back(circleVerts[i]);     // ponto atual na circunferência
            fanVerts.push_back(circleVerts[i + 1]); // próximo ponto
        }

        deviceContext->UpdateSubresource(forceFieldBuffer, 0, nullptr, fanVerts.data(), 0, 0);
        UINT stride = sizeof(Vertex);
        UINT offset = 0;
        deviceContext->IASetVertexBuffers(0, 1, &forceFieldBuffer, &stride, &offset);
        deviceContext->Draw(static_cast<UINT>(fanVerts.size()), 0);
    }

    if (dashActive)
    {
        deviceContext->PSSetShader(pixelShaderProjectile, nullptr, 0); // renderiza o treco branco

        float shieldWidth = 0.25f;  // largura, a ideia é que esse tamanho deve simular a barrinha "deitada" no chão
        float shieldHeight = 0.15f; // Altura, um pouco maior do que a barrinha
        float shieldY = paddleY;    // mesma altura do "pé" da barrinha, ajustar conforme necessário

        Vertex dashShieldVerts[] = {
            // começarei com um retângulo pq já tenho a base dele no Paddle em si, depois penso numa forma de colocar um triângulo que rotaciona dependendo da direção do dash
            {paddleX - shieldWidth / 2, shieldY + shieldHeight, 0.0f}, // esquerda cima
            {paddleX - shieldWidth / 2, shieldY, 0.0f},                // esquerda baixo
            {paddleX + shieldWidth / 2, shieldY, 0.0f},                // direita baixo

            // Triângulo 2
            {paddleX - shieldWidth / 2, shieldY + shieldHeight, 0.0f}, // esquerda cima
            {paddleX + shieldWidth / 2, shieldY, 0.0f},                // direita baixo
            {paddleX + shieldWidth / 2, shieldY + shieldHeight, 0.0f}  // direita cima
        };

        deviceContext->UpdateSubresource(dashShieldBuffer, 0, nullptr, dashShieldVerts, 0, 0);
        UINT stride = sizeof(Vertex);
        UINT offset = 0;
        deviceContext->IASetVertexBuffers(0, 1, &dashShieldBuffer, &stride, &offset);
        deviceContext->Draw(6, 0);
    }

    swapChain->Present(1, 0);
}

// Render loop
void RenderFrame()
{

    if (currentState == STATE_START_MENU)
    {
        RenderMenu();
    }
    else if (currentState == STATE_DIFFICULTY_SELECT)
    {
        RenderDiffSelect();
    }
    else if (currentState == STATE_GAMEPLAY)
    {
        RenderGameplay();
    }
}

// Libera DirectX
void CleanD3D()
{
    if (swapChain)
        swapChain->Release();
    if (renderTargetView)
        renderTargetView->Release();
    if (deviceContext)
        deviceContext->Release();
    if (device)
        device->Release();
    if (vertexBuffer)
        vertexBuffer->Release();
    if (vertexShader)
        vertexShader->Release();
    if (pixelShader)
        pixelShader->Release();
    if (inputLayout)
        inputLayout->Release();
    if (rasterState)
        rasterState->Release();
    if (pixelShaderBlock)
        pixelShaderBlock->Release();
    if (blockColorBuffer)
        blockColorBuffer->Release();
    if (blockVertexBuffer)
        blockVertexBuffer->Release();
    if (ballVertexBuffer)
        ballVertexBuffer->Release();
    if (projectileBuffer)
        projectileBuffer->Release();
    if (forceFieldBuffer)
        forceFieldBuffer->Release();
    if (dashShieldBuffer)
        dashShieldBuffer->Release();
    if (obstacleBuffer)
        obstacleBuffer->Release();
    if (enemyBulletBuffer)
        enemyBulletBuffer->Release();
}

// Função de janela
LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

// WinMain
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    WNDCLASSEX wc = {sizeof(WNDCLASSEX), CS_CLASSDC, WindowProc, 0L, 0L,
                     GetModuleHandle(NULL), NULL, NULL, NULL, NULL,
                     L"TorrouDX", NULL};
    RegisterClassEx(&wc);

    HWND hWnd = CreateWindow(L"TorrouDX", L"Torrou 1 - Aquele jogo que Touhou minha paciencia",
                             WS_OVERLAPPEDWINDOW, 100, 100, 800, 600,
                             NULL, NULL, wc.hInstance, NULL);

    ShowWindow(hWnd, nCmdShow);

    g_hWnd = hWnd;
    // Inicializa DirectX
    if (!InitD3D(hWnd))
        return 0;

    // Loop de mensagens
    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            switch (currentState)
            {
            case GameState::STATE_START_MENU:
                UpdateMenu();
                RenderMenu();
                swapChain->Present(1, 0); // Apresenta a tela
                break;

            case GameState::STATE_DIFFICULTY_SELECT:
                UpdateDiffSelect();
                RenderDiffSelect();
                swapChain->Present(1, 0); // Apresenta a tela
                break;

            case GameState::STATE_PAUSE:
                // UpdatePause();
                // RenderPause();
                swapChain->Present(1, 0); // Apresenta a tela
                break;

            case GameState::STATE_GAMEPLAY:
                UpdateGameplay();
                RenderFrame();
                DrawScore(g_hWnd, score);
                DrawBlocksRemaining(g_hWnd, blocksRemaining);
                DrawLives(g_hWnd, life);
                DrawStage(g_hWnd, stage);
                break;
            }
        }
    }

    CleanD3D();
    UnregisterClass(L"TorrouDX", wc.hInstance);
    return 0;
}