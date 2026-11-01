#include "stdafx.h"
#include "CComGem.h"
#include "ZzzOpenglUtil.h"
#include "ZzzTexture.h"
#include "ZzzOpenData.h"
#include "ZzzEffect.h"
#include "ZzzAI.h"
#include "ZzzCharacter.h"
#include "ZzzLodTerrain.h"
#include "ZzzInterface.h"
#include "CDirection.h"
#include "wsclientinline.h"
#include "MapManager.h"

static CDirection Direction;

CDirection::CDirection()
{
	m_bBossCameraLoaded = false;
	m_bBossCameraSystemEnabled = false;
	m_dwBossCameraApproachMs = 700;
	m_dwBossCameraReturnMs = 700;
	m_BossCameraInfo.clear();
	Init();
}

CDirection::~CDirection()
{

}

void CDirection::Init()
{
	Vector(0.0f, 0.0f, 0.0f, m_vCameraPosition);
	Vector(0.0f, 0.0f, 0.0f, m_v1stPosition);
	Vector(0.0f, 0.0f, 0.0f, m_v2ndPosition);
	Vector(0.0f, 0.0f, 0.0f, m_vResult);

	m_bStateCheck = true;
	m_bCameraCheck = false;
	m_bAction = true;
	m_bTimeCheck = true;
	m_bOrderExit = false;

	m_bDownHero = false;

	m_fCount = 0.0f;
	m_fLength = 0.0f;
	m_fCameraSpeed = 100.0f;
	m_fCameraViewFar = 1000.0f;

	m_iCheckTime = 0;
	m_iTimeSchedule = 0;
	m_iBackupTime = 0;
	m_CameraLevel = 0;

	m_AngleY = 0.0f;

	m_bBossCameraActive = false;
	m_bBossCameraPending = false;
	m_iBossCameraTargetKey = -1;
	m_iBossCameraPendingMonsterIndex = -1;
	m_iBossCameraPendingObjectIndex = -1;
	m_iBossCameraPendingMapNumber = -1;
	m_iBossCameraPendingX = 0;
	m_iBossCameraPendingY = 0;
	m_iBossCameraPendingDurationSeconds = 0;
	m_iBossCameraPendingLevel = 2;
	m_pBossCameraTarget = NULL;
	m_bBossCameraLocalFocus = false;
	Vector(0.0f, 0.0f, 0.0f, m_vBossCameraLocalPosition);
	m_iBossCameraLevel = 2;
	m_dwBossCameraStart = 0;
	m_dwBossCameraEnd = 0;
	m_dwBossCameraPendingExpire = 0;
	Vector(0.0f, 0.0f, 0.0f, m_vBossCameraStartPosition);

	m_CMVP.Init();
	m_CKanturu.Init();
}

void CDirection::CloseAllWindows()
{
	SEASON3B::CNewUIInventoryCtrl::BackupPickedItem();
	g_pNewUISystem->HideAll();
}

bool CDirection::IsDirection()
{
	if (m_bBossCameraActive)
		return true;

	if (gMapManager->currentMap == WD_34CRYWOLF_1ST)
		return m_CMVP.IsCryWolfDirection();
	else if (gMapManager->currentMap == WD_39KANTURU_3RD)
		return m_CKanturu.IsKanturuDirection();

	return false;
}

void CDirection::CheckDirection()
{
	UpdateBossCamera();
	m_CMVP.CryWolfDirection();
	m_CKanturu.KanturuAllDirection();
}


bool CDirection::IsBossCameraActive() const
{
	return m_bBossCameraActive;
}

void CDirection::BeginBossCameraConfig(bool Enabled, DWORD ApproachMs, DWORD ReturnMs)
{
	m_bBossCameraLoaded = true;
	m_bBossCameraSystemEnabled = Enabled;
	m_BossCameraInfo.clear();
	m_dwBossCameraApproachMs = min((DWORD)5000, max((DWORD)0, ApproachMs));
	m_dwBossCameraReturnMs = min((DWORD)5000, max((DWORD)0, ReturnMs));
}

