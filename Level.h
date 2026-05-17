#pragma once
#include "Globals.h"

void ClearLevel();
void ClearTextureCache(); // libera SRVs do cache de texturas (chamada no shutdown)

// Acesso publico ao cache de SRVs: util para previews no editor sem duplicar
// carregamento. Retorna nullptr se o caminho for vazio ou o arquivo invalido.
ID3D11ShaderResourceView* GetCachedTexture(const char* path);

// Resolve metadados de preview a partir do JSON apontado por po.configFile:
//   - po.previewSRV: textura (via GetCachedTexture)
//   - po.previewW / po.previewH: dimensoes reais do objeto
// Permite que o preview do editor de stage desenhe cada PlacedObject com o
// mesmo tamanho que tera em gameplay, em vez do template global do editor.
// Para PLACED_BALLSPAWN ou configFile vazio, deixa os campos com defaults
// (SRV = nullptr, previewW/H = -1).
void ResolvePreviewMeta(PlacedObject& po);

// Le um JSON de bloco para a struct BlockConfig informada. Reusada pelo demo
// do editor de stage para reconstituir as propriedades de cada bloco
// posicionado (movimento, padrao de tiro, etc.) sem duplicar o parser.
bool LoadBlockConfigFromFile(const char* path, BlockConfig& cfg);
void SaveLevel(const char* filename);
void LoadLevel(const char* filename);
void AddBlocks(float x, float y, float width, float height, int hits, int pattern, int count);
void AddBlockFromConfig(float x, float y, const BlockConfig& cfg, ID3D11ShaderResourceView* srv = nullptr);
void AddObstacles(float x, float y, float width, float height);
void InitStage(int stageSelected);
bool SaveStageJSON(const char* fullPath);
bool LoadStageJSON(const char* fullPath);
bool ExportStageTxt(const char* txtPath);
bool SaveGameProject(const char* fullPath);
bool LoadGameProject(const char* fullPath);
void ApplyProjectConfigs();
void PopulateGameplayFromStageObjects();
void AddPortal(float x, float y, float width, float height);