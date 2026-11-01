#include "stdafx.h"
#include "FakeBot.h"
#include "Attack.h"
#include "CustomAttack.h"
#include "DSProtocol.h"
#include "ItemManager.h"
#include "JSProtocol.h"
#include "Map.h"
#include "Monster.h"
#include "PvPTournament.h"
#include "SkillManager.h"
#include "Util.h"
#include "User.h"

CFakeBot gFakeBot;

CFakeBot::CFakeBot()
{
	this->Clear();
}

void CFakeBot::Clear()
{
	this->m_Enabled = 0;
	this->m_AutoConnect = 0;
	this->m_LoginDelay = 15000;
	this->m_RetryDelay = 60000;
	this->m_Count = 0;

	for(int n=0;n<MAX_FAKE_BOT;n++)
	{
		memset(&this->m_Bot[n],0,sizeof(FAKE_BOT_INFO));
		this->m_Bot[n].State = FAKE_BOT_DISABLED;
		this->m_Bot[n].Index = -1;
	}
}

void CFakeBot::Load(const char* path)
{
	this->DisconnectAll("config reload");
	this->Clear();

#if(GAMESERVER_TYPE != 0)
	// FakeBoot represents real accounts and must be owned by one GameServer
	// only.  Loading it on GameServerCS duplicates the same logins, produces
	// result 3 and creates virtual connection/checksum noise.
	LogAdd(LOG_GREEN,"[FakeBoot] Disabled on GameServerCS (owned by normal GameServer)");
	return;
#endif

	FILE* file = 0;

	if(fopen_s(&file,path,"r") != 0 || file == 0)
	{
		LogAdd(LOG_RED,"[FakeBoot] File not found: %s",path);
		return;
	}

	int section = -1;
	char line[256];

	while(fgets(line,sizeof(line),file) != 0)
	{
		char* data = line;

		while(*data == ' ' || *data == '\t')
		{
			data++;
		}

		char* end = data + strlen(data);
		while(end > data && (end[-1] == '\r' || end[-1] == '\n' || end[-1] == ' ' || end[-1] == '\t'))
		{
			*(--end) = 0;
		}

		if(data[0] == 0 || data[0] == ';' || data[0] == '#'
			|| (data[0] == '/' && data[1] == '/'))
		{
			continue;
		}

		if(_stricmp(data,"end") == 0)
		{
			section = -1;
			continue;
		}

		if(section == -1)
		{
			section = atoi(data);
			continue;
		}

		if(section == 0)
		{
			if(sscanf_s(data,"%d %d %d %d",&this->m_Enabled,&this->m_AutoConnect,&this->m_LoginDelay,&this->m_RetryDelay) != 4)
			{
				LogAdd(LOG_RED,"[FakeBoot] Invalid settings line: %s",data);
			}
		}
		else if(section == 1 && this->m_Count < MAX_FAKE_BOT)
		{
			FAKE_BOT_INFO info;
			memset(&info,0,sizeof(info));
			info.Index = -1;
			info.SearchRange = 8;
			info.AttackRange = 3;
			info.AttackDelay = 900;
			info.PotionPercent = 40;
			info.HomeRange = 12;
			info.AutoReset = 0;
			info.AutoPoints = 0;

			int fields = sscanf_s(data,"%d %10s %10s %10s %d %d %d %d %d %d %d %d %d",
				&info.Enabled,
				info.Account,(unsigned int)sizeof(info.Account),
				info.Password,(unsigned int)sizeof(info.Password),
				info.Character,(unsigned int)sizeof(info.Character),
				&info.AdvancedAI,&info.SearchRange,&info.AttackRange,&info.Skill,
				&info.AttackDelay,&info.PotionPercent,&info.HomeRange,
				&info.AutoReset,&info.AutoPoints);

			if(fields < 4)
			{
				LogAdd(LOG_RED,"[FakeBoot] Invalid account line: %s",data);
				continue;
			}

			if(info.Enabled == 0)
			{
				continue;
			}

			info.State = FAKE_BOT_WAITING;
			info.SearchRange = ((info.SearchRange < 2) ? 2 : ((info.SearchRange > 15) ? 15 : info.SearchRange));
			info.AttackRange = ((info.AttackRange < 1) ? 1 : ((info.AttackRange > 8) ? 8 : info.AttackRange));
			info.AttackDelay = ((info.AttackDelay < 250) ? 250 : info.AttackDelay);
			info.PotionPercent = ((info.PotionPercent < 10) ? 10 : ((info.PotionPercent > 90) ? 90 : info.PotionPercent));
			info.HomeRange = ((info.HomeRange < info.SearchRange) ? info.SearchRange : info.HomeRange);
			this->m_Bot[this->m_Count++] = info;
		}
	}

	fclose(file);
	this->m_LoginDelay = ((this->m_LoginDelay < 1000) ? 1000 : this->m_LoginDelay);
	this->m_RetryDelay = ((this->m_RetryDelay < 5000) ? 5000 : this->m_RetryDelay);

	if(this->m_Enabled == 0)
	{
		LogAdd(LOG_GREEN,"[FakeBoot] System disabled");
		return;
	}

	LogAdd(LOG_GREEN,"[FakeBoot] Loaded %d profile(s). AutoConnect=%d",this->m_Count,this->m_AutoConnect);

	if(this->m_AutoConnect != 0)
	{
		this->ConnectAll();
	}
}

