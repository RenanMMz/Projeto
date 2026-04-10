#include "Render.h"
#include <stdio.h>
#include "Editor.h"
#include "Level.h"
#include "./imgui/imgui.h"
#include "./imgui/imgui_impl_win32.h"
#include "./imgui/imgui_impl_dx11.h"

// Shaders como strings mantidos unicamente no render
const char* g_VS = "struct VS_INPUT { float3 pos : POSITION; }; struct PS_INPUT { float4 pos : SV_POSITION; }; PS_INPUT VSMain(VS_INPUT input) { PS_INPUT output; output.pos = float4(input.pos,1.0f); return output; }";
const char* g_PS = "struct PS_INPUT { float4 pos : SV_POSITION; }; float4 PSMain(PS_INPUT input) : SV_TARGET { return float4(1.0f,0.0f,0.0f,1.0f); }";
const char* g_PS_Ball = "struct PS_INPUT { float4 pos : SV_POSITION; }; float4 PSMain(PS_INPUT input) : SV_TARGET { return float4(0.0f,1.0f,0.0f,1.0f); }";
const char* g_PS_Projectile = "struct PS_INPUT { float4 pos : SV_POSITION; }; float4 PSMain(PS_INPUT input) : SV_TARGET { return float4(1.0f,1.0f,1.0f,1.0f); }";
const char* g_PS_Obstacle = "struct PS_INPUT { float4 pos : SV_POSITION; }; float4 PSMain(PS_INPUT input) : SV_TARGET { return float4(1.0f,1.0f,0.0f,1.0f); }";
const char* g_PS_Block = "cbuffer ColorBuffer : register(b0) { float4 blockColor; }; struct PS_INPUT { float4 pos : SV_POSITION; }; float4 PSMain(PS_INPUT input) : SV_TARGET { return blockColor; }";
const char* g_PS_Bullet = "struct PS_INPUT { float4 pos : SV_POSITION; }; float4 PSMain(PS_INPUT input) : SV_TARGET { return float4(0.900f,0.2500f,0.950f,1.0f); }";
const char* g_PS_Menu = "struct PS_INPUT { float4 pos : SV_POSITION; }; float4 PSMain(PS_INPUT input) : SV_TARGET { return float4(1.0f,1.0f,1.0f,1.0f); }";
const char* g_VS_Textured =
"struct VS_INPUT { float3 pos : POSITION; float2 uv : TEXCOORD0; };"
"struct PS_INPUT { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };"
"PS_INPUT VSMain(VS_INPUT input) {"
"  PS_INPUT o; o.pos = float4(input.pos, 1.0f); o.uv = input.uv; return o;"
"}";

// Pixel shader que amostra a textura
const char* g_PS_Textured =
"Texture2D tex : register(t0);"
"SamplerState samp : register(s0);"
"struct PS_INPUT { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };"
"float4 PSMain(PS_INPUT input) : SV_TARGET {"
"  return tex.Sample(samp, input.uv);"
"}";

