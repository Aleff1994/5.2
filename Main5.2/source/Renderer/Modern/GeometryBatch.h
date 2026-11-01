#pragma once

#include "GLResources.h"
#include <cstddef>
#include <vector>

class GeometryBatch
{
public:
	GeometryBatch();
	bool Initialize();
	void Shutdown();
	void Begin();
	bool AddQuad(float x, float y, float width, float height);
	bool Upload();
	void Draw() const;
	size_t GetQuadCount() const;
	size_t GetIndexCount() const;

private:
	GLVertexArray m_VertexArray;
	GLBuffer m_VertexBuffer;
	GLBuffer m_IndexBuffer;
	std::vector<float> m_Vertices;
	std::vector<unsigned short> m_Indices;
	bool m_Ready;
};
