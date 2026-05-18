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
	D3D11_VIEWPORT viewport = {}; viewport.TopLeftX = 0; viewport.TopLeftY = 0; viewport.Width = (FLOAT)g_currentWidth; viewport.Height = (FLOAT)g_currentHeight; viewport.MinDepth = 0.0f; viewport.MaxDepth = 1.0f; deviceContext->RSSetViewports(1, &viewport);
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

// ==========================================
// Carregamento lazy da fonte customizada do menu.
// ----------------------------------------------------------------------
// O ImGui mantem um atlas global de fontes (io.Fonts). Para trocar a
// familia em runtime, e' preciso limpar o atlas, adicionar a nova .ttf,
// reconstruir o atlas e invalidar as texturas do backend D3D11.
// A operacao e' cara — por isso so' e' executada quando o campo
// editorMenuConfig.fontPath muda de fato (comparacao com cache).
// O tamanho base e' fixo (32 px); o tamanho final do texto e' controlado
// no AddText, com escalamento via GPU.
// ==========================================
static ImFont*     s_menuFont         = nullptr;
static std::string s_menuFontLastPath = "(uninitialized)";

static void EnsureMenuFontLoaded()
{
	std::string current = editorMenuConfig.fontPath ? editorMenuConfig.fontPath : "";
	if (current == s_menuFontLastPath) return; // ja carregada

	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->Clear();
	io.Fonts->AddFontDefault(); // fallback obrigatorio

	s_menuFont = nullptr;
	if (!current.empty()) {
		// Falha silenciosa: se o arquivo nao existir ou for invalido,
		// AddFontFromFileTTF retorna nullptr e o overlay cai na fonte default.
		s_menuFont = io.Fonts->AddFontFromFileTTF(current.c_str(), 32.0f);
	}

	io.Fonts->Build();
	ImGui_ImplDX11_InvalidateDeviceObjects();
	ImGui_ImplDX11_CreateDeviceObjects();

	s_menuFontLastPath = current;
}

// Desenha os labels do menu principal sobre os botoes via ImGui (overlay
// renderizado no mesmo frame DX, em vez do GDI antigo, que vivia entre o
// desenho do backbuffer e o Present — provocando flicker visivel).
// Cor, tamanho e familia da fonte sao parametrizaveis em editorMenuConfig.
void RenderMenuTextOverlay()
{
	// Atualiza o atlas se a familia (caminho .ttf) mudou desde o ultimo frame.
	// Deve ocorrer ANTES de ImGui_ImplDX11_NewFrame para nao interferir com
	// recursos em uso no quadro corrente.
	EnsureMenuFontLoaded();

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	ImGuiIO& io = ImGui::GetIO();
	const float w = io.DisplaySize.x;
	const float h = io.DisplaySize.y;

	// Janela fullscreen sem decoracao, transparente e sem captura de input:
	// serve apenas como camada de texto sobreposta aos botoes do menu.
	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImVec2(w, h));
	ImGui::SetNextWindowBgAlpha(0.0f);
	ImGui::Begin("##menu_text", nullptr,
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_NoInputs |
		ImGuiWindowFlags_NoNav |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoBringToFrontOnFocus);

	// Mesma geometria utilizada por RenderMenu: startY 0.2, spacing 0.3 (NDC).
	const float startY  = 0.2f;
	const float spacing = 0.3f;

	// Labels em UTF-8 (flag /utf-8 garante interpretacao correta dos bytes).
	static const char* s_labels[] = {
		"Jogar",
		"Configura\xC3\xA7\xC3\xB5""es", // 'ç' = C3 A7, 'õ' = C3 B5
		"Sair"
	};
	const int labelCount = (int)(sizeof(s_labels) / sizeof(s_labels[0]));

	// Fonte e tamanho parametrizaveis. Quando nao ha fonte custom carregada,
	// usa-se a default do ImGui (continua respeitando o textSize via AddText).
	ImFont* font = s_menuFont ? s_menuFont : ImGui::GetFont();
	float fontSize = (editorMenuConfig.textSize > 4.0f) ? editorMenuConfig.textSize : 24.0f;

	auto toCol = [](float r, float g, float b, float a) {
		auto clamp01 = [](float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); };
		return IM_COL32((int)(clamp01(r) * 255.0f),
		                (int)(clamp01(g) * 255.0f),
		                (int)(clamp01(b) * 255.0f),
		                (int)(clamp01(a) * 255.0f));
	};
	const ImU32 colNormal = toCol(editorMenuConfig.textColorR,
	                              editorMenuConfig.textColorG,
	                              editorMenuConfig.textColorB,
	                              editorMenuConfig.textColorA);
	const ImU32 colSelected = toCol(editorMenuConfig.textSelectedColorR,
	                                editorMenuConfig.textSelectedColorG,
	                                editorMenuConfig.textSelectedColorB,
	                                editorMenuConfig.textSelectedColorA);

	for (int i = 0; i < labelCount && i < mainMenuCount; i++) {
		float ndcY    = startY - i * spacing;
		float pxYCenter = (1.0f - ndcY) * 0.5f * h;

		const char* label = s_labels[i];
		ImVec2 sz = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, label);
		float pxX = w * 0.5f - sz.x * 0.5f;
		float pxY = pxYCenter - sz.y * 0.5f;

		bool sel = (selectedMenuIndex == i);
		ImGui::GetWindowDrawList()->AddText(font, fontSize,
			ImVec2(pxX, pxY),
			sel ? colSelected : colNormal,
			label);
	}

	ImGui::End();
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
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

// Forward declaration — defined later in this file alongside the preview helpers
static void DrawQuadColor(float cx, float cy, float hw, float hh,
	float r, float g, float b, float a);

// ==========================================
// RENDER BOSS
// ==========================================

void RenderBoss()
{
	if (!g_boss.active) return;
	const BossConfig& cfg = g_boss.config;
	const float hw = cfg.width  * 0.5f;
	const float hh = cfg.height;

	if (cfg.archetype == BOSS_ARCH_MULTIPART) {
		// Each active node at its own world position
		for (int ni = 0; ni < cfg.nodeCount && ni < BOSS_MAX_NODES; ni++) {
			if (!g_boss.nodeActive[ni]) continue;
			const float nx = g_boss.nodeX[ni], ny = g_boss.nodeY[ni];
			if (g_boss.nodeSRVs[ni])
				DrawTexturedQuad(g_boss.nodeSRVs[ni], nx - hw, ny, nx + hw, ny + hh);
			else
				DrawQuadColor(nx, ny + hh * 0.5f, hw, hh * 0.5f, 0.75f, 0.2f, 0.75f, 1.0f);
		}
	}
	else {
		// Main boss sprite
		if (g_boss.textureSRV) {
			DrawTexturedQuad(g_boss.textureSRV,
				g_boss.x - hw, g_boss.y, g_boss.x + hw, g_boss.y + hh);
		} else {
			const float r = g_boss.invincible ? 0.45f : 0.9f;
			DrawQuadColor(g_boss.x, g_boss.y + hh * 0.5f, hw, hh * 0.5f, r, 0.2f, 0.2f, 1.0f);
		}
		// Familiars (STATIC_FAMILIARS)
		if (cfg.archetype == BOSS_ARCH_STATIC_FAMILIARS) {
			for (int i = 0; i < cfg.familiarCount && i < BOSS_MAX_FAMILIARS; i++) {
				const FamiliarConfig& fam = cfg.familiars[i];
				const float fx = g_boss.x + fam.relOffsetX +
					cosf(g_boss.familiarAngles[i]) * fam.orbitRadius;
				const float fy = g_boss.y + fam.relOffsetY +
					sinf(g_boss.familiarAngles[i]) * fam.orbitRadius;
				const float fhw = 0.04f;
				if (g_boss.familiarSRVs[i])
					DrawTexturedQuad(g_boss.familiarSRVs[i],
						fx - fhw, fy - fhw, fx + fhw, fy + fhw);
				else
					DrawQuadColor(fx, fy, fhw, fhw, 0.4f, 0.8f, 0.4f, 1.0f);
			}
		}
	}

	// HP bar drawn above the boss
	if (cfg.maxHP > 0) {
		float hpPct = (cfg.maxHP > 0) ? (float)g_boss.hp / (float)cfg.maxHP : 0.0f;
		if (hpPct < 0.0f) hpPct = 0.0f;
		const float barY  = g_boss.y + hh + 0.04f;
		const float barHW = hw;
		const float barHH = 0.015f;
		DrawQuadColor(g_boss.x, barY, barHW, barHH, 0.25f, 0.25f, 0.25f, 1.0f);
		if (hpPct > 0.0f)
			DrawQuadColor(g_boss.x - barHW + barHW * hpPct, barY,
				barHW * hpPct, barHH, 0.85f, 0.15f, 0.15f, 1.0f);
	}
}

