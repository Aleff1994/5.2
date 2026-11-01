#include "stdafx.h"
#include "GearScoreRuntime.h"
#include "Util.h"

// Compatibility entry points only. Excellent ranks are resolved by
// IndependentTierGameplay; retired systems cannot be re-enabled by old INI keys.
namespace GearScoreRuntime {
void Load() { LogAdd(LOG_BLUE,"[ExcellentRanks] Excellent-only build: GearScore, item tiers, PvP tiers and full-set damage retired"); }
void Reset(int) {}
void Begin(LPOBJ) {}
void RecordHarmony(LPOBJ,CItem&,int,int,int) {}
void ApplyArmor(LPOBJ) {}
bool HasArmorHP(LPOBJ) { return false; }
unsigned FullSetPercent(LPOBJ) { return 0; }
bool ApplyFullSetDamage(LPOBJ,int&) { return false; }
bool InspectArmor(LPOBJ,NeonGear::ArmorResult& applied,NeonGear::SetRule& next) { applied={};next={};return false; }
NeonGear::Score Evaluate(CItem&,const IndependentItemTiers::Metadata&,bool) { return {}; }
int Effective(LPOBJ,NeonGear::HarmonyEffect,int nativeValue,bool) { return nativeValue; }
void HarmonyPreview(CItem&,std::uint8_t& effect,std::uint32_t& nativeValue,std::uint32_t& scaled,std::uint32_t& cap) { effect=0;nativeValue=scaled=cap=0; }
}