bool CFakeBot::IsFakeBot(int aIndex) const
{
	return (OBJECT_RANGE(aIndex) != 0 && gObj[aIndex].IsSmartCharacter != 0);
}

bool CFakeBot::IsManagedIndex(int aIndex) const
{
	return (OBJECT_RANGE(aIndex) != 0 && this->FindByIndex(aIndex) >= 0);
}

int CFakeBot::FindByIndex(int aIndex) const
{
	for(int n=0;n<this->m_Count;n++)
	{
		if(this->m_Bot[n].Index == aIndex)
		{
			return n;
		}
	}

	return -1;
}

void CFakeBot::Connect(int slot)
{
	if(slot < 0 || slot >= this->m_Count)
	{
		return;
	}

	FAKE_BOT_INFO* lpInfo = &this->m_Bot[slot];

	if(lpInfo->State == FAKE_BOT_ONLINE || lpInfo->State == FAKE_BOT_ACCOUNT || lpInfo->State == FAKE_BOT_CHARACTER)
	{
		return;
	}

	int index = gObjAddSearch(INVALID_SOCKET,(char*)"127.0.0.1");

	if(index < 0 || gObjAdd(INVALID_SOCKET,(char*)"127.0.0.1",index) < 0)
	{
		lpInfo->NextActionTick = GetTickCount() + this->m_RetryDelay;
		LogAdd(LOG_RED,"[FakeBoot] No free user slot for %s",lpInfo->Character);
		return;
	}

	lpInfo->Index = index;
	gObj[index].IsSmartCharacter = 1;
	gObj[index].LoginMessageSend = 1;
	gObj[index].LoginMessageCount = 1;
	lpInfo->State = FAKE_BOT_ACCOUNT;
	lpInfo->NextActionTick = GetTickCount() + 30000;

	GJConnectAccountSend(index,lpInfo->Account,lpInfo->Password,gObj[index].IpAddr);
	LogAdd(LOG_GREEN,"[FakeBoot] Account login requested for %s at index %d",lpInfo->Character,index);
}

void CFakeBot::Disconnect(int slot, const char* reason)
{
	if(slot < 0 || slot >= this->m_Count)
	{
		return;
	}

	FAKE_BOT_INFO* lpInfo = &this->m_Bot[slot];
	int index = lpInfo->Index;
	lpInfo->Index = -1;
	lpInfo->State = FAKE_BOT_WAITING;
	lpInfo->NextActionTick = GetTickCount() + this->m_RetryDelay;

	if(OBJECT_RANGE(index) != 0 && gObjIsConnected(index) != 0)
	{
		gObjDel(index);

		// A virtual client has no socket completion callback.  If another
		// subsystem changed the object while the login reply was being handled,
		// make sure the allocator slot cannot remain as a ghost online user.
		if(gObjIsConnected(index) != 0)
		{
			gObj[index].Connected = OBJECT_OFFLINE;
			gObjFreeData(index);
		}
	}

	LogAdd(LOG_RED,"[FakeBoot] Character %s disconnected: %s",lpInfo->Character,reason);
}