// ==========================================
// RenderPortals — desenha cada portal ativo no vetor global `portals`.
// Comportamento:
//   - Se o portal possui textureSRV vinculada, utiliza-se o pixel shader
//     texturizado via DrawTexturedQuad, respeitando a regiao (x, y, width,
//     height) da estrutura Portal.
//   - Caso contrario (fallback de desenvolvimento), desenha-se um quad
//     solido ciano por meio do pipeline colorido (pixelShaderBlock),
//     permitindo identificar visualmente portais sem sprite atribuido.
// ==========================================
void RenderPortals()
{
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
			// Fallback colorido: ciano, util durante a fase de criacao do estagio.
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
			deviceContext->IASetInputLayout(inputLayout);
			deviceContext->VSSetShader(vertexShader, nullptr, 0);
			deviceContext->PSSetShader(pixelShaderBlock, nullptr, 0);
			deviceContext->PSSetConstantBuffers(0, 1, &blockColorBuffer);
			UINT s = sizeof(Vertex), o = 0;
			deviceContext->IASetVertexBuffers(0, 1, &obstacleBuffer, &s, &o);
			deviceContext->Draw(6, 0);
		}
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
		ID3D11ShaderResourceView* paddleSRV = editorPlayerTexture; // idle (fallback base)

		if (dashActive) {
			// Dash tem precedência; usa dashDir (nunca paddleMoveDir, que é zerado durante o dash)
			if (dashDir < 0.0f)
				paddleSRV = editorPlayerDashLeftTexture  ? editorPlayerDashLeftTexture
				                                        : editorPlayerRunLeftTexture;
			else
				paddleSRV = editorPlayerDashRightTexture ? editorPlayerDashRightTexture
				                                        : editorPlayerRunRightTexture;
			// Se nem a sprite de run estiver disponível, cai para idle (paddleSRV já aponta para ele)
			if (!paddleSRV) paddleSRV = editorPlayerTexture;
		}
		else {
			if (paddleMoveDir < 0 && editorPlayerRunLeftTexture)
				paddleSRV = editorPlayerRunLeftTexture;
			else if (paddleMoveDir > 0 && editorPlayerRunRightTexture)
				paddleSRV = editorPlayerRunRightTexture;
		}

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

	RenderPortals();

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
	if (currentStageMode == STAGE_BOSS) RenderBoss();
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
		case 2: dr = 1.0f; dg = 0.7f; db = 0.1f; break; // (reservado) = laranja
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

void ResizeViewport(int width, int height)
{
	if (!device || !swapChain || !deviceContext || width <= 0 || height <= 0) return;

	deviceContext->OMSetRenderTargets(0, nullptr, nullptr);
	if (renderTargetView) { renderTargetView->Release(); renderTargetView = nullptr; }

	HRESULT hr = swapChain->ResizeBuffers(0, (UINT)width, (UINT)height, DXGI_FORMAT_UNKNOWN, 0);
	if (FAILED(hr)) return;

	ID3D11Texture2D* backBuffer = nullptr;
	if (FAILED(swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&backBuffer)) || !backBuffer) return;
	device->CreateRenderTargetView(backBuffer, nullptr, &renderTargetView);
	backBuffer->Release();

	deviceContext->OMSetRenderTargets(1, &renderTargetView, nullptr);

	D3D11_VIEWPORT viewport = {};
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width    = (FLOAT)width;
	viewport.Height   = (FLOAT)height;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	deviceContext->RSSetViewports(1, &viewport);
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
// Rajada do demo (preview do editor) — espelha a logica do gameplay
// para que o autor veja o padrao se montando aos poucos.
static BulletBurst s_demoBurst = {};

static float s_demoBX = 0.47f, s_demoBY = 0.5f;
static int   s_demoBActIdx = 0;
static float s_demoBTimer = 0.0f;
static bool  s_demoBInit = false;
// Estado por familiar para animacao no preview: angulo de orbita atual.
static float s_demoFamAngles[BOSS_MAX_FAMILIARS] = {};

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
	s_demoBurst = {};
	s_demoEInit = true;
}

// Estado da simulacao de tiros no preview de boss.
// Alvo virtual fixo onde o paddle real apareceria em gameplay.
static std::vector<EnemyBullet> s_demoBossBullets;
static int   s_demoBossLastActIdx  = -1;
static constexpr float kBossVirtualPaddleX = 0.0f;
static constexpr float kBossVirtualPaddleY = -0.6f;

// ==========================================
// Mini-burst para o demo do boss.
// Replica a logica de Game.cpp:StartBurst/TickBurst em escala reduzida:
//   - patterns 0 (leque) e 2 (radial) sao emitidos instantaneamente.
//   - patterns 1 (velocidades crescentes), 3 (espiral) e 5 (reto p/ baixo)
//     emitem um tiro a cada `stepFrames` frames via DemoBossTickBurst.
// Permite que o demo respeite o act.bulletPattern configurado, em vez de
// usar leque fixo independente do tipo.
// ==========================================
struct DemoBossBurst {
	int   pattern;
	int   count;
	int   idx;
	int   shotsRemaining;
	float speed;
	float angle;        // base (calculada na entrada da acao)
	int   stepFrames;
	int   subTimer;
};

static void DemoBossStartBurst(DemoBossBurst& brst, const BossAction& act,
	float originX, float originY, float targetX, float targetY,
	std::vector<EnemyBullet>& outBullets)
{
	const int   cnt = (act.bulletCount > 0) ? act.bulletCount : 1;
	const float spd = (act.bulletSpeed > 0.0f) ? act.bulletSpeed : 0.012f;
	const int   pat = act.bulletPattern;
	const float baseAng = atan2f(targetY - originY, targetX - originX);

	brst.pattern = pat;
	brst.count   = cnt;
	brst.speed   = spd;
	brst.idx     = 0;
	brst.angle   = baseAng;
	brst.stepFrames = 4;
	brst.subTimer   = brst.stepFrames; // primeiro tiro sai no proximo tick
	brst.shotsRemaining = 0;

	if (pat == 0 || pat == 2) {
		// Padroes simultaneos: emite tudo agora.
		for (int i = 0; i < cnt; i++) {
			float a;
			if (pat == 0) {
				const float spread = 1.0f;
				a = (cnt > 1) ? (baseAng - spread * 0.5f + spread * i / (cnt - 1))
					: baseAng;
			}
			else { // pat == 2: radial
				a = (2.0f * 3.14159265f * i) / (float)cnt;
			}
			EnemyBullet b{};
			b.x = originX; b.y = originY; b.size = 0.012f; b.active = true;
			b.vx = cosf(a) * spd;
			b.vy = sinf(a) * spd;
			outBullets.push_back(b);
		}
		return;
	}

	// Padroes sequenciais (1, 3, 5): TickBurst emitira em frames subsequentes.
	brst.shotsRemaining = cnt;
}

