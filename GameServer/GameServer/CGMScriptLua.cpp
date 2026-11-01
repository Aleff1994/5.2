#include "stdafx.h"
#include "CGMScriptLua.h"
#include "Util.h"
#include "Notice.h"
#include "PvPTournament.h"
#include "DSProtocol.h"
#include "JSProtocol.h"
#include "ItemManager.h"
#include "Gate.h"
#include "DailyReward.h"

CGMScriptLua* g_pScriptLua = nullptr;
int g_LuaExpEventRate = 100;
DWORD g_LuaExpEventEndTime = 0;

static void Lua_Log(const std::string& msg)
{
	LogAdd(LOG_BLUE, "[Lua] %s", msg.c_str());
}

static void Lua_Broadcast(const std::string& msg)
{
	gNotice.GCNoticeSendToAll(0, 1, 100, 100, RGB(255, 200, 80), 100, "%s", msg.c_str());
}

static void Lua_BroadcastColor(const std::string& msg, int r, int g, int b)
{
	gNotice.GCNoticeSendToAll(0, 1, 100, 100, RGB(r, g, b), 100, "%s", msg.c_str());
}

static void Lua_SetExpRate(int rate, int durationSeconds)
{
	g_LuaExpEventRate = rate;
	if (durationSeconds > 0)
	{
		g_LuaExpEventEndTime = GetTickCount() + (DWORD)(durationSeconds * 1000);
	}
	else
	{
		g_LuaExpEventEndTime = 0;
	}
}

static void Lua_StopExpRate()
{
	g_LuaExpEventRate = 100;
	g_LuaExpEventEndTime = 0;
}

static bool Lua_IsExpEventActive()
{
	return (g_LuaExpEventRate != 100);
}

// ============================================
// Funcoes Lua - Controle do Torneio PvP
// ============================================

static void Lua_SetPvPNpc(int classId)
{
	g_PvPEventNpcClass = classId;
}

static void Lua_OpenPvPRegistration()
{
	gPvPTournament.OpenRegistration();
}

static void Lua_StartPvPTournament(int bestOf)
{
	if (bestOf < 1)
	{
		bestOf = 1;
	}
	if ((bestOf % 2) == 0)
	{
		bestOf++;
	}
	g_PvPBestOfRounds = bestOf;
	gPvPTournament.CloseRegistration();
	gPvPTournament.StartTournament();
}

static void Lua_CancelPvPTournament()
{
	gPvPTournament.CancelTournament();
}

static void Lua_SetPvPSchedule(int dayOfWeek, int hour, int minute, int registrationMinutes, int bestOf)
{
	dayOfWeek = (dayOfWeek < -1 || dayOfWeek > 6) ? -1 : dayOfWeek;
	hour = (hour < 0) ? 0 : ((hour > 23) ? 23 : hour);
	minute = (minute < 0) ? 0 : ((minute > 59) ? 59 : minute);
	registrationMinutes = (registrationMinutes < 1) ? 1 : registrationMinutes;
	bestOf = (bestOf < 1) ? 1 : bestOf;
	bestOf += ((bestOf % 2) == 0) ? 1 : 0;
	g_PvPScheduleEnabled = true;
	g_PvPScheduleDayOfWeek = dayOfWeek;
	g_PvPScheduleHour = hour;
	g_PvPScheduleMinute = minute;
	g_PvPScheduleRegistrationMinutes = registrationMinutes;
	g_PvPScheduleBestOf = bestOf;
}

static void Lua_DisablePvPSchedule()
{
	g_PvPScheduleEnabled = false;
}

static void Lua_SetPvPWaitGate(int map, int x, int y, int dir)
{
	gGate.SetCustomGate(PVP_WAIT_GATE, map, x, y, dir);
}

static void Lua_SetPvPLoserGate(int map, int x, int y, int dir)
{
	gGate.SetCustomGate(PVP_LOSER_GATE, map, x, y, dir);
}

