// Party.cpp: implementation of the CParty class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Party.h"
#include "FakeBot.h"
#include "BotPlayer.h"
#include "EffectManager.h"
#include "Map.h"
#include "PartyMatching.h"
#include "ServerInfo.h"
#include "Util.h"

CParty gParty;
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CParty::CParty() // OK
{
	this->m_PartyCount = 0;
}

CParty::~CParty() // OK
{

}

bool CParty::IsParty(int index) // OK
{
	if (OBJECT_RANGE(index) == 0)
	{
		return 0;
	}

	if (this->m_PartyInfo[index].Count == 0)
	{
		return 0;
	}

	return 1;
}

bool CParty::IsLeader(int index, int aIndex) // OK
{
	if (this->IsParty(index) == 0)
	{
		return 0;
	}

	if (this->m_PartyInfo[index].Index[0] == aIndex)
	{
		return 1;
	}

	return 0;
}

bool CParty::IsMember(int index, int aIndex) // OK
{
	if (this->IsParty(index) == 0)
	{
		return 0;
	}

	for (int n = 0; n < MAX_PARTY_USER; n++)
	{
		if (this->m_PartyInfo[index].Index[n] == aIndex)
		{
			return 1;
		}
	}

	return 0;
}

int CParty::GetMemberCount(int index) // OK
{
	if (this->IsParty(index) == 0)
	{
		return 0;
	}

	return this->m_PartyInfo[index].Count;
}

int CParty::GetMemberIndex(int index, int number) // OK
{
	if (this->IsParty(index) == 0)
	{
		return -1;
	}

	if (number < 0 || number >= MAX_PARTY_USER)
	{
		return -1;
	}

	return this->m_PartyInfo[index].Index[number];
}

int CParty::GetMemberNumber(int index, int aIndex) // OK
{
	if (this->IsParty(index) == 0)
	{
		return -1;
	}

	for (int n = 0; n < MAX_PARTY_USER; n++)
	{
		if (this->m_PartyInfo[index].Index[n] == aIndex)
		{
			return n;
		}
	}

	return -1;
}

bool CParty::Create(int aIndex) // OK
{
	if (this->IsParty(gObj[aIndex].PartyNumber) != 0)
	{
		return 0;
	}

	int count = this->m_PartyCount;

	for (int n = 0; n < MAX_OBJECT; n++)
	{
		if (this->m_PartyInfo[count].Count == 0)
		{
			gObj[aIndex].PartyNumber = count;
			this->m_PartyInfo[count].Count = 1;
			this->m_PartyInfo[count].Index[0] = aIndex;

			for (int i = 1; i < MAX_PARTY_USER; i++)
			{
				this->m_PartyInfo[count].Index[i] = -1;
			}
#ifdef PARTY_PASSWORD_JOIN
			this->m_PartyInfo[count].Password.clear();
#endif // PARTY_PASSWORD_JOIN
			this->m_PartyInfo[count].IsPublic = false;
			this->m_PartyInfo[count].HasPublicPwd = false;
			this->m_PartyInfo[count].PublicPassword[0] = '\0';
			this->GCPartyListSend(count);
			this->m_PartyCount = (((++this->m_PartyCount) >= MAX_OBJECT) ? 0 : this->m_PartyCount);
			return 1;
		}
		else
		{
			count = (((++count) >= MAX_OBJECT) ? 0 : count);
		}
	}

	return 0;
}

bool CParty::Destroy(int index) // OK
{
	if (this->IsParty(index) == 0)
	{
		return 0;
	}

	for (int n = 0; n < MAX_PARTY_USER; n++)
	{
		if (OBJECT_RANGE(this->m_PartyInfo[index].Index[n]) != 0)
		{
			gObj[this->m_PartyInfo[index].Index[n]].PartyNumber = -1;
			this->GCPartyDelMemberSend(this->m_PartyInfo[index].Index[n]);
			this->m_PartyInfo[index].Count--;
			this->m_PartyInfo[index].Index[n] = -1;
#ifdef PARTY_PASSWORD_JOIN
			this->m_PartyInfo[index].Password.clear();


			gObj[this->m_PartyInfo[index].Index[n]].CurAutoJoinParty = -1;
			gObj[this->m_PartyInfo[index].Index[n]].CurAutoJoinPartyPassword.clear();
#endif // PARTY_PASSWORD_JOIN
		}
	}

	this->m_PartyInfo[index].IsPublic = false;
	this->m_PartyInfo[index].HasPublicPwd = false;
	this->m_PartyInfo[index].PublicPassword[0] = '\0';

	return 1;
}