// Emite no maximo um tiro por chamada quando ha rajada sequencial pendente.
// originX/Y atualizam a cada chamada para que tiros saiam da posicao
// corrente do boss (que pode estar se movendo).
static void DemoBossTickBurst(DemoBossBurst& brst, float originX, float originY,
	std::vector<EnemyBullet>& outBullets)
{
	if (brst.shotsRemaining <= 0) return;
	brst.subTimer++;
	if (brst.subTimer < brst.stepFrames) return;
	brst.subTimer = 0;

	const int   i   = brst.idx;
	float       a   = brst.angle;
	float       spd = brst.speed;

	switch (brst.pattern) {
	case 1: spd = 0.005f + i * 0.002f; break;        // velocidades crescentes
	case 3: a   = i * 0.5f;                          // espiral
	        spd = 0.003f + i * 0.0003f; break;
	case 5: a   = -1.5708f; break;                   // reto p/ baixo (-PI/2)
	default: break;                                  // 0 e 2 sao instantaneos
	}

	EnemyBullet b{};
	b.x = originX; b.y = originY; b.size = 0.012f; b.active = true;
	b.vx = cosf(a) * spd;
	b.vy = sinf(a) * spd;
	outBullets.push_back(b);

	brst.idx++;
	brst.shotsRemaining--;
}

static DemoBossBurst s_demoBossBurst = {};

