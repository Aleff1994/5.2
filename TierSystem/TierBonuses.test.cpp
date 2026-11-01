#include "../Utilities/IndependentTierBonuses.h"
#include <cassert>
#include <cstdio>
int main() {
 using namespace IndependentItemTiers;
 Values v{1,2,3,4};Metadata m;
 for(unsigned item=0;item<=4;++item) for(unsigned add=1;add<=4;++add) {
  m.itemGrade=static_cast<Grade>(item);m.options={{Family::Excellent,5,static_cast<Grade>(add)}};
  assert(IndependentTierBonuses::LifePercent(m,32,8,true,4,true,v)==static_cast<int>(add));
  assert(IndependentTierBonuses::LifePercent(m,32,8,true,4,false,v)==4);
  assert(IndependentTierBonuses::LifePercent(m,0,8,true,4,true,v)==4);
  assert(IndependentTierBonuses::LifePercent(m,32,8,false,30,true,v)==30);
  assert(IndependentTierBonuses::LifePercent(m,32,2,true,4,true,v)==4);
 }
 m.options={{Family::Ancient,5,Grade::C}};
 assert(IndependentTierBonuses::LifePercent(m,32,8,true,4,true,v)==4);
 m.options={{Family::Excellent,5,Grade::S}};
 assert(IndependentTierBonuses::LifePercent(m,32,8,true,4,true,Values{4,3,2,1})==4);
 assert(IndependentTierBonuses::LifePercent(m,32,8,true,4,true,Values{-1,2,3,4})==4);
 m.options.clear();assert(IndependentTierBonuses::LifePercent(m,32,8,true,4,true,v)==4);
 Values ratios{25,50,75,100};
 assert(IndependentTierBonuses::Scale(20,Grade::C,true,ratios,true)==80);
 assert(IndependentTierBonuses::Scale(20,Grade::S,true,ratios,true)==20);
 assert(IndependentTierBonuses::Scale(10,Grade::C,true,ratios)==3);
 assert(IndependentTierBonuses::Scale(10,Grade::S,true,ratios)==10);
 assert(IndependentTierBonuses::Scale(1,Grade::C,true,ratios)==1);
 assert(IndependentTierBonuses::Scale(0,Grade::C,true,ratios,true)==0);
 puts("PASS: independent item/option grades, all life tiers, legacy/off, absent bits, unrelated options, Ancient isolation and invalid settings.");
}
