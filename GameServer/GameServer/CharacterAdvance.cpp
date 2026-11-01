#include "StdAfx.h"
#include "../../Utilities/ExcRank/ExcRankCore.h"
#include "Util.h"
#include "User.h"
#include "CharacterAdvance.h"
#include "ExcRankRuntime.h"

CCharacterAdvance::CCharacterAdvance()
{
}

CCharacterAdvance::~CCharacterAdvance()
{
}

CCharacterAdvance* CCharacterAdvance::Instance()
{
	static CCharacterAdvance sInstance;
	return &sInstance;
}

void CCharacterAdvance::UpdateInformation(LPOBJ lpObj)
{
	PMSG_STATS_ADVANCE pMsg;

	pMsg.Header.set(0xF3, 0xF3, sizeof(pMsg));

	pMsg.m_PowerATK = lpObj->PowerATTK;
	pMsg.m_KillCount = lpObj->Kills;
	pMsg.m_DeadCount = lpObj->Deads;
	pMsg.AttackSuccessRate  = lpObj->AttackSuccessRate;
	pMsg.DefenseSuccessRate = lpObj->DefenseSuccessRate;

	pMsg.m_DoubleDmgRate      = lpObj->DoubleDamageRate;
	pMsg.m_TripleDmgRate      = lpObj->TripleDamageRate;
	pMsg.m_CriticalDmgRate    = ExcRank::CritChance(lpObj->CriticalDamageRate,lpObj->EffectOption.AddCriticalDamageRate,0);
	pMsg.m_ExcellentDmgRate   = ExcRank::ExcellentChance(lpObj->ExcellentDamageRate,lpObj->EffectOption.AddExcellentDamageRate,0);
	pMsg.m_IncIgnoreShield    = lpObj->IgnoreShieldGaugeRate + lpObj->DecreaseShieldGaugeRate;
	pMsg.m_IncIgnoreDefense   = lpObj->IgnoreDefenseRate + lpObj->EffectOption.AddIgnoreDefenseRate;
	pMsg.OffFullHPRestoreRate = lpObj->OffensiveFullHPRestoreRate;
	pMsg.OffFullMPRestoreRate = lpObj->OffensiveFullMPRestoreRate;
	pMsg.OffFullSDRestoreRate = lpObj->OffensiveFullSDRestoreRate;
	pMsg.OffFullBPRestoreRate = lpObj->OffensiveFullBPRestoreRate;
	//--
	pMsg.m_ReflectDmg          = ExcRankRuntime::ReflectPercent(lpObj,lpObj->DamageReflect + lpObj->EffectOption.AddDamageReflect);
	pMsg.m_ReflectDmgRate      = lpObj->FullDamageReflectRate;
	pMsg.m_ResIgnoreShield     = lpObj->ResistIgnoreShieldGaugeRate;
	pMsg.m_ResIgnoreDefense    = lpObj->ResistIgnoreDefenseRate;
	pMsg.m_ResDoubleDmgRate    = lpObj->ResistDoubleDamageRate;
	pMsg.m_ResAttackStunRate   = lpObj->ResistStunRate;
	pMsg.m_ResCriticalDmgRate  = lpObj->ResistCriticalDamageRate;
	pMsg.m_ResExcellentDmgRate = lpObj->ResistExcellentDamageRate;
	pMsg.DefFullHPRestoreRate = lpObj->DefensiveFullHPRestoreRate;
	pMsg.DefFullMPRestoreRate = lpObj->DefensiveFullMPRestoreRate;
	pMsg.DefFullSDRestoreRate = lpObj->DefensiveFullSDRestoreRate;
	pMsg.DefFullBPRestoreRate = lpObj->DefensiveFullBPRestoreRate;

    // Match the actual multiplicative reduction stages, rather than adding percentages.
    double remaining=100.0;
    for(int n=0;n<MAX_DAMAGE_REDUCTION;++n)remaining*=1.0-(std::max)(0,(std::min)(100,lpObj->DamageReduction[n]))/100.0;
    if(lpObj->EffectOption.AddDamageReduction>0)remaining*=1.0-(std::min)(100,lpObj->EffectOption.AddDamageReduction)/100.0;
    pMsg.m_DmgReductionRate=static_cast<DWORD>(100.0-remaining+0.5);

	DataSend(lpObj->Index, (BYTE*)&pMsg, pMsg.Header.size);
}
