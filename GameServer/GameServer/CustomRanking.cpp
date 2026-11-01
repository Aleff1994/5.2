#include "stdafx.h"
#include "DSProtocol.h"
#include "GameMain.h"
#include "Util.h"
#include "Message.h"
#include "User.h"
#include "Path.h"
#include "ServerInfo.h"
#include "MemScript.h"
#include "CustomRanking.h"
#include "ObjectManager.h"
#include "Notice.h"
#include "SetItemOption.h"

CCustomRanking gCustomRanking;

void CCustomRanking::Load(char* path)
{

	CMemScript* lpMemScript = new CMemScript;

	if (lpMemScript == 0)
	{
		ErrorMessageBox(MEM_SCRIPT_ALLOC_ERROR, path);
		return;
	}

	if (lpMemScript->SetBuffer(path) == 0)
	{
		ErrorMessageBox(lpMemScript->GetLastError());
		delete lpMemScript;
		return;
	}

	for (int n = 0; n < MAX_RANK; n++)
	{
		this->r_Data[n];
	}

	this->m_count = 0;

	try
	{
		while (true)
		{
			if (lpMemScript->GetToken() == TOKEN_END)
			{
				break;
			}

			int section = lpMemScript->GetNumber();

			while (true)
			{
				if (section == 0)
				{
					if (strcmp("end", lpMemScript->GetAsString()) == 0)
					{
						break;
					}

					int index = lpMemScript->GetNumber();

					strcpy_s(this->r_Data[index].Name, lpMemScript->GetAsString());

					strcpy_s(this->r_Data[index].Col1, lpMemScript->GetAsString());

					strcpy_s(this->r_Data[index].Col2, lpMemScript->GetAsString());

					this->m_count++;
				}
			}
		}
	}
	catch (...)
	{
		ErrorMessageBox(lpMemScript->GetLastError());
	}

	delete lpMemScript;
}

void CCustomRanking::GCReqRankingCount(int Index, PMSG_CUSTOM_RANKING_COUNT_RECV* lpMsg)
{
#if (GAMESERVER_CLIENTE_UPDATE >= 6)
	if (gServerInfo.m_CustomRankingSwitch == 0)
	{
		return;
	}

	PMSG_CUSTOM_RANKING_COUNT_SEND pMsg;

	pMsg.header.set(0xF3, 0xE7, sizeof(pMsg));

	pMsg.count = (this->m_count > MAX_RANK) ? MAX_RANK : this->m_count;

	DataSend(Index, (LPBYTE)&pMsg, pMsg.header.size);
#endif
}

void CCustomRanking::GCReqRanking(int Index, PMSG_CUSTOM_RANKING_RECV* lpMsg)
{
#if (GAMESERVER_CLIENTE_UPDATE >= 6)
	if (gObjIsConnected(Index) != false)
	{
		if (gServerInfo.m_CustomRankingSwitch == 0)
		{
			return;
		}

		if (this->m_count <= 0)
		{
			return;
		}

		if (lpMsg->type < 0 || lpMsg->type >= this->m_count)
		{
			return;
		}

		if (this->m_count == 0)
		{
			return;
		}

		SDHP_CUSTOM_RANKING_SEND pMsg;

		pMsg.header.set(0xF4, sizeof(pMsg));

		pMsg.index = Index;

		pMsg.type = lpMsg->type;

		gDataServerConnection.DataSend((BYTE*)&pMsg, pMsg.header.size);
	}
#endif
}

