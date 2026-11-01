// ItemBagManager.cpp: implementation of the CItemBagManager class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ItemBagManager.h"
#include "MemScript.h"
#include "Path.h"
#include "Util.h"

CItemBagManager gItemBagManager;
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CItemBagManager::CItemBagManager() // OK
{
	this->m_ItemBagManagerInfo.clear();
}

CItemBagManager::~CItemBagManager() // OK
{

}

void CItemBagManager::Load(char* path) // OK
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

	this->m_ItemBagManagerInfo.clear();

	try
	{
		while (true)
		{
			if (lpMemScript->GetToken() == TOKEN_END)
			{
				break;
			}

			if (strcmp("end", lpMemScript->GetString()) == 0)
			{
				break;
			}

			ITEM_BAG_MANAGER_INFO info;

			info.Index = lpMemScript->GetNumber();

			info.ItemIndex = lpMemScript->GetAsNumber();

			info.ItemLevel = lpMemScript->GetAsNumber();

			info.MonsterClass = lpMemScript->GetAsNumber();

			info.SpecialValue = lpMemScript->GetAsNumber();

			this->m_ItemBagManagerInfo.insert(std::pair<int, ITEM_BAG_MANAGER_INFO>(info.Index, info));
		}
	}
	catch (...)
	{
		ErrorMessageBox(lpMemScript->GetLastError());
	}

	delete lpMemScript;
}

void CItemBagManager::LoadEventItemBag() // OK
{
	std::map<int, int> LoadEventItemBag;

	// Keep the same EventItemBag layout used by UP35.
	this->LoadEventItemBagFolder("EventItemBag\\Item", LoadEventItemBag);
	this->LoadEventItemBagFolder("EventItemBag\\Monster", LoadEventItemBag);
	this->LoadEventItemBagFolder("EventItemBag\\Special", LoadEventItemBag);
	this->LoadEventItemBagFolder("EventItemBag\\ChaosMix", LoadEventItemBag);
}

void CItemBagManager::LoadEventItemBagFolder(const char* relativeFolder, std::map<int, int>& loadedBags) // OK
{
	char wildcard_path[MAX_PATH];
	char search_path[MAX_PATH];

	wsprintf(search_path, "%s\\", relativeFolder);
	wsprintf(wildcard_path, "%s*", gPath.GetFullPath(search_path));

	WIN32_FIND_DATA data;

	HANDLE file = FindFirstFile(wildcard_path, &data);

	if (file == INVALID_HANDLE_VALUE)
	{
		return;
	}

	do
	{
		if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
		{
			if (strcmp(data.cFileName, ".") != 0 && strcmp(data.cFileName, "..") != 0)
			{
				char child_folder[MAX_PATH];
				wsprintf(child_folder, "%s\\%s", relativeFolder, data.cFileName);
				this->LoadEventItemBagFolder(child_folder, loadedBags);
			}
		}
		else
		{
			if (isdigit(data.cFileName[0]) != 0 && isdigit(data.cFileName[1]) != 0 && isdigit(data.cFileName[2]) != 0)
			{
				if (data.cFileName[3] == ' ' && data.cFileName[4] == '-' && data.cFileName[5] == ' ')
				{
					std::map<int, ITEM_BAG_MANAGER_INFO>::iterator it = this->m_ItemBagManagerInfo.find(atoi(data.cFileName));

					if (it != this->m_ItemBagManagerInfo.end())
					{
						if (loadedBags.find(it->first) == loadedBags.end())
						{
							char path[MAX_PATH];

							wsprintf(path, "%s\\%s", relativeFolder, data.cFileName);

							it->second.ItemBag.Load(gPath.GetFullPath(path));

							loadedBags.insert(std::pair<int, int>(it->first, 1));
						}
					}
				}
			}
		}
	} while (FindNextFile(file, &data) != 0);

	FindClose(file);
}

