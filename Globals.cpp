#include "Globals.h"

// DirectX
HWND                    g_hWnd = nullptr;
IDXGISwapChain* swapChain = nullptr;
ID3D11Device* device = nullptr;
ID3D11DeviceContext* deviceContext = nullptr;
ID3D11RenderTargetView* renderTargetView = nullptr;
ID3D11VertexShader* vertexShader = nullptr;
ID3D11InputLayout* inputLayout = nullptr;
ID3D11RasterizerState* rasterState = nullptr;

ID3D11VertexShader* vertexShaderTextured = nullptr;
ID3D11PixelShader* pixelShaderTextured = nullptr;
ID3D11Buffer* texturedVertexBuffer = nullptr;
ID3D11SamplerState* samplerState = nullptr;

ID3D11Buffer* obstacleBuffer = nullptr;
ID3D11Buffer* vertexBuffer = nullptr;
ID3D11Buffer* blockVertexBuffer = nullptr;
ID3D11Buffer* ballVertexBuffer = nullptr;
ID3D11Buffer* projectileBuffer = nullptr;
ID3D11Buffer* forceFieldBuffer = nullptr;
ID3D11Buffer* dashShieldBuffer = nullptr;
ID3D11Buffer* blockColorBuffer = nullptr;
ID3D11Buffer* enemyBulletBuffer = nullptr;

ID3D11PixelShader* pixelShaderObstacle = nullptr;
ID3D11PixelShader* pixelShader = nullptr;
ID3D11PixelShader* pixelShaderBlock = nullptr;
ID3D11PixelShader* pixelShaderPaddle = nullptr;
ID3D11PixelShader* pixelShaderBall = nullptr;
ID3D11PixelShader* pixelShaderProjectile = nullptr;
ID3D11PixelShader* pixelShaderEnemyBullet = nullptr;
ID3D11PixelShader* pixelShaderMenu = nullptr;

ID3D11ShaderResourceView* editorObstacleTexture = nullptr;
ID3D11ShaderResourceView* menuBgTexture = nullptr;
ID3D11ShaderResourceView* menuTitleTexture = nullptr;
ID3D11ShaderResourceView* menuSelectorTexture = nullptr;
ID3D11ShaderResourceView* stageBgTexture = nullptr;
ID3D11ShaderResourceView* editorPlayerTexture = nullptr;
ID3D11ShaderResourceView* editorBallTexture = nullptr;
ID3D11ShaderResourceView* editorBlockTexture = nullptr;
ID3D11ShaderResourceView* editorBossTexture = nullptr;
ID3D11InputLayout* inputLayoutTextured = nullptr;

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
int  damageEffectTimer = 0;
int  damageBlinkCounter = 0;

bool  projectileActive = false;
float projectileX = 0.0f;
float projectileY = 0.0f;
float projectileSize = 0.02f;
float projectileSpeed = 0.05f;

float       paddleX = 0.0f;
const float paddleY = -0.75f;
float       paddleWidth = 0.08f;
float       paddleHeight = 0.20f;
float       paddleHeightNormal = 0.20f;
float       paddleHeightDash = 0.08f;
bool        paddleVisible = true;
float       paddleEditWidth = 0.08f;
float       paddleEditHeight = 0.20f;
float       paddleEditMoveSpeed = 0.01f;

float ballX = 0.75f;
float ballY = -0.5f;
float ballVelX = 0.000001f;
float ballVelY = 0.02f;
float ballSize = 0.03f;

bool  forceFieldActive = false;
float forceFieldRadius = 0.20f;
float forceFieldTimer = 0.00f;
float forceFieldY = 0.00f;
float forceFieldX = 0.00f;

bool  dashActive = false;
int   dashTimer = 0;
float dashDir = 0.0f;
float dashSpeed = 0.025f;

std::vector<Projectile>  projectiles;
std::vector<Block>       blocks;
std::vector<Obstacle>    obstacles;
std::vector<EnemyBullet> enemyBullets;
std::vector<DroppedItem> droppedItems;

// ==========================================
// CONFIGS DO EDITOR — defaults
// ==========================================

ObstacleConfig editorObstacleConfig = {
	0.3f, 0.02f,
	1.0f, 1.0f, 0.0f, 1.0f,
	false,
	"obstacle_default", ""
};
char editorObstacleNameInput[64] = "obstacle_default";
char editorObstacleWidthInput[32] = "0.3";
char editorObstacleHeightInput[32] = "0.02";
char editorObstacleTexturePathInput[256] = "";

// Boss — inicializado com zeros; campos preenchidos no editor
BossConfig editorBossConfig = {};

// Block/Enemy template
BlockConfig editorBlockConfig = {
	"block_default", "",
	0.15f, 0.06f,
	0.4f, 0.4f, 0.8f, 1.0f,
	false,              // useTexture
	1,                  // maxHits
	0, 1, 0.006f, 60,   // bulletPattern, count, speed, interval
	false,              // invulnerable
	MOV_NONE, 0.0f, 0.3f, 0.2f, // movType, speed, amplitude, radius
	{ true, { true, false, false, false }, { 1.0f, 0.0f, 0.0f, 0.0f } }
};

// Player sprites
PlayerSpriteConfig editorPlayerConfig = {
	"", "", "", "", "", "", "",
	{ DMG_BLINK, 180, 4, 1.0f, 0.3f, 0.3f, 1.0f, "" }
};

// Ball sprite
BallSpriteConfig editorBallConfig = { "", 0.02f };

// Bomba
BombConfig editorBombConfig = {
	"bomb_default", "",
	0,       // habilidade
	0.4f,    // radius
	1.0f,    // damage
	120      // duration
};

// Menu
MenuConfig editorMenuConfig = {
	0.05f, 0.05f, 0.10f, 1.0f,
	0.30f, 0.30f, 0.80f, 1.0f,
	1.00f, 1.00f, 0.30f, 1.0f,
	"", "", "",
	0.0f, 0.75f, 0.8f, 0.15f
};

// Stage start (retro-compatibilidade)
StageStartConfig editorStageConfig = { 0, 0.0f, -0.5f, 0.000001f, 0.02f };

// Stage editor
StageEditorConfig editorStageEditorConfig = {
	STAGE_NORMAL,
	0.05f, 0.05f, 0.15f, 1.0f,
	false, ""
};

// Objetos posicionados no stage editor
std::vector<PlacedObject> stageObjects;

// Preview e demo
bool  editorDemoActive = false;
float editorDemoBossHPPct = 1.0f;