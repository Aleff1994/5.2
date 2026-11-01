#pragma once
#include "IndependentItemTiers.h"
namespace IndependentTierCreation {
using namespace IndependentItemTiers;
enum class ReadStatus { Error,Missing,Found };
enum class WriteStatus { Error,Created,Existing,Conflict };
enum class Result { Error,Legacy,Saved };
struct Policy { bool itemEnabled=false,additionalEnabled=false; bool randomItem=false; Weights itemWeights={60,25,12,3}; Grade itemGrade=Grade::Legacy; Weights weights={60,25,12,3}; bool separateExcellentWeights=false; Weights excellentWeights={0,75,20,5}; };
// Store::Read(serial,itemId,metadata) and Store::Write(serial,itemId,metadata).
// Random::Draw(total,roll) must supply an unbiased server-side roll.
template<class Store,class Random>
Result CreateOnce(std::uint32_t serial,std::uint16_t itemId,std::uint8_t excellentMask,const Policy& policy,Store& store,Random& random,Metadata& output,std::uint8_t ancientOption=0) {
 if(!serial || itemId>8191 || (excellentMask&0xC0) || !ValidGrade(policy.itemGrade)) return Result::Error;
 if(!policy.itemEnabled && !policy.additionalEnabled) return Result::Legacy;
 Metadata existing;std::uint16_t storedItem=0;
 const auto read=store.Read(serial,storedItem,existing);
 if(read==ReadStatus::Error) return Result::Error;
 if(read==ReadStatus::Found) {
  if(storedItem!=itemId || !existing.Valid()) return Result::Error;
  output=existing;return Result::Saved;
 }
 Metadata created;created.itemGrade=policy.itemEnabled?policy.itemGrade:Grade::Legacy;
 if(policy.itemEnabled && policy.randomItem) {
  const auto total=policy.itemWeights.Total();std::uint32_t roll=0;
  if(!total || !random.Draw(total,roll) || roll>=total)return Result::Error;
  created.itemGrade=policy.itemWeights.Choose(roll);
 }
 if(policy.additionalEnabled && (ancientOption&3) && ((ancientOption>>2)&3)) {
  const auto total=policy.weights.Total();std::uint32_t roll=0;
  if(!total || !random.Draw(total,roll) || roll>=total)return Result::Error;
  created.options.push_back({Family::Ancient,0,policy.weights.Choose(roll)});
 }
 if(policy.additionalEnabled && excellentMask) {
  const auto& excellentWeights=policy.separateExcellentWeights?policy.excellentWeights:policy.weights;
  const auto total=excellentWeights.Total();if(!total) return Result::Error;
  for(unsigned n=0;n<6;++n) if(excellentMask&(1u<<n)) {
   std::uint32_t roll=0;if(!random.Draw(total,roll) || roll>=total) return Result::Error;
   created.options.push_back({Family::Excellent,static_cast<std::uint16_t>(n),excellentWeights.Choose(roll)});
  }
 }
 if(created.itemGrade==Grade::Legacy && created.options.empty()) return Result::Legacy;
 const auto write=store.Write(serial,itemId,created);
 if(write==WriteStatus::Created || write==WriteStatus::Existing) { output=created;return Result::Saved; }
 if(write==WriteStatus::Conflict) {
  // Another creator won. Read the authoritative record; never overwrite or roll again.
  if(store.Read(serial,storedItem,existing)==ReadStatus::Found && storedItem==itemId && existing.Valid()) {
   output=existing;return Result::Saved;
  }
 }
 return Result::Error;
}
}