void CDirection::AddBossCameraConfig(int Enabled, int MonsterIndex, int MapNumber,
	int DurationSeconds, int CameraLevel)
{
	if (MonsterIndex < 0 || DurationSeconds <= 0 || m_BossCameraInfo.size() >= 100)
		return;

	BOSS_SPAWN_CAMERA_INFO info = { 0 };
	info.Enabled = Enabled;
	info.MonsterIndex = MonsterIndex;
	info.MapNumber = MapNumber;
	info.DurationMs = DurationSeconds * 1000;
	info.CameraLevel = min(5, max(0, CameraLevel));
	m_BossCameraInfo.push_back(info);
}

void CDirection::StartBossCameraFromServer(int MonsterIndex, int TargetKey, int MapNumber,
    int X, int Y, int DurationSeconds, int CameraLevel)
{
    if (SceneFlag != MAIN_SCENE || Hero == NULL || DurationSeconds <= 0 ||
        MonsterIndex < 0 || TargetKey < 0 || MapNumber != gMapManager->currentMap ||
        X < 0 || X > 255 || Y < 0 || Y > 255)
        return;
    if (m_CMVP.IsCryWolfDirection() || m_CKanturu.IsKanturuDirection())
        return;

    m_bBossCameraPending = true;
    m_iBossCameraPendingMonsterIndex = MonsterIndex;
    m_iBossCameraPendingObjectIndex = TargetKey;
    m_iBossCameraPendingMapNumber = MapNumber;
    m_iBossCameraPendingX = X;
    m_iBossCameraPendingY = Y;
    m_iBossCameraPendingDurationSeconds = min(600, DurationSeconds);
    m_iBossCameraPendingLevel = min(5, max(0, CameraLevel));
    m_dwBossCameraPendingExpire = timeGetTime() + 5000;
    StartPendingBossCamera();
}

bool CDirection::StartPendingBossCamera()
{
    if (!m_bBossCameraPending)
        return false;
    const DWORD now = timeGetTime();
    if ((LONG)(now - m_dwBossCameraPendingExpire) >= 0 || Hero == NULL ||
        m_iBossCameraPendingMapNumber != gMapManager->currentMap)
    {
        m_bBossCameraPending = false;
        return false;
    }
    for (int index = 0; index < MAX_CHARACTERS_CLIENT; ++index)
    {
        CHARACTER* target = gmCharacters->GetCharacter(index);
        if (target != NULL && target != Hero && target->Object.Live &&
            target->MonsterIndex == m_iBossCameraPendingMonsterIndex &&
            target->Key == m_iBossCameraPendingObjectIndex)
        {
            StartBossCamera(target, m_iBossCameraPendingDurationSeconds, m_iBossCameraPendingLevel);
            m_bBossCameraPending = false;
            return true;
        }
    }

    // The event packet can precede viewport creation. Focus coordinates only:
    // CreateMonster here would add a second, renderable boss to the client pool.
    m_vBossCameraLocalPosition[0] = (m_iBossCameraPendingX + 0.5f) * TERRAIN_SCALE;
    m_vBossCameraLocalPosition[1] = (m_iBossCameraPendingY + 0.5f) * TERRAIN_SCALE;
    m_vBossCameraLocalPosition[2] = RequestTerrainHeight(m_vBossCameraLocalPosition[0], m_vBossCameraLocalPosition[1]) + 100.0f;
    m_pBossCameraTarget = NULL;
    m_iBossCameraTargetKey = m_iBossCameraPendingObjectIndex;
    m_bBossCameraLocalFocus = true;
    m_bBossCameraActive = true;
    m_bBossCameraPending = false;
    m_iBossCameraLevel = m_iBossCameraPendingLevel;
    m_dwBossCameraStart = now;
    m_dwBossCameraEnd = now + m_iBossCameraPendingDurationSeconds * 1000;
    VectorCopy(Hero->Object.Position, m_vBossCameraStartPosition);
    VectorCopy(m_vBossCameraStartPosition, m_vCameraPosition);
    return true;
}

void CDirection::StartBossCamera(CHARACTER* Target, int DurationSeconds, int CameraLevel)
{
    if (Target == NULL || Target == Hero || !Target->Object.Live || Hero == NULL)
        return;
    m_bBossCameraActive = true;
    m_bBossCameraLocalFocus = false;
    m_iBossCameraTargetKey = Target->Key;
    m_pBossCameraTarget = Target;
    m_iBossCameraLevel = min(5, max(0, CameraLevel));
    m_dwBossCameraStart = timeGetTime();
    m_dwBossCameraEnd = m_dwBossCameraStart + (DurationSeconds * 1000);
    VectorCopy(Hero->Object.Position, m_vBossCameraStartPosition);
    VectorCopy(m_vBossCameraStartPosition, m_vCameraPosition);
}

