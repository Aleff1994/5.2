#include "stdafx.h"
#include "InstancedGeometry.h"

InstancedGeometry::InstancedGeometry()
	: m_VertexBuffer(GL_ARRAY_BUFFER), m_IndexBuffer(GL_ELEMENT_ARRAY_BUFFER),
	  m_InstanceBuffer(GL_ARRAY_BUFFER), m_Ready(false)
{
}

bool InstancedGeometry::InitializeQuad()
{
	Shutdown();
	static const float vertices[8] =
	{
		0.0f, 0.0f, 1.0f, 0.0f,
		1.0f, 1.0f, 0.0f, 1.0f
	};
	static const unsigned short indices[6] = { 0, 1, 2, 0, 2, 3 };
	if (!m_VertexArray.Create() || !m_VertexBuffer.Create() ||
		!m_IndexBuffer.Create() || !m_InstanceBuffer.Create())
	{
		Shutdown();
		return false;
	}
	m_VertexArray.Bind();
	m_VertexBuffer.Bind();
	m_VertexBuffer.Upload(sizeof(vertices), vertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, (const void*)0);
	m_IndexBuffer.Bind();
	m_IndexBuffer.Upload(sizeof(indices), indices, GL_STATIC_DRAW);
	m_InstanceBuffer.Bind();
	for (int column = 0; column < 4; ++column)
	{
		const GLuint location = (GLuint)(1 + column);
		glEnableVertexAttribArray(location);
		glVertexAttribPointer(location, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 16,
			(const void*)(sizeof(float) * 4 * column));
		glVertexAttribDivisor(location, 1);
	}
	m_Ready = true;
	return true;
}

void InstancedGeometry::Shutdown()
{
	m_Ready = false;
	m_InstanceMatrices.clear();
	m_InstanceBuffer.Reset();
	m_IndexBuffer.Reset();
	m_VertexBuffer.Reset();
	m_VertexArray.Reset();
}

void InstancedGeometry::BeginInstances()
{
	m_InstanceMatrices.clear();
}

bool InstancedGeometry::AddInstanceMatrix(const float* matrix16)
{
	if (matrix16 == NULL) return false;
	m_InstanceMatrices.insert(m_InstanceMatrices.end(), matrix16, matrix16 + 16);
	return true;
}

bool InstancedGeometry::UploadInstances()
{
	if (!m_Ready || m_InstanceMatrices.empty()) return false;
	m_VertexArray.Bind();
	m_InstanceBuffer.Bind();
	m_InstanceBuffer.Upload((GLsizeiptr)(m_InstanceMatrices.size() * sizeof(float)),
		&m_InstanceMatrices[0], GL_DYNAMIC_DRAW);
	return true;
}

void InstancedGeometry::Draw() const
{
	if (!m_Ready || m_InstanceMatrices.empty()) return;
	m_VertexArray.Bind();
	glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, (const void*)0,
		(GLsizei)GetInstanceCount());
}

size_t InstancedGeometry::GetInstanceCount() const
{
	return m_InstanceMatrices.size() / 16;
}
