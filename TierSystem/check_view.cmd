@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 /W4 /Fe"E:\NEONMU\SRCMainGS\Source\TierSystem\TierView.test.exe" /Fo"E:\NEONMU\SRCMainGS\Source\TierSystem\TierView.test.obj" "E:\NEONMU\SRCMainGS\Source\TierSystem\TierView.test.cpp"
if errorlevel 1 exit /b 1
"E:\NEONMU\SRCMainGS\Source\TierSystem\TierView.test.exe"
