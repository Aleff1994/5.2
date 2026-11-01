#include "stdafx.h"
#include "BuildProfile.h"
#include "DSProtocol.h"
#include "Log.h"
#include "Map.h"
#include "Notice.h"
#include "ObjectManager.h"
#include "Protocol.h"

#include <fstream>

#define BUILD_PROFILE_FILE ".\\Data\\Custom\\BuildProfiles.txt"

CBuildProfile gBuildProfile;

CBuildProfile::CBuildProfile() : m_Loaded(false)
{
}

std::string CBuildProfile::MakeKey(const char* account, const char* name, int slot) const
{
	char key[64];
	sprintf_s(key, "%s|%s|%d", account, name, slot);
	return key;
}

int CBuildProfile::GetSlot(const char* name) const
{
	if (_stricmp(name, "pvp") == 0) return 0;
	if (_stricmp(name, "pve") == 0) return 1;
	if (_stricmp(name, "pvm") == 0) return 2;
	return -1;
}

const char* CBuildProfile::GetSlotName(int slot) const
{
	static const char* names[] = { "PvP", "PvE", "PvM" };
	return (slot >= 0 && slot < 3) ? names[slot] : "?";
}

void CBuildProfile::Notice(LPOBJ lpObj, const char* format, ...)
{
	char text[256];
	va_list args;
	va_start(args, format);
	vsprintf_s(text, format, args);
	va_end(args);
	gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, text);
}

void CBuildProfile::Load()
{
	if (m_Loaded) return;
	m_Loaded = true;
	m_Profiles.clear();

	std::ifstream file(BUILD_PROFILE_FILE);
	BUILD_PROFILE_INFO info;
	while (file >> info.Account >> info.Name >> info.Slot >> info.Strength >> info.Dexterity >> info.Vitality >> info.Energy >> info.Leadership)
	{
		if (info.Slot >= 0 && info.Slot < 3)
		{
			m_Profiles[MakeKey(info.Account, info.Name, info.Slot)] = info;
		}
	}
	gLog.Output(LOG_GENERAL, "[BuildProfile] Loaded %d profile(s)", (int)m_Profiles.size());
}

