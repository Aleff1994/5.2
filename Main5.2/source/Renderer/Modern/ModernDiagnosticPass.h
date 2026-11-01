#pragma once

#include <memory>
#include <string>

class RenderMatrices;
class InstancedGeometry;
class ShaderManager;
class ShaderProgram;

class ModernDiagnosticPass
{
public:
	ModernDiagnosticPass();
	~ModernDiagnosticPass();
	bool Initialize(std::string& error);
	void Shutdown();
	bool Render();
	bool IsReady() const;

private:
	ModernDiagnosticPass(const ModernDiagnosticPass&) = delete;
	ModernDiagnosticPass& operator=(const ModernDiagnosticPass&) = delete;
	std::unique_ptr<ShaderManager> m_ShaderManager;
	ShaderProgram* m_Program;
	std::unique_ptr<InstancedGeometry> m_Instances;
	std::unique_ptr<RenderMatrices> m_Matrices;
	bool m_Ready;
};

extern ModernDiagnosticPass g_ModernDiagnosticPass;