static void ResetBossDemo()
{
	s_demoBX = (editorBossConfig.startX != 0.0f) ? editorBossConfig.startX : PREVIEW_CX;
	s_demoBY = (editorBossConfig.startY != 0.0f) ? editorBossConfig.startY : 0.5f;
	s_demoBActIdx = 0; s_demoBTimer = 0.0f;
	for (int i = 0; i < BOSS_MAX_FAMILIARS; i++) s_demoFamAngles[i] = 0.0f;
	s_demoBossBullets.clear();
	s_demoBossLastActIdx = -1;
	s_demoBossBurst = {};
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

// ==========================================
// Estado do demo do Stage Editor.
// ----------------------------------------------------------------------
// Quando g_editorStageDemoActive == true, o preview do stage anima os
// blocos posicionados conforme seus parametros de movType/movSpeed/
// movAmplitude/movRadius e emite projeteis em rajadas periodicas. A
// finalidade e' que o autor visualize o "feel" do estagio sem precisar
// salvar, sair do editor e entrar em gameplay.
// Reconstroi a lista a cada vez que o demo e' (re)ativado, lendo o JSON
// referenciado por cada PlacedObject via LoadBlockConfigFromFile.
// ==========================================
struct StageDemoBlock {
	float originX, originY;
	float curX,    curY;
	float hw, hh;
	float movAngle;
	EnemyMovType movType;
	float movSpeed, movAmplitude, movRadius;
	int   bulletPattern, bulletCount;
	float bulletSpeed;
	int   shootIntervalFrames;
	int   shootTimer;
	float colorR, colorG, colorB, colorA;
	ID3D11ShaderResourceView* srv;
};

// Boss instanciado dentro do demo do Stage (no maximo um por estagio).
// Reproduz simplificacao do RenderPreview_Boss usando sempre a fase 0
// (HP cheio) e direcionando tiros a um paddle virtual em (0, -0.6).
struct StageDemoBoss {
	bool   active;
	int    placedIndex;          // indice no stageObjects para skip no render
	float  curX, curY;           // posicao animada
	int    actIdx;               // passo corrente do script
	float  actTimer;             // tempo decorrido na acao (segundos)
	int    lastShootActIdx;      // detecta entrada em nova acao
	DemoBossBurst burst;         // rajada conforme bulletPattern
	float  hw, hh;               // meio-dimensoes do quad
	BossConfig config;           // copia independente da config global
	ID3D11ShaderResourceView* srv;
};
static std::vector<StageDemoBlock>  s_demoStageBlocks;
static std::vector<EnemyBullet>     s_demoStageBullets;
static StageDemoBoss                s_demoStageBoss;
static bool                         s_demoStageInit = false;

static void InitStageDemo()
{
	s_demoStageBlocks.clear();
	s_demoStageBullets.clear();
	s_demoStageBoss = {}; // reset

	for (int i = 0; i < (int)stageObjects.size(); i++) {
		auto& o = stageObjects[i];

		if (o.type == PLACED_BLOCK) {
			BlockConfig cfg = editorBlockConfig; // defaults antes do load
			if (o.configFile[0] != '\0')
				LoadBlockConfigFromFile(o.configFile, cfg);
			StageDemoBlock d{};
			d.originX = o.x; d.originY = o.y;
			d.curX    = o.x; d.curY    = o.y;
			float w = (o.previewW > 0.0f) ? o.previewW : cfg.width;
			float h = (o.previewH > 0.0f) ? o.previewH : cfg.height;
			d.hw = w * 0.5f; d.hh = h * 0.5f;
			d.movAngle = 0.0f;
			d.movType      = cfg.movType;
			d.movSpeed     = cfg.movSpeed;
			d.movAmplitude = cfg.movAmplitude;
			d.movRadius    = cfg.movRadius;
			d.bulletPattern      = cfg.bulletPattern;
			d.bulletCount        = (cfg.bulletCount > 0) ? cfg.bulletCount : 1;
			d.bulletSpeed        = (cfg.bulletSpeed > 0.0f) ? cfg.bulletSpeed : 0.008f;
			d.shootIntervalFrames= (cfg.shootIntervalFrames > 0) ? cfg.shootIntervalFrames : 60;
			d.shootTimer         = (int)(d.shootIntervalFrames * 0.5f); // dessincroniza arranque
			d.colorR = cfg.colorR; d.colorG = cfg.colorG;
			d.colorB = cfg.colorB; d.colorA = cfg.colorA;
			d.srv    = o.previewSRV;
			s_demoStageBlocks.push_back(d);
		}
		else if (o.type == PLACED_BOSS && !s_demoStageBoss.active) {
			// Carrega a BossConfig do JSON. LoadBossConfig sobrescreve a
			// global editorBossConfig — preservamos e restauramos para nao
			// poluir o estado da aba Boss.
			BossConfig savedConfig = editorBossConfig;
			if (o.configFile[0] != '\0')
				LoadBossConfig(o.configFile);
			s_demoStageBoss.config = editorBossConfig;
			editorBossConfig = savedConfig;

			s_demoStageBoss.active      = true;
			s_demoStageBoss.placedIndex = i;
			s_demoStageBoss.curX = (o.x != 0.0f) ? o.x : s_demoStageBoss.config.startX;
			s_demoStageBoss.curY = (o.y != 0.0f) ? o.y : s_demoStageBoss.config.startY;
			s_demoStageBoss.actIdx       = 0;
			s_demoStageBoss.actTimer     = 0.0f;
			s_demoStageBoss.lastShootActIdx = -1;
			s_demoStageBoss.burst        = {};
			float bw = (s_demoStageBoss.config.width  > 0.0f) ? s_demoStageBoss.config.width  : 0.2f;
			float bh = (s_demoStageBoss.config.height > 0.0f) ? s_demoStageBoss.config.height : 0.2f;
			s_demoStageBoss.hw = bw * 0.5f;
			s_demoStageBoss.hh = bh * 0.5f;
			s_demoStageBoss.srv = o.previewSRV;
		}
	}
	s_demoStageInit = true;
}

// Atualiza o boss do demo do stage por frame: avanca o script da fase 0
// (sem variacao de HP), aciona disparos e mantem o estado de movimento.
// Tiros sao direcionados a um "paddle virtual" em (0, -0.6), mesma logica
// usada no preview do Boss Editor.
static void UpdateStageDemoBoss()
{
	if (!s_demoStageBoss.active) return;
	const BossConfig& cfg = s_demoStageBoss.config;
	if (cfg.hpPhaseCount <= 0) return;

	// Sempre fase 0 no demo do stage (sem slider de HP nesta tela).
	const BossScript& sc = cfg.hpPhases[0].script;
	if (sc.actionCount <= 0) return;

	int actIdx = s_demoStageBoss.actIdx % sc.actionCount;
	const BossAction& act = sc.actions[actIdx];
	s_demoStageBoss.actTimer += 1.0f / 60.0f;

	constexpr float kVirtualPaddleX = 0.0f;
	constexpr float kVirtualPaddleY = -0.6f;

	// Detecta entrada em nova acao.
	//  - SHOOT_FIXED_PTS: uma bullet por ponto fixo (instantaneo).
	//  - SHOOT_TIMED: inicia rajada conforme act.bulletPattern via helper
	//    compartilhado (DemoBossStartBurst); 0/2 emitem tudo agora, 1/3/5
	//    emitem gradualmente em DemoBossTickBurst.
	if (actIdx != s_demoStageBoss.lastShootActIdx) {
		s_demoStageBoss.lastShootActIdx = actIdx;
		s_demoStageBoss.burst = {};

		if (act.type == BOSS_ACT_SHOOT_FIXED_PTS) {
			const float spd = (act.bulletSpeed > 0) ? act.bulletSpeed : 0.012f;
			for (int fp = 0; fp < act.fixedPointCount && fp < 8; fp++) {
				float dx = act.fixedPtsX[fp] - s_demoStageBoss.curX;
				float dy = act.fixedPtsY[fp] - s_demoStageBoss.curY;
				float ang = atan2f(dy, dx);
				EnemyBullet b{};
				b.x = s_demoStageBoss.curX; b.y = s_demoStageBoss.curY;
				b.vx = cosf(ang) * spd;
				b.vy = sinf(ang) * spd;
				b.size = 0.012f; b.active = true;
				s_demoStageBullets.push_back(b);
			}
		}
		else if (act.type == BOSS_ACT_SHOOT_TIMED) {
			DemoBossStartBurst(s_demoStageBoss.burst, act,
				s_demoStageBoss.curX, s_demoStageBoss.curY,
				kVirtualPaddleX, kVirtualPaddleY,
				s_demoStageBullets);
		}
	}

	// Emissao gradual de patterns sequenciais (1, 3, 5).
	if (act.type == BOSS_ACT_SHOOT_TIMED) {
		DemoBossTickBurst(s_demoStageBoss.burst,
			s_demoStageBoss.curX, s_demoStageBoss.curY,
			s_demoStageBullets);
	}

	switch (act.type) {
	case BOSS_ACT_MOVE_TO: {
		float dx = act.targetX - s_demoStageBoss.curX;
		float dy = act.targetY - s_demoStageBoss.curY;
		float len = sqrtf(dx * dx + dy * dy);
		float sp = (act.speed > 0) ? act.speed : 0.01f;
		if (len <= sp || len < 0.005f) {
			s_demoStageBoss.curX = act.targetX;
			s_demoStageBoss.curY = act.targetY;
			s_demoStageBoss.actIdx++; s_demoStageBoss.actTimer = 0.0f;
		}
		else {
			s_demoStageBoss.curX += (dx / len) * sp;
			s_demoStageBoss.curY += (dy / len) * sp;
		}
		break;
	}
	case BOSS_ACT_TELEPORT:
		s_demoStageBoss.curX = act.targetX;
		s_demoStageBoss.curY = act.targetY;
		s_demoStageBoss.actIdx++; s_demoStageBoss.actTimer = 0.0f; break;
	case BOSS_ACT_WAIT:
	case BOSS_ACT_SHOOT_TIMED:
	case BOSS_ACT_SHOOT_FIXED_PTS:
		if (s_demoStageBoss.actTimer >= act.duration) {
			s_demoStageBoss.actIdx++; s_demoStageBoss.actTimer = 0.0f;
		} break;
	default:
		if (s_demoStageBoss.actTimer >= 1.0f) {
			s_demoStageBoss.actIdx++; s_demoStageBoss.actTimer = 0.0f;
		} break;
	}
	if (s_demoStageBoss.actIdx >= sc.actionCount)
		s_demoStageBoss.actIdx = sc.loopFromStep;
}

// Emite bullets baseando-se no pattern. Versao simplificada da logica de
// gameplay (Game.cpp) — suficiente para visualizacao no editor.
static void EmitStageDemoBullets(const StageDemoBlock& d)
{
	const float pi2  = 2.0f * 3.14159265f;
	const int   cnt  = d.bulletCount;
	const float spd  = d.bulletSpeed;
	for (int i = 0; i < cnt; i++) {
		float a;
		switch (d.bulletPattern) {
		case 0: { // leque para baixo
			float spread = 1.0f;
			a = -3.14159265f * 0.5f
				+ (cnt > 1 ? -spread * 0.5f + spread * i / (cnt - 1) : 0.0f);
		} break;
		case 1: a = -3.14159265f * 0.5f; break;               // reto para baixo
		case 2: a = pi2 * i / (cnt > 0 ? cnt : 1); break;      // radial
		case 3: a = i * 0.5f; break;                          // espiral
		case 4: a = pi2 * ((float)rand() / RAND_MAX); break;  // aleatorio
		case 5: a = -3.14159265f * 0.5f; break;               // para baixo fixo
		default: a = -3.14159265f * 0.5f; break;
		}
		EnemyBullet b{};
		b.x = d.curX; b.y = d.curY; b.size = 0.012f; b.active = true;
		b.vx = cosf(a) * spd; b.vy = sinf(a) * spd;
		s_demoStageBullets.push_back(b);
	}
}

static void RenderPreview_Stage()
{
	// Background do stage (cobre o viewport inteiro).
	if (editorStageEditorConfig.useTextureBg && stageBgTexture)
		DrawTexturedQuad(stageBgTexture, -1.0f, -1.0f, 1.0f, 1.0f);
	else
		DrawQuadColor(0.0f, 0.0f, 1.0f, 1.0f,
			editorStageEditorConfig.bgColorR, editorStageEditorConfig.bgColorG,
			editorStageEditorConfig.bgColorB, editorStageEditorConfig.bgColorA);

	DrawBall(editorStageConfig.ballStartX, editorStageConfig.ballStartY, ballSize, 0.2f, 1.0f, 0.3f);

	// ===== MODO DEMO =====
	if (g_editorStageDemoActive) {
		// (Re)inicializa quando o demo e' ligado, ou quando a lista de
		// objetos foi alterada (acrescentou/removeu blocos ou boss).
		const int curBlocks = (int)std::count_if(stageObjects.begin(), stageObjects.end(),
			[](const PlacedObject& o) { return o.type == PLACED_BLOCK; });
		bool curHasBoss = false;
		for (auto& o : stageObjects)
			if (o.type == PLACED_BOSS) { curHasBoss = true; break; }
		if (!s_demoStageInit ||
			(int)s_demoStageBlocks.size() != curBlocks ||
			s_demoStageBoss.active != curHasBoss) {
			InitStageDemo();
		}

		// Atualiza cada bloco do demo: movimento + emissao periodica.
		for (auto& d : s_demoStageBlocks) {
			d.movAngle += d.movSpeed;
			switch (d.movType) {
			case MOV_VERTICAL:
				d.curX = d.originX;
				d.curY = d.originY + sinf(d.movAngle) * d.movAmplitude;
				break;
			case MOV_HORIZONTAL:
				d.curX = d.originX + sinf(d.movAngle) * d.movAmplitude;
				d.curY = d.originY;
				break;
			case MOV_CIRCULAR:
				d.curX = d.originX + cosf(d.movAngle) * d.movRadius;
				d.curY = d.originY + sinf(d.movAngle) * d.movRadius;
				break;
			case MOV_NONE:
			default:
				d.curX = d.originX; d.curY = d.originY;
				break;
			}
			d.shootTimer++;
			if (d.shootTimer >= d.shootIntervalFrames) {
				d.shootTimer = 0;
				EmitStageDemoBullets(d);
			}
		}

		// Atualiza o boss (se presente): script + disparos.
		UpdateStageDemoBoss();

		// Update bullets (move + descarta fora do viewport).
		for (auto& b : s_demoStageBullets) {
			if (!b.active) continue;
			b.x += b.vx; b.y += b.vy;
			if (b.x < -1.1f || b.x > 1.1f || b.y < -1.1f || b.y > 1.1f)
				b.active = false;
		}
		s_demoStageBullets.erase(
			std::remove_if(s_demoStageBullets.begin(), s_demoStageBullets.end(),
				[](const EnemyBullet& b) { return !b.active; }),
			s_demoStageBullets.end());
	}
	else if (s_demoStageInit) {
		// Demo desligado: descarta estado para reiniciar limpo na proxima vez.
		s_demoStageBlocks.clear();
		s_demoStageBullets.clear();
		s_demoStageBoss = {};
		s_demoStageInit = false;
	}

	// ===== DESENHO DOS OBJETOS =====
	auto drawPlacedTexturedOrColor = [](float ox, float oy, ID3D11ShaderResourceView* srv,
		float hw, float hh, float r, float g, float b, float a)
	{
		if (srv) {
			DrawTexturedQuad(srv, ox - hw, oy - hh, ox + hw, oy + hh);
		}
		else {
			DrawQuadColor(ox, oy, hw, hh, r, g, b, a);
		}
	};

	auto sizeFor = [](const PlacedObject& o, float fallbackW, float fallbackH,
		float& outHw, float& outHh)
	{
		outHw = ((o.previewW > 0.0f) ? o.previewW : fallbackW) * 0.5f;
		outHh = ((o.previewH > 0.0f) ? o.previewH : fallbackH) * 0.5f;
	};

	// Se o demo estiver ativo, blocos vem do s_demoStageBlocks (posicao animada).
	// Caso contrario, blocos vem direto do stageObjects (posicao estatica).
	int demoBlockIdx = 0;
	for (auto& o : stageObjects) {
		float hw, hh;
		switch (o.type) {
		case PLACED_BLOCK:
			if (g_editorStageDemoActive && demoBlockIdx < (int)s_demoStageBlocks.size()) {
				const StageDemoBlock& d = s_demoStageBlocks[demoBlockIdx++];
				drawPlacedTexturedOrColor(d.curX, d.curY, d.srv, d.hw, d.hh,
					d.colorR, d.colorG, d.colorB, d.colorA);
			}
			else {
				sizeFor(o, editorBlockConfig.width, editorBlockConfig.height, hw, hh);
				drawPlacedTexturedOrColor(o.x, o.y, o.previewSRV, hw, hh,
					editorBlockConfig.colorR, editorBlockConfig.colorG,
					editorBlockConfig.colorB, editorBlockConfig.colorA);
			}
			break;
		case PLACED_OBSTACLE:
			sizeFor(o, editorObstacleConfig.width, editorObstacleConfig.height, hw, hh);
			drawPlacedTexturedOrColor(o.x, o.y, o.previewSRV, hw, hh,
				editorObstacleConfig.colorR, editorObstacleConfig.colorG,
				editorObstacleConfig.colorB, editorObstacleConfig.colorA); break;
		case PLACED_BOSS:
			if (g_editorStageDemoActive && s_demoStageBoss.active) {
				// Posicao animada pelo script da fase 0.
				drawPlacedTexturedOrColor(s_demoStageBoss.curX, s_demoStageBoss.curY,
					s_demoStageBoss.srv,
					s_demoStageBoss.hw, s_demoStageBoss.hh,
					0.9f, 0.2f, 0.2f, 1.0f);
			}
			else {
				sizeFor(o, editorBossConfig.width, editorBossConfig.height, hw, hh);
				drawPlacedTexturedOrColor(o.x, o.y, o.previewSRV, hw, hh, 0.9f, 0.2f, 0.2f, 1.0f);
			}
			break;
		case PLACED_PORTAL:
			sizeFor(o, editorPortalConfig.width, editorPortalConfig.height, hw, hh);
			drawPlacedTexturedOrColor(o.x, o.y, o.previewSRV, hw, hh, 0.1f, 0.7f, 0.9f, 1.0f); break;
		default:
			DrawBall(o.x, o.y, ballSize, 0.2f, 1.0f, 0.3f); break;
		}
	}

	// Bullets do demo desenhadas por cima.
	for (auto& b : s_demoStageBullets) {
		if (!b.active) continue;
		float r = b.size * 0.5f;
		DrawQuadColor(b.x, b.y, r, r, 1.0f, 0.45f, 0.15f, 1.0f);
	}
}

// Agenda uma rajada do demo. Para padroes "simultaneos" (leque/radial)
// emite tudo aqui mesmo; para os demais, queue para emissao gradual.
static void StartDemoBurst(float ox, float oy)
{
	s_demoBurst.pattern    = editorBlockConfig.bulletPattern;
	s_demoBurst.count      = (editorBlockConfig.bulletCount > 0)
	                         ? editorBlockConfig.bulletCount : 1;
	s_demoBurst.speed      = (editorBlockConfig.bulletSpeed > 0.0f)
	                         ? editorBlockConfig.bulletSpeed : 0.007f;
	s_demoBurst.originX    = ox;
	s_demoBurst.originY    = oy;
	s_demoBurst.angle      = atan2f(PREVIEW_CY - oy, (PREVIEW_CX - 0.2f) - ox);
	s_demoBurst.stepFrames = (editorBlockConfig.burstStepFrames > 0)
	                         ? editorBlockConfig.burstStepFrames : 4;
	s_demoBurst.idx        = 0;

	auto pushBullet = [](float bx, float by, float a, float spd) {
		EnemyBullet b; b.x = bx; b.y = by; b.size = 0.012f; b.active = true;
		b.vx = cosf(a) * spd; b.vy = sinf(a) * spd;
		s_demoBullets.push_back(b);
	};

	if (s_demoBurst.pattern == 0 || s_demoBurst.pattern == 2) {
		const float pi2 = 2.0f * 3.14159265f;
		for (int i = 0; i < s_demoBurst.count; i++) {
			float a;
			if (s_demoBurst.pattern == 0) {
				float sp = 1.0f;
				a = (s_demoBurst.count > 1)
				    ? (s_demoBurst.angle - sp / 2.0f + sp * i / (s_demoBurst.count - 1))
				    : s_demoBurst.angle;
			} else {
				a = pi2 * i / (float)s_demoBurst.count;
			}
			pushBullet(ox, oy, a, s_demoBurst.speed);
		}
		s_demoBurst.shotsRemaining = 0;
		return;
	}

	s_demoBurst.shotsRemaining = s_demoBurst.count;
	s_demoBurst.subTimer       = s_demoBurst.stepFrames;
}

static void TickDemoBurst()
{
	if (s_demoBurst.shotsRemaining <= 0) return;
	s_demoBurst.subTimer++;
	if (s_demoBurst.subTimer < s_demoBurst.stepFrames) return;
	s_demoBurst.subTimer = 0;

	const int   i      = s_demoBurst.idx;
	const float pAngle = s_demoBurst.angle;
	float       spd    = s_demoBurst.speed;
	float       a      = pAngle;
	const float pi2    = 2.0f * 3.14159265f;

	switch (s_demoBurst.pattern) {
	case 1: spd = 0.005f + i * 0.002f; break;
	case 3: a = i * 0.5f; spd = 0.003f + i * 0.0003f; break;
	case 4: a = pi2 * ((float)rand() / RAND_MAX); break;
	case 5: a = -1.5708f; break;
	default: break;
	}
	EnemyBullet b; b.x = s_demoBurst.originX; b.y = s_demoBurst.originY;
	b.size = 0.012f; b.active = true;
	b.vx = cosf(a) * spd; b.vy = sinf(a) * spd;
	s_demoBullets.push_back(b);
	s_demoBurst.idx++;
	s_demoBurst.shotsRemaining--;
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
		// Tiro: agenda nova rajada quando a anterior termina e o
		// intervalo periodico expira. TickDemoBurst emite um tiro por
		// frame para padroes sequenciais; leque/radial sai todo em
		// StartDemoBurst. Origem do tiro acompanha a posicao corrente
		// do inimigo (importante para padroes em movimento — espiral
		// num inimigo circular nao deixa tiros para tras).
		s_demoBurst.originX = s_demoEX;
		s_demoBurst.originY = s_demoEY;
		TickDemoBurst();
		int interval = (editorBlockConfig.shootIntervalFrames > 0) ? editorBlockConfig.shootIntervalFrames : 90;
		if (s_demoBurst.shotsRemaining <= 0 && ++s_demoShootT >= interval) {
			s_demoShootT = 0; StartDemoBurst(s_demoEX, s_demoEY);
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

	// Posicao inicial em NDC do stage (-1..1). O fallback antigo usava
	// PREVIEW_CX (0.47), que escondia o boss da metade esquerda do stage
	// quando o painel era colapsado para edicao em tela cheia. Como o
	// BossConfig zerado coincide com o centro da tela, este e' um default
	// visivel e tambem consistente com a posicao real em gameplay.
	float startCX = editorBossConfig.startX;
	float startCY = editorBossConfig.startY;
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
				int actIdx = s_demoBActIdx % sc.actionCount;
				BossAction& act = sc.actions[actIdx];
				s_demoBTimer += 1.0f / 60.0f;

				// Disparos: detecta entrada em nova acao.
				//  - SHOOT_FIXED_PTS: rajada unica, uma bullet por ponto fixo.
				//  - SHOOT_TIMED: inicia rajada conforme bulletPattern (via
				//    DemoBossStartBurst — patterns 0/2 emitem tudo, 1/3/5
				//    emitem gradualmente em DemoBossTickBurst abaixo).
				if (actIdx != s_demoBossLastActIdx) {
					s_demoBossLastActIdx = actIdx;
					s_demoBossBurst = {};

					if (act.type == BOSS_ACT_SHOOT_FIXED_PTS) {
						const float spd = (act.bulletSpeed > 0) ? act.bulletSpeed : 0.012f;
						for (int fp = 0; fp < act.fixedPointCount && fp < 8; fp++) {
							float dx = act.fixedPtsX[fp] - s_demoBX;
							float dy = act.fixedPtsY[fp] - s_demoBY;
							float ang = atan2f(dy, dx);
							EnemyBullet b{};
							b.x = s_demoBX; b.y = s_demoBY;
							b.vx = cosf(ang) * spd;
							b.vy = sinf(ang) * spd;
							b.size = 0.012f; b.active = true;
							s_demoBossBullets.push_back(b);
						}
					}
					else if (act.type == BOSS_ACT_SHOOT_TIMED) {
						DemoBossStartBurst(s_demoBossBurst, act,
							s_demoBX, s_demoBY,
							kBossVirtualPaddleX, kBossVirtualPaddleY,
							s_demoBossBullets);
					}
				}

				// Emissao gradual dos padroes sequenciais (1, 3, 5).
				if (act.type == BOSS_ACT_SHOOT_TIMED) {
					DemoBossTickBurst(s_demoBossBurst, s_demoBX, s_demoBY,
						s_demoBossBullets);
				}

				switch (act.type) {
				case BOSS_ACT_MOVE_TO: {
					float dx = act.targetX - s_demoBX, dy = act.targetY - s_demoBY, len = sqrtf(dx * dx + dy * dy);
					float sp = (act.speed > 0) ? act.speed : 0.01f;
					// Snap-to-target: evita o flicker visto no runtime
					// (oscilacao quando sp ultrapassa a distancia restante).
					if (len <= sp || len < 0.005f) {
						s_demoBX = act.targetX; s_demoBY = act.targetY;
						s_demoBActIdx++; s_demoBTimer = 0;
					}
					else {
						s_demoBX += (dx / len) * sp; s_demoBY += (dy / len) * sp;
					}
					break;
				}
				case BOSS_ACT_TELEPORT:
					s_demoBX = act.targetX; s_demoBY = act.targetY;
					s_demoBActIdx++; s_demoBTimer = 0; break;
				case BOSS_ACT_WAIT:
				case BOSS_ACT_SHOOT_TIMED:
				case BOSS_ACT_SHOOT_FIXED_PTS:
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

		// Atualiza posicao das bullets demo e descarta as que sairam do viewport.
		for (auto& b : s_demoBossBullets) {
			if (!b.active) continue;
			b.x += b.vx; b.y += b.vy;
			if (b.x < -1.1f || b.x > 1.1f || b.y < -1.1f || b.y > 1.1f)
				b.active = false;
		}
		s_demoBossBullets.erase(
			std::remove_if(s_demoBossBullets.begin(), s_demoBossBullets.end(),
				[](const EnemyBullet& b) { return !b.active; }),
			s_demoBossBullets.end());
	}
	else {
		// Demo desligado: limpa bullets remanescentes para evitar acumulo
		// quando o usuario reativar.
		if (!s_demoBossBullets.empty()) s_demoBossBullets.clear();
		s_demoBossLastActIdx = -1;
		s_demoBossBurst = {};
	}

	// Draw start position marker (small diamond)
	DrawQuadColor(startCX, startCY, 0.015f, 0.015f, 0.3f, 0.9f, 0.3f, 0.6f);

	// Draw movement target markers for current phase script.
	// MOVE_TO → azul-esverdeado; TELEPORT → laranja claro; FIXED_PTS → laranja
	// escuro; SPAWN_MINION → ciano (novo). Todos arrastaveis em UpdateEditor.
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
			if (act.type == BOSS_ACT_SPAWN_MINION) {
				DrawQuadColor(act.spawnX, act.spawnY, 0.012f, 0.012f, 0.2f, 0.85f, 0.95f, 0.55f);
				// Cruz interna para distinguir do marcador de MOVE_TO.
				DrawQuadColor(act.spawnX, act.spawnY, 0.012f, 0.0015f, 1.0f, 1.0f, 1.0f, 0.8f);
				DrawQuadColor(act.spawnX, act.spawnY, 0.0015f, 0.012f, 1.0f, 1.0f, 1.0f, 0.8f);
			}
		}
	}

	// Draw boss sprite/quad
	if (editorBossTexture)
		DrawQuadTexCentered(editorBossTexture, cx, cy, hw, hh);
	else
		DrawQuadColor(cx, cy, hw, hh, 0.9f, 0.2f, 0.2f, 1.0f);

	// Familiares (BOSS_ARCH_STATIC_FAMILIARS) — desenha a orbita pontilhada
	// + o familiar sobre o ponto atual da orbita. Sem demo ativo, posiciona
	// no angulo inicial (angle=0). Com demo, o angulo avanca conforme
	// fam.orbitSpeed (mesma logica de gameplay).
	if (editorBossConfig.archetype == BOSS_ARCH_STATIC_FAMILIARS) {
		for (int i = 0; i < editorBossConfig.familiarCount && i < BOSS_MAX_FAMILIARS; i++) {
			const FamiliarConfig& fam = editorBossConfig.familiars[i];
			if (editorDemoActive) s_demoFamAngles[i] += fam.orbitSpeed;
			const float ang   = s_demoFamAngles[i];
			const float ocx   = cx + fam.relOffsetX;
			const float ocy   = cy + fam.relOffsetY;
			const float fx    = ocx + cosf(ang) * fam.orbitRadius;
			const float fy    = ocy + sinf(ang) * fam.orbitRadius;

			// Orbita: 24 pontos ao redor do centro relativo do familiar.
			const int   kOrbitDots = 24;
			for (int k = 0; k < kOrbitDots; k++) {
				float a = (2.0f * 3.14159265f * k) / kOrbitDots;
				float dx = ocx + cosf(a) * fam.orbitRadius;
				float dy = ocy + sinf(a) * fam.orbitRadius;
				DrawQuadColor(dx, dy, 0.004f, 0.004f, 0.5f, 0.5f, 0.7f, 0.45f);
			}

			// Sprite do familiar a partir do cache de texturas; fallback colorido
			// (quad laranja com borda) caso nao haja textura associada.
			ID3D11ShaderResourceView* famSrv =
				(fam.texturePath[0]) ? GetCachedTexture(fam.texturePath) : nullptr;
			const float famHw = 0.022f;
			if (famSrv) {
				DrawTexturedQuad(famSrv, fx - famHw, fy - famHw, fx + famHw, fy + famHw);
			}
			else {
				DrawQuadColor(fx, fy, famHw, famHw, 0.95f, 0.6f, 0.15f, 1.0f);
				DrawQuadColor(fx, fy + famHw, famHw, 0.003f, 1.0f, 1.0f, 1.0f, 0.7f);
				DrawQuadColor(fx, fy - famHw, famHw, 0.003f, 1.0f, 1.0f, 1.0f, 0.7f);
			}
		}
	}

	// Multipart nodes — desenha cada node na sua posicao inicial configurada,
	// para que o autor possa visualizar a montagem do boss multipartido.
	// Usa textura via cache quando disponivel; fallback colorido (quad roxo)
	// preserva a visibilidade quando a textura ainda nao foi atribuida.
	if (editorBossConfig.archetype == BOSS_ARCH_MULTIPART) {
		const float nhw = hw * 0.6f;
		const float nhh = hh * 0.6f;
		for (int i = 0; i < editorBossConfig.nodeCount && i < BOSS_MAX_NODES; i++) {
			const MultipartNode& n = editorBossConfig.nodes[i];
			ID3D11ShaderResourceView* nodeSrv =
				(n.texturePath[0]) ? GetCachedTexture(n.texturePath) : nullptr;
			if (nodeSrv) {
				DrawTexturedQuad(nodeSrv,
					n.startX - nhw, n.startY - nhh,
					n.startX + nhw, n.startY + nhh);
			}
			else {
				DrawQuadColor(n.startX, n.startY, nhw, nhh, 0.7f, 0.3f, 0.9f, 0.95f);
				DrawQuadColor(n.startX, n.startY + nhh, nhw, 0.003f, 1.0f, 1.0f, 1.0f, 0.6f);
				DrawQuadColor(n.startX, n.startY - nhh, nhw, 0.003f, 1.0f, 1.0f, 1.0f, 0.6f);
			}
		}
	}

	// Bullets demo (SHOOT_TIMED + SHOOT_FIXED_PTS) — pequenos quads laranja.
	// Desenhadas depois do boss/familiares/nodes para ficarem por cima.
	for (auto& b : s_demoBossBullets) {
		if (!b.active) continue;
		float r = b.size * 0.5f;
		DrawQuadColor(b.x, b.y, r, r, 1.0f, 0.45f, 0.15f, 1.0f);
	}

	// Marcador do "paddle virtual" — alvo dos tiros direcionados ao jogador
	// durante a demonstracao. Apenas referencia visual; nao colide com nada.
	if (editorDemoActive) {
		DrawQuadColor(kBossVirtualPaddleX, kBossVirtualPaddleY,
			0.07f, 0.012f, 0.55f, 0.55f, 0.6f, 0.55f);
	}

	// Barra de HP no topo do stage (NDC). Posicionada perto da borda
	// superior central para nao colidir com a area de movimentacao.
	float barW = 0.5f, barFill = editorDemoBossHPPct * barW;
	float barY = 0.92f, barLeft = -barW * 0.5f;
	DrawQuadColor(0.0f, barY, barW * 0.5f, 0.025f, 0.3f, 0.3f, 0.3f, 1.0f);
	if (barFill > 0)
		DrawQuadColor(barLeft + barFill * 0.5f, barY, barFill * 0.5f, 0.022f, 0.8f, 0.15f, 0.15f, 1.0f);
}