bool CParty::AddMember(int index, int aIndex) // OK
{
	if (this->IsParty(index) == 0)
	{
		return 0;
	}

	if (this->GetMemberNumber(index, aIndex) != -1)
	{
		return 0;
	}

	for (int n = 0; n < gServerInfo.m_PartyMaxMembers; n++)
	{
		if (OBJECT_RANGE(this->m_PartyInfo[index].Index[n]) == 0)
		{
			gObj[aIndex].PartyNumber = index;
			this->m_PartyInfo[index].Count++;
			this->m_PartyInfo[index].Index[n] = aIndex;
			this->GCPartyListSend(index);
			return 1;
		}
	}

	return 0;
}

bool CParty::DelMember(int index, int aIndex) // OK
{
	if (this->IsParty(index) == 0)
	{
		return 0;
	}

	for (int n = 0; n < MAX_PARTY_USER; n++)
	{
		if (this->m_PartyInfo[index].Index[n] == aIndex)
		{
#ifdef PARTY_PASSWORD_JOIN
			gObj[aIndex].CurAutoJoinParty = -1;
			gObj[aIndex].CurAutoJoinPartyPassword.clear();
#endif // PARTY_PASSWORD_JOIN
			gObj[aIndex].PartyNumber = -1;
			this->m_PartyInfo[index].Count--;
			this->m_PartyInfo[index].Index[n] = -1;
			this->ChangeLeader(index, n);
			this->GCPartyDelMemberSend(aIndex);
			this->GCPartyListSend(index);
			return 1;
		}
	}

	return 0;
}

void CParty::ChangeLeader(int index, int number) // OK
{
	if (this->IsParty(index) == 0 || number != 0)
	{
		return;
	}

	for (int n = 1; n < MAX_PARTY_USER; n++)
	{
		if (OBJECT_RANGE(this->m_PartyInfo[index].Index[n]) != 0)
		{
			this->m_PartyInfo[index].Index[0] = this->m_PartyInfo[index].Index[n];
			this->m_PartyInfo[index].Index[n] = -1;
			break;
		}
	}
}

bool CParty::AutoAcceptPartyRequest(LPOBJ lpObj, LPOBJ lpTarget)
{
	if (this->IsParty(lpTarget->PartyNumber) == 0)
	{
		if (gObjCheckAutoParty(lpObj, lpTarget) != 0)
		{
			if (this->IsParty(lpObj->PartyNumber) == 0)
			{
				if (this->Create(lpObj->Index) == 0 || this->AddMember(lpObj->PartyNumber, lpTarget->Index) == 0)
				{
					this->GCPartyResultSend(lpObj->Index, 2);
					this->GCPartyResultSend(lpTarget->Index, 2);
					this->Destroy(lpObj->PartyNumber);
					return 1;
				}
				else
				{
					gPartyMatching.GDPartyMatchingJoinCancelSend(lpObj->Index, 1, 1);
					gPartyMatching.GDPartyMatchingJoinCancelSend(lpTarget->Index, 0, 1);
					gPartyMatching.GDPartyMatchingJoinCancelSend(lpTarget->Index, 1, 1);
					return 1;
				}
			}
			else
			{
				if (this->IsLeader(lpObj->PartyNumber, lpObj->Index) != 0 || gObjCheckAutoParty(lpTarget, &gObj[this->GetMemberIndex(lpObj->PartyNumber, 0)]) != 0)
				{
					if (this->AddMember(lpObj->PartyNumber, lpTarget->Index) == 0)
					{
						this->GCPartyResultSend(lpObj->Index, 2);
						this->GCPartyResultSend(lpTarget->Index, 2);
						return 1;
					}
					else
					{
						gPartyMatching.GDPartyMatchingJoinCancelSend(lpTarget->Index, 0, 1);
						gPartyMatching.GDPartyMatchingJoinCancelSend(lpTarget->Index, 1, 1);
						return 1;
					}
				}
			}
		}
	}
	else
	{
		if (gObjCheckAutoParty(lpObj, &gObj[this->GetMemberIndex(lpTarget->PartyNumber, 0)]) != 0)
		{
			if (this->IsParty(lpObj->PartyNumber) == 0)
			{
				if (this->AddMember(lpTarget->PartyNumber, lpObj->Index) == 0)
				{
					this->GCPartyResultSend(lpObj->Index, 2);
					this->GCPartyResultSend(lpTarget->Index, 2);
					return 1;
				}
				else
				{
					gPartyMatching.GDPartyMatchingJoinCancelSend(lpObj->Index, 0, 1);
					gPartyMatching.GDPartyMatchingJoinCancelSend(lpObj->Index, 1, 1);
					return 1;
				}
			}
		}
	}

	return 0;
}