bool CBuildProfile::Save()
{
	std::string temp = std::string(BUILD_PROFILE_FILE) + ".tmp";
	std::ofstream file(temp.c_str(), std::ios::trunc);
	if (!file.is_open()) return false;

	for (std::map<std::string, BUILD_PROFILE_INFO>::const_iterator it = m_Profiles.begin(); it != m_Profiles.end(); ++it)
	{
		const BUILD_PROFILE_INFO& p = it->second;
		file << p.Account << ' ' << p.Name << ' ' << p.Slot << ' '
			<< p.Strength << ' ' << p.Dexterity << ' ' << p.Vitality << ' '
			<< p.Energy << ' ' << p.Leadership << '\n';
	}
	file.close();
	return (MoveFileExA(temp.c_str(), BUILD_PROFILE_FILE, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0);
}

bool CBuildProfile::HasAny(LPOBJ lpObj)
{
	Load();
	for (int slot = 0; slot < 3; ++slot)
	{
		if (m_Profiles.find(MakeKey(lpObj->Account, lpObj->Name, slot)) != m_Profiles.end()) return true;
	}
	return false;
}

bool CBuildProfile::Command(LPOBJ lpObj, const char* action, const char* slotName)
{
	Load();

	if (action[0] == 0 || _stricmp(action, "ajuda") == 0)
	{
		Notice(lpObj, "Build: /build salvar|usar|apagar pvp|pve|pvm");
		Notice(lpObj, "Build: /build listar");
		return true;
	}

	if (_stricmp(action, "listar") == 0)
	{
		for (int slot = 0; slot < 3; ++slot)
		{
			bool saved = (m_Profiles.find(MakeKey(lpObj->Account, lpObj->Name, slot)) != m_Profiles.end());
			Notice(lpObj, "Build %s: %s", GetSlotName(slot), saved ? "salva" : "vazia");
		}
		return true;
	}

	int slot = GetSlot(slotName);
	if (slot < 0)
	{
		Notice(lpObj, "Build invalida. Use pvp, pve ou pvm.");
		return false;
	}

	std::string key = MakeKey(lpObj->Account, lpObj->Name, slot);
	if (_stricmp(action, "salvar") == 0)
	{
		BUILD_PROFILE_INFO info = {};
		strcpy_s(info.Account, lpObj->Account);
		strcpy_s(info.Name, lpObj->Name);
		info.Slot = slot;
		info.Strength = lpObj->Strength;
		info.Dexterity = lpObj->Dexterity;
		info.Vitality = lpObj->Vitality;
		info.Energy = lpObj->Energy;
		info.Leadership = lpObj->Leadership;
		m_Profiles[key] = info;
		if (!Save())
		{
			m_Profiles.erase(key);
			Notice(lpObj, "Erro ao salvar build. Verifique Data\\Custom.");
			return false;
		}
		Notice(lpObj, "Build %s salva com sucesso.", GetSlotName(slot));
		gLog.Output(LOG_COMMAND, "[BuildProfile][%s][%s] Saved %s", lpObj->Account, lpObj->Name, GetSlotName(slot));
		return true;
	}

	if (_stricmp(action, "apagar") == 0)
	{
		if (m_Profiles.erase(key) == 0)
		{
			Notice(lpObj, "Build %s ainda esta vazia.", GetSlotName(slot));
			return false;
		}
		if (!Save())
		{
			Notice(lpObj, "Erro ao atualizar arquivo de builds.");
			return false;
		}
		Notice(lpObj, "Build %s apagada.", GetSlotName(slot));
		return true;
	}

	if (_stricmp(action, "usar") == 0)
	{
		std::map<std::string, BUILD_PROFILE_INFO>::iterator it = m_Profiles.find(key);
		if (it == m_Profiles.end())
		{
			Notice(lpObj, "Build %s nao foi salva.", GetSlotName(slot));
			return false;
		}
		if (lpObj->Interface.use != 0 || lpObj->Teleport != 0 || lpObj->DieRegen != 0 || lpObj->PShopOpen != 0 || gMap[lpObj->Map].CheckAttr(lpObj->X, lpObj->Y, 1) == 0)
		{
			Notice(lpObj, "Troque a build em zona segura e fora de interfaces.");
			return false;
		}

		const BUILD_PROFILE_INFO& p = it->second;
		long long budget = (long long)lpObj->Strength + lpObj->Dexterity + lpObj->Vitality + lpObj->Energy + lpObj->Leadership + lpObj->LevelUpPoint;
		long long used = (long long)p.Strength + p.Dexterity + p.Vitality + p.Energy + p.Leadership;
		if (used < 0 || used > budget)
		{
			Notice(lpObj, "Build %s exige mais pontos do que voce possui.", GetSlotName(slot));
			return false;
		}

		lpObj->Strength = p.Strength;
		lpObj->Dexterity = p.Dexterity;
		lpObj->Vitality = p.Vitality;
		lpObj->Energy = p.Energy;
		lpObj->Leadership = p.Leadership;
		lpObj->LevelUpPoint = (int)(budget - used);
		gObjectManager.CharacterCalcAttribute(lpObj->Index);
		GCNewCharacterInfoSend(lpObj);
		GDCharacterInfoSaveSend(lpObj->Index);
		Notice(lpObj, "Build %s ativada. Pontos livres: %d", GetSlotName(slot), lpObj->LevelUpPoint);
		gLog.Output(LOG_COMMAND, "[BuildProfile][%s][%s] Activated %s", lpObj->Account, lpObj->Name, GetSlotName(slot));
		return true;
	}

	Notice(lpObj, "Acao invalida. Use salvar, usar, listar ou apagar.");
	return false;
}
