#pragma once

struct RenderCapabilities
{
	RenderCapabilities();
	bool Detect();
	bool SupportsOpenGL33() const;

	int OpenGLMajor;
	int OpenGLMinor;
	bool CompatibilityProfile;
	char Vendor[128];
	char Renderer[256];
	char Version[128];
	char GLSLVersion[128];
};
