#include "stdafx.h"
#include "GraphicsOpt.h"

CGraphicsOpt gGraphicsOpt;

// Visual-fidelity rule: dynamic lights from items, fire and skills must never
// disappear because of a global per-frame quota or an arbitrary distance.
bool CGraphicsOpt::m_DynamicLightCulling = false;
float CGraphicsOpt::m_MaxLightDistance = 450.0f; // Distância otimizada para iGPU Intel
int CGraphicsOpt::m_MaxLightsPerFrame = 15;       // Limite leve para não sobrecarregar a Intel HD
int CGraphicsOpt::m_CurrentFrameLights = 0;

static bool g_bInitialized = false;

typedef void (*PFN_ADDTERRAINLIGHT)(float x, float y, float r, float g, float b);
static PFN_ADDTERRAINLIGHT g_pOriginalAddTerrainLight = NULL;

#define MAIN_ADDTERRAINLIGHT_OFFSET 0x005DFE90
#define MAIN_HERO_POS_X 0x08793438
#define MAIN_HERO_POS_Y 0x0879343C

static bool CreateHook5(void* pTarget, void* pDetour, void** ppOriginal)
{
	if(!pTarget || !pDetour) return false;

	BYTE* pTargetByte = (BYTE*)pTarget;
	DWORD dwOldProtect;
	if(!VirtualProtect(pTargetByte, 5, PAGE_EXECUTE_READWRITE, &dwOldProtect))
	{
		return false;
	}

	BYTE* pTrampoline = (BYTE*)VirtualAlloc(NULL, 16, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if(pTrampoline && ppOriginal)
	{
		memcpy(pTrampoline, pTargetByte, 5);
		pTrampoline[5] = 0xE9;
		DWORD dwTrampolineOffset = (DWORD)(pTargetByte + 5) - (DWORD)(pTrampoline + 10);
		memcpy(&pTrampoline[6], &dwTrampolineOffset, 4);
		*ppOriginal = pTrampoline;
	}

	pTargetByte[0] = 0xE9;
	DWORD dwOffset = (DWORD)pDetour - (DWORD)(pTargetByte + 5);
	memcpy(&pTargetByte[1], &dwOffset, 4);

	VirtualProtect(pTargetByte, 5, dwOldProtect, &dwOldProtect);
	FlushInstructionCache(GetCurrentProcess(), pTargetByte, 5);
	return true;
}

CGraphicsOpt::CGraphicsOpt()
{
}

CGraphicsOpt::~CGraphicsOpt()
{
}

// --------------------------------------------------------------------------------------------------
// CONFIGURAÇÕES COMPATÍVEIS COM INTEL HD GRAPHICS (SEM SOFTWARE FALLBACKS)
// --------------------------------------------------------------------------------------------------
void CGraphicsOpt::ApplyQualitySettings()
{
	// Desativa modos que causam lentidão extrema no driver OpenGL da Intel
	glDisable(GL_LINE_SMOOTH);
	glDisable(GL_POINT_SMOOTH);

	// Mantém correção de perspectiva de alta qualidade sem custo
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
}

// --------------------------------------------------------------------------------------------------
// OTIMIZAÇÃO CRÍTICA DE LUZ NO TERRENO (30 BOTS)
// Desafoga a memória compartilhada e a CPU da Intel HD 4600
// --------------------------------------------------------------------------------------------------
void CGraphicsOpt::HookAddTerrainLight(float x, float y, float r, float g, float b)
{
	if(m_DynamicLightCulling)
	{
		if(m_CurrentFrameLights >= m_MaxLightsPerFrame)
		{
			return;
		}

		float heroX = *(float*)MAIN_HERO_POS_X;
		float heroY = *(float*)MAIN_HERO_POS_Y;

		if(heroX > 0.0f && heroY > 0.0f)
		{
			float dx = x - heroX;
			float dy = y - heroY;
			float distSq = (dx * dx) + (dy * dy);

			if(distSq > (m_MaxLightDistance * m_MaxLightDistance))
			{
				return;
			}
		}

		m_CurrentFrameLights++;
	}

	if(g_pOriginalAddTerrainLight)
	{
		g_pOriginalAddTerrainLight(x, y, r, g, b);
	}
}

void CGraphicsOpt::InitPerformanceHooks()
{
	if(g_bInitialized) return;
	g_bInitialized = true;

	// MainScene already paces the complete frame through csteady_clock.
	// A second limiter on both SwapBuffers entry points caused uneven frame
	// times and did not reduce the character/effect rendering workload.

	// Do not install the old AddTerrainLight hook. Even when its distance test
	// looked harmless, the 15-light quota changed which lights reached the
	// renderer according to submission order. Calling the original function
	// directly preserves every item, fire and skill light exactly.
}

void CGraphicsOpt::Init()
{
	ApplyQualitySettings();
	InitPerformanceHooks();
}