int CFakeBot::ConnectAll()
{
	if(this->m_Enabled == 0)
	{
		return -1;
	}

	int queued = 0;
	DWORD tick = GetTickCount();

	for(int n=0;n<this->m_Count;n++)
	{
		FAKE_BOT_INFO* lpInfo = &this->m_Bot[n];

		if(lpInfo->Enabled == 0 || lpInfo->State == FAKE_BOT_ONLINE || lpInfo->State == FAKE_BOT_ACCOUNT || lpInfo->State == FAKE_BOT_CHARACTER)
		{
			continue;
		}

		lpInfo->State = FAKE_BOT_WAITING;
		lpInfo->NextActionTick = tick + ((DWORD)queued * (DWORD)this->m_LoginDelay);
		queued++;
	}

	return queued;
}

int CFakeBot::DisconnectAll(const char* reason)
{
	int count = 0;

	for(int n=0;n<this->m_Count;n++)
	{
		if(this->m_Bot[n].Index >= 0)
		{
			this->Disconnect(n,reason);
			count++;
		}
	}

	return count;
}

int CFakeBot::GetLoadedCount() const
{
	return this->m_Count;
}

int CFakeBot::GetOnlineCount() const
{
	int count = 0;

	for(int n=0;n<this->m_Count;n++)
	{
		if(this->m_Bot[n].State == FAKE_BOT_ONLINE)
		{
			count++;
		}
	}

	return count;
}

void CFakeBot::MainProc()
{
	if(this->m_Enabled == 0)
	{
		return;
	}

	DWORD tick = GetTickCount();

	for(int n=0;n<this->m_Count;n++)
	{
		FAKE_BOT_INFO* lpInfo = &this->m_Bot[n];

		// CharacterInfoSet temporarily clears gObj while the database callback rebuilds it.
		// Keep ownership during an in-flight login; its existing deadline handles failure.
		const bool loginPending = (lpInfo->State == FAKE_BOT_ACCOUNT || lpInfo->State == FAKE_BOT_CHARACTER);
		if(lpInfo->Index >= 0 && (OBJECT_RANGE(lpInfo->Index) == 0 || (!loginPending && gObjIsConnected(lpInfo->Index) == 0)))
		{
			lpInfo->Index = -1;
			lpInfo->State = FAKE_BOT_WAITING;
			lpInfo->NextActionTick = tick + this->m_RetryDelay;
		}

		if(lpInfo->State == FAKE_BOT_WAITING && lpInfo->NextActionTick != 0 && (LONG)(tick - lpInfo->NextActionTick) >= 0)
		{
			this->Connect(n);
			continue;
		}

		if((lpInfo->State == FAKE_BOT_ACCOUNT || lpInfo->State == FAKE_BOT_CHARACTER) && (LONG)(tick - lpInfo->NextActionTick) >= 0)
		{
			this->Disconnect(n,"login timeout");
		}

		if(lpInfo->State == FAKE_BOT_ONLINE)
		{
			// FakeBoot has no physical client to answer the periodic checksum
			// request. The loaded FakeBoot slot/index is authoritative here because
			// CharacterInfoSet may clear IsSmartCharacter while rebuilding gObj.
			if(OBJECT_RANGE(lpInfo->Index) != 0 && gObjIsConnected(lpInfo->Index) != 0)
			{
				LPOBJ lpFake = &gObj[lpInfo->Index];
				lpFake->IsSmartCharacter = 1;
				lpFake->CheckSumTime = 0;
				lpFake->ConnectTickCount = tick;

				// A virtual client must acknowledge viewport creation after
				// /trace, teleport and respawn, otherwise it stays invisible.
				if(lpFake->RegenOk == 1)
				{
					lpFake->RegenOk = 2;
				}

				// A physical client advances the death screen. FakeBoot has no
				// screen, so release the normal server respawn after 5 seconds.
				if(lpFake->Live == 0 && lpFake->DieRegen == 1 && (tick-lpFake->RegenTime) >= 5000)
				{
					lpFake->DieRegen = 2;
				}
			}

			if(lpInfo->AdvancedAI != 0)
			{
				this->RunAdvancedAI(n);
			}
		}
	}
}