bool InitD3D(HWND hWnd) {
	DXGI_SWAP_CHAIN_DESC scd = {}; scd.BufferCount = 1; scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; scd.OutputWindow = hWnd; scd.SampleDesc.Count = 1; scd.Windowed = TRUE;
	if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &scd, &swapChain, &device, nullptr, &deviceContext))) return false;
	ID3D11Texture2D* backBuffer = nullptr; swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&backBuffer); device->CreateRenderTargetView(backBuffer, nullptr, &renderTargetView); backBuffer->Release();
	deviceContext->OMSetRenderTargets(1, &renderTargetView, nullptr);
	D3D11_VIEWPORT viewport = {}; viewport.TopLeftX = 0; viewport.TopLeftY = 0; viewport.Width = 800; viewport.Height = 600; viewport.MinDepth = 0.0f; viewport.MaxDepth = 1.0f; deviceContext->RSSetViewports(1, &viewport);
	D3D11_RASTERIZER_DESC rd = {}; rd.FillMode = D3D11_FILL_SOLID; rd.CullMode = D3D11_CULL_NONE; rd.FrontCounterClockwise = false;
	HRESULT hr = device->CreateRasterizerState(&rd, &rasterState); if (FAILED(hr)) return false; deviceContext->RSSetState(rasterState);

	ID3DBlob* vsBlob = nullptr; ID3DBlob* psBlob = nullptr; ID3DBlob* psBlobBall = nullptr; ID3DBlob* errorBlob = nullptr; ID3DBlob* psBlobBlock = nullptr; ID3DBlob* psBlobProjectile = nullptr; ID3DBlob* psBlobObstacle = nullptr; ID3DBlob* psBlobBullet = nullptr; ID3DBlob* psBlobMenu = nullptr;

	if (FAILED(D3DCompile(g_PS_Block, strlen(g_PS_Block), nullptr, nullptr, nullptr, "PSMain", "ps_4_0", 0, 0, &psBlobBlock, &errorBlob))) return false;
	if (FAILED(D3DCompile(g_PS_Bullet, strlen(g_PS_Bullet), nullptr, nullptr, nullptr, "PSMain", "ps_4_0", 0, 0, &psBlobBullet, &errorBlob))) return false;
	if (FAILED(D3DCompile(g_PS_Projectile, strlen(g_PS_Projectile), nullptr, nullptr, nullptr, "PSMain", "ps_4_0", 0, 0, &psBlobProjectile, &errorBlob))) return false;

	hr = device->CreatePixelShader(psBlobProjectile->GetBufferPointer(), psBlobProjectile->GetBufferSize(), nullptr, &pixelShaderProjectile);
	psBlobProjectile->Release();

	Vertex projectileVertices[6] = {}; D3D11_BUFFER_DESC bdProj = {}; bdProj.Usage = D3D11_USAGE_DEFAULT; bdProj.ByteWidth = sizeof(Vertex) * _countof(projectileVertices); bdProj.BindFlags = D3D11_BIND_VERTEX_BUFFER; D3D11_SUBRESOURCE_DATA initProj = {}; initProj.pSysMem = projectileVertices; hr = device->CreateBuffer(&bdProj, &initProj, &projectileBuffer);
	if (FAILED(D3DCompile(g_VS, strlen(g_VS), nullptr, nullptr, nullptr, "VSMain", "vs_4_0", 0, 0, &vsBlob, &errorBlob))) return false;
	if (FAILED(D3DCompile(g_PS, strlen(g_PS), nullptr, nullptr, nullptr, "PSMain", "ps_4_0", 0, 0, &psBlob, &errorBlob))) return false;
	if (FAILED(D3DCompile(g_PS_Ball, strlen(g_PS_Ball), nullptr, nullptr, nullptr, "PSMain", "ps_4_0", 0, 0, &psBlobBall, &errorBlob))) return false;
	if (FAILED(D3DCompile(g_PS_Obstacle, strlen(g_PS_Obstacle), nullptr, nullptr, nullptr, "PSMain", "ps_4_0", 0, 0, &psBlobObstacle, &errorBlob))) return false;

	hr = device->CreatePixelShader(psBlobBlock->GetBufferPointer(), psBlobBlock->GetBufferSize(), nullptr, &pixelShaderMenu);
	hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vertexShader);
	hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pixelShader);       // shader base (branco)
	hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pixelShaderPaddle); // paddle usa o mesmo
	hr = device->CreatePixelShader(psBlobBall->GetBufferPointer(), psBlobBall->GetBufferSize(), nullptr, &pixelShaderBall);
	hr = device->CreatePixelShader(psBlobBlock->GetBufferPointer(), psBlobBlock->GetBufferSize(), nullptr, &pixelShaderBlock);
	hr = device->CreatePixelShader(psBlobObstacle->GetBufferPointer(), psBlobObstacle->GetBufferSize(), nullptr, &pixelShaderObstacle);
	hr = device->CreatePixelShader(psBlobBullet->GetBufferPointer(), psBlobBullet->GetBufferSize(), nullptr, &pixelShaderEnemyBullet);

	D3D11_INPUT_ELEMENT_DESC layout[] = { {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0} };
	hr = device->CreateInputLayout(layout, 1, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &inputLayout);

	vsBlob->Release(); psBlob->Release(); psBlobBall->Release(); psBlobBlock->Release(); psBlobBullet->Release();

	Vertex vertices[] = { {-0.12f, -0.7f, 0.0f}, {-0.12f, -0.75f, 0.0f}, {0.12f, -0.75f, 0.0f}, {-0.12f, -0.7f, 0.0f}, {0.12f, -0.75f, 0.0f}, {0.12f, -0.7f, 0.0f} };
	Vertex ballVertices[6];

	D3D11_BUFFER_DESC bdMenu = {}; bdMenu.Usage = D3D11_USAGE_DEFAULT; bdMenu.ByteWidth = sizeof(Vertex) * _countof(vertices); bdMenu.BindFlags = D3D11_BIND_VERTEX_BUFFER; D3D11_SUBRESOURCE_DATA initMenu = {}; initMenu.pSysMem = vertices; hr = device->CreateBuffer(&bdMenu, &initMenu, &vertexBuffer);
	D3D11_BUFFER_DESC bdBall = {}; bdBall.Usage = D3D11_USAGE_DEFAULT; bdBall.ByteWidth = sizeof(Vertex) * _countof(ballVertices); bdBall.BindFlags = D3D11_BIND_VERTEX_BUFFER; D3D11_SUBRESOURCE_DATA initBall = {}; initBall.pSysMem = ballVertices; hr = device->CreateBuffer(&bdBall, &initBall, &ballVertexBuffer);
	D3D11_BUFFER_DESC bdPaddle = {}; bdPaddle.Usage = D3D11_USAGE_DEFAULT; bdPaddle.ByteWidth = sizeof(Vertex) * _countof(vertices); bdPaddle.BindFlags = D3D11_BIND_VERTEX_BUFFER; D3D11_SUBRESOURCE_DATA initPaddle = {}; initPaddle.pSysMem = vertices; hr = device->CreateBuffer(&bdPaddle, &initPaddle, &paddleVertexBuffer);
	D3D11_BUFFER_DESC bdShield = {}; bdShield.Usage = D3D11_USAGE_DEFAULT; bdShield.ByteWidth = sizeof(Vertex) * (32 + 2) * 3; bdShield.BindFlags = D3D11_BIND_VERTEX_BUFFER; hr = device->CreateBuffer(&bdShield, nullptr, &forceFieldBuffer);
	D3D11_BUFFER_DESC bdDashShield = {}; bdDashShield.Usage = D3D11_USAGE_DEFAULT; bdDashShield.ByteWidth = sizeof(Vertex) * _countof(vertices); bdDashShield.BindFlags = D3D11_BIND_VERTEX_BUFFER; hr = device->CreateBuffer(&bdDashShield, nullptr, &dashShieldBuffer);
	D3D11_BUFFER_DESC bdBlock = {}; bdBlock.Usage = D3D11_USAGE_DEFAULT; bdBlock.ByteWidth = sizeof(Vertex) * 6; bdBlock.BindFlags = D3D11_BIND_VERTEX_BUFFER; hr = device->CreateBuffer(&bdBlock, nullptr, &blockVertexBuffer);
	D3D11_BUFFER_DESC bdBlockColor = {}; bdBlockColor.Usage = D3D11_USAGE_DEFAULT; bdBlockColor.ByteWidth = sizeof(XMFLOAT4); bdBlockColor.BindFlags = D3D11_BIND_CONSTANT_BUFFER; hr = device->CreateBuffer(&bdBlockColor, nullptr, &blockColorBuffer);
	D3D11_BUFFER_DESC bdObstacle = {}; bdObstacle.Usage = D3D11_USAGE_DEFAULT; bdObstacle.ByteWidth = sizeof(Vertex) * 6; bdObstacle.BindFlags = D3D11_BIND_VERTEX_BUFFER; hr = device->CreateBuffer(&bdObstacle, nullptr, &obstacleBuffer);
	D3D11_BUFFER_DESC bdBullet = {}; bdBullet.Usage = D3D11_USAGE_DEFAULT; bdBullet.ByteWidth = sizeof(Vertex) * 6; bdBullet.BindFlags = D3D11_BIND_VERTEX_BUFFER; hr = device->CreateBuffer(&bdBullet, nullptr, &enemyBulletBuffer);

	struct VertexUV {
		float x, y, z, u, v;
	};

	ID3DBlob* vsTexBlob = nullptr; ID3DBlob* psTexBlob = nullptr;
	D3DCompile(g_VS_Textured, strlen(g_VS_Textured), nullptr, nullptr, nullptr, "VSMain", "vs_4_0", 0, 0, &vsTexBlob, nullptr);
	D3DCompile(g_PS_Textured, strlen(g_PS_Textured), nullptr, nullptr, nullptr, "PSMain", "ps_4_0", 0, 0, &psTexBlob, nullptr);

	device->CreateVertexShader(vsTexBlob->GetBufferPointer(), vsTexBlob->GetBufferSize(), nullptr, &vertexShaderTextured);
	device->CreatePixelShader(psTexBlob->GetBufferPointer(), psTexBlob->GetBufferSize(), nullptr, &pixelShaderTextured);

	// Input layout com POSITION + TEXCOORD
	D3D11_INPUT_ELEMENT_DESC layoutTex[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,                            D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, sizeof(float) * 3,               D3D11_INPUT_PER_VERTEX_DATA, 0},
	};
	// Reutiliza o inputLayout existente ou cria um novo - aqui criamos separado:
	// (declare extern ID3D11InputLayout* inputLayoutTextured; no Globals se quiser separar)
	// Por simplicidade, vamos guardar no proprio inputLayout textured inline na DrawObstaclePreview.

	// Buffer de vertices texturizados (quad = 6 vertices)
	D3D11_BUFFER_DESC bdTex = {};
	bdTex.Usage = D3D11_USAGE_DEFAULT;
	bdTex.ByteWidth = sizeof(VertexUV) * 6;
	bdTex.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	device->CreateBuffer(&bdTex, nullptr, &texturedVertexBuffer);

	// Sampler state (filtro bilinear, wrap)
	D3D11_SAMPLER_DESC sd = {};
	sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sd.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sd.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	device->CreateSamplerState(&sd, &samplerState);

	vsTexBlob->Release(); psTexBlob->Release();

	// Input layout UV global (usado em DrawObstaclePreview e DrawMenuBackground)
	{
		ID3DBlob* vsForLayout = nullptr;
		D3DCompile(g_VS_Textured, strlen(g_VS_Textured), nullptr, nullptr, nullptr,
			"VSMain", "vs_4_0", 0, 0, &vsForLayout, nullptr);
		D3D11_INPUT_ELEMENT_DESC layoutTex[] = {
			{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,              D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, sizeof(float) * 3, D3D11_INPUT_PER_VERTEX_DATA, 0},
		};
		device->CreateInputLayout(layoutTex, 2,
			vsForLayout->GetBufferPointer(), vsForLayout->GetBufferSize(),
			&inputLayoutTextured);
		vsForLayout->Release();
	}

	// Alpha blend state para texturas com transparencia
	{
		D3D11_BLEND_DESC bd = {};
		bd.RenderTarget[0].BlendEnable = TRUE;
		bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		device->CreateBlendState(&bd, &alphaBlendState);
	}

	life = 3; return true;
}

