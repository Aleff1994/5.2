#include "StdAfx.h"
#include "User.h"
#include "Util.h"
#include "MemScript.h"
#include "ItemOption.h"
#include "ItemManager.h"
#include "CGMFlagNatManager.h"

CGMFlagNatManager::CGMFlagNatManager()
{
}

CGMFlagNatManager::~CGMFlagNatManager()
{
	this->m_ItemOption.clear();
}

CGMFlagNatManager* CGMFlagNatManager::Instance()
{
	static CGMFlagNatManager s_Instance;
	return &s_Instance;
}

void CGMFlagNatManager::Load(char* filename)
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
			CUSTOM_FLAG_INFO info = {};
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
			this->m_ItemOption.insert(std::pair<int, CUSTOM_FLAG_INFO>(info.ItemIndex, info));
		}
	}
	catch (...)
	{
		ErrorMessageBox(lpMemScript->GetLastError());
	}

	delete lpMemScript;
}
bool CGMFlagNatManager::CheckFlagByItem(int ItemIndex)
{
	std::map<int, CUSTOM_FLAG_INFO>::iterator it = this->m_ItemOption.find(ItemIndex);

	return (it != this->m_ItemOption.end());
}

int CGMFlagNatManager::GetItemIncDamageRate(int ItemIndex)
{
	std::map<int, CUSTOM_FLAG_INFO>::iterator it = this->m_ItemOption.find(ItemIndex);

	if (it != this->m_ItemOption.end())
	{
		return it->second.IncDamageRate;
	}

	return 0;
}

int CGMFlagNatManager::GetItemDecDamageRate(int ItemIndex)
{
	std::map<int, CUSTOM_FLAG_INFO>::iterator it = this->m_ItemOption.find(ItemIndex);

	if (it != this->m_ItemOption.end())
	{
		return it->second.DecDamageRate;
	}

	return 0;
}

void CGMFlagNatManager::CalcItemCommonOption(LPOBJ lpObj, bool flag)
{
		CItem* pItem = &lpObj->Inventory[EQUIPMENT_FLAG_NAT];

		if (pItem->IsItem() != 0 && pItem->m_IsValidItem != 0 && pItem->m_Durability != 0)
		{
			this->InsertOption(lpObj, pItem, flag);
		}
}

void CGMFlagNatManager::InsertOption(LPOBJ lpObj, CItem* lpItem, bool flag)
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

void CGMFlagNatManager::UpdateInfo(int Index)
{
	this->CurrentInfo = this->m_ItemOption.find(Index);
}

int CGMFlagNatManager::GetIndex(int index)
{
	if (this->CurrentInfo != this->m_ItemOption.end())
	{
		return this->CurrentInfo->second.Opt[index];
	}
	return 0;
}

int CGMFlagNatManager::GetValue(int index)
{
	if (this->CurrentInfo != this->m_ItemOption.end())
	{
		return this->CurrentInfo->second.Val[index];
	}
	return 0;
}

