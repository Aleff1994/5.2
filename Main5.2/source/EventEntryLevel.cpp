#include "stdafx.h"
#include "EventEntryLevel.h"
#include <sol/sol.hpp>

CEventEntryLevel gEventEntryLevel;

CEventEntryLevel::CEventEntryLevel()
	: m_bBloodCastle(false), m_bDevilSquare(false), m_bChaosCastle(false), m_bIllusionTemple(false)
{
	memset(m_BloodCastleEntryLevelCommon, 0, sizeof(m_BloodCastleEntryLevelCommon));
	memset(m_BloodCastleEntryLevelSpecial, 0, sizeof(m_BloodCastleEntryLevelSpecial));
	memset(m_DevilSquareEntryLevelCommon, 0, sizeof(m_DevilSquareEntryLevelCommon));
	memset(m_DevilSquareEntryLevelSpecial, 0, sizeof(m_DevilSquareEntryLevelSpecial));
	memset(m_ChaosCastleEntryLevelCommon, 0, sizeof(m_ChaosCastleEntryLevelCommon));
	memset(m_ChaosCastleEntryLevelSpecial, 0, sizeof(m_ChaosCastleEntryLevelSpecial));
	memset(m_ChaosCastleZen, 0, sizeof(m_ChaosCastleZen));
	memset(m_IllusionTempleEntryLevelMin, 0, sizeof(m_IllusionTempleEntryLevelMin));
	memset(m_IllusionTempleEntryLevelMax, 0, sizeof(m_IllusionTempleEntryLevelMax));
}