void CDirection::UpdateBossCamera()
{
    StartPendingBossCamera();
    if (!m_bBossCameraActive)
        return;
    const DWORD now = timeGetTime();
    if (Hero == NULL || SceneFlag != MAIN_SCENE ||
        m_iBossCameraPendingMapNumber != gMapManager->currentMap ||
        (LONG)(now - m_dwBossCameraEnd) >= 0)
    {
        m_bBossCameraActive = false;
        m_bBossCameraLocalFocus = false;
        m_iBossCameraTargetKey = -1;
        m_pBossCameraTarget = NULL;
        return;
    }
    if (m_bBossCameraLocalFocus)
    {
        // Adopt only the announced server instance, never another boss of its type.
        for (int index = 0; index < MAX_CHARACTERS_CLIENT; ++index)
        {
            CHARACTER* candidate = gmCharacters->GetCharacter(index);
            if (candidate != NULL && candidate != Hero && candidate->Object.Live &&
                candidate->Key == m_iBossCameraTargetKey &&
                candidate->MonsterIndex == m_iBossCameraPendingMonsterIndex)
            {
                m_pBossCameraTarget = candidate;
                m_bBossCameraLocalFocus = false;
                break;
            }
        }
    }
    CHARACTER* target = m_pBossCameraTarget;
    if (!m_bBossCameraLocalFocus && (target == NULL || !target->Object.Live ||
        target->Key != m_iBossCameraTargetKey || target->MonsterIndex != m_iBossCameraPendingMonsterIndex))
    {
        m_bBossCameraActive = false;
        m_pBossCameraTarget = NULL;
        m_iBossCameraTargetKey = -1;
        return;
    }
    vec3_t targetPosition;
    if (m_bBossCameraLocalFocus)
        VectorCopy(m_vBossCameraLocalPosition, targetPosition);
    else
    {
        VectorCopy(target->Object.Position, targetPosition);
        targetPosition[2] += target->Object.BoundingBoxMax[2] * 0.35f;
    }
    const DWORD elapsed = now - m_dwBossCameraStart;
    const DWORD remaining = m_dwBossCameraEnd - now;
    float blend = 1.0f;
    if (m_dwBossCameraApproachMs > 0 && elapsed < m_dwBossCameraApproachMs)
    {
        blend = (float)elapsed / (float)m_dwBossCameraApproachMs;
        blend = blend * blend * (3.0f - 2.0f * blend);
        for (int axis = 0; axis < 3; ++axis)
            m_vCameraPosition[axis] = m_vBossCameraStartPosition[axis] + (targetPosition[axis] - m_vBossCameraStartPosition[axis]) * blend;
    }
    else if (m_dwBossCameraReturnMs > 0 && remaining < m_dwBossCameraReturnMs)
    {
        blend = (float)remaining / (float)m_dwBossCameraReturnMs;
        blend = blend * blend * (3.0f - 2.0f * blend);
        for (int axis = 0; axis < 3; ++axis)
            m_vCameraPosition[axis] = Hero->Object.Position[axis] + (targetPosition[axis] - Hero->Object.Position[axis]) * blend;
    }
    else
        VectorCopy(targetPosition, m_vCameraPosition);
}

void CDirection::SetCameraPosition()
{
	if (m_bStateCheck)
	{
		if (m_iTimeSchedule == 0)
		{
			VectorCopy(Hero->Object.Position, m_v1stPosition);
			m_iTimeSchedule++;
		}

		VectorSubtract(m_v2ndPosition, m_v1stPosition, m_vResult);
		m_fLength = VectorLength(m_vResult);
	}
}

int CDirection::GetCameraPosition(vec3_t GetPosition)
{
	VectorCopy(m_vCameraPosition, GetPosition);
	if (m_bBossCameraActive)
		return m_iBossCameraLevel;
	return m_CameraLevel;
}

