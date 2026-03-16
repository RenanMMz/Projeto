#include "Render.h"
#include <stdio.h>
#include "Editor.h"
#include "Level.h"
#include "./imgui/imgui.h"
#include "./imgui/imgui_impl_win32.h"
#include "./imgui/imgui_impl_dx11.h"

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
	hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pixelShaderPaddle);
	hr = device->CreatePixelShader(psBlobBall->GetBufferPointer(), psBlobBall->GetBufferSize(), nullptr, &pixelShaderBall);
	hr = device->CreatePixelShader(psBlobBlock->GetBufferPointer(), psBlobBlock->GetBufferSize(), nullptr, &pixelShaderBlock);
	hr = device->CreatePixelShader(psBlobObstacle->GetBufferPointer(), psBlobObstacle->GetBufferSize(), nullptr, &pixelShaderObstacle);
	hr = device->CreatePixelShader(psBlobBullet->GetBufferPointer(), psBlobBullet->GetBufferSize(), nullptr, &pixelShaderEnemyBullet);

	D3D11_INPUT_ELEMENT_DESC layout[] = { {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0} };
	hr = device->CreateInputLayout(layout, 1, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &inputLayout);

	vsBlob->Release(); psBlob->Release(); psBlobBall->Release(); psBlobBlock->Release(); psBlobBullet->Release();

	Vertex vertices[] = { {-0.12f, -0.7f, 0.0f}, {-0.12f, -0.75f, 0.0f}, {0.12f, -0.75f, 0.0f}, {-0.12f, -0.7f, 0.0f}, {0.12f, -0.75f, 0.0f}, {0.12f, -0.7f, 0.0f} };
	Vertex ballVertices[6];

	D3D11_BUFFER_DESC bdBall = {}; bdBall.Usage = D3D11_USAGE_DEFAULT; bdBall.ByteWidth = sizeof(Vertex) * _countof(ballVertices); bdBall.BindFlags = D3D11_BIND_VERTEX_BUFFER; D3D11_SUBRESOURCE_DATA initBall = {}; initBall.pSysMem = ballVertices; hr = device->CreateBuffer(&bdBall, &initBall, &ballVertexBuffer);
	D3D11_BUFFER_DESC bdPaddle = {}; bdPaddle.Usage = D3D11_USAGE_DEFAULT; bdPaddle.ByteWidth = sizeof(Vertex) * _countof(vertices); bdPaddle.BindFlags = D3D11_BIND_VERTEX_BUFFER; D3D11_SUBRESOURCE_DATA initPaddle = {}; initPaddle.pSysMem = vertices; hr = device->CreateBuffer(&bdPaddle, &initPaddle, &vertexBuffer);
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

	D3D11_INPUT_ELEMENT_DESC layoutTex[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,                            D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, sizeof(float) * 3,               D3D11_INPUT_PER_VERTEX_DATA, 0},
	};
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
	deviceContext->Draw(6, 0);
}

static void DrawMenuBackground(ID3D11ShaderResourceView* bgTex)
{
	DrawTexturedQuad(bgTex, -1.0f, -1.0f, 1.0f, 1.0f);
}

static void DrawMenuLogo()
{
	if (!menuTitleTexture) return;
	float hw = editorMenuConfig.logoWidth / 2.0f;
	float hh = editorMenuConfig.logoHeight / 2.0f;
	DrawTexturedQuad(menuTitleTexture,
		editorMenuConfig.logoX - hw, editorMenuConfig.logoY - hh,
		editorMenuConfig.logoX + hw, editorMenuConfig.logoY + hh);
}