static void Lua_SetPvPWinnerGate(int map, int x, int y, int dir)
{
	gGate.SetCustomGate(PVP_WINNER_GATE, map, x, y, dir);
}

static void Lua_SetPvPArena(int map, int xA, int yA, int xB, int yB, int dir)
{
	gGate.SetCustomGate(PVP_ARENA_GATE_A, map, xA, yA, dir);
	gGate.SetCustomGate(PVP_ARENA_GATE_B, map, xB, yB, dir);
}

static void Lua_SetPvPRing(int map, int minX, int minY, int maxX, int maxY, bool enabled)
{
	g_PvPRingMap = map;
	g_PvPRingMinX = (minX < maxX) ? minX : maxX;
	g_PvPRingMinY = (minY < maxY) ? minY : maxY;
	g_PvPRingMaxX = (minX > maxX) ? minX : maxX;
	g_PvPRingMaxY = (minY > maxY) ? minY : maxY;
	g_PvPRingEnabled = enabled;
}
// ============================================
// Funcoes Lua - Premios do Torneio PvP
// ============================================

// Auxiliar: acha slot vazio no inventario, insere o item e notifica o cliente
static bool GivePvPItemToInventory(LPOBJ lpObj, int itemIndex, int itemLevel)
{
	CItem item;
	BYTE emptySocket[MAX_SOCKET_OPTION] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

	item.Convert(itemIndex, 0, 0, 0, 0, 0, 0, 0, emptySocket, 0xFF);
	item.m_Level = (BYTE)itemLevel;

	BYTE slot = gItemManager.InventoryInsertItem(lpObj->Index, item);

	if (slot == 0xFF)
	{
		return false;
	}

	PMSG_ITEM_GET_SEND pMsg;
	pMsg.header.setE(0x22, sizeof(pMsg));
	pMsg.result = slot;
	gItemManager.ItemByteConvert(pMsg.ItemInfo, item);
	DataSend(lpObj->Index, (BYTE*)&pMsg, pMsg.header.size);

	return true;
}

static void Lua_GivePvPItem(int aIndex, int section, int type, int itemLevel)
{
	if (OBJECT_RANGE(aIndex) == 0 || gObjIsConnected(aIndex) == 0)
	{
		return;
	}
	if (section < 0 || section > 15 || type < 0 || type > 511 || itemLevel < 0 || itemLevel > 15)
	{
		LogAdd(LOG_RED, "[PvP Premio] Item invalido: section=%d type=%d level=%d.", section, type, itemLevel);
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];
	int itemIndex = GET_ITEM(section, type);

	if (GivePvPItemToInventory(lpObj, itemIndex, itemLevel))
	{
		LogAdd(LOG_BLUE, "[PvP Premio] Item %d (+%d) entregue para %s.", itemIndex, itemLevel, lpObj->Name);
	}
	else
	{
		LogAdd(LOG_RED, "[PvP Premio] Inventario de %s cheio, item NAO entregue.", lpObj->Name);
	}
}

static void Lua_GivePvPCoin(int aIndex, int coinType, int amount)
{
	if (OBJECT_RANGE(aIndex) == 0 || gObjIsConnected(aIndex) == 0)
	{
		return;
	}
	if (coinType < 1 || coinType > 3 || amount <= 0)
	{
		LogAdd(LOG_RED, "[PvP Premio] Coin invalida: tipo=%d quantidade=%d.", coinType, amount);
		return;
	}

	int coin1 = (coinType == 1) ? amount : 0;
	int coin2 = (coinType == 2) ? amount : 0;
	int coin3 = (coinType == 3) ? amount : 0;

	GDSetCoinSend(aIndex, coin1, coin2, coin3, "pvp tournament prize");

	LogAdd(LOG_BLUE, "[PvP Premio] Coin tipo %d, qtd %d entregue para %s.", coinType, amount, gObj[aIndex].Name);
}