bool CParty::PartyPasswordCheck(int index, std::string password)
{
#ifdef PARTY_PASSWORD_JOIN
	if (this->IsParty(index) == 0)
	{
		return 0;
	}

	if (this->m_PartyInfo[index].Password.empty() != 0 || password.empty() != 0)
	{
		return 0;
	}

	return (this->m_PartyInfo[index].Password == password);
#else
	return false;
#endif // PARTY_PASSWORD_JOIN
}

void CParty::CGPartyRequestRecv(PMSG_PARTY_REQUEST_RECV* lpMsg, int aIndex) // OK
{
	LPOBJ lpObj = &gObj[aIndex];

	if (gObjIsConnectedGP(aIndex) == 0)
	{
		return;
	}

	int bIndex = MAKE_NUMBERW(lpMsg->index[0], lpMsg->index[1]);

	if (gObjIsConnectedGP(bIndex) == 0)
	{
		return;
	}

	LPOBJ lpTarget = &gObj[bIndex];

	if (lpObj->Interface.use != 0 || lpTarget->Interface.use != 0)
	{
		return;
	}

	if (CA_MAP_RANGE(lpTarget->Map) != 0 || CC_MAP_RANGE(lpTarget->Map) != 0 || IT_MAP_RANGE(lpTarget->Map) != 0 || DA_MAP_RANGE(lpTarget->Map) != 0 || DG_MAP_RANGE(lpTarget->Map) != 0 || IG_MAP_RANGE(lpTarget->Map) != 0)
	{
		this->GCPartyResultSend(aIndex, 0);
		return;
	}

	if (OBJECT_RANGE(lpObj->PartyTargetUser) != 0 || OBJECT_RANGE(lpTarget->PartyTargetUser) != 0)
	{
		this->GCPartyResultSend(aIndex, 0);
		return;
	}

	if (gServerInfo.m_GensSystemPartyLock != 0 && lpObj->GensFamily != lpTarget->GensFamily)
	{
		this->GCPartyResultSend(aIndex, 6);
		return;
	}

	if (this->AutoAcceptPartyRequest(lpObj, lpTarget) != 0)
	{
		return;
	}

	if (this->IsParty(lpObj->PartyNumber) != 0 && this->IsLeader(lpObj->PartyNumber, aIndex) == 0)
	{
		this->GCPartyResultSend(aIndex, 0);
		return;
	}

	bool automatedTarget = gFakeBot.IsFakeBot(bIndex);
#if(BOT_PLAYER == 1)
	automatedTarget = automatedTarget || gBotPlayer.IsBotPlayer(bIndex);
#endif
	if (automatedTarget && (lpObj->Map != lpTarget->Map ||
		(lpObj->X-lpTarget->X)*(lpObj->X-lpTarget->X) +
		(lpObj->Y-lpTarget->Y)*(lpObj->Y-lpTarget->Y) > 100))
	{
		this->GCPartyResultSend(aIndex, 0);
		return;
	}
	if ((lpTarget->Option & 1) == 0 && !automatedTarget)
	{
		this->GCPartyResultSend(aIndex, 1);
		return;
	}

	if (this->IsParty(lpTarget->PartyNumber) != 0)
	{
		this->GCPartyResultSend(aIndex, 4);
		return;
	}

	lpObj->Interface.use = 1;
	lpObj->Interface.type = INTERFACE_PARTY;
	lpObj->Interface.state = 0;
	lpObj->InterfaceTime = GetTickCount();
	lpObj->TargetNumber = bIndex;
	lpObj->PartyTargetUser = bIndex;

	lpTarget->Interface.use = 1;
	lpTarget->Interface.type = INTERFACE_PARTY;
	lpTarget->Interface.state = 0;
	lpTarget->InterfaceTime = GetTickCount();
	lpTarget->TargetNumber = aIndex;

	PMSG_PARTY_REQUEST_SEND pMsg;

	pMsg.header.set(0x40, sizeof(pMsg));

	pMsg.index[0] = SET_NUMBERHB(aIndex);
	pMsg.index[1] = SET_NUMBERLB(aIndex);

	if (automatedTarget)
	{
		// Use the normal acceptance path: member limits and cleanup stay shared.
		PMSG_PARTY_REQUEST_RESULT_RECV response = {};
		response.header.set(0x41, sizeof(response));
		response.result = 1;
		response.index[0] = SET_NUMBERHB(aIndex);
		response.index[1] = SET_NUMBERLB(aIndex);
		this->CGPartyRequestResultRecv(&response, bIndex);
		return;
	}
	DataSend(bIndex, (BYTE*)&pMsg, pMsg.header.size);
}

