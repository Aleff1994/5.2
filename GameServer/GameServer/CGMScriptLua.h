#pragma once
#include <sol/sol.hpp>
#include <map>
#include <string>

#define LUA_SCRIPT_ROOT "C:\\MuServer1\\ScriptLua"

extern int g_LuaExpEventRate;
extern DWORD g_LuaExpEventEndTime;   // <-- horario (tick) em que o evento deve acabar, 0 = sem evento ativo

struct LUA_LUCKY_WHEEL_CONFIG
{
	bool Enable;
	int FreeSpinsDaily;
	int DailyLimit;
	int TicketSection;
	int TicketIndex;
	int TicketCount;
	int WCoinCost;
	int WCoinPCost;
	int GoblinPointCost;
	int ZenCost;
	bool AnnounceRareReward;
	bool CashEnable;
	bool GoldEnable;
	bool FreeEnable;
	bool GoblinEnable;
};

struct LUA_LUCKY_WHEEL_REWARD
{
	int Kind; // 0 item, 1 WCoinC, 2 WCoinP, 3 GoblinPoint, 4 Zen
	int Pool; // 0 Cash, 1 Gold, 2 Gratis, 3 Goblin
	int Section;
	int Index;
	int Level;
	int Amount;
	int Skill;
	int Luck;
	int Option;
	int Excellent;
	int Ancient;
	int Durability;
	int Harmony;
	int ItemOptionEx;
	int SocketBonus;
	int Duration;
	int Socket[5];
	int Weight;
	bool Rare;
	std::string Name;
};

class CGMScriptLua
{
private:
	sol::state lua;
	std::map<std::string, FILETIME> m_LastWriteTime; // guarda a ultima data de modificacao de cada script
	FILETIME m_EventEntryLastWriteTime;
	FILETIME m_EventEntryPendingWriteTime;
	DWORD m_EventEntryPendingTick;
	bool m_EventEntryEnabled[4];
	int m_EventEntryLevel[4][7][2][2]; // evento, faixa, comum/especial, min/max
	int m_EventEntryType;
public:
	CGMScriptLua();
	~CGMScriptLua();

	bool RunScript(const char* path);
	int LoadFolder(const char* folder);
	void CheckEventEntryReload();
	bool GetDailyRewardCalendar(int& today, int& daysInMonth, int& startWeekday);
	bool GetDailyRewardDefinition(int day, int& kind, int& section, int& index, int& level, int& amount,
		int& skill, int& luck, int& option, int& excellent, int& ancient, int& durability, std::string& name);
	bool GetLuckyWheelConfig(LUA_LUCKY_WHEEL_CONFIG& config);
	bool GetLuckyWheelReward(int rewardIndex, LUA_LUCKY_WHEEL_REWARD& reward);
	int GetLuckyWheelRewardCount();
	bool GetEventEntryLevel(int eventType, int eventLevel, bool specialClass, int& minReset, int& maxReset) const;
	int CalculateEventEntryLevel(int eventType, bool specialClass, int characterLevel, int resetCount,
		int masterResetCount, bool thirdClass, int roomCount) const;

	// Chama a funcao "OnPvPChampion(index)" definida no script .lua, se ela existir
	void CallPvPChampion(int championIndex);
private:
	void RegisterFunctions();
	void ReloadEventEntryLevel();
	int LoadFolderRecursive(const std::string& folder);
};

enum LUA_EVENT_ENTRY_TYPE
{
	LUA_EVENT_BLOOD_CASTLE = 0,
	LUA_EVENT_DEVIL_SQUARE = 1,
	LUA_EVENT_CHAOS_CASTLE = 2,
	LUA_EVENT_ILLUSION_TEMPLE = 3,
};

extern CGMScriptLua* g_pScriptLua;

// Chamada a cada "tick" do server para verificar se o evento deve acabar
void LuaEventCheckExpire();
