#pragma once

#include "RenderCapabilities.h"
#include "Modern/ModernRenderBackend.h"

enum RenderBackendRequest
{
	RENDER_BACKEND_AUTO,
	RENDER_BACKEND_LEGACY,
	RENDER_BACKEND_MODERN
};

enum RenderBackendType
{
	RENDER_BACKEND_TYPE_LEGACY,
	RENDER_BACKEND_TYPE_MODERN
};

class RenderBackend
{
public:
	RenderBackend();
	~RenderBackend();
	bool Initialize(RenderBackendRequest request, const RenderCapabilities& capabilities);
	void Shutdown();
	RenderBackendType GetActiveType() const;
	const char* GetActiveName() const;
	const char* GetFallbackReason() const;
	static RenderBackendRequest ParseRequest(const char* value);
	static const char* GetRequestName(RenderBackendRequest request);

	ModernRenderBackend* GetModernBackend() { return m_ActiveType == RENDER_BACKEND_TYPE_MODERN ? &m_ModernBackend : nullptr; }

private:
	RenderBackendType m_ActiveType;
	char m_FallbackReason[256];
	ModernRenderBackend m_ModernBackend;
};

extern RenderBackend g_RenderBackend;
