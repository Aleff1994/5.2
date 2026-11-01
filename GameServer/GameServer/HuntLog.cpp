#include "stdafx.h"
#include "HuntLog.h"
#include "ItemBagManager.h"
#include "ItemDrop.h"
#include "ItemManager.h"
#include "MonsterManager.h"
#include "ServerInfo.h"
#include "Util.h"
#include "BonusManager.h"
#include "CGMScriptLua.h"
#include "CustomMonster.h"
#include "ExperienceTable.h"
#include "GameMain.h"
#include "MapManager.h"
#include "Map.h"
#include "MasterSkillTree.h"
#include "CrywolfSync.h"
#include "MemScript.h"
#include "Log.h"

CHuntLog gHuntLog;

void CHuntLog::Load(char* path)
{
	CMemScript* script = new CMemScript;
	if (script == 0) { ErrorMessageBox(MEM_SCRIPT_ALLOC_ERROR,path); return; }
	if (script->SetBuffer(path) == 0) { ErrorMessageBox(script->GetLastError()); delete script; return; }
	m_MonsterClasses.clear();
	try
	{
		while (true)
		{
			if (script->GetToken() == TOKEN_END || strcmp("end",script->GetString()) == 0) break;
			int enabled=script->GetNumber();
			int monsterClass=script->GetAsNumber();
			if (enabled != 0 && monsterClass >= 0 && !IsEnabledMonster(monsterClass)) m_MonsterClasses.push_back(monsterClass);
		}
	}
	catch (...) { ErrorMessageBox(script->GetLastError()); }
	delete script;
	LogAdd(LOG_BLUE,"[HuntLog] Monstros habilitados: %d",(int)m_MonsterClasses.size());
}

bool CHuntLog::IsEnabledMonster(int monsterClass) const
{
	for (std::vector<int>::const_iterator it=m_MonsterClasses.begin();it!=m_MonsterClasses.end();++it) if ((*it)==monsterClass) return true;
	return false;
}

static QWORD HuntLogExperience(LPOBJ player,LPOBJ monster,int damage)
{
	int level=((monster->Level+25)*monster->Level)/3;
	int playerLevel=player->Level+((gMasterSkillTree.CheckMasterLevel(player)!=0)?player->MasterLevel:0);
	if ((monster->Level+10)<playerLevel && playerLevel>0) level=(level*(monster->Level+10))/playerLevel;
	if (monster->Level>=65) level+=(monster->Level-64)*(monster->Level/4);
	QWORD exp=max(level,0); exp+=exp/4;
	bool master=(gMasterSkillTree.CheckMasterLevel(player)!=0);
	int serverRate=master?gServerInfo.m_AddMasterExperienceRate[player->AccountLevel]:gServerInfo.m_AddExperienceRate[player->AccountLevel];
	if (player->AttackCustomOffline==1) serverRate=gServerInfo.m_OffAttackExperienceRate[player->AccountLevel];
	else if (player->AutoResetEnable==1) serverRate=gServerInfo.m_AutoResetExperienceRate[player->AccountLevel];
	if (g_LuaExpEventRate!=100) serverRate=g_LuaExpEventRate;
	damage=min(damage,(int)monster->MaxLife);
	exp=((QWORD)damage*exp/(QWORD)max((int)monster->MaxLife,1))*serverRate;
	exp=(exp*(master?(player->MasterExperienceRate+player->EffectOption.AddMasterExperienceRate):(player->ExperienceRate+player->EffectOption.AddExperienceRate)))/100;
	int mapRate=master?gMapManager.GetMapMasterExperienceRate(player->Map):gMapManager.GetMapExperienceRate(player->Map);
	if (player->Map==MAP_VULCANUS) mapRate+=player->EffectOption.AddVolcanoExperienceRate;
	exp=(exp*mapRate)/100;
	exp=(exp*gBonusManager.GetBonusValue(player,master?BONUS_INDEX_MASTER_EXPERIENCE_RATE:BONUS_INDEX_EXPERIENCE_RATE,100,-1,-1,monster->Class,monster->Level))/100;
	exp=(exp*(master?gCustomMonster.GetCustomMonsterMasterExperienceRate(monster->Class,monster->Map):gCustomMonster.GetCustomMonsterExperienceRate(monster->Class,monster->Map)))/100;
	exp=(exp*(master?gExperienceTable.GetMasterExperienceRate(player):gExperienceTable.GetExperienceRate(player)))/100;
	if (gCrywolfSync.CheckApplyPenalty()!=0 && gCrywolfSync.GetOccupationState()==1) exp=(exp*gCrywolfSync.GetGettingExpPenaltyRate())/100;
	return exp;
}

