#pragma once
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <vector>
#include <DirectXMath.h>
#include <fstream>
#include <sstream>
#include <string>

#include <DDSTextureLoader.h>
#include <WICTextureLoader.h>
#include <SpriteBatch.h>
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "D3DCompiler.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

using namespace DirectX;

// ==========================================
// ENUMS
// ==========================================

enum EditorMode {
	EDITOR_MODE_PLAYER,
	EDITOR_MODE_BALL,
	EDITOR_MODE_STAGE,
	EDITOR_MODE_OBSTACLE,
	EDITOR_MODE_ENEMY,
	EDITOR_MODE_BOSS,
	EDITOR_MODE_MENU,
	EDITOR_MODE_PORTAL
};

enum GameState {
	STATE_START_MENU,
	STATE_DIFFICULTY_SELECT,
	STATE_OPTIONS,
	STATE_GAMEPLAY,
	STATE_GAMEOVER,
	STATE_PAUSE,
	STATE_EDITOR
};

enum EnemyMovType {
	MOV_NONE = 0,
	MOV_VERTICAL = 1,
	MOV_HORIZONTAL = 2,
	MOV_CIRCULAR = 3
};

enum DamageEffectType {
	DMG_BLINK = 0,
	DMG_TINT = 1,
	DMG_SPRITE = 2
};

// ==========================================
// STRUCTS DE GAMEPLAY
// ==========================================

struct Projectile {
	float x, y; bool active;
};

struct DroppedItem {
	float x, y;
	int   type;    // 0=vida, 1=shield, 2=reservado, 3=pontos
	bool  active;
};

struct Block {
	float x, y, width, height;
	bool  active;
	int   hits;
	// Tiro
	int   bulletPattern;
	int   bulletCount;
	float bulletSpeed;
	int   shootIntervalFrames;
	int   shootTimer;
	// Invulnerabilidade
	bool  invulnerable;
	// iFrame pos-hit
	bool  iFrameBlock;
	int   iFrameBlockTimer;
	// Aparencia
	bool  useTexture;
	ID3D11ShaderResourceView* textureSRV = nullptr;
	float colorR, colorG, colorB, colorA;
	// Movimentacao
	EnemyMovType movType;
	float movSpeed;
	float movAmplitude;
	float movRadius;
	float movAngle;
	float movOriginX;
	float movOriginY;
	float movDir;
	// Drops
	bool  hasDrop;
	float dropWeights[4]; // pesos: vida, shield, reservado, pontos
};

struct Obstacle {
	float x, y, width, height;
	bool  active;
	bool  useTexture;
	ID3D11ShaderResourceView* textureSRV = nullptr;
	float colorR, colorG, colorB, colorA;
};

struct Vertex {
	float x, y, z;
};
struct VertexMenu {
	float x, y, z; float r, g, b, a;
};

struct EnemyBullet {
	float x, y, vx, vy, size; bool active;
};

// ==========================================
// STRUCTS DO EDITOR
// ==========================================

struct ObstacleConfig {
	float width, height;
	float colorR, colorG, colorB, colorA;
	bool  useTexture;
	char  name[64];
	char  texturePath[256];
};

struct ColorConstantBuffer {
	DirectX::XMFLOAT4 color;
};

struct DropTable {
	bool  hasDrop;
	bool  entryEnabled[4];   // 0=vida,1=shield,2=reservado,3=pontos
	float entryWeight[4];
};

struct DamageEffectConfig {
	DamageEffectType type;
	int   durationFrames;
	int   blinkIntervalFrames;
	float tintR, tintG, tintB, tintA;
	char  damageSpritePath[256];
};

struct BlockConfig {
	char  name[64];
	char  texturePath[256];
	float width, height;
	float colorR, colorG, colorB, colorA;
	bool  useTexture;
	int   maxHits;
	int   bulletPattern;
	int   bulletCount;
	float bulletSpeed;
	int   shootIntervalFrames;
	bool  invulnerable;
	EnemyMovType movType;
	float movSpeed;
	float movAmplitude;
	float movRadius;
	DropTable dropTable;
};

// ==========================================
// BOSS SCRIPTING
// ==========================================