// ---------------------------------------------------------------------------
// Preview do Portal: desenha o sprite (ou retangulo ciano de fallback)
// centralizado na area de preview, com as dimensoes do editorPortalConfig.
// ---------------------------------------------------------------------------
static void RenderPreview_Portal()
{
	float hw = (editorPortalConfig.width  > 0.0f) ? editorPortalConfig.width  * 0.5f : 0.05f;
	float hh = (editorPortalConfig.height > 0.0f) ? editorPortalConfig.height * 0.5f : 0.075f;

	if (editorPortalTexture) {
		DrawQuadTexCentered(editorPortalTexture, PREVIEW_CX, PREVIEW_CY, hw, hh);
	} else {
		// Sem sprite: retangulo ciano semitransparente com borda, para que
		// o autor visualize a hitbox/area mesmo sem textura definida.
		DrawQuadColor(PREVIEW_CX, PREVIEW_CY, hw, hh, 0.1f, 0.7f, 0.9f, 0.85f);
		DrawQuadColor(PREVIEW_CX, PREVIEW_CY + hh - 0.003f, hw, 0.003f, 1.0f, 1.0f, 1.0f, 0.6f);
		DrawQuadColor(PREVIEW_CX, PREVIEW_CY - hh + 0.003f, hw, 0.003f, 1.0f, 1.0f, 1.0f, 0.6f);
		DrawQuadColor(PREVIEW_CX - hw + 0.003f, PREVIEW_CY, 0.003f, hh, 1.0f, 1.0f, 1.0f, 0.6f);
		DrawQuadColor(PREVIEW_CX + hw - 0.003f, PREVIEW_CY, 0.003f, hh, 1.0f, 1.0f, 1.0f, 0.6f);
	}
}