void DrawRectButton(float x1, float y1, float x2, float y2, const float color[4]) {
	Vertex vertices[] = { {x1,y1,0.0f}, {x1,y2,0.0f}, {x2,y2,0.0f}, {x1,y1,0.0f}, {x2,y2,0.0f}, {x2,y1,0.0f} };
	deviceContext->UpdateSubresource(vertexBuffer, 0, nullptr, vertices, 0, 0);
	ColorConstantBuffer cb = { DirectX::XMFLOAT4(color[0], color[1], color[2], color[3]) };
	deviceContext->UpdateSubresource(blockColorBuffer, 0, nullptr, &cb, 0, 0);
	deviceContext->IASetInputLayout(inputLayout); deviceContext->VSSetShader(vertexShader, nullptr, 0);
	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	deviceContext->PSSetShader(pixelShaderBlock, nullptr, 0); deviceContext->PSSetConstantBuffers(0, 1, &blockColorBuffer);
	UINT stride = sizeof(Vertex); UINT offset = 0; deviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
	deviceContext->Draw(6, 0);
}

void DrawLives(HWND hwnd, int life) {
	HDC hdc = GetDC(hwnd); SetBkMode(hdc, TRANSPARENT); SetTextColor(hdc, RGB(255, 255, 255));
	wchar_t buffer[32]; swprintf(buffer, 32, L"Lives: %d", life); TextOutW(hdc, 10, 10, buffer, (int)wcslen(buffer)); ReleaseDC(hwnd, hdc);
}

void DrawStage(HWND hwnd, int stage) {
	HDC hdc = GetDC(hwnd); SetBkMode(hdc, TRANSPARENT); SetTextColor(hdc, RGB(255, 255, 255));
	wchar_t buffer[32]; swprintf(buffer, 32, L"Stage: %d", stage + 1); TextOutW(hdc, 400, 10, buffer, (int)wcslen(buffer)); ReleaseDC(hwnd, hdc);
}

void DrawScore(HWND hwnd, int score) {
	HDC hdc = GetDC(hwnd); SetBkMode(hdc, TRANSPARENT); SetTextColor(hdc, RGB(255, 255, 255));
	wchar_t buffer[32]; swprintf(buffer, 32, L"Score: %d", score * 10); TextOutW(hdc, 10, 30, buffer, (int)wcslen(buffer)); ReleaseDC(hwnd, hdc);
}

void DrawBlocksRemaining(HWND hwnd, int blocksRemaining) {
	HDC hdc = GetDC(hwnd); SetBkMode(hdc, TRANSPARENT); SetTextColor(hdc, RGB(255, 255, 255));
	wchar_t buffer[32]; swprintf(buffer, 32, L"Blocks Remaining: %d", blocksRemaining); TextOutW(hdc, 200, 10, buffer, (int)wcslen(buffer)); ReleaseDC(hwnd, hdc);
}

// Desenha quad texturizado arbitrÃ¡rio em NDC
static void DrawTexturedQuad(ID3D11ShaderResourceView* srv,
	float x1, float y1, float x2, float y2)
{
	if (!srv) return;
	struct VertexUV {
		float x, y, z, u, v;
	};
	VertexUV verts[] = {
		{x1, y2, 0, 0, 0},
		{x1, y1, 0, 0, 1},
		{x2, y1, 0, 1, 1},
		{x1, y2, 0, 0, 0},
		{x2, y1, 0, 1, 1},
		{x2, y2, 0, 1, 0},
	};
	deviceContext->UpdateSubresource(texturedVertexBuffer, 0, nullptr, verts, 0, 0);
	UINT stride = sizeof(VertexUV), offset = 0;
	deviceContext->IASetInputLayout(inputLayoutTextured);
	deviceContext->IASetVertexBuffers(0, 1, &texturedVertexBuffer, &stride, &offset);
	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	deviceContext->VSSetShader(vertexShaderTextured, nullptr, 0);
	deviceContext->PSSetShader(pixelShaderTextured, nullptr, 0);
	deviceContext->PSSetShaderResources(0, 1, &srv);
	deviceContext->PSSetSamplers(0, 1, &samplerState);

	// Ativa alpha blending para transparencia de PNGs
	float blendFactor[4] = { 0, 0, 0, 0 };
	deviceContext->OMSetBlendState(alphaBlendState, blendFactor, 0xffffffff);
	deviceContext->Draw(6, 0);
	// Desativa blend state para nao afetar draws solidos
	deviceContext->OMSetBlendState(nullptr, blendFactor, 0xffffffff);
}

// Fundo fullscreen
static void DrawMenuBackground(ID3D11ShaderResourceView* bgTex)
{
	DrawTexturedQuad(bgTex, -1.0f, -1.0f, 1.0f, 1.0f);
}

// Logo configurÃ¡vel via editorMenuConfig
static void DrawMenuLogo()
{
	if (!menuTitleTexture) return;
	float hw = editorMenuConfig.logoWidth / 2.0f;
	float hh = editorMenuConfig.logoHeight / 2.0f;
	DrawTexturedQuad(menuTitleTexture,
		editorMenuConfig.logoX - hw, editorMenuConfig.logoY - hh,
		editorMenuConfig.logoX + hw, editorMenuConfig.logoY + hh);
}

// BotÃ£o de menu: cor sÃ³lida + Ã­cone de seleÃ§Ã£o Ã  esquerda quando ativo
static void DrawMenuButton(float x1, float y1, float x2, float y2,
	const float color[4], bool selected)
{
	// Corpo do botÃ£o (cor sÃ³lida)
	Vertex vertices[] = {
		{x1,y1,0},{x1,y2,0},{x2,y2,0},
		{x1,y1,0},{x2,y2,0},{x2,y1,0}
	};
	deviceContext->UpdateSubresource(vertexBuffer, 0, nullptr, vertices, 0, 0);
	ColorConstantBuffer cb = { DirectX::XMFLOAT4(color[0],color[1],color[2],color[3]) };
	deviceContext->UpdateSubresource(blockColorBuffer, 0, nullptr, &cb, 0, 0);
	deviceContext->IASetInputLayout(inputLayout);
	deviceContext->VSSetShader(vertexShader, nullptr, 0);
	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	deviceContext->PSSetShader(pixelShaderBlock, nullptr, 0);
	deviceContext->PSSetConstantBuffers(0, 1, &blockColorBuffer);
	UINT stride = sizeof(Vertex), offset = 0;
	deviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
	deviceContext->Draw(6, 0);

	// Ãcone de seleÃ§Ã£o: quadrado Ã  esquerda dentro do botÃ£o, sÃ³ quando selecionado
	if (selected && menuSelectorTexture) {
		float btnH = y2 - y1;
		float iconSz = btnH * 0.7f;                 // tamanho do Ã­cone = 70% da altura
		float margin = btnH * 0.15f;                // margem interna
		float ix1 = x1 + margin;
		float ix2 = ix1 + iconSz;
		float iy1 = y1 + (btnH - iconSz) / 2.0f;   // centralizado verticalmente
		float iy2 = iy1 + iconSz;
		DrawTexturedQuad(menuSelectorTexture, ix1, iy1, ix2, iy2);
	}
}

void RenderMenu() {
	float clearColor[4] = {
		editorMenuConfig.bgColorR, editorMenuConfig.bgColorG,
		editorMenuConfig.bgColorB, editorMenuConfig.bgColorA
	};
	deviceContext->ClearRenderTargetView(renderTargetView, clearColor);
	DrawMenuBackground(menuBgTexture);
	DrawMenuLogo();

	float startY = 0.2f, spacing = 0.3f, buttonWidth = 0.8f, buttonHeight = 0.2f;
	float colorNormal[4] = { editorMenuConfig.buttonColorR,   editorMenuConfig.buttonColorG,
								editorMenuConfig.buttonColorB,   editorMenuConfig.buttonColorA };
	float colorSelected[4] = { editorMenuConfig.selectedColorR, editorMenuConfig.selectedColorG,
								editorMenuConfig.selectedColorB, editorMenuConfig.selectedColorA };

	for (int i = 0; i < mainMenuCount; i++) {
		bool sel = (selectedMenuIndex == i);
		float yCenter = startY - i * spacing;
		float x1 = -buttonWidth / 2.0f, x2 = buttonWidth / 2.0f;
		float y1 = yCenter - buttonHeight / 2.0f, y2 = yCenter + buttonHeight / 2.0f;
		DrawMenuButton(x1, y1, x2, y2, sel ? colorSelected : colorNormal, sel);
	}
}

