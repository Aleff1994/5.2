#pragma once

#include "GLResources.h"
#include <memory>
#include <string>
#include <unordered_map>

class TextureManager
{
public:
	GLTexture2D* Create(const std::string& name);
	GLTexture2D* Find(const std::string& name) const;
	void Clear();
	size_t GetCount() const;

private:
	std::unordered_map<std::string, std::unique_ptr<GLTexture2D> > m_Textures;
};
