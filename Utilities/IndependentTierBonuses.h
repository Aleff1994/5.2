#pragma once
#include "IndependentItemTiers.h"
namespace IndependentTierBonuses {
inline int Scale(int value,IndependentItemTiers::Grade grade,bool enabled,const IndependentItemTiers::Values& ratios,bool divisor=false) {
 if(!enabled || value<=0 || grade==IndependentItemTiers::Grade::Legacy || ratios.c<=0 || ratios.s>100 || !ratios.Ordered(true))return value;
 const int percent=ratios.Resolve(grade,100,true);
 // Divisors (level/N) become larger for weaker grades. Direct bonuses round
 // to nearest integer, with a minimum of one for an existing positive option.
 const long long result=divisor?(static_cast<long long>(value)*100+percent-1)/percent:(static_cast<long long>(value)*percent+50)/100;
 return static_cast<int>(result<1?1:result>2147483647?2147483647:result);
}
inline int Excellent(const IndependentItemTiers::Metadata& metadata,unsigned mask,int slot,int option,int legacy,bool enabled,const IndependentItemTiers::Values& life,const IndependentItemTiers::Values& ratios) {
 if(!enabled || slot<3 || slot>8 || !(mask&(1u<<(slot-3))) || !metadata.Valid())return legacy;
 const auto grade=metadata.Get(IndependentItemTiers::Family::Excellent,static_cast<std::uint16_t>(slot-3));
 if(option==91)return (life.c>=0 && life.s<=100 && life.Ordered(true))?life.Resolve(grade,legacy,true):legacy;
 if((option<86 || option>99) && option!=114 && option!=115)return legacy;
 return Scale(legacy,grade,true,ratios,option==93 || option==95 || option==114);
}
// Excellent option IDs are bit positions, not display text IDs.
// Only the user-approved life scale is active in this first policy.
inline int LifePercent(const IndependentItemTiers::Metadata& metadata,
 unsigned excellentMask,int specialSlot,bool isLifeOption,int legacyValue,
 bool enabled,const IndependentItemTiers::Values& values) {
 if(!enabled || !isLifeOption || specialSlot<3 || specialSlot>8 || !metadata.Valid()) return legacyValue;
 const unsigned bit=static_cast<unsigned>(specialSlot-3);
 if((excellentMask & (1u<<bit))==0 || values.c<0 || values.s>100 || !values.Ordered(true)) return legacyValue;
 return values.Resolve(metadata.Get(IndependentItemTiers::Family::Excellent,static_cast<std::uint16_t>(bit)),legacyValue,true);
}
}
