#include "Level.h"
#include <map>
#include <string>

// Forward declarations das funcoes de load do Editor (evita include circular)
bool LoadPlayerConfig(const char* fullPath);
bool LoadBallConfig(const char* fullPath);

// ==========================================
// TEXTURE CACHE — evita recarregar a mesma textura
// ==========================================
static std::map<std::string, ID3D11ShaderResourceView*> s_textureCache;

static ID3D11ShaderResourceView* CacheLoadTexture(const char* path) {
	if (!path || !path[0]) return nullptr;
	auto it = s_textureCache.find(path);
	if (it != s_textureCache.end()) return it->second;
	ID3D11ShaderResourceView* srv = nullptr;
	std::wstring wpath(path, path + strlen(path));
	CreateWICTextureFromFile(device, wpath.c_str(), nullptr, &srv);
	s_textureCache[path] = srv;
	return srv;
}
bool LoadBombConfig(const char* fullPath);
bool LoadMenuConfig(const char* fullPath);

void ClearLevel()
{
	blocks.clear();
	obstacles.clear();
	projectiles.clear();
	enemyBullets.clear();
	droppedItems.clear();
	blocksRemaining = 0;
	blocksInitialCount = 0;
}

// Cria um bloco a partir de um BlockConfig carregado do editor
void AddBlockFromConfig(float x, float y, const BlockConfig& cfg, ID3D11ShaderResourceView* srv)
{
	blocksRemaining++;
	blocksInitialCount++;
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
	b.textureSRV = srv;
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
	blocksInitialCount++;
	Block b = {};
	b.x = x; b.y = y; b.width = width; b.height = height;
	b.hits = hits; b.bulletPattern = pattern; b.bulletCount = count;
	b.bulletSpeed = 0.007f; b.shootIntervalFrames = 0;
	b.active = true; b.invulnerable = false;
	b.textureSRV = nullptr;
	b.colorR = 0.4f; b.colorG = 0.4f; b.colorB = 0.8f; b.colorA = 1.0f;
	b.movType = MOV_NONE; b.movDir = 1.0f;
	blocks.push_back(b);
}

void AddObstacles(float x, float y, float width, float height)
{
	Obstacle o = {};
	o.x = x; o.y = y; o.width = width; o.height = height;
	o.active = true;
	o.textureSRV = nullptr;
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
			blocksInitialCount++;
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

	// Carrega textura de fundo do stage se configurada
	if (editorStageEditorConfig.useTextureBg && editorStageEditorConfig.bgTexturePath[0]) {
		if (stageBgTexture) { stageBgTexture->Release(); stageBgTexture = nullptr; }
		std::wstring wpath(editorStageEditorConfig.bgTexturePath,
			editorStageEditorConfig.bgTexturePath + strlen(editorStageEditorConfig.bgTexturePath));
		CreateWICTextureFromFile(device, wpath.c_str(), nullptr, &stageBgTexture);
	}

	return true;
}

// ==========================================
// POPULATE GAMEPLAY FROM STAGE OBJECTS
// Converte stageObjects (editor) em entidades de gameplay (blocks/obstacles)
// Cada PlacedObject referencia um JSON de config que eh lido para obter stats.
// ==========================================

