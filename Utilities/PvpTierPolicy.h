#pragma once
#include <windows.h>
namespace PvpTierPolicy {
struct Settings {
 bool enabled, visual;
 int rates[11], bonusStep;
 Settings(const char* path) {
  enabled = false; // Retired: old configuration cannot enable PvP tiers.
  visual = false;
  bonusStep = GetPrivateProfileIntA("Tier", "PvpBonusPercentPerLevel", 10, path);
  if(bonusStep < 0) bonusStep=0; if(bonusStep > 100) bonusStep=100;
  const int defaults[11]={0,90,85,80,75,70,65,60,50,40,30};
  for(int n=0;n<=10;++n) rates[n]=defaults[n];
 }
 int Percent(int level) const {
  if(!enabled || level<1 || level>10) return 100;
  return 100+(level-1)*bonusStep;
 }
};
inline const Settings& Server() { static const Settings s("..\\Data\\Item\\TierSystem.ini"); return s; }
inline const Settings& Client() { static const Settings s("Data\\TierSystem.ini"); return s; }
inline int Level(unsigned char raw, bool active) {
 if(!active) return 0; return raw>=1 && raw<=10 ? raw : 1;
}
}
