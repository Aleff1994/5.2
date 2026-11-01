#include "stdafx.h"
#include "BufferManager.h"

GLBuffer* BufferManager::Create(const std::string& name, GLenum target)
{
	GLBuffer* existing = Find(name);
	if (existing != NULL) return existing;
	std::unique_ptr<GLBuffer> buffer(new GLBuffer(target));
	if (!buffer->Create()) return NULL;
	GLBuffer* result = buffer.get();
	m_Buffers.emplace(name, std::move(buffer));
	return result;
}
GLBuffer* BufferManager::Find(const std::string& name) const
{
	auto found = m_Buffers.find(name);
	return found != m_Buffers.end() ? found->second.get() : NULL;
}
void BufferManager::Clear() { m_Buffers.clear(); }
size_t BufferManager::GetCount() const { return m_Buffers.size(); }
