#include "Level.h"

void ClearLevel()
{
	blocks.clear();
	obstacles.clear();
	projectiles.clear();
	enemyBullets.clear();
	droppedItems.clear();
	blocksRemaining = 0;
}

// Cria um bloco a partir de um BlockConfig carregado do editor
void AddBlockFromConfig(float x, float y, const BlockConfig& cfg)
{
	blocksRemaining++;
	Block b = {};
	b.x = x; b.y = y;
	b.width = cfg.width;
	b.height = cfg.height;
	b.hits = cfg.maxHits;
	b.active = true;
	b.bulletPattern = cfg.bulletPattern;
	b.bulletCount = cfg.bulletCount;
	b.bulletSpeed = cfg.bulletSpeed;
	b.shootIntervalFrames = cfg.shootIntervalFrames;
	b.shootTimer = 0;
	b.invulnerable = cfg.invulnerable;
	b.useTexture = cfg.useTexture;
	b.colorR = cfg.colorR; b.colorG = cfg.colorG;
	b.colorB = cfg.colorB; b.colorA = cfg.colorA;
	b.movType = cfg.movType;
	b.movSpeed = cfg.movSpeed;
	b.movAmplitude = cfg.movAmplitude;
	b.movRadius = cfg.movRadius;
	b.movAngle = 0.0f;
	b.movOriginX = x;
	b.movOriginY = y;
	b.movDir = 1.0f;
	b.hasDrop = cfg.dropTable.hasDrop;
	for (int i = 0; i < 4; i++)
		b.dropWeights[i] = (cfg.dropTable.entryEnabled[i]) ? cfg.dropTable.entryWeight[i] : 0.0f;
	blocks.push_back(b);
}

// Retrocompatibilidade com o sistema antigo de AddBlocks
void AddBlocks(float x, float y, float width, float height, int hits, int pattern, int count)
{
	blocksRemaining++;
	Block b = {};
	b.x = x; b.y = y; b.width = width; b.height = height;
	b.hits = hits; b.bulletPattern = pattern; b.bulletCount = count;
	b.bulletSpeed = 0.007f; b.shootIntervalFrames = 0;
	b.active = true; b.invulnerable = false;
	b.colorR = 0.4f; b.colorG = 0.4f; b.colorB = 0.8f; b.colorA = 1.0f;
	b.movType = MOV_NONE; b.movDir = 1.0f;
	blocks.push_back(b);
}

void AddObstacles(float x, float y, float width, float height)
{
	Obstacle o = {};
	o.x = x; o.y = y; o.width = width; o.height = height;
	o.active = true;
	o.colorR = 1.0f; o.colorG = 1.0f; o.colorB = 0.0f; o.colorA = 1.0f;
	obstacles.push_back(o);
}

// ==========================================
// SAVE / LOAD LEGADO (.txt)
// ==========================================

void SaveLevel(const char* filename)
{
	std::ofstream file(filename);
	if (!file.is_open()) return;
	file << "P " << bulletPattern << " " << balasPorBloco << "\n";
	for (auto& b : blocks) {
		if (b.active)
			file << "B " << b.x << " " << b.y << " " << b.width << " " << b.height
			<< " " << b.hits << " " << b.bulletPattern << " " << b.bulletCount
			<< " " << b.bulletSpeed << " " << b.shootIntervalFrames
			<< " " << (int)b.invulnerable
			<< " " << (int)b.movType << " " << b.movSpeed
			<< " " << b.movAmplitude << " " << b.movRadius
			<< " " << (int)b.hasDrop
			<< " " << b.dropWeights[0] << " " << b.dropWeights[1]
			<< " " << b.dropWeights[2] << " " << b.dropWeights[3]
			<< "\n";
	}
	for (auto& o : obstacles) {
		if (o.active)
			file << "O " << o.x << " " << o.y << " " << o.width << " " << o.height << "\n";
	}
	file.close();
}

