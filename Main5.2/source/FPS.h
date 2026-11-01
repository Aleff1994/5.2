#pragma once

#include <windows.h>

//====================================================================================================
// SISTEMA DE 60 FPS ESTÁVEL SEM QUEDAS (Main.dll)
// Mantém taxa cravada em 60 FPS com precisão de micro-segundos,
// sincronizado com o gerenciador de renderização e animação.
//====================================================================================================

class CFPS
{
public:
	CFPS();
	virtual ~CFPS();

	void Init();
	static void LockFPS();
	static void SetTargetFPS(double fps);

private:
	static LARGE_INTEGER m_Frequency;
	static LARGE_INTEGER m_LastTime;
	static double m_TargetFrameTime;
};

extern CFPS gFPS;
