#pragma once
#include "IndependentItemTiers.h"
namespace ItemRatingPolicy {
struct Settings {
 int damageWeight=100,defenseWeight=100,magicWeight=100,cap=500,levelPercent=5;
 IndependentItemTiers::Values points{10,20,35,50};
 int percent[12][4]{}; // Strength, Agility, Vitality, Energy; groups 0..11 only.
 Settings(){for(int g=0;g<12;++g){if(g==5){percent[g][1]=120;percent[g][3]=150;}else if(g<6){percent[g][0]=150;percent[g][1]=120;}else{percent[g][1]=30;percent[g][2]=60;}}}
};
struct Result {unsigned base=0,grade=0;unsigned short stats[4]{};};
inline unsigned Limit(long long v,unsigned maximum=1000000){return v<=0?0:v>maximum?maximum:static_cast<unsigned>(v);}
inline Result Evaluate(unsigned item,unsigned mask,int damageMin,int damageMax,int defense,int magic,
 const IndependentItemTiers::Metadata& metadata,const IndependentItemTiers::Values& scale,const Settings& settings,bool enabled,int level=0){
 Result r;
 if(!enabled || item>=12*512 || !metadata.Valid() || metadata.itemGrade==IndependentItemTiers::Grade::Legacy ||
    !IndependentItemTiers::ValidGrade(metadata.itemGrade) || scale.c<=0 || scale.s>100 || !scale.Ordered(true))return r;
 long long damage=(static_cast<long long>(damageMin<0?0:damageMin)+(damageMax<0?0:damageMax))/2;
 r.base=Limit((damage*settings.damageWeight+static_cast<long long>(defense<0?0:defense)*settings.defenseWeight+static_cast<long long>(magic<0?0:magic)*settings.magicWeight)/100);
 const int itemLevel=level<0?0:level>15?15:level;
 const int step=settings.levelPercent>=0 && settings.levelPercent<=100?settings.levelPercent:5;
 r.base=Limit((static_cast<long long>(r.base)*(100+itemLevel*step)+50)/100);
 if(!r.base)r.base=1;
 long long grade=(static_cast<long long>(r.base)*scale.Resolve(metadata.itemGrade,100,true)+50)/100;
 for(unsigned bit=0;bit<6;++bit)if(mask&(1u<<bit)){
  const auto quality=metadata.Get(IndependentItemTiers::Family::Excellent,static_cast<std::uint16_t>(bit));
  if(IndependentItemTiers::ValidGrade(quality) && quality!=IndependentItemTiers::Grade::Legacy)grade+=settings.points.Resolve(quality,0,true);
 }
 r.grade=Limit(grade);
 if(mask&63)for(unsigned n=0;n<4;++n)r.stats[n]=static_cast<unsigned short>(Limit((static_cast<long long>(r.grade)*settings.percent[item/512][n]+99)/100,settings.cap));
 return r;
}
template<class Obj> inline void Apply(const Result& r,Obj& o){o.AddStrength+=r.stats[0];o.AddDexterity+=r.stats[1];o.AddVitality+=r.stats[2];o.AddEnergy+=r.stats[3];}
}