void RenderDiffSelect() {
	float clearColor[4] = {
		editorMenuConfig.bgColorR, editorMenuConfig.bgColorG,
		editorMenuConfig.bgColorB, editorMenuConfig.bgColorA
	};
	deviceContext->ClearRenderTargetView(renderTargetView, clearColor);
	DrawMenuBackground(menuBgTexture);
	DrawMenuLogo();

	float startY = 0.5f, spacing = 0.3f, buttonWidth = 0.8f, buttonHeight = 0.2f;
	float colorNormal[4] = { editorMenuConfig.buttonColorR,   editorMenuConfig.buttonColorG,
								editorMenuConfig.buttonColorB,   editorMenuConfig.buttonColorA };
	float colorSelected[4] = { editorMenuConfig.selectedColorR, editorMenuConfig.selectedColorG,
								editorMenuConfig.selectedColorB, editorMenuConfig.selectedColorA };

	for (int i = 0; i < difficultyCount; i++) {
		bool sel = (selectedMenuIndex == i);
		float yCenter = startY - i * spacing;
		float x1 = -buttonWidth / 2.0f, x2 = buttonWidth / 2.0f;
		float y1 = yCenter - buttonHeight / 2.0f, y2 = yCenter + buttonHeight / 2.0f;
		DrawMenuButton(x1, y1, x2, y2, sel ? colorSelected : colorNormal, sel);
	}
}

