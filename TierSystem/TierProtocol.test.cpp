#include "../Utilities/IndependentTierProtocol.h"
#include <cassert>
#include <cstdio>
using namespace IndependentTierProtocol;
int main() {
 for(auto op:{Operation::Read,Operation::Create,Operation::ReadResult,Operation::CreateResult}) {
  Message m;m.operation=op;m.requestId=0xffffffffu;
  for(unsigned n=1;n<=MaxRecords;++n) {
   Entry e;e.serial=0xffffffffu-n;e.itemId=8191;
   e.status=op==Operation::ReadResult?Status::Found:op==Operation::CreateResult?Status::Existing:Status::Missing;
   e.metadata.itemGrade=IndependentItemTiers::Grade::B;
   for(unsigned j=0;j<32;++j) e.metadata.options.push_back({IndependentItemTiers::Family::Excellent,static_cast<std::uint16_t>(j),IndependentItemTiers::Grade::S});
   m.entries.push_back(e);
   Bytes bytes;assert(Encode(m,bytes));Message decoded;assert(Decode(bytes.data(),bytes.size(),decoded));
   Bytes encoded;assert(Encode(decoded,encoded));assert(bytes==encoded);
   for(std::size_t cut=0;cut<bytes.size();++cut) assert(!Decode(bytes.data(),cut,decoded));
   for(std::size_t i=0;i<bytes.size();++i) {
    Bytes corrupt=bytes;corrupt[i]^=0xff;Message result;
    if(Decode(corrupt.data(),corrupt.size(),result)) { Bytes again;assert(Encode(result,again));assert(again==corrupt); }
   }
  }
  m.entries.push_back(m.entries.back());Bytes bytes;assert(!Encode(m,bytes));
 }
 Message m;m.requestId=1;Entry e;e.serial=123;m.entries.push_back(e);
 Bytes bytes;assert(Encode(m,bytes));Message sentinel;sentinel.requestId=77;
 bytes[4]=99;assert(!Decode(bytes.data(),bytes.size(),sentinel));assert(sentinel.requestId==77);
 m.entries.push_back(e);assert(!Encode(m,bytes));
 m.entries.resize(1);m.entries[0].serial=0;assert(!Encode(m,bytes));
 puts("PASS: read/create/replies, 1..16 entries, max payloads, truncation, mutation, duplicates, serial bounds and no partial output.");
}
