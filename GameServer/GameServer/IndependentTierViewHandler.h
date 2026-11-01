#pragma once
#include "User.h"
#include "ItemOption.h"
#include "ShopManager.h"
#include "Util.h"
#include "Protocol.h"
#include "ExcRankRuntime.h"
#include "IndependentTierBonuses.h"
#include "../../Utilities/IndependentTierViewProtocol.h"
namespace IndependentTierViewHandler {
inline void Receive(int user,const BYTE* bytes,int size) {
 using namespace IndependentTierViewProtocol;
 if(!OBJECT_USER_RANGE(user)||!gObjIsConnectedGP(user)||size<0)return;
 Message request;if(!Decode(bytes,static_cast<std::size_t>(size),request)||request.reply)return;
 static DWORD last[MAX_OBJECT]={};DWORD now=GetTickCount();if(last[user]&&now-last[user]<100)return;last[user]=now;
 Message reply=request;reply.reply=true;reply.status=Status::Unavailable;const auto cfg=ExcRank::Policy();
 if(IndependentTierControl::Enabled()&&cfg->enabled) {
 LPOBJ owner=&gObj[user];CItem shopItem;CItem* selected=nullptr;
 const unsigned address=request.slot;
 if(address<243 && address<INVENTORY_SIZE)selected=&owner->Inventory[address];
 else if(address>=WarehouseBase && address<WarehouseBase+WAREHOUSE_SIZE) {
  if(owner->Interface.use && owner->Interface.type==INTERFACE_WAREHOUSE && owner->Interface.state && owner->LoadWarehouse && owner->Warehouse)
   selected=&owner->Warehouse[address-WarehouseBase];
 } else if(address>=TradeOwnBase && address<TradeOtherBase+TRADE_SIZE) {
  const int partner=owner->TargetNumber;
  if(owner->Interface.use && owner->Interface.type==INTERFACE_TRADE && owner->Interface.state &&
     OBJECT_USER_RANGE(partner) && gObjIsConnectedGP(partner) && gObj[partner].TargetNumber==user &&
     gObj[partner].Interface.use && gObj[partner].Interface.type==INTERFACE_TRADE && gObj[partner].Interface.state) {
   CItem* trade=address<TradeOtherBase?owner->Trade:gObj[partner].Trade;
   if(trade)selected=&trade[address-(address<TradeOtherBase?TradeOwnBase:TradeOtherBase)];
  }
 } else if(address>=ShopBase && address<ShopBase+SHOP_SIZE) {
  if(owner->Interface.use && owner->Interface.type==INTERFACE_SHOP && SHOP_RANGE(owner->TargetShopNumber) &&
     gShopManager.GetItemByIndex(owner->TargetShopNumber,&shopItem,address-ShopBase))selected=&shopItem;
 }
 if(address>=PersonalOwnBase && address<PersonalOwnBase+32) {
  const unsigned index=INVENTORY_EXT4_SIZE+address-PersonalOwnBase;
  if(index<INVENTORY_FULL_SIZE && index<INVENTORY_SIZE)selected=&owner->Inventory[index];
 } else if(address>=PersonalOtherBase && address<PersonalOtherBase+32) {
  const int dealer=owner->PShopDealerIndex;
  if(owner->PShopWantDeal && OBJECT_USER_RANGE(dealer) && gObjIsConnectedGP(dealer) &&
     gObj[dealer].PShopOpen && memcmp(owner->PShopDealerName,gObj[dealer].Name,sizeof(owner->PShopDealerName))==0) {
   const unsigned index=INVENTORY_EXT4_SIZE+address-PersonalOtherBase;
   if(index<INVENTORY_FULL_SIZE && index<INVENTORY_SIZE)selected=&gObj[dealer].Inventory[index];
  }
 }

 if(selected&&selected->IsItem()&&selected->m_Index==request.itemId) {
  auto& item=*selected;
  if((address>=ShopBase&&address<ShopBase+SHOP_SIZE)||!item.m_Serial)reply.status=Status::Legacy;
  else {
   reply.serial=item.m_Serial;ExcRank::ItemOptions stored;
   auto state=ExcRankCache::Get().Lookup(item.m_Serial,item.m_Index,stored);
   if(state==ExcRankCache::State::Ready) {
    if(item.m_NewOption&63)reply.ranked.attributes=stored.attributes;
    for(auto option:stored.slots)if(cfg->Allows(item.m_Index,option.option)&&cfg->Value(option)){
     unsigned n=static_cast<unsigned>(reply.ranked.slots.size());reply.ranked.slots.push_back(option);reply.rankedValues[n]=static_cast<std::uint16_t>(cfg->Value(option));
    }
    reply.status=reply.ranked.slots.empty()&&!reply.ranked.attributes[0].id?Status::Legacy:Status::Found;
   } else if(state==ExcRankCache::State::Legacy)reply.status=Status::Legacy;
   else {reply.status=Status::Pending;ExcRankCache::Get().Request(item.m_Serial,item.m_Index);}
  }
  IndependentItemTiers::Metadata native;
  auto nativeState=IndependentTierCache::Get().Lookup(item.m_Serial,item.m_Index,native);
  if(nativeState==IndependentTierCache::State::Ready && !native.options.empty()) {
   reply.metadata=native;reply.ranked.slots.clear();
   reply.status=reply.ranked.attributes[0].id?Status::Found:Status::Legacy;
   const auto& settings=IndependentTierGameplay::GetSettings();
   for(int n=3;n<=8;++n)if(item.m_NewOption&(1u<<(n-3))){
    auto resolved=settings.excellent.Resolve(native,item.m_NewOption,n,item.m_SpecialIndex[n],item.m_SpecialValue[n],true,settings.life);
    reply.optionTypes[n-3]=static_cast<std::uint8_t>(resolved.kind);reply.optionValues[n-3]=static_cast<std::uint16_t>(resolved.value);
   }
  } else if(nativeState!=IndependentTierCache::State::Legacy)IndependentTierCache::Get().Request(item.m_Serial,item.m_Index);
  reply.gradeEnabled=ExcRankRuntime::GradeEnabled();const auto score=ExcRank::EvaluateScore(*cfg,item.m_Index,item.m_Level,item.m_Option3,item.m_NewOption,item.m_SetOption>0,reply.ranked,reply.gradeEnabled);reply.scoreValid=score.valid;reply.baseScore=score.base;reply.gradeScore=score.grade; if(reply.gradeEnabled)for(auto option:reply.metadata.options){const unsigned grade=static_cast<unsigned>(option.grade);if(grade>=2 && grade<=4)reply.gradeScore+=(grade==2?20:grade==3?35:50);}
 }
 }
 std::vector<std::uint8_t> packet;if(Encode(reply,packet))DataSend(user,packet.data(),static_cast<int>(packet.size()));
}
}