void LoadLevel(const char* filename)
{
	ClearLevel();
	std::ifstream file(filename);
	if (!file.is_open()) return;
	std::string line;
	while (std::getline(file, line)) {
		std::stringstream ss(line);
		char type; ss >> type;

		if (type == 'P') {
			ss >> bulletPattern >> balasPorBloco;
		}
		else if (type == 'B') {
			float x, y, w, h, bspd, mspd, mamp, mrad;
			float dw0, dw1, dw2, dw3;
			int hits, pattern, count, interval, inv, movt, hasd;
			ss >> x >> y >> w >> h >> hits;
			if (!(ss >> pattern))  pattern = 0;
			if (!(ss >> count))    count = 1;
			if (!(ss >> bspd))     bspd = 0.007f;
			if (!(ss >> interval)) interval = 0;
			if (!(ss >> inv))      inv = 0;
			if (!(ss >> movt))     movt = 0;
			if (!(ss >> mspd))     mspd = 0.0f;
			if (!(ss >> mamp))     mamp = 0.3f;
			if (!(ss >> mrad))     mrad = 0.2f;
			if (!(ss >> hasd))     hasd = 0;
			if (!(ss >> dw0))      dw0 = 1.0f;
			if (!(ss >> dw1))      dw1 = 0.0f;
			if (!(ss >> dw2))      dw2 = 0.0f;
			if (!(ss >> dw3))      dw3 = 0.0f;

			blocksRemaining++;
			Block b = {};
			b.x = x; b.y = y; b.width = w; b.height = h;
			b.hits = hits; b.active = true;
			b.bulletPattern = pattern; b.bulletCount = count;
			b.bulletSpeed = bspd; b.shootIntervalFrames = interval;
			b.invulnerable = (inv != 0);
			b.movType = (EnemyMovType)movt;
			b.movSpeed = mspd; b.movAmplitude = mamp; b.movRadius = mrad;
			b.movOriginX = x; b.movOriginY = y; b.movDir = 1.0f;
			b.hasDrop = (hasd != 0);
			b.dropWeights[0] = dw0; b.dropWeights[1] = dw1;
			b.dropWeights[2] = dw2; b.dropWeights[3] = dw3;
			b.colorR = 0.4f; b.colorG = 0.4f; b.colorB = 0.8f; b.colorA = 1.0f;
			blocks.push_back(b);
		}
		else if (type == 'O') {
			float x, y, w, h; ss >> x >> y >> w >> h;
			AddObstacles(x, y, w, h);
		}
	}
	file.close();
}

// ==========================================
// SAVE / LOAD DO STAGE EDITOR (JSON)
// ==========================================

bool SaveStageJSON(const char* fullPath)
{
	std::ofstream f(fullPath);
	if (!f.is_open()) return false;
	f << "{\n";
	f << "  \"stageMode\": " << (int)editorStageEditorConfig.mode << ",\n";
	f << "  \"bgColorR\": " << editorStageEditorConfig.bgColorR << ",\n";
	f << "  \"bgColorG\": " << editorStageEditorConfig.bgColorG << ",\n";
	f << "  \"bgColorB\": " << editorStageEditorConfig.bgColorB << ",\n";
	f << "  \"bgColorA\": " << editorStageEditorConfig.bgColorA << ",\n";
	f << "  \"useTextureBg\": " << (editorStageEditorConfig.useTextureBg ? "true" : "false") << ",\n";
	f << "  \"bgTexture\": \"" << editorStageEditorConfig.bgTexturePath << "\",\n";
	f << "  \"ballStartX\": " << editorStageConfig.ballStartX << ",\n";
	f << "  \"ballStartY\": " << editorStageConfig.ballStartY << ",\n";
	f << "  \"ballVelX\": " << editorStageConfig.ballStartVelX << ",\n";
	f << "  \"ballVelY\": " << editorStageConfig.ballStartVelY << ",\n";
	f << "  \"objectCount\": " << (int)stageObjects.size() << ",\n";
	f << "  \"objects\": [\n";
	for (int i = 0; i < (int)stageObjects.size(); i++) {
		const auto& o = stageObjects[i];
		f << "    { \"type\": " << (int)o.type
			<< ", \"x\": " << o.x
			<< ", \"y\": " << o.y
			<< ", \"configFile\": \"" << o.configFile << "\""
			<< ", \"displayName\": \"" << o.displayName << "\" }";
		if (i < (int)stageObjects.size() - 1) f << ",";
		f << "\n";
	}
	f << "  ]\n}\n";
	f.close();
	return true;
}

