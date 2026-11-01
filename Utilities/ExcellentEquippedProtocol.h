#pragma once
#include <cstdint>
#include <vector>
namespace ExcellentEquipped {
constexpr unsigned Subcode=0x6C;
constexpr unsigned Kinds[]={86,88,89,91,92,207,96,97,115,202,203,204,205,206};
constexpr unsigned Count=sizeof(Kinds)/sizeof(Kinds[0]);
struct Snapshot { bool enabled=false,pending=false;unsigned pieces=0,ranks[3]={},values[Count]={},exclusive[4]={},physicalPercent=0,magicPercent=0; };
inline void Put(std::vector<std::uint8_t>& b,unsigned v,unsigned n) { for(unsigned i=0;i<n;++i)b.push_back(static_cast<std::uint8_t>(v>>(i*8))); }
inline unsigned Read(const std::uint8_t* b,unsigned n) { unsigned v=0;for(unsigned i=0;i<n;++i)v|=unsigned(b[i])<<(i*8);return v; }
inline std::vector<std::uint8_t> Request(unsigned nonce) { std::vector<std::uint8_t>b{0xC1,9,0xF3,Subcode,2};Put(b,nonce,4);return b; }
inline bool IsRequest(const std::uint8_t* p,unsigned size) { return p && size==9 && p[0]==0xC1 && p[1]==size && p[2]==0xF3 && p[3]==Subcode && p[4]==2; }
inline std::vector<std::uint8_t> Encode(unsigned nonce,const Snapshot& s) {
 auto b=Request(nonce);b.push_back(static_cast<std::uint8_t>((s.enabled?1:0)|(s.pending?2:0)));b.push_back(static_cast<std::uint8_t>(s.pieces));
 for(unsigned n:s.ranks)b.push_back(static_cast<std::uint8_t>(n));
 for(unsigned v:s.values)Put(b,v,4);for(unsigned v:s.exclusive)Put(b,v,4);Put(b,s.physicalPercent,4);Put(b,s.magicPercent,4);b[1]=static_cast<std::uint8_t>(b.size());return b;
}
inline bool Decode(const std::uint8_t* p,unsigned size,unsigned nonce,Snapshot& out) {
 if(!p || size!=38+Count*4 || p[0]!=0xC1 || p[1]!=size || p[2]!=0xF3 || p[3]!=Subcode || p[4]!=2 || Read(p+5,4)!=nonce || (p[9]&~3) || p[10]>18)return false;
 Snapshot s;s.enabled=(p[9]&1)!=0;s.pending=(p[9]&2)!=0;s.pieces=p[10];
 unsigned ranks=0;for(unsigned i=0;i<3;++i){s.ranks[i]=p[11+i];ranks+=s.ranks[i];}if(ranks>108)return false;
 for(unsigned i=0;i<Count;++i){s.values[i]=Read(p+14+i*4,4);if(s.values[i]>1000000)return false;}
 if(!s.enabled && (s.pending || s.pieces || ranks))return false;
 if(!s.enabled)for(unsigned v:s.values)if(v)return false;
 for(unsigned i=0;i<4;++i){s.exclusive[i]=Read(p+14+Count*4+i*4,4);if(s.exclusive[i]>1000)return false;}s.physicalPercent=Read(p+30+Count*4,4);s.magicPercent=Read(p+34+Count*4,4);if(s.physicalPercent>1000||s.magicPercent>1000)return false;
 if(!s.enabled){for(unsigned v:s.exclusive)if(v)return false;if(s.physicalPercent||s.magicPercent)return false;}
 out=s;return true;
}
}
