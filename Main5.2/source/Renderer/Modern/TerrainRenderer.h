#pragma once

#include "Renderer/Modern/ModernRenderBackend.h"
#include "Renderer/Modern/GLResources.h"
#include "Renderer/Modern/ShaderProgram.h"
#include <vector>
#include <array>

#undef TERRAIN_SIZE
#undef TERRAIN_SCALE

extern float PrimaryTerrainHeight[];
extern float BackTerrainHeight[];
extern vec3_t PrimaryTerrainLight[];
extern vec3_t BackTerrainLight[];
extern vec3_t TerrainNormal[];
extern unsigned char TerrainMappingLayer1[];
extern unsigned char TerrainMappingLayer2[];
extern float TerrainMappingAlpha[];
extern WORD TerrainWall[];
extern float WorldTime;
extern int FrustrumBoundMinX;
extern int FrustrumBoundMaxX;
extern int FrustrumBoundMinY;
extern int FrustrumBoundMaxY;
extern bool CameraTopViewEnable;
extern vec3_t CameraPosition;
extern float g_fFrustumRange;
extern int gMapManager_currentMap;

struct TerrainVertex
{
	vec3_t position;
	vec3_t normal;
	vec2_t texCoord;
	vec4_t color;
};

struct TerrainChunk
{
	GLVertexArray vao;
	GLBuffer vbo;
	GLBuffer ebo;
	int indexCount = 0;
	int baseX = 0;
	int baseY = 0;
	bool hasWater = false;
	bool valid = false;
};

class TerrainRenderer
{
public:
	TerrainRenderer();
	~TerrainRenderer();

	bool Initialize(ModernRenderBackend* backend);
	void Shutdown();

	void BuildChunks();
	void Render(ModernRenderBackend* backend, bool editFlag = false);
	void RenderAfter(ModernRenderBackend* backend, bool editFlag = false);

	void SetCamera(const vec3_t& position, const float* modelViewProj, const float* modelView, const float* normalMatrix);
	void SetLight(const vec3_t& dir, const vec3_t& color, const vec3_t& ambient);
	void SetFog(const vec3_t& color, float start, float end);
	void SetWaterMove(float move) { m_WaterMove = move; }

private:
	ModernRenderBackend* m_Backend = nullptr;
	ShaderProgram* m_TerrainShader = nullptr;

	std::array<std::array<TerrainChunk, 32>, 32> m_Chunks;
	int m_ChunksX = 0;
	int m_ChunksY = 0;

	// Camera/light state
	vec3_t m_CameraPos;
	float m_ModelViewProj[16];
	float m_ModelView[16];
	float m_NormalMatrix[9];
	vec3_t m_LightDir;
	vec3_t m_LightColor;
	vec3_t m_AmbientColor;
	vec3_t m_FogColor;
	float m_FogStart = 0.0f;
	float m_FogEnd = 0.0f;
	float m_WaterMove = 0.0f;

	bool BuildChunk(int chunkX, int chunkY);
	void RenderChunk(const TerrainChunk& chunk, bool isWater);
	void SetupTerrainUniforms(bool isWater);
};

extern TerrainRenderer g_TerrainRenderer;