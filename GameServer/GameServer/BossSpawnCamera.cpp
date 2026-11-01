#include "stdafx.h"
#include "BossSpawnCamera.h"
#include "Log.h"
#include "Util.h"

CBossSpawnCamera gBossSpawnCamera;

CBossSpawnCamera::CBossSpawnCamera()
{
	m_Enabled = 0;
	m_ApproachMilliseconds = 700;
	m_ReturnMilliseconds = 700;
	m_LastObjectIndex = -1;
	m_LastObjectTick = 0;
}

void CBossSpawnCamera::Load(const char* path)
{
	m_Boss.clear();
	m_Enabled = 0;
	m_ApproachMilliseconds = 700;
	m_ReturnMilliseconds = 700;

	FILE* file = NULL;

	if (fopen_s(&file, path, "r") != 0 || file == NULL)
	{
		LogAdd(LOG_RED, "[BossSpawnCamera] Nao foi possivel abrir: %s", path);
		return;
	}

	int section = -1;
	char line[512];

	while (fgets(line, sizeof(line), file) != NULL)
	{
		char* cursor = line;

		while (*cursor == ' ' || *cursor == '\t')
			++cursor;

		if (*cursor == 0 || *cursor == '\r' || *cursor == '\n' ||
			(cursor[0] == '/' && cursor[1] == '/'))
			continue;

		if (_strnicmp(cursor, "end", 3) == 0)
		{
			section = -1;
			continue;
		}

		if (section == -1)
		{
			if (sscanf_s(cursor, "%d", &section) != 1)
				section = -1;

			continue;
		}

		if (section == 0)
		{
			int enabled;
			int approach;
			int returnTime;

			if (sscanf_s(cursor, "%d %d %d", &enabled, &approach, &returnTime) == 3)
			{
				m_Enabled = enabled;
				m_ApproachMilliseconds = max(100, approach);
				m_ReturnMilliseconds = max(100, returnTime);
			}
		}
		else if (section == 1)
		{
			BOSS_SPAWN_CAMERA_CONFIG info;

			if (sscanf_s(cursor, "%d %d %d %d %d", &info.Enabled,
				&info.MonsterIndex, &info.MapNumber, &info.DurationSeconds,
				&info.CameraLevel) != 5)
				continue;

			if (info.Enabled != 0 && info.MonsterIndex >= 0 &&
				info.DurationSeconds > 0 && m_Boss.size() < MAX_BOSS_SPAWN_CAMERA)
			{
				info.CameraLevel = min(5, max(0, info.CameraLevel));
				m_Boss.push_back(info);
			}
		}
	}

	fclose(file);

	LogAdd(LOG_BLUE, "[BossSpawnCamera] Sistema: %d, bosses carregados: %d",
		m_Enabled, (int)m_Boss.size());
}
void CBossSpawnCamera::GCSend(int aIndex)
{
	PMSG_BOSS_SPAWN_CAMERA_SEND pMsg = {};
	pMsg.Enabled = m_Enabled;
	pMsg.ApproachMilliseconds = m_ApproachMilliseconds;
	pMsg.ReturnMilliseconds = m_ReturnMilliseconds;
	pMsg.Count = min((int)m_Boss.size(), MAX_BOSS_SPAWN_CAMERA);

	for (int index = 0; index < pMsg.Count; ++index)
		pMsg.Boss[index] = m_Boss[index];

	WORD size = sizeof(PSWMSG_HEAD) + (sizeof(int) * 4) +
		(sizeof(BOSS_SPAWN_CAMERA_CONFIG) * pMsg.Count);
	pMsg.header.set(0xF3, 0xF4, size);
	DataSend(aIndex, (BYTE*)&pMsg, size);
}

void CBossSpawnCamera::GCStartSend(int aIndex, int objectIndex)
{
	if (m_Enabled == 0 || !OBJECT_RANGE(objectIndex))
		return;

	LPOBJ monster = &gObj[objectIndex];
	if (monster->Type != OBJECT_MONSTER && monster->Type != OBJECT_NPC)
		return;

	DWORD now = GetTickCount();
	if (m_LastObjectIndex == objectIndex && now - m_LastObjectTick < 3000)
		return;

	for (std::vector<BOSS_SPAWN_CAMERA_CONFIG>::const_iterator it = m_Boss.begin();
		it != m_Boss.end(); ++it)
	{
		if (!it->Enabled || it->MonsterIndex != monster->Class ||
			(it->MapNumber != -1 && it->MapNumber != monster->Map))
			continue;

		m_LastObjectIndex = objectIndex;
		m_LastObjectTick = now;

		PMSG_BOSS_SPAWN_CAMERA_START_SEND pMsg = {};
		pMsg.header.set(0xF3, 0xF5, sizeof(pMsg));
		pMsg.MonsterIndex = monster->Class;
		pMsg.ObjectIndex = monster->Index;
		pMsg.MapNumber = monster->Map;
		pMsg.X = monster->X;
		pMsg.Y = monster->Y;
		pMsg.DurationSeconds = it->DurationSeconds;
		pMsg.CameraLevel = it->CameraLevel;

		for (int n = OBJECT_START_USER; n < MAX_OBJECT; ++n)
		{
			if (!OBJECT_RANGE(n) || gObj[n].Type != OBJECT_USER ||
				gObj[n].Connected < OBJECT_ONLINE || gObj[n].Map != monster->Map)
				continue;

			DataSend(n, (BYTE*)&pMsg, sizeof(pMsg));
		}
		return;
	}
}