enum BossActionType {
	BOSS_ACT_MOVE_TO,
	BOSS_ACT_TELEPORT,
	BOSS_ACT_SHOOT_TIMED,
	BOSS_ACT_SHOOT_FIXED_PTS,
	BOSS_ACT_CHARGE_PLAYER,
	BOSS_ACT_WAIT,
	BOSS_ACT_SPAWN_MINION,
	BOSS_ACT_CHANGE_SPRITE,
	BOSS_ACT_INVINCIBLE,
};

struct BossAction {
	BossActionType type;
	float targetX, targetY, speed;
	int   bulletPattern, bulletCount;
	float bulletSpeed, duration;
	int   fixedPointCount;
	float fixedPtsX[8], fixedPtsY[8];
	float spawnX, spawnY;
	int   minionPatternIndex;
	char  spritePath[256];
	bool  invincibleOn;
};

#define BOSS_SCRIPT_MAX_ACTIONS 32

struct BossScript {
	BossAction actions[BOSS_SCRIPT_MAX_ACTIONS];
	int        actionCount;
	int        loopFromStep;
};

struct BossHPPhase {
	float      hpThresholdPct;
	BossScript script;
};

struct FamiliarConfig {
	float relOffsetX, relOffsetY;
	float orbitRadius, orbitSpeed;
	int   bulletPattern, bulletCount;
	float bulletSpeed, shootIntervalSec;
	char  texturePath[256];
};

struct MultipartNode {
	char       texturePath[256];
	float      startX, startY;
	BossScript script;
};

enum BossArchetype {
	BOSS_ARCH_SCRIPTED,
	BOSS_ARCH_STATIC,
	BOSS_ARCH_STATIC_FAMILIARS,
	BOSS_ARCH_MULTIPART,
};

#define BOSS_MAX_HP_PHASES  4
#define BOSS_MAX_FAMILIARS  8
#define BOSS_MAX_NODES      8

struct BossConfig {
	char           name[64];
	char           texturePath[256];
	int            maxHP;
	float          width, height;
	BossArchetype  archetype;
	float          startX, startY;
	BossHPPhase    hpPhases[BOSS_MAX_HP_PHASES];
	int            hpPhaseCount;
	FamiliarConfig familiars[BOSS_MAX_FAMILIARS];
	int            familiarCount;
	MultipartNode  nodes[BOSS_MAX_NODES];
	int            nodeCount;
};

// ==========================================
// BOSS RUNTIME STATE
// ==========================================

struct BossState {
	bool   active;
	float  x, y;
	int    hp;
	int    currentPhase;   // index into config.hpPhases
	int    actionIdx;      // current step in the active script
	float  actionTimer;   // frames elapsed in the current action
	bool   invincible;
	BossConfig config;     // full copy loaded at stage start
	// Runtime SRVs (not owned; lives in the texture cache)
	ID3D11ShaderResourceView* textureSRV;
	ID3D11ShaderResourceView* familiarSRVs[BOSS_MAX_FAMILIARS];
	ID3D11ShaderResourceView* nodeSRVs[BOSS_MAX_NODES];
	// Familiar orbit
	float  familiarAngles[BOSS_MAX_FAMILIARS];
	float  familiarShootTimers[BOSS_MAX_FAMILIARS];
	// Multipart node positions and script state
	float  nodeX[BOSS_MAX_NODES];
	float  nodeY[BOSS_MAX_NODES];
	bool   nodeActive[BOSS_MAX_NODES];
	int    nodeActionIdx[BOSS_MAX_NODES];
	float  nodeActionTimer[BOSS_MAX_NODES];
};

// ==========================================
// SPRITES DO JOGADOR
// ==========================================

struct PlayerSpriteConfig {
	char texturePath[256];
	char runRightTexturePath[256];
	char runLeftTexturePath[256];
	char projectileTexturePath[256];
	char shieldTexturePath[256];
	char dashRightTexturePath[256];
	char dashLeftTexturePath[256];
	DamageEffectConfig damageEffect;
};

struct BallSpriteConfig {
	char  texturePath[256];
	float initialSpeed;
};

