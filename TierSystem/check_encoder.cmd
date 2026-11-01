@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul
if errorlevel 1 exit /b 1
cl /nologo /Zs /EHsc /std:c++17 /DWIN32 /DNDEBUG /D_CONSOLE /DGAMESERVER_PREMIUM=1 /I"E:\NEONMU\SRCMainGS\Source\Encoder\Encoder" "E:\NEONMU\SRCMainGS\Source\Encoder\Encoder\Encoder.cpp"
exit /b %errorlevel%
