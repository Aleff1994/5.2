#include "StdAfx.h"
#include "User.h"
#include "Util.h"
#include "MemScript.h"
#include "ItemOption.h"
#include "ItemManager.h"
#include "CGMPetManager.h"

CGMPetManager::CGMPetManager()
{
}

CGMPetManager::~CGMPetManager()
{
	this->m_ItemOption.clear();
}

CGMPetManager* CGMPetManager::Instance()
{
	static CGMPetManager s_Instance;
	return &s_Instance;
}

void CGMPetManager::Load(char* filename)
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
			CUSTOM_PET_INFO info = {};
			int type = lpMemScript->GetNumber();
			int index = lpMemScript->GetAsNumber();
			info.ItemIndex = SafeGetItem(GET_ITEM(type, index));
		info.IncDamageRate = lpMemScript->GetAsNumber();
		info.DecDamageRate = lpMemScript->GetAsNumber();
			for (int n = 0; n < MAX_ADD_OPTION_INFO; n++)
			{
				info.Opt[n] = lpMemScript->GetAsNumber();
				info.Val[n] = lpMemScript->GetAsNumber();
			}
			this->m_ItemOption.insert(std::pair<int, CUSTOM_PET_INFO>(info.ItemIndex, info));
		}
	}
	catch (...)
	{
		ErrorMessageBox(lpMemScript->GetLastError());
	}

	delete lpMemScript;
}
bool CGMPetManager::CheckPetByItem(int ItemIndex)
{
	std::map<int, CUSTOM_PET_INFO>::iterator it = this->m_ItemOption.find(ItemIndex);

	return (it != this->m_ItemOption.end());
}

bool CGMPetManager::PetIsInmortal(int ItemIndex)
{
	std::map<int, CUSTOM_PET_INFO>::iterator it = this->m_ItemOption.find(ItemIndex);

	if (it != this->m_ItemOption.end())
	{
		return (it->second.Inmortal != 0);
	}

	return false;
}

int CGMPetManager::GetItemPetIncDamageRate(int ItemIndex)
{
	std::map<int, CUSTOM_PET_INFO>::iterator it = this->m_ItemOption.find(ItemIndex);

	if (it != this->m_ItemOption.end())
	{
		return it->second.IncDamageRate;
	}

	return 0;
}

int CGMPetManager::GetItemPetDecDamageRate(int ItemIndex)
{
	std::map<int, CUSTOM_PET_INFO>::iterator it = this->m_ItemOption.find(ItemIndex);

	if (it != this->m_ItemOption.end())
	{
		return it->second.DecDamageRate;
	}

	return 0;
}

void CGMPetManager::CalcItemCommonOption(LPOBJ lpObj, bool flag)
{
	int _inv[] = { EQUIPMENT_HELPER, EQUIPMENT_EAGLE, EQUIPMENT_MUUN, -1 };

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

void CGMPetManager::InsertOption(LPOBJ lpObj, CItem* lpItem, bool flag)
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

void CGMPetManager::UpdateInfo(int Index)
{
	this->CurrentInfo = this->m_ItemOption.find(Index);
}

int CGMPetManager::GetIndex(int index)
{
	if (this->CurrentInfo != this->m_ItemOption.end())
	{
		return this->CurrentInfo->second.Opt[index];
	}
	return 0;
}

int CGMPetManager::GetValue(int index)
{
	if (this->CurrentInfo != this->m_ItemOption.end())
	{
		return this->CurrentInfo->second.Val[index];
	}
	return 0;
}