struct MenuConfig {
	float bgColorR, bgColorG, bgColorB, bgColorA;
	float buttonColorR, buttonColorG, buttonColorB, buttonColorA;
	float selectedColorR, selectedColorG, selectedColorB, selectedColorA;
	char  bgTexturePath[256];
	char  titleTexturePath[256];
	char  selectorTexturePath[256];
	float logoX, logoY;
	float logoWidth, logoHeight;
};

// ==========================================
// STAGE EDITOR
// ==========================================

enum StageMode {
	STAGE_NORMAL = 0, STAGE_BOSS = 1
};
enum PlacedObjectType {
	PLACED_BLOCK = 0, PLACED_OBSTACLE = 1, PLACED_BOSS = 2, PLACED_BALLSPAWN = 3
};

struct PlacedObject {
	PlacedObjectType type;
	float x, y;
	char  configFile[256];
	char  displayName[64];
	bool  selected;
};

struct StageEditorConfig {
	StageMode mode;
	float bgColorR, bgColorG, bgColorB, bgColorA;
	bool  useTextureBg;
	char  bgTexturePath[256];
};

struct Portal
{
	float x, y, width, height;
	bool active;
	ID3D11ShaderResourceView* textureSRV = nullptr;
};

struct StageStartConfig {
	int   stageNumber;
	float ballStartX, ballStartY;
	float ballStartVelX, ballStartVelY;
};


// ==========================================
// GAME PROJECT CONFIG
// ==========================================

#define PROJECT_MAX_STAGES 16

struct GameProjectConfig {
	char playerConfigPath[MAX_PATH];
	char ballConfigPath[MAX_PATH];
	char menuConfigPath[MAX_PATH];
	int  stageCount;
	char stagePaths[PROJECT_MAX_STAGES][MAX_PATH]; // paths para os JSONs de stage
};

// ==========================================
// VARIAVEIS GLOBAIS (extern)
// ==========================================

extern HWND                       g_hWnd;
extern int                        g_currentWidth;
extern int                        g_currentHeight;
extern IDXGISwapChain* swapChain;
extern ID3D11Device* device;
extern ID3D11DeviceContext* deviceContext;
extern ID3D11RenderTargetView* renderTargetView;
extern ID3D11VertexShader* vertexShader;
extern ID3D11InputLayout* inputLayout;
extern ID3D11RasterizerState* rasterState;

extern ID3D11VertexShader* vertexShaderTextured;
extern ID3D11PixelShader* pixelShaderTextured;
extern ID3D11Buffer* texturedVertexBuffer;
extern ID3D11SamplerState* samplerState;

extern ID3D11Buffer* paddleVertexBuffer;
extern ID3D11Buffer* obstacleBuffer;
extern ID3D11Buffer* vertexBuffer;        // buffer geral (inclui paddle)
extern ID3D11Buffer* blockVertexBuffer;
extern ID3D11Buffer* ballVertexBuffer;
extern ID3D11Buffer* projectileBuffer;
extern ID3D11Buffer* forceFieldBuffer;
extern ID3D11Buffer* dashShieldBuffer;
extern ID3D11Buffer* blockColorBuffer;
extern ID3D11Buffer* enemyBulletBuffer;

extern ID3D11PixelShader* pixelShaderObstacle;
extern ID3D11PixelShader* pixelShader;
extern ID3D11PixelShader* pixelShaderBlock;
extern ID3D11PixelShader* pixelShaderPaddle;
extern ID3D11PixelShader* pixelShaderBall;
extern ID3D11PixelShader* pixelShaderProjectile;
extern ID3D11PixelShader* pixelShaderEnemyBullet;
extern ID3D11PixelShader* pixelShaderMenu;

extern ID3D11ShaderResourceView* editorObstacleTexture;
extern ID3D11ShaderResourceView* menuBgTexture;
extern ID3D11ShaderResourceView* menuTitleTexture;
extern ID3D11ShaderResourceView* menuSelectorTexture;
extern ID3D11ShaderResourceView* stageBgTexture;
// SRVs de preview por painel
extern ID3D11ShaderResourceView* editorPlayerTexture;   // sprite idle do jogador
extern ID3D11ShaderResourceView* editorBallTexture;     // sprite da bola
extern ID3D11ShaderResourceView* editorBlockTexture;    // sprite do inimigo/bloco
extern ID3D11ShaderResourceView* editorBossTexture;     // sprite do boss
extern ID3D11ShaderResourceView* editorPlayerRunRightTexture;
extern ID3D11ShaderResourceView* editorPlayerRunLeftTexture;
extern ID3D11ShaderResourceView* editorPlayerDashRightTexture;
extern ID3D11ShaderResourceView* editorPlayerDashLeftTexture;
extern ID3D11ShaderResourceView* editorProjectileTexture;
extern ID3D11InputLayout* inputLayoutTextured;
extern ID3D11BlendState* alphaBlendState;

