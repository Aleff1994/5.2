#pragma once

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

inline void WriteLoginTrace(const char* format, ...)
{
	// Diagnostico de login desativado: nao criar LoginTrace_<PID>.log.
	(void)format;
}
