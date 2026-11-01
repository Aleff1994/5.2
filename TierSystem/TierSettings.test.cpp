#include "../Utilities/IndependentTierSettingsExport.h"
#include <cassert>
#include <string>
int main() {
 char temp[MAX_PATH],name[MAX_PATH];assert(GetTempPathA(MAX_PATH,temp));assert(GetTempFileNameA(temp,"tier",0,name));
 std::string input=name,output=input+".out";
 assert(WritePrivateProfileStringA("Tier","ItemTierSystemEnable","1",input.c_str()));
 assert(WritePrivateProfileStringA("Tier","AdditionalTierVisualEnable","1",input.c_str()));
 assert(WritePrivateProfileStringA("Tier","MetadataTransportEnable","1",input.c_str()));
 assert(IndependentTierSettingsExport::Export(input.c_str(),output.c_str(),false));
 assert(GetPrivateProfileIntA("Tier","ItemTierSystemEnable",9,output.c_str())==9);
 assert(GetPrivateProfileIntA("Tier","ItemTierVisualEnable",9,output.c_str())==9);
 assert(GetPrivateProfileIntA("Tier","AdditionalTierSystemEnable",9,output.c_str())==9);
 assert(GetPrivateProfileIntA("Tier","AdditionalTierVisualEnable",9,output.c_str())==9);
 assert(GetPrivateProfileIntA("Tier","MetadataTransportEnable",9,output.c_str())==9);
 assert(GetPrivateProfileIntA("LifePercent","S",0,output.c_str())==4);
 assert(IndependentTierSettingsExport::Export(input.c_str(),output.c_str(),true));
 assert(GetPrivateProfileIntA("Tier","MetadataTransportEnable",9,output.c_str())==9);
 assert(WritePrivateProfileStringA("LifePercent","C","99",input.c_str()));
 assert(!IndependentTierSettingsExport::Export(input.c_str(),output.c_str(),true));
 assert(GetPrivateProfileIntA("LifePercent","C",0,output.c_str())==1);
 assert(WritePrivateProfileStringA("LifePercent","C","1",input.c_str()));
 assert(IndependentTierSettingsExport::Export(input.c_str(),output.c_str(),true));
 assert(GetPrivateProfileIntA("ExcellentAttackSpeed","S",0,output.c_str())==40);
 assert(GetPrivateProfileIntA("ExcellentMaximumSD","S",0,output.c_str())==5);
 assert(GetPrivateProfileIntA("ExcellentLifeOnKill","S",0,output.c_str())==6);
 assert(WritePrivateProfileStringA("ExcellentDamagePerLevel","C","0",input.c_str()));
 assert(!IndependentTierSettingsExport::Export(input.c_str(),output.c_str(),true));
 assert(GetPrivateProfileIntA("ExcellentDamagePerLevel","C",0,output.c_str())==30);
 assert(DeleteFileA(input.c_str()));assert(DeleteFileA(output.c_str()));
 puts("PASS: retired switches removed, defaults, GS-only authority, consistent life scale and invalid scale fallback.");
}
