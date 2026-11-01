@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul
cl /nologo /EHsc /std:c++17 /Fe"E:\NEONMU\SRCMainGS\Source\TierSystem\MiniMapNpcOnly.test.exe" /Fo"E:\NEONMU\SRCMainGS\Source\TierSystem\MiniMapNpcOnly.test.obj" "E:\NEONMU\SRCMainGS\Source\TierSystem\MiniMapNpcOnly.test.cpp"
if errorlevel 1 exit /b 1
"E:\NEONMU\SRCMainGS\Source\TierSystem\MiniMapNpcOnly.test.exe"