static void Lua_GivePvPVip(int aIndex, int vipLevel, int days)
{
	if (OBJECT_RANGE(aIndex) == 0 || gObjIsConnected(aIndex) == 0)
	{
		return;
	}
	if (vipLevel < 1 || days <= 0)
	{
		LogAdd(LOG_RED, "[PvP Premio] VIP invalido: nivel=%d dias=%d.", vipLevel, days);
		return;
	}

	GJAccountLevelSaveSend(aIndex, vipLevel, days * 86400);
	GJAccountLevelSend(aIndex);

	LogAdd(LOG_BLUE, "[PvP Premio] VIP nivel %d por %d dias entregue para %s.", vipLevel, days, gObj[aIndex].Name);
}
static void Lua_SpawnPvPNpc(int classId, int map, int x, int y, int dir)
{
	if (!SpawnPvPTournamentNpc(classId, map, x, y, dir))
	{
		LogAdd(LOG_RED, "[PvP] Falha ao spawnar NPC do torneio (classId=%d map=%d x=%d y=%d).", classId, map, x, y);
	}
}

static void Lua_RemovePvPNpc()
{
	RemovePvPTournamentNpc();
}

CGMScriptLua::CGMScriptLua()
{
	memset(&this->m_EventEntryLastWriteTime, 0, sizeof(this->m_EventEntryLastWriteTime));
	memset(&this->m_EventEntryPendingWriteTime, 0, sizeof(this->m_EventEntryPendingWriteTime));
	this->m_EventEntryPendingTick = 0;
	memset(this->m_EventEntryEnabled, 0, sizeof(this->m_EventEntryEnabled));
	memset(this->m_EventEntryLevel, 0, sizeof(this->m_EventEntryLevel));
	this->m_EventEntryType = 0;
	lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::math, sol::lib::os);
	RegisterFunctions();
}

CGMScriptLua::~CGMScriptLua()
{
}