void CCustomRanking::GDCustomRankingRecv(BYTE* ReceiveBuffer)
{
#if (GAMESERVER_CLIENTE_UPDATE >= 6)
	if (ReceiveBuffer == 0)
	{
		return;
	}

	SDHP_CUSTOM_RANKING_RECV* Data = (SDHP_CUSTOM_RANKING_RECV*)ReceiveBuffer;
	if (Data->type < 0 || Data->type >= this->m_count || Data->type >= MAX_RANK ||
		gObjIsConnected(Data->index) == false)
	{
		return;
	}

	BYTE send[4096];
	PMSG_CUSTOM_RANKING_SEND pMsg;
	memset(send, 0, sizeof(send));
	memset(&pMsg, 0, sizeof(pMsg));
	pMsg.header.set(0xF3, 0xE6, 0);
	pMsg.RankIndex = Data->type;
	memcpy(pMsg.rankname, this->r_Data[Data->type].Name, sizeof(pMsg.rankname));
	memcpy(pMsg.col1, this->r_Data[Data->type].Col1, sizeof(pMsg.col1));
	memcpy(pMsg.col2, this->r_Data[Data->type].Col2, sizeof(pMsg.col2));
	int size = sizeof(pMsg);
	int offset = sizeof(SDHP_CUSTOM_RANKING_RECV);
	pMsg.count = 0;
	int maxCount = (sizeof(send) - sizeof(pMsg)) / sizeof(CUSTOM_RANKING_DATA);
	int safeCount = (Data->count < 0) ? 0 : ((Data->count > maxCount) ? maxCount : Data->count);
	for (int n = 0; n < safeCount; n++)
	{
		CUSTOM_RANKING_DATA* Data2 = (CUSTOM_RANKING_DATA*)(ReceiveBuffer + offset);
		CUSTOM_RANKING_DATA info;
		memset(&info, 0, sizeof(info));
		memcpy(info.szName, Data2->szName, sizeof(info.szName));
		info.szName[sizeof(info.szName) - 1] = 0;
		info.Score = Data2->Score;
		info.Vip = Data2->Vip;
		info.Class = (BYTE)gCharacterManager.GetCharacterClass(Data2->Class);
		memcpy(&send[size], &info, sizeof(CUSTOM_RANKING_DATA));
		size += sizeof(CUSTOM_RANKING_DATA);
		offset += (sizeof(CUSTOM_RANKING_DATA));
		pMsg.count++;
	}
	pMsg.header.size[0] = SET_NUMBERHB(size);
	pMsg.header.size[1] = SET_NUMBERLB(size);
	memcpy(send, &pMsg, sizeof(pMsg));
	DataSend(Data->index, send, size);
#endif
}

// ==================== NOVO: Photo/Preview do Ranking ====================

void CCustomRanking::GCReqRankingPhoto(int Index, PMSG_CUSTOM_RANKING_PHOTO_RECV* lpMsg)
{
#if (GAMESERVER_CLIENTE_UPDATE >= 6)
	if (gObjIsConnected(Index) == false)
	{
		return;
	}
	if (gServerInfo.m_CustomRankingSwitch == 0)
	{
		return;
	}

	char safeName[11] = { 0 };
	memcpy(safeName, lpMsg->Name, 10);
	if (safeName[0] == 0)
	{
		return;
	}
	for (int n = 0; safeName[n] != 0; n++)
	{
		unsigned char value = (unsigned char)safeName[n];
		if (isalnum(value) == 0 && value != '_' && value != '-')
		{
			LogAdd(LOG_RED, "[CustomRanking] Nome invalido na consulta de aparencia: %s", gObj[Index].Name);
			return;
		}
	}

	SDHP_CUSTOM_RANKING_PHOTO_SEND pMsg;
	pMsg.header.set(0xF9, sizeof(pMsg));
	pMsg.index = Index;
	memcpy(pMsg.Name, safeName, sizeof(pMsg.Name));

	gDataServerConnection.DataSend((BYTE*)&pMsg, pMsg.header.size);
#endif
}

void CCustomRanking::GDCustomRankingPhotoRecv(BYTE* ReceiveBuffer)
{
#if (GAMESERVER_CLIENTE_UPDATE >= 6)
	SDHP_CUSTOM_RANKING_PHOTO_RECV* Data = (SDHP_CUSTOM_RANKING_PHOTO_RECV*)ReceiveBuffer;

	PMSG_CUSTOM_RANKING_PHOTO_SEND pMsg;
	pMsg.header.set(0xF3, 0xE9, sizeof(pMsg));
	pMsg.result = Data->result;
	pMsg.Class = 0;
	memset(pMsg.Equipment, 0, sizeof(pMsg.Equipment));

	if (Data->result == 1)
	{
		pMsg.Class = gCharacterManager.GetCharacterClass(Data->Class);

		PART_t player_body[12];

		for (int l = 0; l < 12; l++)
		{
			player_body[l] = Data->Inventory[l];
			pMsg.Equipment[l] = player_body[l].GetPackedData();
			pMsg.Equipment[l] |= (l & 0x1F);
		}

		DWORD mask = ~(0x03 << 7);
		int pos = EQUIPMENT_HELPER;

		bool IsFullSet = gSetItemOption.IsFullSet(player_body, 12);

		if (pMsg.Equipment[pos] != 0 && pMsg.Equipment[pos] != 0xffffffff)
		{
			pMsg.Equipment[pos] &= mask;
		}

		if (IsFullSet == true)
			player_body[pos].ExtOption = 1;
		else
			player_body[pos].ExtOption = 0;

		pMsg.Equipment[pos] |= (player_body[pos].ExtOption & 0x03) << 7;
	}

	DataSend(Data->index, (BYTE*)&pMsg, sizeof(pMsg));
#endif
}

// ==========================================================================
