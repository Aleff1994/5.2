#include "stdafx.h"
#include "ShaderManager.h"

ShaderManager::ShaderManager() : m_CompileCount(0), m_CacheHitCount(0) {}

ShaderProgram* ShaderManager::Create(const std::string& name, const char* vertexSource,
	const char* fragmentSource, std::string& error)
{
	ShaderProgram* existing = Find(name);
	if (existing != NULL) { ++m_CacheHitCount; return existing; }
	std::unique_ptr<ShaderProgram> program(new ShaderProgram());
	if (!program->Build(vertexSource, fragmentSource, error)) return NULL;
	ShaderProgram* result = program.get();
	m_Programs.emplace(name, std::move(program));
	++m_CompileCount;
	return result;
}
ShaderProgram* ShaderManager::Find(const std::string& name) const
{
	auto found = m_Programs.find(name);
	return found != m_Programs.end() ? found->second.get() : NULL;
}
void ShaderManager::Clear() { m_Programs.clear(); m_CompileCount = 0; m_CacheHitCount = 0; }
size_t ShaderManager::GetCount() const { return m_Programs.size(); }
size_t ShaderManager::GetCompileCount() const { return m_CompileCount; }
size_t ShaderManager::GetCacheHitCount() const { return m_CacheHitCount; }
