#include "stdafx.h"
#include "ModernFrustum.h"
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

static const char* g_HiZVertexShader = R"(
#version 330 compatibility
layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inTexCoord;

out vec2 vTexCoord;

void main()
{
	vTexCoord = inTexCoord;
	gl_Position = vec4(inPosition, 0.0, 1.0);
}
)";

static const char* g_HiZFragmentShader = R"(
#version 330 compatibility
uniform sampler2D uDepthTexture;
uniform int uLevel;

in vec2 vTexCoord;
out float outDepth;

float GetDepthAtLevel(vec2 uv, int level)
{
	vec2 texelSize = 1.0 / textureSize(uDepthTexture, level);
	vec2 sampleUV = uv * float(1 << level) / float(1 << level);
	
	vec4 samples[4];
	samples[0] = texture(uDepthTexture, uv + vec2(-0.5, -0.5) * texelSize, -float(level));
	samples[1] = texture(uDepthTexture, uv + vec2( 0.5, -0.5) * texelSize, -float(level));
	samples[2] = texture(uDepthTexture, uv + vec2(-0.5,  0.5) * texelSize, -float(level));
	samples[3] = texture(uDepthTexture, uv + vec2( 0.5,  0.5) * texelSize, -float(level));
	
	return max(max(samples[0].r, samples[1].r), max(samples[2].r, samples[3].r));
}

void main()
{
	outDepth = GetDepthAtLevel(vTexCoord, uLevel);
}
)";

ModernFrustum::ModernFrustum() : m_Valid(false), m_HiZEnabled(false),
	m_HiZTexture(0), m_HiZFBO(0), m_HiZWidth(0), m_HiZHeight(0),
	m_HiZLevels(0), m_HiZProgram(0), m_HiZVertexArray(0)
{
}

ModernFrustum::~ModernFrustum()
{
	DestroyHiZResources();
}

bool ModernFrustum::NormalizePlane(Plane& plane)
{
	const float length = std::sqrt(plane.A * plane.A + plane.B * plane.B + plane.C * plane.C);
	if (length <= 0.000001f) return false;
	plane.A /= length;
	plane.B /= length;
	plane.C /= length;
	plane.D /= length;
	return true;
}

bool ModernFrustum::UpdateFromOpenGL()
{
	GLfloat modelViewValues[16];
	GLfloat projectionValues[16];
	glGetFloatv(GL_MODELVIEW_MATRIX, modelViewValues);
	glGetFloatv(GL_PROJECTION_MATRIX, projectionValues);

	const glm::mat4 modelView = glm::make_mat4(modelViewValues);
	const glm::mat4 projection = glm::make_mat4(projectionValues);
	const glm::mat4 clip = projection * modelView;

	const float signs[6][2] =
	{
		{ 1.0f, 0.0f }, { -1.0f, 0.0f },
		{ 1.0f, 1.0f }, { -1.0f, 1.0f },
		{ 1.0f, 2.0f }, { -1.0f, 2.0f }
	};

	for (int i = 0; i < 6; ++i)
	{
		const float sign = signs[i][0];
		const int row = (int)signs[i][1];
		Plane& plane = m_Planes[i];
		plane.A = clip[0][3] + sign * clip[0][row];
		plane.B = clip[1][3] + sign * clip[1][row];
		plane.C = clip[2][3] + sign * clip[2][row];
		plane.D = clip[3][3] + sign * clip[3][row];
		if (!NormalizePlane(plane)) { m_Valid = false; return false; }
	}

	m_Valid = true;
	return true;
}

bool ModernFrustum::ContainsSphere(float x, float y, float z, float radius) const
{
	if (!m_Valid) return true;
	if (radius < 0.0f) radius = -radius;
	for (int i = 0; i < 6; ++i)
	{
		const Plane& plane = m_Planes[i];
		if (plane.A * x + plane.B * y + plane.C * z + plane.D < -radius)
			return false;
	}
	return true;
}

bool ModernFrustum::IsValid() const
{
	return m_Valid;
}

// ============================================================
// Hi-Z Occlusion Culling Implementation
// ============================================================

void ModernFrustum::BuildHiZDepthBuffer(int width, int height, GLuint depthTexture)
{
	if (!m_HiZEnabled) return;
	
	if (m_HiZWidth != width || m_HiZHeight != height || m_HiZTexture == 0)
	{
		DestroyHiZResources();
		CreateHiZResources(width, height);
	}
	
	// Copy depth texture to Hi-Z texture base level
	glBindTexture(GL_TEXTURE_2D, m_HiZTexture);
	glCopyImageSubData(
		depthTexture, GL_TEXTURE_2D, 0, 0, 0, 0,
		m_HiZTexture, GL_TEXTURE_2D, 0, 0, 0, 0,
		width, height, 1);
	
	// Build mip chain for Hi-Z
	BuildHiZMipChain();
}

