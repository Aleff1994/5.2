#pragma once
#include <windows.h>
// Global activation in the primary GS Common; shared by GS/SubSala/GSCS/DS.
// Restart GameServers/DataServer after changing the Common configuration.
namespace IndependentTierControl {
inline const char* Path(){return "..\\4.- GameServer\\Data\\GameServerInfo - Common.dat";}
inline bool Read(const char* path){return GetPrivateProfileIntA("GameServerInfo","TierSystemEnable",0,path)==1;}
inline bool Enabled(){static const bool enabled=Read(Path());return enabled;}
inline unsigned FullCount(){
 static const unsigned target=GetPrivateProfileIntA("Tier","FullExcellentCount",4,"..\\Data\\Item\\TierSystem.ini");
 return target>=1 && target<=6?target:4;
}
}
