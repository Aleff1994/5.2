#pragma once

#include "GLResources.h"
#include <memory>
#include <string>
#include <unordered_map>

class BufferManager
{
public:
	GLBuffer* Create(const std::string& name, GLenum target);
	GLBuffer* Find(const std::string& name) const;
	void Clear();
	size_t GetCount() const;

private:
	std::unordered_map<std::string, std::unique_ptr<GLBuffer> > m_Buffers;
};