void RenderGameplay() {
	// Cor de fundo do stage (usa config do editor se disponivel)
	float clearColor[4] = {
		editorStageEditorConfig.bgColorR, editorStageEditorConfig.bgColorG,
		editorStageEditorConfig.bgColorB, editorStageEditorConfig.bgColorA
	};
	deviceContext->ClearRenderTargetView(renderTargetView, clearColor);

	// Textura de fundo do stage
	if (editorStageEditorConfig.useTextureBg && stageBgTexture)
		DrawTexturedQuad(stageBgTexture, -1.0f, -1.0f, 1.0f, 1.0f);

	UINT stride = sizeof(Vertex); UINT offset = 0; deviceContext->IASetInputLayout(inputLayout);
	deviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset); deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	deviceContext->VSSetShader(vertexShader, nullptr, 0); deviceContext->PSSetShader(pixelShader, nullptr, 0);

	// Paddle — textura direcional ou idle ou cor solida
	if (paddleVisible) {
		ID3D11ShaderResourceView* paddleSRV = editorPlayerTexture; // idle
		if (paddleMoveDir < 0 && editorPlayerRunLeftTexture)
			paddleSRV = editorPlayerRunLeftTexture;
		else if (paddleMoveDir > 0 && editorPlayerRunRightTexture)
			paddleSRV = editorPlayerRunRightTexture;

		if (paddleSRV) {
			DrawTexturedQuad(paddleSRV,
				paddleX - paddleWidth / 2, paddleY,
				paddleX + paddleWidth / 2, paddleY + paddleHeight);
		}
		else {
			deviceContext->PSSetShader(pixelShaderPaddle, nullptr, 0);
			UINT s = sizeof(Vertex), o = 0;
			deviceContext->IASetInputLayout(inputLayout);
			deviceContext->IASetVertexBuffers(0, 1, &paddleVertexBuffer, &s, &o);
			deviceContext->VSSetShader(vertexShader, nullptr, 0);
			deviceContext->Draw(6, 0);
		}
	}

	for (auto& portal : portals)
	{
		if (!portal.active) continue;
		if (portal.textureSRV)
		{
			DrawTexturedQuad(portal.textureSRV,
				portal.x - portal.width / 2, portal.y,
				portal.x + portal.width / 2, portal.y + portal.height);
		}
		else
		{
			// Padrão se não tiver textura, ciano
			Vertex pVerts[] = {
				{portal.x - portal.width / 2, portal.y + portal.height, 0.0f},
				{portal.x - portal.width / 2, portal.y, 0.0f},
				{portal.x + portal.width / 2, portal.y, 0.0f},
				{portal.x - portal.width / 2, portal.y + portal.height, 0.0f},
				{portal.x + portal.width / 2, portal.y, 0.0f},
				{portal.x + portal.width / 2, portal.y + portal.height, 0.0f}
			};
			deviceContext->UpdateSubresource(obstacleBuffer, 0, nullptr, pVerts, 0, 0);
			XMFLOAT4 pColor(0.0f, 1.0f, 1.0f, 1.0f);
			deviceContext->UpdateSubresource(blockColorBuffer, 0, nullptr, &pColor, 0, 0);
			deviceContext->PSSetShader(pixelShaderBlock, nullptr, 0);
			deviceContext->PSSetConstantBuffers(0, 1, &blockColorBuffer);
			UINT s = sizeof(Vertex), o = 0;
			deviceContext->IASetVertexBuffers(0, 1, &obstacleBuffer, &s, &o);
			deviceContext->Draw(6, 0);
		}
	}

	// Bola — textura ou cor solida
	if (editorBallTexture) {
		DrawTexturedQuad(editorBallTexture,
			ballX - ballSize, ballY - ballSize,
			ballX + ballSize, ballY + ballSize);
	}
	else {
		deviceContext->IASetInputLayout(inputLayout);
		deviceContext->VSSetShader(vertexShader, nullptr, 0);
		deviceContext->PSSetShader(pixelShaderBall, nullptr, 0);
		UINT s = sizeof(Vertex), o = 0;
		deviceContext->IASetVertexBuffers(0, 1, &ballVertexBuffer, &s, &o);
		deviceContext->Draw(6, 0);
	}

	// Projeteis do jogador — textura ou cor solida
	for (auto& p : projectiles) {
		if (!p.active) continue;
		float ps = projectileSize * 0.8f;
		if (editorProjectileTexture) {
			DrawTexturedQuad(editorProjectileTexture,
				p.x - ps, p.y - ps, p.x + ps, p.y + ps);
		}
		else {
			Vertex projVertices[] = { {p.x - ps, p.y + ps, 0.0f}, {p.x - ps, p.y - ps, 0.0f}, {p.x + ps, p.y - ps, 0.0f}, {p.x - ps, p.y + ps, 0.0f}, {p.x + ps, p.y - ps, 0.0f}, {p.x + ps, p.y + ps, 0.0f} };
			deviceContext->UpdateSubresource(projectileBuffer, 0, nullptr, projVertices, 0, 0);
			UINT s = sizeof(Vertex), o = 0;
			deviceContext->IASetInputLayout(inputLayout);
			deviceContext->VSSetShader(vertexShader, nullptr, 0);
			deviceContext->PSSetShader(pixelShaderProjectile, nullptr, 0);
			deviceContext->IASetVertexBuffers(0, 1, &projectileBuffer, &s, &o); deviceContext->Draw(6, 0);
		}
	}

	// Obstaculos — textura ou cor do obstaculo
	for (auto& obstacle : obstacles) {
		if (!obstacle.active) continue;
		if (obstacle.useTexture && obstacle.textureSRV) {
			DrawTexturedQuad(obstacle.textureSRV,
				obstacle.x - obstacle.width / 2, obstacle.y,
				obstacle.x + obstacle.width / 2, obstacle.y + obstacle.height);
		}
		else {
			Vertex vertices[] = { {obstacle.x - obstacle.width / 2, obstacle.y + obstacle.height, 0.0f}, {obstacle.x - obstacle.width / 2, obstacle.y, 0.0f}, {obstacle.x + obstacle.width / 2, obstacle.y, 0.0f}, {obstacle.x - obstacle.width / 2, obstacle.y + obstacle.height, 0.0f}, {obstacle.x + obstacle.width / 2, obstacle.y, 0.0f}, {obstacle.x + obstacle.width / 2, obstacle.y + obstacle.height, 0.0f} };
			deviceContext->UpdateSubresource(obstacleBuffer, 0, nullptr, vertices, 0, 0);
			XMFLOAT4 obsColor(obstacle.colorR, obstacle.colorG, obstacle.colorB, obstacle.colorA);
			deviceContext->UpdateSubresource(blockColorBuffer, 0, nullptr, &obsColor, 0, 0);
			deviceContext->IASetInputLayout(inputLayout);
			deviceContext->VSSetShader(vertexShader, nullptr, 0);
			deviceContext->PSSetShader(pixelShaderBlock, nullptr, 0);
			deviceContext->PSSetConstantBuffers(0, 1, &blockColorBuffer);
			UINT s = sizeof(Vertex), o = 0;
			deviceContext->IASetVertexBuffers(0, 1, &obstacleBuffer, &s, &o); deviceContext->Draw(6, 0);
		}
	}

	// Blocos/Inimigos — textura individual ou cor do bloco configurada no editor
	for (auto& block : blocks) {
		if (!block.active) continue;
		if (block.useTexture && block.textureSRV) {
			DrawTexturedQuad(block.textureSRV,
				block.x - block.width / 2, block.y,
				block.x + block.width / 2, block.y + block.height);
		}
		else {
			XMFLOAT4 color(block.colorR, block.colorG, block.colorB, block.colorA);
			deviceContext->PSSetConstantBuffers(0, 1, &blockColorBuffer); deviceContext->UpdateSubresource(blockColorBuffer, 0, nullptr, &color, 0, 0);
			Vertex vertices[] = { {block.x - block.width / 2, block.y + block.height, 0.0f}, {block.x - block.width / 2, block.y, 0.0f}, {block.x + block.width / 2, block.y, 0.0f}, {block.x - block.width / 2, block.y + block.height, 0.0f}, {block.x + block.width / 2, block.y, 0.0f}, {block.x + block.width / 2, block.y + block.height, 0.0f} };
			deviceContext->UpdateSubresource(blockVertexBuffer, 0, nullptr, vertices, 0, 0);
			UINT s = sizeof(Vertex), o = 0;
			deviceContext->IASetInputLayout(inputLayout);
			deviceContext->VSSetShader(vertexShader, nullptr, 0);
			deviceContext->PSSetShader(pixelShaderBlock, nullptr, 0);
			deviceContext->IASetVertexBuffers(0, 1, &blockVertexBuffer, &s, &o); deviceContext->Draw(6, 0);
		}
	}

	// Balas inimigas
	deviceContext->PSSetShader(pixelShaderEnemyBullet, nullptr, 0);
	for (auto& bullet : enemyBullets) {
		if (!bullet.active) continue; Vertex verticesBullet[] = { {bullet.x - bullet.size, bullet.y + bullet.size, 0.0f}, {bullet.x - bullet.size, bullet.y - bullet.size, 0.0f}, {bullet.x + bullet.size, bullet.y - bullet.size, 0.0f}, {bullet.x - bullet.size, bullet.y + bullet.size, 0.0f}, {bullet.x + bullet.size, bullet.y - bullet.size, 0.0f}, {bullet.x + bullet.size, bullet.y + bullet.size, 0.0f} };
		UINT s = sizeof(Vertex), o = 0;
		deviceContext->IASetInputLayout(inputLayout);
		deviceContext->VSSetShader(vertexShader, nullptr, 0);
		deviceContext->IASetVertexBuffers(0, 1, &enemyBulletBuffer, &s, &o); deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		deviceContext->UpdateSubresource(enemyBulletBuffer, 0, nullptr, verticesBullet, 0, 0); deviceContext->Draw(6, 0);
	}

	// Dropped items (queda)
	for (auto& d : droppedItems) {
		if (!d.active) continue;
		float dsz = 0.02f;
		float dr = 0.3f, dg = 1.0f, db = 0.3f;
		switch (d.type) {
		case 0: dr = 1.0f; dg = 0.3f; db = 0.3f; break; // vida = vermelho
		case 1: dr = 0.3f; dg = 0.5f; db = 1.0f; break; // shield = azul
		case 2: dr = 1.0f; dg = 0.7f; db = 0.1f; break; // bomba = laranja
		case 3: dr = 1.0f; dg = 1.0f; db = 0.2f; break; // pontos = amarelo
		}
		Vertex dv[] = { {d.x-dsz,d.y+dsz,0},{d.x-dsz,d.y-dsz,0},{d.x+dsz,d.y-dsz,0},{d.x-dsz,d.y+dsz,0},{d.x+dsz,d.y-dsz,0},{d.x+dsz,d.y+dsz,0} };
		deviceContext->UpdateSubresource(blockVertexBuffer, 0, nullptr, dv, 0, 0);
		XMFLOAT4 dc(dr, dg, db, 1.0f);
		deviceContext->UpdateSubresource(blockColorBuffer, 0, nullptr, &dc, 0, 0);
		deviceContext->PSSetShader(pixelShaderBlock, nullptr, 0);
		deviceContext->PSSetConstantBuffers(0, 1, &blockColorBuffer);
		UINT s = sizeof(Vertex), o = 0;
		deviceContext->IASetInputLayout(inputLayout);
		deviceContext->VSSetShader(vertexShader, nullptr, 0);
		deviceContext->IASetVertexBuffers(0, 1, &blockVertexBuffer, &s, &o); deviceContext->Draw(6, 0);
	}

	// Force field
	if (forceFieldActive) {
		deviceContext->PSSetShader(pixelShaderProjectile, nullptr, 0); const int segments = 32; std::vector<Vertex> circleVerts; circleVerts.push_back({ forceFieldX, forceFieldY, 0.0f });
		for (int i = 0; i <= segments; i++) {
			float theta = (2 * 3.14159265f * i) / segments; float x = forceFieldX + cosf(theta) * forceFieldRadius; float y = forceFieldY + sinf(theta) * forceFieldRadius; circleVerts.push_back({ x, y, 0.0f });
		}
		std::vector<Vertex> fanVerts; for (int i = 1; i < (int)circleVerts.size() - 1; i++) {
			fanVerts.push_back(circleVerts[0]); fanVerts.push_back(circleVerts[i]); fanVerts.push_back(circleVerts[i + 1]);
		}
		deviceContext->IASetInputLayout(inputLayout);
		deviceContext->VSSetShader(vertexShader, nullptr, 0);
		deviceContext->UpdateSubresource(forceFieldBuffer, 0, nullptr, fanVerts.data(), 0, 0);
		UINT s = sizeof(Vertex), o = 0;
		deviceContext->IASetVertexBuffers(0, 1, &forceFieldBuffer, &s, &o); deviceContext->Draw(static_cast<UINT>(fanVerts.size()), 0);
	}

	// Dash shield
	if (dashActive) {
		deviceContext->PSSetShader(pixelShaderProjectile, nullptr, 0); float shieldWidth = 0.25f; float shieldHeight = 0.15f; float shieldY = paddleY;
		Vertex dashShieldVerts[] = { {paddleX - shieldWidth / 2, shieldY + shieldHeight, 0.0f}, {paddleX - shieldWidth / 2, shieldY, 0.0f}, {paddleX + shieldWidth / 2, shieldY, 0.0f}, {paddleX - shieldWidth / 2, shieldY + shieldHeight, 0.0f}, {paddleX + shieldWidth / 2, shieldY, 0.0f}, {paddleX + shieldWidth / 2, shieldY + shieldHeight, 0.0f} };
		deviceContext->IASetInputLayout(inputLayout);
		deviceContext->VSSetShader(vertexShader, nullptr, 0);
		deviceContext->UpdateSubresource(dashShieldBuffer, 0, nullptr, dashShieldVerts, 0, 0);
		UINT s = sizeof(Vertex), o = 0;
		deviceContext->IASetVertexBuffers(0, 1, &dashShieldBuffer, &s, &o); deviceContext->Draw(6, 0);
	}
}