void CParty::CGPartyRequestResultRecv(PMSG_PARTY_REQUEST_RESULT_RECV* lpMsg, int aIndex) // OK
{
	LPOBJ lpObj = &gObj[aIndex];

	if (gObjIsConnectedGP(aIndex) == 0)
	{
		return;
	}

	int bIndex = MAKE_NUMBERW(lpMsg->index[0], lpMsg->index[1]);

	if (gObjIsConnectedGP(bIndex) == 0)
	{
		return;
	}

	LPOBJ lpTarget = &gObj[bIndex];

	if (lpObj->Interface.use == 0 || lpObj->Interface.type != INTERFACE_PARTY)
	{
		return;
	}

	if (lpTarget->Interface.use == 0 || lpTarget->Interface.type != INTERFACE_PARTY)
	{
		return;
	}

	if (CA_MAP_RANGE(lpObj->Map) != 0 || CC_MAP_RANGE(lpObj->Map) != 0 || IT_MAP_RANGE(lpObj->Map) != 0 || DA_MAP_RANGE(lpObj->Map) != 0 || DG_MAP_RANGE(lpObj->Map) != 0 || IG_MAP_RANGE(lpObj->Map) != 0)
	{
		this->GCPartyResultSend(aIndex, 0);
		goto CLEAR_JUMP;
	}

	if (lpMsg->result == 0)
	{
		// The invitation was rejected: notify the player who sent it.
		this->GCPartyResultSend(bIndex, 1);
		goto CLEAR_JUMP;
	}

	if (this->Create(bIndex) == 0)
	{
		if (this->AddMember(lpTarget->PartyNumber, aIndex) == 0)
		{
			this->GCPartyResultSend(aIndex, 2);
			this->GCPartyResultSend(bIndex, 2);
		}
		else
		{
			gPartyMatching.GDPartyMatchingJoinCancelSend(aIndex, 0, 1);
			gPartyMatching.GDPartyMatchingJoinCancelSend(aIndex, 1, 1);
		}
	}
	else
	{
		if (this->AddMember(lpTarget->PartyNumber, aIndex) == 0)
		{
			this->GCPartyResultSend(aIndex, 2);
			this->GCPartyResultSend(bIndex, 2);
			this->Destroy(lpTarget->PartyNumber);
		}
		else
		{
			gPartyMatching.GDPartyMatchingJoinCancelSend(aIndex, 0, 1);
			gPartyMatching.GDPartyMatchingJoinCancelSend(aIndex, 1, 1);
			gPartyMatching.GDPartyMatchingJoinCancelSend(bIndex, 1, 1);
		}
	}

CLEAR_JUMP:
	lpObj->Interface.use = 0;
	lpObj->Interface.type = INTERFACE_NONE;
	lpObj->Interface.state = 0;
	lpObj->InterfaceTime = 0;
	lpObj->TargetNumber = -1;
	lpObj->PartyTargetUser = -1;

	lpTarget->Interface.use = 0;
	lpTarget->Interface.type = INTERFACE_NONE;
	lpTarget->Interface.state = 0;
	lpTarget->InterfaceTime = 0;
	lpTarget->TargetNumber = -1;
	lpTarget->PartyTargetUser = -1;
}

