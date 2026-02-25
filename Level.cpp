#include "Level.h"

void ClearLevel() {
    blocks.clear();
    obstacles.clear();
    projectiles.clear();
    enemyBullets.clear();
    blocksRemaining = 0;
}

void SaveLevel(const char* filename) {
    std::ofstream file(filename);
    if (file.is_open()) {
        // Salva os padrões do Bullet Hell
        file << "P " << bulletPattern << " " << balasPorBloco << "\n";

        for (auto& b : blocks) {
            if (b.active)
                file << "B " << b.x << " " << b.y << " " << b.width << " " << b.height << " " << b.hits << " " << b.bulletPattern << " " << b.bulletCount << "\n";
        }
        for (auto& o : obstacles) {
            if (o.active)
                file << "O " << o.x << " " << o.y << " " << o.width << " " << o.height << "\n";
        }
        file.close();
    }
}

void AddBlocks(float x, float y, float width, float height, int hits, int pattern, int count) {
    blocksRemaining++;
    Block b;
    b.x = x; b.y = y; b.width = width; b.height = height; b.hits = hits;
    b.bulletPattern = pattern;
    b.bulletCount = count;
    b.active = true;
    b.iFrameBlock = false;
    b.iFrameBlockTimer = 0;
    blocks.push_back(b);
}

void AddObstacles(float x, float y, float width, float height) {
    Obstacle o;
    o.x = x; o.y = y; o.width = width; o.height = height; o.active = true;
    obstacles.push_back(o);
}

void LoadLevel(const char* filename) {
    ClearLevel();
    std::ifstream file(filename);
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line))
        {
            std::stringstream ss(line);
            char type;
            ss >> type;

            if (type == 'P') {
                ss >> bulletPattern >> balasPorBloco;
            }
            else if (type == 'B') {
                float x, y, w, h;
                int hits;
                int pattern = 0;
                int count = 1;

                ss >> x >> y >> w >> h >> hits;
                if (!(ss >> pattern)) pattern = 0;
                if (!(ss >> count)) count = 1;

                AddBlocks(x, y, w, h, hits, pattern, count);
            }
            else if (type == 'O') {
                float x, y, w, h;
                ss >> x >> y >> w >> h;
                AddObstacles(x, y, w, h);
            }
        }
        file.close();
    }
}

void InitStage(int stageSelected) {
    // Reseta a posição do player e da bola a cada fase
    paddleX = 0.0f;
    ballX = 0.75f;
    ballY = -0.5f;
    ballVelY = 0.02f;
    ballVelX = 0.000000001f;

    char filename[64];
    snprintf(filename, sizeof(filename), "stage%d.txt", stageSelected);
    LoadLevel(filename);

    stageTransitionTimer = 60;
}