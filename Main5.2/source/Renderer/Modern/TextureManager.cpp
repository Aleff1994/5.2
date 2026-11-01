#include "stdafx.h"
#include "TextureManager.h"

GLTexture2D* TextureManager::Create(const std::string& name)
{
	GLTexture2D* existing = Find(name);
	if (existing != NULL) return existing;
	std::unique_ptr<GLTexture2D> texture(new GLTexture2D());
	if (!texture->Create()) return NULL;
	GLTexture2D* result = texture.get();
	m_Textures.emplace(name, std::move(texture));
	return result;
}
GLTexture2D* TextureManager::Find(const std::string& name) const
{
	auto found = m_Textures.find(name);
	return found != m_Textures.end() ? found->second.get() : NULL;
}
void TextureManager::Clear() { m_Textures.clear(); }
size_t TextureManager::GetCount() const { return m_Textures.size(); }