void CFakeBot::OnAccountLogin(int aIndex)
{
	int slot = this->FindByIndex(aIndex);

	if(slot < 0 || this->m_Bot[slot].State != FAKE_BOT_ACCOUNT || this->IsManagedIndex(aIndex) == 0)
	{
		return;
	}

	FAKE_BOT_INFO* lpInfo = &this->m_Bot[slot];
	lpInfo->State = FAKE_BOT_CHARACTER;
	lpInfo->NextActionTick = GetTickCount() + 30000;
	GDCharacterInfoSend(aIndex,lpInfo->Character);
	LogAdd(LOG_GREEN,"[FakeBoot] Character load requested: %s",lpInfo->Character);
}

void CFakeBot::OnAccountLoginFailed(int aIndex, int result)
{
	int slot = this->FindByIndex(aIndex);

	if(slot < 0 || this->IsManagedIndex(aIndex) == 0)
	{
		return;
	}

	char reason[64];
	sprintf_s(reason,sizeof(reason),"account login rejected (result %d)",result);
	this->Disconnect(slot,reason);
}

void CFakeBot::OnCharacterLoaded(int aIndex)
{
	int slot = this->FindByIndex(aIndex);

	// CharacterInfoSet rebuilds gObj and may clear IsSmartCharacter.  The
	// managed slot/index is the stable identity while the virtual character is
	// being loaded.
	if(slot < 0 || this->IsManagedIndex(aIndex) == 0 || gObjIsConnectedGP(aIndex) == 0)
	{
		return;
	}

	FAKE_BOT_INFO* lpInfo = &this->m_Bot[slot];
	lpInfo->State = FAKE_BOT_ONLINE;
	lpInfo->NextActionTick = 0;
	lpInfo->HomeMap = gObj[aIndex].Map;
	lpInfo->HomeX = gObj[aIndex].X;
	lpInfo->HomeY = gObj[aIndex].Y;
	lpInfo->LastTargetSearchTick = 0;
	lpInfo->TargetSearchEmpty = false;
	lpInfo->LastAttackTick = GetTickCount();
	lpInfo->LastMoveTick = GetTickCount();
	lpInfo->LastPotionTick = GetTickCount();
	gObj[aIndex].CheckSumTime = 0;
	this->ConfigureAutoProgress(lpInfo,&gObj[aIndex]);
	LogAdd(LOG_GREEN,"[FakeBoot] Character online: %s [%s] index %d",gObj[aIndex].Name,gObj[aIndex].Account,aIndex);
}

void CFakeBot::ConfigureAutoProgress(FAKE_BOT_INFO* lpInfo, LPOBJ lpObj)
{
	if(lpInfo->AutoReset != 0)
	{
		lpObj->AutoResetEnable = 1;
	}

	if(lpInfo->AutoPoints == 0)
	{
		return;
	}

	// Desired class builds. The official auto-point routine still enforces the
	// account stat cap and only spends points genuinely earned by the character.
	int desired[5] = {5000,5000,5000,5000,0};
	switch(lpObj->Class)
	{
		case CLASS_DW: desired[0]=500; desired[1]=5000; desired[2]=5000; desired[3]=30000; break;
		case CLASS_DK: desired[0]=20000; desired[1]=15000; desired[2]=10000; desired[3]=5000; break;
		case CLASS_FE: desired[0]=5000; desired[1]=25000; desired[2]=5000; desired[3]=15000; break;
		case CLASS_MG: desired[0]=15000; desired[1]=10000; desired[2]=5000; desired[3]=20000; break;
		case CLASS_DL: desired[0]=15000; desired[1]=8000; desired[2]=8000; desired[3]=8000; desired[4]=15000; break;
		case CLASS_SU: desired[0]=500; desired[1]=5000; desired[2]=5000; desired[3]=30000; break;
		case CLASS_RF: desired[0]=20000; desired[1]=8000; desired[2]=15000; desired[3]=7000; break;
	}

	int current[5] = {lpObj->Strength,lpObj->Dexterity,lpObj->Vitality,lpObj->Energy,lpObj->Leadership};
	lpObj->AutoAddPointCount = 0;
	for(int n=0;n<5;n++)
	{
		lpObj->AutoResetStats[n] = desired[n];
		lpObj->AutoAddPointStats[n] = ((desired[n] > current[n]) ? (desired[n]-current[n]) : 0);
		if(lpObj->AutoAddPointStats[n] > 0)
		{
			lpObj->AutoAddPointCount++;
		}
	}
}

