@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul
if errorlevel 1 exit /b 1
cl /nologo /Zs /EHsc /std:c++17 /DWIN32 /DNDEBUG /D_WINDOWS /DGAMESERVER_TYPE=0 /DGAMESERVER_UPDATE=603 /DGAMESERVER_LANGUAGE=1 /DGAMESERVER_LICENSE=0 /I"E:\NEONMU\SRCMainGS\Source\GameServer\GameServer" /I"E:\NEONMU\SRCMainGS\Source\GameServer\GameServer\lua" /I"E:\NEONMU\SRCMainGS\Source\GameServer\GameServer\lua\include52" "E:\NEONMU\SRCMainGS\Source\GameServer\GameServer\Protocol.cpp"
exit /b %errorlevel%
