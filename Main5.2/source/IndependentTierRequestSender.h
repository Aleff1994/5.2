#pragma once
#include "../../Utilities/IndependentTierViewProtocol.h"
#include "StreamPacketEngine.h"
namespace IndependentTierRequestSender {
inline bool Send(const IndependentTierViewProtocol::Message& request) {
 if(request.reply)return false;
 std::vector<std::uint8_t> plain;
 if(!IndependentTierViewProtocol::Encode(request,plain))return false;
 // StreamPacketEngine applies the protocol XOR from subcode onward. The
 // socket layer adds its transport protection later; these are distinct steps.
 CStreamPacketEngine packet;packet.Init(0xC1,0xF3);
 packet.AddData(plain.data()+3,static_cast<WORD>(plain.size()-3));packet.Send();return true;
}
}
