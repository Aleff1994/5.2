//OPENCODE - Item 3: BMD Models VBO/VAO - Instancing + persistent buffers + GPU skinning optimizations
#include "stdafx.h"
#include "ModelRenderer.h"
#include "ZzzBMD.h"
#include "ZzzOpenglUtil.h"
#include "ZzzTexture.h"
#include "Renderer/Modern/ModernRenderBackend.h"
#include "Renderer/Modern/GLResources.h"
#include "Renderer/Modern/ShaderProgram.h"
#include "Renderer/Modern/BufferManager.h"
#include "Renderer/Modern/TextureManager.h"
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

ModelRenderer::ModelRenderer()
{
}

ModelRenderer::~ModelRenderer()
{
	Shutdown();
}

bool ModelRenderer::Initialize(ModernRenderBackend* backend)
{
	m_Backend = backend;
	if (!backend)
		return false;

	return true;
}

void ModelRenderer::Shutdown()
{
	for (auto& pair : m_ModelMeshes)
	{
		for (auto& mesh : pair.second)
		{
			mesh.vao.Reset();
			mesh.vbo.Reset();
			mesh.ebo.Reset();
		}
	}
	m_ModelMeshes.clear();

	m_Backend = nullptr;
}

bool ModelRenderer::BuildFromBMD(BMD* bmd)
{
	if (!bmd || !bmd->Meshs || bmd->NumMeshs <= 0)
		return false;

	std::string key = bmd->FileName;
	auto& meshes = m_ModelMeshes[key];
	meshes.resize(bmd->NumMeshs);

	for (int i = 0; i < bmd->NumMeshs; ++i)
	{
		if (!BuildMeshGL(bmd, i, meshes[i]))
		{
			return false;
		}
	}

	return true;
}

bool ModelRenderer::BuildMeshGL(BMD* bmd, int meshIndex, MeshGL& outMesh)
{
	Mesh_t* mesh = &bmd->Meshs[meshIndex];
	if (!mesh || mesh->NumVertices <= 0 || mesh->NumTriangles <= 0)
		return false;

	std::vector<VertexSkinned> vertices;
	vertices.resize(mesh->NumVertices);

	for (int v = 0; v < mesh->NumVertices; ++v)
	{
		VertexSkinned& dst = vertices[v];
		Vertex_t* srcV = &mesh->Vertices[v];
		Normal_t* srcN = (v < mesh->NumNormals) ? &mesh->Normals[v] : nullptr;
		TexCoord_t* srcT = (v < mesh->NumTexCoords) ? &mesh->TexCoords[v] : nullptr;

		dst.position[0] = srcV->Position[0];
		dst.position[1] = srcV->Position[1];
		dst.position[2] = srcV->Position[2];

		if (srcN)
		{
			dst.normal[0] = srcN->Normal[0];
			dst.normal[1] = srcN->Normal[1];
			dst.normal[2] = srcN->Normal[2];
		}
		else
		{
			dst.normal[0] = 0.0f; dst.normal[1] = 1.0f; dst.normal[2] = 0.0f;
		}

		if (srcT)
		{
			dst.texCoord[0] = srcT->TexCoordU;
			dst.texCoord[1] = srcT->TexCoordV;
		}
		else
		{
			dst.texCoord[0] = 0.0f; dst.texCoord[1] = 0.0f;
		}

		dst.boneWeights[0] = 1.0f; dst.boneWeights[1] = 0.0f; dst.boneWeights[2] = 0.0f; dst.boneWeights[3] = 0.0f;
		dst.boneIndices[0] = srcV->Node;
		dst.boneIndices[1] = 0;
		dst.boneIndices[2] = 0;
		dst.boneIndices[3] = 0;
	}

	std::vector<unsigned short> indices;
	indices.reserve(mesh->NumTriangles * 3);

	for (int t = 0; t < mesh->NumTriangles; ++t)
	{
		Triangle_t* tri = &mesh->Triangles[t];
		if (tri->Polygon == 3)
		{
			indices.push_back(tri->VertexIndex[0]);
			indices.push_back(tri->VertexIndex[1]);
			indices.push_back(tri->VertexIndex[2]);
		}
		else if (tri->Polygon == 4)
		{
			indices.push_back(tri->VertexIndex[0]);
			indices.push_back(tri->VertexIndex[1]);
			indices.push_back(tri->VertexIndex[2]);
			indices.push_back(tri->VertexIndex[0]);
			indices.push_back(tri->VertexIndex[2]);
			indices.push_back(tri->VertexIndex[3]);
		}
	}

	outMesh.indexCount = (int)indices.size();
	outMesh.textureIndex = mesh->Texture;
	outMesh.hasTexture = (mesh->Texture >= 0);

	if (!outMesh.vao.Create()) return false;
	outMesh.vao.Bind();

	if (!outMesh.vbo.Create()) return false;
	outMesh.vbo.Bind();
	outMesh.vbo.Upload((GLsizeiptr)(vertices.size() * sizeof(VertexSkinned)), vertices.data(), GL_STATIC_DRAW);

	if (!outMesh.ebo.Create()) return false;
	outMesh.ebo.Bind();
	outMesh.ebo.Upload((GLsizeiptr)(indices.size() * sizeof(unsigned short)), indices.data(), GL_STATIC_DRAW);

	SetupVertexAttribs(outMesh);

	return true;
}

