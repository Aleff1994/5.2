#include "StdAfx.h"
#include "Util.h"
#include "Log.h"
#include "Notice.h"
#include "Message.h"
#include "NpcTalk.h"
#include "GameMain.h"
#include "ItemOption.h"
#include "ItemManager.h"
#include "RandomManager.h"
#include "SocketManager.h"
#include "ItemOptionRate.h"
#include "CMixGoblinExpansion.h"
#include "MemScript.h"

#ifdef CHAOS_MACHINE_EXTENSION
CMixGoblinExpansion gpMixGoblinExpansion;

CMixGoblinExpansion::CMixGoblinExpansion() :Npc(238), Map(3), PosX(185), PosY(105)
{
}

CMixGoblinExpansion::~CMixGoblinExpansion()
{
	this->Release();
}

void CMixGoblinExpansion::Release()
{
	max_mix = 0;

	for (type_map_mixgoblin::iterator it = mixgoblin.begin(); it != mixgoblin.end(); it++)
	{
		it->second.mixRecipient.clear();
		it->second.mixSuccessfully.clear();
	}

	mixgoblin.clear();
}

void CMixGoblinExpansion::OpenFile(std::string filename)
{
	CMemScript* lpMemScript = new CMemScript;

	if (lpMemScript == 0 || lpMemScript->SetBuffer(filename.c_str()) == 0)
	{
		ErrorMessageBox("[MixExpansion] Error loading: %s", filename.c_str());
		delete lpMemScript;
		return;
	}

	this->Release();

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
				if (strcmp("end", lpMemScript->GetAsString()) == 0)
				{
					break;
				}

				if (section == 0)
				{
					this->Npc = lpMemScript->GetNumber();
					this->Map = lpMemScript->GetAsNumber();
					this->PosX = lpMemScript->GetAsNumber();
					this->PosY = lpMemScript->GetAsNumber();
				}
				else if (section == 1)
				{
					MIX_TEMPLATE info;
					info.m_iMixIndex = lpMemScript->GetNumber();
					info.m_iName = lpMemScript->GetAsString();
					for (int n = 0; n < MAX_ACCOUNT_LEVEL; n++) info.m_iSuccessRate[n] = lpMemScript->GetAsNumber();
					info.m_iRequiredType = lpMemScript->GetAsNumber();
					info.m_RequiredZen = lpMemScript->GetAsNumber();
					info.m_iMainItem = lpMemScript->GetAsNumber();
					info.TalismanOfChaos = lpMemScript->GetAsNumber();
					info.TalismanOfLuck = lpMemScript->GetAsNumber();
					info.TalismanLuckRate = lpMemScript->GetAsNumber();
					info.TalismanCnt = lpMemScript->GetAsNumber();
					if (info.m_iMixIndex > 0) this->mixgoblin.insert(type_map_mixgoblin::value_type(info.m_iMixIndex, info));
				}
				else if (section == 2)
				{
					int key = lpMemScript->GetNumber();
					MIX_RECIPIENT info;
					info.mainIndex = lpMemScript->GetAsNumber();
					info.minIndex = lpMemScript->GetAsNumber();
					info.maxIndex = lpMemScript->GetAsNumber();
					info.minLevel = lpMemScript->GetAsNumber();
					info.maxLevel = lpMemScript->GetAsNumber();
					info.minDurability = lpMemScript->GetAsNumber();
					info.maxDurability = lpMemScript->GetAsNumber();
					info.skill = lpMemScript->GetAsNumber();
					info.luck = lpMemScript->GetAsNumber();
					info.minOpc = lpMemScript->GetAsNumber();
					info.maxOpc = lpMemScript->GetAsNumber();
					info.exeItem = lpMemScript->GetAsNumber();
					info.accItem = lpMemScript->GetAsNumber();
					info.minCnt = lpMemScript->GetAsNumber();
					info.maxCnt = lpMemScript->GetAsNumber();
					this->push_back(key, 0, info);
				}
				else if (section == 3)
				{
					int key = lpMemScript->GetNumber();
					MIX_RECIPIENT info;
					memset(&info, 0, sizeof(info));
					info.minIndex = lpMemScript->GetAsNumber();
					info.maxIndex = lpMemScript->GetAsNumber();
					info.minLevel = lpMemScript->GetAsNumber();
					info.skill = lpMemScript->GetAsNumber();
					info.luck = lpMemScript->GetAsNumber();
					info.minOpc = lpMemScript->GetAsNumber();
					info.exeItem = lpMemScript->GetAsNumber();
					info.accItem = lpMemScript->GetAsNumber();
					info.minCnt = 1;
					info.maxCnt = 1;
					this->push_back(key, 1, info);
				}
			}
		}
	}
	catch (...)
	{
		ErrorMessageBox(lpMemScript->GetLastError());
	}

	delete lpMemScript;
	this->max_mix = this->mixgoblin.size();
	LogAdd(LOG_BLUE, "[MixExpansion] Loaded %d mixes from TXT", this->max_mix);
}

