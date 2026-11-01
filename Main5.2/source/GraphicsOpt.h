#pragma once

#include <windows.h>
#include <gl/GL.h>

//====================================================================================================
// OTIMIZAÇÃO DE ALTA PERFORMANCE PARA INTEL HD GRAPHICS E 30+ BOTS
// - Zero overhead de driver OpenGL (sem chamadas redundantes de glBindTexture por frame)
// - Compatibilidade 100% nativa com Intel HD Graphics 4600 (sem software fallbacks)
// - Culling de luz dinâmica nos bots (recupera CPU e estabiliza FPS)
// - Convive com o limitador unico de frame do MainScene (sem espera duplicada)
//====================================================================================================

class CGraphicsOpt
{
public:
	CGraphicsOpt();
	virtual ~CGraphicsOpt();

	void Init();

	static void ApplyQualitySettings();
	static void InitPerformanceHooks();

	// Otimização de luz dinâmica nos bots
	static void HookAddTerrainLight(float x, float y, float r, float g, float b);

	static bool m_DynamicLightCulling;
	static float m_MaxLightDistance;
	static int m_MaxLightsPerFrame;
	static int m_CurrentFrameLights;
};

extern CGraphicsOpt gGraphicsOpt;
