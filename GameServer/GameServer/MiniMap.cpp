// MiniMap.cpp: implementation of the CMiniMap class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "MiniMap.h"
#include "GameMaster.h"
#include "Map.h"
#include "MemScript.h"
#include "Party.h"
#include "Util.h"
#include "MonsterSetBase.h"
#include "MonsterManager.h"
#include "Gate.h"

CMiniMap gMiniMap;
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CMiniMap::CMiniMap() // OK
{
	#if(GAMESERVER_UPDATE>=802)

	this->m_MiniMapInfo.clear();

	#endif
}

CMiniMap::~CMiniMap() // OK
{

}

void CMiniMap::Load(char* path) // OK
{
	#if(GAMESERVER_UPDATE>=802)

	CMemScript* lpMemScript = new CMemScript;

	if(lpMemScript == 0)
	{
		ErrorMessageBox(MEM_SCRIPT_ALLOC_ERROR,path);
		return;
	}

	if(lpMemScript->SetBuffer(path) == 0)
	{
		ErrorMessageBox(lpMemScript->GetLastError());
		delete lpMemScript;
		return;
	}

	this->m_MiniMapInfo.clear();

	try
	{
		while(true)
		{
			if(lpMemScript->GetToken() == TOKEN_END)
			{
				break;
			}

			if(strcmp("end",lpMemScript->GetString()) == 0)
			{
				break;
			}

			MINI_MAP_INFO info;

			memset(&info,0,sizeof(info));

			info.Index = lpMemScript->GetNumber();

			info.Group = lpMemScript->GetAsNumber();

			info.Type = lpMemScript->GetAsNumber();

			info.X = lpMemScript->GetAsNumber();

			info.Y = lpMemScript->GetAsNumber();

			strcpy_s(info.Text,lpMemScript->GetAsString());

			this->m_MiniMapInfo.push_back(info);
		}
	}
	catch(...)
	{
		ErrorMessageBox(lpMemScript->GetLastError());
	}

	delete lpMemScript;

	#endif
}

void CMiniMap::CGMiniMapStartPartyInfoRecv(int aIndex) // OK
{
	#if(GAMESERVER_UPDATE>=802)

	LPOBJ lpObj = &gObj[aIndex];

	if(gObjIsConnectedGP(aIndex) == 0)
	{
		return;
	}

	lpObj->MiniMapState = 1;

	if(lpObj->MiniMapValue != lpObj->Map)
	{
		this->GCMiniMapInfoSend(aIndex);

		lpObj->MiniMapValue = lpObj->Map;
	}

	#endif
}

void CMiniMap::CGMiniMapClosePartyInfoRecv(int aIndex) // OK
{
	#if(GAMESERVER_UPDATE>=802)

	LPOBJ lpObj = &gObj[aIndex];

	if(gObjIsConnectedGP(aIndex) == 0)
	{
		return;
	}

	lpObj->MiniMapState = 0;

	#endif
}

void CMiniMap::CGMiniMapInfoRecv(PMSG_MINI_MAP_INFO_RECV* lpMsg,int aIndex) // OK
{
	#if(GAMESERVER_UPDATE>=802)

	LPOBJ lpObj = &gObj[aIndex];

	if(gObjIsConnectedGP(aIndex) == 0)
	{
		return;
	}

	#endif
}

void CMiniMap::GCMiniMapPartyInfoSend(int aIndex) // OK
{
	#if(GAMESERVER_UPDATE>=802)

	if(gObj[aIndex].MiniMapState == 0)
	{
		return;
	}

	int index = gObj[aIndex].PartyNumber;

	if(index == -1)
	{
		return;
	}

	BYTE send[256];

	PMSG_MINI_MAP_PARTY_INFO_SEND pMsg;

	pMsg.header.set(0xE7,0x01,0);

	int size = sizeof(pMsg);

	pMsg.count = 0;

	PMSG_MINI_MAP_PARTY_INFO info;

	for(int n=0;n < MAX_PARTY_USER;n++)
	{
		if(OBJECT_RANGE(gParty.m_PartyInfo[index].Index[n]) == 0)
		{
			continue;
		}

		LPOBJ lpObj = &gObj[gParty.m_PartyInfo[index].Index[n]];

		memcpy(info.name,lpObj->Name,sizeof(info.name));

		info.map = lpObj->Map;

		info.x = (BYTE)lpObj->X;

		info.y = (BYTE)lpObj->Y;

		memcpy(&send[size],&info,sizeof(info));
		size += sizeof(info);

		pMsg.count++;
	}

	pMsg.header.size = size;

	memcpy(send,&pMsg,sizeof(pMsg));

	DataSend(aIndex,send,size);

	#endif
}