bool CMixGoblinExpansion::IsGoblinMix(int mainKey)
{
	type_map_mixgoblin::iterator it = mixgoblin.find(mainKey);

	return it != mixgoblin.end();
}

MIX_TEMPLATE* CMixGoblinExpansion::FindTemplate(int mixIndex)
{
	type_map_mixgoblin::iterator it = mixgoblin.find(mixIndex);

	if (it != mixgoblin.end())
		return &it->second;

	return NULL;
}

void CMixGoblinExpansion::push_back(int index, int Type, MIX_RECIPIENT info)
{
	type_map_mixgoblin::iterator it = mixgoblin.find(index);

	if (it != mixgoblin.end())
	{
		if (Type == 0)
		{
			it->second.mixRecipient.push_back(info);
		}
		else if (Type == 1)
		{
			it->second.mixSuccessfully.push_back(info);
		}
	}
}

void CMixGoblinExpansion::GDMixInventoryReq(LPOBJ lpObj)
{
	//if (gObjIsAccountValid(lpObj->Index, lpObj->Account) == 0
	//	|| lpObj->LoadMixInventory != 0)
	//{
	//	return;
	//}
	return;

	SDHP_REQUEST_MIX_GOBBLIN_ITEM pMsg;

	pMsg.header.set(0xF8, 0x00, sizeof(pMsg));

	pMsg.index = lpObj->Index;

	memcpy(pMsg.Account, lpObj->Account, sizeof(pMsg.Account));

	memcpy(pMsg.Name, lpObj->Name, sizeof(pMsg.Name));

	gDataServerConnection.DataSend((BYTE*)&pMsg, pMsg.header.size);
}

void CMixGoblinExpansion::GDMixInventorySave(LPOBJ lpObj)
{
	//if (lpObj->LoadMixInventory == 0)
	//{
	//	return;
	//}
	return;

	SDHP_MIX_GOBBLIN_ITEM_DEFAULT pMsg;

	pMsg.header.set(0xF8, 0x01, sizeof(pMsg));

	pMsg.index = lpObj->Index;

	pMsg.m_iMixIndex = lpObj->m_iMixIndex;

	memcpy(pMsg.Account, lpObj->Account, sizeof(pMsg.Account));

	memcpy(pMsg.iName, lpObj->Name, sizeof(pMsg.iName));

	for (int n = 0; n < MIX_INVENTORY_SIZE; n++)
	{
		gItemManager.DBItemByteConvert(pMsg.Inventory[n], &lpObj->InventoryMix[n]);
	}

	gDataServerConnection.DataSend((BYTE*)&pMsg, sizeof(pMsg));
}

void CMixGoblinExpansion::GDMixInventoryRecv(SDHP_MIX_GOBBLIN_ITEM_DEFAULT* Data)
{
	//if (gObjIsAccountValid(Data->index, Data->Account) == 0)
	//{
	//	LogAdd(LOG_RED, "[DGMixInventoryRecv] Invalid Account [%d](%s)", Data->index, Data->Account);
	//	CloseClient(Data->index);
	//	return;
	//}
	return;
	LPOBJ lpObj = &gObj[Data->index];

	if (lpObj->LoadMixInventory != 0)
	{
		return;
	}

	int count = 0;

	lpObj->LoadMixInventory = 1;

	memset(lpObj->InventoryMixMap, 0xFF, MIX_INVENTORY_SIZE);

	for (int n = 0; n < MIX_INVENTORY_SIZE; n++)
	{
		CItem item;

		lpObj->InventoryMix[n].Clear();

		if (gItemManager.ConvertItemByte(&item, Data->Inventory[n]) != 0)
		{
			if (gItemManager.MixExpansionAddItem(lpObj->Index, item, n) != 0xFF)
			{
				count++;
			}
		}
	}

	if (count == 0)
	{
		lpObj->m_iMixState = 0;
		lpObj->m_iMixIndex = 0xFF;
	}
	else
	{
		lpObj->m_iMixState = Data->m_iMixState;
		lpObj->m_iMixIndex = Data->m_iMixIndex;
	}

	this->InventoryMixList(lpObj);


	//lpObj->m_iMixState = 0;
	//lpObj->m_iMixIndex = 0xFF;
	this->MixRecipientIndex(lpObj, lpObj->m_iMixIndex);
}

