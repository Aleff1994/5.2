#pragma once
#include "IndependentItemTiers.h"
#include "ExcRank/ExcRankCore.h"
namespace IndependentTierViewProtocol {
// Dedicated inventory inspection protocol. No client-authoritative ranks.
// C1,size,F3,63,version,kind,token:u32,slot:u16,item:u16; reply adds
// status:u8,serial:u32,lifeActive:u8,lifePercent:u8,metadataLength:u8,metadata. Explicit little endian.
// Disjoint address ranges prevent a warehouse/trade slot from aliasing inventory.
constexpr unsigned WarehouseBase=256,TradeOwnBase=512,TradeOtherBase=544,ShopBase=768,PersonalOwnBase=1024,PersonalOtherBase=1056;
inline bool ValidSlot(unsigned slot) {
 return slot<243 || (slot>=WarehouseBase && slot<WarehouseBase+240) ||
 (slot>=TradeOwnBase && slot<TradeOtherBase+32) || (slot>=ShopBase && slot<ShopBase+120) ||
 (slot>=PersonalOwnBase && slot<PersonalOtherBase+32);
}
enum class Status : std::uint8_t { Pending=0,Found=1,Legacy=2,Unavailable=3 };
struct Message {
 bool reply=false;std::uint32_t token=0;std::uint16_t slot=0,itemId=0;
 Status status=Status::Pending;std::uint32_t serial=0;
 std::uint8_t optionTypes[6]={};std::uint16_t optionValues[6]={};std::uint16_t ancientValue=0;std::uint8_t itemPercent=100;
 bool lifeActive=false;std::uint8_t lifePercent=0,fullCount=4;
 std::uint32_t baseScore=0,gradeScore=0;std::uint16_t exclusive[4]={};
 bool gradeEnabled=false;bool scoreValid=false;std::uint32_t refinePoints=0,rankPoints=0,lifePoints=0;
 std::uint8_t harmonyEffect=0;std::uint32_t harmonyNative=0,harmonyPvP=0,harmonyCap=0;
 IndependentItemTiers::Metadata metadata; // Compatibility-only fields, never used as ranked slots.
 ExcRank::ItemOptions ranked;std::uint16_t rankedValues[4]={};
};
inline void Put(std::vector<std::uint8_t>& b,std::uint32_t v,unsigned n) {for(unsigned i=0;i<n;++i)b.push_back(static_cast<std::uint8_t>(v>>(8*i)));}
inline std::uint32_t Read(const std::uint8_t* b,unsigned n) {std::uint32_t v=0;for(unsigned i=0;i<n;++i)v|=static_cast<std::uint32_t>(b[i])<<(8*i);return v;}
inline bool EncodeLegacy(const Message& m,std::vector<std::uint8_t>& out) {
 if(!m.token || !ValidSlot(m.slot)||m.itemId>8191)return false;
 std::vector<std::uint8_t>b{0xC1,0,0xF3,0x63,11,static_cast<std::uint8_t>(m.reply?1:0)};Put(b,m.token,4);Put(b,m.slot,2);Put(b,m.itemId,2);
 if(m.reply){if(static_cast<unsigned>(m.status)>3)return false;b.push_back(static_cast<std::uint8_t>(m.status));Put(b,m.serial,4);
  std::vector<std::uint8_t>blob;if(m.status==Status::Found){if(!m.serial||(m.ranked.slots.empty()&&!m.ranked.attributes[0].id)||!ExcRank::Encode(m.ranked,blob))return false;}
  else if(!m.ranked.slots.empty()||m.ranked.attributes[0].id)return false;
  b.push_back(static_cast<std::uint8_t>(blob.size()));b.insert(b.end(),blob.begin(),blob.end());
  for(unsigned n=0;n<m.ranked.slots.size();++n){if(!m.rankedValues[n]||m.rankedValues[n]>1000)return false;Put(b,m.rankedValues[n],2);}
  if(m.baseScore>1000000||m.gradeScore>1000000||(!m.gradeEnabled&&m.gradeScore)||(!m.scoreValid&&(m.baseScore||m.gradeScore)))return false;
  b.push_back(static_cast<std::uint8_t>((m.scoreValid?1:0)|(m.gradeEnabled?2:0)));Put(b,m.baseScore,4);Put(b,m.gradeScore,4);
 }
 b[1]=static_cast<std::uint8_t>(b.size());out.swap(b);return true;
}
inline bool DecodeLegacy(const std::uint8_t* b,std::size_t n,Message& out) {
 if(!b||n<14||n>54||b[0]!=0xC1||b[1]!=n||b[2]!=0xF3||b[3]!=0x63||b[4]!=11||b[5]>1)return false;
 Message m;m.reply=b[5]!=0;m.token=Read(b+6,4);m.slot=static_cast<std::uint16_t>(Read(b+10,2));m.itemId=static_cast<std::uint16_t>(Read(b+12,2));
 if(!m.token||!ValidSlot(m.slot)||m.itemId>8191)return false;
 if(!m.reply){if(n!=14)return false;}
 else {if(n<29||b[14]>3)return false;m.status=static_cast<Status>(b[14]);m.serial=Read(b+15,4);unsigned length=b[19];
  if(m.status==Status::Found){if(!m.serial||length<3||20+length+9>n||!ExcRank::Decode(b+20,length,m.ranked)||(m.ranked.slots.empty()&&!m.ranked.attributes[0].id)||n!=29+length+2*m.ranked.slots.size())return false;
   for(unsigned i=0;i<m.ranked.slots.size();++i){m.rankedValues[i]=static_cast<std::uint16_t>(Read(b+20+length+i*2,2));if(!m.rankedValues[i]||m.rankedValues[i]>1000)return false;}
  }else if(length||n!=29)return false;
  const auto footer=n-9;if(b[footer]>3)return false;m.scoreValid=(b[footer]&1)!=0;m.gradeEnabled=(b[footer]&2)!=0;m.baseScore=Read(b+footer+1,4);m.gradeScore=Read(b+footer+5,4);if(m.baseScore>1000000||m.gradeScore>1000000||(!m.gradeEnabled&&m.gradeScore)||(!m.scoreValid&&(m.baseScore||m.gradeScore)))return false;
 }
 out=m;return true;
}
// Version 12 appends authoritative native-EX ranks and resolved values.
inline bool Encode(const Message& m,std::vector<std::uint8_t>& out){
 std::vector<std::uint8_t> bytes;if(!EncodeLegacy(m,bytes))return false;
 bytes[4]=12;
 if(m.reply)for(unsigned bit=0;bit<6;++bit){
  auto grade=m.metadata.Get(IndependentItemTiers::Family::Excellent,static_cast<std::uint16_t>(bit));
  if(grade!=IndependentItemTiers::Grade::Legacy && (static_cast<unsigned>(grade)<2 || static_cast<unsigned>(grade)>4))return false;
  bytes.push_back(static_cast<std::uint8_t>(grade));bytes.push_back(m.optionTypes[bit]);Put(bytes,m.optionValues[bit],2);
 }
 bytes[1]=static_cast<std::uint8_t>(bytes.size());out.swap(bytes);return true;
}
inline bool Decode(const std::uint8_t* b,std::size_t n,Message& out){
 if(!b || n<14 || n>100 || b[4]!=12 || b[1]!=n)return false;
 const bool reply=b[5]==1;const unsigned tail=reply?24:0;if(n<14+tail)return false;
 std::vector<std::uint8_t> legacy(b,b+n-tail);legacy[4]=11;legacy[1]=static_cast<std::uint8_t>(legacy.size());
 Message m;if(!DecodeLegacy(legacy.data(),legacy.size(),m))return false;
 if(reply)for(unsigned bit=0;bit<6;++bit){
  const auto* p=b+n-24+bit*4;unsigned grade=p[0];if(grade && (grade<2 || grade>4))return false;
  if(grade){if(!p[1] || !Read(p+2,2))return false;m.metadata.options.push_back({IndependentItemTiers::Family::Excellent,static_cast<std::uint16_t>(bit),static_cast<IndependentItemTiers::Grade>(grade)});}
  m.optionTypes[bit]=p[1];m.optionValues[bit]=static_cast<std::uint16_t>(Read(p+2,2));
 }
 out=m;return true;
}
inline bool Matches(const Message& request,const Message& reply){return !request.reply&&reply.reply&&request.token&&request.token==reply.token&&request.slot==reply.slot&&request.itemId==reply.itemId;}
}
