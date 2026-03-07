#include "Globals.h"

// DirectX
HWND g_hWnd = nullptr;
IDXGISwapChain* swapChain = nullptr;
ID3D11Device* device = nullptr;
ID3D11DeviceContext* deviceContext = nullptr;
ID3D11RenderTargetView* renderTargetView = nullptr;
ID3D11VertexShader* vertexShader = nullptr;
ID3D11InputLayout* inputLayout = nullptr;
ID3D11RasterizerState* rasterState = nullptr;

// Buffers
ID3D11Buffer* obstacleBuffer = nullptr;
ID3D11Buffer* vertexBuffer = nullptr;
ID3D11Buffer* blockVertexBuffer = nullptr;
ID3D11Buffer* ballVertexBuffer = nullptr;
ID3D11Buffer* projectileBuffer = nullptr;
ID3D11Buffer* forceFieldBuffer = nullptr;
ID3D11Buffer* dashShieldBuffer = nullptr;
ID3D11Buffer* blockColorBuffer = nullptr;
ID3D11Buffer* enemyBulletBuffer = nullptr;

// Shaders
ID3D11PixelShader* pixelShaderObstacle = nullptr;
ID3D11PixelShader* pixelShader = nullptr;
ID3D11PixelShader* pixelShaderBlock = nullptr;
ID3D11PixelShader* pixelShaderPaddle = nullptr;
ID3D11PixelShader* pixelShaderBall = nullptr;
ID3D11PixelShader* pixelShaderProjectile = nullptr;
ID3D11PixelShader* pixelShaderEnemyBullet = nullptr;
ID3D11PixelShader* pixelShaderMenu = nullptr;

// Menu e Estado do Jogo
GameState currentState = STATE_START_MENU;
EditorMode currentEditorMode = EDITOR_MODE_PLAYER;
int selectedMenuIndex = 0;
const char* mainMenuItems[] = { "Start", "Options", "Close" };
const int mainMenuCount = 3;
int difficulty = 0;
const int difficultyCount = 4;
bool g_wasUpPressed = false;
bool g_wasDownPressed = false;
bool g_wasZPressed = false;
bool modoEditor = false;

// Gameplay Variáveis
int balasPorBloco = 1;
int bulletPattern = 0;
bool timeout = false;
int stage = 0;
int stageTransitionTimer = 0;
int life = 0;
int cfgLife = 3;
int timer = 0;
int bossHP = 0;
int blocksRemaining = 0;
int timeCount = 0;
int score = 0;
int highScore = 0;
int combo = 0;
bool iFrame = false;
int iFrameTimer = 0;

// Projéteis
bool projectileActive = false;
float projectileX = 0.0f;
float projectileY = 0.0f;
float projectileSize = 0.02f;
float projectileSpeed = 0.05f;

// Barrinha (Paddle)
float paddleX = 0.0f;
const float paddleY = -0.75f;
float paddleWidth = 0.08f;
float paddleHeight = 0.20f;
float paddleHeightNormal = 0.20f;
float paddleHeightDash = 0.08f;
bool paddleVisible = true;

// Bola
float ballX = 0.75f;
float ballY = -0.5f;
float ballVelX = 0.000001f;
float ballVelY = 0.02f;
float ballSize = 0.03f;

// Shield
bool forceFieldActive = false;
float forceFieldRadius = 0.20f;
float forceFieldTimer = 0.00f;
float forceFieldY = 0.00f;
float forceFieldX = 0.00f;

// Dash
bool dashActive = false;
int dashTimer = 0;
float dashDir = 0.0f;
float dashSpeed = 0.025f;

// Vetores
std::vector<Projectile> projectiles;
std::vector<Block> blocks;
std::vector<Obstacle> obstacles;
std::vector<EnemyBullet> enemyBullets;

// Configs
ObstacleConfig editorObstacleConfig = { 0.3f, 0.02f, 1.0f, 1.0f, 0.0f, 1.0f, "obstacle_default"};

char editorObstacleNameInput[64] = "obstacle_default";
char editorObstacleWidthInput[32] = "0.3";
char editorObstacleHeightInput[32] = "0.02";