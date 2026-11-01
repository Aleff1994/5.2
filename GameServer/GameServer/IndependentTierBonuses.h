#pragma once
#include "IndependentTierCache.h"
#include "../../Utilities/IndependentTierBonuses.h"
#include "../../Utilities/IndependentExcellentTables.h"
namespace IndependentTierGameplay {
struct Settings {
 IndependentExcellentTables::Tables excellent;
 bool enabled,itemEnabled;
 IndependentItemTiers::Values life,ratios,itemRatios;
 Settings() {
  const char* path="..\\Data\\Item\\TierSystem.ini";
  excellent.threeRanksOnly=true;
  excellent.dreamProfile=true;
  unsigned tableCount=0;auto tableDefs=IndependentExcellentTables::Definitions(tableCount);
  const char* gradeKeys[]={"C","B","A","S"};
  for(unsigned row=0;row<tableCount;++row) {
   const auto& defaults=tableDefs[row].defaults;const int fallback[]={defaults.c,defaults.b,defaults.a,defaults.s};int values[4];
   for(int i=0;i<4;++i)values[i]=static_cast<int>(GetPrivateProfileIntA(tableDefs[row].section,gradeKeys[i],fallback[i],path));
   IndependentItemTiers::Values candidate{values[0],values[1],values[2],values[3]};
   if(IndependentExcellentTables::Valid(candidate,tableDefs[row]))excellent.rows[row]=candidate;
  }
  enabled=IndependentTierControl::Enabled(); itemEnabled=false; // Excellent-only: native item base stats.
  itemRatios={static_cast<int>(GetPrivateProfileIntA("ItemTierPercent","C",25,path)),static_cast<int>(GetPrivateProfileIntA("ItemTierPercent","B",50,path)),static_cast<int>(GetPrivateProfileIntA("ItemTierPercent","A",75,path)),static_cast<int>(GetPrivateProfileIntA("ItemTierPercent","S",100,path))};
  ratios={static_cast<int>(GetPrivateProfileIntA("AdditionalTierPercent","C",25,path)),
          static_cast<int>(GetPrivateProfileIntA("AdditionalTierPercent","B",50,path)),
          static_cast<int>(GetPrivateProfileIntA("AdditionalTierPercent","A",75,path)),
          static_cast<int>(GetPrivateProfileIntA("AdditionalTierPercent","S",100,path))};
  life={static_cast<int>(GetPrivateProfileIntA("LifePercent","C",1,path)),
        static_cast<int>(GetPrivateProfileIntA("LifePercent","B",2,path)),
        static_cast<int>(GetPrivateProfileIntA("LifePercent","A",3,path)),
        static_cast<int>(GetPrivateProfileIntA("LifePercent","S",4,path))};
 }
};
inline const Settings& GetSettings() { static const Settings value;return value; }
template<class Item> inline int Base(const Item* item,int value) {
 const auto& settings=GetSettings();if(!settings.itemEnabled || value<=0)return value;
 IndependentItemTiers::Metadata metadata;
 if(IndependentTierCache::Get().Lookup(item->m_Serial,item->m_Index,metadata)!=IndependentTierCache::State::Ready)return value;
 return IndependentTierBonuses::Scale(value,metadata.itemGrade,true,settings.itemRatios);
}
}