void CleanD3D() {
	if (swapChain) swapChain->Release(); if (renderTargetView) renderTargetView->Release(); if (deviceContext) deviceContext->Release(); if (device) device->Release();
	if (vertexBuffer) vertexBuffer->Release(); if (paddleVertexBuffer) paddleVertexBuffer->Release(); if (vertexShader) vertexShader->Release(); if (pixelShader) pixelShader->Release(); if (inputLayout) inputLayout->Release();
	if (rasterState) rasterState->Release(); if (pixelShaderBlock) pixelShaderBlock->Release(); if (blockColorBuffer) blockColorBuffer->Release();
	if (blockVertexBuffer) blockVertexBuffer->Release(); if (ballVertexBuffer) ballVertexBuffer->Release(); if (projectileBuffer) projectileBuffer->Release();
	if (forceFieldBuffer) forceFieldBuffer->Release(); if (dashShieldBuffer) dashShieldBuffer->Release(); if (obstacleBuffer) obstacleBuffer->Release(); if (enemyBulletBuffer) enemyBulletBuffer->Release();
}


// RenderEditorUI foi movido para Editor.cpp

void DrawObstaclePreview(float centerX, float centerY)
{
	float hw = editorObstacleConfig.width / 2.0f;
	float hh = editorObstacleConfig.height / 2.0f;
	float x1 = centerX - hw, x2 = centerX + hw;
	float y1 = centerY - hh, y2 = centerY + hh;

	struct VertexUV {
		float x, y, z, u, v;
	};

	VertexUV verts[] = {
		{x1, y2, 0, 0, 0},
		{x1, y1, 0, 0, 1},
		{x2, y1, 0, 1, 1},
		{x1, y2, 0, 0, 0},
		{x2, y1, 0, 1, 1},
		{x2, y2, 0, 1, 0},
	};

	deviceContext->UpdateSubresource(texturedVertexBuffer, 0, nullptr, verts, 0, 0);

	UINT stride = sizeof(VertexUV), offset = 0;
	deviceContext->IASetInputLayout(inputLayoutTextured);
	deviceContext->IASetVertexBuffers(0, 1, &texturedVertexBuffer, &stride, &offset);
	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	deviceContext->VSSetShader(vertexShaderTextured, nullptr, 0);

	if (editorObstacleTexture) {
		deviceContext->PSSetShader(pixelShaderTextured, nullptr, 0);
		deviceContext->PSSetShaderResources(0, 1, &editorObstacleTexture);
		deviceContext->PSSetSamplers(0, 1, &samplerState);
	}
	else {
		float color[4] = {
			editorObstacleConfig.colorR, editorObstacleConfig.colorG,
			editorObstacleConfig.colorB, editorObstacleConfig.colorA
		};
		ColorConstantBuffer cb = { DirectX::XMFLOAT4(color[0], color[1], color[2], color[3]) };
		deviceContext->UpdateSubresource(blockColorBuffer, 0, nullptr, &cb, 0, 0);
		deviceContext->PSSetShader(pixelShaderBlock, nullptr, 0);
		deviceContext->PSSetConstantBuffers(0, 1, &blockColorBuffer);
	}

	deviceContext->Draw(6, 0);
}


// ==========================================
// SISTEMA DE PREVIEW DO EDITOR
// ==========================================
// Area de preview: viewport principal (800x600).
// O painel ImGui ocupa 380px a esquerda.
// Preview center em NDC: x ~0.47, y = 0.
// PREV_L a PREV_R / PREV_B a PREV_T = limites da area visivel.

#include <algorithm>

static const float PREVIEW_CX = 0.47f;
static const float PREVIEW_CY = 0.0f;
static const float PREV_L = -0.05f, PREV_R = 0.97f;
static const float PREV_T = 0.95f, PREV_B = -0.95f;

// --- estado de simulacao (privado) ---
static float s_prevBallX = 0.47f, s_prevBallY = 0.0f;
static float s_prevBallVX = 0.008f, s_prevBallVY = 0.012f;

static float s_demoEX = 0.47f, s_demoEY = 0.0f;
static float s_demoEDir = 1.0f, s_demoEAngle = 0.0f;
static int   s_demoShootT = 0;
static std::vector<EnemyBullet> s_demoBullets;
static bool  s_demoEInit = false;

static float s_demoBX = 0.47f, s_demoBY = 0.5f;
static int   s_demoBActIdx = 0;
static float s_demoBTimer = 0.0f;
static bool  s_demoBInit = false;

// --- helpers de desenho ---

static void DrawQuadColor(float cx, float cy, float hw, float hh,
	float r, float g, float b, float a)
{
	Vertex verts[] = {
		{cx - hw, cy + hh, 0}, {cx - hw, cy - hh, 0}, {cx + hw, cy - hh, 0},
		{cx - hw, cy + hh, 0}, {cx + hw, cy - hh, 0}, {cx + hw, cy + hh, 0}
	};
	deviceContext->UpdateSubresource(vertexBuffer, 0, nullptr, verts, 0, 0);
	ColorConstantBuffer cb = { DirectX::XMFLOAT4(r, g, b, a) };
	deviceContext->UpdateSubresource(blockColorBuffer, 0, nullptr, &cb, 0, 0);
	UINT stride = sizeof(Vertex), offset = 0;
	deviceContext->IASetInputLayout(inputLayout);
	deviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	deviceContext->VSSetShader(vertexShader, nullptr, 0);
	deviceContext->PSSetShader(pixelShaderBlock, nullptr, 0);
	deviceContext->PSSetConstantBuffers(0, 1, &blockColorBuffer);
	deviceContext->Draw(6, 0);
}

static void DrawQuadTexCentered(ID3D11ShaderResourceView* srv,
	float cx, float cy, float hw, float hh)
{
	DrawTexturedQuad(srv, cx - hw, cy - hh, cx + hw, cy + hh);
}

static void DrawBall(float cx, float cy, float sz, float r, float g, float b)
{
	DrawQuadColor(cx, cy, sz, sz, r, g, b, 1.0f);
}

static void DrawBullet(float cx, float cy, float sz)
{
	DrawQuadColor(cx, cy, sz, sz, 0.9f, 0.25f, 0.95f, 1.0f);
}

// --- reinit de demo ---

static void ResetEnemyDemo()
{
	s_demoEX = PREVIEW_CX; s_demoEY = PREVIEW_CY;
	s_demoEDir = 1.0f; s_demoEAngle = 0.0f;
	s_demoShootT = 0; s_demoBullets.clear();
	s_demoEInit = true;
}

static void ResetBossDemo()
{
	s_demoBX = (editorBossConfig.startX != 0.0f) ? editorBossConfig.startX : PREVIEW_CX;
	s_demoBY = (editorBossConfig.startY != 0.0f) ? editorBossConfig.startY : 0.5f;
	s_demoBActIdx = 0; s_demoBTimer = 0.0f;
	s_demoBInit = true;
}

// --- preview por painel ---

static void RenderPreview_Player()
{
	float hw = paddleEditWidth / 2.0f, hh = paddleEditHeight / 2.0f;
	if (editorPlayerTexture)
		DrawQuadTexCentered(editorPlayerTexture, PREVIEW_CX, PREVIEW_CY, hw, hh);
	else
		DrawQuadColor(PREVIEW_CX, PREVIEW_CY, hw, hh, 0.3f, 0.5f, 1.0f, 1.0f);
	// Linha de chao
	DrawQuadColor(PREVIEW_CX, PREVIEW_CY - hh - 0.003f, 0.4f, 0.003f, 0.4f, 0.4f, 0.4f, 1.0f);
}

