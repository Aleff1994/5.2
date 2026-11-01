#pragma once
#include "Protocol.h"
#include <vector>

#define HUNT_LOG_MAX_DROPS 8
#define HUNT_LOG_REQUEST_SUB 0xF2
#define HUNT_LOG_RESPONSE_SUB 0x26

#pragma pack(push,1)
struct PMSG_HUNT_LOG_REQUEST_RECV { PSBMSG_HEAD header; WORD TargetIndex; };
struct PMSG_HUNT_LOG_DROP_SEND { WORD ItemIndex; DWORD Rate; };
struct PMSG_HUNT_LOG_INFO_SEND
{
	PSWMSG_HEAD header;
	WORD TargetIndex;
	WORD MonsterClass;
	WORD Level;
	DWORD Life;
	DWORD MaxLife;
	DWORD Zen;
	QWORD Experience;
	DWORD DamageContribution;
	BYTE IsBoss;
	BYTE DropCount;
	BYTE CommonPoolCount;
	PMSG_HUNT_LOG_DROP_SEND Drops[HUNT_LOG_MAX_DROPS];
};
#pragma pack(pop)

class CHuntLog
{
public:
	void Load(char* path);
	bool IsEnabledMonster(int monsterClass) const;
	void CGRequestRecv(PMSG_HUNT_LOG_REQUEST_RECV* lpMsg,int aIndex);
private:
	std::vector<int> m_MonsterClasses;
};
extern CHuntLog gHuntLog;
