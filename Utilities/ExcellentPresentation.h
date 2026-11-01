#pragma once
#include "IndependentItemTiers.h"
namespace ExcellentPresentation {
struct Summary {unsigned count=0,target=4,known=0,epic=0;bool full=false,perfect=false;};
inline Summary Inspect(unsigned mask,const IndependentItemTiers::Metadata& metadata,int target) {
 Summary result;result.target=(target>=1 && target<=6)?static_cast<unsigned>(target):4u;
 for(unsigned bit=0;bit<6;++bit)if(mask&(1u<<bit)) {
  ++result.count;
  const auto grade=metadata.Get(IndependentItemTiers::Family::Excellent,static_cast<std::uint16_t>(bit));
  if(IndependentItemTiers::ValidGrade(grade) && grade!=IndependentItemTiers::Grade::Legacy)++result.known;
  if(grade==IndependentItemTiers::Grade::S)++result.epic;
 }
 result.full=result.count>=result.target;
 result.perfect=result.full && result.known==result.count && result.epic==result.count;
 return result;
}
}
