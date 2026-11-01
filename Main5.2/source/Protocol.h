#pragma once
#include "WSclient.h"
#define SET_NUMBERHB(x) ((BYTE)((DWORD)(x)>>(DWORD)8))
#define SET_NUMBERLB(x) ((BYTE)((DWORD)(x)&0xFF))
#define SET_NUMBERHW(x) ((WORD)((DWORD)(x)>>(DWORD)16))
#define SET_NUMBERLW(x) ((WORD)((DWORD)(x)&0xFFFF))
#define SET_NUMBERHDW(x) ((DWORD)((QWORD)(x)>>(QWORD)32))
#define SET_NUMBERLDW(x) ((DWORD)((QWORD)(x)&0xFFFFFFFF))

#define MAKE_NUMBERW(x,y) ((WORD)(((BYTE)((y)&0xFF))|((BYTE)((x)&0xFF)<<8)))
#define MAKE_NUMBERDW(x,y) ((DWORD)(((WORD)((y)&0xFFFF))|((WORD)((x)&0xFFFF)<<16)))
#define MAKE_NUMBERQW(x,y) ((QWORD)(((DWORD)((y)&0xFFFFFFFF))|((DWORD)((x)&0xFFFFFFFF)<<32)))
//===Move Item
struct PMSG_ITEM_MOVE_RECV
{
	PSBMSG_HEAD h;
	BYTE sFlag;
	BYTE tFlag;
	BYTE Source;
	BYTE Target;
};
struct XULY_CGPACKET
{
	PSBMSG_HEAD header; // C3:F3:03
	DWORD ThaoTac;
};
struct SEND_COUNTLIST
{
	PSWMSG_HEAD header;
	int Count;
	BYTE Type;
};

struct RANK_INFO_SEND
{
	char NameRank[128];
};
#define MAX_BOSS_SPAWN_CAMERA_NETWORK 100

#pragma pack(push, 1)
struct BOSS_SPAWN_CAMERA_NETWORK_INFO
{
	int Enabled;
	int MonsterIndex;
	int MapNumber;
	int DurationSeconds;
	int CameraLevel;
};

struct PMSG_BOSS_SPAWN_CAMERA_RECV
{
	PSWMSG_HEAD header;
	int Enabled;
	int ApproachMilliseconds;
	int ReturnMilliseconds;
	int Count;
	BOSS_SPAWN_CAMERA_NETWORK_INFO Boss[MAX_BOSS_SPAWN_CAMERA_NETWORK];
};

struct PMSG_BOSS_SPAWN_CAMERA_START_RECV
{
	PSBMSG_HEAD header;
	int MonsterIndex;
	int ObjectIndex;
	int MapNumber;
	int X;
	int Y;
	int DurationSeconds;
	int CameraLevel;
};

struct PMSG_WAREHOUSE_PAGE_INFO_RECV
{
	PSBMSG_HEAD header;
	BYTE CurrentWarehouse;
	BYTE MaxWarehouse;
	BYTE RequestId;
};

struct PMSG_BUILD_SET_BONUS_RECV
{
	PSBMSG_HEAD header;
	BYTE Enabled;
};

struct PMSG_DYNAMIC_MINIMAP_HEAD_RECV
{
	PSWMSG_HEAD header;
	BYTE map;
	BYTE reset;
	WORD count;
};

struct PMSG_DYNAMIC_MINIMAP_ENTRY_RECV
{
	BYTE kind;
	BYTE x;
	BYTE y;
	BYTE tx;
	BYTE ty;
	WORD id;
	char text[32];
};

struct PMSG_MINIMAP_GM_TELEPORT_SEND
{
	PSBMSG_HEAD header; // C1:FA:03
	BYTE map;
	BYTE x;
	BYTE y;
};
#pragma pack(pop)

extern bool gBuildSetBonusEnabled;
extern std::vector <std::string> m_DataSelectNameTop;
BOOL ProtocolCoreEx(BYTE head, BYTE* lpMsg, int size, int key);

