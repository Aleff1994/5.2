@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 /W4 /Fe"E:\NEONMU\SRCMainGS\Source\TierSystem\TierProtocol.test.exe" /Fo"E:\NEONMU\SRCMainGS\Source\TierSystem\TierProtocol.test.obj" "E:\NEONMU\SRCMainGS\Source\TierSystem\TierProtocol.test.cpp"
if errorlevel 1 exit /b 1
"E:\NEONMU\SRCMainGS\Source\TierSystem\TierProtocol.test.exe"