bool CDirection::DirectionCameraMove()
{
	if (m_bCameraCheck)
	{
		SetCameraPosition();
		m_bStateCheck = false;

		VectorNormalize(m_vResult);
		vec3_t vTemp;

		VectorScale(m_vResult, m_fCount, vTemp);
		VectorAdd(m_v1stPosition, vTemp, m_vCameraPosition);
		m_fCount += m_fCameraSpeed;

		if (m_fLength <= VectorLength(vTemp))
		{
			VectorCopy(m_v2ndPosition, m_vCameraPosition);
			m_bAction = true;
			m_bCameraCheck = false;
			m_fCount = 0.0f;
			VectorCopy(m_v2ndPosition, m_v1stPosition);
		}
		return true;
	}

	return false;
}

void CDirection::DeleteMonster()
{
	int Count = stl_Monster.size();

	if (Count == 0)
		return;

	for (int i = 0; i < Count; i++)
	{
		DeleteCharacter(i + NUMOFMON);
		stl_Monster.erase(stl_Monster.begin());
	}
}

float CDirection::CalculateAngle(CHARACTER* c, int x, int y, float Angle)
{
	vec3_t vTemp, vTemp2, vResult;
	float fx = (float)(x * TERRAIN_SCALE) + 0.5f * TERRAIN_SCALE;
	float fy = (float)(y * TERRAIN_SCALE) + 0.5f * TERRAIN_SCALE;

	Vector(fx, fy, 0.0f, vTemp);
	Vector(c->Object.Position[0], c->Object.Position[1], 0.0f, vTemp2);

	VectorSubtract(vTemp2, vTemp, vResult);
	Vector(0.0f, 1.0f, 0.0f, vTemp2);

	VectorNormalize(vResult);

	float fAngle = acos(DotProduct(vResult, vTemp2)) / 3.14159 * 180;

	if (vResult[0] > 0) fAngle = 360 - fAngle;

	return fAngle;
}

void CDirection::SummonCreateMonster(int Type, int x, int y, float Angle, bool NextCheck, bool SummonAni, float AniSpeed)
{
	CHARACTER* c = NULL;
	DirectionMonster DMonster = { 0, };

	DMonster.m_Index = stl_Monster.size();
	DMonster.m_bAngleCheck = true;
	DMonster.m_iActionCheck = 0;

	stl_Monster.push_back(DMonster);

	c = CreateMonster(Type, x, y, DMonster.m_Index + NUMOFMON);

	c->Key = NUMOFMON + DMonster.m_Index++;
	c->Object.Angle[2] = Angle;
	c->Weapon[0].Type = -1;
	c->Weapon[1].Type = -1;
	c->Object.Alpha = 0.0f;

	int Index = 0;

	switch (Type)
	{
	case 344: Index = MODEL_MONSTER01 + 91;	break;
	case 341: Index = MODEL_MONSTER01 + 94;	break;
	case 440:
	case 340: Index = MODEL_MONSTER01 + 92;	break;
	case 345: Index = MODEL_MONSTER01 + 93;	break;
	case 348: Index = MODEL_MONSTER01 + 99;	break;
	case 349: Index = MODEL_MONSTER01 + 89;	break;
	case 361: Index = MODEL_MONSTER01 + 121;	break;
	}

	BMD* b = gmClientModels->GetModel(Index);

	if (AniSpeed >= 0.0f)
		b->Actions[MONSTER01_WALK].PlaySpeed = AniSpeed;

	if (SummonAni)
	{
		if (Type == 361)
		{
			vec3_t Light, Angle;

			Vector(0.5f, 0.8f, 1.0f, Light);
			Vector(0.0f, 0.0f, 0.0f, Angle);
			CreateEffect(MODEL_STORM2, c->Object.Position, Angle, Light, 1, NULL, -1, 0, 0, 0, 1.6f);
			CreateEffect(MODEL_STORM2, c->Object.Position, Angle, Light, 1, NULL, -1, 0, 0, 0, 1.3f);
			CreateEffect(MODEL_STORM2, c->Object.Position, Angle, Light, 1, NULL, -1, 0, 0, 0, 0.7f);

			PlayBuffer(SOUND_KANTURU_3RD_MAYA_END);
		}
		else
		{
			PlayBuffer(SOUND_CRY1ST_SUMMON);

			vec3_t vPos;
			Vector(c->Object.Position[0] + 20.0f, c->Object.Position[1] + 20.0f, c->Object.Position[2], vPos);

			CreateJoint(BITMAP_JOINT_THUNDER + 1, vPos, vPos, c->Object.Angle, 7, NULL, 60.f + rand() % 10);
			CreateJoint(BITMAP_JOINT_THUNDER + 1, vPos, vPos, c->Object.Angle, 7, NULL, 50.f + rand() % 10);
			CreateJoint(BITMAP_JOINT_THUNDER + 1, vPos, vPos, c->Object.Angle, 7, NULL, 50.f + rand() % 10);
			CreateJoint(BITMAP_JOINT_THUNDER + 1, vPos, vPos, c->Object.Angle, 7, NULL, 60.f + rand() % 10);

			CreateParticle(BITMAP_SMOKE + 4, c->Object.Position, c->Object.Angle, c->Object.Light, 1, 5.0f);
			CreateParticle(BITMAP_SMOKE + 4, c->Object.Position, c->Object.Angle, c->Object.Light, 1, 5.0f);
			CreateParticle(BITMAP_SMOKE + 4, c->Object.Position, c->Object.Angle, c->Object.Light, 1, 5.0f);

			Vector(c->Object.Position[0], c->Object.Position[1], c->Object.Position[2] + 120.0f, vPos);
			CreateJoint(BITMAP_JOINT_THUNDER, c->Object.Position, vPos, c->Object.Angle, 17);
			CreateJoint(BITMAP_JOINT_THUNDER, c->Object.Position, vPos, c->Object.Angle, 17);
			CreateJoint(BITMAP_JOINT_THUNDER, c->Object.Position, vPos, c->Object.Angle, 17);
			CreateJoint(BITMAP_JOINT_THUNDER, c->Object.Position, vPos, c->Object.Angle, 17);
		}

	}

	if (NextCheck) m_iCheckTime++;
}