static void RenderPreview_Ball()
{
	// Atualiza simulacao
	s_prevBallX += s_prevBallVX; s_prevBallY += s_prevBallVY;
	float sz = ballSize;
	if (s_prevBallX - sz < PREV_L) {
		s_prevBallX = PREV_L + sz; s_prevBallVX *= -1;
	}
	if (s_prevBallX + sz > PREV_R) {
		s_prevBallX = PREV_R - sz; s_prevBallVX *= -1;
	}
	if (s_prevBallY + sz > PREV_T) {
		s_prevBallY = PREV_T - sz; s_prevBallVY *= -1;
	}
	if (s_prevBallY - sz < PREV_B) {
		s_prevBallY = PREV_B + sz; s_prevBallVY *= -1;
	}
	// Paredes
	float halfW = (PREV_R - PREV_L) / 2.0f;
	DrawQuadColor(PREV_L + 0.003f, PREVIEW_CY, 0.003f, 0.95f, 0.5f, 0.5f, 0.5f, 1.0f);
	DrawQuadColor(PREV_R - 0.003f, PREVIEW_CY, 0.003f, 0.95f, 0.5f, 0.5f, 0.5f, 1.0f);
	DrawQuadColor(PREVIEW_CX, PREV_T - 0.003f, halfW, 0.003f, 0.5f, 0.5f, 0.5f, 1.0f);
	DrawQuadColor(PREVIEW_CX, PREV_B + 0.003f, halfW, 0.003f, 0.5f, 0.5f, 0.5f, 1.0f);
	if (editorBallTexture)
		DrawQuadTexCentered(editorBallTexture, s_prevBallX, s_prevBallY, sz, sz);
	else
		DrawBall(s_prevBallX, s_prevBallY, sz, 0.2f, 1.0f, 0.3f);
}

static void RenderPreview_Obstacle()
{
	float hw = editorObstacleConfig.width / 2.0f, hh = editorObstacleConfig.height / 2.0f;
	if (editorObstacleConfig.useTexture && editorObstacleTexture)
		DrawQuadTexCentered(editorObstacleTexture, PREVIEW_CX, PREVIEW_CY, hw, hh);
	else
		DrawQuadColor(PREVIEW_CX, PREVIEW_CY, hw, hh,
			editorObstacleConfig.colorR, editorObstacleConfig.colorG,
			editorObstacleConfig.colorB, editorObstacleConfig.colorA);
}

static void RenderPreview_Stage()
{
	// Stage preview cobre a tela inteira — o stage eh o viewport completo do jogo
	if (editorStageEditorConfig.useTextureBg && stageBgTexture)
		DrawTexturedQuad(stageBgTexture, -1.0f, -1.0f, 1.0f, 1.0f);
	else
		DrawQuadColor(0.0f, 0.0f, 1.0f, 1.0f,
			editorStageEditorConfig.bgColorR, editorStageEditorConfig.bgColorG,
			editorStageEditorConfig.bgColorB, editorStageEditorConfig.bgColorA);

	DrawBall(editorStageConfig.ballStartX, editorStageConfig.ballStartY, ballSize, 0.2f, 1.0f, 0.3f);

	for (auto& o : stageObjects) {
		switch (o.type) {
		case PLACED_BLOCK:
			DrawQuadColor(o.x, o.y, editorBlockConfig.width / 2.0f, editorBlockConfig.height / 2.0f,
				editorBlockConfig.colorR, editorBlockConfig.colorG,
				editorBlockConfig.colorB, editorBlockConfig.colorA); break;
		case PLACED_OBSTACLE:
			DrawQuadColor(o.x, o.y, editorObstacleConfig.width / 2.0f, editorObstacleConfig.height / 2.0f,
				editorObstacleConfig.colorR, editorObstacleConfig.colorG,
				editorObstacleConfig.colorB, editorObstacleConfig.colorA); break;
		case PLACED_BOSS:
			DrawQuadColor(o.x, o.y, editorBossConfig.width / 2.0f, editorBossConfig.height / 2.0f,
				0.9f, 0.2f, 0.2f, 1.0f); break;
		default:
			DrawBall(o.x, o.y, ballSize, 0.2f, 1.0f, 0.3f); break;
		}
	}
}

static void FireDemoPattern(float ox, float oy)
{
	int pat = editorBlockConfig.bulletPattern;
	int cnt = editorBlockConfig.bulletCount;
	float spd = (editorBlockConfig.bulletSpeed > 0) ? editorBlockConfig.bulletSpeed : 0.007f;
	float pi2 = 2.0f * 3.14159265f;
	float pAngle = atan2f(PREVIEW_CY - oy, (PREVIEW_CX - 0.2f) - ox);
	for (int i = 0; i < cnt; i++) {
		float a = pAngle;
		switch (pat) {
		case 0: {
			float sp = 1.0f, sa = pAngle - sp / 2.0f; a = (cnt > 1) ? sa + (sp * i / (cnt - 1)) : pAngle;
		} break;
		case 1: spd = 0.005f + i * 0.002f; break;
		case 2: a = pi2 * i / cnt; break;
		case 3: a = i * 0.5f; spd = 0.003f + i * 0.0003f; break;
		case 4: a = pi2 * ((float)rand() / RAND_MAX); break;
		case 5: a = -1.5708f; break;
		}
		EnemyBullet b; b.x = ox; b.y = oy; b.size = 0.012f; b.active = true;
		b.vx = cosf(a) * spd; b.vy = sinf(a) * spd;
		s_demoBullets.push_back(b);
	}
}

static void RenderPreview_Enemy()
{
	float hw = editorBlockConfig.width / 2.0f, hh = editorBlockConfig.height / 2.0f;
	float cx = PREVIEW_CX, cy = PREVIEW_CY;

	if (editorDemoActive) {
		if (!s_demoEInit) ResetEnemyDemo();
		// Movimento
		switch (editorBlockConfig.movType) {
		case MOV_VERTICAL:
			s_demoEY += editorBlockConfig.movSpeed * s_demoEDir;
			if (fabsf(s_demoEY - PREVIEW_CY) >= editorBlockConfig.movAmplitude) s_demoEDir *= -1.0f;
			break;
		case MOV_HORIZONTAL:
			s_demoEX += editorBlockConfig.movSpeed * s_demoEDir;
			if (fabsf(s_demoEX - PREVIEW_CX) >= editorBlockConfig.movAmplitude) s_demoEDir *= -1.0f;
			break;
		case MOV_CIRCULAR:
			s_demoEAngle += editorBlockConfig.movSpeed;
			s_demoEX = PREVIEW_CX + cosf(s_demoEAngle) * editorBlockConfig.movRadius;
			s_demoEY = PREVIEW_CY + sinf(s_demoEAngle) * editorBlockConfig.movRadius;
			break;
		default: s_demoEX = PREVIEW_CX; s_demoEY = PREVIEW_CY; break;
		}
		// Tiro
		int interval = (editorBlockConfig.shootIntervalFrames > 0) ? editorBlockConfig.shootIntervalFrames : 90;
		if (++s_demoShootT >= interval) {
			s_demoShootT = 0; FireDemoPattern(s_demoEX, s_demoEY);
		}
		// Atualiza balas
		for (auto& bl : s_demoBullets) {
			if (!bl.active) continue;
			bl.x += bl.vx; bl.y += bl.vy;
			if (bl.x < PREV_L || bl.x > PREV_R || bl.y < PREV_B || bl.y > PREV_T) bl.active = false;
		}
		s_demoBullets.erase(std::remove_if(s_demoBullets.begin(), s_demoBullets.end(),
			[](const EnemyBullet& b) { return !b.active; }), s_demoBullets.end());
		for (auto& bl : s_demoBullets) if (bl.active) DrawBullet(bl.x, bl.y, bl.size);
		cx = s_demoEX; cy = s_demoEY;
	}
	if (editorBlockTexture)
		DrawQuadTexCentered(editorBlockTexture, cx, cy, hw, hh);
	else
		DrawQuadColor(cx, cy, hw, hh,
			editorBlockConfig.colorR, editorBlockConfig.colorG,
			editorBlockConfig.colorB, editorBlockConfig.colorA);
}

