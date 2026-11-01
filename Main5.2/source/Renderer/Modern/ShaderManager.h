#pragma once

#include "ShaderProgram.h"
#include <memory>
#include <string>
#include <unordered_map>

class ShaderManager
{
public:
	ShaderManager();
	ShaderProgram* Create(const std::string& name, const char* vertexSource,
		const char* fragmentSource, std::string& error);
	ShaderProgram* Find(const std::string& name) const;
	void Clear();
	size_t GetCount() const;
	size_t GetCompileCount() const;
	size_t GetCacheHitCount() const;

private:
	std::unordered_map<std::string, std::unique_ptr<ShaderProgram> > m_Programs;
	size_t m_CompileCount;
	size_t m_CacheHitCount;
};