bool CDirection::MoveCreatedMonster(int Index, int x, int y, float Angle, int Speed)
{
	CHARACTER* c = NULL;
	bool bNext = false;

	for (int i = 0; i < MAX_CHARACTERS_CLIENT; i++)
	{
		c = gmCharacters->GetCharacter(i);

		if (c && c->Object.Live && c->Key == Index + NUMOFMON)
			break;
	}

	int PresentX = (int)(c->Object.Position[0]) / TERRAIN_SCALE;
	int PresentY = (int)(c->Object.Position[1]) / TERRAIN_SCALE;

	if (PresentX == x && PresentY == y) bNext = true;

	if (!bNext)
	{
		int iResult = 0;

		if (stl_Monster[Index].m_bAngleCheck)
		{
			int	iAngle1 = (int)CalculateAngle(c, x, y, Angle);
			int iAngle2 = (int)c->Object.Angle[2];

			if ((iAngle1 - Angle) > 180)
				iAngle1 = iAngle1 - 360;

			iResult = iAngle1 - iAngle2;
			c->Blood = false;
		}

		if (iResult <= 3 && iResult >= -3)
		{
			c->Blood = true;
			stl_Monster[Index].m_bAngleCheck = false;
		}

		if (c->Blood)
		{
			c->MoveSpeed = Speed;
			SetAction(&c->Object, MONSTER01_WALK);
			MoveCharacterPosition(c);
		}
		else
		{
			if (iResult > 3 && iResult <= 180)
				c->Object.Angle[2] += 3.0f;
			else
				c->Object.Angle[2] -= 3.0f;

			SetAction(&c->Object, MONSTER01_STOP1);
		}
	}
	else
	{
		c->Object.Position[0] = (float)(x * TERRAIN_SCALE) + 0.5f * TERRAIN_SCALE;
		c->Object.Position[1] = (float)(y * TERRAIN_SCALE) + 0.5f * TERRAIN_SCALE;

		stl_Monster[Index].m_bAngleCheck = true;
		SetAction(&c->Object, MONSTER01_STOP1);
		return true;
	}

	return false;
}

