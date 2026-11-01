#pragma once

#include "DataServerProtocol.h"

#pragma pack(push, 1)
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

void GDDailyRewardLoadRecv(SDHP_DAILY_REWARD_LOAD_SEND* lpMsg, int serverIndex);
void GDDailyRewardSaveRecv(SDHP_DAILY_REWARD_SAVE_SEND* lpMsg);
