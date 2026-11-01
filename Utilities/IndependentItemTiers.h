#pragma once
// Independent tier metadata. Deliberately separate from the legacy 12/16-byte item formats.
// This module is not a packet handler: only authoritative creation code may assign rolls.
#include <cstdint>
#include <cstddef>
#include <vector>
namespace IndependentItemTiers {
enum class Grade : std::uint8_t { Legacy=0, C=1, B=2, A=3, S=4 };
enum class Family : std::uint8_t { Excellent=1, Ancient=2 };
inline bool ValidGrade(Grade g) { return static_cast<unsigned>(g)<=4; }
inline const char* Label(Grade g) {
 switch(g) { case Grade::C:return "C"; case Grade::B:return "B"; case Grade::A:return "A"; case Grade::S:return "S"; default:return ""; }
}
struct OptionGrade {
 Family family;
 std::uint16_t optionId;
 Grade grade;
};
struct Metadata {
 static const std::size_t MaxOptions=32;
 Grade itemGrade=Grade::Legacy;
 std::vector<OptionGrade> options;
 bool Valid() const {
  if(!ValidGrade(itemGrade) || options.size()>MaxOptions) return false;
  for(std::size_t i=0;i<options.size();++i) {
   const auto& o=options[i];
   if((o.family!=Family::Excellent && o.family!=Family::Ancient) ||
       !ValidGrade(o.grade) || o.grade==Grade::Legacy) return false;
   for(std::size_t j=0;j<i;++j) if(options[j].family==o.family && options[j].optionId==o.optionId) return false;
  }
  return true;
 }
 Grade Get(Family family,std::uint16_t id) const {
  for(const auto& o:options) if(o.family==family && o.optionId==id) return o.grade;
  return Grade::Legacy;
 }
};
// Explicit little-endian encoding; no C++ struct layout or padding on disk/wire.
// [version=1][item grade][option count], then [family][id low][id high][grade].
inline bool Encode(const Metadata& meta,std::vector<std::uint8_t>& output) {
 if(!meta.Valid()) return false;
 std::vector<std::uint8_t> result;
 result.push_back(1); result.push_back(static_cast<std::uint8_t>(meta.itemGrade));
 result.push_back(static_cast<std::uint8_t>(meta.options.size()));
 for(const auto& o:meta.options) {
  result.push_back(static_cast<std::uint8_t>(o.family));
  result.push_back(static_cast<std::uint8_t>(o.optionId));
  result.push_back(static_cast<std::uint8_t>(o.optionId>>8));
  result.push_back(static_cast<std::uint8_t>(o.grade));
 }
 output.swap(result); return true;
}
inline bool Decode(const std::uint8_t* bytes,std::size_t size,Metadata& output) {
 if(!bytes || size<3 || bytes[0]!=1 || bytes[2]>Metadata::MaxOptions || size!=3+4*static_cast<std::size_t>(bytes[2])) return false;
 Metadata result; result.itemGrade=static_cast<Grade>(bytes[1]);
 for(std::size_t p=3;p<size;p+=4)
  result.options.push_back({static_cast<Family>(bytes[p]),static_cast<std::uint16_t>(bytes[p+1]|(bytes[p+2]<<8)),static_cast<Grade>(bytes[p+3])});
 if(!result.Valid()) return false;
 output=result; return true;
}
struct Weights {
 std::uint32_t c,b,a,s;
 std::uint32_t Total() const {
  const std::uint64_t total=static_cast<std::uint64_t>(c)+b+a+s;
  return total>0 && total<=1000000 ? static_cast<std::uint32_t>(total):0;
 }
 // Caller supplies an unbiased server roll in [0,Total). No modulo reduction here.
 Grade Choose(std::uint32_t roll) const {
  if(!Total() || roll>=Total()) return Grade::Legacy;
  if(roll<c) return Grade::C; roll-=c;
  if(roll<b) return Grade::B; roll-=b;
  if(roll<a) return Grade::A; return Grade::S;
 }
};
struct Values {
 int c,b,a,s;
 int Resolve(Grade grade,int legacyValue,bool enabled) const {
  if(!enabled) return legacyValue;
  switch(grade) { case Grade::C:return c; case Grade::B:return b; case Grade::A:return a; case Grade::S:return s; default:return legacyValue; }
 }
 // Use false for denominators such as level/N: a smaller denominator is stronger.
 bool Ordered(bool higherIsBetter) const {
  return higherIsBetter ? c<=b && b<=a && a<=s : c>=b && b>=a && a>=s;
 }
};
}
