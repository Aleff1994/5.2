//OPENCODE - Profiling system: Real-time GPU/CPU metrics
//OPENCODE - Frame pacing/timestep system: Separate render FPS (60) from game logic fixed timestep (25Hz)
#pragma once

#include <string>
#include <vector>
#include "GLResources.h"
#include "ShaderProgram.h"
#include "RenderStats.h"
#include "ShaderManager.h"
#include "BufferManager.h"
#include "TextureManager.h"

class ShadowRenderer;
class ModernFrustum;
class TerrainRenderer;
class ModelRenderer;

#define MODERN_MAX_BONES 128

class ModernRenderBackend
{
public:
	bool Initialize();
	void Shutdown();
	bool ValidateInfrastructure(std::string& error) const;

	// Shadow renderer integration
	void SetShadowRenderer(ShadowRenderer* shadowRenderer);
	ShadowRenderer* GetShadowRenderer() const;
	void BindShadowMaps() const;
	void SetShadowUniforms(ShaderProgram* shader) const;
	bool InitializeShadowRenderer(ModernFrustum* frustum, TerrainRenderer* terrainRenderer, ModelRenderer* modelRenderer);

	// Shader programs
	enum ShaderType
	{
		SHADER_SKINNED_MESH = 0,
		SHADER_STATIC_MESH,
		SHADER_TERRAIN,
		SHADER_PARTICLE,
		SHADER_SKYBOX,
		SHADER_POST_PROCESS,
		SHADER_COUNT
	};

	ShaderProgram* GetShader(ShaderType type) const;

	// Bone matrix buffer for GPU skinning
	void UpdateBoneMatrices(const float* matrices, int count);
	void BindBoneBuffer() const;
	GLBuffer* CreateBoneBuffer();

	// OPENCODE: Profiling stats access
	RenderStats* GetRenderStats();
	const char* GetInitializationError() const { return m_InitializationError.c_str(); }

private:
	ShaderProgram* m_Shaders[SHADER_COUNT] = { nullptr };
	GLBuffer* m_BoneBuffer = nullptr;
	ShadowRenderer* m_ShadowRenderer = nullptr;
	RenderStats m_RenderStats; // OPENCODE: Profiling stats
	ShaderManager m_ShaderManager;
	BufferManager m_BufferManager;
	TextureManager m_TextureManager;
	std::string m_InitializationError;
};