void CParty::CGPartyListRecv(int aIndex) // OK
{
	LPOBJ lpObj = &gObj[aIndex];

	if (gObjIsConnectedGP(aIndex) == 0)
	{
		return;
	}

#ifdef INCREASE_PARTY_MANAGER
	BYTE send[512];

	PMSG_PARTY_LIST_SEND pMsg;
#else
	BYTE send[256];

	PMSG_PARTY_LIST_SEND pMsg;
#endif // INCREASE_PARTY_MANAGER

	pMsg.header.set(0x42, 0, 0);

	int size = sizeof(pMsg);

	pMsg.count = 0;

	if (this->IsParty(lpObj->PartyNumber) != 0)
	{
		PMSG_PARTY_LIST info;

		for (int n = 0; n < MAX_PARTY_USER; n++)
		{
			if (OBJECT_RANGE(this->m_PartyInfo[lpObj->PartyNumber].Index[n]) == 0)
			{
				continue;
			}

			LPOBJ lpTarget = &gObj[this->m_PartyInfo[lpObj->PartyNumber].Index[n]];

			memcpy(info.name, lpTarget->Name, sizeof(info.name));

			info.number = n;

			info.map = lpTarget->Map;

			info.x = (BYTE)lpTarget->X;

			info.y = (BYTE)lpTarget->Y;

			info.CurLife = (DWORD)(lpTarget->Life);

			info.MaxLife = (DWORD)(lpTarget->MaxLife + lpTarget->AddLife);

#if(GAMESERVER_UPDATE>=801)
			info.ServerCode = gServerInfo.m_ServerCode + 1;

			info.CurMana = (DWORD)(lpTarget->Mana);

			info.MaxMana = (DWORD)(lpTarget->MaxMana + lpTarget->AddMana);
#endif

			memcpy(&send[size], &info, sizeof(info));
			size += sizeof(info);

			pMsg.count++;
		}
	}

#ifdef INCREASE_PARTY_MANAGER
	pMsg.header.size[0] = SET_NUMBERHB(size);
	pMsg.header.size[1] = SET_NUMBERLB(size);
#else
	pMsg.header.size = size;
#endif // INCREASE_PARTY_MANAGER

	memcpy(send, &pMsg, sizeof(pMsg));

	DataSend(aIndex, send, size);
}

void CParty::CGPartyDelMemberRecv(PMSG_PARTY_DEL_MEMBER_RECV* lpMsg, int aIndex) // OK
{
	LPOBJ lpObj = &gObj[aIndex];

	if (gObjIsConnectedGP(aIndex) == 0)
	{
		return;
	}

	int bIndex = this->GetMemberIndex(lpObj->PartyNumber, lpMsg->number);

	if (gObjIsConnectedGP(bIndex) == 0)
	{
		return;
	}

	LPOBJ lpTarget = &gObj[bIndex];

	if (aIndex != bIndex && this->IsLeader(lpObj->PartyNumber, aIndex) == 0)
	{
		return;
	}

	if (IT_MAP_RANGE(lpTarget->Map) != 0 || DG_MAP_RANGE(lpTarget->Map) != 0 || IG_MAP_RANGE(lpTarget->Map) != 0)
	{
		return;
	}

	if (this->GetMemberCount(lpObj->PartyNumber) <= 2)
	{
		this->Destroy(lpObj->PartyNumber);
	}
	else
	{
		this->DelMember(lpObj->PartyNumber, bIndex);
	}
}

void CParty::GCPartyResultSend(int aIndex, BYTE result) // OK
{
	PMSG_PARTY_RESULT_SEND pMsg;

	pMsg.header.set(0x41, sizeof(pMsg));

	pMsg.result = result;

	DataSend(aIndex, (BYTE*)&pMsg, pMsg.header.size);
}

void CParty::GCPartyListSend(int index) // OK
{
	if (this->IsParty(index) == 0)
	{
		return;
	}

#ifdef INCREASE_PARTY_MANAGER
	BYTE send[512];

	PMSG_PARTY_LIST_SEND pMsg;
#else
	BYTE send[256];

	PMSG_PARTY_LIST_SEND pMsg;
#endif // INCREASE_PARTY_MANAGER

	pMsg.header.set(0x42, 0, 0);

	int size = sizeof(pMsg);

	pMsg.count = 0;

	PMSG_PARTY_LIST info;

	for (int n = 0; n < MAX_PARTY_USER; n++)
	{
		if (OBJECT_RANGE(this->m_PartyInfo[index].Index[n]) == 0)
		{
			continue;
		}

		LPOBJ lpObj = &gObj[this->m_PartyInfo[index].Index[n]];

		memcpy(info.name, lpObj->Name, sizeof(info.name));

		info.number = n;

		info.map = lpObj->Map;

		info.x = (BYTE)lpObj->X;

		info.y = (BYTE)lpObj->Y;

		info.CurLife = (DWORD)(lpObj->Life);

		info.MaxLife = (DWORD)(lpObj->MaxLife + lpObj->AddLife);

#if(GAMESERVER_UPDATE>=801)
		info.ServerCode = gServerInfo.m_ServerCode + 1;

		info.CurMana = (DWORD)(lpObj->Mana);

		info.MaxMana = (DWORD)(lpObj->MaxMana + lpObj->AddMana);
#endif

		memcpy(&send[size], &info, sizeof(info));
		size += sizeof(info);

		pMsg.count++;
	}

#ifdef INCREASE_PARTY_MANAGER
	pMsg.header.size[0] = SET_NUMBERHB(size);
	pMsg.header.size[1] = SET_NUMBERLB(size);
#else
	pMsg.header.size = size;
#endif // INCREASE_PARTY_MANAGER

	memcpy(send, &pMsg, sizeof(pMsg));

	for (int n = 0; n < MAX_PARTY_USER; n++)
	{
		if (OBJECT_RANGE(this->m_PartyInfo[index].Index[n]) != 0)
		{
			DataSend(this->m_PartyInfo[index].Index[n], send, size);
		}
	}

#if(GAMESERVER_UPDATE>=603)
	for (int n = 0; n < MAX_PARTY_USER; n++)
	{
		if (OBJECT_RANGE(this->m_PartyInfo[index].Index[n]) != 0)
		{
			gEffectManager.GCPartyEffectListSend(&gObj[this->m_PartyInfo[index].Index[n]]);
		}
	}
#endif
}

