#include "../Utilities/IndependentTierViewProtocol.h"
#include <cassert>
#include <cstdio>
int main() {
 using namespace IndependentTierViewProtocol;using namespace IndependentItemTiers;
 Message q;q.token=123;q.slot=12;q.itemId=4096;std::vector<std::uint8_t> b;Message d;
 assert(Encode(q,b)&&b.size()==14&&Decode(b.data(),b.size(),d));
 for(unsigned n=0;n<b.size();++n)assert(!Decode(b.data(),n,d));
 Message reply=q;reply.reply=true;reply.status=Status::Found;reply.serial=0xffffffff;
 reply.metadata.itemGrade=Grade::B;reply.optionTypes[5]=91;reply.optionValues[5]=1;reply.ancientValue=4;reply.itemPercent=25;
 for(unsigned i=0;i<32;++i)reply.metadata.options.push_back({Family::Excellent,static_cast<std::uint16_t>(i),Grade::S});
 assert(Encode(reply,b)&&b.size()==191&&Decode(b.data(),b.size(),d));assert(Matches(q,d));assert(d.optionTypes[5]==91 && d.optionValues[5]==1 && d.ancientValue==4 && d.itemPercent==25);
 assert(d.metadata.itemGrade==Grade::B&&d.metadata.Get(Family::Excellent,5)==Grade::S);
 for(unsigned n=0;n<b.size();++n)assert(!Decode(b.data(),n,d));
 Message other=q;++other.token;assert(!Matches(other,reply));other=q;++other.slot;assert(!Matches(other,reply));other=q;++other.itemId;assert(!Matches(other,reply));
 for(unsigned status=0;status<=3;++status) {reply.status=static_cast<Status>(status);assert(Encode(reply,b)&&Decode(b.data(),b.size(),d));}
 reply.status=Status::Found;reply.lifeActive=true;reply.lifePercent=1;
 assert(Encode(reply,b)&&Decode(b.data(),b.size(),d)&&d.lifeActive&&d.lifePercent==1);
 reply.lifePercent=101;assert(!Encode(reply,b));reply.lifePercent=4;
 reply.status=Status::Pending;assert(!Encode(reply,b));reply.status=Status::Found;
 reply.serial=0;assert(!Encode(reply,b));q.slot=243;assert(!Encode(q,b));
 for(unsigned slot: {0u,236u,237u,242u,256u,495u,512u,543u,544u,575u,768u,887u,1024u,1055u,1056u,1087u}) {
  q.slot=static_cast<std::uint16_t>(slot);assert(Encode(q,b)&&Decode(b.data(),b.size(),d));assert(d.slot==slot);
 }
 for(unsigned slot: {243u,255u,496u,511u,576u,767u,888u,1023u,1088u,65535u}) {
  q.slot=static_cast<std::uint16_t>(slot);assert(!Encode(q,b));
 }
 q.slot=256;assert(Encode(q,b));b[4]=1;assert(!Decode(b.data(),b.size(),d));
 b[4]=2;assert(!Decode(b.data(),b.size(),d));
 Message inventory=q;inventory.slot=0;Message storage=q;storage.reply=true;assert(!Matches(inventory,storage));
 q.slot=12;q.token=0;assert(!Encode(q,b));
 puts("PASS: request/reply, maximum payload, independent grades, unsigned serial, truncations, stale tokens, wrong slot/type, statuses and bounds.");
}
