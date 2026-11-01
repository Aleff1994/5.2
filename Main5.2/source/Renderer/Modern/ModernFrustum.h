#pragma once

#include <vector>
#include <GL/glew.h>

class ModernFrustum
{
public:
	ModernFrustum();
	~ModernFrustum();
	
	bool UpdateFromOpenGL();
	bool ContainsSphere(float x, float y, float z, float radius) const;
	bool IsValid() const;
	
	// Hi-Z Occlusion Culling
	void BuildHiZDepthBuffer(int width, int height, GLuint depthTexture);
	void BindHiZDepthBuffer() const;
	bool IsSphereOccludedByHiZ(float x, float y, float z, float radius) const;
	
	// GPU Occlusion Queries
	void BeginOcclusionQuery(GLuint queryId);
	void EndOcclusionQuery();
	bool IsOcclusionQueryResultAvailable(GLuint queryId) const;
	GLuint GetOcclusionQueryResult(GLuint queryId) const;
	
	// Configuration
	void SetHiZEnabled(bool enabled);
	bool IsHiZEnabled() const;
	
private:
	struct Plane
	{
		float A;
		float B;
		float C;
		float D;
	};
	
	bool NormalizePlane(Plane& plane);
	Plane m_Planes[6];
	bool m_Valid;
	
	// Hi-Z Depth Buffer
	bool m_HiZEnabled = false;
	GLuint m_HiZTexture = 0;
	GLuint m_HiZFBO = 0;
	std::vector<GLuint> m_HiZMipLevels;
	int m_HiZWidth = 0;
	int m_HiZHeight = 0;
	int m_HiZLevels = 0;
	
	// Hi-Z Compute Shader
	GLuint m_HiZProgram = 0;
	GLuint m_HiZVertexArray = 0;
	
	// Occlusion Queries
	std::vector<GLuint> m_OcclusionQueries;
	
	// Hi-Z Methods
	bool BuildHiZMipChain();
	void CreateHiZProgram();
	void CreateHiZResources(int width, int height);
	void DestroyHiZResources();
};

extern ModernFrustum g_ModernFrustum;
