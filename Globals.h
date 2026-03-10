#pragma once
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
	EDITOR_MODE_BOSS
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

// ==========================================
// DECLARAÇÃO DAS VARIÁVEIS GLOBAIS (extern)
// ==========================================

// DirectX
extern HWND g_hWnd;
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

// Buffers
extern ID3D11Buffer* obstacleBuffer;
extern ID3D11Buffer* vertexBuffer;
extern ID3D11Buffer* blockVertexBuffer;
extern ID3D11Buffer* ballVertexBuffer;
extern ID3D11Buffer* projectileBuffer;
extern ID3D11Buffer* forceFieldBuffer;
extern ID3D11Buffer* dashShieldBuffer;
extern ID3D11Buffer* blockColorBuffer;
extern ID3D11Buffer* enemyBulletBuffer;

// Shaders
extern ID3D11PixelShader* pixelShaderObstacle;
extern ID3D11PixelShader* pixelShader;
extern ID3D11PixelShader* pixelShaderBlock;
extern ID3D11PixelShader* pixelShaderPaddle;
extern ID3D11PixelShader* pixelShaderBall;
extern ID3D11PixelShader* pixelShaderProjectile;
extern ID3D11PixelShader* pixelShaderEnemyBullet;
extern ID3D11PixelShader* pixelShaderMenu;
extern ID3D11ShaderResourceView* editorObstacleTexture;

// Menu e Estado do Jogo
extern double g_targetFPS;
extern double g_maxFrameTime;
extern GameState currentState;
extern EditorMode currentEditorMode;
extern int selectedMenuIndex;
extern const char* mainMenuItems[];
extern const int mainMenuCount;
extern int difficulty;
extern const int difficultyCount;
extern bool g_wasUpPressed;
extern bool g_wasDownPressed;
extern bool g_wasZPressed;
extern bool modoEditor;

// Gameplay Variáveis
extern int balasPorBloco;
extern int bulletPattern;
extern bool timeout;
extern int stage;
extern int stageTransitionTimer;
extern int life;
extern int cfgLife;
extern int timer;
extern int bossHP;
extern int blocksRemaining;
extern int timeCount;
extern int score;
extern int highScore;
extern int combo;
extern bool iFrame;
extern int iFrameTimer;

// Projéteis
extern bool projectileActive;
extern float projectileX;
extern float projectileY;
extern float projectileSize;
extern float projectileSpeed;

// Barrinha (Paddle)
extern float paddleX;
extern const float paddleY;
extern float paddleWidth;
extern float paddleHeight;
extern float paddleHeightNormal;
extern float paddleHeightDash;
extern bool paddleVisible;

// Bola
extern float ballX;
extern float ballY;
extern float ballVelX;
extern float ballVelY;
extern float ballSize;

// Shield
extern bool forceFieldActive;
extern float forceFieldRadius;
extern float forceFieldTimer;
extern float forceFieldY;
extern float forceFieldX;

// Dash
extern bool dashActive;
extern int dashTimer;
extern float dashDir;
extern float dashSpeed;

// Vetores (Listas de objetos)
extern std::vector<Projectile> projectiles;
extern std::vector<Block> blocks;
extern std::vector<Obstacle> obstacles;
extern std::vector<EnemyBullet> enemyBullets;

// Configs
extern ObstacleConfig editorObstacleConfig;

extern char editorObstacleNameInput[64];
extern char editorObstacleWidthInput[32];
extern char editorObstacleHeightInput[32];
extern char editorObstacleTexturePathInput[256];