static void DrawMenuButton(float x1, float y1, float x2, float y2,
	const float color[4], bool selected)
{
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

	if (selected && menuSelectorTexture) {
		float btnH = y2 - y1;
		float iconSz = btnH * 0.7f;                
		float margin = btnH * 0.15f;              
		float ix1 = x1 + margin;
		float ix2 = ix1 + iconSz;
		float iy1 = y1 + (btnH - iconSz) / 2.0f;  
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
	float clearColor[4] = { 0.2f, 0.2f, 0.6f, 1.0f }; deviceContext->ClearRenderTargetView(renderTargetView, clearColor);
	UINT stride = sizeof(Vertex); UINT offset = 0; deviceContext->IASetInputLayout(inputLayout);
	deviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset); deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	deviceContext->VSSetShader(vertexShader, nullptr, 0); deviceContext->PSSetShader(pixelShader, nullptr, 0);

	if (paddleVisible) {
		deviceContext->PSSetShader(pixelShaderPaddle, nullptr, 0); deviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset); deviceContext->Draw(6, 0);
	}
	deviceContext->PSSetShader(pixelShaderBall, nullptr, 0); deviceContext->IASetVertexBuffers(0, 1, &ballVertexBuffer, &stride, &offset); deviceContext->Draw(6, 0);

	deviceContext->PSSetShader(pixelShaderProjectile, nullptr, 0);
	for (auto& p : projectiles) {
		if (!p.active) continue; Vertex projVertices[] = { {p.x - (projectileSize * 0.8f), p.y + (projectileSize * 0.8f), 0.0f}, {p.x - (projectileSize * 0.8f), p.y - (projectileSize * 0.8f), 0.0f}, {p.x + (projectileSize * 0.8f), p.y - (projectileSize * 0.8f), 0.0f}, {p.x - (projectileSize * 0.8f), p.y + (projectileSize * 0.8f), 0.0f}, {p.x + (projectileSize * 0.8f), p.y - (projectileSize * 0.8f), 0.0f}, {p.x + (projectileSize * 0.8f), p.y + (projectileSize * 0.8f), 0.0f} };
		deviceContext->UpdateSubresource(projectileBuffer, 0, nullptr, projVertices, 0, 0); deviceContext->IASetVertexBuffers(0, 1, &projectileBuffer, &stride, &offset); deviceContext->Draw(6, 0);
	}

	deviceContext->PSSetShader(pixelShaderObstacle, nullptr, 0); deviceContext->IASetVertexBuffers(0, 1, &obstacleBuffer, &stride, &offset);
	for (auto& obstacle : obstacles) {
		if (!obstacle.active) continue; Vertex vertices[] = { {obstacle.x - obstacle.width / 2, obstacle.y + obstacle.height, 0.0f}, {obstacle.x - obstacle.width / 2, obstacle.y, 0.0f}, {obstacle.x + obstacle.width / 2, obstacle.y, 0.0f}, {obstacle.x - obstacle.width / 2, obstacle.y + obstacle.height, 0.0f}, {obstacle.x + obstacle.width / 2, obstacle.y, 0.0f}, {obstacle.x + obstacle.width / 2, obstacle.y + obstacle.height, 0.0f} };
		deviceContext->UpdateSubresource(obstacleBuffer, 0, nullptr, vertices, 0, 0); deviceContext->Draw(6, 0);
	}

	deviceContext->PSSetShader(pixelShaderBlock, nullptr, 0); deviceContext->IASetVertexBuffers(0, 1, &blockVertexBuffer, &stride, &offset);
	for (auto& block : blocks) {
		if (!block.active) continue; XMFLOAT4 color;
		if (block.hits >= 3) color = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f); else if (block.hits == 2) color = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f); else if (block.hits == 1) color = XMFLOAT4(0.4f, 0.4f, 0.4f, 1.0f);
		deviceContext->PSSetConstantBuffers(0, 1, &blockColorBuffer); deviceContext->UpdateSubresource(blockColorBuffer, 0, nullptr, &color, 0, 0);
		Vertex vertices[] = { {block.x - block.width / 2, block.y + block.height, 0.0f}, {block.x - block.width / 2, block.y, 0.0f}, {block.x + block.width / 2, block.y, 0.0f}, {block.x - block.width / 2, block.y + block.height, 0.0f}, {block.x + block.width / 2, block.y, 0.0f}, {block.x + block.width / 2, block.y + block.height, 0.0f} };
		deviceContext->UpdateSubresource(blockVertexBuffer, 0, nullptr, vertices, 0, 0); deviceContext->Draw(6, 0);
	}

	deviceContext->PSSetShader(pixelShaderEnemyBullet, nullptr, 0);
	for (auto& bullet : enemyBullets) {
		if (!bullet.active) continue; Vertex verticesBullet[] = { {bullet.x - bullet.size, bullet.y + bullet.size, 0.0f}, {bullet.x - bullet.size, bullet.y - bullet.size, 0.0f}, {bullet.x + bullet.size, bullet.y - bullet.size, 0.0f}, {bullet.x - bullet.size, bullet.y + bullet.size, 0.0f}, {bullet.x + bullet.size, bullet.y - bullet.size, 0.0f}, {bullet.x + bullet.size, bullet.y + bullet.size, 0.0f} };
		deviceContext->IASetVertexBuffers(0, 1, &enemyBulletBuffer, &stride, &offset); deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		deviceContext->UpdateSubresource(enemyBulletBuffer, 0, nullptr, verticesBullet, 0, 0); deviceContext->Draw(6, 0);
	}

	if (forceFieldActive) {
		deviceContext->PSSetShader(pixelShaderProjectile, nullptr, 0); const int segments = 32; std::vector<Vertex> circleVerts; circleVerts.push_back({ forceFieldX, forceFieldY, 0.0f });
		for (int i = 0; i <= segments; i++) {
			float theta = (2 * 3.14159265f * i) / segments; float x = forceFieldX + cosf(theta) * forceFieldRadius; float y = forceFieldY + sinf(theta) * forceFieldRadius; circleVerts.push_back({ x, y, 0.0f });
		}
		std::vector<Vertex> fanVerts; for (int i = 1; i < circleVerts.size() - 1; i++) {
			fanVerts.push_back(circleVerts[0]); fanVerts.push_back(circleVerts[i]); fanVerts.push_back(circleVerts[i + 1]);
		}
		deviceContext->UpdateSubresource(forceFieldBuffer, 0, nullptr, fanVerts.data(), 0, 0); deviceContext->IASetVertexBuffers(0, 1, &forceFieldBuffer, &stride, &offset); deviceContext->Draw(static_cast<UINT>(fanVerts.size()), 0);
	}

	if (dashActive) {
		deviceContext->PSSetShader(pixelShaderProjectile, nullptr, 0); float shieldWidth = 0.25f; float shieldHeight = 0.15f; float shieldY = paddleY;
		Vertex dashShieldVerts[] = { {paddleX - shieldWidth / 2, shieldY + shieldHeight, 0.0f}, {paddleX - shieldWidth / 2, shieldY, 0.0f}, {paddleX + shieldWidth / 2, shieldY, 0.0f}, {paddleX - shieldWidth / 2, shieldY + shieldHeight, 0.0f}, {paddleX + shieldWidth / 2, shieldY, 0.0f}, {paddleX + shieldWidth / 2, shieldY + shieldHeight, 0.0f} };
		deviceContext->UpdateSubresource(dashShieldBuffer, 0, nullptr, dashShieldVerts, 0, 0); deviceContext->IASetVertexBuffers(0, 1, &dashShieldBuffer, &stride, &offset); deviceContext->Draw(6, 0);
	}
}

void CleanD3D() {
	if (swapChain) swapChain->Release(); if (renderTargetView) renderTargetView->Release(); if (deviceContext) deviceContext->Release(); if (device) device->Release();
	if (vertexBuffer) vertexBuffer->Release(); if (vertexShader) vertexShader->Release(); if (pixelShader) pixelShader->Release(); if (inputLayout) inputLayout->Release();
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