// ---------------------------------------------------------------------------
// RenderEditor — clears + dispatcher de preview
// ---------------------------------------------------------------------------
void RenderEditor()
{
	float clearColor[4] = { 0.08f, 0.08f, 0.12f, 1.0f };
	deviceContext->ClearRenderTargetView(renderTargetView, clearColor);

	// Sem linha divisoria fixa: no modo multi-window cada painel ImGui
	// flutua livremente, entao nao existe mais uma "borda direita do
	// painel" estavel para marcar.

	switch (currentEditorMode) {
	case EDITOR_MODE_PLAYER:   RenderPreview_Player();   break;
	case EDITOR_MODE_BALL:     RenderPreview_Ball();     break;
	case EDITOR_MODE_STAGE:    RenderPreview_Stage();    break;
	case EDITOR_MODE_OBSTACLE: RenderPreview_Obstacle(); break;
	case EDITOR_MODE_ENEMY:    RenderPreview_Enemy();    break;
	case EDITOR_MODE_BOSS:     RenderPreview_Boss();     break;
	case EDITOR_MODE_MENU:     RenderMenu();             break;
	case EDITOR_MODE_PORTAL:   RenderPreview_Portal();   break;
	}
}

// ==========================================
// DEBUG UI â exibida durante gameplay (fora do editor)
// Usa ImGui para mostrar info bÃ¡sica sem bloquear input
// ==========================================
void RenderDebugUI()
{
	// Mantido como wrapper para compatibilidade — o overlay de gameplay
	// agora e' uma HUD propria (RenderGameplayHUD), nao um debug widget.
	RenderGameplayHUD();
}

