#pragma once
#include "Globals.h"
#include <cmath> // Para o INFINITY

// Struct auxiliar para cálculo de colisão AABB Swept.
struct SweepResult {
    float t;      // tempo normalizado de colisão (0 a 1)
    float nx, ny; // normal (direção para onde a aresta do objeto está virada)
};

// Declaração das funções de matemática e física
SweepResult SweptAABB(float bx, float by, float bw, float bh, float vx, float vy, float ox, float oy, float ow, float oh);
bool CircleRectCollision(float cx, float cy, float radius, float rx, float ry, float rw, float rh);