bool CItemBagManager::GetItemByItemIndex(int ItemIndex, int ItemLevel, LPOBJ lpObj, CItem* lpItem) // OK
{
	for (std::map<int, ITEM_BAG_MANAGER_INFO>::iterator it = this->m_ItemBagManagerInfo.begin(); it != this->m_ItemBagManagerInfo.end(); it++)
	{
		if (it->second.ItemIndex != -1 && it->second.ItemIndex == ItemIndex && (it->second.ItemLevel == -1 || it->second.ItemLevel == ItemLevel))
		{
			return it->second.ItemBag.GetItem(lpObj, lpItem);
		}
	}

	return 0;
}

bool CItemBagManager::GetItemByMonsterClass(int MonsterClass, LPOBJ lpObj, CItem* lpItem) // OK
{
	for (std::map<int, ITEM_BAG_MANAGER_INFO>::iterator it = this->m_ItemBagManagerInfo.begin(); it != this->m_ItemBagManagerInfo.end(); it++)
	{
		if (it->second.MonsterClass != -1 && it->second.MonsterClass == MonsterClass)
		{
			return it->second.ItemBag.GetItem(lpObj, lpItem);
		}
	}

	return 0;
}

bool CItemBagManager::GetItemBySpecialValue(int SpecialValue, LPOBJ lpObj, CItem* lpItem) // OK
{
	for (std::map<int, ITEM_BAG_MANAGER_INFO>::iterator it = this->m_ItemBagManagerInfo.begin(); it != this->m_ItemBagManagerInfo.end(); it++)
	{
		if (it->second.SpecialValue != -1 && it->second.SpecialValue == SpecialValue)
		{
			return it->second.ItemBag.GetItem(lpObj, lpItem);
		}
	}

	return 0;
}

bool CItemBagManager::DropItemByItemIndex(int ItemIndex, int ItemLevel, LPOBJ lpObj, int map, int x, int y) // OK
{
	for (std::map<int, ITEM_BAG_MANAGER_INFO>::iterator it = this->m_ItemBagManagerInfo.begin(); it != this->m_ItemBagManagerInfo.end(); it++)
	{
		if (it->second.ItemIndex != -1 && it->second.ItemIndex == ItemIndex && (it->second.ItemLevel == -1 || it->second.ItemLevel == ItemLevel))
		{
			return it->second.ItemBag.DropItem(lpObj, map, x, y);
		}
	}

	return 0;
}

bool CItemBagManager::DropItemByMonsterClass(int MonsterClass, LPOBJ lpObj, int map, int x, int y) // OK
{
	for (std::map<int, ITEM_BAG_MANAGER_INFO>::iterator it = this->m_ItemBagManagerInfo.begin(); it != this->m_ItemBagManagerInfo.end(); it++)
	{
		if (it->second.MonsterClass != -1 && it->second.MonsterClass == MonsterClass)
		{
			return it->second.ItemBag.DropItem(lpObj, map, x, y);
		}
	}

	return 0;
}

bool CItemBagManager::DropItemBySpecialValue(int SpecialValue, LPOBJ lpObj, int map, int x, int y) // OK
{
	for (std::map<int, ITEM_BAG_MANAGER_INFO>::iterator it = this->m_ItemBagManagerInfo.begin(); it != this->m_ItemBagManagerInfo.end(); it++)
	{
		if (it->second.SpecialValue != -1 && it->second.SpecialValue == SpecialValue)
		{
			return it->second.ItemBag.DropItem(lpObj, map, x, y);
		}
	}

	return 0;
}

void CItemBagManager::CollectMonsterDropItems(int MonsterClass, std::vector<int>& items) const
{
	for (std::map<int,ITEM_BAG_MANAGER_INFO>::const_iterator it = m_ItemBagManagerInfo.begin(); it != m_ItemBagManagerInfo.end(); ++it)
	{
		if (it->second.MonsterClass == MonsterClass) it->second.ItemBag.CollectItemIndexes(items);
	}
}