static bool LoadBlockConfigFromFile(const char* path, BlockConfig& cfg)
{
	std::ifstream jf(path);
	if (!jf.is_open()) return false;
	std::string line;
	DropTable& dt = cfg.dropTable;
	while (std::getline(jf, line)) {
		if (line.find("\"width\"") != std::string::npos) sscanf_s(line.c_str(), " \"width\": %f,", &cfg.width);
		if (line.find("\"height\"") != std::string::npos) sscanf_s(line.c_str(), " \"height\": %f,", &cfg.height);
		if (line.find("\"maxHits\"") != std::string::npos) sscanf_s(line.c_str(), " \"maxHits\": %d,", &cfg.maxHits);
		if (line.find("\"bulletPattern\"") != std::string::npos) sscanf_s(line.c_str(), " \"bulletPattern\": %d,", &cfg.bulletPattern);
		if (line.find("\"bulletCount\"") != std::string::npos) sscanf_s(line.c_str(), " \"bulletCount\": %d,", &cfg.bulletCount);
		if (line.find("\"bulletSpeed\"") != std::string::npos) sscanf_s(line.c_str(), " \"bulletSpeed\": %f,", &cfg.bulletSpeed);
		if (line.find("\"shootInterval\"") != std::string::npos) sscanf_s(line.c_str(), " \"shootInterval\": %d,", &cfg.shootIntervalFrames);
		if (line.find("\"invulnerable\": true") != std::string::npos) cfg.invulnerable = true;
		if (line.find("\"invulnerable\": false") != std::string::npos) cfg.invulnerable = false;
		if (line.find("\"useTexture\": true") != std::string::npos) cfg.useTexture = true;
		if (line.find("\"useTexture\": false") != std::string::npos) cfg.useTexture = false;
		if (line.find("\"colorR\"") != std::string::npos) sscanf_s(line.c_str(), " \"colorR\": %f,", &cfg.colorR);
		if (line.find("\"colorG\"") != std::string::npos) sscanf_s(line.c_str(), " \"colorG\": %f,", &cfg.colorG);
		if (line.find("\"colorB\"") != std::string::npos) sscanf_s(line.c_str(), " \"colorB\": %f,", &cfg.colorB);
		if (line.find("\"colorA\"") != std::string::npos) sscanf_s(line.c_str(), " \"colorA\": %f,", &cfg.colorA);
		if (line.find("\"texturePath\"") != std::string::npos) {
			size_t s = line.find(": \"") + 3, e = line.rfind("\"");
			if (s < e) { std::string v = line.substr(s, e - s); strcpy_s(cfg.texturePath, 256, v.c_str()); }
		}
		if (line.find("\"movType\"") != std::string::npos) {
			int v; sscanf_s(line.c_str(), " \"movType\": %d,", &v); cfg.movType = (EnemyMovType)v;
		}
		if (line.find("\"movSpeed\"") != std::string::npos) sscanf_s(line.c_str(), " \"movSpeed\": %f,", &cfg.movSpeed);
		if (line.find("\"movAmplitude\"") != std::string::npos) sscanf_s(line.c_str(), " \"movAmplitude\": %f,", &cfg.movAmplitude);
		if (line.find("\"movRadius\"") != std::string::npos) sscanf_s(line.c_str(), " \"movRadius\": %f,", &cfg.movRadius);
		if (line.find("\"hasDrop\": true") != std::string::npos) dt.hasDrop = true;
		if (line.find("\"hasDrop\": false") != std::string::npos) dt.hasDrop = false;
		for (int i = 0; i < 4; i++) {
			char key[32]; sprintf_s(key, "\"dropEnabled%d\"", i);
			if (line.find(key) != std::string::npos)
				dt.entryEnabled[i] = (line.find("true") != std::string::npos);
			sprintf_s(key, "\"dropWeight%d\"", i);
			if (line.find(key) != std::string::npos)
				sscanf_s(line.c_str(), " \"%*[^\"]\": %f,", &dt.entryWeight[i]);
		}
	}
	jf.close();
	return true;
}

static bool LoadObstacleDimsFromFile(const char* path, float& w, float& h,
	float& cr, float& cg, float& cb, float& ca, bool& useTexture, char* texPath, int texPathLen)
{
	std::ifstream jf(path);
	if (!jf.is_open()) return false;
	std::string line;
	while (std::getline(jf, line)) {
		if (line.find("\"width\"") != std::string::npos) sscanf_s(line.c_str(), " \"width\": %f,", &w);
		if (line.find("\"height\"") != std::string::npos) sscanf_s(line.c_str(), " \"height\": %f,", &h);
		if (line.find("\"r\":") != std::string::npos)
			sscanf_s(line.c_str(), " \"color\": { \"r\": %f , \"g\": %f , \"b\": %f , \"a\": %f }",
				&cr, &cg, &cb, &ca);
		if (line.find("\"useTexture\": true") != std::string::npos) useTexture = true;
		if (line.find("\"useTexture\": false") != std::string::npos) useTexture = false;
		if (line.find("\"texture\"") != std::string::npos) {
			size_t s = line.find(": \"") + 3, e = line.rfind("\"");
			if (s < e) { std::string v = line.substr(s, e - s); strcpy_s(texPath, texPathLen, v.c_str()); }
		}
	}
	jf.close();
	return true;
}