bool LoadStageJSON(const char* fullPath)
{
	std::ifstream f(fullPath);
	if (!f.is_open()) return false;
	stageObjects.clear();
	std::string line;
	while (std::getline(f, line)) {
		if (line.find("\"stageMode\"") != std::string::npos) {
			int v; sscanf_s(line.c_str(), " \"stageMode\": %d,", &v); editorStageEditorConfig.mode = (StageMode)v;
		}
		if (line.find("\"bgColorR\"") != std::string::npos) sscanf_s(line.c_str(), " \"bgColorR\": %f,", &editorStageEditorConfig.bgColorR);
		if (line.find("\"bgColorG\"") != std::string::npos) sscanf_s(line.c_str(), " \"bgColorG\": %f,", &editorStageEditorConfig.bgColorG);
		if (line.find("\"bgColorB\"") != std::string::npos) sscanf_s(line.c_str(), " \"bgColorB\": %f,", &editorStageEditorConfig.bgColorB);
		if (line.find("\"bgColorA\"") != std::string::npos) sscanf_s(line.c_str(), " \"bgColorA\": %f,", &editorStageEditorConfig.bgColorA);
		if (line.find("\"useTextureBg\": true") != std::string::npos) editorStageEditorConfig.useTextureBg = true;
		if (line.find("\"useTextureBg\": false") != std::string::npos) editorStageEditorConfig.useTextureBg = false;
		if (line.find("\"ballStartX\"") != std::string::npos) sscanf_s(line.c_str(), " \"ballStartX\": %f,", &editorStageConfig.ballStartX);
		if (line.find("\"ballStartY\"") != std::string::npos) sscanf_s(line.c_str(), " \"ballStartY\": %f,", &editorStageConfig.ballStartY);
		if (line.find("\"ballVelX\"") != std::string::npos) sscanf_s(line.c_str(), " \"ballVelX\": %f,", &editorStageConfig.ballStartVelX);
		if (line.find("\"ballVelY\"") != std::string::npos) sscanf_s(line.c_str(), " \"ballVelY\": %f,", &editorStageConfig.ballStartVelY);
		if (line.find("\"bgTexture\"") != std::string::npos) {
			size_t s = line.find(": \"") + 3, e = line.rfind("\"");
			if (s < e) {
				std::string v = line.substr(s, e - s); strcpy_s(editorStageEditorConfig.bgTexturePath, 256, v.c_str());
			}
		}
		// Leitura de objetos (formato compacto numa linha)
		if (line.find("\"type\"") != std::string::npos && line.find("\"x\"") != std::string::npos) {
			PlacedObject po = {};
			int t; float px, py;
			sscanf_s(line.c_str(), " { \"type\": %d, \"x\": %f, \"y\": %f,", &t, &px, &py);
			po.type = (PlacedObjectType)t; po.x = px; po.y = py;
			// configFile
			size_t cf = line.find("\"configFile\": \"");
			if (cf != std::string::npos) {
				cf += 15;
				size_t ce = line.find("\"", cf);
				if (ce != std::string::npos) {
					std::string v = line.substr(cf, ce - cf);
					strcpy_s(po.configFile, 256, v.c_str());
				}
			}
			// displayName
			size_t dn = line.find("\"displayName\": \"");
			if (dn != std::string::npos) {
				dn += 16;
				size_t de = line.find("\"", dn);
				if (de != std::string::npos) {
					std::string v = line.substr(dn, de - dn);
					strcpy_s(po.displayName, 64, v.c_str());
				}
			}
			stageObjects.push_back(po);
		}
	}
	f.close();
	return true;
}

// ==========================================
// INIT STAGE
// ==========================================

void InitStage(int stageSelected)
{
	paddleX = 0.0f;
	paddleWidth = paddleEditWidth;
	paddleHeight = paddleHeightNormal;
	ballX = editorStageConfig.ballStartX;
	ballY = editorStageConfig.ballStartY;
	ballVelX = editorStageConfig.ballStartVelX;
	ballVelY = editorStageConfig.ballStartVelY;

	char filename[64];
	snprintf(filename, sizeof(filename), "stage%d.txt", stageSelected);
	LoadLevel(filename);
	stageTransitionTimer = 60;
}