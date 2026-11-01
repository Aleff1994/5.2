#pragma once

#include "NewUIBase.h"
#include "Protocol.h"

#define DAILY_REWARD_MAX_DAYS 31

#pragma pack(push, 1)
struct PMSG_DAILY_REWARD_REQUEST_SEND { PSBMSG_HEAD header; };
struct PMSG_DAILY_REWARD_CLAIM_SEND { PSBMSG_HEAD header; };
struct PMSG_DAILY_REWARD_BONUS_SEND { PSBMSG_HEAD header; };
struct PMSG_DAILY_REWARD_INFO_RECV
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
struct PMSG_DAILY_REWARD_RESULT_RECV
{
	PSBMSG_HEAD header;
	BYTE Success;
	BYTE WasBonus;
};
#pragma pack(pop)

namespace SEASON3B
{
	class CNewUIDailyReward
	{
	public:
		CNewUIDailyReward();
		void Open();
		void Close();
		bool IsOpen() const;
		bool IsEnabled() const;
		bool IsMouseInPanel() const;
		bool UpdateKeyEvent();
		void ReceiveInfo(PMSG_DAILY_REWARD_INFO_RECV* lpMsg);
		void ReceiveResult(PMSG_DAILY_REWARD_RESULT_RECV* lpMsg);
		void Render();
	private:
		bool m_Open;
		bool m_TextureLoaded;
		bool m_IconLoaded;
		bool m_ButtonLoaded;
	bool m_DataLoaded;
	BYTE m_Month;
	BYTE m_Today;
		BYTE m_DaysInMonth;
		BYTE m_StartWeekday;
		BYTE m_CanClaimToday;
		BYTE m_ClaimedDays[DAILY_REWARD_MAX_DAYS];
		BYTE m_RewardKind[DAILY_REWARD_MAX_DAYS];
		WORD m_RewardItemType[DAILY_REWARD_MAX_DAYS];
		BYTE m_RewardLevel[DAILY_REWARD_MAX_DAYS];
		WORD m_RewardAmount[DAILY_REWARD_MAX_DAYS];
		BYTE m_BonusAvailable;
		BYTE m_BonusClaimed;
		DWORD m_LastRequest;
		float m_IconX, m_IconY, m_DragOffsetX, m_DragOffsetY; bool m_IconPositioned, m_IconDragging, m_IconMoved;
		enum { BITMAP_DAILY_REWARD_PANEL = 35701, BITMAP_DAILY_REWARD_ICON = 35702, BITMAP_DAILY_REWARD_BUTTON = 35703 };
		void SendRequest(BYTE subCode);
		void RenderLauncher();
		void RenderTextLine(int x, int y, const char* text, int width, int sort, BYTE r, BYTE g, BYTE b);
	};

	extern CNewUIDailyReward g_DailyRewardUI;

	#define LUCKY_WHEEL_MAX_REWARDS 16
	#pragma pack(push, 1)
	struct PMSG_LUCKY_WHEEL_REWARD_RECV
	{
		BYTE Kind; WORD ItemType; BYTE Level; DWORD Amount; BYTE Skill; BYTE Luck; BYTE Option;
		BYTE Excellent; BYTE Ancient; BYTE Harmony; BYTE ItemOptionEx; BYTE SocketBonus; BYTE Socket[5]; char Name[32];
	};
	struct PMSG_LUCKY_WHEEL_INFO_RECV
	{
		PSWMSG_HEAD header; BYTE Enabled; BYTE Pool; BYTE TabEnabled[4]; BYTE RewardCount; BYTE FreeRemaining; BYTE DailyRemaining;
		DWORD WCoinCost; DWORD WCoinPCost; DWORD GoblinPointCost; DWORD ZenCost; WORD TicketType; BYTE TicketCount;
		PMSG_LUCKY_WHEEL_REWARD_RECV Reward[LUCKY_WHEEL_MAX_REWARDS];
	};
	struct PMSG_LUCKY_WHEEL_RESULT_RECV { PSBMSG_HEAD header; BYTE Success; BYTE RewardIndex; };
	struct PMSG_LUCKY_WHEEL_CLAIM_RECV { PSBMSG_HEAD header; BYTE Success; };
	struct PMSG_LUCKY_WHEEL_REQUEST_SEND { PSBMSG_HEAD header; BYTE Pool; };
	struct PMSG_LUCKY_WHEEL_SPIN_SEND { PSBMSG_HEAD header; BYTE Payment; };
	struct PMSG_LUCKY_WHEEL_CLAIM_SEND { PSBMSG_HEAD header; };
	#pragma pack(pop)

	class CNewUILuckyWheel
	{
	public:
		CNewUILuckyWheel();
		~CNewUILuckyWheel();
		void Open(); void Close(); bool IsOpen() const; bool IsEnabled() const; bool IsMouseInPanel() const; bool UpdateKeyEvent();
		void ReceiveInfo(PMSG_LUCKY_WHEEL_INFO_RECV* lpMsg);
		void ReceiveResult(PMSG_LUCKY_WHEEL_RESULT_RECV* lpMsg);
		void ReceiveClaim(PMSG_LUCKY_WHEEL_CLAIM_RECV* lpMsg);
		void Render();
	private:
		bool m_Open, m_Loaded, m_Spinning, m_Waiting, m_TextureLoaded, m_ClaimReady, m_Claiming;
		BYTE m_Count, m_FreeRemaining, m_DailyRemaining, m_Highlight, m_Target, m_Pool, m_TabEnabled[4];
		DWORD m_WCoinCost, m_WCoinPCost, m_GPCost, m_ZenCost, m_SpinStart, m_LastStep;
		float m_IconX, m_IconY, m_DragOffsetX, m_DragOffsetY; bool m_IconPositioned, m_IconDragging, m_IconMoved;
		WORD m_TicketType; BYTE m_TicketCount;
		PMSG_LUCKY_WHEEL_REWARD_RECV m_Rewards[LUCKY_WHEEL_MAX_REWARDS];
		ITEM* m_Items[LUCKY_WHEEL_MAX_REWARDS];
		enum { BITMAP_LUCKY_WHEEL_PANEL = 35720, BITMAP_LUCKY_WHEEL_LOGO = 35721 };
		void ClearItems(); void SendInfo(); void SendSpin(BYTE payment); void SendClaim();
		void Text(int x,int y,const char* text,int width,int sort,BYTE r,BYTE g,BYTE b);
		void StyledButton(float x,float y,float width,float height,const char* text,bool hover,bool active=true);
	};

	extern CNewUILuckyWheel g_LuckyWheelUI;
}
