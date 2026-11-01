#include "stdafx.h"
#include "ModernDiagnosticPass.h"
#include "RenderMatrices.h"
#include "ShaderManager.h"
#include "InstancedGeometry.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

ModernDiagnosticPass g_ModernDiagnosticPass;

namespace
{
	void RestoreCapability(GLenum capability, GLboolean enabled)
	{
		if (enabled == GL_TRUE) glEnable(capability);
		else glDisable(capability);
	}
}

ModernDiagnosticPass::ModernDiagnosticPass()
	: m_ShaderManager(new ShaderManager()), m_Program(NULL),
	  m_Instances(new InstancedGeometry()),
	  m_Matrices(new RenderMatrices()), m_Ready(false)
{
}

ModernDiagnosticPass::~ModernDiagnosticPass()
{
	Shutdown();
}

bool ModernDiagnosticPass::Initialize(std::string& error)
{
	Shutdown();
	static const char* vertexSource =
		"#version 330 compatibility\n"
		"layout(location = 0) in vec2 inPosition;\n"
		"layout(location = 1) in mat4 inInstanceModel;\n"
		"uniform mat4 uMvp;\n"
		"void main() { gl_Position = uMvp * inInstanceModel * vec4(inPosition, 0.0, 1.0); }\n";
	static const char* fragmentSource =
		"#version 330 compatibility\n"
		"out vec4 outColor;\n"
		"void main() { outColor = vec4(1.0, 0.1, 0.8, 1.0); }\n";

	GLint previousVertexArray = 0;
	GLint previousArrayBuffer = 0;
	glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVertexArray);
	glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previousArrayBuffer);

	m_Program = m_ShaderManager->Create("modern_diagnostic", vertexSource, fragmentSource, error);
	ShaderProgram* cachedProgram = m_ShaderManager->Create("modern_diagnostic", vertexSource, fragmentSource, error);
	if (m_Program == NULL || cachedProgram != m_Program ||
		m_ShaderManager->GetCompileCount() != 1 || m_ShaderManager->GetCacheHitCount() != 1 ||
		!m_Instances->InitializeQuad())
	{
		if (error.empty()) error = "failed to create diagnostic shader, VAO, VBO or EBO";
		Shutdown();
		glBindVertexArray((GLuint)previousVertexArray);
		glBindBuffer(GL_ARRAY_BUFFER, (GLuint)previousArrayBuffer);
		return false;
	}

	m_Instances->BeginInstances();
	const glm::mat4 instanceMatrices[4] =
	{
		glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.5f, 0.5f, 1.0f)),
		glm::translate(glm::mat4(1.0f), glm::vec3(0.5f, 0.0f, 0.0f)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.5f, 0.5f, 1.0f)),
		glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.5f, 0.0f)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.5f, 0.5f, 1.0f)),
		glm::translate(glm::mat4(1.0f), glm::vec3(0.5f, 0.5f, 0.0f)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.5f, 0.5f, 1.0f))
	};
	for (int i = 0; i < 4; ++i)
	{
		if (!m_Instances->AddInstanceMatrix(glm::value_ptr(instanceMatrices[i])))
		{
			error = "failed to add diagnostic instance matrix";
			Shutdown();
			return false;
		}
	}
	if (m_Instances->GetInstanceCount() != 4 || !m_Instances->UploadInstances())
	{
		error = "failed to upload four diagnostic instances";
		Shutdown();
		return false;
	}

	const glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(-0.96f, 0.80f, 0.0f)) *
		glm::scale(glm::mat4(1.0f), glm::vec3(0.16f, 0.16f, 1.0f));
	m_Matrices->SetModel(model);
	m_Matrices->SetView(glm::mat4(1.0f));
	m_Matrices->SetOrthographic(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
	glBindVertexArray((GLuint)previousVertexArray);
	glBindBuffer(GL_ARRAY_BUFFER, (GLuint)previousArrayBuffer);
	m_Ready = true;
	return true;
}

void ModernDiagnosticPass::Shutdown()
{
	m_Ready = false;
	m_Program = NULL;
	m_ShaderManager->Clear();
	m_Instances->Shutdown();
}

bool ModernDiagnosticPass::Render()
{
	if (!m_Ready) return false;
	GLint previousProgram = 0;
	GLint previousVertexArray = 0;
	GLint previousArrayBuffer = 0;
	glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
	glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVertexArray);
	glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previousArrayBuffer);
	const GLboolean blendEnabled = glIsEnabled(GL_BLEND);
	const GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
	const GLboolean cullEnabled = glIsEnabled(GL_CULL_FACE);
	const GLboolean scissorEnabled = glIsEnabled(GL_SCISSOR_TEST);

	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_SCISSOR_TEST);
	m_Program->Bind();
	m_Program->SetMatrix4("uMvp", glm::value_ptr(m_Matrices->GetMVP()));
	m_Instances->Draw();

	glUseProgram((GLuint)previousProgram);
	glBindVertexArray((GLuint)previousVertexArray);
	glBindBuffer(GL_ARRAY_BUFFER, (GLuint)previousArrayBuffer);
	RestoreCapability(GL_BLEND, blendEnabled);
	RestoreCapability(GL_DEPTH_TEST, depthEnabled);
	RestoreCapability(GL_CULL_FACE, cullEnabled);
	RestoreCapability(GL_SCISSOR_TEST, scissorEnabled);
	return true;
}

bool ModernDiagnosticPass::IsReady() const
{
	return m_Ready;
}