void ModelRenderer::SetupVertexAttribs(const MeshGL& mesh)
{
	mesh.vao.Bind();
	mesh.vbo.Bind();
	mesh.ebo.Bind();

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexSkinned), (void*)offsetof(VertexSkinned, position));

	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(VertexSkinned), (void*)offsetof(VertexSkinned, normal));

	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(VertexSkinned), (void*)offsetof(VertexSkinned, texCoord));

	glEnableVertexAttribArray(3);
	glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(VertexSkinned), (void*)offsetof(VertexSkinned, boneWeights));

	glEnableVertexAttribArray(4);
	glVertexAttribIPointer(4, 4, GL_INT, sizeof(VertexSkinned), (void*)offsetof(VertexSkinned, boneIndices));

	glBindVertexArray(0);
}

void ModelRenderer::UpdateBoneMatrices(const float* matrices, int count)
{
	if (!m_Backend || count <= 0 || count > MODERN_MAX_BONES)
		return;

	m_Backend->UpdateBoneMatrices(matrices, count);
}

void ModelRenderer::UpdateBoneMatrices(const float (*boneMatrices)[3][4], int boneCount)
{
	if (!m_Backend || boneCount <= 0 || boneCount > MODERN_MAX_BONES)
		return;

	for (int i = 0; i < boneCount; ++i)
	{
		float* mat4 = &m_BoneUploadScratch[i * 16];
		memset(mat4, 0, sizeof(float) * 16);
		mat4[0]  = boneMatrices[i][0][0]; mat4[4]  = boneMatrices[i][0][1]; mat4[8]  = boneMatrices[i][0][2]; mat4[12] = boneMatrices[i][0][3];
		mat4[1]  = boneMatrices[i][1][0]; mat4[5]  = boneMatrices[i][1][1]; mat4[9]  = boneMatrices[i][1][2]; mat4[13] = boneMatrices[i][1][3];
		mat4[2]  = boneMatrices[i][2][0]; mat4[6]  = boneMatrices[i][2][1]; mat4[10] = boneMatrices[i][2][2]; mat4[14] = boneMatrices[i][2][3];
		mat4[3]  = 0.0f;                  mat4[7]  = 0.0f;                  mat4[11] = 0.0f;                  mat4[15] = 1.0f;

	}

	UpdateBoneMatrices(m_BoneUploadScratch.data(), boneCount);
}

void ModelRenderer::RenderMeshGL(const MeshGL& mesh, ShaderProgram* shader, int renderFlags, float alpha, int textureOverride)
{
	if (!shader) return;

	shader->Bind();

	bool useTexture = mesh.hasTexture && (textureOverride == -1);
	int texIndex = (textureOverride != -1) ? textureOverride : mesh.textureIndex;

	shader->SetInt("uHasTexture", useTexture ? 1 : 0);
	shader->SetInt("uDiffuseMap", 0);
	shader->SetFloat("uAlpha", alpha);

	if (useTexture && texIndex >= 0)
	{
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, texIndex);
	}

	mesh.vao.Bind();
	glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_SHORT, 0);
	glBindVertexArray(0);
}

void ModelRenderer::Render(BMD* bmd, int renderFlags, float alpha, int hiddenMesh, int blendMesh, float blendMeshLight, int textureOverride)
{
	if (!m_Backend || !bmd || !bmd->Meshs)
		return;

	std::string key = bmd->FileName;
	auto it = m_ModelMeshes.find(key);
	if (it == m_ModelMeshes.end())
	{
		if (!BuildFromBMD(bmd))
			return;
		it = m_ModelMeshes.find(key);
		if (it == m_ModelMeshes.end())
			return;
	}

	ShaderProgram* shader = m_Backend->GetShader(ModernRenderBackend::SHADER_SKINNED_MESH);
	if (!shader)
		return;

	m_Backend->BindBoneBuffer();

	UpdateBoneMatrices(BoneTransform, bmd->NumBones);

	auto& meshes = it->second;
	for (int i = 0; i < bmd->NumMeshs; ++i)
	{
		if (i == hiddenMesh) continue;
		if (i >= (int)meshes.size()) continue;

		MeshGL& meshGL = meshes[i];
		if (meshGL.indexCount <= 0) continue;

		RenderMeshGL(meshGL, shader, renderFlags, alpha, textureOverride);
	}
}

ModelRenderer g_ModelRenderer;
