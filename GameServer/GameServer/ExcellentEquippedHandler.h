#pragma once
#include "ExcRankRuntime.h"
#include "CharacterAdvance.h"
#include "../../Utilities/ExcellentEquippedProtocol.h"
namespace ExcellentEquippedHandler {
inline ExcellentEquipped::Snapshot Capture(LPOBJ object) {
 ExcellentEquipped::Snapshot out;const auto cfg=ExcRank::Policy();out.enabled=IndependentTierControl::Enabled()&&cfg->enabled;if(!out.enabled)return out;
 const auto* state=ExcRankRuntime::Get(object);if(!state){out.pending=true;return out;}const auto& totals=state->totals;out.pending=state->pending;out.physicalPercent=state->physical;out.magicPercent=state->magical;for(unsigned i=0;i<4;++i)out.exclusive[i]=state->appliedExclusive[i];
 out.pieces=totals.pieces;for(unsigned n=0;n<3;++n)out.ranks[n]=totals.ranks[n];
 const unsigned kinds[]={0,92,203,207,97,206,204,91,202,89,88,205,86};
 for(unsigned id=1;id<=12;++id)for(unsigned k=0;k<ExcellentEquipped::Count;++k)if(ExcellentEquipped::Kinds[k]==kinds[id])out.values[k]=state->displayed[id];
 return out;
}
inline void Receive(int user,const BYTE* bytes,int size) {
 if(size<0 || !OBJECT_USER_RANGE(user) || !gObjIsConnectedGP(user) || !ExcellentEquipped::IsRequest(bytes,static_cast<unsigned>(size)))return;
 static DWORD last[MAX_OBJECT_USER]={};auto& tick=last[user-OBJECT_START_USER];const DWORD now=GetTickCount();if(tick && now-tick<500)return;tick=now;
 CharacterAdvance->UpdateInformation(&gObj[user]);
 auto packet=ExcellentEquipped::Encode(ExcellentEquipped::Read(bytes+5,4),Capture(&gObj[user]));DataSend(user,packet.data(),static_cast<int>(packet.size()));
}
}
