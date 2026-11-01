#pragma once
#include "../../Utilities/IndependentTierProtocol.h"
#include "GameMain.h"
#include "../../Utilities/IndependentTierControl.h"
#include <map>
namespace IndependentTierChannel {
using namespace IndependentTierProtocol;
struct Pending { Message request,response; DWORD started=0;bool ready=false; };
struct Channel {
 std::uint32_t next=0;std::map<std::uint32_t,Pending> pending;
 bool Enabled() const { return IndependentTierControl::Enabled(); }
 void Expire() { const DWORD now=GetTickCount();for(auto i=pending.begin();i!=pending.end();) if(now-i->second.started>=30000) i=pending.erase(i);else ++i; }
 // Returns zero if disabled/disconnected/invalid/full. Caller must not publish a new
 // ranked item until its CreateResult is Created or Existing. Error is NOT Missing.
 std::uint32_t Send(Message request) {
  Expire();if(!Enabled() || !gDataServerConnection.CheckState() || pending.size()>=256 || Response(request.operation)) return 0;
  do { ++next; } while(!next || pending.count(next));
  request.requestId=next;Bytes bytes;if(!Encode(request,bytes)) return 0;
  Pending state;state.request=request;state.started=GetTickCount();pending.emplace(next,state);
  if(!gDataServerConnection.DataSend(bytes.data(),static_cast<int>(bytes.size()))) { pending.erase(next);return 0; }
  return next;
 }
 void Receive(const BYTE* bytes,int size) {
  Expire();if(!Enabled() || size<0) return;
  Message reply;if(!Decode(bytes,static_cast<std::size_t>(size),reply) || !Response(reply.operation)) return;
  auto it=pending.find(reply.requestId);if(it==pending.end() || it->second.ready) return;
  const auto& expected=it->second.request;
  if(reply.entries.size()!=expected.entries.size() ||
     reply.operation!=(expected.operation==Operation::Read?Operation::ReadResult:Operation::CreateResult)) return;
  for(std::size_t n=0;n<reply.entries.size();++n) if(reply.entries[n].serial!=expected.entries[n].serial) return;
  it->second.response=reply;it->second.ready=true;
 }
 bool Take(std::uint32_t requestId,Message& result) {
  Expire();auto i=pending.find(requestId);if(i==pending.end() || !i->second.ready) return false;
  result=i->second.response;pending.erase(i);return true;
 }
 void Disconnected() { pending.clear(); }
};
inline Channel& Get() { static Channel channel;return channel; }
}