bool CDirection::ActionCreatedMonster(int Index, int Action, int Count, bool TankerAttack, bool NextCheck)
{
	CHARACTER* c = NULL;
	bool bNext = false;

	for (int i = 0; i < MAX_CHARACTERS_CLIENT; i++)
	{
		c = gmCharacters->GetCharacter(i);

		if (c && c->Object.Live && c->Key == Index + NUMOFMON) break;
	}

	if (stl_Monster[Index].m_iActionCheck == Count) bNext = true;

	if (!bNext)
	{
		if (c->Object.CurrentAction != Action)
		{
			c->Object.CurrentAction = Action;
			c->Object.AnimationFrame = 0.0f;
			stl_Monster[Index].m_iActionCheck++;

			if (TankerAttack)
				CreateEffect(MODEL_ARROW_TANKER, c->Object.Position, c->Object.Angle, c->Object.Light, 1, &c->Object, c->Object.PKKey);
		}
	}
	else
	{
		if (NextCheck)
		{
			stl_Monster[Index].m_iActionCheck = 0;
			m_iCheckTime++;
		}

		if (c->Object.AnimationFrame >= 8.0f) return true;
	}

	return false;
}

void CDirection::HeroFallingDownDirection()
{
	if (!m_bDownHero)
		return;

	Hero->Object.Gravity += 1.5f;
	Hero->Object.Angle[0] -= 2.f;
	Hero->Object.m_bActionStart = true;
	Hero->Object.Direction[1] += Hero->Object.Direction[0];

	if (Hero->Object.Gravity <= 2.f)
		m_AngleY = Hero->Object.Angle[2];

	if (Hero->Object.Angle[0] <= -90.0f)
		Hero->Object.Angle[0] = -90.0f;

	Hero->Object.Angle[2] = m_AngleY;

	if (Hero->Object.Gravity >= 150.0f)
		Hero->Object.Gravity = 150.0f;

	Hero->Object.Position[2] -= Hero->Object.Gravity;

	FaillingEffect();
}

void CDirection::FaillingEffect()
{
	vec3_t Pos, Light;
	float Scale = 1.3f + rand() % 10 / 30.0f;
	Vector(0.05f, 0.05f, 0.08f, Light);

	Pos[0] = Hero->Object.Position[0] + (float)(rand() % 20 - 10) * 70.0f;
	Pos[1] = Hero->Object.Position[1] + (float)(rand() % 20 - 10) * 70.0f;
	Pos[2] = Hero->Object.Position[2] - rand() % 200 - 500.0f;

	CreateParticle(BITMAP_CLOUD, Pos, Hero->Object.Angle, Light, 13, Scale);

	Pos[0] = Hero->Object.Position[0] + (float)(rand() % 20 - 10) * 70.0f;
	Pos[1] = Hero->Object.Position[1] + (float)(rand() % 20 - 10) * 70.0f;
	Pos[2] = Hero->Object.Position[2] - rand() % 200 - 500.0f;

	Vector(0.05f, 0.05f, 0.05f, Light);
	CreateParticle(BITMAP_CLOUD, Pos, Hero->Object.Angle, Light, 13, Scale);
}

void CDirection::HeroFallingDownInit()
{
	if (!m_bDownHero)
	{
		Hero->Object.m_bActionStart = false;
		Hero->Object.Gravity = 0.0f;
		Hero->Object.Angle[0] = 0.0f;
	}
}

void CDirection::CameraLevelUp()
{
	if (m_CameraLevel < 4) m_CameraLevel++;
}

void CDirection::SetNextDirectionPosition(int x, int y, int z, float Speed)
{
	m_iCheckTime = 0;
	m_bCameraCheck = true;
	m_bStateCheck = true;
	m_fCameraSpeed = Speed;

	float fx = (float)(x * TERRAIN_SCALE) + 0.5f * TERRAIN_SCALE;
	float fy = (float)(y * TERRAIN_SCALE) + 0.5f * TERRAIN_SCALE;
	float fz = (float)(z * TERRAIN_SCALE) + 0.5f * TERRAIN_SCALE;

	Vector(fx, fy, fz, m_v2ndPosition);
	m_iTimeSchedule++;
}

bool CDirection::GetTimeCheck(int DelayTime)
{
	int PresentTime = timeGetTime();

	if (m_bTimeCheck)
	{
		m_iBackupTime = PresentTime;
		m_bTimeCheck = false;
	}

	if (m_iBackupTime + DelayTime <= PresentTime)
	{
		m_bTimeCheck = true;
		m_iCheckTime++;
		return true;
	}
	return false;
}