int CFakeBot::FindMonster(FAKE_BOT_INFO* lpInfo, LPOBJ lpObj) const
{
	// During an official 1x1 match, fight the opponent selected by the event.
	int tournamentOpponent = gPvPTournament.GetMatchOpponent(lpObj->Index);
	if(OBJECT_RANGE(tournamentOpponent) != 0 && tournamentOpponent != lpObj->Index &&
		gObjIsConnected(tournamentOpponent) != 0 && gObj[tournamentOpponent].Live != 0 &&
		gObj[tournamentOpponent].Map == lpObj->Map &&
		gMap[gObj[tournamentOpponent].Map].CheckAttr(gObj[tournamentOpponent].X,gObj[tournamentOpponent].Y,1) == 0)
	{
		return tournamentOpponent;
	}

	// Behave like a peaceful player: never attack another player first, but
	// immediately defend itself when a player (or monster) attacks it.
	int attacker = lpObj->LastAttackerID;
	if(OBJECT_RANGE(attacker) != 0 && attacker != lpObj->Index &&
		gObjIsConnected(attacker) != 0 && gObj[attacker].Live != 0 &&
		gObj[attacker].Map == lpObj->Map &&
		(gObj[attacker].Type == OBJECT_USER || gObj[attacker].Type == OBJECT_BOTS || gObj[attacker].Type == OBJECT_MONSTER) &&
		gMap[gObj[attacker].Map].CheckAttr(gObj[attacker].X,gObj[attacker].Y,1) == 0)
	{
		int distance = (int)sqrt((float)(((lpObj->X-gObj[attacker].X)*(lpObj->X-gObj[attacker].X))+((lpObj->Y-gObj[attacker].Y)*(lpObj->Y-gObj[attacker].Y))));
		if(distance <= lpInfo->SearchRange)
		{
			return attacker;
		}
	}

	lpObj->LastAttackerID = -1;

	// Empty-area polling is bounded; retaliation above remains immediate.
	const DWORD searchTick = GetTickCount();
	if (lpInfo->TargetSearchEmpty && searchTick-lpInfo->LastTargetSearchTick < 500)
		return -1;
	lpInfo->LastTargetSearchTick = searchTick;
	int bestTarget = -1;
	int bestDistance = lpInfo->SearchRange + 1;

	for(int n=OBJECT_START_MONSTER;n<MAX_OBJECT_MONSTER;n++)
	{
		LPOBJ lpTarget = &gObj[n];

		if(gObjIsConnected(n) == 0 || lpTarget->Live == 0 || lpTarget->Type != OBJECT_MONSTER || lpTarget->Map != lpObj->Map)
		{
			continue;
		}

		if (abs(lpObj->X-lpTarget->X) > lpInfo->SearchRange ||
			abs(lpObj->Y-lpTarget->Y) > lpInfo->SearchRange) continue;
		if(gMap[lpTarget->Map].CheckAttr(lpTarget->X,lpTarget->Y,1) != 0)
		{
			continue;
		}

		int homeDistance = (int)sqrt((float)(((lpInfo->HomeX-lpTarget->X)*(lpInfo->HomeX-lpTarget->X))+((lpInfo->HomeY-lpTarget->Y)*(lpInfo->HomeY-lpTarget->Y))));
		if(lpTarget->Map != lpInfo->HomeMap || homeDistance > lpInfo->HomeRange)
		{
			continue;
		}

		int distance = (int)sqrt((float)(((lpObj->X-lpTarget->X)*(lpObj->X-lpTarget->X))+((lpObj->Y-lpTarget->Y)*(lpObj->Y-lpTarget->Y))));
		if(distance <= lpInfo->SearchRange && distance < bestDistance)
		{
			bestTarget = n;
			bestDistance = distance;
		}
	}

	lpInfo->TargetSearchEmpty = bestTarget < 0;
	return bestTarget;
}

