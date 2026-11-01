#pragma once
#include "IndependentItemTiers.h"
namespace IndependentExcellentTables {
using IndependentItemTiers::Values;using IndependentItemTiers::Grade;
enum Kind { LifeOnKillDivisor=200,ManaOnKillDivisor=201,MaximumSD=202,MonsterDamage=203,CriticalRate=204,CriticalResistance=205,LifeRecoveryPercent=206 };
struct Definition {const char* section;Values defaults;bool divisor;int maximum;};
inline const Definition* Definitions(unsigned& count) {
 static const Definition rows[]={
 {"ExcellentDamageRate",{6,8,9,10},false,100},
 {"ExcellentDamagePerLevel",{30,25,20,15},true,65535},
 {"ExcellentDamagePercent",{6,8,9,10},false,100},
 {"ExcellentAttackSpeed",{20,30,35,40},false,5000},
 {"ExcellentLifeOnKill",{12,10,8,6},true,65535},
 {"ExcellentManaOnKill",{12,10,8,6},true,65535},
 {"ExcellentMaximumSD",{2,3,4,5},false,100},
 {"ExcellentDamageReduction",{2,3,4,5},false,100},
 {"ExcellentReflect",{2,3,4,5},false,100},
 {"ExcellentDefenseRatePvM",{6,9,12,15},false,1000},
 {"ExcellentZenRate",{10,15,20,25},false,1000},
 {"ExcellentMonsterDamage",{6,7,8,10},false,100},
 {"ExcellentCriticalRate",{6,7,8,10},false,100},
 {"ExcellentCriticalResistance",{6,7,8,10},false,100},
 {"ExcellentLifeRecoveryPercent",{1,1,2,3},false,100},
 {"ExcellentBookDamagePercent",{8,9,10,13},false,100}};
 count=sizeof(rows)/sizeof(rows[0]);return rows;
}
inline bool Valid(const Values& v,const Definition& d) {
 return v.c>0 && v.b>0 && v.a>0 && v.s>0 && v.c<=d.maximum && v.b<=d.maximum && v.a<=d.maximum && v.s<=d.maximum && v.Ordered(!d.divisor);
}
struct Result {int kind,value;};
struct Tables {
 Values rows[16];
 bool dreamProfile=false;
 bool threeRanksOnly=false;
 Tables(){unsigned count=0;auto defs=Definitions(count);for(unsigned i=0;i<count;++i)rows[i]=defs[i].defaults;}
 Result Resolve(const IndependentItemTiers::Metadata& metadata,unsigned mask,int slot,int kind,int legacy,bool enabled,const Values& life) const {
  Result original{kind,legacy};
  if(!enabled || slot<3 || slot>8 || !(mask&(1u<<(slot-3))))return original;
  const auto grade=metadata.Get(IndependentItemTiers::Family::Excellent,static_cast<std::uint16_t>(slot-3));
  if(!IndependentItemTiers::ValidGrade(grade) || grade==Grade::Legacy || (threeRanksOnly && grade==Grade::C))return original;
  if(kind==91)return {kind,(life.c>=0 && life.s<=100 && life.Ordered(true))?life.Resolve(grade,legacy,true):legacy};
  // Recovery slots are authoritative: some weapon configs reuse native kind 99 for both rows.
  if(dreamProfile && (kind==98 || kind==99))kind=slot==4?98:slot==3?99:kind;
  int table=-1,newKind=kind;
  switch(kind){
   case 92:table=0;break;
   case 93:case 95:case 114:table=dreamProfile?11:1;if(dreamProfile)newKind=MonsterDamage;break;
   case 94:case 96:case 115:table=(dreamProfile && kind==115)?15:2;break;
   case 97:table=3;break;
   case 98:table=dreamProfile?14:4;newKind=dreamProfile?LifeRecoveryPercent:LifeOnKillDivisor;break;
   case 99:table=dreamProfile?12:5;newKind=dreamProfile?CriticalRate:ManaOnKillDivisor;break;
   case 90:table=6;newKind=dreamProfile?MaximumSD:90;break;
   case 89:table=7;break;
   case 88:table=8;break;
   case 87:table=dreamProfile?13:9;if(dreamProfile)newKind=CriticalResistance;break;
   case 86:table=10;break;
   default:return original;
  }
  unsigned count=0;auto defs=Definitions(count);if(!Valid(rows[table],defs[table]))return original;
  return {newKind,rows[table].Resolve(grade,legacy,true)};
 }
};
inline int ScaleMonsterDamage(int damage,int percent,bool playerTarget) {
 if(playerTarget || damage<=0 || percent<=0)return damage;
 const long long amount=static_cast<long long>(damage)*(100LL+percent)/100;
 return amount>2147483647LL?2147483647:static_cast<int>(amount);
}
inline int Recovery(double maximum,int legacyPercent,double tierFraction,int flat=0) {
 const double amount=maximum*(legacyPercent/100.0+tierFraction)+flat;
 return amount<=0?0:amount>=2147483647.0?2147483647:static_cast<int>(amount);
}
}
