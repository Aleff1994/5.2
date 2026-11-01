#pragma once
#include "IndependentItemTiers.h"
namespace IndependentTierProtocol {
using Bytes=std::vector<std::uint8_t>;
const unsigned Head=0xE9, HeaderSize=11, MaxRecords=16;
enum class Operation:std::uint8_t { Read=1,Create=2,ReadResult=129,CreateResult=130 };
enum class Status:std::uint8_t { Missing=0,Found=1,Created=2,Existing=3,Conflict=4,Error=5 };
struct Entry {
 std::uint32_t serial=0;
 std::uint16_t itemId=0;
 Status status=Status::Missing;
 IndependentItemTiers::Metadata metadata;
};
struct Message { Operation operation=Operation::Read; std::uint32_t requestId=0; std::vector<Entry> entries; };
inline void U32(Bytes& b,std::uint32_t v) { for(unsigned n=0;n<4;++n) b.push_back(static_cast<std::uint8_t>(v>>(8*n))); }
inline std::uint32_t U32(const std::uint8_t* p) { return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|(std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24); }
inline bool Response(Operation op) { return op==Operation::ReadResult || op==Operation::CreateResult; }
inline bool HasMetadata(Operation op,Status s) { return op==Operation::Create || (op==Operation::ReadResult && s==Status::Found); }
inline bool Allowed(Operation op,Status s) {
 if(op==Operation::Read || op==Operation::Create) return s==Status::Missing;
 if(op==Operation::ReadResult) return s==Status::Missing || s==Status::Found || s==Status::Error;
 return op==Operation::CreateResult && (s==Status::Created || s==Status::Existing || s==Status::Conflict || s==Status::Error);
}
inline bool Encode(const Message& m,Bytes& output) {
 if(!m.requestId || m.entries.empty() || m.entries.size()>MaxRecords ||
   (m.operation!=Operation::Read && m.operation!=Operation::Create && !Response(m.operation))) return false;
 Bytes b={0xC2,0,0,static_cast<std::uint8_t>(Head),1,static_cast<std::uint8_t>(m.operation)};
 U32(b,m.requestId); b.push_back(static_cast<std::uint8_t>(m.entries.size()));
 for(std::size_t i=0;i<m.entries.size();++i) {
  const auto& e=m.entries[i]; if(!e.serial || !Allowed(m.operation,e.status)) return false;
  for(std::size_t j=0;j<i;++j) if(m.entries[j].serial==e.serial) return false;
  U32(b,e.serial);
  if(Response(m.operation)) b.push_back(static_cast<std::uint8_t>(e.status));
  if(HasMetadata(m.operation,e.status)) {
   if(e.itemId>8191) return false;
   Bytes payload;if(!IndependentItemTiers::Encode(e.metadata,payload)) return false;
   b.push_back(static_cast<std::uint8_t>(e.itemId));b.push_back(static_cast<std::uint8_t>(e.itemId>>8));
   b.push_back(static_cast<std::uint8_t>(payload.size()));b.insert(b.end(),payload.begin(),payload.end());
  }
 }
 b[1]=static_cast<std::uint8_t>(b.size()>>8);b[2]=static_cast<std::uint8_t>(b.size());output.swap(b);return true;
}
inline bool Decode(const std::uint8_t* bytes,std::size_t size,Message& output) {
 if(!bytes || size<HeaderSize || size>HeaderSize+MaxRecords*139 || bytes[0]!=0xC2 ||
    size!=((unsigned(bytes[1])<<8)|bytes[2]) || bytes[3]!=Head || bytes[4]!=1 || bytes[10]==0 || bytes[10]>MaxRecords) return false;
 Message m;m.operation=static_cast<Operation>(bytes[5]);m.requestId=U32(bytes+6);
 if(!m.requestId || (m.operation!=Operation::Read && m.operation!=Operation::Create && !Response(m.operation))) return false;
 std::size_t pos=HeaderSize;
 for(unsigned n=0;n<bytes[10];++n) {
  if(pos+4>size) return false;Entry e;e.serial=U32(bytes+pos);pos+=4;
  if(Response(m.operation)) { if(pos>=size) return false;e.status=static_cast<Status>(bytes[pos++]); }
  if(!Allowed(m.operation,e.status) || !e.serial) return false;
  for(const auto& prior:m.entries) if(prior.serial==e.serial) return false;
  if(HasMetadata(m.operation,e.status)) {
   if(pos+3>size) return false;e.itemId=bytes[pos]|(unsigned(bytes[pos+1])<<8);const unsigned len=bytes[pos+2];pos+=3;
   if(e.itemId>8191 || pos+len>size || !IndependentItemTiers::Decode(bytes+pos,len,e.metadata)) return false;
   pos+=len;
  }
  m.entries.push_back(e);
 }
 if(pos!=size) return false;output=m;return true;
}
}
