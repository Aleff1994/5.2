#include "stdafx.h"
#include "TerrainRenderer.h"
#include "ZzzOpenglUtil.h"
#include "ZzzLodTerrain.h"
#include "ZzzTexture.h"
#include "ZzzInfomation.h"
#include "Renderer/Modern/ModernRenderBackend.h"
#include "Renderer/Modern/GLResources.h"
#include "Renderer/Modern/ShaderProgram.h"
#include "Renderer/Modern/BufferManager.h"
#include "Renderer/Modern/TextureManager.h"
#include "MapManager.h"
#include <vector>
#include <cmath>
#include <algorithm>

constexpr int TERRAIN_CHUNK_SIZE = 32;
constexpr int TERRAIN_CHUNK_VERTS = TERRAIN_CHUNK_SIZE * TERRAIN_CHUNK_SIZE * 4;
constexpr int TERRAIN_CHUNK_INDICES = TERRAIN_CHUNK_SIZE * TERRAIN_CHUNK_SIZE * 6;

const int g_TerrainSize = 256;
const int g_TerrainScale = 100;

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

inline int TerrainIndexRepeat(int x, int y)
{
	return (y & (g_TerrainSize - 1)) * g_TerrainSize + (x & (g_TerrainSize - 1));
}

TerrainRenderer::TerrainRenderer()
	: m_ChunksX(0), m_ChunksY(0), m_WaterMove(0.0f)
{
	memset(m_ModelViewProj, 0, sizeof(m_ModelViewProj));
	memset(m_ModelView, 0, sizeof(m_ModelView));
	memset(m_NormalMatrix, 0, sizeof(m_NormalMatrix));
	m_LightDir[0] = 0.0f; m_LightDir[1] = -0.1f; m_LightDir[2] = -0.8f;
	m_LightColor[0] = 1.0f; m_LightColor[1] = 1.0f; m_LightColor[2] = 1.0f;
	m_AmbientColor[0] = 0.3f; m_AmbientColor[1] = 0.3f; m_AmbientColor[2] = 0.3f;
	m_FogColor[0] = 0.5f; m_FogColor[1] = 0.5f; m_FogColor[2] = 0.6f;
}

TerrainRenderer::~TerrainRenderer()
{
	Shutdown();
}

bool TerrainRenderer::Initialize(ModernRenderBackend* backend)
{
	m_Backend = backend;
	if (!backend) return false;

	m_TerrainShader = backend->GetShader(ModernRenderBackend::SHADER_TERRAIN);
	if (!m_TerrainShader) return false;

	BuildChunks();
	return true;
}

void TerrainRenderer::Shutdown()
{
	for (int y = 0; y < m_ChunksY; ++y)
	{
		for (int x = 0; x < m_ChunksX; ++x)
		{
			TerrainChunk& chunk = m_Chunks[y][x];
			if (chunk.valid)
			{
				chunk.vao.Reset();
				chunk.vbo.Reset();
				chunk.ebo.Reset();
				chunk.valid = false;
			}
		}
	}
	m_ChunksX = 0;
	m_ChunksY = 0;
}

void TerrainRenderer::SetCamera(const vec3_t& position, const float* modelViewProj, const float* modelView, const float* normalMatrix)
{
	VectorCopy(position, m_CameraPos);
	if (modelViewProj) memcpy(m_ModelViewProj, modelViewProj, 16 * sizeof(float));
	if (modelView) memcpy(m_ModelView, modelView, 16 * sizeof(float));
	if (normalMatrix) memcpy(m_NormalMatrix, normalMatrix, 9 * sizeof(float));
}

void TerrainRenderer::SetLight(const vec3_t& dir, const vec3_t& color, const vec3_t& ambient)
{
	VectorCopy(dir, m_LightDir);
	VectorCopy(color, m_LightColor);
	VectorCopy(ambient, m_AmbientColor);
}

void TerrainRenderer::SetFog(const vec3_t& color, float start, float end)
{
	VectorCopy(color, m_FogColor);
	m_FogStart = start;
	m_FogEnd = end;
}

