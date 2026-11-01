#pragma once

#include "Protocol.h"

#define MAX_BOSS_SPAWN_CAMERA 100

struct BOSS_SPAWN_CAMERA_CONFIG
{
	int Enabled;
	int MonsterIndex;
	int MapNumber;
	int DurationSeconds;
	int CameraLevel;
};

#pragma pack(push, 1)
struct PMSG_BOSS_SPAWN_CAMERA_SEND
{
	PSWMSG_HEAD header;
	int Enabled;
	int ApproachMilliseconds;
	int ReturnMilliseconds;
	int Count;
	BOSS_SPAWN_CAMERA_CONFIG Boss[MAX_BOSS_SPAWN_CAMERA];
};

struct PMSG_BOSS_SPAWN_CAMERA_START_SEND
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
#pragma pack(pop)

class CBossSpawnCamera
{
public:
	CBossSpawnCamera();
	void Load(const char* path);
	void GCSend(int aIndex);
	void GCStartSend(int aIndex, int objectIndex);

private:
	int m_Enabled;
	int m_ApproachMilliseconds;
	int m_ReturnMilliseconds;
	std::vector<BOSS_SPAWN_CAMERA_CONFIG> m_Boss;
	int m_LastObjectIndex;
	DWORD m_LastObjectTick;
};

extern CBossSpawnCamera gBossSpawnCamera;
