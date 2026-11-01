#pragma once
#include <cstdint>

namespace ItemOptionWire {
constexpr unsigned SocketSlots=5;
// GS decides the meaning of the shared Harmony/socket-bonus byte per INSTANCE.
// A model listed as socket-capable does not imply that this instance has slots.
inline bool IsSocketInstance(int type,unsigned count) noexcept {
    return type>=0 && type<12*512 && count>0 && count<=SocketSlots;
}
template<class Item>
void Decode(Item& item,std::uint8_t sharedOption,const std::uint8_t* sockets,unsigned seedOptionCount) noexcept {
    item.SocketCount=0;
    item.SocketSeedSetOption=0xFF;
    item.Jewel_Of_Harmony_Option=(sharedOption>>4)&15;
    item.Jewel_Of_Harmony_OptionLevel=sharedOption&15;
    for(unsigned slot=0;slot<SocketSlots;++slot) {
        const auto raw=sockets?sockets[slot]:std::uint8_t{0xFF};
        item.bySocketOption[slot]=raw;
        item.SocketSeedID[slot]=0xFF;
        item.SocketSphereLv[slot]=0;
        if(raw==0xFF)continue;
        // Keep later occupied slots even if an older item has a gap.
        item.SocketCount=static_cast<std::uint8_t>(slot+1);
        if(raw!=0xFE && seedOptionCount) {
            item.SocketSeedID[slot]=static_cast<std::uint8_t>(raw%seedOptionCount);
            item.SocketSphereLv[slot]=static_cast<std::uint8_t>(raw/seedOptionCount+1);
        }
    }
    if(IsSocketInstance(item.Type,item.SocketCount)) {
        item.SocketSeedSetOption=sharedOption;
        item.Jewel_Of_Harmony_Option=0;
        item.Jewel_Of_Harmony_OptionLevel=0;
    }
}
}
