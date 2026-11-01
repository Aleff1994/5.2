#pragma once
#include "User.h"
#include "../../Utilities/IndependentItemTiers.h"
#include "../../Utilities/GearScore/ItemGearScore.h"
#include "../../Utilities/FullSetDamage.h"

namespace GearScoreRuntime {
// Runtime sidecar preserves OBJECTSTRUCT layout and validates connection identity.
void Load();
// Clear sidecar on object initialization/disconnection, even when a socket is reused.
void Reset(int index);
void Begin(LPOBJ object);
void RecordHarmony(LPOBJ object, CItem& item, int type, int option, int nativeValue);
void ApplyArmor(LPOBJ object);
bool HasArmorHP(LPOBJ object);
unsigned FullSetPercent(LPOBJ object);
bool ApplyFullSetDamage(LPOBJ object,int& damage);
// Read the last applied result, without recalculating stats or requesting metadata.
bool InspectArmor(LPOBJ object, NeonGear::ArmorResult& applied, NeonGear::SetRule& next);
NeonGear::Score Evaluate(CItem& item, const IndependentItemTiers::Metadata& metadata, bool ready);
int Effective(LPOBJ object, NeonGear::HarmonyEffect effect, int nativeValue, bool pvp);
void HarmonyPreview(CItem& item,std::uint8_t& effect,std::uint32_t& nativeValue,std::uint32_t& scaled,std::uint32_t& cap);
}
