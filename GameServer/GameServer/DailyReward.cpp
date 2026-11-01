#include "stdafx.h"
#include "DailyReward.h"
#include "CGMScriptLua.h"
#include "DSProtocol.h"
#include "GameMain.h"
#include "ItemManager.h"
#include "Notice.h"
#include "Util.h"

CDailyReward gDailyReward;

CDailyReward::CDailyReward()
{
	memset(this->m_Cache, 0, sizeof(this->m_Cache));
}

bool CDailyReward::GetCalendar(int& today, int& daysInMonth, int& startWeekday, DWORD& monthKey)
{
	if (g_pScriptLua == 0 || !g_pScriptLua->GetDailyRewardCalendar(today, daysInMonth, startWeekday))
	{
		return false;
	}

	SYSTEMTIME st;
	GetLocalTime(&st);
	monthKey = (DWORD)(st.wYear * 100 + st.wMonth);
	return (today >= 1 && today <= 31 && daysInMonth >= 28 && daysInMonth <= 31 && startWeekday >= 1 && startWeekday <= 7);
}

void CDailyReward::CGRequestInfoRecv(int aIndex)
{
	this->RequestLoad(aIndex, false, false);
}

void CDailyReward::CGClaimRecv(int aIndex)
{
	this->RequestLoad(aIndex, true, false);
}

void CDailyReward::CGBonusRecv(int aIndex)
{
	this->RequestLoad(aIndex, false, true);
}

void CDailyReward::RequestLoad(int aIndex, bool claim, bool bonus)
{
	if (OBJECT_RANGE(aIndex) == 0 || gObjIsConnectedGP(aIndex) == 0)
	{
		return;
	}

	int today, daysInMonth, startWeekday;
	DWORD monthKey;
	if (!this->GetCalendar(today, daysInMonth, startWeekday, monthKey))
	{
		this->SendResult(aIndex, false, bonus);
		return;
	}

	DAILY_REWARD_CACHE& cache = this->m_Cache[aIndex];
	if (cache.Loaded && cache.MonthKey == monthKey && strncmp(cache.Account, gObj[aIndex].Account, 10) == 0)
	{
		if (claim || bonus)
		{
			this->Claim(aIndex, bonus);
		}
		else
		{
			this->SendInfo(aIndex);
		}
		return;
	}

	cache.PendingClaim = claim;
	cache.PendingBonus = bonus;
	cache.Loaded = false;

	SDHP_DAILY_REWARD_LOAD_SEND pMsg;
	pMsg.header.set(0xFA, 0x00, sizeof(pMsg));
	pMsg.index = (WORD)aIndex;
	memcpy(pMsg.account, gObj[aIndex].Account, sizeof(pMsg.account));
	pMsg.MonthKey = monthKey;
	gDataServerConnection.DataSend((BYTE*)&pMsg, pMsg.header.size);
}

void CDailyReward::DGLoadRecv(SDHP_DAILY_REWARD_LOAD_RECV* lpMsg)
{
	int aIndex = lpMsg->index;
	if (OBJECT_RANGE(aIndex) == 0 || gObjIsConnectedGP(aIndex) == 0 || strncmp(gObj[aIndex].Account, lpMsg->account, 10) != 0)
	{
		return;
	}

	DAILY_REWARD_CACHE& cache = this->m_Cache[aIndex];
	memset(cache.Account, 0, sizeof(cache.Account));
	memcpy(cache.Account, lpMsg->account, 10);
	cache.Loaded = true;
	cache.MonthKey = lpMsg->MonthKey;
	cache.ClaimedMask = lpMsg->ClaimedMask;
	cache.BonusClaimed = lpMsg->BonusClaimed;

	bool claim = cache.PendingClaim;
	bool bonus = cache.PendingBonus;
	cache.PendingClaim = false;
	cache.PendingBonus = false;

	if (claim || bonus)
	{
		this->Claim(aIndex, bonus);
	}
	else
	{
		this->SendInfo(aIndex);
	}
}

bool CDailyReward::Claim(int aIndex, bool bonus)
{
	int today, daysInMonth, startWeekday;
	DWORD monthKey;
	if (!this->GetCalendar(today, daysInMonth, startWeekday, monthKey))
	{
		this->SendResult(aIndex, false, bonus);
		return false;
	}

	DAILY_REWARD_CACHE& cache = this->m_Cache[aIndex];
	if (!cache.Loaded || cache.MonthKey != monthKey)
	{
		this->RequestLoad(aIndex, !bonus, bonus);
		return false;
	}

	if (bonus)
	{
		DWORD required = (daysInMonth == 31) ? 0x7FFFFFFF : ((1u << daysInMonth) - 1u);
		if (cache.BonusClaimed || (cache.ClaimedMask & required) != required || !this->GiveReward(aIndex, 32))
		{
			this->SendResult(aIndex, false, true);
			return false;
		}
		cache.BonusClaimed = 1;
	}
	else
	{
		DWORD bit = (1u << (today - 1));
		if ((cache.ClaimedMask & bit) != 0 || !this->GiveReward(aIndex, today))
		{
			this->SendResult(aIndex, false, false);
			return false;
		}
		cache.ClaimedMask |= bit;
	}

	this->Save(aIndex);
	this->SendResult(aIndex, true, bonus);
	this->SendInfo(aIndex);
	return true;
}

