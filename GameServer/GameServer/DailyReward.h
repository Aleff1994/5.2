#pragma once

#include "Protocol.h"

#define DAILY_REWARD_MAX_DAYS 31

enum eDailyRewardClientSubCode
{
	DAILY_REWARD_CG_INFO = 0x21,
	DAILY_REWARD_GC_INFO = 0x22,
	DAILY_REWARD_CG_CLAIM = 0x23,
	DAILY_REWARD_GC_RESULT = 0x24,
	DAILY_REWARD_CG_BONUS = 0x25,
};

enum eDailyRewardClientRequestSubCode
{
	DAILY_REWARD_F3_INFO = 0xEA,
	DAILY_REWARD_F3_CLAIM = 0xEF,
	DAILY_REWARD_F3_BONUS = 0xF1,
};

#pragma pack(push, 1)
struct PMSG_DAILY_REWARD_REQUEST_RECV { PSBMSG_HEAD header; };
struct PMSG_DAILY_REWARD_CLAIM_RECV { PSBMSG_HEAD header; };
struct PMSG_DAILY_REWARD_BONUS_RECV { PSBMSG_HEAD header; };

struct PMSG_DAILY_REWARD_INFO_SEND
{
	PSBMSG_HEAD header;
	BYTE Month;
	BYTE Today;
	BYTE DaysInMonth;
	BYTE StartWeekday;
	BYTE CanClaimToday;
	BYTE ClaimedDays[DAILY_REWARD_MAX_DAYS];
	BYTE BonusAvailable;
	BYTE BonusClaimed;
	BYTE RewardKind[DAILY_REWARD_MAX_DAYS];
	WORD RewardItemType[DAILY_REWARD_MAX_DAYS];
	BYTE RewardLevel[DAILY_REWARD_MAX_DAYS];
	WORD RewardAmount[DAILY_REWARD_MAX_DAYS];
};

struct PMSG_DAILY_REWARD_RESULT_SEND
{
	PSBMSG_HEAD header;
	BYTE Success;
	BYTE WasBonus;
};

struct SDHP_DAILY_REWARD_LOAD_SEND
{
	PSBMSG_HEAD header;
	WORD index;
	char account[11];
	DWORD MonthKey;
};

struct SDHP_DAILY_REWARD_LOAD_RECV
{
	PSBMSG_HEAD header;
	WORD index;
	char account[11];
	DWORD MonthKey;
	DWORD ClaimedMask;
	BYTE BonusClaimed;
};

struct SDHP_DAILY_REWARD_SAVE_SEND
{
	PSBMSG_HEAD header;
	WORD index;
	char account[11];
	DWORD MonthKey;
	DWORD ClaimedMask;
	BYTE BonusClaimed;
};
#pragma pack(pop)

struct DAILY_REWARD_CACHE
{
	char Account[11];
	bool Loaded;
	bool PendingClaim;
	bool PendingBonus;
	DWORD MonthKey;
	DWORD ClaimedMask;
	BYTE BonusClaimed;
};

class CDailyReward
{
public:
	CDailyReward();
	void CGRequestInfoRecv(int aIndex);
	void CGClaimRecv(int aIndex);
	void CGBonusRecv(int aIndex);
	void DGLoadRecv(SDHP_DAILY_REWARD_LOAD_RECV* lpMsg);
	void ReloadConfig();
private:
	DAILY_REWARD_CACHE m_Cache[MAX_OBJECT];
	void RequestLoad(int aIndex, bool claim, bool bonus);
	void Save(int aIndex);
	void SendInfo(int aIndex);
	void SendResult(int aIndex, bool success, bool bonus);
	bool Claim(int aIndex, bool bonus);
	bool GiveReward(int aIndex, int rewardDay);
	bool GetCalendar(int& today, int& daysInMonth, int& startWeekday, DWORD& monthKey);
};

extern CDailyReward gDailyReward;