bool TerrainRenderer::BuildChunk(int chunkX, int chunkY)
{
	TerrainChunk& chunk = m_Chunks[chunkY][chunkX];
	if (chunk.valid) return true;

	int startX = chunkX * TERRAIN_CHUNK_SIZE;
	int startY = chunkY * TERRAIN_CHUNK_SIZE;
	int endX = startX + TERRAIN_CHUNK_SIZE;
	if (endX > g_TerrainSize) endX = g_TerrainSize;
	int endY = startY + TERRAIN_CHUNK_SIZE;
	if (endY > g_TerrainSize) endY = g_TerrainSize;

	std::vector<TerrainVertex> vertices;
	std::vector<unsigned short> indices;
	vertices.reserve((endX - startX) * (endY - startY) * 4);
	indices.reserve((endX - startX) * (endY - startY) * 6);

	chunk.hasWater = false;
	chunk.baseX = startX;
	chunk.baseY = startY;

	for (int y = startY; y < endY; ++y)
	{
		for (int x = startX; x < endX; ++x)
		{
			int idx = y * g_TerrainSize + x;
			
			float h1 = PrimaryTerrainHeight[idx];
			int idx2 = TerrainIndexRepeat(x + 1, y);
			float h2 = PrimaryTerrainHeight[idx2];
			int idx3 = TerrainIndexRepeat(x + 1, y + 1);
			float h3 = PrimaryTerrainHeight[idx3];
			int idx4 = TerrainIndexRepeat(x, y + 1);
			float h4 = PrimaryTerrainHeight[idx4];

			bool isWater = false;
			unsigned char layer1 = TerrainMappingLayer1[idx];
			if (layer1 == 5 || layer1 == 11)
				isWater = true;

			if (isWater) chunk.hasWater = true;

			float fx = (float)x * g_TerrainScale;
			float fy = (float)y * g_TerrainScale;
			float fScale = (float)g_TerrainScale;

			TerrainVertex v[4];
			
			v[0].position[0] = fx;
			v[0].position[1] = fy;
			v[0].position[2] = h1;
			v[0].normal[0] = TerrainNormal[idx][0];
			v[0].normal[1] = TerrainNormal[idx][1];
			v[0].normal[2] = TerrainNormal[idx][2];
			v[0].texCoord[0] = (float)x / g_TerrainSize;
			v[0].texCoord[1] = (float)y / g_TerrainSize;
			v[0].color[0] = PrimaryTerrainLight[idx][0];
			v[0].color[1] = PrimaryTerrainLight[idx][1];
			v[0].color[2] = PrimaryTerrainLight[idx][2];
			v[0].color[3] = 1.0f;

			v[1].position[0] = fx + fScale;
			v[1].position[1] = fy;
			v[1].position[2] = h2;
			v[1].normal[0] = TerrainNormal[idx2][0];
			v[1].normal[1] = TerrainNormal[idx2][1];
			v[1].normal[2] = TerrainNormal[idx2][2];
			v[1].texCoord[0] = (float)(x + 1) / g_TerrainSize;
			v[1].texCoord[1] = (float)y / g_TerrainSize;
			v[1].color[0] = PrimaryTerrainLight[idx2][0];
			v[1].color[1] = PrimaryTerrainLight[idx2][1];
			v[1].color[2] = PrimaryTerrainLight[idx2][2];
			v[1].color[3] = 1.0f;

			v[2].position[0] = fx + fScale;
			v[2].position[1] = fy + fScale;
			v[2].position[2] = h3;
			v[2].normal[0] = TerrainNormal[idx3][0];
			v[2].normal[1] = TerrainNormal[idx3][1];
			v[2].normal[2] = TerrainNormal[idx3][2];
			v[2].texCoord[0] = (float)(x + 1) / g_TerrainSize;
			v[2].texCoord[1] = (float)(y + 1) / g_TerrainSize;
			v[2].color[0] = PrimaryTerrainLight[idx3][0];
			v[2].color[1] = PrimaryTerrainLight[idx3][1];
			v[2].color[2] = PrimaryTerrainLight[idx3][2];
			v[2].color[3] = 1.0f;

			v[3].position[0] = fx;
			v[3].position[1] = fy + fScale;
			v[3].position[2] = h4;
			v[3].normal[0] = TerrainNormal[idx4][0];
			v[3].normal[1] = TerrainNormal[idx4][1];
			v[3].normal[2] = TerrainNormal[idx4][2];
			v[3].texCoord[0] = (float)x / g_TerrainSize;
			v[3].texCoord[1] = (float)(y + 1) / g_TerrainSize;
			v[3].color[0] = PrimaryTerrainLight[idx4][0];
			v[3].color[1] = PrimaryTerrainLight[idx4][1];
			v[3].color[2] = PrimaryTerrainLight[idx4][2];
			v[3].color[3] = 1.0f;

			int baseIdx = (int)vertices.size();
			vertices.insert(vertices.end(), v, v + 4);
			indices.push_back(baseIdx + 0);
			indices.push_back(baseIdx + 1);
			indices.push_back(baseIdx + 2);
			indices.push_back(baseIdx + 0);
			indices.push_back(baseIdx + 2);
			indices.push_back(baseIdx + 3);
		}
	}

	if (vertices.empty()) return false;

	if (!chunk.vao.Create()) return false;
	chunk.vao.Bind();

	if (!chunk.vbo.Create()) return false;
	chunk.vbo.Bind();
	chunk.vbo.Upload((GLsizeiptr)(vertices.size() * sizeof(TerrainVertex)), vertices.data(), GL_STATIC_DRAW);

	if (!chunk.ebo.Create()) return false;
	chunk.ebo.Bind();
	chunk.ebo.Upload((GLsizeiptr)(indices.size() * sizeof(unsigned short)), indices.data(), GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), (void*)offsetof(TerrainVertex, position));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), (void*)offsetof(TerrainVertex, normal));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), (void*)offsetof(TerrainVertex, texCoord));
	glEnableVertexAttribArray(3);
	glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), (void*)offsetof(TerrainVertex, color));

	glBindVertexArray(0);

	chunk.indexCount = (int)indices.size();
	chunk.valid = true;
	return true;
}

