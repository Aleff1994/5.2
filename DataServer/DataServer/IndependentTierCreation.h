#pragma once
#include "IndependentTierStore.h"
#include "Util.h"
#include "../../Utilities/IndependentTierControl.h"
#include "../../Utilities/IndependentTierCreation.h"
#include <bcrypt.h>
#include <map>
#pragma comment(lib,"bcrypt.lib")
namespace IndependentTierDropCreation {
namespace Core=IndependentTierCreation;
struct Random {
 bool Draw(std::uint32_t total,std::uint32_t& roll) {
  if(!total) return false;
  const std::uint32_t threshold=(std::uint32_t(0)-total)%total;
  for(int n=0;n<64;++n) {
   std::uint32_t value=0;
   if(BCryptGenRandom(0,reinterpret_cast<PUCHAR>(&value),sizeof(value),BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0) return false;
   if(value>=threshold) { roll=value%total;return true; }
  }
  return false;
 }
};
struct Store {
 Core::ReadStatus Read(std::uint32_t serial,std::uint16_t& item,IndependentItemTiers::Metadata& metadata) {
  IndependentTierStore::Record record;const auto status=IndependentTierStore::Read(gQueryManager,serial,record);
  if(status==IndependentTierStore::Status::Missing) return Core::ReadStatus::Missing;
  if(status!=IndependentTierStore::Status::Found) return Core::ReadStatus::Error;
  item=static_cast<std::uint16_t>(record.originalItemId);metadata=record.metadata;return Core::ReadStatus::Found;
 }
 Core::WriteStatus Write(std::uint32_t serial,std::uint16_t item,const IndependentItemTiers::Metadata& metadata) {
  switch(IndependentTierStore::Create(gQueryManager,serial,item,metadata)) {
   case IndependentTierStore::Status::Created:return Core::WriteStatus::Created;
   case IndependentTierStore::Status::Existing:return Core::WriteStatus::Existing;
   case IndependentTierStore::Status::Conflict:return Core::WriteStatus::Conflict;
   default:return Core::WriteStatus::Error;
  }
 }
};
struct Settings {
 bool enabled=false,item=false,additional=false;
 IndependentItemTiers::Weights weights={60,25,12,3},itemWeights={60,25,12,3};
 bool separateExcellentWeights=false;
 IndependentItemTiers::Weights excellentWeights={0,75,20,5};
 std::map<unsigned,IndependentItemTiers::Grade> grades;
 static const char* Path() { return "..\\Data\\Item\\TierSystem.ini"; }
 Settings() {
  enabled=additional=IndependentTierControl::Enabled(); item=false;
  itemWeights.c=GetPrivateProfileIntA("ItemTierWeights","C",60,Path());
  itemWeights.b=GetPrivateProfileIntA("ItemTierWeights","B",25,Path());
  itemWeights.a=GetPrivateProfileIntA("ItemTierWeights","A",12,Path());
  itemWeights.s=GetPrivateProfileIntA("ItemTierWeights","S",3,Path());
  weights.c=GetPrivateProfileIntA("AdditionalTierWeights","C",60,Path());
  weights.b=GetPrivateProfileIntA("AdditionalTierWeights","B",25,Path());
  weights.a=GetPrivateProfileIntA("AdditionalTierWeights","A",12,Path());
  weights.s=GetPrivateProfileIntA("AdditionalTierWeights","S",3,Path());
  char configured[24]={0};
  GetPrivateProfileStringA("ExcellentRankWeights","Rare","",configured,sizeof(configured),Path());
  { // Excellent-only generation always uses the three supported ranks.
   excellentWeights.b=GetPrivateProfileIntA("ExcellentRankWeights","Rare",75,Path());
   excellentWeights.a=GetPrivateProfileIntA("ExcellentRankWeights","Legendary",20,Path());
   excellentWeights.s=GetPrivateProfileIntA("ExcellentRankWeights","Epic",5,Path());
   separateExcellentWeights=true;
   if(!excellentWeights.Total()) {
    excellentWeights={0,75,20,5};
    LogAdd(LOG_RED,"[ExcellentRanks] Invalid weights; using Rare=75 Legendary=20 Epic=5");
   }
  }
 }
 IndependentItemTiers::Grade Grade(unsigned itemId) {
  auto found=grades.find(itemId);if(found!=grades.end()) return found->second;
  char key[16],value[8]={0};sprintf_s(key,"%u",itemId);GetPrivateProfileStringA("ItemGrades",key,"",value,sizeof(value),Path());
  using G=IndependentItemTiers::Grade;G grade=G::Legacy;
  if(!value[1]) { if(value[0]=='C') grade=G::C;else if(value[0]=='B') grade=G::B;else if(value[0]=='A') grade=G::A;else if(value[0]=='S') grade=G::S; }
  grades[itemId]=grade;return grade;
 }
};
inline bool Prepare(std::uint32_t serial,unsigned itemId,unsigned map,unsigned excellentMask,unsigned ancientOption) {
 static Settings settings;
 if(!settings.enabled || (!settings.item && !settings.additional)) return true;
 // First rollout: standard weapons/armor on world-drop routes only. No crafting,
 // wings, pets or special inventory routes; their option bits have different meanings.
 if(map>=0xE0 || itemId>=14*512) return true;
 Core::Policy policy;policy.itemEnabled=settings.item;policy.additionalEnabled=settings.additional;
 policy.randomItem=true;policy.itemWeights=settings.itemWeights;policy.weights=settings.weights;
 policy.separateExcellentWeights=settings.separateExcellentWeights;policy.excellentWeights=settings.excellentWeights;
 Store store;Random random;IndependentItemTiers::Metadata result;
 return Core::CreateOnce(serial,static_cast<std::uint16_t>(itemId),static_cast<std::uint8_t>((itemId/512==12)?0:(excellentMask&63)),policy,store,random,result,0)!=Core::Result::Error;
}
}