void CGMScriptLua::RegisterFunctions()
{
	lua.set_function("SpawnPvPNpc", &Lua_SpawnPvPNpc);
	lua.set_function("RemovePvPNpc", &Lua_RemovePvPNpc);
	lua.set_function("Log", &Lua_Log);
	lua.set_function("Broadcast", &Lua_Broadcast);
	lua.set_function("BroadcastColor", &Lua_BroadcastColor);
	lua.set_function("SetExpRate", &Lua_SetExpRate);
	lua.set_function("StopExpRate", &Lua_StopExpRate);
	lua.set_function("IsExpEventActive", &Lua_IsExpEventActive);

	lua.set_function("SetPvPNpc", &Lua_SetPvPNpc);
	lua.set_function("OpenPvPRegistration", &Lua_OpenPvPRegistration);
	lua.set_function("StartPvPTournament", &Lua_StartPvPTournament);
	lua.set_function("CancelPvPTournament", &Lua_CancelPvPTournament);
	lua.set_function("SetPvPSchedule", &Lua_SetPvPSchedule);
	lua.set_function("DisablePvPSchedule", &Lua_DisablePvPSchedule);
	lua.set_function("SetPvPWaitGate", &Lua_SetPvPWaitGate);
	lua.set_function("SetPvPLoserGate", &Lua_SetPvPLoserGate);
	lua.set_function("SetPvPWinnerGate", &Lua_SetPvPWinnerGate);
	lua.set_function("SetPvPArena", &Lua_SetPvPArena);
	lua.set_function("SetPvPRing", &Lua_SetPvPRing);

	lua.set_function("GivePvPItem", &Lua_GivePvPItem);
	lua.set_function("GivePvPCoin", &Lua_GivePvPCoin);
	lua.set_function("GivePvPVip", &Lua_GivePvPVip);

	lua.set_function("OpenFolder", [](const char*) {});
	lua.set_function("SetBloodCastleInfo", [this](int index, int commonMin, int commonMax, int specialMin, int specialMax)
	{
		if (index < 0 || index >= 7) return;
		this->m_EventEntryEnabled[LUA_EVENT_BLOOD_CASTLE] = true;
		this->m_EventEntryLevel[LUA_EVENT_BLOOD_CASTLE][index][0][0] = commonMin;
		this->m_EventEntryLevel[LUA_EVENT_BLOOD_CASTLE][index][0][1] = commonMax;
		this->m_EventEntryLevel[LUA_EVENT_BLOOD_CASTLE][index][1][0] = specialMin;
		this->m_EventEntryLevel[LUA_EVENT_BLOOD_CASTLE][index][1][1] = specialMax;
	});
	lua.set_function("SetDevilSquareInfo", [this](int index, int commonMin, int commonMax, int specialMin, int specialMax)
	{
		if (index < 0 || index >= 6) return;
		this->m_EventEntryEnabled[LUA_EVENT_DEVIL_SQUARE] = true;
		this->m_EventEntryLevel[LUA_EVENT_DEVIL_SQUARE][index][0][0] = commonMin;
		this->m_EventEntryLevel[LUA_EVENT_DEVIL_SQUARE][index][0][1] = commonMax;
		this->m_EventEntryLevel[LUA_EVENT_DEVIL_SQUARE][index][1][0] = specialMin;
		this->m_EventEntryLevel[LUA_EVENT_DEVIL_SQUARE][index][1][1] = specialMax;
	});
	lua.set_function("SetChaosCastleInfo", [this](int index, int commonMin, int commonMax, int specialMin, int specialMax)
	{
		if (index < 0 || index >= 6) return;
		this->m_EventEntryEnabled[LUA_EVENT_CHAOS_CASTLE] = true;
		this->m_EventEntryLevel[LUA_EVENT_CHAOS_CASTLE][index][0][0] = commonMin;
		this->m_EventEntryLevel[LUA_EVENT_CHAOS_CASTLE][index][0][1] = commonMax;
		this->m_EventEntryLevel[LUA_EVENT_CHAOS_CASTLE][index][1][0] = specialMin;
		this->m_EventEntryLevel[LUA_EVENT_CHAOS_CASTLE][index][1][1] = specialMax;
	});
	lua.set_function("SetIllusionTempleInfo", [this](int index, int minReset, int maxReset)
	{
		if (index < 0 || index >= 5) return;
		this->m_EventEntryEnabled[LUA_EVENT_ILLUSION_TEMPLE] = true;
		for (int special = 0; special < 2; special++)
		{
			this->m_EventEntryLevel[LUA_EVENT_ILLUSION_TEMPLE][index][special][0] = minReset;
			this->m_EventEntryLevel[LUA_EVENT_ILLUSION_TEMPLE][index][special][1] = maxReset;
		}
	});
}

bool CGMScriptLua::RunScript(const char* path)
{
	try
	{
		lua.script_file(path);
		return true;
	}
	catch (const sol::error& e)
	{
		LogAdd(LOG_RED, "[Lua Error] %s", e.what());
	}
	return false;
}

int CGMScriptLua::LoadFolder(const char* folder)
{
	// O caminho recebido e mantido por compatibilidade. Todos os scripts ficam
	// centralizados no MuServer e sao pesquisados tambem nas subpastas.
	int count = this->LoadFolderRecursive(LUA_SCRIPT_ROOT);
	if (count > 0)
	{
		this->ReloadEventEntryLevel();
	}
	return count;
}