void TerrainRenderer::BuildChunks()
{
	m_ChunksX = (g_TerrainSize + TERRAIN_CHUNK_SIZE - 1) / TERRAIN_CHUNK_SIZE;
	m_ChunksY = (g_TerrainSize + TERRAIN_CHUNK_SIZE - 1) / TERRAIN_CHUNK_SIZE;

	for (int y = 0; y < m_ChunksY; ++y)
	{
		for (int x = 0; x < m_ChunksX; ++x)
		{
			BuildChunk(x, y);
		}
	}
}

void TerrainRenderer::SetupTerrainUniforms(bool isWater)
{
	if (!m_TerrainShader) return;

	m_TerrainShader->Bind();
	m_TerrainShader->SetMatrix4("uModelViewProjection", m_ModelViewProj);
	m_TerrainShader->SetMatrix4("uModelView", m_ModelView);
	m_TerrainShader->SetMatrix4("uNormalMatrix", m_NormalMatrix);
	m_TerrainShader->SetFloat("uWaterMove", m_WaterMove);
	m_TerrainShader->SetInt("uIsWater", isWater ? 1 : 0);
	m_TerrainShader->SetVector4("uLightDir", m_LightDir[0], m_LightDir[1], m_LightDir[2], 0.0f);
	m_TerrainShader->SetVector4("uLightColor", m_LightColor[0], m_LightColor[1], m_LightColor[2], 1.0f);
	m_TerrainShader->SetVector4("uAmbientColor", m_AmbientColor[0], m_AmbientColor[1], m_AmbientColor[2], 1.0f);
	m_TerrainShader->SetVector4("uFogColor", m_FogColor[0], m_FogColor[1], m_FogColor[2], 1.0f);
	m_TerrainShader->SetFloat("uFogStart", m_FogStart);
	m_TerrainShader->SetFloat("uFogEnd", m_FogEnd);
	m_TerrainShader->SetInt("uDiffuseMap", 0);
	m_TerrainShader->SetInt("uLayer2Map", 1);
	m_TerrainShader->SetInt("uHasLayer2", 0);
	m_TerrainShader->SetFloat("uLayer2Alpha", 1.0f);
}

void TerrainRenderer::RenderChunk(const TerrainChunk& chunk, bool isWater)
{
	if (!chunk.valid || chunk.indexCount == 0) return;

	SetupTerrainUniforms(isWater);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);

	chunk.vao.Bind();
	glDrawElements(GL_TRIANGLES, chunk.indexCount, GL_UNSIGNED_SHORT, 0);
	glBindVertexArray(0);
}