void CFakeBot::UseLifePotion(FAKE_BOT_INFO* lpInfo, LPOBJ lpObj)
{
	int maxLife = (int)(lpObj->MaxLife + lpObj->AddLife);

	if(maxLife <= 0 || ((lpObj->Life * 100) / maxLife) > lpInfo->PotionPercent)
	{
		return;
	}

	DWORD tick = GetTickCount();
	if((tick-lpInfo->LastPotionTick) < 1500)
	{
		return;
	}

	PMSG_ITEM_USE_RECV pMsg;
	pMsg.header.set(0x26,sizeof(pMsg));
	pMsg.SourceSlot = 0xFF;
	pMsg.SourceSlot = ((pMsg.SourceSlot == 0xFF) ? gItemManager.GetInventoryItemSlot(lpObj,GET_ITEM(14,3),-1) : pMsg.SourceSlot);
	pMsg.SourceSlot = ((pMsg.SourceSlot == 0xFF) ? gItemManager.GetInventoryItemSlot(lpObj,GET_ITEM(14,2),-1) : pMsg.SourceSlot);
	pMsg.SourceSlot = ((pMsg.SourceSlot == 0xFF) ? gItemManager.GetInventoryItemSlot(lpObj,GET_ITEM(14,1),-1) : pMsg.SourceSlot);
	pMsg.TargetSlot = 0xFF;
	pMsg.type = 0;

	if(INVENTORY_FULL_RANGE(pMsg.SourceSlot) != 0)
	{
		gItemManager.CGItemUseRecv(&pMsg,lpObj->Index);
		lpInfo->LastPotionTick = tick;
	}
}

int CFakeBot::FindChatBot(int playerIndex, const char* name, int range) const
{
	if (!OBJECT_RANGE(playerIndex) || !gObjIsConnectedGP(playerIndex)) return -1;
	const LPOBJ player = &gObj[playerIndex];
	int nearest = -1;
	int bestDistance = range * range + 1;
	for (int slot = 0; slot < this->m_Count; ++slot)
	{
		int index = this->m_Bot[slot].Index;
		if (this->m_Bot[slot].State != FAKE_BOT_ONLINE || !OBJECT_RANGE(index) || !gObjIsConnectedGP(index) || !gObj[index].Live ||
			gObj[index].Map != player->Map) continue;
		if (name != 0 && _stricmp(gObj[index].Name, name) != 0) continue;
		int dx = player->X - gObj[index].X;
		int dy = player->Y - gObj[index].Y;
		int distance = dx*dx + dy*dy;
		if (distance <= range*range && distance < bestDistance)
		{
			nearest = index;
			bestDistance = distance;
		}
	}
	return nearest;
}

void CFakeBot::UseManaPotion(FAKE_BOT_INFO* lpInfo, LPOBJ lpObj)
{
	const float maxMana = lpObj->MaxMana + lpObj->AddMana;
	const DWORD tick = GetTickCount();
	if (maxMana <= 0 || lpObj->Mana * 100.0f / maxMana >= 40.0f ||
		(tick-lpInfo->LastPotionTick) < 1500) return;
	PMSG_ITEM_USE_RECV message = {};
	message.header.set(0x26, sizeof(message));
	message.SourceSlot = 0xFF;
	for (int type = 6; type >= 4 && message.SourceSlot == 0xFF; --type)
		message.SourceSlot = gItemManager.GetInventoryItemSlot(lpObj, GET_ITEM(14,type), -1);
	message.TargetSlot = 0xFF;
	message.type = 0;
	if (INVENTORY_FULL_RANGE(message.SourceSlot))
	{
		gItemManager.CGItemUseRecv(&message, lpObj->Index);
		lpInfo->LastPotionTick = tick;
	}
}

