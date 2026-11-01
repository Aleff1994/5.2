#include "stdafx.h"
#include "RenderBackend.h"
#include "LegacyRenderBackend.h"
#include "Modern/ModernRenderBackend.h"
#include "Modern/ModernFrustum.h"
#include "Modern/TerrainRenderer.h"
#include "Modern/ModelRenderer.h"

#include <string.h>

extern ModernFrustum g_ModernFrustum;
extern TerrainRenderer g_TerrainRenderer;
extern ModelRenderer g_ModelRenderer;

RenderBackend g_RenderBackend;

RenderBackend::RenderBackend()
	: m_ActiveType(RENDER_BACKEND_TYPE_LEGACY)
{
	m_FallbackReason[0] = '\0';
}

RenderBackend::~RenderBackend()
{
	Shutdown();
}

bool RenderBackend::Initialize(RenderBackendRequest request, const RenderCapabilities& capabilities)
{
	m_ActiveType = RENDER_BACKEND_TYPE_LEGACY;
	m_FallbackReason[0] = '\0';

	if (request == RENDER_BACKEND_MODERN || request == RENDER_BACKEND_AUTO)
	{
		if (!capabilities.SupportsOpenGL33())
		{
			strncpy_s(m_FallbackReason, sizeof(m_FallbackReason),
				"OpenGL 3.3 is not supported by the active context", _TRUNCATE);
		}
		else
		{
			m_ActiveType = RENDER_BACKEND_TYPE_MODERN;
		}
	}

	if (m_ActiveType == RENDER_BACKEND_TYPE_MODERN)
	{
		if (!m_ModernBackend.Initialize())
		{
			_snprintf_s(m_FallbackReason, sizeof(m_FallbackReason), _TRUNCATE,
				"Modern initialization failed: %s", m_ModernBackend.GetInitializationError());
			m_ActiveType = RENDER_BACKEND_TYPE_LEGACY;
		}
		else
		{
			// Initialize ShadowRenderer with modern dependencies
			m_ModernBackend.InitializeShadowRenderer(&g_ModernFrustum, &g_TerrainRenderer, &g_ModelRenderer);
		}
	}

	if (m_ActiveType == RENDER_BACKEND_TYPE_LEGACY)
	{
		LegacyRenderBackend legacyBackend;
		return legacyBackend.Initialize();
	}

	return true;
}

void RenderBackend::Shutdown()
{
	if (m_ActiveType == RENDER_BACKEND_TYPE_MODERN)
	{
		m_ModernBackend.Shutdown();
	}
}

RenderBackendType RenderBackend::GetActiveType() const { return m_ActiveType; }

const char* RenderBackend::GetActiveName() const
{
	return m_ActiveType == RENDER_BACKEND_TYPE_MODERN ? "Modern" : "Legacy";
}

const char* RenderBackend::GetFallbackReason() const { return m_FallbackReason; }

RenderBackendRequest RenderBackend::ParseRequest(const char* value)
{
	if (value != NULL && _stricmp(value, "Legacy") == 0)
		return RENDER_BACKEND_LEGACY;
	if (value != NULL && _stricmp(value, "Modern") == 0)
		return RENDER_BACKEND_MODERN;
	return RENDER_BACKEND_AUTO;
}

const char* RenderBackend::GetRequestName(RenderBackendRequest request)
{
	switch (request)
	{
	case RENDER_BACKEND_LEGACY: return "Legacy";
	case RENDER_BACKEND_MODERN: return "Modern";
	default: return "Auto";
	}
}