void CMiniMap::GCMiniMapInfoSend(int aIndex) // OK
{
	#if(GAMESERVER_UPDATE>=802)

	int count = 0;

	for(std::vector<MINI_MAP_INFO>::iterator it=this->m_MiniMapInfo.begin();it != this->m_MiniMapInfo.end();it++)
	{
		if(it->Index != gObj[aIndex].Map)
		{
			continue;
		}

		PMSG_MINI_MAP_INFO_SEND pMsg;

		pMsg.header.set(0xE7,0x03,sizeof(pMsg));

		pMsg.index = count++;

		pMsg.group = it->Group;

		pMsg.type = it->Type;

		pMsg.flag = 0;

		pMsg.x = it->X;

		pMsg.y = it->Y;

		memcpy(pMsg.text,it->Text,sizeof(pMsg.text));

		DataSend(aIndex,(BYTE*)&pMsg,pMsg.header.size);
	}

	#endif
}

namespace {
const char* ClassicPortalMapName(int map){
 switch(map){case 0:return "Lorencia";case 1:return "Dungeon";case 2:return "Devias";case 3:return "Noria";case 4:return "Lost Tower";case 6:return "Arena";case 7:return "Atlans";case 8:return "Tarkan";case 10:return "Icarus";default:return NULL;}
}
}

void CMiniMap::GCDynamicMapInfoSend(int aIndex)
{
	if (gObjIsConnectedGP(aIndex) == 0)
	{
		return;
	}

	std::vector<PMSG_DYNAMIC_MINIMAP_ENTRY> entries;
    // Only actual live NPCs. Spawn section numbers alone do not prove NPC type.
    // Keep a bounded nearest-four selection; no monster/player markers in this NPC list.
    for(int n=OBJECT_START_MONSTER;n<MAX_OBJECT_MONSTER;++n)
    {
        const LPOBJ npc=&gObj[n];
        if(npc->Type!=OBJECT_NPC || npc->Connected==OBJECT_OFFLINE || !npc->Live || npc->Map!=gObj[aIndex].Map)continue;
        bool duplicate=false;
        for(size_t j=0;j<entries.size();++j)if(entries[j].x==npc->X && entries[j].y==npc->Y){duplicate=true;break;}
        if(duplicate)continue;
        const int dx=npc->X-gObj[aIndex].X,dy=npc->Y-gObj[aIndex].Y,distance=dx*dx+dy*dy;
        size_t at=0;
        for(;at<entries.size();++at){const int x=entries[at].x-gObj[aIndex].X,y=entries[at].y-gObj[aIndex].Y;if(distance<x*x+y*y)break;}
        if(at>=4)continue;
        PMSG_DYNAMIC_MINIMAP_ENTRY info={};info.kind=1;info.x=(BYTE)npc->X;info.y=(BYTE)npc->Y;info.tx=info.x;info.ty=info.y;info.id=(WORD)npc->Class;
        strncpy_s(info.text,gMonsterManager.GetMonsterName(npc->Class),_TRUNCATE);
        entries.insert(entries.begin()+at,info);if(entries.size()>4)entries.resize(4);
    }

    // Navigation portals use a separate budget; NPCs remain nearest-four.
    if(ClassicPortalMapName(gObj[aIndex].Map)){
        const auto gates=gGate.GetMapGates(gObj[aIndex].Map);
        unsigned portals=0;
        for(const auto& gate:gates){
            GATE_INFO destination;
            if(gate.Flag!=1 || !gGate.GetInfo(gate.TargetGate,&destination) || destination.Map==gate.Map)continue;
            const char* name=ClassicPortalMapName(destination.Map);
            if(!name || portals>=64)continue;
            PMSG_DYNAMIC_MINIMAP_ENTRY info={};
            info.kind=2;info.x=static_cast<BYTE>((gate.X+gate.TX)/2);info.y=static_cast<BYTE>((gate.Y+gate.TY)/2);
            info.tx=info.x;info.ty=info.y;info.id=static_cast<WORD>(gate.Index);
            _snprintf_s(info.text,sizeof(info.text),_TRUNCATE,"Portal > %s",name);
            entries.push_back(info);++portals;
        }
    }

	// Send every marker in legacy-safe chunks. A single packet with all map
	// records can exceed the client's 2 KB stream buffer and disappear entirely.
	const size_t chunkCapacity = 40;
	for (size_t offset = 0; offset < entries.size() || offset == 0; offset += chunkCapacity)
	{
		const size_t chunkCount = min(chunkCapacity, entries.size() - offset);
		const int size = sizeof(PMSG_DYNAMIC_MINIMAP_HEAD) + (int)(chunkCount * sizeof(PMSG_DYNAMIC_MINIMAP_ENTRY));
		BYTE* send = new BYTE[size];
		PMSG_DYNAMIC_MINIMAP_HEAD* head = (PMSG_DYNAMIC_MINIMAP_HEAD*)send;
		head->header.set(0xFA, 0x02, size);
		head->map = (BYTE)gObj[aIndex].Map;
		head->reset = (offset == 0) ? 1 : 0;
		head->count = (WORD)chunkCount;
		if (chunkCount != 0)
		{
			memcpy(send + sizeof(PMSG_DYNAMIC_MINIMAP_HEAD), &entries[offset], chunkCount * sizeof(PMSG_DYNAMIC_MINIMAP_ENTRY));
		}
		DataSend(aIndex, send, size);
		delete[] send;
	}
}

