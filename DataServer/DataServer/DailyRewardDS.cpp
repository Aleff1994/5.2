#include "stdafx.h"
#include "DailyRewardDS.h"
#include "QueryManager.h"
#include "SocketManager.h"

static void EnsureDailyRewardTable()
{
	static bool initialized = false;
	if (initialized) return;

	gQueryManager.ExecQuery(
		"IF OBJECT_ID('dbo.DailyRewardAccount','U') IS NULL "
		"CREATE TABLE dbo.DailyRewardAccount ("
		"AccountID varchar(10) NOT NULL, MonthKey int NOT NULL, "
		"ClaimedMask int NOT NULL CONSTRAINT DF_DailyReward_Claimed DEFAULT(0), "
		"BonusClaimed tinyint NOT NULL CONSTRAINT DF_DailyReward_Bonus DEFAULT(0), "
		"UpdatedAt datetime NOT NULL CONSTRAINT DF_DailyReward_Updated DEFAULT(GETDATE()), "
		"CONSTRAINT PK_DailyRewardAccount PRIMARY KEY(AccountID,MonthKey))");
	gQueryManager.Close();
	initialized = true;
}

void GDDailyRewardLoadRecv(SDHP_DAILY_REWARD_LOAD_SEND* lpMsg, int serverIndex)
{
	EnsureDailyRewardTable();

	SDHP_DAILY_REWARD_LOAD_RECV pMsg;
	pMsg.header.set(0xFA, 0x00, sizeof(pMsg));
	pMsg.index = lpMsg->index;
	memcpy(pMsg.account, lpMsg->account, sizeof(pMsg.account));
	pMsg.MonthKey = lpMsg->MonthKey;
	pMsg.ClaimedMask = 0;
	pMsg.BonusClaimed = 0;

	if (gQueryManager.ExecQuery("SELECT ClaimedMask,BonusClaimed FROM DailyRewardAccount WHERE AccountID='%s' AND MonthKey=%d", lpMsg->account, lpMsg->MonthKey) != 0 && gQueryManager.Fetch() != SQL_NO_DATA)
	{
		pMsg.ClaimedMask = (DWORD)gQueryManager.GetAsInteger("ClaimedMask");
		pMsg.BonusClaimed = (BYTE)gQueryManager.GetAsInteger("BonusClaimed");
	}
	gQueryManager.Close();
	gSocketManager.DataSend(serverIndex, (BYTE*)&pMsg, pMsg.header.size);
}

void GDDailyRewardSaveRecv(SDHP_DAILY_REWARD_SAVE_SEND* lpMsg)
{
	EnsureDailyRewardTable();
	gQueryManager.ExecQuery(
		"UPDATE DailyRewardAccount SET ClaimedMask=%u,BonusClaimed=%u,UpdatedAt=GETDATE() WHERE AccountID='%s' AND MonthKey=%u; "
		"IF @@ROWCOUNT=0 INSERT INTO DailyRewardAccount(AccountID,MonthKey,ClaimedMask,BonusClaimed) VALUES('%s',%u,%u,%u)",
		lpMsg->ClaimedMask, lpMsg->BonusClaimed, lpMsg->account, lpMsg->MonthKey,
		lpMsg->account, lpMsg->MonthKey, lpMsg->ClaimedMask, lpMsg->BonusClaimed);
	gQueryManager.Close();
}
