#pragma once
#include "IndependentItemTiers.h"
namespace NativeExcellentSelection {
// Single rank for every present EX, or explicit flag/rank pairs.
inline bool Build(unsigned mask,const std::vector<unsigned>& input,IndependentItemTiers::Metadata& output){
 if(!mask || mask>63 || input.empty())return false;
 IndependentItemTiers::Metadata result;
 if(input.size()==1){
  if(input[0]<1 || input[0]>3)return false;
  for(unsigned bit=0;bit<6;++bit)if(mask&(1u<<bit))result.options.push_back({IndependentItemTiers::Family::Excellent,static_cast<std::uint16_t>(bit),static_cast<IndependentItemTiers::Grade>(input[0]+1)});
 }else{
  if(input.size()%2 || input.size()>12)return false;
  unsigned selected=0;
  for(unsigned n=0;n<input.size();n+=2){
   unsigned flag=input[n],rank=input[n+1];
   if(!flag || flag>32 || (flag&(flag-1)) || !(mask&flag) || (selected&flag) || rank<1 || rank>3)return false;
   unsigned bit=0;while((1u<<bit)!=flag)++bit;
   result.options.push_back({IndependentItemTiers::Family::Excellent,static_cast<std::uint16_t>(bit),static_cast<IndependentItemTiers::Grade>(rank+1)});selected|=flag;
  }
  if(selected!=mask)return false;
 }
 if(!result.Valid())return false;output=result;return true;
}
}
