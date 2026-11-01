#ifndef _DIRECTION_H_
#define _DIRECTION_H_

#include <vector>

#include "Singleton.h"
#include "CMVP1stDirection.h"
#include "CKANTURUDirection.h"

struct BOSS_SPAWN_CAMERA_INFO
{
	int Enabled;
	int MonsterIndex;
	int MapNumber;
	int DurationMs;
	int CameraLevel;
};

class CDirection : public Singleton <CDirection>
{
private:
	vec3_t	m_vCameraPosition;
	vec3_t	m_v1stPosition;
	vec3_t	m_v2ndPosition;
	vec3_t	m_vResult;

	bool	m_bCameraCheck;
	float	m_fCount;
	float	m_fLength;
	float	m_fCameraSpeed;

	bool	m_bTimeCheck;
	int		m_iBackupTime;
	bool	m_bStateCheck;

	bool m_bBossCameraLoaded;
	bool m_bBossCameraSystemEnabled;
	bool m_bBossCameraActive;
	bool m_bBossCameraPending;
	int m_iBossCameraTargetKey;
	int m_iBossCameraPendingMonsterIndex;
	int m_iBossCameraPendingObjectIndex;
	int m_iBossCameraPendingMapNumber;
	int m_iBossCameraPendingX;
	int m_iBossCameraPendingY;
	int m_iBossCameraPendingDurationSeconds;
	int m_iBossCameraPendingLevel;
	CHARACTER* m_pBossCameraTarget;
	bool m_bBossCameraLocalFocus;
	vec3_t m_vBossCameraLocalPosition;
	int m_iBossCameraLevel;
	DWORD m_dwBossCameraApproachMs;
	DWORD m_dwBossCameraReturnMs;
	DWORD m_dwBossCameraStart;
	DWORD m_dwBossCameraEnd;
	DWORD m_dwBossCameraPendingExpire;
	vec3_t m_vBossCameraStartPosition;
	std::vector<BOSS_SPAWN_CAMERA_INFO> m_BossCameraInfo;

	void UpdateBossCamera();
	bool StartPendingBossCamera();
	void StartBossCamera(CHARACTER* Target, int DurationSeconds, int CameraLevel);
public:
	std::vector<DirectionMonster> stl_Monster;

	bool	m_bOrderExit;
	int		m_iTimeSchedule;
	int		m_CameraLevel;
	float	m_fCameraViewFar;
	int		m_iCheckTime;
	bool	m_bAction;
	bool	m_bDownHero;
	float	m_AngleY;

	CKanturuDirection m_CKanturu;
	CMVP1STDirection  m_CMVP;


	CDirection();
	virtual ~CDirection(); 

	void Init();
	void CloseAllWindows();
	bool IsDirection();
	bool IsBossCameraActive() const;
	void BeginBossCameraConfig(bool Enabled, DWORD ApproachMs, DWORD ReturnMs);
	void AddBossCameraConfig(int Enabled, int MonsterIndex, int MapNumber,
		int DurationSeconds, int CameraLevel);
	void StartBossCameraFromServer(int MonsterIndex, int TargetKey, int MapNumber,
		int X, int Y, int DurationSeconds, int CameraLevel);
	void CheckDirection();
	void SetCameraPosition();
	int GetCameraPosition(vec3_t GetPosition);
	bool DirectionCameraMove();
	void DeleteMonster();


	float CalculateAngle(CHARACTER* c, int x, int y, float Angle);
	void CameraLevelUp();
	void SetNextDirectionPosition(int x, int y, int z, float Speed);
	bool GetTimeCheck(int DelayTime);
	void HeroFallingDownDirection();
	void HeroFallingDownInit();
	void FaillingEffect();

	void SummonCreateMonster(int Type, int x, int y, float Angle, bool NextCheck = true, bool SummonAni = true, float AniSpeed = -1.0f);
	bool MoveCreatedMonster(int Type, int x, int y, float Angle, int Speed);
	bool ActionCreatedMonster(int Type, int Action, int Count, bool TankerAttack = false, bool NextCheck = false);
};

#define g_Direction CDirection::GetSingleton()

#endif _DIRECTION_H_