void CFakeBot::RunAdvancedAI(int slot)
{
	FAKE_BOT_INFO* lpInfo = &this->m_Bot[slot];

	if(OBJECT_RANGE(lpInfo->Index) == 0 || gObjIsConnectedGP(lpInfo->Index) == 0)
	{
		return;
	}

	LPOBJ lpObj = &gObj[lpInfo->Index];

	if(lpObj->Live == 0 || lpObj->State != OBJECT_PLAYING || lpObj->Teleport != 0 || lpObj->Interface.use != 0)
	{
		return;
	}

	if(lpObj->Map != lpInfo->HomeMap || gMap[lpObj->Map].CheckAttr(lpObj->X,lpObj->Y,1) != 0)
	{
		return;
	}

	this->UseLifePotion(lpInfo,lpObj);
	this->UseManaPotion(lpInfo,lpObj);

	int target = this->FindMonster(lpInfo,lpObj);
	DWORD tick = GetTickCount();

	if(target < 0)
	{
		int homeDistance = (int)sqrt((float)(((lpInfo->HomeX-lpObj->X)*(lpInfo->HomeX-lpObj->X))+((lpInfo->HomeY-lpObj->Y)*(lpInfo->HomeY-lpObj->Y))));

		if(homeDistance > 2 && (tick-lpInfo->LastMoveTick) >= 1000)
		{
			if (lpObj->PathCur < lpObj->PathCount &&
				lpObj->MTX == lpInfo->HomeX && lpObj->MTY == lpInfo->HomeY) return;
			lpObj->MTX = lpInfo->HomeX;
			lpObj->MTY = lpInfo->HomeY;
			lpObj->ActionState.Move = 1;
			PathFindMoveMsgSend(lpObj);
			lpInfo->LastMoveTick = tick;
		}
		else if(homeDistance <= 2 && (tick-lpInfo->LastMoveTick) >= 3500)
		{
			// Small idle patrol makes the FakeBoot look active without letting it
			// leave its configured farming area.
			for(int attempt=0;attempt<8;attempt++)
			{
				int x = lpInfo->HomeX + ((GetLargeRand()%7)-3);
				int y = lpInfo->HomeY + ((GetLargeRand()%7)-3);
				if(gMap[lpObj->Map].CheckAttr(x,y,1) == 0)
				{
					lpObj->MTX = x;
					lpObj->MTY = y;
					lpObj->ActionState.Move = 1;
					PathFindMoveMsgSend(lpObj);
					lpInfo->LastMoveTick = tick;
					break;
				}
			}
		}
		return;
	}

	LPOBJ lpTarget = &gObj[target];
	int distance = (int)sqrt((float)(((lpObj->X-lpTarget->X)*(lpObj->X-lpTarget->X))+((lpObj->Y-lpTarget->Y)*(lpObj->Y-lpTarget->Y))));

	if(distance > lpInfo->AttackRange)
	{
		if((tick-lpInfo->LastMoveTick) >= 700)
		{
			if (lpObj->PathCur < lpObj->PathCount &&
				lpObj->MTX == lpTarget->X && lpObj->MTY == lpTarget->Y) return;
			lpObj->MTX = lpTarget->X;
			lpObj->MTY = lpTarget->Y;
			lpObj->ActionState.Move = 1;
			PathFindMoveMsgSend(lpObj);
			lpInfo->LastMoveTick = tick;
		}
		return;
	}

	int attackDelay = lpInfo->AttackDelay-(lpObj->Dexterity/6);
	if(attackDelay < 250) { attackDelay = 250; }
	if(attackDelay > lpInfo->AttackDelay) { attackDelay = lpInfo->AttackDelay; }

	if((tick-lpInfo->LastAttackTick) < (DWORD)attackDelay)
	{
		return;
	}

	int skill = 0;
	if(gCustomAttack.GetAttackSkill(lpObj,&skill,lpInfo->Skill) == 0)
	{
		skill = 0;
	}

	CSkill* lpSkill = ((skill > 0) ? gSkillManager.GetSkill(lpObj,skill) : 0);
	lpObj->TargetNumber = target;
	lpObj->Dir = GetPathPacketDirPos(lpTarget->X-lpObj->X,lpTarget->Y-lpObj->Y);
	GCActionSend(lpObj,ACTION_ATTACK1,lpObj->Index,target);
	gAttack.Attack(lpObj,lpTarget,lpSkill,1,0,0,0,0);
	lpInfo->LastAttackTick = tick;
}