void CParty::GCPartyDelMemberSend(int aIndex) // OK
{
	PBMSG_HEAD pMsg;

	pMsg.set(0x43, sizeof(pMsg));

	DataSend(aIndex, (BYTE*)&pMsg, pMsg.size);
}

void CParty::GCPartyLifeSend(int index) // OK
{
	if (this->IsParty(index) == 0)
	{
		return;
	}

	BYTE send[256];

	PMSG_PARTY_LIFE_SEND pMsg;

	pMsg.header.set(0x44, 0);

	int size = sizeof(pMsg);

	pMsg.count = 0;

	PMSG_PARTY_LIFE info;

	for (int n = 0; n < MAX_PARTY_USER; n++)
	{
		if (OBJECT_RANGE(this->m_PartyInfo[index].Index[n]) == 0)
		{
			continue;
		}

		LPOBJ lpObj = &gObj[this->m_PartyInfo[index].Index[n]];

#if(GAMESERVER_UPDATE>=801)
		info.life = (BYTE)(lpObj->Life / ((lpObj->MaxLife + lpObj->AddLife) / 100));

		info.mana = (BYTE)(lpObj->Mana / ((lpObj->MaxMana + lpObj->AddMana) / 100));

		memcpy(info.name, lpObj->Name, sizeof(info.name));

		memcpy(&send[size], &info, sizeof(info));
		size += sizeof(info);

		pMsg.count++;
#else
		info.value = ((n * 16) & 0xF0) | ((BYTE)(lpObj->Life / ((lpObj->MaxLife + lpObj->AddLife) / 10)) & 0x0F);

		memcpy(&send[size], &info, sizeof(info));
		size += sizeof(info);

		pMsg.count++;
#endif
	}

	pMsg.header.size = size;

	memcpy(send, &pMsg, sizeof(pMsg));

	for (int n = 0; n < MAX_PARTY_USER; n++)
	{
		if (OBJECT_RANGE(this->m_PartyInfo[index].Index[n]) != 0)
		{
			DataSend(this->m_PartyInfo[index].Index[n], send, size);
		}
	}

#if(GAMESERVER_UPDATE>=603)
	for (int n = 0; n < MAX_PARTY_USER; n++)
	{
		if (OBJECT_RANGE(this->m_PartyInfo[index].Index[n]) != 0)
		{
			gEffectManager.GCPartyEffectListSend(&gObj[this->m_PartyInfo[index].Index[n]]);
		}
	}
#endif
}

//. ============================================================
//. NOVO - Party Match ("Party Disponivel")
//. ============================================================

bool CParty::TogglePublic(int aIndex, bool enable, bool usePassword, const char* password, BYTE* outResult)
{
	LPOBJ lpObj = &gObj[aIndex];

	if (this->IsParty(lpObj->PartyNumber) == 0)
	{
		*outResult = PARTY_MATCH_TOGGLE_NO_PARTY;
		return false;
	}

	if (this->IsLeader(lpObj->PartyNumber, aIndex) == 0)
	{
		*outResult = PARTY_MATCH_TOGGLE_NOT_LEADER;
		return false;
	}

	PARTY_INFO* info = &this->m_PartyInfo[lpObj->PartyNumber];

	info->IsPublic = enable;

	if (enable && usePassword && password != NULL && password[0] != '\0')
	{
		info->HasPublicPwd = true;
		strncpy(info->PublicPassword, password, sizeof(info->PublicPassword) - 1);
		info->PublicPassword[sizeof(info->PublicPassword) - 1] = '\0';
	}
	else
	{
		info->HasPublicPwd = false;
		info->PublicPassword[0] = '\0';
	}

	*outResult = PARTY_MATCH_TOGGLE_OK;
	return true;
}