static void HuntLogAddDrop(PMSG_HUNT_LOG_INFO_SEND& msg,int itemIndex,DWORD rate)
{
	if (itemIndex < 0 || msg.DropCount >= HUNT_LOG_MAX_DROPS) return;
	for (int n=0;n<msg.DropCount;n++) if (msg.Drops[n].ItemIndex == itemIndex) return;
	PMSG_HUNT_LOG_DROP_SEND& drop = msg.Drops[msg.DropCount++];
	drop.ItemIndex = (WORD)itemIndex;
	drop.Rate = rate;
}

void CHuntLog::CGRequestRecv(PMSG_HUNT_LOG_REQUEST_RECV* lpMsg,int aIndex)
{
	if (gObjIsConnectedGP(aIndex) == 0 || OBJECT_RANGE(lpMsg->TargetIndex) == 0) return;
	LPOBJ player=&gObj[aIndex]; LPOBJ monster=&gObj[lpMsg->TargetIndex];
	if (monster->Type != OBJECT_MONSTER || monster->Live == 0 || monster->Map != player->Map) return;

	PMSG_HUNT_LOG_INFO_SEND msg; ZeroMemory(&msg,sizeof(msg));
	msg.header.set(0xEC,HUNT_LOG_RESPONSE_SUB,sizeof(msg));
	msg.TargetIndex=(WORD)monster->Index;
	msg.IsBoss=(IsEnabledMonster(monster->Class)?1:0);
	if (msg.IsBoss == 0) { DataSend(aIndex,(BYTE*)&msg,sizeof(msg)); return; }
	msg.MonsterClass=(WORD)monster->Class; msg.Level=(WORD)monster->Level;
	msg.Life=(DWORD)max(monster->Life,0.0f); msg.MaxLife=(DWORD)max(monster->MaxLife,0.0f);
	__int64 zen=((__int64)monster->Money*gServerInfo.m_MoneyAmountDropRate[player->AccountLevel])/100;
	if (gServerInfo.m_MoneyValue[player->AccountLevel]>0) zen=(__int64)monster->Level*gServerInfo.m_MoneyValue[player->AccountLevel];
	zen=(zen*player->MoneyAmountDropRate)/100; msg.Zen=(DWORD)max((__int64)1,zen);
	int contribution=0;
	for (int n=0;n<MAX_HIT_DAMAGE;n++) if (monster->HitDamage[n].index==aIndex) { contribution=monster->HitDamage[n].damage; break; }
	msg.DamageContribution=(DWORD)max(contribution,0);
	msg.Experience=HuntLogExperience(player,monster,contribution);

	std::vector<int> items; gItemBagManager.CollectMonsterDropItems(monster->Class,items);
	const std::vector<ITEM_DROP_INFO>& configured=gItemDrop.GetInfoList();
	int dropAccountLevel=(player->AccountLevel>=0 && player->AccountLevel<4)?player->AccountLevel:0;
	for (std::vector<ITEM_DROP_INFO>::const_iterator it=configured.begin();it!=configured.end();++it)
	{
		if (it->DropRateByAccount[dropAccountLevel]>0 && (it->MapNumber==-1 || it->MapNumber==monster->Map) && (it->MonsterClass==-1 || it->MonsterClass==monster->Class) && monster->Level>=it->MonsterLevelMin && monster->Level<=it->MonsterLevelMax) items.push_back(it->Index);
	}
	for (size_t n=0;n<items.size();++n) HuntLogAddDrop(msg,items[n],0);
	for (std::vector<ITEM_DROP_INFO>::const_iterator it=configured.begin();it!=configured.end();++it)
	{
		if (it->DropRateByAccount[dropAccountLevel]>0 && (it->MapNumber==-1 || it->MapNumber==monster->Map) && (it->MonsterClass==-1 || it->MonsterClass==monster->Class) && monster->Level>=it->MonsterLevelMin && monster->Level<=it->MonsterLevelMax)
		{
			for (int n=0;n<msg.DropCount;n++) if (msg.Drops[n].ItemIndex==it->Index) msg.Drops[n].Rate=it->DropRateByAccount[dropAccountLevel];
		}
	}
	const MONSTER_ITEM_INFO* common=gMonsterManager.GetMonsterItemInfo(min(monster->Level,MAX_MONSTER_LEVEL-1));
	msg.CommonPoolCount=(BYTE)(common?min(common->IndexCount,255):0);
	DataSend(aIndex,(BYTE*)&msg,sizeof(msg));
}
