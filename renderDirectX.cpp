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

ID3D11Buffer *forceFieldBuffer = nullptr;
ID3D11ShaderResourceView *forceFieldTexture = nullptr;

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
const float paddleWidth = 0.10f; // largura (0.5 esquerda + 0.5 direita)
float paddleHeight = 0.24f;
float paddleHeightNormal = 0.24f;
float paddleHeightDash = 0.10f;

// bolinha
float ballX = 0.0f;
float ballY = -0.5f; // começa acima da barrinha
float ballSize = 0.03f;
float ballVelX = 0.01f;
float ballVelY = 0.01f;

// shield
bool forceFieldActive = false;
float forceFieldRadius = 0.25f;
float forceFieldTimer = 0.00f;
float forceFieldY = 0.00f;
float forceFieldX = 0.00f;

// rasteira
bool dashActive = false;
int dashTimer = 0;
float dashDir = 0.0f;     // -1 para esquerda, +1 para direita
float dashSpeed = 0.025f; // velocidade durante a rasteira, só um pouco mais rápido do que a velocidade normal

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
/*
// checa se ponto P(px,py) está dentro do triângulo p0,p1,p2
bool PointInTriangle(float px, float py,
                     float x0, float y0,
                     float x1, float y1,
                     float x2, float y2)
{
    auto sign = [](float ax, float ay, float bx, float by, float cx, float cy) -> float
    {
        return (ax - cx) * (by - cy) - (bx - cx) * (ay - cy);
    };
    float d1 = sign(px, py, x0, y0, x1, y1);
    float d2 = sign(px, py, x1, y1, x2, y2);
    float d3 = sign(px, py, x2, y2, x0, y0);

    bool has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    bool has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);

    return !(has_neg && has_pos);
}
*/
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
    ballVelY -= 0.0005f;

    // colisão com paredes
    if (ballX - ballSize < -0.9f) // esquerda
    {
        ballX = -0.9f + ballSize;
        ballVelX *= -1; // inverte velocidade horizontal
    }

    if (ballX + ballSize > 0.9f) // direita
    {
        ballX = 0.9f - ballSize;
        ballVelX *= -1;
    }

    // colisão com teto
    if (ballY + ballSize > 1.0f)
    {
        ballY = 1.0f - ballSize;
        ballVelY *= -1; // inverte velocidade vertical
    }
    if (ballY - ballSize < -0.72f)
    {
        // Bola caiu no chão -> rebate com menos força
        ballY = -0.72f + ballSize;
        ballVelY *= -0.75f;
    }

    // colisão com a barra

    float paddleHitOffset = (ballX - paddleX) / paddleWidth; // Local da barrinha onde

    if (ballY - ballSize <= paddleY + paddleHeight &&
        ballX >= paddleX - paddleWidth / 2 &&
        ballX <= paddleX + paddleWidth / 2 &&
        ballY > paddleY) // para não "colar"
    {
        ballVelY *= -1;
        ballY = paddleY + paddleHeight + ballSize; // corrigir posição

        ballVelX += paddleHitOffset * 0.02f;
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

    if (ballVelX > 0.02f)
    {
        ballVelX = 0.02f; // Limita a velocidade horizontal da bolinha para ela não ficar rápida demais, ajustar valor conforme necessário
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
        float hitboxScale = 1.6f;
        float expandedSize = ballSize * hitboxScale;
        if (p.x >= ballX - expandedSize &&
            p.x <= ballX + expandedSize &&
            p.y >= ballY - expandedSize &&
            p.y <= ballY + expandedSize)
        {
            float hitOffset = (p.x - ballX) / expandedSize; // Local onde a bolinha foi atingida pelo proj

            ballVelX += hitOffset * 0.02f; // impulso horizontal dependendo de onde a bolinha foi atingida, supostamente tiros mais distantes do centro possuem maior impacto horizontal

            ballVelY = 0.03f; // impulso vertical ao acertar a bolinha

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

void UpdateForceField()
{
    if (forceFieldActive)
    {
        forceFieldX = paddleX;
        forceFieldY = paddleY + (paddleHeight / 2);
        forceFieldTimer -= 1;
        if (forceFieldTimer <= 0)
        {
            forceFieldActive = false;
        }

        // colisão com a bolinha
        float dx = ballX - forceFieldX;
        float dy = ballY - forceFieldY;
        float distSq = dx * dx + dy * dy;
        float minDist = forceFieldRadius + ballSize;

        if (distSq < minDist * minDist) // colisão ocorreu
        {
            float angle = atan2f(dy, dx);
            if (angle >= 0 && angle <= 3.14159265f) // parte superior do shield
            {
                float dist = sqrtf(distSq);
                if (dist == 0.0f)
                    dist = 0.00001f; // evita divisão por zero

                float nx = dx / dist;
                float ny = dy / dist;

                // Reposiciona a bola para fora do shield
                ballX = forceFieldX + nx * (minDist);
                ballY = forceFieldY + ny * (minDist);

                // Reflete a velocidade da bolinha
                float dot = ballVelX * nx + ballVelY * ny;
                ballVelX -= 2 * dot * nx;
                ballVelY -= 2 * dot * ny;

                // Impulso extra (get parried idiot)
                ballVelX += nx * 0.01f;
                ballVelY += ny * 0.01f;
            }
        }
    }
}

void UpdateDash()
{
    if (dashActive)
    {
        paddleHeight = paddleHeightDash;
        dashTimer -= 1;
        if (dashTimer <= 0)
        {
            dashActive = false;
            paddleHeight = paddleHeightNormal;
        }

        paddleX += (dashDir*dashSpeed);
    }
}

void ActivateforceField()
{
    forceFieldActive = true;
    forceFieldX = paddleX;
    forceFieldY = paddleY + paddleHeight / 2;
    forceFieldTimer = 60; // Em frames
}

void ActivateDash()
{
    dashActive = true;
    dashTimer = 30;
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

    // vertex buffer do shield
    D3D11_BUFFER_DESC bdShield = {};
    bdShield.Usage = D3D11_USAGE_DEFAULT;
    bdShield.ByteWidth = sizeof(Vertex) * (32 + 2) * 3; // número suficiente de vértices para o círculo
    bdShield.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    hr = device->CreateBuffer(&bdShield, nullptr, &forceFieldBuffer);
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
                {p.x - (projectileSize * 0.8f), p.y + (projectileSize * 0.8f), 0.0f},
                {p.x - (projectileSize * 0.8f), p.y - (projectileSize * 0.8f), 0.0f},
                {p.x + (projectileSize * 0.8f), p.y - (projectileSize * 0.8f), 0.0f},

                {p.x - (projectileSize * 0.8f), p.y + (projectileSize * 0.8f), 0.0f},
                {p.x + (projectileSize * 0.8f), p.y - (projectileSize * 0.8f), 0.0f},
                {p.x + (projectileSize * 0.8f), p.y + (projectileSize * 0.8f), 0.0f},
            };

        deviceContext->UpdateSubresource(projectileBuffer, 0, nullptr, projVertices, 0, 0);
        deviceContext->IASetVertexBuffers(0, 1, &projectileBuffer, &stride, &offset);
        deviceContext->Draw(6, 0);
    }

    // desenhar shield
    if (forceFieldActive)
    {
        deviceContext->PSSetShader(pixelShaderProjectile, nullptr, 0);

        const int segments = 32; // Segmentos para formar o círculo

        std::vector<Vertex> circleVerts;

        // Vértice central
        circleVerts.push_back({forceFieldX, forceFieldY, 0.0f});

        for (int i = 0; i <= segments; i++)
        {
            float theta = (3.14159265f * i) / segments; // arco superior (180°)
            float x = forceFieldX + cosf(theta) * forceFieldRadius;
            float y = forceFieldY + sinf(theta) * forceFieldRadius;
            circleVerts.push_back({x, y, 0.0f});
        }

        // Triângulos em forma de leque... ou pelo menos era pra ser
        std::vector<Vertex> fanVerts;
        for (int i = 1; i < circleVerts.size() - 1; i++)
        {
            fanVerts.push_back(circleVerts[0]);     // centro
            fanVerts.push_back(circleVerts[i]);     // ponto atual na circunferência
            fanVerts.push_back(circleVerts[i + 1]); // próximo ponto
        }

        /*Vertex forceFieldVertices[] =
            {
                {forceFieldX - forceFieldRadius, forceFieldY + forceFieldRadius, 0.0f},
                {forceFieldX - forceFieldRadius, forceFieldY - forceFieldRadius, 0.0f},
                {forceFieldX + forceFieldRadius, forceFieldY - forceFieldRadius, 0.0f},

                {forceFieldX - forceFieldRadius, forceFieldY + forceFieldRadius, 0.0f},
                {forceFieldX + forceFieldRadius, forceFieldY - forceFieldRadius, 0.0f},
                {forceFieldX + forceFieldRadius, forceFieldY + forceFieldRadius, 0.0f},
            };*/
        deviceContext->UpdateSubresource(forceFieldBuffer, 0, nullptr, fanVerts.data(), 0, 0);
        UINT stride = sizeof(Vertex);
        UINT offset = 0;
        deviceContext->IASetVertexBuffers(0, 1, &forceFieldBuffer, &stride, &offset);
        deviceContext->Draw(static_cast<UINT>(fanVerts.size()), 0);
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
            static bool xWasPressed = false;
            if (GetAsyncKeyState('X') & 0x8000) // Deve priorizar o dash acima do Shield
            {
                if (!xWasPressed)
                {
                    if (!forceFieldActive) // Shield não deve estar ativo
                    {
                        if (GetAsyncKeyState(VK_LEFT) & 0x8000)
                        {
                            // dashActive = true;
                            dashDir = -1.0f; // esquerda
                            // dashTimer = 20;
                            // paddleHeight = paddleHeightDash;
                            ActivateDash();
                        }
                        else if (GetAsyncKeyState(VK_RIGHT) & 0x8000)
                        {
                            // dashActive = true;
                            dashDir = 1.0f; // direita
                            // dashTimer = 20;
                            // paddleHeight = paddleHeightDash;
                            ActivateDash();
                        }
                        else
                        {
                            // sem direção = shield
                            ActivateforceField();
                        }
                    }
                }
                xWasPressed = true;
            }
            else
            {
                xWasPressed = false;
            }
            /*if (GetAsyncKeyState('X') & 0x8000) // X para criar o shield
            {
                if (!xWasPressed && !forceFieldActive)
                {
                    ActivateforceField();
                }
                xWasPressed = true;
            }
            else
            {
                xWasPressed = false;
            }*/
            if (GetAsyncKeyState('Z') & 0x8000) // Z para atirar
            {

                if (!zWasPressed)
                {
                    Projectile p;
                    p.x = paddleX;
                    p.y = paddleY + paddleHeight + 0.003f;
                    p.active = true;
                    projectiles.push_back(p);
                }
                zWasPressed = true;
            }
            else
            {
                zWasPressed = false;
            }
            if (!forceFieldActive && !dashActive) // movimentação é bloqueada enquanto o escudo estiver ativo
            {
                if (GetAsyncKeyState(VK_LEFT) & 0x8000)
                {
                    paddleX -= 0.02f; // velocidade para a esquerda
                }
                if (GetAsyncKeyState(VK_RIGHT) & 0x8000)
                {
                    paddleX += 0.02f; // velocidade para a direita
                }
            }
            // Limite para não sair da tela
            if (paddleX - paddleWidth / 2 < -0.90f)
                paddleX = -0.90f + paddleWidth / 2;
            if (paddleX + paddleWidth / 2 > 0.90f)
                paddleX = 0.90f - paddleWidth / 2;

            UpdatePaddle();
            UpdateBall();
            UpdateProjectiles();
            UpdateForceField();
            UpdateDash();
            RenderFrame();
        }
    }

    CleanD3D();
    UnregisterClass(L"BreakoutDX", wc.hInstance);
    return 0;
}