void CMixGoblinExpansion::winMixInfo(LPOBJ lpObj)
{
	BYTE send[4096];

	PHEADER_MIX_DEFAULT pMsg;

	pMsg.Header.setE(0x4F, 0x02, 0);

	int size = sizeof(pMsg);

	pMsg.Value = 0;

	PMSG_MIX_ITEM_LIST info;

	for (type_map_mixgoblin::iterator it = mixgoblin.begin(); it != mixgoblin.end(); it++)
	{
		info.iMixIndex = it->second.m_iMixIndex;

		strcpy_s(info.iName, it->second.m_iName.c_str());

		info.iName[MAX_MIX_NAME_SIZE-1] = 0;

		memcpy(&send[size], &info, sizeof(info));

		size += sizeof(info);

		pMsg.Value++;
	}

	pMsg.Header.size[0] = SET_NUMBERHB(size);

	pMsg.Header.size[1] = SET_NUMBERLB(size);

	memcpy(send, &pMsg, sizeof(pMsg));

	DataSend(lpObj->Index, send, size);
}

void CMixGoblinExpansion::InventoryMixList(LPOBJ lpObj)
{
	BYTE send[4096];

	PHEADER_MIX_DEFAULT pMsg;

	pMsg.Header.setE(0x4F, 0x05, 0);

	int size = sizeof(pMsg);

	pMsg.Value = 0;

	PMSG_DEFAULT_ITEM_LIST info;

	for (int n = 0; n < MIX_INVENTORY_SIZE; n++)
	{
		if (lpObj->InventoryMix[n].IsItem() != 0 && lpObj->InventoryMix[n].m_ItemExist != 0)
		{
			info.slot = n;

			gItemManager.ItemByteConvert(info.ItemInfo, lpObj->InventoryMix[n]);

			memcpy(&send[size], &info, sizeof(info));
			size += sizeof(info);

			pMsg.Value++;
		}
	}

	pMsg.Header.size[0] = SET_NUMBERHB(size);
	pMsg.Header.size[1] = SET_NUMBERLB(size);

	memcpy(send, &pMsg, sizeof(pMsg));

	DataSend(lpObj->Index, send, size);
}

void CMixGoblinExpansion::MixRecipientIndex(LPOBJ lpObj, BYTE mixIndex)
{
	PHEADER_MIX_DEFAULT pMsg;

	pMsg.Header.setE(0x4F, 0x03, sizeof(PHEADER_MIX_DEFAULT));

	pMsg.Value = mixIndex;

	DataSend(lpObj->Index, (BYTE*)&pMsg, sizeof(pMsg));
}

void CMixGoblinExpansion::MixRecipientInfo(LPOBJ lpObj, MIX_TEMPLATE* MixTemplate)
{
	BYTE send[4096];

	PHEADER_MIX_INFORMATION pMsg;

	pMsg.Header.setE(0x4F, 0x04, 0);

	int size = sizeof(pMsg);

	pMsg.iSuccessRate = MixTemplate->m_iSuccessRate[lpObj->AccountLevel];

	pMsg.iTalismanOfLuck = MixTemplate->TalismanOfLuck;

	pMsg.iTalismanOfChaos = MixTemplate->TalismanOfChaos;

	pMsg.iTalismanLuckRate = MixTemplate->TalismanLuckRate;

	pMsg.iRequiredType = MixTemplate->m_iRequiredType;

	pMsg.iRequiredZen = MixTemplate->m_RequiredZen;

	std::vector<MIX_RECIPIENT>* s = &MixTemplate->mixRecipient;

	pMsg.Value = 0;

	for (int i = 0; i < s->size(); i++)
	{
		memcpy(&send[size], &((*s)[i]), sizeof(MIX_RECIPIENT));

		pMsg.Value++;

		size += sizeof(MIX_RECIPIENT);
	}

	pMsg.Header.size[0] = SET_NUMBERHB(size);

	pMsg.Header.size[1] = SET_NUMBERLB(size);

	memcpy(send, &pMsg, sizeof(pMsg));

	DataSend(lpObj->Index, send, size);
}

