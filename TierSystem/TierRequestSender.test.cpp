#include <windows.h>
#include <cassert>
#include <cstring>
#include <vector>
#include <cstdio>
static std::vector<unsigned char> captured;
int SendPacket(char* p,int n,BOOL,BOOL) {captured.assign(p,p+n);return n;}
#include "../Main5.2/source/IndependentTierRequestSender.h"
int main(){
 using namespace IndependentTierViewProtocol;
 const unsigned char filter[]={0xAB,0x11,0xCD,0xFE,0x18,0x23,0xC5,0xA3,0xCA,0x33,0xC1,0xCC,0x66,0x67,0x21,0xF3,0x32,0x12,0x15,0x35,0x29,0xFF,0xFE,0x1D,0x44,0xEF,0xCD,0x41,0x26,0x3C,0x4E,0x4D};
 for(unsigned slot: {12u,76u,204u,236u,256u,512u,544u,768u,1024u,1056u}){
  Message q;q.token=0xfedcba98;q.slot=static_cast<std::uint16_t>(slot);q.itemId=4096;
  std::vector<std::uint8_t> plain;assert(Encode(q,plain));assert(IndependentTierRequestSender::Send(q));
  assert(captured!=plain);Message decoded;assert(!Decode(captured.data(),captured.size(),decoded));
  for(int n=static_cast<int>(captured.size())-1;n>2;--n)captured[n]^=captured[n-1]^filter[n%32];
  assert(captured==plain);assert(Decode(captured.data(),captured.size(),decoded));assert(decoded.slot==q.slot && decoded.token==q.token && decoded.itemId==q.itemId);
 }
 puts("PASS: actual client StreamPacketEngine wire encoding and server XOR reversal for all supported tier query contexts.");
}