void CGMScriptLua::ReloadEventEntryLevel()
{
	memset(this->m_EventEntryEnabled, 0, sizeof(this->m_EventEntryEnabled));
	memset(this->m_EventEntryLevel, 0, sizeof(this->m_EventEntryLevel));
	try
	{
		sol::protected_function load = this->lua["LoadEventEntryLevel"];
		if (!load.valid()) return;
		sol::protected_function_result result = load();
		if (!result.valid())
		{
			sol::error error = result;
			LogAdd(LOG_RED, "[EventEntryLevel Lua] %s", error.what());
			return;
		}
		const char* mainInfoPath = "C:\\MuServer1\\Tools\\MainInfo\\MainInfo.ini";
		this->m_EventEntryType = GetPrivateProfileIntA("EventSettings", "EventEntryType", 0, mainInfoPath);
		if (this->m_EventEntryType < 0 || this->m_EventEntryType > 4) this->m_EventEntryType = 0;

		const char* iniPath = "C:\\MuServer1\\Data\\Custom\\EventEntry.txt";
		if (GetFileAttributesA(iniPath) != INVALID_FILE_ATTRIBUTES)
		{
			const char* sections[3] = { "BloodCastle", "DevilSquare", "ChaosCastle" };
			const char* prefixes[3] = { "BC", "DS", "CC" };
			const int counts[3] = { 7, 6, 6 };
			for (int eventType = 0; eventType < 3; ++eventType)
			{
				if (!this->m_EventEntryEnabled[eventType]) continue;
				for (int i = 0; i < counts[eventType]; ++i)
				{
					char key[64];
					sprintf_s(key, "%s%dLevelMin_Normal", prefixes[eventType], i + 1);
					this->m_EventEntryLevel[eventType][i][0][0] = GetPrivateProfileIntA(sections[eventType], key, this->m_EventEntryLevel[eventType][i][0][0], iniPath);
					sprintf_s(key, "%s%dLevelMax_Normal", prefixes[eventType], i + 1);
					this->m_EventEntryLevel[eventType][i][0][1] = GetPrivateProfileIntA(sections[eventType], key, this->m_EventEntryLevel[eventType][i][0][1], iniPath);
					sprintf_s(key, "%s%dLevelMin_MGDLRF", prefixes[eventType], i + 1);
					this->m_EventEntryLevel[eventType][i][1][0] = GetPrivateProfileIntA(sections[eventType], key, this->m_EventEntryLevel[eventType][i][1][0], iniPath);
					sprintf_s(key, "%s%dLevelMax_MGDLRF", prefixes[eventType], i + 1);
					this->m_EventEntryLevel[eventType][i][1][1] = GetPrivateProfileIntA(sections[eventType], key, this->m_EventEntryLevel[eventType][i][1][1], iniPath);
				}
			}
		}
		LogAdd(LOG_BLUE, "[EventEntry] Type=%d carregado", this->m_EventEntryType);
		WIN32_FILE_ATTRIBUTE_DATA eventEntryData;
		if (GetFileAttributesExA(iniPath, GetFileExInfoStandard, &eventEntryData) != 0)
		{
			this->m_EventEntryLastWriteTime = eventEntryData.ftLastWriteTime;
		}
	}
	catch (const sol::error& e)
	{
		LogAdd(LOG_RED, "[EventEntryLevel Lua] %s", e.what());
	}
}

void CGMScriptLua::CheckEventEntryReload()
{
	const char* iniPath = "C:\\MuServer1\\Data\\Custom\\EventEntry.txt";
	WIN32_FILE_ATTRIBUTE_DATA data;
	if (GetFileAttributesExA(iniPath, GetFileExInfoStandard, &data) == 0)
	{
		return;
	}

	if (CompareFileTime(&this->m_EventEntryLastWriteTime, &data.ftLastWriteTime) == 0)
	{
		this->m_EventEntryPendingTick = 0;
		return;
	}

	if (this->m_EventEntryPendingTick == 0 || CompareFileTime(&this->m_EventEntryPendingWriteTime, &data.ftLastWriteTime) != 0)
	{
		this->m_EventEntryPendingWriteTime = data.ftLastWriteTime;
		this->m_EventEntryPendingTick = GetTickCount();
		return;
	}

	// Espera o arquivo permanecer estavel por um ciclo antes de recarregar.
	if ((GetTickCount() - this->m_EventEntryPendingTick) < 1500)
	{
		return;
	}

	this->ReloadEventEntryLevel();
	this->m_EventEntryLastWriteTime = data.ftLastWriteTime;
	this->m_EventEntryPendingTick = 0;
	LogAdd(LOG_BLUE, "[EventEntry] EventEntry.txt alterado e recarregado automaticamente.");
}