void PopulateGameplayFromStageObjects()
{
	ClearLevel();
	for (auto& o : stageObjects) {
		if (o.type == PLACED_BLOCK) {
			BlockConfig cfg = editorBlockConfig; // usar defaults do editor como base
			if (o.configFile[0] != '\0')
				LoadBlockConfigFromFile(o.configFile, cfg);
			ID3D11ShaderResourceView* srv = nullptr;
			if (cfg.useTexture && cfg.texturePath[0])
				srv = CacheLoadTexture(cfg.texturePath);
			AddBlockFromConfig(o.x, o.y, cfg, srv);
		}
		else if (o.type == PLACED_OBSTACLE) {
			float w = editorObstacleConfig.width;
			float h = editorObstacleConfig.height;
			float cr = 1.0f, cg = 1.0f, cb = 0.0f, ca = 1.0f;
			bool useTexture = false;
			char texPath[256] = {};
			if (o.configFile[0] != '\0')
				LoadObstacleDimsFromFile(o.configFile, w, h, cr, cg, cb, ca, useTexture, texPath, 256);
			Obstacle obs = {};
			obs.x = o.x; obs.y = o.y;
			obs.width = w; obs.height = h;
			obs.active = true;
			obs.useTexture = useTexture;
			obs.textureSRV = (useTexture && texPath[0]) ? CacheLoadTexture(texPath) : nullptr;
			obs.colorR = cr; obs.colorG = cg; obs.colorB = cb; obs.colorA = ca;
			obstacles.push_back(obs);
		}
		// PLACED_BOSS e PLACED_BALLSPAWN sao tratados separadamente
	}
}

// ==========================================
// INIT STAGE
// ==========================================

void InitStage(int stageSelected)
{
	paddleX = 0.0f;
	paddleWidth = paddleEditWidth;
	paddleHeight = paddleHeightNormal;

	// Tenta carregar via projeto (stage JSON) primeiro
	if (stageSelected < gameProject.stageCount && gameProject.stagePaths[stageSelected][0]) {
		LoadStageJSON(gameProject.stagePaths[stageSelected]);
		// Aplica posicao da bola do stage config
		ballX = editorStageConfig.ballStartX;
		ballY = editorStageConfig.ballStartY;
		ballVelX = editorStageConfig.ballStartVelX;
		ballVelY = editorStageConfig.ballStartVelY;
		// Popula gameplay entities a partir dos stageObjects
		PopulateGameplayFromStageObjects();
		stageTransitionTimer = 60;
		return;
	}

	// Fallback: tenta carregar .txt exportado do mesmo path com extensao diferente
	if (stageSelected < gameProject.stageCount && gameProject.stagePaths[stageSelected][0]) {
		char txtPath[MAX_PATH];
		strncpy_s(txtPath, MAX_PATH, gameProject.stagePaths[stageSelected], MAX_PATH - 1);
		char* ext = strrchr(txtPath, '.');
		if (ext) strcpy_s(ext, MAX_PATH - (ext - txtPath), ".txt");
		std::ifstream test(txtPath);
		if (test.is_open()) {
			test.close();
			ballX = editorStageConfig.ballStartX;
			ballY = editorStageConfig.ballStartY;
			ballVelX = editorStageConfig.ballStartVelX;
			ballVelY = editorStageConfig.ballStartVelY;
			LoadLevel(txtPath);
			stageTransitionTimer = 60;
			return;
		}
	}

	// Fallback final: stage%d.txt no diretorio de trabalho
	ballX = editorStageConfig.ballStartX;
	ballY = editorStageConfig.ballStartY;
	ballVelX = editorStageConfig.ballStartVelX;
	ballVelY = editorStageConfig.ballStartVelY;

	char filename[64];
	snprintf(filename, sizeof(filename), "stage%d.txt", stageSelected);
	LoadLevel(filename);
	stageTransitionTimer = 60;
}

// ==========================================
// STAGE EXPORT — gera stageN.txt a partir de stageObjects
// chamado automaticamente ao salvar um stage do editor
// ==========================================