void ModernFrustum::CreateHiZResources(int width, int height)
{
	m_HiZWidth = width;
	m_HiZHeight = height;
	
	// Calculate mip levels
	m_HiZLevels = 1;
	int w = width, h = height;
	while (w > 1 || h > 1)
	{
		w = std::max(1, w >> 1);
		h = std::max(1, h >> 1);
		m_HiZLevels++;
	}
	
	// Create Hi-Z texture with mip levels
	glGenTextures(1, &m_HiZTexture);
	glBindTexture(GL_TEXTURE_2D, m_HiZTexture);
	glTexStorage2D(GL_TEXTURE_2D, m_HiZLevels, GL_R32F, width, height);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	
	// Create FBO
	glGenFramebuffers(1, &m_HiZFBO);
	
	// Create mip level textures for rendering
	m_HiZMipLevels.resize(m_HiZLevels);
	for (int i = 0; i < m_HiZLevels; ++i)
	{
		int w = std::max(1, m_HiZWidth >> i);
		int h = std::max(1, m_HiZHeight >> i);
		
		GLuint tex;
		glGenTextures(1, &tex);
		glBindTexture(GL_TEXTURE_2D, tex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, w, h, 0, GL_RED, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		
		m_HiZMipLevels[i] = tex;
	}
	
	// Create Hi-Z compute program
	CreateHiZProgram();
	
	// Create fullscreen quad VAO
	glGenVertexArrays(1, &m_HiZVertexArray);
	GLuint vbo;
	glGenBuffers(1, &vbo);
	
	float quadVertices[] = {
		-1.0f, -1.0f, 0.0f, 0.0f,
		 1.0f, -1.0f, 1.0f, 0.0f,
		-1.0f,  1.0f, 0.0f, 1.0f,
		 1.0f,  1.0f, 1.0f, 1.0f
	};
	
	glBindVertexArray(m_HiZVertexArray);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
	
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
	
	glBindVertexArray(0);
}

void ModernFrustum::DestroyHiZResources()
{
	if (m_HiZTexture != 0)
	{
		glDeleteTextures(1, &m_HiZTexture);
		m_HiZTexture = 0;
	}
	
	if (m_HiZFBO != 0)
	{
		glDeleteFramebuffers(1, &m_HiZFBO);
		m_HiZFBO = 0;
	}
	
	for (GLuint tex : m_HiZMipLevels)
	{
		if (tex != 0) glDeleteTextures(1, &tex);
	}
	m_HiZMipLevels.clear();
	
	if (m_HiZProgram != 0)
	{
		glDeleteProgram(m_HiZProgram);
		m_HiZProgram = 0;
	}
	
	if (m_HiZVertexArray != 0)
	{
		glDeleteVertexArrays(1, &m_HiZVertexArray);
		m_HiZVertexArray = 0;
	}
	
	m_HiZWidth = 0;
	m_HiZHeight = 0;
	m_HiZLevels = 0;
}

void ModernFrustum::CreateHiZProgram()
{
	GLuint vs = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vs, 1, &g_HiZVertexShader, nullptr);
	glCompileShader(vs);
	
	GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fs, 1, &g_HiZFragmentShader, nullptr);
	glCompileShader(fs);
	
	m_HiZProgram = glCreateProgram();
	glAttachShader(m_HiZProgram, vs);
	glAttachShader(m_HiZProgram, fs);
	glLinkProgram(m_HiZProgram);
	
	glDeleteShader(vs);
	glDeleteShader(fs);
	
	GLint success;
	glGetProgramiv(m_HiZProgram, GL_LINK_STATUS, &success);
	if (!success)
	{
		char infoLog[512];
		glGetProgramInfoLog(m_HiZProgram, 512, nullptr, infoLog);
		// Log error
	}
}

bool ModernFrustum::BuildHiZMipChain()
{
	if (!m_HiZEnabled || m_HiZProgram == 0) return false;
	
	glUseProgram(m_HiZProgram);
	
	for (int level = 1; level < m_HiZLevels; ++level)
	{
		int w = std::max(1, m_HiZWidth >> level);
		int h = std::max(1, m_HiZHeight >> level);
		
		glBindFramebuffer(GL_FRAMEBUFFER, m_HiZFBO);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_HiZMipLevels[level], 0);
		glViewport(0, 0, w, h);
		
		glUniform1i(glGetUniformLocation(m_HiZProgram, "uLevel"), level - 1);
		glUniform1i(glGetUniformLocation(m_HiZProgram, "uDepthTexture"), 0);
		
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, m_HiZMipLevels[level - 1]);
		
		glBindVertexArray(m_HiZVertexArray);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	}
	
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glUseProgram(0);
	
	// Generate mipmaps for the base texture
	glBindTexture(GL_TEXTURE_2D, m_HiZTexture);
	glGenerateMipmap(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0);
	
	return true;
}

void ModernFrustum::BindHiZDepthBuffer() const
{
	if (m_HiZEnabled && m_HiZTexture != 0)
	{
		glActiveTexture(GL_TEXTURE0 + 7); // Use texture unit 7 for Hi-Z
		glBindTexture(GL_TEXTURE_2D, m_HiZTexture);
	}
}

bool ModernFrustum::IsSphereOccludedByHiZ(float x, float y, float z, float radius) const
{
	if (!m_HiZEnabled || !m_Valid) return false;
	
	// First check frustum culling
	if (!ContainsSphere(x, y, z, radius)) return true;
	
	// Hi-Z test would require projecting sphere to screen space
	// and sampling the Hi-Z buffer. This is a simplified version.
	// Full implementation requires projecting the sphere to screen space
	// and testing against the Hi-Z depth buffer.
	
	// For now, return false (not occluded) - full implementation
	// requires screen-space projection and Hi-Z sampling
	return false;
}

// ============================================================
// GPU Occlusion Queries
// ============================================================

void ModernFrustum::BeginOcclusionQuery(GLuint queryId)
{
	glBeginQuery(GL_SAMPLES_PASSED, queryId);
}

void ModernFrustum::EndOcclusionQuery()
{
	glEndQuery(GL_SAMPLES_PASSED);
}

bool ModernFrustum::IsOcclusionQueryResultAvailable(GLuint queryId) const
{
	GLint available = 0;
	glGetQueryObjectiv(queryId, GL_QUERY_RESULT_AVAILABLE, &available);
	return available == GL_TRUE;
}

GLuint ModernFrustum::GetOcclusionQueryResult(GLuint queryId) const
{
	GLuint result = 0;
	glGetQueryObjectuiv(queryId, GL_QUERY_RESULT, &result);
	return result;
}

ModernFrustum g_ModernFrustum;