static void RenderPreview_Boss()
{
	float hw = (editorBossConfig.width > 0) ? editorBossConfig.width / 2.0f : 0.1f;
	float hh = (editorBossConfig.height > 0) ? editorBossConfig.height / 2.0f : 0.1f;

	// Use configured start position (or center as fallback)
	float startCX = (editorBossConfig.startX != 0.0f) ? editorBossConfig.startX : PREVIEW_CX;
	float startCY = (editorBossConfig.startY != 0.0f) ? editorBossConfig.startY : 0.5f;
	float cx = startCX, cy = startCY;

	// Determine current HP phase for drawing targets
	int currentPhase = 0;
	if (editorBossConfig.hpPhaseCount > 0) {
		for (int i = 0; i < editorBossConfig.hpPhaseCount; i++)
			if (editorDemoBossHPPct <= editorBossConfig.hpPhases[i].hpThresholdPct) currentPhase = i;
	}

	if (editorDemoActive) {
		if (!s_demoBInit) ResetBossDemo();
		if (editorBossConfig.hpPhaseCount > 0) {
			BossScript& sc = editorBossConfig.hpPhases[currentPhase].script;
			if (sc.actionCount > 0) {
				BossAction& act = sc.actions[s_demoBActIdx % sc.actionCount];
				s_demoBTimer += 1.0f / 60.0f;
				switch (act.type) {
				case BOSS_ACT_MOVE_TO: {
					float dx = act.targetX - s_demoBX, dy = act.targetY - s_demoBY, len = sqrtf(dx * dx + dy * dy);
					if (len < 0.01f) {
						s_demoBActIdx++; s_demoBTimer = 0;
					}
					else {
						float sp = (act.speed > 0) ? act.speed : 0.01f; s_demoBX += (dx / len) * sp; s_demoBY += (dy / len) * sp;
					}
					break;
				}
				case BOSS_ACT_TELEPORT:
					s_demoBX = act.targetX; s_demoBY = act.targetY;
					s_demoBActIdx++; s_demoBTimer = 0; break;
				case BOSS_ACT_WAIT:
				case BOSS_ACT_SHOOT_TIMED:
					if (s_demoBTimer >= act.duration) {
						s_demoBActIdx++; s_demoBTimer = 0;
					} break;
				default:
					if (s_demoBTimer >= 1.0f) {
						s_demoBActIdx++; s_demoBTimer = 0;
					} break;
				}
				if (s_demoBActIdx >= sc.actionCount) s_demoBActIdx = sc.loopFromStep;
			}
		}
		cx = s_demoBX; cy = s_demoBY;
	}

	// Draw start position marker (small diamond)
	DrawQuadColor(startCX, startCY, 0.015f, 0.015f, 0.3f, 0.9f, 0.3f, 0.6f);

	// Draw movement target markers for current phase script
	if (editorBossConfig.hpPhaseCount > 0) {
		BossScript& sc = editorBossConfig.hpPhases[currentPhase].script;
		for (int i = 0; i < sc.actionCount; i++) {
			BossAction& act = sc.actions[i];
			if (act.type == BOSS_ACT_MOVE_TO || act.type == BOSS_ACT_TELEPORT) {
				float r = (act.type == BOSS_ACT_MOVE_TO) ? 0.3f : 0.9f;
				float g = (act.type == BOSS_ACT_MOVE_TO) ? 0.6f : 0.3f;
				DrawQuadColor(act.targetX, act.targetY, 0.012f, 0.012f, r, g, 0.9f, 0.5f);
			}
			if (act.type == BOSS_ACT_SHOOT_FIXED_PTS) {
				for (int fp = 0; fp < act.fixedPointCount; fp++)
					DrawQuadColor(act.fixedPtsX[fp], act.fixedPtsY[fp], 0.008f, 0.008f, 1.0f, 0.5f, 0.2f, 0.5f);
			}
		}
	}

	// Draw boss sprite/quad
	if (editorBossTexture)
		DrawQuadTexCentered(editorBossTexture, cx, cy, hw, hh);
	else
		DrawQuadColor(cx, cy, hw, hh, 0.9f, 0.2f, 0.2f, 1.0f);

	// Barra de HP (topo da area de preview)
	float barW = 0.35f, barFill = editorDemoBossHPPct * barW;
	float barY = PREV_T - 0.08f, barCX = PREV_L + 0.05f + barW / 2.0f;
	DrawQuadColor(barCX, barY, barW / 2.0f, 0.025f, 0.3f, 0.3f, 0.3f, 1.0f);
	if (barFill > 0)
		DrawQuadColor(PREV_L + 0.05f + barFill / 2.0f, barY, barFill / 2.0f, 0.022f, 0.8f, 0.15f, 0.15f, 1.0f);
}

static void RenderPreview_Bomb()
{
	float r = (editorBombConfig.radius > 0) ? editorBombConfig.radius : 0.3f;
	DrawQuadColor(PREVIEW_CX, PREVIEW_CY, r, r, 0.9f, 0.4f, 0.1f, 0.2f);  // area
	DrawQuadColor(PREVIEW_CX, PREVIEW_CY, 0.04f, 0.04f, 1.0f, 0.6f, 0.1f, 1.0f); // icone
}

// ---------------------------------------------------------------------------
// RenderEditor — clears + dispatcher de preview
// ---------------------------------------------------------------------------
void RenderEditor()
{
	float clearColor[4] = { 0.08f, 0.08f, 0.12f, 1.0f };
	deviceContext->ClearRenderTargetView(renderTargetView, clearColor);

	// Linha divisoria (borda direita do painel ImGui) — nao desenhar no Stage (fullscreen)
	if (currentEditorMode != EDITOR_MODE_STAGE)
		DrawQuadColor(-0.05f, 0.0f, 0.003f, 1.0f, 0.25f, 0.25f, 0.30f, 1.0f);

	switch (currentEditorMode) {
	case EDITOR_MODE_PLAYER:   RenderPreview_Player();   break;
	case EDITOR_MODE_BALL:     RenderPreview_Ball();     break;
	case EDITOR_MODE_STAGE:    RenderPreview_Stage();    break;
	case EDITOR_MODE_OBSTACLE: RenderPreview_Obstacle(); break;
	case EDITOR_MODE_ENEMY:    RenderPreview_Enemy();    break;
	case EDITOR_MODE_BOSS:     RenderPreview_Boss();     break;
	case EDITOR_MODE_BOMB:     RenderPreview_Bomb();     break;
	case EDITOR_MODE_MENU:     RenderMenu();             break;
	}
}

// ==========================================
// DEBUG UI â exibida durante gameplay (fora do editor)
// Usa ImGui para mostrar info bÃ¡sica sem bloquear input
// ==========================================
void RenderDebugUI()
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	// Janela minimalista no canto inferior direito, sem foco
	ImGuiIO& io = ImGui::GetIO();
	ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 160.0f, io.DisplaySize.y - 80.0f));
	ImGui::SetNextWindowSize(ImVec2(155.0f, 75.0f));
	ImGui::SetNextWindowBgAlpha(0.45f);
	ImGui::Begin("##dbg", nullptr,
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_NoInputs |
		ImGuiWindowFlags_NoNav |
		ImGuiWindowFlags_NoMove);

	ImGui::Text("Score : %d", score);
	ImGui::Text("Vidas  : %d", life);
	ImGui::Text("Stage  : %d", stage);
	ImGui::Text("Blocos : %d", blocksRemaining);

	ImGui::End();
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}