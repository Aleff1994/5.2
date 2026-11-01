#include "stdafx.h"
#include "RenderCapabilities.h"

#include <stdio.h>
#include <string.h>

namespace
{
	void CopyGLString(char* destination, size_t destinationSize, GLenum name)
	{
		const GLubyte* value = glGetString(name);
		if (value == NULL)
		{
			value = (const GLubyte*)"unknown";
		}
		strncpy_s(destination, destinationSize, (const char*)value, _TRUNCATE);
	}
}

RenderCapabilities::RenderCapabilities()
	: OpenGLMajor(0), OpenGLMinor(0), CompatibilityProfile(false)
{
	Vendor[0] = '\0';
	Renderer[0] = '\0';
	Version[0] = '\0';
	GLSLVersion[0] = '\0';
}

bool RenderCapabilities::Detect()
{
	CopyGLString(Vendor, sizeof(Vendor), GL_VENDOR);
	CopyGLString(Renderer, sizeof(Renderer), GL_RENDERER);
	CopyGLString(Version, sizeof(Version), GL_VERSION);
	CopyGLString(GLSLVersion, sizeof(GLSLVersion), GL_SHADING_LANGUAGE_VERSION);

	OpenGLMajor = 0;
	OpenGLMinor = 0;
	sscanf_s(Version, "%d.%d", &OpenGLMajor, &OpenGLMinor);
	CompatibilityProfile = false;

#ifdef GL_CONTEXT_PROFILE_MASK
	if (OpenGLMajor > 3 || (OpenGLMajor == 3 && OpenGLMinor >= 2))
	{
		GLint profileMask = 0;
		glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &profileMask);
		CompatibilityProfile = (profileMask & GL_CONTEXT_COMPATIBILITY_PROFILE_BIT) != 0;
	}
#endif
	return OpenGLMajor > 0;
}

bool RenderCapabilities::SupportsOpenGL33() const
{
	return OpenGLMajor > 3 || (OpenGLMajor == 3 && OpenGLMinor >= 3);
}