bool CDailyReward::GiveReward(int aIndex, int rewardDay)
{
	int kind, section, index, level, amount, skill, luck, option, excellent, ancient, durability;
	std::string name;
	if (g_pScriptLua == 0 || !g_pScriptLua->GetDailyRewardDefinition(rewardDay, kind, section, index, level, amount,
		skill, luck, option, excellent, ancient, durability, name))
	{
		return false;
	}

	if (kind == 1)
	{
		GDSetCoinSend(aIndex, amount, 0, 0, "daily reward");
	}
	else
	{
		amount = min(max(amount, 1), 255);
		if (!gItemManager.CheckItemInventorySpace(&gObj[aIndex], GET_ITEM(section, index), amount, true, true))
		{
			gNotice.GCNoticeSend(aIndex, 1, 0, 0, 0, 0, 0, "Inventario sem espaco para todos os itens do premio diario.");
			return false;
		}

		CItem item;
		BYTE socket[MAX_SOCKET_OPTION] = { 0xFF,0xFF,0xFF,0xFF,0xFF };
		item.Convert(GET_ITEM(section, index), skill, luck, option, excellent, ancient, 0, 0, socket, 0xFF);
		item.m_Level = (BYTE)level;
		if (durability > 0)
		{
			item.m_Durability = (float)durability;
		}
		for (int count = 0; count < amount; ++count)
		{
			BYTE slot = gItemManager.InventoryInsertItem(aIndex, item);
			if (slot == 0xFF)
			{
				LogAdd(LOG_RED, "[DailyReward] Falha ao inserir item %d/%d para %s", count + 1, amount, gObj[aIndex].Name);
				return false;
			}
			PMSG_ITEM_GET_SEND pMsg;
			pMsg.header.setE(0x22, sizeof(pMsg));
			pMsg.result = slot;
			gItemManager.ItemByteConvert(pMsg.ItemInfo, item);
			DataSend(aIndex, (BYTE*)&pMsg, pMsg.header.size);
		}
	}

	gNotice.GCNoticeSend(aIndex, 1, 0, 0, 0, 0, 0, "Premio diario recebido: %s", name.c_str());
	return true;
}

void CDailyReward::Save(int aIndex)
{
	DAILY_REWARD_CACHE& cache = this->m_Cache[aIndex];
	SDHP_DAILY_REWARD_SAVE_SEND pMsg;
	pMsg.header.set(0xFA, 0x01, sizeof(pMsg));
	pMsg.index = (WORD)aIndex;
	memcpy(pMsg.account, gObj[aIndex].Account, sizeof(pMsg.account));
	pMsg.MonthKey = cache.MonthKey;
	pMsg.ClaimedMask = cache.ClaimedMask;
	pMsg.BonusClaimed = cache.BonusClaimed;
	gDataServerConnection.DataSend((BYTE*)&pMsg, pMsg.header.size);
}

void CDailyReward::SendInfo(int aIndex)
{
	int today, daysInMonth, startWeekday;
	DWORD monthKey;
	if (!this->GetCalendar(today, daysInMonth, startWeekday, monthKey)) return;
	DAILY_REWARD_CACHE& cache = this->m_Cache[aIndex];

	PMSG_DAILY_REWARD_INFO_SEND pMsg;
	pMsg.header.set(0xEC, DAILY_REWARD_GC_INFO, sizeof(pMsg));
	SYSTEMTIME rewardTime;
	GetLocalTime(&rewardTime);
	pMsg.Month = (BYTE)rewardTime.wMonth;
	pMsg.Today = (BYTE)today;
	pMsg.DaysInMonth = (BYTE)daysInMonth;
	pMsg.StartWeekday = (BYTE)startWeekday;
	memset(pMsg.ClaimedDays, 0, sizeof(pMsg.ClaimedDays));
	for (int n = 0; n < daysInMonth; ++n) pMsg.ClaimedDays[n] = ((cache.ClaimedMask & (1u << n)) != 0);
	pMsg.CanClaimToday = (pMsg.ClaimedDays[today - 1] == 0);
	DWORD required = (daysInMonth == 31) ? 0x7FFFFFFF : ((1u << daysInMonth) - 1u);
	pMsg.BonusClaimed = cache.BonusClaimed;
	pMsg.BonusAvailable = (!cache.BonusClaimed && (cache.ClaimedMask & required) == required);
	for (int n = 0; n < DAILY_REWARD_MAX_DAYS; ++n)
	{
		int kind = 0, section = 0, index = 0, level = 0, amount = 0;
		int skill = 0, luck = 0, option = 0, excellent = 0, ancient = 0, durability = 0;
		std::string name;
		if (n < daysInMonth && g_pScriptLua != 0 &&
			g_pScriptLua->GetDailyRewardDefinition(n + 1, kind, section, index, level, amount,
				skill, luck, option, excellent, ancient, durability, name))
		{
			pMsg.RewardKind[n] = (BYTE)kind;
			pMsg.RewardItemType[n] = (kind == 0) ? (WORD)GET_ITEM(section, index) : 0xFFFF;
			pMsg.RewardLevel[n] = (BYTE)level;
			pMsg.RewardAmount[n] = (WORD)min(max(amount, 0), 65535);
		}
		else
		{
			pMsg.RewardKind[n] = 0xFF;
			pMsg.RewardItemType[n] = 0xFFFF;
			pMsg.RewardLevel[n] = 0;
			pMsg.RewardAmount[n] = 0;
		}
	}
	DataSend(aIndex, (BYTE*)&pMsg, pMsg.header.size);
}

void CDailyReward::SendResult(int aIndex, bool success, bool bonus)
{
	PMSG_DAILY_REWARD_RESULT_SEND pMsg;
	pMsg.header.set(0xEC, DAILY_REWARD_GC_RESULT, sizeof(pMsg));
	pMsg.Success = success;
	pMsg.WasBonus = bonus;
	DataSend(aIndex, (BYTE*)&pMsg, pMsg.header.size);
}

void CDailyReward::ReloadConfig()
{
	LogAdd(LOG_BLUE, "[DailyReward] Configuracao Lua atualizada por hot reload.");
}