bool CGMScriptLua::GetEventEntryLevel(int eventType, int eventLevel, bool specialClass, int& minReset, int& maxReset) const
{
	if (eventType < 0 || eventType >= 4 || eventLevel < 0 || eventLevel >= 7 || !this->m_EventEntryEnabled[eventType])
	{
		return false;
	}
	int special = specialClass ? 1 : 0;
	minReset = this->m_EventEntryLevel[eventType][eventLevel][special][0];
	maxReset = this->m_EventEntryLevel[eventType][eventLevel][special][1];
	return (maxReset >= minReset);
}

int CGMScriptLua::CalculateEventEntryLevel(int eventType, bool specialClass, int characterLevel, int resetCount,
	int masterResetCount, bool thirdClass, int roomCount) const
{
	if (eventType < 0 || eventType >= 3 || roomCount < 1 || !this->m_EventEntryEnabled[eventType]) return -2;
	if (this->m_EventEntryType == 3) return roomCount - 1;
	if (this->m_EventEntryType == 0 && thirdClass) return roomCount - 1;
	int special = specialClass ? 1 : 0;
	int value = (this->m_EventEntryType == 1) ? resetCount : ((this->m_EventEntryType == 2) ? masterResetCount : characterLevel);
	for (int room = 0; room < roomCount; ++room)
	{
		int minValue = this->m_EventEntryLevel[eventType][room][special][0];
		int maxValue = this->m_EventEntryLevel[eventType][room][special][1];
		if (value >= minValue && value <= maxValue) return room;
	}
	return -1;
}

bool CGMScriptLua::GetDailyRewardCalendar(int& today, int& daysInMonth, int& startWeekday)
{
	// Usa a data oficial do GameServer. Nao depende de os.date do Lua,
	// que pode ser removido por outro script no ambiente compartilhado.
	SYSTEMTIME st;
	GetLocalTime(&st);
	today = st.wDay;

	int firstWeekday = (int)st.wDayOfWeek - ((int)st.wDay - 1) % 7;
	if (firstWeekday < 0)
	{
		firstWeekday += 7;
	}
	startWeekday = firstWeekday + 1; // 1=DOM ... 7=SAB

	static const int monthDays[12] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
	daysInMonth = monthDays[st.wMonth - 1];
	if (st.wMonth == 2 && ((st.wYear % 400) == 0 || ((st.wYear % 4) == 0 && (st.wYear % 100) != 0)))
	{
		daysInMonth = 29;
	}

	return true;
}
bool CGMScriptLua::GetDailyRewardDefinition(int day, int& kind, int& section, int& index, int& level, int& amount,
	int& skill, int& luck, int& option, int& excellent, int& ancient, int& durability, std::string& name)
{
	try
	{
		sol::function fn = lua["GetDailyRewardDefinition"];
		if (!fn.valid()) return false;
		sol::protected_function_result result = fn(day);
		if (!result.valid() || result.get_type() != sol::type::table) return false;
		sol::table data = result;
		kind = data.get_or("kind", 0);
		section = data.get_or("section", 14);
		index = data.get_or("index", 13);
		level = data.get_or("level", 0);
		amount = data.get_or("amount", 1);
		skill = data.get_or("skill", 0);
		luck = data.get_or("luck", 0);
		option = data.get_or("option", 0);
		excellent = data.get_or("excellent", 0);
		ancient = data.get_or("ancient", 0);
		durability = data.get_or("durability", data.get_or("dur", 0));
		name = data.get_or("name", std::string("Premio diario"));
		return (amount > 0);
	}
	catch (const sol::error& e)
	{
		LogAdd(LOG_RED, "[DailyReward Lua] Definition: %s", e.what());
		return false;
	}
}

