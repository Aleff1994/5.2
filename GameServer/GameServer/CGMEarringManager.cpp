#include "StdAfx.h"
#include "User.h"
#include "Util.h"
#include "MemScript.h"
#include "ItemOption.h"
#include "ItemManager.h"
#include "CGMEarringManager.h"

CGMEarringManager::CGMEarringManager()
{
}

CGMEarringManager::~CGMEarringManager()
{
	this->m_ItemOption.clear();
}

CGMEarringManager* CGMEarringManager::Instance()
{
	static CGMEarringManager s_Instance;
	return &s_Instance;
}

void CGMEarringManager::Load(char* filename)
{
	CMemScript* lpMemScript = new CMemScript;
	this->m_ItemOption.clear();

	if (lpMemScript == 0 || lpMemScript->SetBuffer(filename) == 0)
	{
		ErrorMessageBox((lpMemScript == 0) ? MEM_SCRIPT_ALLOC_ERROR : lpMemScript->GetLastError());
		delete lpMemScript;
		return;
	}

	try
	{
		while (true)
		{
			if (lpMemScript->GetToken() == TOKEN_END || strcmp("end", lpMemScript->GetString()) == 0) break;
			CUSTOM_EARRING_INFO info = {};
			int type = lpMemScript->GetNumber();
			int index = lpMemScript->GetAsNumber();
			info.ItemIndex = SafeGetItem(GET_ITEM(type, index));
		info.IncDamageRate = 0;
		info.DecDamageRate = 0;
			for (int n = 0; n < MAX_ADD_OPTION_INFO; n++)
			{
				info.Opt[n] = lpMemScript->GetAsNumber();
				info.Val[n] = lpMemScript->GetAsNumber();
			}
			this->m_ItemOption.insert(std::pair<int, CUSTOM_EARRING_INFO>(info.ItemIndex, info));
		}
	}
	catch (...)
	{
		ErrorMessageBox(lpMemScript->GetLastError());
	}

	delete lpMemScript;
}
bool CGMEarringManager::CheckFlagByItem(int ItemIndex)
{
	std::map<int, CUSTOM_EARRING_INFO>::iterator it = this->m_ItemOption.find(ItemIndex);

	return (it != this->m_ItemOption.end());
}

int CGMEarringManager::GetItemIncDamageRate(int ItemIndex)
{
	std::map<int, CUSTOM_EARRING_INFO>::iterator it = this->m_ItemOption.find(ItemIndex);

	if (it != this->m_ItemOption.end())
	{
		return it->second.IncDamageRate;
	}

	return 0;
}

int CGMEarringManager::GetItemDecDamageRate(int ItemIndex)
{
	std::map<int, CUSTOM_EARRING_INFO>::iterator it = this->m_ItemOption.find(ItemIndex);

	if (it != this->m_ItemOption.end())
	{
		return it->second.DecDamageRate;
	}

	return 0;
}

void CGMEarringManager::CalcItemCommonOption(LPOBJ lpObj, bool flag)
{
	int _inv[] = { EQUIPMENT_ERING_R, EQUIPMENT_ERING_L, -1 };

	for (int i = 0; _inv[i] != -1; i++)
	{
		int n = _inv[i];

		CItem* pItem = &lpObj->Inventory[n];

		if (pItem->IsItem() != 0 && pItem->m_IsValidItem != 0 && pItem->m_Durability != 0)
		{
			this->InsertOption(lpObj, pItem, flag);
		}
	}
}

void CGMEarringManager::InsertOption(LPOBJ lpObj, CItem* lpItem, bool flag)
{
	this->UpdateInfo(lpItem->m_Index);

	if (this->CurrentInfo != this->m_ItemOption.end())
	{
		for (size_t i = 0; i < MAX_ADD_OPTION_INFO; i++)
		{
			int index = this->GetIndex(i);

			int value = this->GetValue(i);

			if (index == 0 || value == 0)
				continue;

			gItemOption.AddOption(lpObj, index, value, lpItem, flag);
		}
	}
}

void CGMEarringManager::UpdateInfo(int Index)
{
	this->CurrentInfo = this->m_ItemOption.find(Index);
}

int CGMEarringManager::GetIndex(int index)
{
	if (this->CurrentInfo != this->m_ItemOption.end())
	{
		return this->CurrentInfo->second.Opt[index];
	}
	return 0;
}

int CGMEarringManager::GetValue(int index)
{
	if (this->CurrentInfo != this->m_ItemOption.end())
	{
		return this->CurrentInfo->second.Val[index];
	}
	return 0;
}
