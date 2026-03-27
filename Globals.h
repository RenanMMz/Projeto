#pragma once
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <vector>
#include <DirectXMath.h>
#include <fstream>
#include <sstream>
#include <string>

#include <wrl/client.h>
#include <DDSTextureLoader.h>
#include <WICTextureLoader.h>
#include <SpriteBatch.h>
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "D3DCompiler.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

using namespace DirectX;

// ==========================================
// ENUMS E STRUCTS
// ==========================================

enum EditorMode {
	EDITOR_MODE_PLAYER,
	EDITOR_MODE_BALL,
	EDITOR_MODE_STAGE,
	EDITOR_MODE_OBSTACLE,
	EDITOR_MODE_ENEMY,
	EDITOR_MODE_BOSS,
	EDITOR_MODE_BOMB,   // NOVO
	EDITOR_MODE_MENU    // NOVO
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

struct ColorConstantBuffer {
	DirectX::XMFLOAT4 color;
};
struct Projectile {
	float x, y; bool active;
};
struct Block {
	float x, y, width, height;
	bool active;
	int hits;
	int bulletPattern;
	int bulletCount;
	bool iFrameBlock;
	int iFrameBlockTimer;
};
struct Obstacle {
	float x, y, width, height; bool active;
};
struct ObstacleConfig {
	float width;
	float height;
	float colorR, colorG, colorB, colorA;
	char name[64];
	char texturePath[256];
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

struct DropConfig {
	bool  hasDrop;
	int   dropType;     // 0=vida, 1=shield, 2=bomba, 3=pontos
	float dropChance;   // 0.0 - 1.0
};

struct BlockConfig {
	char  name[64];
	char  texturePath[256];
	float width, height;
	float colorR, colorG, colorB, colorA;
	int   maxHits;
	int   bulletPattern;
	int   bulletCount;
	DropConfig drop;
};

struct BossPhase {
	int   hpThreshold;      // ativa quando HP <= X%
	int   bulletPattern;
	int   bulletCount;
	float bulletSpeed;
	int   movementPattern;  // 0=waypoints, 1=senoidal, 2=circular, 3=dash
	float movementSpeed;
	float amplitude;        // senoidal/circular
	float frequency;        // senoidal
};

struct BossConfig {
	char     name[64];
	char     texturePath[256];
	int      maxHP;
	float    width, height;
	int      phaseCount;
	BossPhase phases[4];
	int      waypointCount;
	float    waypointX[16];
	float    waypointY[16];
};

struct PlayerSpriteConfig {
	char texturePath[256];
	char projectileTexturePath[256];
	char shieldTexturePath[256];
	char dashTexturePath[256];
};

struct BallSpriteConfig {
	char texturePath[256];
};

struct BombConfig {
	char  name[64];
	char  texturePath[256];
	int   type;         // 0=habilidade, 1=explosivo, 2=ambos
	float radius;
	float damage;
	int   duration;
	int   bulletPattern;
	int   bulletCount;
	float bulletSpeed;
};

struct MenuConfig {
	// Cores
	float bgColorR, bgColorG, bgColorB, bgColorA;
	float buttonColorR, buttonColorG, buttonColorB, buttonColorA;
	float selectedColorR, selectedColorG, selectedColorB, selectedColorA;
	// Texturas
	char  bgTexturePath[256];
	char  titleTexturePath[256];
	char  selectorTexturePath[256]; // icone/seta de selecao
	// Logo - posicao e tamanho em NDC (-1 a 1)
	float logoX;        // centro horizontal
	float logoY;        // centro vertical
	float logoWidth;    // largura total
	float logoHeight;   // altura total
};

struct StageStartConfig {
	int   stageNumber;
	float ballStartX, ballStartY;
	float ballStartVelX, ballStartVelY;
};

struct Portal
{
	float x, y, width, height;
	bool active;
};

// ==========================================
// DECLARAÇÃO DAS VARIÁVEIS GLOBAIS (extern)
// ==========================================

// DirectX core
extern HWND                       g_hWnd;
extern IDXGISwapChain* swapChain;
extern ID3D11Device* device;
extern ID3D11DeviceContext* deviceContext;
extern ID3D11RenderTargetView* renderTargetView;
extern ID3D11VertexShader* vertexShader;
extern ID3D11InputLayout* inputLayout;
extern ID3D11RasterizerState* rasterState;

// Shaders texturizados
extern ID3D11VertexShader* vertexShaderTextured;
extern ID3D11PixelShader* pixelShaderTextured;
extern ID3D11Buffer* texturedVertexBuffer;
extern ID3D11SamplerState* samplerState;

// Buffers
extern ID3D11Buffer* paddleVertexBuffer;
extern ID3D11Buffer* obstacleBuffer;
extern ID3D11Buffer* vertexBuffer;
extern ID3D11Buffer* blockVertexBuffer;
extern ID3D11Buffer* ballVertexBuffer;
extern ID3D11Buffer* projectileBuffer;
extern ID3D11Buffer* forceFieldBuffer;
extern ID3D11Buffer* dashShieldBuffer;
extern ID3D11Buffer* blockColorBuffer;
extern ID3D11Buffer* enemyBulletBuffer;
extern ID3D11Buffer* portalBuffer;

// Pixel Shaders
extern ID3D11PixelShader* pixelShaderObstacle;
extern ID3D11PixelShader* pixelShader;
extern ID3D11PixelShader* pixelShaderBlock;
extern ID3D11PixelShader* pixelShaderPaddle;
extern ID3D11PixelShader* pixelShaderBall;
extern ID3D11PixelShader* pixelShaderProjectile;
extern ID3D11PixelShader* pixelShaderEnemyBullet;
extern ID3D11PixelShader* pixelShaderMenu;

// Texturas do editor
extern ID3D11ShaderResourceView* editorObstacleTexture;
extern ID3D11ShaderResourceView* menuBgTexture;
extern ID3D11ShaderResourceView* menuTitleTexture;
extern ID3D11ShaderResourceView* menuSelectorTexture;  // icone/seta de selecao
extern ID3D11InputLayout* inputLayoutTextured;

// Menu e Estado do Jogo
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

// Gameplay
extern int   balasPorBloco;
extern int   bulletPattern;
extern bool  timeout;
extern int   stage;
extern int   stageTransitionTimer;
extern int   life;
extern int   cfgLife;
extern int   timer;
extern int   bossHP;
extern int   blocksRemaining;
extern int   timeCount;
extern int   score;
extern int   highScore;
extern int   combo;
extern bool  iFrame;
extern int   iFrameTimer;
extern bool ballInTransit;
extern int portalTimer;
extern float portalExitX, portalExitY;
extern float portalExitVelX, portalExitVelY;
extern float portalEntranceX;
extern float portalEntranceY;

// Projéteis
extern bool  projectileActive;
extern float projectileX;
extern float projectileY;
extern float projectileSize;
extern float projectileSpeed;

// Paddle
extern float       paddleX;
extern const float paddleY;
extern float       paddleWidth;
extern float       paddleHeight;
extern float       paddleHeightNormal;
extern float       paddleHeightDash;
extern bool        paddleVisible;

// Bola
extern float ballX;
extern float ballY;
extern float ballVelX;
extern float ballVelY;
extern float ballSize;

// Shield
extern bool  forceFieldActive;
extern float forceFieldRadius;
extern float forceFieldTimer;
extern float forceFieldY;
extern float forceFieldX;

// Dash
extern bool  dashActive;
extern int   dashTimer;
extern float dashDir;
extern float dashSpeed;

// Vetores
extern std::vector<Projectile>  projectiles;
extern std::vector<Block>       blocks;
extern std::vector<Obstacle>    obstacles;
extern std::vector<EnemyBullet> enemyBullets;
extern std::vector<Portal> portals;

// Configs do editor (existentes)
extern ObstacleConfig editorObstacleConfig;
extern char editorObstacleNameInput[64];
extern char editorObstacleWidthInput[32];
extern char editorObstacleHeightInput[32];
extern char editorObstacleTexturePathInput[256];

// Configs do editor (novas)
extern BossConfig         editorBossConfig;
extern BlockConfig        editorBlockConfig;
extern PlayerSpriteConfig editorPlayerConfig;
extern BallSpriteConfig   editorBallConfig;
extern BombConfig         editorBombConfig;
extern MenuConfig         editorMenuConfig;
extern StageStartConfig   editorStageConfig;

// Portal
extern Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> portalTex;
extern std::unique_ptr<DirectX::SpriteBatch> spriteBatch;
extern float portalTexWidth;
extern float portalTexHeight;
extern int portalCooldown;