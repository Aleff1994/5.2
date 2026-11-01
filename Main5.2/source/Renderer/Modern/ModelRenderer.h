//OPENCODE - Item 3: BMD Models VBO/VAO - Instancing + persistent buffers + GPU skinning optimizations
#pragma once

#include "ZzzBMD.h"
#include "Renderer/Modern/ModernRenderBackend.h"
#include "Renderer/Modern/GLResources.h"
#include "Renderer/Modern/ShaderProgram.h"
#include <vector>
#include <unordered_map>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <array>

struct VertexSkinned
{
	vec3_t position;
	vec3_t normal;
	vec2_t texCoord;
	vec4_t boneWeights;
	int boneIndices[4];
};

struct MeshGL
{
	GLVertexArray vao;
	GLBuffer vbo;
	GLBuffer ebo;
	int indexCount = 0;
	int textureIndex = -1;
	bool hasTexture = false;

	MeshGL() = default;
	MeshGL(MeshGL&&) noexcept = default;
	MeshGL& operator=(MeshGL&&) noexcept = default;
	MeshGL(const MeshGL&) = delete;
	MeshGL& operator=(const MeshGL&) = delete;
};

class ModelRenderer
{
public:
	ModelRenderer();
	~ModelRenderer();

	bool Initialize(ModernRenderBackend* backend);
	void Shutdown();

	bool BuildFromBMD(BMD* bmd);
	void Render(BMD* bmd, int renderFlags, float alpha = 1.0f, int hiddenMesh = -1, int blendMesh = -1, float blendMeshLight = 1.0f, int textureOverride = -1);

	void UpdateBoneMatrices(const float* matrices, int count);
	void UpdateBoneMatrices(const float (*boneMatrices)[3][4], int boneCount);

private:
	ModernRenderBackend* m_Backend = nullptr;
	std::unordered_map<std::string, std::vector<MeshGL>> m_ModelMeshes;
	std::array<float, MODERN_MAX_BONES * 16> m_BoneUploadScratch = {};

	bool BuildMeshGL(BMD* bmd, int meshIndex, MeshGL& outMesh);
	void SetupVertexAttribs(const MeshGL& mesh);
	void RenderMeshGL(const MeshGL& mesh, ShaderProgram* shader, int renderFlags, float alpha, int textureOverride);
};

extern ModelRenderer g_ModelRenderer;