bool CEventEntryLevel::Init()
{
	m_bBloodCastle = m_bDevilSquare = m_bChaosCastle = m_bIllusionTemple = false;

	try
	{
		sol::state lua;
		lua.open_libraries(sol::lib::base, sol::lib::table);
		lua.set_function("OpenFolder", [](const char*) {});
		lua.set_function("SetBloodCastleInfo", [this](int index, int commonMin, int commonMax, int specialMin, int specialMax)
		{
			if (index < 0 || index >= 7) return;
			m_BloodCastleEntryLevelCommon[index][0] = commonMin;
			m_BloodCastleEntryLevelCommon[index][1] = commonMax;
			m_BloodCastleEntryLevelSpecial[index][0] = specialMin;
			m_BloodCastleEntryLevelSpecial[index][1] = specialMax;
			m_bBloodCastle = true;
		});
		lua.set_function("SetDevilSquareInfo", [this](int index, int commonMin, int commonMax, int specialMin, int specialMax)
		{
			if (index < 0 || index >= 6) return;
			m_DevilSquareEntryLevelCommon[index][0] = commonMin;
			m_DevilSquareEntryLevelCommon[index][1] = commonMax;
			m_DevilSquareEntryLevelSpecial[index][0] = specialMin;
			m_DevilSquareEntryLevelSpecial[index][1] = specialMax;
			m_bDevilSquare = true;
		});
		lua.set_function("SetChaosCastleInfo", [this](int index, int commonMin, int commonMax, int specialMin, int specialMax)
		{
			if (index < 0 || index >= 6) return;
			m_ChaosCastleEntryLevelCommon[index][0] = commonMin;
			m_ChaosCastleEntryLevelCommon[index][1] = commonMax;
			m_ChaosCastleEntryLevelSpecial[index][0] = specialMin;
			m_ChaosCastleEntryLevelSpecial[index][1] = specialMax;
			m_bChaosCastle = true;
		});
		lua.set_function("SetIllusionTempleInfo", [this](int index, int minLevel, int maxLevel)
		{
			if (index < 0 || index >= 5) return;
			m_IllusionTempleEntryLevelMin[index] = minLevel;
			m_IllusionTempleEntryLevelMax[index] = maxLevel;
			m_bIllusionTemple = true;
		});

		sol::protected_function_result loaded = lua.safe_script_file("Data\\Lua\\Config\\EventEntryLevel.lua", sol::script_pass_on_error);
		if (!loaded.valid()) return false;
		sol::protected_function load = lua["LoadEventEntryLevel"];
		if (!load.valid()) return false;
		sol::protected_function_result result = load();
		if (!result.valid()) return false;

		const char* iniPath = "Data\\Custom\\EventEntry.txt";
		if (GetFileAttributesA(iniPath) != INVALID_FILE_ATTRIBUTES)
		{
			char key[64];
			for (int i = 0; i < 7 && m_bBloodCastle; ++i)
			{
				sprintf_s(key, "BC%dLevelMin_Normal", i + 1);
				m_BloodCastleEntryLevelCommon[i][0] = GetPrivateProfileIntA("BloodCastle", key, m_BloodCastleEntryLevelCommon[i][0], iniPath);
				sprintf_s(key, "BC%dLevelMax_Normal", i + 1);
				m_BloodCastleEntryLevelCommon[i][1] = GetPrivateProfileIntA("BloodCastle", key, m_BloodCastleEntryLevelCommon[i][1], iniPath);
				sprintf_s(key, "BC%dLevelMin_MGDLRF", i + 1);
				m_BloodCastleEntryLevelSpecial[i][0] = GetPrivateProfileIntA("BloodCastle", key, m_BloodCastleEntryLevelSpecial[i][0], iniPath);
				sprintf_s(key, "BC%dLevelMax_MGDLRF", i + 1);
				m_BloodCastleEntryLevelSpecial[i][1] = GetPrivateProfileIntA("BloodCastle", key, m_BloodCastleEntryLevelSpecial[i][1], iniPath);
			}
			for (int i = 0; i < 6 && m_bDevilSquare; ++i)
			{
				sprintf_s(key, "DS%dLevelMin_Normal", i + 1);
				m_DevilSquareEntryLevelCommon[i][0] = GetPrivateProfileIntA("DevilSquare", key, m_DevilSquareEntryLevelCommon[i][0], iniPath);
				sprintf_s(key, "DS%dLevelMax_Normal", i + 1);
				m_DevilSquareEntryLevelCommon[i][1] = GetPrivateProfileIntA("DevilSquare", key, m_DevilSquareEntryLevelCommon[i][1], iniPath);
				sprintf_s(key, "DS%dLevelMin_MGDLRF", i + 1);
				m_DevilSquareEntryLevelSpecial[i][0] = GetPrivateProfileIntA("DevilSquare", key, m_DevilSquareEntryLevelSpecial[i][0], iniPath);
				sprintf_s(key, "DS%dLevelMax_MGDLRF", i + 1);
				m_DevilSquareEntryLevelSpecial[i][1] = GetPrivateProfileIntA("DevilSquare", key, m_DevilSquareEntryLevelSpecial[i][1], iniPath);
			}
			for (int i = 0; i < 6 && m_bChaosCastle; ++i)
			{
				sprintf_s(key, "CC%dLevelMin_Normal", i + 1);
				m_ChaosCastleEntryLevelCommon[i][0] = GetPrivateProfileIntA("ChaosCastle", key, m_ChaosCastleEntryLevelCommon[i][0], iniPath);
				sprintf_s(key, "CC%dLevelMax_Normal", i + 1);
				m_ChaosCastleEntryLevelCommon[i][1] = GetPrivateProfileIntA("ChaosCastle", key, m_ChaosCastleEntryLevelCommon[i][1], iniPath);
				sprintf_s(key, "CC%dLevelMin_MGDLRF", i + 1);
				m_ChaosCastleEntryLevelSpecial[i][0] = GetPrivateProfileIntA("ChaosCastle", key, m_ChaosCastleEntryLevelSpecial[i][0], iniPath);
				sprintf_s(key, "CC%dLevelMax_MGDLRF", i + 1);
				m_ChaosCastleEntryLevelSpecial[i][1] = GetPrivateProfileIntA("ChaosCastle", key, m_ChaosCastleEntryLevelSpecial[i][1], iniPath);
			}
			for (int i = 0; i < 7 && m_bChaosCastle; ++i)
			{
				sprintf_s(key, "CC%dZen", i + 1);
				m_ChaosCastleZen[i] = GetPrivateProfileIntA("ChaosCastle", key, 0, iniPath);
			}
		}
		return true;
	}
	catch (const sol::error&)
	{
		return false;
	}
}

int CEventEntryLevel::CalculateEntryLevel(int eventType, bool specialClass, int characterLevel, int resetCount,
	int masterResetCount, bool thirdClass, int entryType) const
{
	int roomCount = (eventType == EVENT_ENTRY_BLOOD_CASTLE) ? 7 : 6;
	if (entryType == 3) return roomCount - 1;
	if (entryType == 0 && thirdClass) return roomCount - 1;

	const int (*normal)[2] = 0;
	const int (*special)[2] = 0;
	if (eventType == EVENT_ENTRY_BLOOD_CASTLE)
	{
		normal = m_BloodCastleEntryLevelCommon;
		special = m_BloodCastleEntryLevelSpecial;
	}
	else if (eventType == EVENT_ENTRY_DEVIL_SQUARE)
	{
		normal = m_DevilSquareEntryLevelCommon;
		special = m_DevilSquareEntryLevelSpecial;
	}
	else
	{
		normal = m_ChaosCastleEntryLevelCommon;
		special = m_ChaosCastleEntryLevelSpecial;
	}
	const int (*ranges)[2] = specialClass ? special : normal;
	int value = (entryType == 1) ? resetCount : ((entryType == 2) ? masterResetCount : characterLevel);
	for (int i = 0; i < roomCount; ++i)
	{
		if (value >= ranges[i][0] && value <= ranges[i][1]) return i;
	}
	return -1;
}
