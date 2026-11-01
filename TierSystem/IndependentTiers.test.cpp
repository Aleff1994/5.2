#include "../Utilities/IndependentItemTiers.h"
#include <cassert>
#include <cstdio>
using namespace IndependentItemTiers;
int main() {
 unsigned cases=0;
 // Every item grade and all 4^6 combinations of six Excellent option grades.
 for(unsigned item=0;item<=4;++item) for(unsigned bits=0;bits<4096;++bits) {
  Metadata m; m.itemGrade=static_cast<Grade>(item);
  for(unsigned n=0;n<6;++n) m.options.push_back({Family::Excellent,static_cast<std::uint16_t>(n),static_cast<Grade>(1+((bits>>(2*n))&3))});
  m.options.push_back({Family::Ancient,0,Grade::S}); // same id, distinct namespace
  std::vector<std::uint8_t> bytes; assert(Encode(m,bytes));
  Metadata restored; assert(Decode(bytes.data(),bytes.size(),restored));
  assert(restored.itemGrade==m.itemGrade);
  for(auto o:m.options) assert(restored.Get(o.family,o.optionId)==o.grade);
  assert(restored.Get(Family::Ancient,999)==Grade::Legacy);
  ++cases;
 }
 Metadata sentinel; sentinel.itemGrade=Grade::B;
 std::vector<std::uint8_t> valid; assert(Encode(sentinel,valid));
 for(std::size_t n=0;n<valid.size();++n) { assert(!Decode(valid.data(),n,sentinel)); assert(sentinel.itemGrade==Grade::B); }
 auto bad=valid;bad.push_back(0);assert(!Decode(bad.data(),bad.size(),sentinel));
 bad=valid;bad[0]=2;assert(!Decode(bad.data(),bad.size(),sentinel));
 bad=valid;bad[1]=255;assert(!Decode(bad.data(),bad.size(),sentinel));
 Metadata duplicate;duplicate.options={{Family::Excellent,1,Grade::S},{Family::Excellent,1,Grade::C}};
 assert(!Encode(duplicate,valid));
 const Weights w={60,25,12,3}; unsigned counts[5]={0};
 for(unsigned i=0;i<100;++i) ++counts[static_cast<unsigned>(w.Choose(i))];
 assert(counts[1]==60 && counts[2]==25 && counts[3]==12 && counts[4]==3);
 assert(w.Choose(100)==Grade::Legacy);
 assert((Weights{0,0,0,0}).Choose(0)==Grade::Legacy);
 assert((Weights{0xffffffffu,1,1,1}).Total()==0);
 const Values life={1,2,3,4};assert(life.Ordered(true));
 for(unsigned n=1;n<=4;++n) { assert(life.Resolve(static_cast<Grade>(n),99,true)==(int)n); assert(life.Resolve(static_cast<Grade>(n),99,false)==99); }
 assert(life.Resolve(Grade::Legacy,99,true)==99);
 assert((Values{25,20,15,10}).Ordered(false));
 Metadata maximum;for(unsigned i=0;i<32;++i) maximum.options.push_back({Family::Ancient,static_cast<std::uint16_t>(i),Grade::A});
 assert(Encode(maximum,valid));maximum.options.push_back({Family::Ancient,32,Grade::A});assert(!Encode(maximum,valid));
 puts("PASS: 20480 independent combinations, namespace separation, codec limits, malformed records, weighted roll boundaries, legacy/off behavior.");
}