int CParty::GetPublicPartyCount()
{
	int count = 0;

	for (int index = 0; index < MAX_OBJECT; index++)
	{
		if (this->IsParty(index) == 0)
		{
			continue;
		}

		if (this->m_PartyInfo[index].IsPublic)
		{
			count++;
		}
	}

	return count;
}

bool CParty::IsPublic(int index)
{
	if (this->IsParty(index) == 0)
	{
		return false;
	}

	return this->m_PartyInfo[index].IsPublic;
}

bool CParty::CheckPublicPassword(int index, const char* password)
{
	if (this->IsParty(index) == 0)
	{
		return false;
	}

	PARTY_INFO* info = &this->m_PartyInfo[index];

	if (info->HasPublicPwd == false)
	{
		return true; //. sem senha, sempre passa
	}

	if (password == NULL)
	{
		return false;
	}

	return (strncmp(info->PublicPassword, password, sizeof(info->PublicPassword)) == 0);
}

bool CParty::JoinPublicParty(int leaderIndex, int joiningIndex, const char* password, BYTE* outResult)
{
	if (gObjIsConnectedGP(leaderIndex) == 0)
	{
		*outResult = PARTY_MATCH_JOIN_PARTY_NOT_FOUND;
		return false;
	}

	if (gObjIsConnectedGP(joiningIndex) == 0)
	{
		*outResult = PARTY_MATCH_JOIN_PARTY_NOT_FOUND;
		return false;
	}

	LPOBJ lpLeader = &gObj[leaderIndex];
	LPOBJ lpJoining = &gObj[joiningIndex];

	//. quem esta entrando ja esta em uma party?
	if (this->IsParty(lpJoining->PartyNumber) != 0)
	{
		*outResult = PARTY_MATCH_JOIN_ALREADY_IN_PARTY;
		return false;
	}

	//. a party do lider ainda existe?
	if (this->IsParty(lpLeader->PartyNumber) == 0)
	{
		*outResult = PARTY_MATCH_JOIN_PARTY_NOT_FOUND;
		return false;
	}

	//. quem chamou ainda e o lider dessa party? (lider pode ter trocado)
	if (this->IsLeader(lpLeader->PartyNumber, leaderIndex) == 0)
	{
		*outResult = PARTY_MATCH_JOIN_PARTY_NOT_FOUND;
		return false;
	}

	PARTY_INFO* info = &this->m_PartyInfo[lpLeader->PartyNumber];

	//. a party ainda esta publica? (pode ter sido desativada entre a
	//. listagem no cliente e o clique em "Entrar")
	if (info->IsPublic == false)
	{
		*outResult = PARTY_MATCH_JOIN_NOT_PUBLIC;
		return false;
	}

	//. senha correta?
	if (this->CheckPublicPassword(lpLeader->PartyNumber, password) == false)
	{
		*outResult = PARTY_MATCH_JOIN_WRONG_PASSWORD;
		return false;
	}

	//. tem vaga? (AddMember ja faz essa checagem internamente e falha
	//. retornando 0, mas conferimos antes para devolver o motivo certo)
	if (info->Count >= gServerInfo.m_PartyMaxMembers)
	{
		*outResult = PARTY_MATCH_JOIN_PARTY_FULL;
		return false;
	}

	if (this->AddMember(lpLeader->PartyNumber, joiningIndex) == 0)
	{
		*outResult = PARTY_MATCH_JOIN_PARTY_FULL;
		return false;
	}

	*outResult = PARTY_MATCH_JOIN_OK;
	return true;
}

//. ----------------------------------------------------------
//. 0xBA:01 (Client -> GameServer) - pedido de lista
//. ----------------------------------------------------------
void CParty::CGPartyMatchListRequestRecv(PMSG_PARTY_MATCH_LIST_REQUEST_RECV* lpMsg, int aIndex)
{
	if (gObjIsConnectedGP(aIndex) == 0)
	{
		return;
	}

	this->GCPartyMatchListSend(aIndex);
}

