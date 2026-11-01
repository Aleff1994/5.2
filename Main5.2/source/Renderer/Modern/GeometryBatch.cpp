#include "stdafx.h"
#include "GeometryBatch.h"

GeometryBatch::GeometryBatch()
	: m_VertexBuffer(GL_ARRAY_BUFFER), m_IndexBuffer(GL_ELEMENT_ARRAY_BUFFER), m_Ready(false)
{
}

bool GeometryBatch::Initialize()
{
	Shutdown();
	if (!m_VertexArray.Create() || !m_VertexBuffer.Create() || !m_IndexBuffer.Create())
	{
		Shutdown();
		return false;
	}
	m_VertexArray.Bind();
	m_VertexBuffer.Bind();
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, (const void*)0);
	m_IndexBuffer.Bind();
	m_Ready = true;
	return true;
}

void GeometryBatch::Shutdown()
{
	m_Ready = false;
	m_Vertices.clear();
	m_Indices.clear();
	m_IndexBuffer.Reset();
	m_VertexBuffer.Reset();
	m_VertexArray.Reset();
}

void GeometryBatch::Begin()
{
	m_Vertices.clear();
	m_Indices.clear();
}

bool GeometryBatch::AddQuad(float x, float y, float width, float height)
{
	if (m_Vertices.size() / 2 > 65531) return false;
	const unsigned short base = (unsigned short)(m_Vertices.size() / 2);
	const float vertices[8] =
	{
		x, y, x + width, y,
		x + width, y + height, x, y + height
	};
	const unsigned short indices[6] =
	{
		(unsigned short)(base + 0), (unsigned short)(base + 1), (unsigned short)(base + 2),
		(unsigned short)(base + 0), (unsigned short)(base + 2), (unsigned short)(base + 3)
	};
	m_Vertices.insert(m_Vertices.end(), vertices, vertices + 8);
	m_Indices.insert(m_Indices.end(), indices, indices + 6);
	return true;
}

bool GeometryBatch::Upload()
{
	if (!m_Ready || m_Vertices.empty() || m_Indices.empty()) return false;
	m_VertexArray.Bind();
	m_VertexBuffer.Bind();
	m_VertexBuffer.Upload((GLsizeiptr)(m_Vertices.size() * sizeof(float)), &m_Vertices[0], GL_DYNAMIC_DRAW);
	m_IndexBuffer.Bind();
	m_IndexBuffer.Upload((GLsizeiptr)(m_Indices.size() * sizeof(unsigned short)), &m_Indices[0], GL_DYNAMIC_DRAW);
	return true;
}

void GeometryBatch::Draw() const
{
	if (!m_Ready || m_Indices.empty()) return;
	m_VertexArray.Bind();
	glDrawElements(GL_TRIANGLES, (GLsizei)m_Indices.size(), GL_UNSIGNED_SHORT, (const void*)0);
}

size_t GeometryBatch::GetQuadCount() const { return m_Indices.size() / 6; }
size_t GeometryBatch::GetIndexCount() const { return m_Indices.size(); }
