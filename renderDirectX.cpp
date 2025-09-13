#define UNICODE
#define _UNICODE
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <vector>
#pragma comment(lib, "D3DCompiler.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

// Variáveis globais DirectX
IDXGISwapChain *swapChain = nullptr;
ID3D11Device *device = nullptr;
ID3D11DeviceContext *deviceContext = nullptr;
ID3D11RenderTargetView *renderTargetView = nullptr;

ID3D11Buffer *vertexBuffer = nullptr;
ID3D11VertexShader *vertexShader = nullptr;
ID3D11PixelShader *pixelShader = nullptr;
ID3D11InputLayout *inputLayout = nullptr;
ID3D11RasterizerState *rasterState = nullptr;
ID3D11Buffer *ballVertexBuffer = nullptr;

ID3D11Buffer *projectileBuffer = nullptr;

ID3D11PixelShader *pixelShaderPaddle = nullptr;     // barrinha
ID3D11PixelShader *pixelShaderBall = nullptr;       // bolinha
ID3D11PixelShader *pixelShaderProjectile = nullptr; // tirinho
// tirinho
bool projectileActive = false;
float projectileX = 0.0f;
float projectileY = 0.0f;
float projectileSize = 0.02f;
float projectileSpeed = 0.05f;

// barrinha
float paddleX = 0.0f;            // posição horizontal (em coordenadas Normalized Device Coordinates)
const float paddleY = -0.75f;    // posição fixa no Y
const float paddleWidth = 0.24f; // largura (0.12 esquerda + 0.12 direita)
const float paddleHeight = 0.05f;

// bolinha
float ballX = 0.0f;
float ballY = -0.5f; // começa acima da barrinha
float ballSize = 0.03f;
float ballVelX = 0.01f;
float ballVelY = 0.01f;

struct Projectile
{
    float x, y;
    bool active;
};

std::vector<Projectile> projectiles;

struct Vertex
{
    float x, y, z;
};

void UpdatePaddle()
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

void UpdateBall()
{
    ballX += ballVelX;
    ballY += ballVelY;
    ballVelY -= 0.0003f;

    // colisão com paredes
    if (ballX - ballSize < -0.9f || ballX + ballSize > 0.9f)
        ballVelX *= -1; // rebater horizontal

    if (ballY + ballSize > 1.0f)
    {
        ballY= 1.0f - ballSize;
        ballVelY *= -1; // rebater no teto
    }
    if (ballY - ballSize < -0.72f)
    {
        // bola caiu fora -> resetar
        ballVelY *= -0.5f;
    }

    // colisão com a barra
    if (ballY - ballSize <= paddleY + paddleHeight &&
        ballX >= paddleX - paddleWidth / 2 &&
        ballX <= paddleX + paddleWidth / 2 &&
        ballY > paddleY) // para não "colar"
    {
        ballVelY *= -1;
        ballY = paddleY + paddleHeight + ballSize; // corrigir posição
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

    deviceContext->UpdateSubresource(ballVertexBuffer, 0, nullptr, ballVertices, 0, 0);
}

void UpdateProjectiles()
{
    for (auto &p : projectiles)
    {
        if (!p.active)
            continue;

        p.y += projectileSpeed;

        // colisão com a bolinha
        if (p.x >= ballX - ballSize &&
            p.x <= ballX + ballSize &&
            p.y >= ballY - ballSize &&
            p.y <= ballY + ballSize)
        {
            ballVelY = 0.03f;
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

    ID3DBlob *psBlobProjectile = nullptr;

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

    hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vertexShader);
    if (FAILED(hr))
        return false;

    hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pixelShaderPaddle);
    if (FAILED(hr))
        return false;
    hr = device->CreatePixelShader(psBlobBall->GetBufferPointer(), psBlobBall->GetBufferSize(), nullptr, &pixelShaderBall);
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

    return true;
}

// Render loop
void RenderFrame()
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
    deviceContext->PSSetShader(pixelShaderPaddle, nullptr, 0);
    deviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
    deviceContext->Draw(6, 0);

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
                {p.x - projectileSize, p.y + projectileSize, 0.0f},
                {p.x - projectileSize, p.y - projectileSize, 0.0f},
                {p.x + projectileSize, p.y - projectileSize, 0.0f},

                {p.x - projectileSize, p.y + projectileSize, 0.0f},
                {p.x + projectileSize, p.y - projectileSize, 0.0f},
                {p.x + projectileSize, p.y + projectileSize, 0.0f},
            };

        deviceContext->UpdateSubresource(projectileBuffer, 0, nullptr, projVertices, 0, 0);
        deviceContext->IASetVertexBuffers(0, 1, &projectileBuffer, &stride, &offset);
        deviceContext->Draw(6, 0);
    }
    swapChain->Present(1, 0);
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
                     L"BreakoutDX", NULL};
    RegisterClassEx(&wc);

    HWND hWnd = CreateWindow(L"BreakoutDX", L"Breakout com DirectX 11",
                             WS_OVERLAPPEDWINDOW, 100, 100, 800, 600,
                             NULL, NULL, wc.hInstance, NULL);

    ShowWindow(hWnd, nCmdShow);

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
            static bool zWasPressed = false;
            if (GetAsyncKeyState('Z') & 0x8000) // Z para atirar
            {

                if (!zWasPressed)
                {
                    Projectile p;
                    p.x = paddleX;
                    p.y = paddleY + paddleHeight + 0.05f;
                    p.active = true;
                    projectiles.push_back(p);
                }
                zWasPressed = true;
            }
            else
            {
                zWasPressed = false;
            }

            if (GetAsyncKeyState(VK_LEFT) & 0x8000)
            {
                paddleX -= 0.02f; // velocidade
            }
            if (GetAsyncKeyState(VK_RIGHT) & 0x8000)
            {
                paddleX += 0.02f; // velocidade
            }
            // Limite para não sair da tela
            if (paddleX - paddleWidth / 2 < -0.90f)
                paddleX = -0.90f + paddleWidth / 2;
            if (paddleX + paddleWidth / 2 > 0.90f)
                paddleX = 0.90f - paddleWidth / 2;

            UpdatePaddle();
            UpdateBall();
            UpdateProjectiles();
            RenderFrame();
        }
    }

    CleanD3D();
    UnregisterClass(L"BreakoutDX", wc.hInstance);
    return 0;
}