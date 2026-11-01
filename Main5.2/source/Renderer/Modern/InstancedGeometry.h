#pragma once

#include "GLResources.h"
#include <cstddef>
#include <vector>

class InstancedGeometry
{
public:
	InstancedGeometry();
	bool InitializeQuad();
	void Shutdown();
	void BeginInstances();
	bool AddInstanceMatrix(const float* matrix16);
	bool UploadInstances();
	void Draw() const;
	size_t GetInstanceCount() const;

private:
	GLVertexArray m_VertexArray;
	GLBuffer m_VertexBuffer;
	GLBuffer m_IndexBuffer;
	GLBuffer m_InstanceBuffer;
	std::vector<float> m_InstanceMatrices;
	bool m_Ready;
};