// ==========================================
// HUD DO GAMEPLAY - overlay ImGui exibido durante STATE_GAMEPLAY.
// ----------------------------------------------------------------------
// Substitui as antigas chamadas DrawScore/DrawLives/DrawStage via GDI,
// que pintavam direto no HDC do hwnd e sumiam a cada Present (flicker).
// Funciona em build editor e em build standalone (g_isEditorEnabled=false).
//
// Layout:
//   [VIDAS x N]                    [STAGE N]                   [SCORE 1234]
//                                  [HP do boss, quando ativo]   [COMBO xN]
//   ...
//   [Blocos: N]  (canto inferior esquerdo, somente fora de boss)
//
// As janelas usam NoBackground + NoInputs + NoNav para nao roubar foco
// do jogo. Posicionamento absoluto em pixels evita problemas de layout
// quando a janela e' redimensionada.
// ==========================================
void RenderGameplayHUD()
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	ImGuiIO& io = ImGui::GetIO();
	const ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_NoInputs |
		ImGuiWindowFlags_NoNav |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoBackground |
		ImGuiWindowFlags_AlwaysAutoResize;

	// --- Vidas (topo esquerdo) ---
	ImGui::SetNextWindowPos(ImVec2(12.0f, 8.0f));
	ImGui::Begin("##hud_lives", nullptr, flags);
	ImGui::SetWindowFontScale(1.35f);
	ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 90, 90, 255));
	ImGui::Text("VIDAS x %d", (life > 0) ? life : 0);
	ImGui::PopStyleColor();
	ImGui::End();

	// --- Stage (topo centro) ---
	{
		char buf[32]; sprintf_s(buf, "STAGE %d", stage + 1);
		ImVec2 textSz = ImGui::CalcTextSize(buf);
		float scale   = 1.2f;
		float winW    = textSz.x * scale + 24.0f;
		ImGui::SetNextWindowPos(ImVec2((io.DisplaySize.x - winW) * 0.5f, 8.0f));
		ImGui::Begin("##hud_stage", nullptr, flags);
		ImGui::SetWindowFontScale(scale);
		ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(230, 230, 230, 255));
		ImGui::TextUnformatted(buf);
		ImGui::PopStyleColor();
		ImGui::End();
	}

	// --- Score + Combo (topo direito) ---
	{
		char buf[64]; sprintf_s(buf, "SCORE %d", score);
		ImVec2 textSz = ImGui::CalcTextSize(buf);
		float scale   = 1.45f;
		float winW    = textSz.x * scale + 24.0f;
		ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - winW - 8.0f, 8.0f));
		ImGui::Begin("##hud_score", nullptr, flags);
		ImGui::SetWindowFontScale(scale);
		ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 220, 90, 255));
		ImGui::TextUnformatted(buf);
		ImGui::PopStyleColor();
		if (combo > 1) {
			ImGui::SetWindowFontScale(scale * 0.7f);
			ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 150, 60, 255));
			ImGui::Text("COMBO x%d", combo);
			ImGui::PopStyleColor();
		}
		ImGui::End();
	}

	// --- Boss HP (topo centro, abaixo do STAGE) ---
	if (currentStageMode == STAGE_BOSS && g_boss.active) {
		const float barW = 380.0f, barH = 14.0f;
		const float winW = barW + 16.0f;
		const float winX = (io.DisplaySize.x - winW) * 0.5f;
		const float winY = 42.0f;
		ImGui::SetNextWindowPos(ImVec2(winX, winY));
		ImGui::SetNextWindowSize(ImVec2(winW, 0.0f));
		ImGui::Begin("##hud_boss", nullptr,
			ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
			ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoSavedSettings);
		if (g_boss.config.name[0]) {
			ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 110, 110, 255));
			ImGui::TextUnformatted(g_boss.config.name);
			ImGui::PopStyleColor();
		}
		float frac = (g_boss.config.maxHP > 0)
			? (float)g_boss.hp / (float)g_boss.config.maxHP : 0.0f;
		if (frac < 0.0f) frac = 0.0f;
		if (frac > 1.0f) frac = 1.0f;
		ImDrawList* dl = ImGui::GetWindowDrawList();
		ImVec2 p = ImGui::GetCursorScreenPos();
		dl->AddRectFilled(p, ImVec2(p.x + barW, p.y + barH), IM_COL32(40, 40, 40, 230));
		if (frac > 0.0f) {
			ImU32 col = (frac > 0.5f) ? IM_COL32(220, 50, 50, 255)
			                          : IM_COL32(240, 130, 30, 255);
			dl->AddRectFilled(p, ImVec2(p.x + barW * frac, p.y + barH), col);
		}
		dl->AddRect(p, ImVec2(p.x + barW, p.y + barH), IM_COL32(255, 255, 255, 180));
		ImGui::Dummy(ImVec2(barW, barH));
		ImGui::End();
	}

	// --- Blocos restantes (canto inferior esquerdo, somente stages normais) ---
	if (currentStageMode != STAGE_BOSS) {
		char buf[32]; sprintf_s(buf, "Blocos: %d", blocksRemaining);
		ImGui::SetNextWindowPos(ImVec2(12.0f, io.DisplaySize.y - 32.0f));
		ImGui::Begin("##hud_blocks", nullptr, flags);
		ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(190, 190, 190, 220));
		ImGui::TextUnformatted(buf);
		ImGui::PopStyleColor();
		ImGui::End();
	}

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

