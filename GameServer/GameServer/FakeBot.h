#pragma once

#include "User.h"

#define MAX_FAKE_BOT MAX_OBJECT_USER

enum eFakeBotState
{
	FAKE_BOT_DISABLED = 0,
	FAKE_BOT_WAITING = 1,
	FAKE_BOT_ACCOUNT = 2,
	FAKE_BOT_CHARACTER = 3,
	FAKE_BOT_ONLINE = 4,
};

struct FAKE_BOT_INFO
{
	int Enabled;
	int State;
	int Index;
	DWORD NextActionTick;
	char Account[11];
	char Password[11];
	char Character[11];
	int AdvancedAI;
	int SearchRange;
	int AttackRange;
	int Skill;
	int AttackDelay;
	int PotionPercent;
	int HomeRange;
	int AutoReset;
	int AutoPoints;
	int HomeMap;
	int HomeX;
	int HomeY;
	DWORD LastTargetSearchTick;
	bool TargetSearchEmpty;
	DWORD LastAttackTick;
	DWORD LastMoveTick;
	DWORD LastPotionTick;
};

class CFakeBot
{
public:
	CFakeBot();
	void Load(const char* path);
	void MainProc();
	void OnAccountLogin(int aIndex);
	void OnAccountLoginFailed(int aIndex, int result);
	void OnCharacterLoaded(int aIndex);
	bool IsFakeBot(int aIndex) const;
	bool IsManagedIndex(int aIndex) const;
	int ConnectAll();
	int DisconnectAll(const char* reason);
	int GetLoadedCount() const;
	int GetOnlineCount() const;
	int FindChatBot(int playerIndex, const char* name, int range) const;

private:
	void Clear();
	void Connect(int slot);
	void Disconnect(int slot, const char* reason);
	int FindByIndex(int aIndex) const;
	void RunAdvancedAI(int slot);
	int FindMonster(FAKE_BOT_INFO* lpInfo, LPOBJ lpObj) const;
	void UseLifePotion(FAKE_BOT_INFO* lpInfo, LPOBJ lpObj);
	void UseManaPotion(FAKE_BOT_INFO* lpInfo, LPOBJ lpObj);
	void ConfigureAutoProgress(FAKE_BOT_INFO* lpInfo, LPOBJ lpObj);

	int m_Enabled;
	int m_AutoConnect;
	int m_LoginDelay;
	int m_RetryDelay;
	int m_Count;
	FAKE_BOT_INFO m_Bot[MAX_FAKE_BOT];
};

extern CFakeBot gFakeBot;
