#pragma once

#if(BOT_PLAYER == 1)

#include "User.h"

#define MAX_BOTPLAYER MAX_OBJECT_BOTS
#define MAX_BOTPLAYER_BODY 9
#define MAX_BOTPLAYER_ROUTE 16
#define MAX_BOTPLAYER_MESSAGE 5

struct BOTPLAYER_BODY_ITEM
{
	int num;
	int level;
	int opt;
	int Enabled;
};

struct BOTPLAYER_ROUTE
{
	BYTE x;
	BYTE y;
};

struct BOTPLAYER_INFO
{
	int Enabled;
	int Class;
	char Name[11];
	BYTE Map;
	BYTE X;
	BYTE Y;
	BYTE Dir;
	int Level;
	int PKLevel;
	int AttackEnabled;
	int AttackPlayers;
	int AttackMonsters;
	int CanBeAttacked;
	int SkillNumber;
	int Strength;
	int Dexterity;
	int Vitality;
	int Energy;
	int Leadership;
	int Life;
	int Mana;
	int PotionEnabled;
	int PotionPercent;
	int PotionDelay;
	int PotionRecoverPercent;
	int BuffEnabled;
	int BuffSkillNumber;
	int BuffTargetMode;
	int BuffRange;
	int BuffPower;
	int BuffTime;
	int BuffDelay;
	int AttackRange;
	int SearchRange;
	int AttackDelay;
	int MoveEnabled;
	int MoveDelay;
	int RouteCount;
	int CurrentRoute;
	int TalkEnabled;
	int TalkDelay;
	int MessageCount;
	int CurrentMessage;
	int index;
	DWORD LastTargetSearchTick;
	bool TargetSearchEmpty;
	DWORD LastAttackTick;
	DWORD LastMoveTick;
	DWORD LastTalkTick;
	DWORD LastPotionTick;
	DWORD LastBuffTick;
	BOTPLAYER_BODY_ITEM body[MAX_BOTPLAYER_BODY];
	BOTPLAYER_ROUTE route[MAX_BOTPLAYER_ROUTE];
	char message[MAX_BOTPLAYER_MESSAGE][90];
};

class CBotPlayer
{
public:
	CBotPlayer();
	void Load(char* path);
	void MakeBot();
	void MainProc();
	bool IsBotPlayer(int aIndex);
	bool CanAttackBot(int aIndex);
	int SpawnBots(int count,int map,int x,int y,int dir,int classFilter=-1);
	int KillBots(int count);
	int FindNearestBot(int aIndex);
	int FindBotByName(char* name);
	void JoinOpenPvPTournament();
	void SendBotChat(LPOBJ lpBot, char* message);
private:
	int Enabled;
	int BotCount;
	int TemplateCount;
	BOTPLAYER_INFO bot[MAX_BOTPLAYER];
	void Clear();
	void RemoveBots();
	bool CreateBot(int botNum);
	void SetBotStats(int aIndex,int botNum);
	void InsertItem(int aIndex, BOTPLAYER_BODY_ITEM* lpItem, int slot);
	int SearchTarget(BOTPLAYER_INFO* lpInfo, LPOBJ lpBot);
	void AttackTarget(BOTPLAYER_INFO* lpInfo, LPOBJ lpBot, int target);
	void MoveBot(BOTPLAYER_INFO* lpInfo, LPOBJ lpBot);
	void TalkBot(BOTPLAYER_INFO* lpInfo, LPOBJ lpBot);
	void PotionBot(BOTPLAYER_INFO* lpInfo, LPOBJ lpBot);
	void BuffBot(BOTPLAYER_INFO* lpInfo, LPOBJ lpBot);
	int FindBuffTarget(BOTPLAYER_INFO* lpInfo, LPOBJ lpBot);
	bool ApplyBotBuff(BOTPLAYER_INFO* lpInfo, LPOBJ lpBot, LPOBJ lpTarget);
};

extern CBotPlayer gBotPlayer;

#endif




