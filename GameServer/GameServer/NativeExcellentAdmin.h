#pragma once
#include "IndependentTierCache.h"
#include "Notice.h"
#include <vector>
#include "../../Utilities/NativeExcellentSelection.h"
#include "../../Utilities/ExcRank/ExcRankRandom.h"
#include "ExcRankCache.h"
namespace NativeExcellentAdmin {
struct Pending {unsigned token,serial,item;int user;std::uintptr_t socket;char account[11];unsigned stage=0;DWORD stamp=0;};
inline std::vector<Pending>& Requests(){static std::vector<Pending> list;return list;}
inline bool Assign(LPOBJ object,unsigned slot,const std::vector<unsigned>& selection){
 if(!object || slot>=INVENTORY_SIZE || Requests().size()>=64)return false;
 auto& item=object->Inventory[slot];
 if(!item.IsItem() || !item.m_Serial || !(item.m_NewOption&63))return false;
 if(item.m_Index>=12*512){const auto cfg=ExcRank::Policy();if(!cfg)return false;const auto type=cfg->Class(item.m_Index);if(type!=ExcRank::ItemClass::Ring && type!=ExcRank::ItemClass::Pendant)return false;}
 IndependentTierProtocol::Message message;message.operation=IndependentTierProtocol::Operation::Create;
 IndependentTierProtocol::Entry entry;entry.serial=item.m_Serial;entry.itemId=item.m_Index;
 if(!NativeExcellentSelection::Build(item.m_NewOption&63,selection,entry.metadata))return false;
 message.entries.push_back(entry);const auto token=IndependentTierChannel::Get().Send(message);if(!token)return false;
 Pending pending{};pending.token=token;pending.serial=item.m_Serial;pending.item=item.m_Index;pending.user=object->Index;
 pending.socket=static_cast<std::uintptr_t>(object->Socket);pending.stamp=GetTickCount();memcpy(pending.account,object->Account,11);Requests().push_back(pending);return true;
}
inline void Pump(){
 auto& list=Requests();for(auto it=list.begin();it!=list.end();){
  bool saved=false;
  const bool timeout=GetTickCount()-it->stamp>=40000;
  if(it->stage==0){
   IndependentTierProtocol::Message reply;
   if(!IndependentTierChannel::Get().Take(it->token,reply)){if(!timeout){++it;continue;}}
   else saved=!reply.entries.empty() && (reply.entries[0].status==IndependentTierProtocol::Status::Created || reply.entries[0].status==IndependentTierProtocol::Status::Existing);
   if(saved){
    auto& cache=IndependentTierCache::Get();{std::lock_guard<std::mutex> lock(cache.mutex);cache.records.erase(it->serial);}cache.Request(it->serial,it->item);
    ExcRankProtocol::Message read;read.operation=ExcRankProtocol::Operation::Read;
    ExcRankProtocol::Entry entry;entry.serial=it->serial;read.entries.push_back(entry);
    it->token=ExcRankChannel::Get().Send(read);it->stage=1;it->stamp=GetTickCount();
    if(it->token){++it;continue;}saved=false;
   }
  }else{
   ExcRankProtocol::Message reply;
   if(!ExcRankChannel::Get().Take(it->token,reply)){if(!timeout){++it;continue;}}
   else if(!reply.entries.empty()){
    const auto& entry=reply.entries[0];
    if(it->stage==1 && entry.status==ExcRankProtocol::Status::Missing){
     ExcRank::ItemOptions attributes;ExcRank::SecureRandom rng;
     if(ExcRank::GenerateExclusive(*ExcRank::Policy(),it->item,rng,attributes,true) && attributes.attributes[0].id){
      ExcRankProtocol::Message create;create.operation=ExcRankProtocol::Operation::Create;
      ExcRankProtocol::Entry generated;generated.serial=it->serial;generated.itemId=static_cast<std::uint16_t>(it->item);generated.metadata=attributes;create.entries.push_back(generated);
      it->token=ExcRankChannel::Get().Send(create);it->stage=2;it->stamp=GetTickCount();
      if(it->token){++it;continue;}
     }
    }else if(it->stage==1)saved=entry.status==ExcRankProtocol::Status::Found && entry.itemId==it->item && entry.metadata.attributes[0].id;
    else saved=entry.status==ExcRankProtocol::Status::Created || entry.status==ExcRankProtocol::Status::Existing;
   }
   if(saved){auto& cache=ExcRankCache::Get();{std::lock_guard<std::mutex> lock(cache.mutex);cache.records.erase(it->serial);}cache.Request(it->serial,it->item);}
  }
  if(OBJECT_USER_RANGE(it->user)&&gObjIsConnectedGP(it->user)&&it->socket==static_cast<std::uintptr_t>(gObj[it->user].Socket)&&!memcmp(it->account,gObj[it->user].Account,11))
   gNotice.GCNoticeSend(it->user,1,0,0,0,0,0,saved?"Tier EX: tiers e bonus exclusivos salvos. Reabra o inventario.":"Tier EX: bonus exclusivos nao confirmados; verifique o DataServer.");
  it=list.erase(it);
 }
}
}