bool CGMScriptLua::GetLuckyWheelConfig(LUA_LUCKY_WHEEL_CONFIG& config)
{
	try
	{
		sol::function fn = lua["GetLuckyWheelConfig"];
		if (!fn.valid()) return false;
		sol::protected_function_result result = fn();
		if (!result.valid() || result.get_type() != sol::type::table) return false;
		sol::table data = result;
		config.Enable = data.get_or("enable", true);
		config.FreeSpinsDaily = data.get_or("freeSpinsDaily", 1);
		config.DailyLimit = data.get_or("dailyLimit", 30);
		config.TicketSection = data.get_or("ticketSection", 14);
		config.TicketIndex = data.get_or("ticketIndex", 210);
		config.TicketCount = data.get_or("ticketCount", 1);
		config.WCoinCost = data.get_or("wcoinCost", 100);
		config.WCoinPCost = data.get_or("wcoinPCost", 30);
		config.GoblinPointCost = data.get_or("goblinPointCost", 50);
		config.ZenCost = data.get_or("zenCost", 10000000);
		config.AnnounceRareReward = data.get_or("announceRareReward", true);
		config.CashEnable = data.get_or("cashEnable", true);
		config.GoldEnable = data.get_or("goldEnable", true);
		config.FreeEnable = data.get_or("freeEnable", true);
		config.GoblinEnable = data.get_or("goblinEnable", false);
		return config.DailyLimit > 0;
	}
	catch (const sol::error& e)
	{
		LogAdd(LOG_RED, "[LuckyWheel Lua] Config: %s", e.what());
		return false;
	}
}

int CGMScriptLua::GetLuckyWheelRewardCount()
{
	try
	{
		sol::function fn = lua["GetLuckyWheelRewardCount"];
		if (!fn.valid()) return 0;
		sol::protected_function_result result = fn();
		if (!result.valid()) return 0;
		return result.get<int>();
	}
	catch (const sol::error& e)
	{
		LogAdd(LOG_RED, "[LuckyWheel Lua] RewardCount: %s", e.what());
		return 0;
	}
}

bool CGMScriptLua::GetLuckyWheelReward(int rewardIndex, LUA_LUCKY_WHEEL_REWARD& reward)
{
	try
	{
		sol::function fn = lua["GetLuckyWheelReward"];
		if (!fn.valid()) return false;
		sol::protected_function_result result = fn(rewardIndex);
		if (!result.valid() || result.get_type() != sol::type::table) return false;
		sol::table data = result;
		reward.Kind = data.get_or("kind", 0);
		reward.Pool = data.get_or("pool", 0);
		reward.Section = data.get_or("section", 14);
		reward.Index = data.get_or("index", 13);
		reward.Level = data.get_or("level", 0);
		reward.Amount = data.get_or("amount", 1);
		reward.Skill = data.get_or("skill", 0);
		reward.Luck = data.get_or("luck", 0);
		reward.Option = data.get_or("option", 0);
		reward.Excellent = data.get_or("excellent", data.get_or("ex", 0));
		if (data.get_or("full", false)) reward.Excellent = 63;
		reward.Ancient = data.get_or("ancient", 0);
		reward.Durability = data.get_or("durability", data.get_or("dur", 0));
		reward.Harmony = data.get_or("harmony", data.get_or("joh", 0));
		reward.ItemOptionEx = data.get_or("itemOptionEx", data.get_or("optionEx", 0));
		reward.SocketBonus = data.get_or("socketBonus", 0xFF);
		reward.Duration = data.get_or("duration", 0);
		for (int n = 0; n < MAX_SOCKET_OPTION; n++) reward.Socket[n] = 0xFF;
		sol::optional<sol::table> sockets = data["sockets"];
		if (sockets)
		{
			for (int n = 0; n < MAX_SOCKET_OPTION; n++)
			{
				reward.Socket[n] = sockets->get_or(n + 1, 0xFF);
			}
		}
		reward.Weight = data.get_or("weight", 1);
		reward.Rare = data.get_or("rare", false);
		reward.Name = data.get_or("name", std::string("Lucky Wheel Reward"));
		return reward.Weight > 0 && reward.Amount > 0;
	}
	catch (const sol::error& e)
	{
		LogAdd(LOG_RED, "[LuckyWheel Lua] Reward: %s", e.what());
		return false;
	}
}