void CMixGoblinExpansion::ReceiveMixInfo(int aIndex, BYTE* ReceiveBuffer)
{
	LPOBJ lpObj = &gObj[aIndex];

	if (gObjIsConnectedGP(aIndex) == 0)
	{
		return;
	}

	LPPHEADER_MIX_DEFAULT Data = (LPPHEADER_MIX_DEFAULT)ReceiveBuffer;

	int mixIndex = Data->Value;

	MIX_TEMPLATE* Info = this->FindTemplate(mixIndex);

	if (Info == NULL)
	{
		lpObj->m_iMixIndex = 0xFF;
		this->MixRecipientIndex(lpObj, 0xFF);
	}
	else
	{
		lpObj->m_iMixIndex = mixIndex;
		this->MixRecipientInfo(lpObj, Info);
	}
}

bool CMixGoblinExpansion::TalkNpc(LPOBJ lpObj, int npc, int map, int Tx, int Ty)
{
	if (this->Npc == npc)
	{
		if (this->Map != -1 && this->Map != map)
		{
			return false;
		}

		if (this->PosX != -1 && this->PosX != Tx)
		{
			return false;
		}

		if (this->PosY != -1 && this->PosY != Ty)
		{
			return false;
		}

		PMSG_NPC_TALK_SEND pMsg;

		pMsg.header.setE(0x30, sizeof(pMsg));

		pMsg.result = 40;

		DataSend(lpObj->Index, (BYTE*)&pMsg, pMsg.header.size);

		lpObj->m_iMixState = 0;

		lpObj->m_iMixIndex = 0xFF;

		this->winMixInfo(lpObj);

		return true;
	}
	return false;
}

void CMixGoblinExpansion::Clear(LPOBJ lpObj)
{
	memset(lpObj->InventoryMixMap, 0xFF, MIX_INVENTORY_SIZE);

	for (int i = 0; i < MIX_INVENTORY_SIZE; i++)
	{
		lpObj->InventoryMix[i].Clear();
	}
}

bool CMixGoblinExpansion::CheckRecipe(CItem* pItem, MIX_RECIPIENT* pMixItems, RECIPIENT_CHECK* Check)
{
	int m_sType = pItem->m_Index;
	int m_iLevel = pItem->m_Level;
	int m_iDurability = (int)pItem->m_Durability;
	int m_iOption = 0;
	int m_bMixLuck = 0;
	int m_SpecialItem = 0;

	m_iOption = pItem->m_Option3;
	m_bMixLuck = pItem->m_Option2;

	if ((pItem->m_NewOption & 63) > 0)
		m_SpecialItem |= RCP_SP_EXCELLENT;
	if (pItem->m_RequireLevel >= 380)
		m_SpecialItem |= RCP_SP_ADD380ITEM;
	if (pItem->m_SetOption != 0)
		m_SpecialItem |= RCP_SP_SETITEM;

	if ((m_sType >= pMixItems->minIndex && m_sType <= pMixItems->maxIndex)
		&& m_iLevel >= pMixItems->minLevel && m_iLevel <= pMixItems->maxLevel
		&& m_iDurability >= pMixItems->minDurability && m_iDurability <= pMixItems->maxDurability
		&& m_iOption >= pMixItems->minOpc && m_iOption <= pMixItems->maxOpc
		&& (pMixItems->luck == 0 || (pMixItems->luck != 0 && m_bMixLuck))
		&& (pMixItems->exeItem == 0 || (pMixItems->exeItem != 0 && ((m_SpecialItem & RCP_SP_EXCELLENT) == RCP_SP_EXCELLENT)))
		&& (pMixItems->accItem == 0 || (pMixItems->accItem != 0 && ((m_SpecialItem & RCP_SP_SETITEM) == RCP_SP_SETITEM)))
		)
	{
		Check->SpecialItem = MIX_SOURCE_PARTIALLY;

		Check->iNumMixItem++;
		if (Check->iNumMixItem >= pMixItems->minCnt && Check->iNumMixItem <= pMixItems->maxCnt)
		{
			Check->SpecialItem = MIX_SOURCE_YES;
		}
		return true;
	}
	return false;
}

