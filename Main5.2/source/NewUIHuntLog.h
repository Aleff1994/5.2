#pragma once
#include "Protocol.h"
class CHARACTER;
#define HUNT_LOG_MAX_DROPS 8
#pragma pack(push,1)
struct PMSG_HUNT_LOG_REQUEST_SEND { PSBMSG_HEAD header; WORD TargetIndex; };
struct PMSG_HUNT_LOG_DROP_RECV { WORD ItemIndex; DWORD Rate; };
struct PMSG_HUNT_LOG_INFO_RECV { PSWMSG_HEAD header; WORD TargetIndex; WORD MonsterClass; WORD Level; DWORD Life; DWORD MaxLife; DWORD Zen; unsigned __int64 Experience; DWORD DamageContribution; BYTE IsBoss; BYTE DropCount; BYTE CommonPoolCount; PMSG_HUNT_LOG_DROP_RECV Drops[HUNT_LOG_MAX_DROPS]; };
#pragma pack(pop)
namespace SEASON3B
{
	class CNewUIHuntLog
	{
	public:
		CNewUIHuntLog();
		void OnMonsterDamage(CHARACTER* target, unsigned int damage);
		void OnMonsterKilled(CHARACTER* target, unsigned int experience);
		void OnGroundDrop(int itemType, unsigned int value, int x, int y);
		void ReceiveInfo(PMSG_HUNT_LOG_INFO_RECV* info);
		void Render();
		void Clear();
	private:
		int m_TargetKey;
		unsigned int m_LastDamage;
		unsigned long long m_TotalDamage;
		DWORD m_ServerLife, m_ServerMaxLife, m_Zen, m_LastRequest;
		DWORD m_ServerDamageContribution;
		bool m_IsBoss;
		unsigned __int64 m_Experience;
		unsigned __int64 m_LastRealExperience;
		DWORD m_LastRealZen, m_LastKillTick;
		int m_LastKillX, m_LastKillY, m_LastDropType;
		int m_CurrentMonsterClass, m_LastKilledMonsterClass;
		BYTE m_DropCount;
		BYTE m_CommonPoolCount;
		PMSG_HUNT_LOG_DROP_RECV m_Drops[HUNT_LOG_MAX_DROPS];
		void SendRequest(WORD targetIndex);
	};
	extern CNewUIHuntLog g_HuntLog;
}