//. ----------------------------------------------------------
//. 0xBA:02 (GameServer -> Client) - resposta da lista
//. ----------------------------------------------------------
void CParty::GCPartyMatchListSend(int aIndex)
{
	BYTE send[sizeof(PMSG_PARTY_MATCH_LIST_SEND) + (sizeof(PMSG_PARTY_MATCH_ENTRY) * MAX_PARTY_MATCH_LIST)];

	PMSG_PARTY_MATCH_LIST_SEND pMsg;

	pMsg.header.set(0xBA, 0x02, 0);

	int size = sizeof(pMsg);

	pMsg.count = 0;

	for (int index = 0; index < MAX_OBJECT; index++)
	{
		if (pMsg.count >= MAX_PARTY_MATCH_LIST)
		{
			break; //. lista cheia, nao manda mais
		}

		if (this->IsParty(index) == 0)
		{
			continue;
		}

		if (this->m_PartyInfo[index].IsPublic == false)
		{
			continue;
		}

		int leaderIndex = this->m_PartyInfo[index].Index[0];

		if (gObjIsConnectedGP(leaderIndex) == 0)
		{
			continue; //. lider caiu/desconectou mas a party ainda nao foi destruida
		}

		LPOBJ lpLeader = &gObj[leaderIndex];

		PMSG_PARTY_MATCH_ENTRY entry;

		memcpy(entry.LeaderName, lpLeader->Name, sizeof(entry.LeaderName));
		entry.LeaderLevel = (WORD)lpLeader->Level;
		entry.LeaderClass = (BYTE)lpLeader->Class;
		entry.MapNumber = lpLeader->Map;
		entry.MemberCount = (BYTE)this->m_PartyInfo[index].Count;
		entry.MaxMembers = (BYTE)gServerInfo.m_PartyMaxMembers;
		entry.HasPassword = this->m_PartyInfo[index].HasPublicPwd ? 1 : 0;
		entry.PartyLeaderIndex = (DWORD)leaderIndex;

		memcpy(&send[size], &entry, sizeof(entry));
		size += sizeof(entry);

		pMsg.count++;
	}

	pMsg.header.size[0] = SET_NUMBERHB(size);
	pMsg.header.size[1] = SET_NUMBERLB(size);

	memcpy(send, &pMsg, sizeof(pMsg));

	DataSend(aIndex, send, size);
}

//. ----------------------------------------------------------
//. 0xBA:03 (Client -> GameServer) - ativar/desativar publica
//. ----------------------------------------------------------
void CParty::CGPartyMatchToggleRecv(PMSG_PARTY_MATCH_TOGGLE_RECV* lpMsg, int aIndex)
{
	if (gObjIsConnectedGP(aIndex) == 0)
	{
		return;
	}

	BYTE result = PARTY_MATCH_TOGGLE_OK;

	bool enable = (lpMsg->enable != 0);
	bool usePassword = (lpMsg->usePassword != 0);

	char password[PARTY_MATCH_PWD_LEN];
	memcpy(password, lpMsg->password, sizeof(password));
	password[sizeof(password) - 1] = '\0';

	this->TogglePublic(aIndex, enable, usePassword, password, &result);

	bool currentlyEnabled = this->IsPublic(gObj[aIndex].PartyNumber);

	this->GCPartyMatchToggleSend(aIndex, result, currentlyEnabled ? 1 : 0);
}

//. ----------------------------------------------------------
//. 0xBA:04 (GameServer -> Client) - resultado do toggle
//. ----------------------------------------------------------
void CParty::GCPartyMatchToggleSend(int aIndex, BYTE result, BYTE enabled)
{
	PMSG_PARTY_MATCH_TOGGLE_SEND pMsg;

	pMsg.header.set(0xBA, 0x04, sizeof(pMsg));
	pMsg.result = result;
	pMsg.enabled = enabled;

	DataSend(aIndex, (BYTE*)&pMsg, pMsg.header.size);
}

//. ----------------------------------------------------------
//. 0xBA:05 (Client -> GameServer) - pedido de entrada
//. ----------------------------------------------------------
void CParty::CGPartyMatchJoinRecv(PMSG_PARTY_MATCH_JOIN_RECV* lpMsg, int aIndex)
{
	if (gObjIsConnectedGP(aIndex) == 0)
	{
		return;
	}

	BYTE result = PARTY_MATCH_JOIN_OK;

	char password[PARTY_MATCH_PWD_LEN];
	memcpy(password, lpMsg->password, sizeof(password));
	password[sizeof(password) - 1] = '\0';

	int leaderIndex = (int)lpMsg->PartyLeaderIndex;

	this->JoinPublicParty(leaderIndex, aIndex, password, &result);

	this->GCPartyMatchJoinSend(aIndex, result);
}

//. ----------------------------------------------------------
//. 0xBA:06 (GameServer -> Client) - resultado da entrada
//. ----------------------------------------------------------
void CParty::GCPartyMatchJoinSend(int aIndex, BYTE result)
{
	PMSG_PARTY_MATCH_JOIN_SEND pMsg;

	pMsg.header.set(0xBA, 0x06, sizeof(pMsg));
	pMsg.result = result;

	DataSend(aIndex, (BYTE*)&pMsg, pMsg.header.size);
}
