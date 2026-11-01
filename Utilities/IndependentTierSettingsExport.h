#pragma once
#include <windows.h>
#include <cstdio>
#include "IndependentItemTiers.h"
#include "IndependentExcellentTables.h"
namespace IndependentTierSettingsExport {
// Separate INI sidecars keep the existing data.pak binary layout unchanged.
inline bool Export(const char* input,const char* output,bool server) {
 bool ok=true;
 // Activation lives only in the server INI, never in MainInfo/client exports.
 const char* retired[]={"Enable","ItemTierSystemEnable","ItemTierVisualEnable","AdditionalTierSystemEnable","AdditionalTierVisualEnable","MetadataTransportEnable","IndependentDropCreationEnable"};
 for(const char* key:retired)if(!WritePrivateProfileStringA("Tier",key,nullptr,output))ok=false;
 const char* flags[]={"PvpSystemEnable","PvpVisualEnable","EquipmentSystemEnable","EquipmentVisualEnable","ExcellentProfile"};
 for(const char* key:flags) {
  const char* value=GetPrivateProfileIntA("Tier",key,0,input)==1?"1":"0";
  if(!WritePrivateProfileStringA("Tier",key,value,output))ok=false;
 }
 int full=static_cast<int>(GetPrivateProfileIntA("Tier","FullExcellentCount",4,input));
 if(full<1 || full>6)full=4;
 char fullText[8];sprintf_s(fullText,"%d",full);
 if(!WritePrivateProfileStringA("Tier","FullExcellentCount",fullText,output))ok=false;
 int bonus=static_cast<int>(GetPrivateProfileIntA("Tier","PvpBonusPercentPerLevel",10,input));
 if(bonus<0)bonus=0;if(bonus>100)bonus=100;
 char text[24];sprintf_s(text,"%d",bonus);
 if(!WritePrivateProfileStringA("Tier","PvpBonusPercentPerLevel",text,output))ok=false;
 const char* grades[]={"C","B","A","S"};int life[4];
 for(int n=0;n<4;++n)life[n]=static_cast<int>(GetPrivateProfileIntA("LifePercent",grades[n],n+1,input));
 const IndependentItemTiers::Values scale{life[0],life[1],life[2],life[3]};
 if(scale.c<0 || scale.s>100 || !scale.Ordered(true)) {
  // Same safe default on client and server; report invalid input to the caller.
  for(int n=0;n<4;++n)life[n]=n+1;ok=false;
 }
 for(int n=0;n<4;++n) {
  sprintf_s(text,"%d",life[n]);
  if(!WritePrivateProfileStringA("LifePercent",grades[n],text,output))ok=false;
 }
 const char* sections[]={"AdditionalTierPercent","ItemTierWeights","AdditionalTierWeights","ItemTierPercent"};
 const int defaults[4][4]={{25,50,75,100},{60,25,12,3},{60,25,12,3},{25,50,75,100}};
 for(int section=0;section<4;++section) {
  int values[4];for(int i=0;i<4;++i)values[i]=static_cast<int>(GetPrivateProfileIntA(sections[section],grades[i],defaults[section][i],input));
  const long long total=static_cast<long long>(values[0])+values[1]+values[2]+values[3];
  bool valid=(section==0 || section==3)?(values[0]>0 && values[0]<=values[1] && values[1]<=values[2] && values[2]<=values[3] && values[3]<=100):
      (values[0]>=0 && values[1]>=0 && values[2]>=0 && values[3]>=0 && total>0 && total<=1000000);
  if(!valid){ok=false;for(int i=0;i<4;++i)values[i]=defaults[section][i];}
  for(int i=0;i<4;++i){sprintf_s(text,"%d",values[i]);if(!WritePrivateProfileStringA(sections[section],grades[i],text,output))ok=false;}
 }
 unsigned tableCount=0;const auto definitions=IndependentExcellentTables::Definitions(tableCount);
 for(unsigned i=0;i<tableCount;++i) {
  const auto& def=definitions[i];const int fallback[]={def.defaults.c,def.defaults.b,def.defaults.a,def.defaults.s};int values[4];
  for(int g=0;g<4;++g)values[g]=static_cast<int>(GetPrivateProfileIntA(def.section,grades[g],fallback[g],input));
  IndependentItemTiers::Values table{values[0],values[1],values[2],values[3]};
  if(!IndependentExcellentTables::Valid(table,def)){ok=false;for(int g=0;g<4;++g)values[g]=fallback[g];}
  for(int g=0;g<4;++g){sprintf_s(text,"%d",values[g]);if(!WritePrivateProfileStringA(def.section,grades[g],text,output))ok=false;}
 }
 return ok;
}
}