bool ExportStageTxt(const char* txtPath)
{
	std::ofstream f(txtPath);
	if (!f.is_open()) return false;

	f << "P 0 1\n"; // padrao de bullet e balasPorBloco padrao

	for (auto& o : stageObjects) {
		if (o.type == PLACED_BLOCK) {
			// Tenta carregar o BlockConfig do arquivo para pegar dimensoes/stats
			BlockConfig cfg = {};
			// defaults
			cfg.width = editorBlockConfig.width;
			cfg.height = editorBlockConfig.height;
			cfg.maxHits = editorBlockConfig.maxHits;
			cfg.bulletPattern = editorBlockConfig.bulletPattern;
			cfg.bulletCount = editorBlockConfig.bulletCount;
			cfg.bulletSpeed = editorBlockConfig.bulletSpeed;
			cfg.shootIntervalFrames = editorBlockConfig.shootIntervalFrames;
			cfg.invulnerable = editorBlockConfig.invulnerable;
			cfg.movType = editorBlockConfig.movType;
			cfg.movSpeed = editorBlockConfig.movSpeed;
			cfg.movAmplitude = editorBlockConfig.movAmplitude;
			cfg.movRadius = editorBlockConfig.movRadius;
			cfg.dropTable = editorBlockConfig.dropTable;

			// Se tem arquivo JSON proprio, le as dimensoes/stats dele
			if (o.configFile[0] != '\0') {
				std::ifstream jf(o.configFile);
				if (jf.is_open()) {
					std::string line;
					while (std::getline(jf, line)) {
						if (line.find("\"width\"") != std::string::npos) sscanf_s(line.c_str(), " \"width\": %f,", &cfg.width);
						if (line.find("\"height\"") != std::string::npos) sscanf_s(line.c_str(), " \"height\": %f,", &cfg.height);
						if (line.find("\"maxHits\"") != std::string::npos) sscanf_s(line.c_str(), " \"maxHits\": %d,", &cfg.maxHits);
						if (line.find("\"bulletPattern\"") != std::string::npos) sscanf_s(line.c_str(), " \"bulletPattern\": %d,", &cfg.bulletPattern);
						if (line.find("\"bulletCount\"") != std::string::npos) sscanf_s(line.c_str(), " \"bulletCount\": %d,", &cfg.bulletCount);
						if (line.find("\"bulletSpeed\"") != std::string::npos) sscanf_s(line.c_str(), " \"bulletSpeed\": %f,", &cfg.bulletSpeed);
						if (line.find("\"shootInterval\"") != std::string::npos) sscanf_s(line.c_str(), " \"shootInterval\": %d,", &cfg.shootIntervalFrames);
						if (line.find("\"invulnerable\": true") != std::string::npos) cfg.invulnerable = true;
						if (line.find("\"invulnerable\": false") != std::string::npos) cfg.invulnerable = false;
						if (line.find("\"movType\"") != std::string::npos) {
							int v; sscanf_s(line.c_str(), " \"movType\": %d,", &v); cfg.movType = (EnemyMovType)v;
						}
						if (line.find("\"movSpeed\"") != std::string::npos) sscanf_s(line.c_str(), " \"movSpeed\": %f,", &cfg.movSpeed);
						if (line.find("\"movAmplitude\"") != std::string::npos) sscanf_s(line.c_str(), " \"movAmplitude\": %f,", &cfg.movAmplitude);
						if (line.find("\"movRadius\"") != std::string::npos) sscanf_s(line.c_str(), " \"movRadius\": %f,", &cfg.movRadius);
						if (line.find("\"hasDrop\": true") != std::string::npos) cfg.dropTable.hasDrop = true;
						if (line.find("\"hasDrop\": false") != std::string::npos) cfg.dropTable.hasDrop = false;
						for (int i = 0; i < 4; i++) {
							char key[32]; sprintf_s(key, "\"dropEnabled%d\"", i);
							if (line.find(key) != std::string::npos) {
								cfg.dropTable.entryEnabled[i] = (line.find("true") != std::string::npos);
							}
							sprintf_s(key, "\"dropWeight%d\"", i);
							if (line.find(key) != std::string::npos)
								sscanf_s(line.c_str(), " \"%*[^\"]\": %f,", &cfg.dropTable.entryWeight[i]);
						}
					}
					jf.close();
				}
			}

			float dw0 = cfg.dropTable.entryEnabled[0] ? cfg.dropTable.entryWeight[0] : 0.0f;
			float dw1 = cfg.dropTable.entryEnabled[1] ? cfg.dropTable.entryWeight[1] : 0.0f;
			float dw2 = cfg.dropTable.entryEnabled[2] ? cfg.dropTable.entryWeight[2] : 0.0f;
			float dw3 = cfg.dropTable.entryEnabled[3] ? cfg.dropTable.entryWeight[3] : 0.0f;

			f << "B " << o.x << " " << o.y
				<< " " << cfg.width << " " << cfg.height
				<< " " << cfg.maxHits
				<< " " << cfg.bulletPattern << " " << cfg.bulletCount
				<< " " << cfg.bulletSpeed << " " << cfg.shootIntervalFrames
				<< " " << (int)cfg.invulnerable
				<< " " << (int)cfg.movType << " " << cfg.movSpeed
				<< " " << cfg.movAmplitude << " " << cfg.movRadius
				<< " " << (int)cfg.dropTable.hasDrop
				<< " " << dw0 << " " << dw1 << " " << dw2 << " " << dw3
				<< "\n";
		}
		else if (o.type == PLACED_OBSTACLE) {
			float hw = editorObstacleConfig.width / 2.0f;
			float hh = editorObstacleConfig.height / 2.0f;
			// Tenta ler dimensoes do arquivo proprio
			if (o.configFile[0] != '\0') {
				std::ifstream jf(o.configFile);
				if (jf.is_open()) {
					std::string line;
					while (std::getline(jf, line)) {
						if (line.find("\"width\"") != std::string::npos) sscanf_s(line.c_str(), " \"width\": %f,", &hw);
						if (line.find("\"height\"") != std::string::npos) sscanf_s(line.c_str(), " \"height\": %f,", &hh);
					}
					jf.close();
					hw /= 2.0f; hh /= 2.0f;
				}
			}
			f << "O " << o.x - hw << " " << o.y - hh
				<< " " << hw * 2.0f << " " << hh * 2.0f << "\n";
		}
		// PLACED_BOSS e PLACED_BALLSPAWN nao geram linhas no .txt (tratados separadamente)
	}
	f.close();
	return true;
}

