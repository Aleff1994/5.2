#include "../Utilities/IndependentTierCreation.h"
#include <cassert>
#include <cstdio>
using namespace IndependentTierCreation;
struct Store {
 ReadStatus read=ReadStatus::Missing;WriteStatus write=WriteStatus::Created;
 unsigned reads=0,writes=0;std::uint16_t item=4096;Metadata saved;
 ReadStatus Read(std::uint32_t,std::uint16_t& id,Metadata& out) { ++reads;if(read==ReadStatus::Found) { id=item;out=saved; }return read; }
 WriteStatus Write(std::uint32_t,std::uint16_t id,const Metadata& m) { ++writes;
  if(write==WriteStatus::Created || write==WriteStatus::Existing) { saved=m;item=id;read=ReadStatus::Found; }
  else if(write==WriteStatus::Conflict) { saved.itemGrade=Grade::A;read=ReadStatus::Found; }
  return write;
 }
};
struct Random { unsigned calls=0;bool fail=false;bool Draw(std::uint32_t total,std::uint32_t& roll) { ++calls;roll=total-1;return !fail; } };
int main() {
 Policy p;p.itemEnabled=true;p.additionalEnabled=true;p.itemGrade=Grade::B;
 Store s;Random rng;Metadata out;
 assert(CreateOnce(123,4096,63,p,s,rng,out)==Result::Saved);assert(out.itemGrade==Grade::B);assert(out.options.size()==6);
 for(auto o:out.options) assert(o.grade==Grade::S);
 assert(s.writes==1 && rng.calls==6);
 // Load/move/retry never re-rolls or writes again.
 assert(CreateOnce(123,4096,63,p,s,rng,out)==Result::Saved);assert(s.writes==1 && rng.calls==6);
 Store broken;broken.read=ReadStatus::Error;Random noRoll;
 assert(CreateOnce(123,4096,63,p,broken,noRoll,out)==Result::Error);assert(noRoll.calls==0 && broken.writes==0);
 Store invalid;Random failed;failed.fail=true;
 assert(CreateOnce(123,4096,63,p,invalid,failed,out)==Result::Error);assert(invalid.writes==0);
 Store conflict;conflict.write=WriteStatus::Conflict;Random r2;
 assert(CreateOnce(123,4096,1,p,conflict,r2,out)==Result::Saved);assert(out.itemGrade==Grade::A);assert(conflict.writes==1 && r2.calls==1);
 Store off;Random r3;Policy disabled;
 assert(CreateOnce(123,4096,63,disabled,off,r3,out)==Result::Legacy);assert(!off.reads && !off.writes && !r3.calls);
 for(unsigned mask=0;mask<64;++mask) {
  Store store;Random random;Metadata m;assert(CreateOnce(123,4096,(std::uint8_t)mask,p,store,random,m)==Result::Saved);
  for(unsigned i=0;i<6;++i) assert(m.Get(Family::Excellent,(std::uint16_t)i)==((mask&(1<<i))?Grade::S:Grade::Legacy));
 }
 assert(CreateOnce(0,4096,1,p,s,rng,out)==Result::Error);
 assert(CreateOnce(123,8192,1,p,s,rng,out)==Result::Error);
 assert(CreateOnce(123,4096,128,p,s,rng,out)==Result::Error);
 Policy randomPolicy;randomPolicy.itemEnabled=true;randomPolicy.additionalEnabled=true;randomPolicy.randomItem=true;
 randomPolicy.itemWeights={100,0,0,0};randomPolicy.weights={0,0,0,100};
 Store randomStore;Random randomRng;Metadata randomMeta;
 assert(CreateOnce(777,4096,32,randomPolicy,randomStore,randomRng,randomMeta,5)==Result::Saved);
 assert(randomMeta.itemGrade==Grade::C && randomMeta.Get(Family::Excellent,5)==Grade::S && randomMeta.Get(Family::Ancient,0)==Grade::S);
 assert(randomRng.calls==3);assert(CreateOnce(777,4096,32,randomPolicy,randomStore,randomRng,randomMeta,5)==Result::Saved);assert(randomRng.calls==3);
 puts("PASS: item B / options S, 64 masks, no reroll, read errors, RNG failures, conflicts, disabled behavior, identity bounds.");
}