void TerrainRenderer::Render(ModernRenderBackend* backend, bool editFlag)
{
	if (!m_Backend || !m_TerrainShader) return;

	m_WaterMove = (float)((int)(WorldTime) % 20000) * 0.00005f;

	// Get camera matrices from globals
	extern float CameraMatrix[3][4];
	extern vec3_t CameraAngle;
	
	// Build modelview and projection matrices from camera
	float modelView[16] = {0};
	float projection[16] = {0};
	float modelViewProj[16] = {0};
	float normalMatrix[9] = {0};
	
	// Build view matrix from camera
	float cosYaw = cosf(CameraAngle[2]);
	float sinYaw = sinf(CameraAngle[2]);
	float cosPitch = cosf(CameraAngle[0]);
	float sinPitch = sinf(CameraAngle[0]);
	
	modelView[0] = cosYaw;
	modelView[1] = sinYaw * sinPitch;
	modelView[2] = -sinYaw * cosPitch;
	modelView[3] = 0;
	modelView[4] = 0;
	modelView[5] = cosPitch;
	modelView[6] = sinPitch;
	modelView[7] = 0;
	modelView[8] = sinYaw;
	modelView[9] = -cosYaw * sinPitch;
	modelView[10] = cosYaw * cosPitch;
	modelView[11] = 0;
	modelView[12] = -CameraPosition[0] * modelView[0] - CameraPosition[1] * modelView[4] - CameraPosition[2] * modelView[8];
	modelView[13] = -CameraPosition[0] * modelView[1] - CameraPosition[1] * modelView[5] - CameraPosition[2] * modelView[9];
	modelView[14] = -CameraPosition[0] * modelView[2] - CameraPosition[1] * modelView[6] - CameraPosition[2] * modelView[10];
	modelView[15] = 1.0f;

	// Perspective projection
	float fov = 55.0f * 3.14159f / 180.0f;
	float aspect = 1024.0f / 768.0f;
	float nearPlane = 20.0f;
	float farPlane = 2000.0f;
	float f = 1.0f / tanf(fov * 0.5f);
	
	projection[0] = f / aspect; projection[1] = 0; projection[2] = 0; projection[3] = 0;
	projection[4] = 0; projection[5] = f; projection[6] = 0; projection[7] = 0;
	projection[8] = 0; projection[9] = 0; projection[10] = (farPlane + nearPlane) / (nearPlane - farPlane); projection[11] = -1;
	projection[12] = 0; projection[13] = 0; projection[14] = (2 * farPlane * nearPlane) / (nearPlane - farPlane); projection[15] = 0;

	// ModelViewProjection = Projection * ModelView
	for (int i = 0; i < 4; ++i)
		for (int j = 0; j < 4; ++j)
		{
			m_ModelViewProj[i * 4 + j] = 0;
			for (int k = 0; k < 4; ++k)
				m_ModelViewProj[i * 4 + j] += projection[i * 4 + k] * modelView[k * 4 + j];
		}

	memcpy(m_ModelView, modelView, 16 * sizeof(float));
	
	// Normal matrix (upper 3x3 of modelView)
	for (int i = 0; i < 3; ++i)
		for (int j = 0; j < 3; ++j)
			m_NormalMatrix[i * 3 + j] = modelView[i * 4 + j];

	DisableAlphaBlend();
	EnableDepthTest();
	EnableCullFace();

	for (int y = 0; y < m_ChunksY; ++y)
	{
		for (int x = 0; x < m_ChunksX; ++x)
		{
			TerrainChunk& chunk = m_Chunks[y][x];
			if (!chunk.valid) continue;

			float chunkCenterX = (chunk.baseX + TERRAIN_CHUNK_SIZE * 0.5f) * g_TerrainScale;
			float chunkCenterY = (chunk.baseY + TERRAIN_CHUNK_SIZE * 0.5f) * g_TerrainScale;
			const float dx = chunkCenterX - m_CameraPos[0];
			const float dy = chunkCenterY - m_CameraPos[1];
			const float distSq = dx * dx + dy * dy;
			
			if (distSq > (5000.0f * 5000.0f) && !CameraTopViewEnable) continue;

			RenderChunk(chunk, chunk.hasWater);
		}
	}
}

void TerrainRenderer::RenderAfter(ModernRenderBackend* backend, bool editFlag)
{
	if (World != 39) return;

	EnableAlphaTest();
	for (int y = 0; y < m_ChunksY; ++y)
	{
		for (int x = 0; x < m_ChunksX; ++x)
		{
			TerrainChunk& chunk = m_Chunks[y][x];
			if (!chunk.valid) continue;

			const float chunkCenterX = (chunk.baseX + TERRAIN_CHUNK_SIZE * 0.5f) * g_TerrainScale;
			const float chunkCenterY = (chunk.baseY + TERRAIN_CHUNK_SIZE * 0.5f) * g_TerrainScale;
			const float dx = chunkCenterX - m_CameraPos[0];
			const float dy = chunkCenterY - m_CameraPos[1];
			if (!CameraTopViewEnable && (dx * dx + dy * dy) > (5000.0f * 5000.0f))
				continue;
			RenderChunk(chunk, false);
		}
	}
}

TerrainRenderer g_TerrainRenderer;