// ==========================================
// GAME PROJECT — Save / Load
// ==========================================

bool SaveGameProject(const char* fullPath)
{
	std::ofstream f(fullPath);
	if (!f.is_open()) return false;
	f << "{\n";
	f << "  \"playerConfig\": \"" << gameProject.playerConfigPath << "\",\n";
	f << "  \"ballConfig\": \"" << gameProject.ballConfigPath << "\",\n";
	f << "  \"bombConfig\": \"" << gameProject.bombConfigPath << "\",\n";
	f << "  \"menuConfig\": \"" << gameProject.menuConfigPath << "\",\n";
	f << "  \"stageCount\": " << gameProject.stageCount << ",\n";
	f << "  \"stages\": [\n";
	for (int i = 0; i < gameProject.stageCount; i++) {
		f << "    \"" << gameProject.stagePaths[i] << "\"";
		if (i < gameProject.stageCount - 1) f << ",";
		f << "\n";
	}
	f << "  ]\n}\n";
	f.close();
	strncpy_s(gameProjectPath, MAX_PATH, fullPath, MAX_PATH - 1);
	return true;
}

bool LoadGameProject(const char* fullPath);  // forward decl

// Carrega cada JSON referenciado e aplica as configs na memoria
void ApplyProjectConfigs()
{
	if (gameProject.playerConfigPath[0]) LoadPlayerConfig(gameProject.playerConfigPath);
	if (gameProject.ballConfigPath[0])   LoadBallConfig(gameProject.ballConfigPath);
	if (gameProject.bombConfigPath[0])   LoadBombConfig(gameProject.bombConfigPath);
	if (gameProject.menuConfigPath[0])   LoadMenuConfig(gameProject.menuConfigPath);

	// Stages: carrega o primeiro stage (stage 0) se existir
	if (gameProject.stageCount > 0 && gameProject.stagePaths[0][0])
		LoadStageJSON(gameProject.stagePaths[0]);
}

bool LoadGameProject(const char* fullPath)
{
	std::ifstream f(fullPath);
	if (!f.is_open()) return false;

	gameProject = {};
	std::string line;
	int stageIdx = 0;

	while (std::getline(f, line)) {
		auto readStr = [&](const char* key, char* dest) {
			if (line.find(key) != std::string::npos) {
				size_t s = line.find(": \"") + 3, e = line.rfind("\"");
				if (s < e) {
					std::string v = line.substr(s, e - s); strcpy_s(dest, MAX_PATH, v.c_str());
				}
			}
			};
		readStr("\"playerConfig\"", gameProject.playerConfigPath);
		readStr("\"ballConfig\"", gameProject.ballConfigPath);
		readStr("\"bombConfig\"", gameProject.bombConfigPath);
		readStr("\"menuConfig\"", gameProject.menuConfigPath);

		if (line.find("\"stageCount\"") != std::string::npos)
			sscanf_s(line.c_str(), " \"stageCount\": %d,", &gameProject.stageCount);

		// Linhas de stage (dentro do array "stages")
		if (line.find("\"") != std::string::npos &&
			line.find(":") == std::string::npos &&
			stageIdx < PROJECT_MAX_STAGES) {
			size_t s = line.find("\"") + 1, e = line.rfind("\"");
			if (s < e) {
				std::string v = line.substr(s, e - s);
				if (!v.empty() && v.find("{") == std::string::npos)
					strcpy_s(gameProject.stagePaths[stageIdx++], MAX_PATH, v.c_str());
			}
		}
	}
	f.close();
	strncpy_s(gameProjectPath, MAX_PATH, fullPath, MAX_PATH - 1);
	ApplyProjectConfigs();
	return true;
}