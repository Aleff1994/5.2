#pragma once
#include <cstdint>
#include <cstddef>
namespace FullSetDamage {
constexpr unsigned Marker=0x200; // Internal only; 0x100 is native triple damage.
constexpr unsigned Tag=0xD7;
template<class Object> bool Matches(Object& object) {
 int family=-1;
 for(int n=0;n<5;++n) {
  auto& item=object.Inventory[n+2];
  if(!item.IsItem() || !item.m_IsValidItem || !(item.m_Durability>0) || item.m_Index/512!=7+n)return false;
  const int index=item.m_Index%512;
  if(n && index!=family)return false;
  family=index;
 }
 return true;
}
inline int Scale(int damage,unsigned percent) {
 if(damage<=0 || percent>100)return damage;
 const std::int64_t scaled=static_cast<std::int64_t>(damage)*(100+percent)/100;
 return scaled>2147483647?2147483647:static_cast<int>(scaled);
}
inline bool Read(const std::uint8_t* packet,std::size_t length,std::size_t offset) {
 return packet && offset<=length && length-offset>=2 && packet[offset]==Tag && packet[offset+1]>0 && packet[offset+1]<=100;
}
inline void Tint(unsigned type,float* color) {
 switch(type&15) {
 case 0:color[0]=1.f;color[1]=.86f;color[2]=.22f;break;
 case 2:color[0]=.65f;color[1]=1.f;color[2]=.15f;break;
 case 3:color[0]=.70f;color[1]=.55f;color[2]=1.f;break;
 default:break;
 }
}
}