// ==========================================
// OPTIONS — clear de fundo (D3D)
// ==========================================
void RenderOptions()
{
	float clearColor[4] = {
		editorMenuConfig.bgColorR, editorMenuConfig.bgColorG,
		editorMenuConfig.bgColorB, editorMenuConfig.bgColorA
	};
	deviceContext->ClearRenderTargetView(renderTargetView, clearColor);
	DrawMenuBackground(menuBgTexture);
}

// ==========================================
// OPTIONS — UI ImGui (selecao de resolucao)
// Aplica via SetWindowPos + AdjustWindowRect; o WM_SIZE resultante
// dispara ResizeViewport automaticamente.
// ==========================================
void RenderOptionsUI()
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	struct Res { int w, h; const char* label; };
	static const Res kResolutions[] = {
		{ 800,  600,  "800 x 600"   },
		{ 1280, 720,  "1280 x 720"  },
		{ 1920, 1080, "1920 x 1080" },
	};
	static int s_selectedIdx = 0;

	// Sincroniza o indice com a resolucao atual quando a tela abre
	for (int i = 0; i < (int)(sizeof(kResolutions) / sizeof(kResolutions[0])); i++) {
		if (kResolutions[i].w == g_currentWidth && kResolutions[i].h == g_currentHeight) {
			s_selectedIdx = i;
			break;
		}
	}

	ImGuiIO& io = ImGui::GetIO();
	ImVec2 winSize(380.0f, 220.0f);
	ImGui::SetNextWindowPos(ImVec2((io.DisplaySize.x - winSize.x) * 0.5f,
	                               (io.DisplaySize.y - winSize.y) * 0.5f));
	ImGui::SetNextWindowSize(winSize);
	ImGui::Begin("Configuracoes", nullptr,
		ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

	ImGui::Text("Resolucao da janela:");
	ImGui::Spacing();

	const char* items[8]; int itemCount = (int)(sizeof(kResolutions) / sizeof(kResolutions[0]));
	for (int i = 0; i < itemCount; i++) items[i] = kResolutions[i].label;
	ImGui::Combo("##resolucao", &s_selectedIdx, items, itemCount);

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	if (ImGui::Button("Aplicar", ImVec2(120, 0))) {
		const Res& chosen = kResolutions[s_selectedIdx];
		// AdjustWindowRect converte area de cliente para tamanho externo (com bordas)
		RECT rc = { 0, 0, chosen.w, chosen.h };
		LONG style = GetWindowLong(g_hWnd, GWL_STYLE);
		AdjustWindowRect(&rc, style, FALSE);
		SetWindowPos(g_hWnd, nullptr, 0, 0,
			rc.right - rc.left, rc.bottom - rc.top,
			SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
		// O WM_SIZE resultante dispara ResizeViewport automaticamente.
	}
	ImGui::SameLine();
	if (ImGui::Button("Voltar", ImVec2(120, 0))) {
		selectedMenuIndex = 1;
		currentState = GameState::STATE_START_MENU;
	}

	ImGui::TextDisabled("ESC tambem volta ao menu.");

	ImGui::End();
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}