void CMiniMap::CGGMTeleportRecv(PMSG_MINIMAP_GM_TELEPORT_RECV* lpMsg, int aIndex)
{
	if (lpMsg == 0 || gObjIsConnectedGP(aIndex) == 0)
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];
	// Some administrators receive their authority directly from the account
	// CtlCode and are not duplicated in GameMaster.txt. Accept both official
	// authority paths, while still rejecting every normal player server-side.
	const bool isAdministrator = (gGameMaster.CheckGameMasterLevel(lpObj, 1) != 0) || ((lpObj->Authority & 32) == 32);
	if (!isAdministrator || lpMsg->map != (BYTE)lpObj->Map)
	{
		return;
	}

	int targetX = lpMsg->x;
	int targetY = lpMsg->y;
	if (targetX == 0 || targetY == 0 || gMap[lpObj->Map].CheckAttr(targetX, targetY, 4) != 0 || gMap[lpObj->Map].CheckAttr(targetX, targetY, 8) != 0)
	{
		bool found = false;
		for (int radius = 1; radius <= 12 && !found; ++radius)
		{
			for (int oy = -radius; oy <= radius && !found; ++oy)
			{
				for (int ox = -radius; ox <= radius; ++ox)
				{
					if (abs(ox) != radius && abs(oy) != radius) continue;
					const int testX = targetX + ox;
					const int testY = targetY + oy;
					if (testX <= 0 || testX >= 256 || testY <= 0 || testY >= 256) continue;
					if (gMap[lpObj->Map].CheckAttr(testX, testY, 4) == 0 && gMap[lpObj->Map].CheckAttr(testX, testY, 8) == 0)
					{
						targetX = testX;
						targetY = testY;
						found = true;
						break;
					}
				}
			}
		}
		if (!found) return;
	}

	gObjTeleport(aIndex, lpObj->Map, targetX, targetY);
	LogAdd(LOG_BLACK, "[MiniMapTeleport][%s][%s] Map:%d X:%d Y:%d", lpObj->Account, lpObj->Name, lpObj->Map, targetX, targetY);
}
