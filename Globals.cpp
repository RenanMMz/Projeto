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
std::unique_ptr<DirectX::SpriteBatch> spriteBatch = nullptr;

// Shaders texturizados
ID3D11VertexShader* vertexShaderTextured = nullptr;
ID3D11PixelShader* pixelShaderTextured = nullptr;
ID3D11Buffer* texturedVertexBuffer = nullptr;
ID3D11SamplerState* samplerState = nullptr;

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
ID3D11Buffer* paddleVertexBuffer = nullptr;
ID3D11Buffer* portalBuffer = nullptr;

// Pixel Shaders
ID3D11PixelShader* pixelShaderObstacle = nullptr;
ID3D11PixelShader* pixelShader = nullptr;
ID3D11PixelShader* pixelShaderBlock = nullptr;
ID3D11PixelShader* pixelShaderPaddle = nullptr;
ID3D11PixelShader* pixelShaderBall = nullptr;
ID3D11PixelShader* pixelShaderProjectile = nullptr;
ID3D11PixelShader* pixelShaderEnemyBullet = nullptr;
ID3D11PixelShader* pixelShaderMenu = nullptr;

// Texturas do editor
ID3D11ShaderResourceView* editorObstacleTexture = nullptr;
ID3D11ShaderResourceView* menuBgTexture = nullptr;
ID3D11ShaderResourceView* menuTitleTexture = nullptr;
ID3D11ShaderResourceView* menuSelectorTexture = nullptr;
ID3D11InputLayout* inputLayoutTextured = nullptr;

// Menu e Estado do Jogo
GameState  currentState = STATE_START_MENU;
EditorMode currentEditorMode = EDITOR_MODE_PLAYER;
int selectedMenuIndex = 0;
const char* mainMenuItems[] = { "Start", "Options", "Close" };
const int   mainMenuCount = 3;
int  difficulty = 0;
const int difficultyCount = 4;
bool g_wasUpPressed = false;
bool g_wasDownPressed = false;
bool g_wasZPressed = false;
bool modoEditor = false;

// Gameplay
int  balasPorBloco = 1;
int  bulletPattern = 0;
bool timeout = false;
int  stage = 0;
int  stageTransitionTimer = 0;
int  life = 0;
int  cfgLife = 3;
int  timer = 0;
int  bossHP = 0;
int  blocksRemaining = 0;
int  timeCount = 0;
int  score = 0;
int  highScore = 0;
int  combo = 0;
bool iFrame = false;
int  iFrameTimer = 0;

// Portal
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> portalTex = nullptr;

bool ballInTransit = false;
int portalTimer = 0;
float portalExitX = 0.0f;
float portalExitY = 0.0f;
float portalExitVelX = 0.0f;
float portalExitVelY = 0.0f;
float portalEntranceX = 0.0f;
float portalEntranceY = 0.0f;
float portalTexWidth = 0.0f;
float portalTexHeight = 0.0f;
int portalCooldown = 0;

// Projéteis
bool  projectileActive = false;
float projectileX = 0.0f;
float projectileY = 0.0f;
float projectileSize = 0.02f;
float projectileSpeed = 0.05f;

// Paddle
float       paddleX = 0.0f;
const float paddleY = -0.75f;
float       paddleWidth = 0.08f;
float       paddleHeight = 0.20f;
float       paddleHeightNormal = 0.20f;
float       paddleHeightDash = 0.08f;
bool        paddleVisible = true;

// Bola
float ballX = 0.75f;
float ballY = -0.5f;
float ballVelX = 0.0f;
float ballVelY = 0.02f;
float ballSize = 0.03f;

// Shield
bool  forceFieldActive = false;
float forceFieldRadius = 0.20f;
float forceFieldTimer = 0.00f;
float forceFieldY = 0.00f;
float forceFieldX = 0.00f;

// Dash
bool  dashActive = false;
int   dashTimer = 0;
float dashDir = 0.0f;
float dashSpeed = 0.025f;

// Vetores
std::vector<Projectile>  projectiles;
std::vector<Block>       blocks;
std::vector<Obstacle>    obstacles;
std::vector<EnemyBullet> enemyBullets;
std::vector<Portal> portals;

// ==========================================
// CONFIGS DO EDITOR
// ==========================================

// Obstáculo (existente)
ObstacleConfig editorObstacleConfig = { 0.3f, 0.02f, 1.0f, 1.0f, 0.0f, 1.0f, "obstacle_default", "" };
char editorObstacleNameInput[64] = "obstacle_default";
char editorObstacleWidthInput[32] = "0.3";
char editorObstacleHeightInput[32] = "0.02";
char editorObstacleTexturePathInput[256] = "";

// Boss
BossConfig editorBossConfig = {};

// Block/Enemy template
BlockConfig editorBlockConfig = {
	"block_default", "",
	0.15f, 0.06f,
	0.4f, 0.4f, 0.8f, 1.0f,
	1, 0, 1,
	{ false, 0, 0.0f }
};

// Player sprites
PlayerSpriteConfig editorPlayerConfig = { "", "", "", "" };

// Ball sprite
BallSpriteConfig editorBallConfig = { "" };

// Bomba/Especial
BombConfig editorBombConfig = {
	"bomb_default", "",
	0,          // type: habilidade
	0.4f,       // radius
	1.0f,       // damage
	120,        // duration (frames)
	2,          // bulletPattern: radial
	8,          // bulletCount
	0.006f      // bulletSpeed
};

// Menu
MenuConfig editorMenuConfig = {
	0.05f, 0.05f, 0.10f, 1.0f,   // bg
	0.30f, 0.30f, 0.80f, 1.0f,   // button
	1.00f, 1.00f, 0.30f, 1.0f,   // selected
	"", "", "",                   // bgTexture, titleTexture, selectorTexture
	0.0f, 0.75f, 0.8f, 0.15f     // logoX, logoY, logoWidth, logoHeight
};

// Stage start
StageStartConfig editorStageConfig = {
	0,
	0.0f, -0.5f,
	0.000001f, 0.02f
};