extern double       g_targetFPS;
extern double       g_maxFrameTime;
extern GameState    currentState;
extern EditorMode   currentEditorMode;
extern int          selectedMenuIndex;
extern const char* mainMenuItems[];
extern const int    mainMenuCount;
extern int          difficulty;
extern const int    difficultyCount;
extern bool         g_wasUpPressed;
extern bool         g_wasDownPressed;
extern bool         g_wasZPressed;
extern bool         modoEditor;

extern int   balasPorBloco;
extern int   bulletPattern;
extern bool  timeout;
extern int   stage;
extern int   stageTransitionTimer;
extern int   life;
extern int   cfgLife;
extern int   timer;
extern int       bossHP;
extern BossState g_boss;
extern StageMode currentStageMode;
extern int   blocksRemaining;
extern int   blocksInitialCount;  // total de blocos ao iniciar o stage
extern int   timeCount;
extern int   score;
extern int   highScore;
extern int   combo;
extern bool  iFrame;
extern int   iFrameTimer;
extern int   damageEffectTimer;
extern int   damageBlinkCounter;
extern bool ballInTransit;
extern int portalTimer;
extern float portalExitX, portalExitY;
extern float portalExitVelX, portalExitVelY;
extern float portalEntranceX;
extern float portalEntranceY;

extern bool  projectileActive;
extern float projectileX, projectileY, projectileSize, projectileSpeed;

extern float       paddleX;
extern const float paddleY;
extern float       paddleWidth;
extern float       paddleHeight;
extern float       paddleHeightNormal;
extern float       paddleHeightDash;
extern bool        paddleVisible;
extern int         paddleMoveDir; // -1 left, 0 idle, +1 right
// Valores editáveis pelo editor (aplicados ao paddle em runtime)
extern float       paddleEditWidth;
extern float       paddleEditHeight;
extern float       paddleEditMoveSpeed;

extern float ballX, ballY, ballVelX, ballVelY, ballSize;

extern bool  forceFieldActive;
extern float forceFieldRadius, forceFieldTimer, forceFieldY, forceFieldX;

extern bool  dashActive;
extern int   dashTimer;
extern float dashDir, dashSpeed;

extern std::vector<Projectile>   projectiles;
extern std::vector<Block>        blocks;
extern std::vector<Obstacle>     obstacles;
extern std::vector<EnemyBullet>  enemyBullets;
extern std::vector<DroppedItem>  droppedItems;
extern std::vector<Portal> portals;

extern ObstacleConfig      editorObstacleConfig;
extern char                editorObstacleNameInput[64];
extern char                editorObstacleWidthInput[32];
extern char                editorObstacleHeightInput[32];
extern char                editorObstacleTexturePathInput[256];

extern BossConfig          editorBossConfig;
extern BlockConfig         editorBlockConfig;
extern PlayerSpriteConfig  editorPlayerConfig;
extern BallSpriteConfig    editorBallConfig;
extern MenuConfig          editorMenuConfig;
extern StageStartConfig    editorStageConfig;
extern StageEditorConfig   editorStageEditorConfig;

// Portal
extern std::unique_ptr<DirectX::SpriteBatch> spriteBatch;
extern float portalTexWidth;
extern float portalTexHeight;
extern int portalCooldown;

extern std::vector<PlacedObject> stageObjects;
// Projeto ativo
extern GameProjectConfig gameProject;
extern char              gameProjectPath[MAX_PATH]; // path do game_config.json aberto

// Preview e demo do editor
extern bool  editorDemoActive;      // toggle de demonstracao (inimigos, boss)
extern float editorDemoBossHPPct;   // slider de HP simulado para o boss