#include "stdafx.h"
#include "FPS.h"
#include "GraphicsOpt.h"

CFPS gFPS;

LARGE_INTEGER CFPS::m_Frequency;
LARGE_INTEGER CFPS::m_LastTime;
double CFPS::m_TargetFrameTime = 1000.0 / 60.0; // 16.6666 ms

typedef BOOL (WINAPI* PFNWGLSWAPINTERVALEXTPROC)(int interval);

CFPS::CFPS()
{
}

CFPS::~CFPS()
{
}

void CFPS::SetTargetFPS(double fps)
{
	if(fps <= 0.0)
	{
		fps = 60.0;
	}
	m_TargetFrameTime = 1000.0 / fps;
}

void CFPS::LockFPS()
{
	CGraphicsOpt::m_CurrentFrameLights = 0;

	if(m_Frequency.QuadPart == 0)
	{
		QueryPerformanceFrequency(&m_Frequency);
		QueryPerformanceCounter(&m_LastTime);
		return;
	}

	LARGE_INTEGER currentTime;
	QueryPerformanceCounter(&currentTime);

	const LONGLONG targetTicks = (LONGLONG)((m_TargetFrameTime / 1000.0) * (double)m_Frequency.QuadPart);
	const LONGLONG deadline = m_LastTime.QuadPart + targetTicks;

	if(currentTime.QuadPart < deadline)
	{
		double sleepTime = (double)(deadline - currentTime.QuadPart) * 1000.0 / (double)m_Frequency.QuadPart;

		if(sleepTime > 1.5)
		{
			Sleep((DWORD)(sleepTime - 1.0));
		}

		do
		{
			QueryPerformanceCounter(&currentTime);
		}
		while(currentTime.QuadPart < deadline);
	}

	// Preserve the absolute 60 Hz cadence instead of moving the reference to
	// the end of every frame.  Resynchronise only after a real long stall so a
	// loading pause cannot cause a burst of catch-up frames.
	if(currentTime.QuadPart - deadline > targetTicks * 4)
		m_LastTime = currentTime;
	else
		m_LastTime.QuadPart = deadline;
}

void CFPS::Init()
{
	// Garante temporizador de 1ms de alta precisão
	timeBeginPeriod(1);

	QueryPerformanceFrequency(&m_Frequency);
	QueryPerformanceCounter(&m_LastTime);

	// IMPORTANTE PARA INTEL HD GRAPHICS:
	// Desativamos o VSync do driver (0). O driver da Intel com VSync ativo derruba o FPS
	// imediatamente para 30 FPS se o quadro passar de 16.6ms!
	// Deixamos nosso próprio temporizador de alta precisão segurar em 60 FPS lisos.
	PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
	if(wglSwapIntervalEXT != NULL)
	{
		wglSwapIntervalEXT(0); // 0 = Sem trava de VSync duplo da Intel
	}
}