void CMixGoblinExpansion::GenerateMix(LPOBJ lpObj, MIX_TEMPLATE* info)
{
	int NumRecipient = info->mixRecipient.size();

	lpObj->ChaosMoney = info->m_RequiredZen;

	lpObj->ChaosSuccessRate = info->m_iSuccessRate[lpObj->AccountLevel];

	if (NumRecipient != 0)
	{
		RECIPIENT_CHECK* RecipientCheck = new RECIPIENT_CHECK[NumRecipient];

		int mainIndex = -1;
		int iLinealPos = -1;
		int TalismanOfLuck = 0;
		int TalismanOfChaos = 0;

		for (int i = 0; i < NumRecipient; i++)
		{
			RecipientCheck[i].iNumMixItem = 0;
			RecipientCheck[i].SpecialItem = MIX_SOURCE_ERROR;

			if (info->m_iMainItem != 0 && info->m_iMainItem == info->mixRecipient[i].mainIndex)
				mainIndex = i;
		}

		for (int i = 0; i < MIX_INVENTORY_SIZE; i++)
		{
			if (lpObj->InventoryMix[i].IsItem() != 0 && lpObj->InventoryMix[i].m_ItemExist != 0)
			{
				if (info->TalismanOfLuck != -1 && info->TalismanOfLuck == lpObj->InventoryMix[i].m_Index)
				{
					TalismanOfLuck++;
				}
				if (info->TalismanOfChaos != -1 && info->TalismanOfChaos == lpObj->InventoryMix[i].m_Index)
				{
					TalismanOfChaos++;
				}

				for (int j = 0; j < NumRecipient; j++)
				{
					if (this->CheckRecipe(&lpObj->InventoryMix[i], &info->mixRecipient[j], &RecipientCheck[j]))
					{
						if (mainIndex == j)
						{
							iLinealPos = i;
						}
					}
				}
			}
		}

		if (TalismanOfLuck > 0 && TalismanOfLuck <= info->TalismanCnt)
		{
			lpObj->ChaosSuccessRate += TalismanOfLuck* info->TalismanLuckRate;
		}

		if (lpObj->ChaosSuccessRate > 100)
			lpObj->ChaosSuccessRate = 100;

		if ((GetLargeRand() % 100) < lpObj->ChaosSuccessRate)
		{
			CItem item;
			BYTE ItemOption1;
			BYTE ItemOption2;
			BYTE ItemOption3;
			BYTE ItemNewOption;
			BYTE ItemAncOption;

			if (iLinealPos != -1)
			{
				item = lpObj->InventoryMix[iLinealPos];

				MIX_RECIPIENT temp = info->mixSuccessfully[0];

				ItemOption1 = item.m_Option1;
				ItemOption2 = item.m_Option2;
				ItemOption3 = item.m_Option3;
				ItemNewOption = item.m_NewOption;
				ItemAncOption = item.m_SetOption;

				if(temp.minLevel != 0xFF)
					item.m_Level = temp.minLevel;

				if (temp.skill != 0xFF)
					ItemOption1 = temp.skill;

				if (temp.luck != 0xFF)
					ItemOption2 = temp.luck;

				if (temp.minOpc != 0xFF)
					ItemOption3 = temp.minOpc;

				if (temp.exeItem != 0xFF)
					ItemNewOption = temp.exeItem;

				if (temp.accItem != 0xFF)
					ItemAncOption = temp.accItem;

				item.m_Durability = (float)(gItemManager.GetItemDurability(item.m_Index, item.m_Level, ItemNewOption, 0));

				item.Convert(item.m_Index, ItemOption1, ItemOption2, ItemOption3, ItemNewOption, ItemAncOption, item.m_JewelOfHarmonyOption, item.m_ItemOptionEx, item.m_SocketOption, item.m_SocketOptionBonus);
			}
			else
			{
				item.Clear();

				int index = 0;
				int Size = info->mixSuccessfully.size();

				if (Size > 1)
				{
					index = (GetLargeRand() % Size);
				}

				int ItemIndex = 0;
				BYTE ItemLevel = 0;

				MIX_RECIPIENT temp = info->mixSuccessfully[index];

				if (temp.minIndex == temp.maxIndex)
				{
					ItemIndex = temp.minIndex;
				}
				else if (temp.minIndex >= 0 && temp.maxIndex < 0x1FFF)
				{
					ITEM_INFO ItemInfo;

					while (true)
					{
						ItemIndex = (GetLargeRand() % temp.maxIndex);

						if (gItemManager.GetInfo(ItemIndex, &ItemInfo) != 0)
						{
							break;
						}
					}
				}

				if (temp.minLevel != 0xFF)
					ItemLevel = temp.minLevel;
				else
					gItemOptionRate.GetItemOption0(1, &ItemLevel);

				if (temp.skill != 0xFF)
					ItemOption1 = temp.skill;
				else
					gItemOptionRate.GetItemOption1(1, &ItemOption1);

				if (temp.luck != 0xFF)
					ItemOption2 = temp.luck;
				else
					gItemOptionRate.GetItemOption2(1, &ItemOption2);

				if (temp.minOpc != 0xFF)
					ItemOption3 = temp.minOpc;
				else
					gItemOptionRate.GetItemOption3(1, &ItemOption3);

				if (temp.exeItem != 0xFF)
					ItemNewOption = temp.exeItem;
				else
					gItemOptionRate.GetItemOption4(4, &ItemNewOption);

				if (temp.accItem != 0xFF)
					ItemAncOption = temp.accItem;
				else
					gItemOptionRate.GetItemOption5(2, &ItemAncOption);

				BYTE ItemSocketOption[MAX_SOCKET_OPTION] = { 0xFF,0xFF,0xFF,0xFF,0xFF };

				item.m_Index = ItemIndex;

				item.m_Level = ItemLevel;

				item.m_Durability = (float)(gItemManager.GetItemDurability(ItemIndex, ItemLevel, ItemNewOption, 0));

				gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "[ItemIndex: %d][ItemLevel: %d][ItemNewOption: %d]", ItemIndex, ItemLevel, ItemNewOption);

				//gItemOptionRate.GetItemOption6(((gItemManager.GetItemTwoHand(ItemIndex) == 0) ? 3 : 4), &ItemSocketOption[0]);

				item.Convert(ItemIndex, ItemOption1, ItemOption2, ItemOption3, ItemNewOption, ItemAncOption, 0, 0, ItemSocketOption, 0xFF);
			}

			this->Clear(lpObj);

			if (gItemManager.MixExpansionAddItem(lpObj->Index, item, 0) == 0xFF)
			{
				gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "[Error al agregar el item]");
			}

			gLog.Output(LOG_CHAOS_MIX, "[%s][Success][%s][%s] - (ChaosSuccessRate: %d, ChaosMoney: %d)", info->m_iName.c_str(), lpObj->Account, lpObj->Name, lpObj->ChaosSuccessRate, lpObj->ChaosMoney);
		}
		else
		{
			this->Clear(lpObj);

			gLog.Output(LOG_CHAOS_MIX, "[%s][Failure][%s][%s] - (ChaosSuccessRate: %d, ChaosMoney: %d)", info->m_iName.c_str(), lpObj->Account, lpObj->Name, lpObj->ChaosSuccessRate, lpObj->ChaosMoney);

		}

		delete [] RecipientCheck;
	}
}

void CMixGoblinExpansion::CreateMix(int aIndex, BYTE* ReceiveBuffer)
{
	LPOBJ lpObj = &gObj[aIndex];

	if (gObjIsConnectedGP(aIndex) == 0)
	{
		return;
	}

	if (lpObj->m_iMixIndex != -1)
	{
		int mixIndex = lpObj->m_iMixIndex;

		MIX_TEMPLATE* info = this->FindTemplate(mixIndex);

		if (info != nullptr)
		{
			this->GenerateMix(lpObj, info);
		}
		else
		{
			this->Clear(lpObj);
		}
		this->InventoryMixList(lpObj);
	}
}

#endif // CHAOS_MACHINE_EXTENSION