int CGMScriptLua::LoadFolderRecursive(const std::string& folder)
{
	int count = 0;

	char searchPath[256];
	sprintf_s(searchPath, "%s\\*", folder.c_str());

	WIN32_FIND_DATAA findData;
	HANDLE hFind = FindFirstFileA(searchPath, &findData);

	if (hFind == INVALID_HANDLE_VALUE)
	{
		return 0;
	}

	do
	{
		if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			if (strcmp(findData.cFileName, ".") != 0 && strcmp(findData.cFileName, "..") != 0)
			{
				count += this->LoadFolderRecursive(folder + "\\" + findData.cFileName);
			}
		}
		else
		{
			std::string fileName = findData.cFileName;
			if (fileName.length() < 4 || _stricmp(fileName.c_str() + fileName.length() - 4, ".lua") != 0)
			{
				continue;
			}

			std::string fullPath = folder + "\\" + fileName;

			bool needsReload = true;

			auto it = m_LastWriteTime.find(fullPath);

			if (it != m_LastWriteTime.end())
			{
				if (CompareFileTime(&it->second, &findData.ftLastWriteTime) == 0)
				{
					needsReload = false;
				}
			}

			if (needsReload)
			{
				if (this->RunScript(fullPath.c_str()))
				{
					LogAdd(LOG_BLUE, "[Lua] Script carregado: %s", fullPath.c_str());
					m_LastWriteTime[fullPath] = findData.ftLastWriteTime;
					count++;
				}
				else
				{
					LogAdd(LOG_RED, "[Lua] Falha ao carregar: %s", findData.cFileName);
				}
			}
		}
	} while (FindNextFileA(hFind, &findData));

	FindClose(hFind);

	return count;
}

void CGMScriptLua::CallPvPChampion(int championIndex)
{
	sol::function fn = lua["OnPvPChampion"];

	if (!fn.valid())
	{
		LogAdd(LOG_RED, "[Lua] OnPvPChampion nao esta definida em nenhum script carregado.");
		return;
	}

	sol::protected_function_result result = fn(championIndex);

	if (!result.valid())
	{
		sol::error err = result;
		LogAdd(LOG_RED, "[Lua Error] OnPvPChampion: %s", err.what());
	}
}

void LuaEventCheckExpire()
{
	static DWORD lastScriptCheck = 0;
	if (g_pScriptLua != nullptr && (GetTickCount() - lastScriptCheck) >= 2000)
	{
		lastScriptCheck = GetTickCount();
		g_pScriptLua->CheckEventEntryReload();
		if (g_pScriptLua->LoadFolder(LUA_SCRIPT_ROOT) > 0)
		{
			gDailyReward.ReloadConfig();
		}
	}

	if (g_LuaExpEventEndTime == 0)
	{
		return;
	}

	if (GetTickCount() >= g_LuaExpEventEndTime)
	{
		g_LuaExpEventRate = 100;
		g_LuaExpEventEndTime = 0;
		gNotice.GCNoticeSendToAll(0, 1, 100, 100, RGB(255, 150, 150), 100, "%s", "O evento de EXP terminou!");
	}
}
