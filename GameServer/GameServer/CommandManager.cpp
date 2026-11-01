// CommandManager.cpp: implementation of the CCommandManager class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "CommandManager.h"
#include "BloodCastle.h"
#include "BotPlayer.h"
#include "BuildProfile.h"
#include "BonusManager.h"
#include "CashShop.h"
#include "ChaosCastle.h"
#include "Command.h"
#include "CustomArena.h"
#include "CustomAttack.h"
#include "CustomEventDrop.h"
#include "CustomPick.h"
#include "CustomQuest.h"
#include "CustomQuiz.h"
#include "CustomRankUser.h"
#include "CustomTop.h"
#include "CustomStore.h"
#include "DevilSquare.h"
#include "DSProtocol.h"
#include "EventGvG.h"
#include "EventHideAndSeek.h"
#include "EventKillAll.h"
#include "EventPvP.h"
#include "EventRunAndCatch.h"
#include "EventRussianRoulette.h"
#include "EventQuickly.h"
#include "EventStart.h"
#include "EventTvT.h"
#include "FakeBot.h"
#include "Filter.h"
#include "FilterRename.h"
#include "GameMain.h"
#include "GameMaster.h"
#include "Guild.h"
#include "GuildClass.h"
#include "InvasionManager.h"
#include "IllusionTemple.h"
#include "ItemManager.h"
#include "ItemBagManager.h"
#include "JewelMix.h"
#include "JSProtocol.h"
#include "Log.h"
#include "MapServerManager.h"
#include "MasterSkillTree.h"
#include "Message.h"
#include "Monster.h"
#include "Move.h"
#include "Notice.h"
#include "ObjectManager.h"
#include "PcPoint.h"
#include "Protocol.h"
#include "Quest.h"
#include "QuestReward.h"
#include "ResetTable.h"
#include "ServerInfo.h"
#include "Util.h"
#include "ReiDoMU.h"
#include "Warehouse.h"
#include "ChatManager.h"
#include "CGMScriptLua.h"
#include "EventFindPath.h"
#include "MasterResetTable.h"
#include "GearScoreRuntime.h"
#include "ExcRankAdmin.h"
#include <bcrypt.h>
#pragma comment(lib,"bcrypt.lib")
#include "../../Utilities/IndependentTierControl.h"

#include <map>
#include <string>

CCommandManager gCommandManager;

namespace
{
	#define LUCKY_WHEEL_MAX_REWARDS 16
	#pragma pack(push, 1)
	struct PMSG_LUCKY_WHEEL_REWARD
	{
		BYTE Kind; WORD ItemType; BYTE Level; DWORD Amount; BYTE Skill; BYTE Luck; BYTE Option;
		BYTE Excellent; BYTE Ancient; BYTE Harmony; BYTE ItemOptionEx; BYTE SocketBonus; BYTE Socket[5]; char Name[32];
	};
	struct PMSG_LUCKY_WHEEL_INFO_SEND
	{
		PWMSG_HEAD header; BYTE SubCode; BYTE Enabled; BYTE Pool; BYTE TabEnabled[4]; BYTE RewardCount; BYTE FreeRemaining; BYTE DailyRemaining;
		DWORD WCoinCost; DWORD WCoinPCost; DWORD GoblinPointCost; DWORD ZenCost; WORD TicketType; BYTE TicketCount;
		PMSG_LUCKY_WHEEL_REWARD Reward[LUCKY_WHEEL_MAX_REWARDS];
	};
	struct PMSG_LUCKY_WHEEL_RESULT_SEND { PSBMSG_HEAD header; BYTE Success; BYTE RewardIndex; };
	struct PMSG_LUCKY_WHEEL_CLAIM_SEND { PSBMSG_HEAD header; BYTE Success; };
	#pragma pack(pop)

	void LuckyWheelSendResult(LPOBJ lpObj, BYTE success, BYTE rewardIndex)
	{
		PMSG_LUCKY_WHEEL_RESULT_SEND pMsg;
		pMsg.header.set(0xEC, 0x63, sizeof(pMsg)); pMsg.Success=success; pMsg.RewardIndex=rewardIndex;
		DataSend(lpObj->Index,(BYTE*)&pMsg,sizeof(pMsg));
	}

	struct LUCKY_WHEEL_DAILY_STATE { int DateKey; int FreeSpins; int TotalSpins; };
	std::map<std::string, LUCKY_WHEEL_DAILY_STATE> gLuckyWheelDailyState;
	struct LUCKY_WHEEL_PENDING_REWARD { LUA_LUCKY_WHEEL_REWARD Reward; BYTE RewardIndex; };
	std::map<std::string, LUCKY_WHEEL_PENDING_REWARD> gLuckyWheelPendingReward;
	bool gLuckyWheelDailyLoaded=false;
	const char* gLuckyWheelDailyFile="C:\\MuServer1\\Data\\LuckyWheelDaily.txt";

	void LoadLuckyWheelDailyState()
	{
		if(gLuckyWheelDailyLoaded) return;
		gLuckyWheelDailyLoaded=true;
		FILE* file=0; if(fopen_s(&file,gLuckyWheelDailyFile,"rt")!=0 || !file) return;
		char account[11]; LUCKY_WHEEL_DAILY_STATE state;
		while(fscanf_s(file,"%10s %d %d %d",account,(unsigned)_countof(account),&state.DateKey,&state.FreeSpins,&state.TotalSpins)==4)
			gLuckyWheelDailyState[account]=state;
		fclose(file);
	}

	void SaveLuckyWheelDailyState()
	{
		FILE* file=0; if(fopen_s(&file,gLuckyWheelDailyFile,"wt")!=0 || !file) return;
		for(std::map<std::string,LUCKY_WHEEL_DAILY_STATE>::const_iterator it=gLuckyWheelDailyState.begin();it!=gLuckyWheelDailyState.end();++it)
			fprintf(file,"%s %d %d %d\n",it->first.c_str(),it->second.DateKey,it->second.FreeSpins,it->second.TotalSpins);
		fclose(file);
	}

	int GetLuckyWheelDateKey()
	{
		time_t now = time(0); tm localTime; localtime_s(&localTime, &now);
		return ((localTime.tm_year + 1900) * 10000) + ((localTime.tm_mon + 1) * 100) + localTime.tm_mday;
	}

	LUCKY_WHEEL_DAILY_STATE& GetLuckyWheelDailyState(LPOBJ lpObj)
	{
		LoadLuckyWheelDailyState();
		std::string key(lpObj->Account, strnlen(lpObj->Account, sizeof(lpObj->Account)));
		LUCKY_WHEEL_DAILY_STATE& state = gLuckyWheelDailyState[key];
		int dateKey = GetLuckyWheelDateKey();
		if (state.DateKey != dateKey) { state.DateKey = dateKey; state.FreeSpins = 0; state.TotalSpins = 0; }
		return state;
	}

	std::string GetLuckyWheelAccountKey(LPOBJ lpObj)
	{
		return std::string(lpObj->Account, strnlen(lpObj->Account, sizeof(lpObj->Account)));
	}

	bool LuckyWheelGiveReward(LPOBJ lpObj, const LUA_LUCKY_WHEEL_REWARD& reward)
	{
		if (reward.Kind == 0)
		{
			int itemIndex = GET_ITEM(reward.Section, reward.Index);
			if (!gItemManager.CheckItemInventorySpace(lpObj, itemIndex, reward.Amount, false, false))
			{
				gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "Lucky Wheel: libere espaco no inventario.");
				return false;
			}
			for (int n = 0; n < reward.Amount; n++)
			{
				BYTE sockets[MAX_SOCKET_OPTION];
				for (int i = 0; i < MAX_SOCKET_OPTION; i++) sockets[i] = (BYTE)reward.Socket[i];
				DWORD duration = (reward.Duration > 0) ? ((DWORD)time(0) + reward.Duration) : 0;
				GDCreateItemSend(lpObj->Index, 0xEB, 0, 0, itemIndex, (BYTE)reward.Level, (BYTE)reward.Durability,
					(BYTE)reward.Skill, (BYTE)reward.Luck, (BYTE)reward.Option, -1, (BYTE)reward.Excellent,
					(BYTE)reward.Ancient, (BYTE)reward.Harmony, (BYTE)reward.ItemOptionEx, sockets,
					(BYTE)reward.SocketBonus, duration, static_cast<BYTE>(ExcRank::Origin::Reward));
			}
			return true;
		}
		switch (reward.Kind)
		{
		case 1: GDSetCoinSend(lpObj->Index, reward.Amount, 0, 0, "LuckyWheel"); break;
		case 2: GDSetCoinSend(lpObj->Index, 0, reward.Amount, 0, "LuckyWheel"); break;
		case 3: GDSetCoinSend(lpObj->Index, 0, 0, reward.Amount, "LuckyWheel"); break;
		case 4:
			lpObj->Money = ((MAX_MONEY - lpObj->Money) < (DWORD)reward.Amount) ? MAX_MONEY : (lpObj->Money + reward.Amount);
			GCMoneySend(lpObj->Index, lpObj->Money); break;
		default: return false;
		}
		return true;
	}

	bool LuckyWheelCommand(LPOBJ lpObj, const char* payment)
	{
		auto Reject=[lpObj](){ LuckyWheelSendResult(lpObj,0,0); return true; };
		if (!g_pScriptLua) { gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,"Lucky Wheel: Lua nao iniciado."); return Reject(); }
		if(gLuckyWheelPendingReward.find(GetLuckyWheelAccountKey(lpObj))!=gLuckyWheelPendingReward.end())
		{ gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,"Lucky Wheel: resgate o premio pendente antes de girar novamente."); return Reject(); }
		LUA_LUCKY_WHEEL_CONFIG config;
		if (!g_pScriptLua->GetLuckyWheelConfig(config) || !config.Enable)
		{ gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,"Lucky Wheel: sistema desativado."); return Reject(); }
		int requestedPool=(!_stricmp(payment,"wcoin"))?0:((!_stricmp(payment,"wcoinp"))?1:((!_stricmp(payment,"free"))?2:3));
		bool poolEnabled=(requestedPool==0)?config.CashEnable:((requestedPool==1)?config.GoldEnable:((requestedPool==2)?config.FreeEnable:config.GoblinEnable));
		if(!poolEnabled) { gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,"Lucky Wheel: esta roleta esta desativada."); return Reject(); }

		LUCKY_WHEEL_DAILY_STATE& state = GetLuckyWheelDailyState(lpObj);
		if (state.TotalSpins >= config.DailyLimit)
		{ gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,"Lucky Wheel: limite diario atingido (%d).",config.DailyLimit); return Reject(); }

		int type = -1;
		if (!payment[0] || !_stricmp(payment,"free")) type=0;
		else if (!_stricmp(payment,"ticket")) type=1;
		else if (!_stricmp(payment,"wcoin")) type=2;
		else if (!_stricmp(payment,"gp")) type=3;
		else if (!_stricmp(payment,"zen")) type=4;
		else if (!_stricmp(payment,"wcoinp")) type=5;
		if (type < 0) { gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,"Uso: /lucky free | ticket | wcoin | gp | zen"); return Reject(); }

		int ticketIndex=GET_ITEM(config.TicketSection,config.TicketIndex);
		if (type==0 && state.FreeSpins>=config.FreeSpinsDaily) { gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,"Lucky Wheel: giro gratis de hoje ja utilizado."); return Reject(); }
		if (type==1 && gItemManager.GetInventoryItemCount(lpObj,ticketIndex,-1)<config.TicketCount) { gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,"Lucky Wheel: precisa de %d ticket(s).",config.TicketCount); return Reject(); }
		if (type==2 && lpObj->Coin1<config.WCoinCost) { gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,"Lucky Wheel: precisa de %d WCoinC.",config.WCoinCost); return Reject(); }
		if (type==5 && lpObj->Coin2<config.WCoinPCost) { gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,"Lucky Wheel: precisa de %d WCoinP.",config.WCoinPCost); return Reject(); }
		if (type==3 && lpObj->Coin3<config.GoblinPointCost) { gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,"Lucky Wheel: precisa de %d Goblin Point.",config.GoblinPointCost); return Reject(); }
		if (type==4 && lpObj->Money<(DWORD)config.ZenCost) { gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,"Lucky Wheel: precisa de %d Zen.",config.ZenCost); return Reject(); }

		// O giro gratis usa a mesma lista visual da aba CASH; nao cria uma terceira aba.
		int pool=(type==5)?1:((type==3)?3:0);
		std::vector<LUA_LUCKY_WHEEL_REWARD> rewards; int totalWeight=0;
		for (int n=1;n<=g_pScriptLua->GetLuckyWheelRewardCount();n++)
		{
			LUA_LUCKY_WHEEL_REWARD reward;
			if (g_pScriptLua->GetLuckyWheelReward(n,reward) && reward.Pool==pool) { rewards.push_back(reward); totalWeight+=reward.Weight; }
		}
		if (rewards.empty() || totalWeight<=0) { gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,"Lucky Wheel: nenhum premio valido no Lua."); return Reject(); }

		int roll=GetLargeRand()%totalWeight; LUA_LUCKY_WHEEL_REWARD* selected=0;
		for (size_t n=0;n<rewards.size();n++) { if (roll<rewards[n].Weight) { selected=&rewards[n]; break; } roll-=rewards[n].Weight; }
		if (!selected) { LuckyWheelSendResult(lpObj,0,0); return true; }
		if(selected->Kind==0 && !gItemManager.CheckItemInventorySpace(lpObj,GET_ITEM(selected->Section,selected->Index),selected->Amount,false,false))
		{ gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,"Lucky Wheel: libere espaco no inventario antes de girar."); return Reject(); }
		BYTE selectedIndex=(BYTE)(selected-&rewards[0]);

		if (type==0) state.FreeSpins++;
		else if (type==1) gItemManager.DeleteInventoryItemCount(lpObj,ticketIndex,-1,config.TicketCount);
		else if (type==2) GDSetCoinSend(lpObj->Index,-config.WCoinCost,0,0,"LuckyWheel");
		else if (type==5) GDSetCoinSend(lpObj->Index,0,-config.WCoinPCost,0,"LuckyWheel");
		else if (type==3) GDSetCoinSend(lpObj->Index,0,0,-config.GoblinPointCost,"LuckyWheel");
		else { lpObj->Money-=config.ZenCost; GCMoneySend(lpObj->Index,lpObj->Money); }
		state.TotalSpins++;
		SaveLuckyWheelDailyState();
		LUCKY_WHEEL_PENDING_REWARD pending; pending.Reward=*selected; pending.RewardIndex=selectedIndex;
		gLuckyWheelPendingReward[GetLuckyWheelAccountKey(lpObj)]=pending;
		LogAdd(LOG_BLUE,"[LuckyWheel] Account:%s Name:%s Payment:%d Reward:%s",lpObj->Account,lpObj->Name,type,selected->Name.c_str());
		// Nao anunciar aqui: o cliente ainda esta animando e a mensagem revelaria
		// o resultado antes de a roleta parar. O resultado aparece na propria janela.
		LuckyWheelSendResult(lpObj,1,selectedIndex);
		return true;
	}
}

void LuckyWheelClaim(int aIndex)
{
	if(!OBJECT_RANGE(aIndex) || !gObjIsConnectedGP(aIndex)) return;
	LPOBJ lpObj=&gObj[aIndex]; std::string key=GetLuckyWheelAccountKey(lpObj);
	std::map<std::string,LUCKY_WHEEL_PENDING_REWARD>::iterator it=gLuckyWheelPendingReward.find(key);
	PMSG_LUCKY_WHEEL_CLAIM_SEND pMsg; pMsg.header.set(0xEC,0x64,sizeof(pMsg)); pMsg.Success=0;
	if(it!=gLuckyWheelPendingReward.end() && LuckyWheelGiveReward(lpObj,it->second.Reward))
	{
		gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,"Lucky Wheel: premio resgatado: %s",it->second.Reward.Name.c_str());
		LogAdd(LOG_BLUE,"[LuckyWheel] Account:%s Name:%s Claimed:%s",lpObj->Account,lpObj->Name,it->second.Reward.Name.c_str());
		gLuckyWheelPendingReward.erase(it); pMsg.Success=1;
	}
	DataSend(aIndex,(BYTE*)&pMsg,sizeof(pMsg));
}

void LuckyWheelSendInfo(int aIndex, BYTE pool)
{
	if (!OBJECT_RANGE(aIndex) || !gObjIsConnectedGP(aIndex) || !g_pScriptLua) return;
	LPOBJ lpObj=&gObj[aIndex]; LUA_LUCKY_WHEEL_CONFIG config;
	if (!g_pScriptLua->GetLuckyWheelConfig(config)) return;
	PMSG_LUCKY_WHEEL_INFO_SEND pMsg; ZeroMemory(&pMsg,sizeof(pMsg));
	if(pool>3) pool=0;
	pMsg.header.set(0xEC,sizeof(pMsg)); pMsg.SubCode=0x62; pMsg.Enabled=config.Enable?1:0; pMsg.Pool=pool;
	pMsg.TabEnabled[0]=config.CashEnable; pMsg.TabEnabled[1]=config.GoldEnable; pMsg.TabEnabled[2]=config.FreeEnable; pMsg.TabEnabled[3]=config.GoblinEnable;
	LUCKY_WHEEL_DAILY_STATE& state=GetLuckyWheelDailyState(lpObj);
	pMsg.FreeRemaining=(BYTE)max(0,config.FreeSpinsDaily-state.FreeSpins);
	pMsg.DailyRemaining=(BYTE)max(0,config.DailyLimit-state.TotalSpins);
	pMsg.WCoinCost=config.WCoinCost; pMsg.WCoinPCost=config.WCoinPCost; pMsg.GoblinPointCost=config.GoblinPointCost; pMsg.ZenCost=config.ZenCost;
	pMsg.TicketType=(WORD)GET_ITEM(config.TicketSection,config.TicketIndex); pMsg.TicketCount=(BYTE)config.TicketCount;
	for (int n=1;n<=g_pScriptLua->GetLuckyWheelRewardCount() && pMsg.RewardCount<LUCKY_WHEEL_MAX_REWARDS;n++)
	{
		LUA_LUCKY_WHEEL_REWARD reward; if (!g_pScriptLua->GetLuckyWheelReward(n,reward) || reward.Pool!=pool) continue;
		PMSG_LUCKY_WHEEL_REWARD& out=pMsg.Reward[pMsg.RewardCount++];
		out.Kind=(BYTE)reward.Kind; out.ItemType=(WORD)GET_ITEM(reward.Section,reward.Index); out.Level=(BYTE)reward.Level;
		out.Amount=reward.Amount; out.Skill=(BYTE)reward.Skill; out.Luck=(BYTE)reward.Luck; out.Option=(BYTE)reward.Option;
		out.Excellent=(BYTE)reward.Excellent; out.Ancient=(BYTE)reward.Ancient; out.Harmony=(BYTE)reward.Harmony;
		out.ItemOptionEx=(BYTE)reward.ItemOptionEx; out.SocketBonus=(BYTE)reward.SocketBonus;
		for(int i=0;i<MAX_SOCKET_OPTION;i++) out.Socket[i]=(BYTE)reward.Socket[i];
		strncpy_s(out.Name,reward.Name.c_str(),_TRUNCATE);
	}
	DataSend(aIndex,(BYTE*)&pMsg,sizeof(pMsg));
}

void LuckyWheelSpin(int aIndex, BYTE pool)
{
	if (!OBJECT_RANGE(aIndex) || !gObjIsConnectedGP(aIndex)) return;
	static const char* names[]={"wcoin","wcoinp","free","gp"};
	if (pool>=4) return;
	LuckyWheelCommand(&gObj[aIndex],names[pool]);
}
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CCommandManager::CCommandManager() // OK
{
}

CCommandManager::~CCommandManager() // OK
{
}

void CCommandManager::Init() // OK
{
	for (int n = 0; n < MAX_COMMAND; n++)
	{
		this->m_CommandInfo[n].code = -1;

		this->Add(gCommand.GetCommand(n), n + 1);
	}
}

void CCommandManager::MainProc() // OK
{
	for (int n = OBJECT_START_USER; n < MAX_OBJECT; n++)
	{
		if (gObjIsConnectedGP(n) == 0)
		{
			continue;
		}

		if (gObj[n].AutoAddPointCount > 0)
		{
			this->CommandAddPointAutoProc(&gObj[n]);
		}

		if (gObj[n].AutoResetEnable != 0)
		{
			this->CommandResetAutoProc(&gObj[n]);
		}
	}
}

void CCommandManager::Add(char* label, int code) // OK
{
	for (int n = 0; n < MAX_COMMAND; n++)
	{
		if (this->m_CommandInfo[n].code != -1)
		{
			continue;
		}

		this->m_CommandInfo[n].code = code;
		strcpy_s(this->m_CommandInfo[n].label, label);
		break;
	}
}

long CCommandManager::GetNumber(char* arg, int pos) // OK
{
	int count = 0, p = 0;

	char buffer[60] = { 0 };

	int len = strlen(arg);

	len = ((len >= sizeof(buffer)) ? (sizeof(buffer) - 1) : len);

	for (int n = 0; n < len; n++)
	{
		if (arg[n] == 0x20)
		{
			count++;
		}
		else if (count == pos)
		{
			buffer[p] = arg[n];
			p++;
		}
	}

	return atoi(buffer);
}

void CCommandManager::GetString(char* arg, char* out, int size, int pos) // OK
{
	int count = 0, p = 0;

	char buffer[60] = { 0 };

	int len = strlen(arg);

	len = ((len >= sizeof(buffer)) ? (sizeof(buffer) - 1) : len);

	for (int n = 0; n < len; n++)
	{
		if (arg[n] == 0x20)
		{
			count++;
		}
		else if (count == pos)
		{
			buffer[p] = arg[n];
			p++;
		}
	}

	memcpy(out, buffer, (size - 1));
}

long CCommandManager::GetCommandCode(char* label) // OK
{
	for (int n = 0; n < MAX_COMMAND; n++)
	{
		if (this->m_CommandInfo[n].code == -1)
		{
			continue;
		}

		if (_stricmp(label, this->m_CommandInfo[n].label) == 0)
		{
			return this->m_CommandInfo[n].code;
		}
	}

	return 0;
}

bool CCommandManager::ManagementCore(LPOBJ lpObj, char* message, int Npc) // OK
{
	char command[32] = { 0 };

	memset(command, 0, sizeof(command));

	this->GetString(message, command, sizeof(command), 0);

	// Read-only report; authoritative switch remains in GS Common.
	if (_stricmp(command, "/excellent") == 0)
	{
		gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,"Excellent Rare/Legendary/Epic: %s (Common: TierSystemEnable).",ExcRankRuntime::Enabled()?"ATIVADO":"DESATIVADO");
		return true;
	}

    if(_stricmp(command,"/tierex")==0) {
        if(!gGameMaster.CheckGameMasterLevel(lpObj,1))return true;
        std::istringstream input(message);std::string verb,token;unsigned slot=0;std::vector<unsigned> selection;
        bool valid=bool(input>>verb>>token) && ExcRank::Number(token,slot);
        while(valid && (input>>token)){unsigned value=0;if(!ExcRank::Number(token,value)||selection.size()>=12){valid=false;break;}selection.push_back(value);}
        if(!valid || !NativeExcellentAdmin::Assign(lpObj,slot,selection))
            gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,"Uso: /tierex slot rank OU /tierex slot flag rank [flag rank]. Informe todas as EX do item; nao sobrescreve tiers.");
        return true;
    }
    if(_stricmp(command,"/excrank")==0) {
        if(!gGameMaster.CheckGameMasterLevel(lpObj,1)){gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,"ExcRank: sem permissao.");return true;}
        std::istringstream args(message);std::string verb,mode,token;unsigned slot=0;ExcRank::ItemOptions block;bool valid=true;
        args>>verb;if(!(args>>token)||!ExcRank::Number(token,slot)||slot>=INVENTORY_SIZE||!(args>>mode))valid=false;
        if(valid&&mode=="random") {
            unsigned rank=0;if(args>>token){valid=ExcRank::Number(token,rank)&&rank>=1&&rank<=3;}if(args>>token)valid=false;
            struct Random {bool Draw(std::uint32_t total,std::uint32_t& roll){if(!total)return false;const std::uint32_t threshold=(std::uint32_t(0)-total)%total;for(int tries=0;tries<64;++tries){std::uint32_t n=0;if(BCryptGenRandom(0,reinterpret_cast<PUCHAR>(&n),sizeof(n),BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0)return false;if(n>=threshold){roll=n%total;return true;}}return false;}} rng;
            if(valid)valid=ExcRank::Generate(*ExcRank::Policy(),lpObj->Inventory[slot].m_Index,ExcRank::Origin::Admin,rng,block,static_cast<ExcRank::Rank>(rank));
        } else if(valid) {
            do {unsigned option=0,rank=0;if(!ExcRank::Number(mode,option)||option<1||option>12||!(args>>token)||!ExcRank::Number(token,rank)||rank<1||rank>3||block.slots.size()>=4){valid=false;break;}block.slots.push_back(ExcRank::Slot(option,static_cast<ExcRank::Rank>(rank)));}while(args>>mode);
        }
        if(!valid)block.slots.clear();
        if(block.slots.empty()||!ExcRankAdmin::Assign(lpObj,slot,block))gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,"Uso: /excrank slot opcao rank [opcao rank] ou /excrank slot random [1..3]. Nao sobrescreve bloco existente.");
        return true;
    }

	if (_stricmp(command, "/clima") == 0)
	{
		if (gGameMaster.CheckGameMasterLevel(lpObj, 1) == 0)
		{
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "Clima: sem permissao.");
			return 1;
		}

		char mode[24] = { 0 };
		char target[24] = { 0 };
		this->GetString(message, mode, sizeof(mode), 1);
		this->GetString(message, target, sizeof(target), 2);
		if (mode[0] == 0 || _stricmp(mode, "status") == 0)
		{
			const int currentType = gMap[lpObj->Map].GetWeather() >> 4;
			const int currentIntensity = gMap[lpObj->Map].GetWeather() & 15;
			const char* currentName = "limpo";
			if (currentType == 1) currentName = currentIntensity <= 4 ? "garoa" : "chuva";
			else if (currentType == 2) currentName = "tempestade";
			else if (currentType == 3) currentName = "tempestade de areia";
			else if (currentType == 4) currentName = currentIntensity >= 15 ? "nevasca" : "neve";
			else if (currentType == 5) currentName = "nevoa";
			else if (currentType == 6) currentName = "nublado";
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "Clima mapa %d: %s (intensidade %d, modo %s).", lpObj->Map, currentName, currentIntensity, gMap[lpObj->Map].IsWeatherAutomatic() ? "automatico" : "manual");
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "/clima: 0=automatico, 1=ceu limpo, 2=chuva, 3=tempestade, 4=areia.");
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "/clima: 5=neve, 6=nevasca, 7=nevoa, 8=garoa, 9=nublado.");
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "/clima off = desliga o clima em TODOS os mapas.");
			return 1;
		}
		const bool automatic = (_stricmp(mode, "auto") == 0 || strcmp(mode, "0") == 0);
		const bool clearAll = (_stricmp(mode, "off") == 0 || _stricmp(mode, "desligar") == 0 || _stricmp(mode, "desativar") == 0);
		BYTE weather = 0;
		BYTE intensity = 0;
		if (_stricmp(mode, "chuva") == 0 || _stricmp(mode, "rain") == 0 || strcmp(mode, "2") == 0)
		{
			weather = 1;
			intensity = 10;
		}
		else if (_stricmp(mode, "tempestade") == 0 || _stricmp(mode, "storm") == 0 || strcmp(mode, "3") == 0)
		{
			weather = 2;
			intensity = 14;
		}
		else if (_stricmp(mode, "areia") == 0 || _stricmp(mode, "sand") == 0 || strcmp(mode, "4") == 0)
		{
			weather = 3;
			intensity = 12;
		}
		else if (_stricmp(mode, "neve") == 0 || _stricmp(mode, "snow") == 0 || strcmp(mode, "5") == 0)
		{
			weather = 4;
			intensity = 10;
		}
		else if (_stricmp(mode, "nevasca") == 0 || _stricmp(mode, "blizzard") == 0 || strcmp(mode, "6") == 0)
		{
			weather = 4;
			intensity = 15;
		}
		else if (_stricmp(mode, "nevoa") == 0 || _stricmp(mode, "fog") == 0 || strcmp(mode, "7") == 0)
		{
			weather = 5;
			intensity = 10;
		}
		else if (_stricmp(mode, "garoa") == 0 || _stricmp(mode, "drizzle") == 0 || strcmp(mode, "8") == 0)
		{
			weather = 1;
			intensity = 4;
		}
		else if (_stricmp(mode, "nublado") == 0 || _stricmp(mode, "cloudy") == 0 || strcmp(mode, "9") == 0)
		{
			weather = 6;
			intensity = 11;
		}
		else if (_stricmp(mode, "limpo") != 0 && _stricmp(mode, "clear") != 0 && strcmp(mode, "1") != 0 && !automatic && !clearAll)
		{
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "Uso: /clima 0|1|2|3|4|5|6|7|8|9 [mapa|todos] ou /clima off.");
			return 1;
		}

		int firstMap = lpObj->Map;
		int lastMap = lpObj->Map;
		if (clearAll || _stricmp(target, "todos") == 0 || _stricmp(target, "all") == 0)
		{
			firstMap = 0;
			lastMap = MAX_MAP - 1;
		}
		else if (target[0] != 0)
		{
			char* end = 0;
			long parsed = strtol(target, &end, 10);
			if (*end != 0 || parsed < 0 || parsed >= MAX_MAP)
			{
				gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "Clima: mapa invalido.");
				return 1;
			}
			firstMap = lastMap = (int)parsed;
		}

		for (int map = firstMap; map <= lastMap; ++map)
		{
			if (automatic)
			{
				gMap[map].SetWeatherAutomatic();
			}
			else
			{
				gMap[map].SetWeather(weather, intensity);
			}
		}
		const char* appliedName = "ceu limpo";
		if (automatic) appliedName = "automatico";
		else if (weather == 1) appliedName = intensity <= 4 ? "garoa" : "chuva";
		else if (weather == 2) appliedName = "tempestade";
		else if (weather == 3) appliedName = "tempestade de areia";
		else if (weather == 4) appliedName = intensity >= 15 ? "nevasca" : "neve";
		else if (weather == 5) appliedName = "nevoa";
		else if (weather == 6) appliedName = "nublado";
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "Clima: %s aplicado em %d mapa(s).", appliedName, lastMap - firstMap + 1);
		return 1;
	}

	if (_stricmp(command, "/tempo") == 0)
	{
		if (gGameMaster.CheckGameMasterLevel(lpObj, 1) == 0)
		{
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "Tempo: sem permissao.");
			return 1;
		}
		char value[24] = { 0 };
		this->GetString(message, value, sizeof(value), 1);
		if (value[0] == 0 || _stricmp(value, "status") == 0)
		{
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "Tempo visual: %s. /tempo 0=real, 1=dia, 2=tarde, 3=noite, 4=amanhecer.", IsVisualTimeAutomatic() ? "relogio real" : "horario fixo");
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "Horario exato: /tempo HH:MM.");
			return 1;
		}
		if (_stricmp(value, "auto") == 0 || _stricmp(value, "real") == 0 || strcmp(value, "0") == 0)
		{
			SetVisualTimeAutomatic();
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "Tempo: relogio real do servidor ativado.");
			return 1;
		}
		int hour = -1;
		int minute = -1;
		if (strcmp(value, "1") == 0) { hour = 12; minute = 0; }
		else if (strcmp(value, "2") == 0) { hour = 18; minute = 0; }
		else if (strcmp(value, "3") == 0) { hour = 0; minute = 0; }
		else if (strcmp(value, "4") == 0) { hour = 6; minute = 0; }
		char extra = 0;
		if ((hour < 0 && sscanf_s(value, "%d:%d%c", &hour, &minute, &extra, 1) != 2) || !SetVisualTimeManual(hour, minute))
		{
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "Uso: /tempo 0|1|2|3|4 ou HH:MM (00:00-23:59).");
			return 1;
		}
		const char* timeName = strcmp(value, "1") == 0 ? "dia" :
			strcmp(value, "2") == 0 ? "tarde" :
			strcmp(value, "3") == 0 ? "noite" :
			strcmp(value, "4") == 0 ? "amanhecer" : "horario personalizado";
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "Tempo: %s (%02d:%02d) definido.", timeName, hour, minute);
		return 1;
	}

	if (_stricmp(command, "/lucky") == 0 || _stricmp(command, "/roleta") == 0)
	{
		char payment[16] = { 0 };
		this->GetString(message, payment, sizeof(payment), 1);
		return LuckyWheelCommand(lpObj, payment);
	}

	if (_stricmp(command, "/build") == 0)
	{
		char action[16] = { 0 };
		char slot[16] = { 0 };
		this->GetString(message, action, sizeof(action), 1);
		this->GetString(message, slot, sizeof(slot), 2);
		return gBuildProfile.Command(lpObj, action, slot);
	}

	if (_stricmp(command, "/fakebot") == 0)
	{
		if (gGameMaster.CheckGameMasterLevel(lpObj, 1) == 0)
		{
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "FakeBot: voce nao tem permissao.");
			return 0;
		}

		char action[24] = { 0 };
		this->GetString(message, action, sizeof(action), 1);

		if (_stricmp(action, "connectall") == 0)
		{
			int queued = gFakeBot.ConnectAll();
			if (queued < 0)
			{
				gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "FakeBot: sistema desativado.");
			}
			else
			{
				gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "FakeBot: %d conexao(oes) agendada(s).", queued);
			}
			return 1;
		}

		if (_stricmp(action, "disconnectall") == 0)
		{
			int disconnected = gFakeBot.DisconnectAll("GM command");
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "FakeBot: %d desconectado(s).", disconnected);
			return 1;
		}

		if (_stricmp(action, "status") == 0)
		{
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "FakeBot: %d online de %d carregado(s).", gFakeBot.GetOnlineCount(), gFakeBot.GetLoadedCount());
			return 1;
		}

		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "Uso: /fakebot connectall | disconnectall | status");
		return 1;
	}

#if(BOT_PLAYER == 1)
	// Bot test commands are self-authorized by CommandBot/CommandKillBot. Handle
	// them before Command.txt lookup so a stale numeric command table cannot
	// silently discard /boot after new commands are added.
	if (_stricmp(command, "/boot") == 0 || _stricmp(command, "/fake") == 0)
	{
		char* argument = &message[strlen(command)];
		if (argument[0] == 0x20)
		{
			argument++;
		}
		return this->CommandBot(lpObj, argument);
	}

	if (_stricmp(command, "/killbot") == 0)
	{
		char* argument = &message[strlen(command)];
		if (argument[0] == 0x20)
		{
			argument++;
		}
		return this->CommandKillBot(lpObj, argument);
	}
#endif

	COMMAND_LIST CommandData;

	if (gCommand.GetInfoByName(command, &CommandData) == 0)
	{
		return 0;
	}

	int code = this->GetCommandCode(command);

	char* argument = &message[strlen(command)];

	if (argument[0] == 0x20)
	{
		argument++;
	}


	if (CommandData.Enable[lpObj->AccountLevel] == 0)
	{
		return 0;
	}

	if (lpObj->Money < (DWORD)CommandData.Money[lpObj->AccountLevel]) //OK
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(67), CommandData.Money[lpObj->AccountLevel]);

		if (Npc >= 0)
		{
			GCChatTargetNewSend(lpObj, Npc, gMessage.GlobalText(67), CommandData.Money[lpObj->AccountLevel]);
		}

		return 0;
	}


	if ((DWORD)CommandData.Money[lpObj->AccountLevel] > 0) //OK
	{
		if (lpObj->Interface.use == INTERFACE_TRADE)
		{
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(835));

			if (Npc >= 0)
			{
				GCChatTargetNewSend(lpObj, Npc, gMessage.GlobalText(835));
			}
			return 0;
		}
	}

	if (lpObj->Level < (CommandData.MinLevel[lpObj->AccountLevel])) //OK
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(65), CommandData.MinLevel[lpObj->AccountLevel]);

		if (Npc > 0)
		{
			GCChatTargetNewSend(lpObj, Npc, gMessage.GlobalText(65));
		}
		return 0;
	}

	if (CommandData.MaxLevel[lpObj->AccountLevel] != 0 && lpObj->Level > (CommandData.MaxLevel[lpObj->AccountLevel])) //OK
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(765), CommandData.MaxLevel[lpObj->AccountLevel]);

		if (Npc >= 0)
		{
			GCChatTargetNewSend(lpObj, Npc, gMessage.GlobalText(765), CommandData.MaxLevel[lpObj->AccountLevel]);
		}
		return 0;
	}

	if (lpObj->Reset < (CommandData.MinReset[lpObj->AccountLevel])) //OK
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(66), CommandData.MinReset[lpObj->AccountLevel]);

		if (Npc >= 0)
		{
			GCChatTargetNewSend(lpObj, Npc, gMessage.GlobalText(66), CommandData.MinReset[lpObj->AccountLevel]);
		}
		return 0;
	}

	if (CommandData.MaxReset[lpObj->AccountLevel] != 0 && lpObj->Reset > (CommandData.MaxReset[lpObj->AccountLevel])) //OK
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(232), CommandData.MaxReset[lpObj->AccountLevel]);

		if (Npc >= 0)
		{
			GCChatTargetNewSend(lpObj, Npc, gMessage.GlobalText(232), CommandData.MaxReset[lpObj->AccountLevel]);
		}
		return 0;
	}

	DWORD tick = (GetTickCount() - lpObj->CommandDelay[code]) / 1000;

	if (tick < ((DWORD)CommandData.Delay)) //OK
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(68), (CommandData.Delay - tick));

		if (Npc >= 0)
		{
			GCChatTargetNewSend(lpObj, Npc, gMessage.GlobalText(68), (CommandData.Delay - tick));
		}
		return 0;
	}

	if (CommandData.GameMaster != 0 && gGameMaster.CheckGameMasterLevel(lpObj, CommandData.GameMaster) == 0) //OK
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(64));

		if (Npc >= 0)
		{
			GCChatTargetNewSend(lpObj, Npc, gMessage.GlobalText(64));
		}
		return 0;
	}

	if (CommandData.Coin1 > 0 || CommandData.Coin2 > 0 || CommandData.Coin3 > 0)
	{
		if (lpObj->CommandNotice[code] == 0)
		{
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(837), CommandData.Coin1, CommandData.Coin2, CommandData.Coin3);
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(838));

			if (Npc >= 0)
			{
				GCChatTargetNewSend(lpObj, Npc, gMessage.GlobalText(837), CommandData.Coin1, CommandData.Coin2, CommandData.Coin3);
			}

			lpObj->CommandNotice[code] = 1;
			return 0;
		}
	}

	if (lpObj->Coin1 < CommandData.Coin1) //OK
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(766), CommandData.Coin1);

		if (Npc >= 0)
		{
			GCChatTargetNewSend(lpObj, Npc, gMessage.GlobalText(766), CommandData.Coin1);
		}
		return 0;
	}

	if (lpObj->Coin2 < CommandData.Coin2) //OK
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(767), CommandData.Coin2);

		if (Npc >= 0)
		{
			GCChatTargetNewSend(lpObj, Npc, gMessage.GlobalText(767), CommandData.Coin2);
		}
		return 0;
	}

	if (lpObj->Coin3 < CommandData.Coin3) //OK
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(768), CommandData.Coin3);

		if (Npc >= 0)
		{
			GCChatTargetNewSend(lpObj, Npc, gMessage.GlobalText(768), CommandData.Coin3);
		}
		return 0;
	}

	int Result = 0;

	switch (code)
	{
	case COMMAND_MOVE:
		Result = CommandMove(lpObj, argument);
		break;
	case COMMAND_POST:
		Result = CommandPost(lpObj, argument);
		break;
	case COMMAND_ADD_POINT1:
		Result = CommandAddPoint(lpObj, argument, 0);
		break;
	case COMMAND_ADD_POINT2:
		Result = CommandAddPoint(lpObj, argument, 1);
		break;
	case COMMAND_ADD_POINT3:
		Result = CommandAddPoint(lpObj, argument, 2);
		break;
	case COMMAND_ADD_POINT4:
		Result = CommandAddPoint(lpObj, argument, 3);
		break;
	case COMMAND_ADD_POINT5:
		Result = CommandAddPoint(lpObj, argument, 4);
		break;
	case COMMAND_PK_CLEAR:
		Result = CommandPKClear(lpObj, argument, Npc);
		break;
	case COMMAND_MONEY:
		Result = CommandMoney(lpObj, argument);
		break;
	case COMMAND_SETLEVEL:
		Result = CommandSetLevel(lpObj, argument);
		break;
	case COMMAND_SETSTATUS:
		Result = this->CommandSetStatus(lpObj, argument);
		break;
	case COMMAND_SET_KIT:
		Result = this->CommandSetKit(lpObj, argument);
		break;
	case COMMAND_REGISTRAR_TESTE:
		Result = this->CommandRegistrar(lpObj, argument);
		break;
	case COMMAND_BOT:
		Result = this->CommandBot(lpObj, argument);
		break;
	case COMMAND_KILLBOT:
		Result = this->CommandKillBot(lpObj, argument);
		break;
	case COMMAND_CHANGE:
		Result = CommandChange(lpObj, argument, Npc);
		break;
	case COMMAND_WARE:
		Result = CommandWare(lpObj, argument);
		break;
	case COMMAND_RESET:
		Result = CommandReset(lpObj, argument, Npc);
		break;
	case COMMAND_GM_MOVE:
		Result = CommandGMMove(lpObj, argument);
		break;
	case COMMAND_GM_POST:
		Result = CommandGMPost(lpObj, argument);
		break;
	case COMMAND_TRACK:
		Result = CommandTrack(lpObj, argument);
		break;
	case COMMAND_TRACE:
		Result = CommandTrace(lpObj, argument);
		break;
	case COMMAND_DISCONNECT:
		Result = CommandDisconnect(lpObj, argument);
		break;
	case COMMAND_FIREWORKS:
		Result = CommandFireworks(lpObj, argument);
		break;
	case COMMAND_MAKE:
		Result = CommandMake(lpObj, argument);
		break;
	case COMMAND_SKIN:
		Result = CommandSkin(lpObj, argument);
		break;
	case COMMAND_SET_MONEY:
		Result = CommandSetMoney(lpObj, argument);
		break;
	case COMMAND_NOTICE:
		Result = CommandNotice(lpObj, argument);
		break;
	case COMMAND_MASTER_RESET:
		Result = CommandMasterReset(lpObj, argument, Npc);
		break;
	case COMMAND_GUILD_WAR:
		Result = CommandGuildWar(lpObj, argument);
		break;
	case COMMAND_BATTLE_SOCCER:
		Result = CommandBattleSoccer(lpObj, argument);
		break;
	case COMMAND_REQUEST:
		Result = CommandRequest(lpObj, argument);
		break;
	case COMMAND_CUSTOM_STORE:
		Result = gCustomStore.CommandCustomStore(lpObj, argument);
		break;
	case COMMAND_CUSTOM_OFFSTORE:
		Result = gCustomStore.CommandCustomStoreOffline(lpObj, argument);
		break;
	case COMMAND_HIDE:
		Result = CommandHide(lpObj, argument);
		break;
	case COMMAND_CUSTOM_ATTACK:
		Result = gCustomAttack.CommandCustomAttack(lpObj, argument);
		break;
	case COMMAND_CUSTOM_OFFATTACK:
		Result = gCustomAttack.CommandCustomAttackOffline(lpObj, argument);
		break;
	case COMMAND_REI:
		gReiDoMU.CommandReiDoMU(lpObj, argument);
		Result = 0;
		break;
	case COMMAND_CLEAR_INVENTORY:
		Result = CommandClearInventory(lpObj, argument);
		break;
	case COMMAND_SETVIP:
		Result = CommandSetVIP(lpObj, argument);
		break;
	case COMMAND_MARRY:
		Result = CommandMarry(lpObj, argument);
		break;
	case COMMAND_OPENWAREHOUSE:
		Result = CommandOpenWarehouse(lpObj);
		break;
	case COMMAND_CHANGECLASS:
		Result = CommandChangeClass(lpObj, argument, Npc);
		break;
	case COMMAND_REWARD:
		Result = CommandReward(lpObj, argument);
		break;
	case COMMAND_REWARDALL:
		Result = CommandRewardAll(lpObj, argument);
		break;
	case COMMAND_RELOAD:
		Result = CommandReload(lpObj, argument);
		break;
	case COMMAND_RENAME:
		Result = CommandRename(lpObj, argument);
		break;
	case COMMAND_EVENTQUICKLY:
		gEventQuickly.CommandEventQuickly(lpObj, argument);
		Result = 0;
		break;
	case COMMAND_INF:
		Result = CommandInfo(lpObj, argument);
		break;
	case COMMAND_BLOCCHAR:
		Result = CommandBlocChar(lpObj, argument);
		break;
	case COMMAND_BLOCACC:
		Result = CommandBlocAcc(lpObj, argument);
		break;
	case COMMAND_EVENTSTART:
		gEventStart.CommandEventStart(lpObj, argument);
		Result = 0;
		break;
	case COMMAND_EVENTSTARTJOIN:
		gEventStart.CommandEventStartJoin(lpObj, argument);
		Result = 0;
		break;
	case COMMAND_EVENTGET:
		gEventRunAndCatch.CommandEventRunAndCatch(lpObj, argument);
		Result = 0;
		break;
	case COMMAND_EVENTGETJOIN:
	{
		gEventRunAndCatch.CommandEventRunAndCatchJoin(lpObj, argument);
		gEventRussianRoulette.CommandEventRussianRouletteJoin(lpObj, argument);
		gEventKillAll.CommandEventKillAllJoin(lpObj, argument);
		Result = 0;
	}
	break;
	case COMMAND_EVENTESC:
		gEventHideAndSeek.CommandEventHideAndSeek(lpObj, argument);
		Result = 0;
		break;
	case COMMAND_MAKESET:
		Result = CommandMakeSet(lpObj, argument);
		break;
	case COMMAND_DROP:
		Result = CommandDrop(lpObj, argument);
		break;
	case COMMAND_QUEST:
		gCustomQuest.QuestCommand(lpObj, argument, Npc);
		Result = 0;
		break;
	case COMMAND_GIFT:
		Result = CommandGift(lpObj, argument);
		break;
	case COMMAND_EVENTROULETTE:
		gEventRussianRoulette.CommandEventRussianRoulette(lpObj, argument);
		Result = 0;
		break;
	case COMMAND_TOP:
		Result = CommandTop(lpObj, argument);
		break;
	case COMMAND_READD:
		Result = CommandReAdd(lpObj, argument, Npc);
		break;
	case COMMAND_QUIZ:
		gCustomQuiz.CommandQuiz(lpObj, argument);
		Result = 0;
		break;
	case COMMAND_PICK:
		Result = gCustomPick.CommandPick(lpObj, argument);
		break;
	case COMMAND_HELPER:
		Result = CommandHelper(lpObj, argument, Npc);
		break;
	case COMMAND_REMASTER:
		Result = CommandReMaster(lpObj, argument, Npc);
		break;
	case COMMAND_STARTBC:
		Result = CommandStartBC(lpObj, argument);
		break;
	case COMMAND_STARTDS:
		Result = CommandStartDS(lpObj, argument);
		break;
	case COMMAND_STARTCC:
		Result = CommandStartCC(lpObj, argument);
		break;
	case COMMAND_STARTIT:
		Result = CommandStartIT(lpObj, argument);
		break;
	case COMMAND_DISABLEPVP:
		Result = CommandDisablePvp(lpObj, argument, Npc);
		break;
	case COMMAND_LOCK:
		Result = CommandLock(lpObj, argument);
		break;
	case COMMAND_UNLOCK:
		Result = CommandUnLock(lpObj, argument);
		break;
	case COMMAND_MOVEALL:
		Result = CommandMoveAll(lpObj, argument);
		break;
	case COMMAND_MOVEGUILD:
		Result = CommandMoveGuild(lpObj, argument);
		break;
	case COMMAND_PVP:
		Result = gEventPvP.CommandEventPvP(lpObj, argument);
		break;
	case COMMAND_KILLALL:
		Result = gEventKillAll.CommandEventKillAll(lpObj, argument);
		break;
	case COMMAND_STARTTVT:
		Result = CommandStartTvT(lpObj, argument);
		break;
	case COMMAND_SPOT:
		Result = CommandSpot(lpObj, argument);
		break;
	case COMMAND_SETCOIN:
		Result = CommandSetCoin(lpObj, argument);
		break;
	case COMMAND_PACK:
		Result = gJewelMix.CommandJewelMix(lpObj, argument);
		break;
	case COMMAND_UNPACK:
		Result = gJewelMix.CommandJewelUnMix(lpObj, argument);
		break;
	case COMMAND_STARTQUIZ:
		Result = CommandStartQuiz(lpObj, argument);
		break;
	case COMMAND_STARTDROP:
		Result = CommandStartDrop(lpObj, argument);
		break;
	case COMMAND_STARTKING:
		Result = CommandStartKing(lpObj, argument);
		break;
	case COMMAND_STARTINVASION:
		Result = CommandStartInvasion(lpObj, argument);
		break;
	case COMMAND_STARTARENA:
		Result = CommandStartCustomArena(lpObj, argument);
		break;
	case COMMAND_STARTGVG:
		Result = CommandStartGvG(lpObj, argument);
		break;
#ifdef EVENT_FIND_PATH_NPC_CLIK
	case COMMAND_START_FIND_PATH_NPC:
		Result = CommandStartFindPathToNpc(lpObj, argument);
		break;
#endif // EVENT_FIND_PATH_NPC_CLIK
#ifdef PARTY_PASSWORD_JOIN
	case COMMAND_SET_PARTY:
		Result = CommandSetParty(lpObj, argument);
		break;
	case COMMAND_WARP:
		Result = CommandGMWarp(lpObj, argument);
		break;
#endif // PARTY_PASSWORD_JOIN

	default:
		return 0;
	}

	if (Result)
	{
		this->RemoveRequisites(lpObj, CommandData.Index);
		lpObj->CommandDelay[code] = GetTickCount();
	}

	return 1;
}

void CCommandManager::RemoveRequisites(LPOBJ lpObj, int IdCommand)
{
	COMMAND_LIST CommandData;

	if (gCommand.GetInfo(IdCommand, &CommandData) == 1)
	{
		if (CommandData.Money[lpObj->AccountLevel] > 0)
		{
			lpObj->Money -= CommandData.Money[lpObj->AccountLevel];

			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(836), CommandData.Money[lpObj->AccountLevel]);

			GCMoneySend(lpObj->Index, lpObj->Money);
		}

		if (CommandData.Coin1 > 0 || CommandData.Coin2 > 0 || CommandData.Coin3 > 0)
		{
			GDSetCoinSend(lpObj->Index, -(CommandData.Coin1), -(CommandData.Coin2), -(CommandData.Coin3), "Command");
		}
	}
}

bool CCommandManager::CommandMove(LPOBJ lpObj, char* arg) // OK
{
	MOVE_INFO MoveInfo;

	if (gMove.GetInfoByName(arg, &MoveInfo) != 0)
	{
		gMove.Move(lpObj, MoveInfo.Index);
		return 1;
	}
	return 0;
}

bool CCommandManager::CommandGMWarp(LPOBJ lpObj, char* arg) // OK
{
	int x = this->GetNumber(arg, 0);
	int y = this->GetNumber(arg, 1);

	if (x <= 0 || y <= 0 || x >= 256 || y >= 256)
	{
		return 0;
	}

	gObjTeleport(lpObj->Index, lpObj->Map, x, y);

	gLog.Output(LOG_COMMAND, "[CommandGMWarp][%s][%s] - (Map: %d, X: %d, Y: %d)", lpObj->Account, lpObj->Name, lpObj->Map, x, y);

	return 1;
}

bool CCommandManager::CommandPost(LPOBJ lpObj, char* arg) // OK
{
	gFilter.CheckSyntax(arg);

	if (gServerInfo.m_SwitchChatAntiSpam == 1)
	{
		if (lpObj->ChatLimitTime > 0)
		{
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(269), lpObj->ChatLimitTime);
			return 1;
		}

		if (GMAntiSpam->InvokeMessage(lpObj->Account, arg) == false)
		{
			lpObj->ChatLimitTime = gServerInfo.m_ChatAntiSpamTimeBlock;
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(269), lpObj->ChatLimitTime);
			return 1;
		}
	}

	if (gServerInfo.m_CommandPostType == 0)
	{
		PostMessageNew(lpObj->Name, gMessage.GlobalText(69), arg);
	}
	else if (gServerInfo.m_CommandPostType == 1)
	{
		PostMessage2(lpObj->Name, gMessage.GlobalText(69), arg);
	}
	else if (gServerInfo.m_CommandPostType == 2)
	{
		PostMessage3(lpObj->Name, gMessage.GlobalText(69), arg);
	}
	else if (gServerInfo.m_CommandPostType == 3)
	{
		PostMessage4(lpObj->Name, gMessage.GlobalText(69), arg);
	}
	else if (gServerInfo.m_CommandPostType == 4)
	{
		GDGlobalPostSend(gMapServerManager.GetMapServerGroup(), 0, lpObj->Name, arg);
	}
	else if (gServerInfo.m_CommandPostType == 5)
	{
		GDGlobalPostSend(gMapServerManager.GetMapServerGroup(), 1, lpObj->Name, arg);
	}
	else if (gServerInfo.m_CommandPostType == 6)
	{
		GDGlobalPostSend(gMapServerManager.GetMapServerGroup(), 2, lpObj->Name, arg);
	}
	else if (gServerInfo.m_CommandPostType == 7)
	{
		GDGlobalPostSend(gMapServerManager.GetMapServerGroup(), 3, lpObj->Name, arg);
	}

	gLog.Output(LOG_COMMAND, "[CommandPost][%s][%s] - (Message: %s)", lpObj->Account, lpObj->Name, arg);

	return 1;
}

bool CCommandManager::CommandAddPoint(LPOBJ lpObj, char* arg, int type) // OK
{

	if (lpObj->Class != CLASS_DL && type == 4)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(71));
		return 0;
	}

	char mode[5] = { 0 };

	this->GetString(arg, mode, sizeof(mode), 0);

	if (strcmp(mode, "auto") == 0)
	{
		this->CommandAddPointAuto(lpObj, arg, type);
		return 1;
	}

	int amount = this->GetNumber(arg, 0);

	if (amount <= 0 || lpObj->LevelUpPoint < amount)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(72));
		return 0;
	}

	if (gObjectManager.CharacterLevelUpPointAdd(lpObj, type, amount) == 0)
	{
		return 0;
	}

	GCNewCharacterInfoSend(lpObj);

	gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(74), amount, lpObj->LevelUpPoint);

	gLog.Output(LOG_COMMAND, "[CommandAddPoint][%s][%s] - (Type: %d, Amount: %d)", lpObj->Account, lpObj->Name, type, amount);

	return 1;
}

void CCommandManager::CommandAddPointAuto(LPOBJ lpObj, char* arg, int type) // OK
{
	if (gServerInfo.m_CommandAddPointAutoEnable[lpObj->AccountLevel] == 0)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(70));
		return;
	}

	int amount = this->GetNumber(arg, 1);

	if (amount > 0)
	{
		lpObj->AutoAddPointCount = 0;

		lpObj->AutoAddPointStats[type] = amount;

		lpObj->AutoAddPointCount = ((lpObj->AutoAddPointStats[0] > 0) ? (lpObj->AutoAddPointCount + 1) : lpObj->AutoAddPointCount);

		lpObj->AutoAddPointCount = ((lpObj->AutoAddPointStats[1] > 0) ? (lpObj->AutoAddPointCount + 1) : lpObj->AutoAddPointCount);

		lpObj->AutoAddPointCount = ((lpObj->AutoAddPointStats[2] > 0) ? (lpObj->AutoAddPointCount + 1) : lpObj->AutoAddPointCount);

		lpObj->AutoAddPointCount = ((lpObj->AutoAddPointStats[3] > 0) ? (lpObj->AutoAddPointCount + 1) : lpObj->AutoAddPointCount);

		lpObj->AutoAddPointCount = ((lpObj->AutoAddPointStats[4] > 0) ? (lpObj->AutoAddPointCount + 1) : lpObj->AutoAddPointCount);

		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(114), amount);

		gLog.Output(LOG_COMMAND, "[CommandAddPointAuto][%s][%s] - (Type: %d, Amount: %d)", lpObj->Account, lpObj->Name, type, amount);
	}
	else
	{
		lpObj->AutoAddPointCount = 0;

		lpObj->AutoAddPointStats[type] = 0;

		lpObj->AutoAddPointCount = ((lpObj->AutoAddPointStats[0] > 0) ? (lpObj->AutoAddPointCount + 1) : lpObj->AutoAddPointCount);

		lpObj->AutoAddPointCount = ((lpObj->AutoAddPointStats[1] > 0) ? (lpObj->AutoAddPointCount + 1) : lpObj->AutoAddPointCount);

		lpObj->AutoAddPointCount = ((lpObj->AutoAddPointStats[2] > 0) ? (lpObj->AutoAddPointCount + 1) : lpObj->AutoAddPointCount);

		lpObj->AutoAddPointCount = ((lpObj->AutoAddPointStats[3] > 0) ? (lpObj->AutoAddPointCount + 1) : lpObj->AutoAddPointCount);

		lpObj->AutoAddPointCount = ((lpObj->AutoAddPointStats[4] > 0) ? (lpObj->AutoAddPointCount + 1) : lpObj->AutoAddPointCount);

		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(115));
	}
}

bool CCommandManager::CommandPKClear(LPOBJ lpObj, char* arg, int Npc) // OK
{
	if (lpObj->PKLevel <= PVP_NEUTRAL)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(76));

		if (Npc >= 0)
		{
			GCChatTargetNewSend(lpObj, Npc, gMessage.GlobalText(76));
		}
		return 0;
	}

	lpObj->PKLevel = PVP_NEUTRAL;
	lpObj->PKCount = 0;

	if (gServerInfo.m_PKTimePenaltyForKill > 0)
	{
		lpObj->PKTime = 0;
	}

	GCPKLevelSend(lpObj->Index, lpObj->PKLevel, lpObj->PKTime);

	gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(78));

	if (Npc >= 0)
	{
		GCChatTargetNewSend(lpObj, Npc, gMessage.GlobalText(78));
	}

	gLog.Output(LOG_COMMAND, "[CommandPKClear][%s][%s] - (PKLevel: %d)", lpObj->Account, lpObj->Name, lpObj->PKLevel);

	return 1;
}

bool CCommandManager::CommandMoney(LPOBJ lpObj, char* arg) // OK
{
	int money = this->GetNumber(arg, 0);

	if (money < 0)
	{
		return 0;
	}

	if (money == 0)
	{
		lpObj->Money = 0;
	}
	else if (gObjCheckMaxMoney(lpObj->Index, money) == 0)
	{
		lpObj->Money = MAX_MONEY;
	}
	else
	{
		lpObj->Money += money;
	}

	GCMoneySend(lpObj->Index, lpObj->Money);

	gLog.Output(LOG_COMMAND, "[CommandMoney][%s][%s] - (Money: %d)", lpObj->Account, lpObj->Name, money);

	return 1;
}

bool CCommandManager::CommandChange(LPOBJ lpObj, char* arg, int Npc) // OK
{
	int ChangeUp = lpObj->ChangeUp;

	if (lpObj->Class == CLASS_MG || lpObj->Class == CLASS_DL || lpObj->Class == CLASS_RF || lpObj->Class == CLASS_GL)
	{
		ChangeUp++;
	}

	if (ChangeUp >= gServerInfo.m_CommandChangeLimit[lpObj->AccountLevel])
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(83));

		if (Npc >= 0)
		{
			GCChatTargetNewSend(lpObj, Npc, gMessage.GlobalText(83));
		}
		return 0;
	}


	int indexMax = ((ChangeUp == 0) ? 3 : ((ChangeUp == 2 || ChangeUp == 3) ? 9 : 6));

	for (int n = 0; n <= indexMax; n++)
	{
		if (gQuest.CheckQuestListState(lpObj, n, QUEST_FINISH) == 0)
		{
			if (n != 3 || lpObj->Class == CLASS_DK)
			{
				gQuest.AddQuestList(lpObj, n, QUEST_ACCEPT);
				gQuestReward.InsertQuestReward(lpObj, n);
				gQuest.AddQuestList(lpObj, n, QUEST_FINISH);
			}
		}
	}

	lpObj->SendQuestInfo = 0;

	gQuest.GCQuestInfoSend(lpObj->Index);

	gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(84));

	if (Npc >= 0)
	{
		GCChatTargetNewSend(lpObj, Npc, gMessage.GlobalText(84));
	}

	gCustomRankUser.GCReqRankLevelUser(lpObj->Index, lpObj->Index);

	gLog.Output(LOG_COMMAND, "[CommandChange][%s][%s] - (ChangeUp: %d)", lpObj->Account, lpObj->Name, ChangeUp);

	return 1;
}

bool CCommandManager::CommandWare(LPOBJ lpObj, char* arg) // OK
{
	bool warehouseOpen = (lpObj->Interface.use != 0 && lpObj->Interface.type == INTERFACE_WAREHOUSE && lpObj->Interface.state != 0 && lpObj->LoadWarehouse != 0 && lpObj->LoadGuildWarehouse == 0);

	if ((lpObj->Interface.use != 0 && warehouseOpen == false) || lpObj->TradeDuel != 0 || lpObj->State == OBJECT_DELCMD || lpObj->DieRegen != 0 || lpObj->Teleport != 0)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(86));
		return 0;
	}

	int number = this->GetNumber(arg, 0);
	bool silent = (this->GetNumber(arg, 1) != 0);

	if (number < 0 || number >= gServerInfo.m_CommandWareNumber[lpObj->AccountLevel])
	{
		if (silent == false)
		{
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(87));
		}
		return 0;
	}

	if (warehouseOpen != false)
	{
		GDInventoryItemSaveSend(lpObj->Index);
		gWarehouse.GDWarehouseItemSaveSend(lpObj->Index);
	}

	lpObj->WarehouseMoney = 0;

	memset(lpObj->WarehouseMap, 0xFF, WAREHOUSE_SIZE);

	for (int n = 0; n < WAREHOUSE_SIZE; n++)
	{
		lpObj->Warehouse[n].Clear();
	}

	if (warehouseOpen != false)
	{
		lpObj->Interface.use = 1;
		lpObj->Interface.type = INTERFACE_WAREHOUSE;
		lpObj->Interface.state = 0;
		lpObj->LoadWarehouse = 0;
		lpObj->LoadGuildWarehouse = 0;
		lpObj->WarehouseNumber = number;
		gWarehouse.GDWarehouseItemSend(lpObj->Index, lpObj->Account);
	}
	else
	{
		lpObj->Interface.use = 0;
		lpObj->Interface.type = INTERFACE_NONE;
		lpObj->Interface.state = 0;
		lpObj->LoadWarehouse = 0;
		lpObj->LoadGuildWarehouse = 0;
		lpObj->WarehouseNumber = number;
	}

	if (silent == false)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(88), number);
	}

	gLog.Output(LOG_COMMAND, "[CommandWare][%s][%s] - (Number: %d)", lpObj->Account, lpObj->Name, number);

	return 1;
}

bool CCommandManager::CommandReset(LPOBJ lpObj, char* arg, int Npc) // OK
{
	char mode[5] = { 0 };

	this->GetString(arg, mode, sizeof(mode), 0);

	if (strcmp(mode, "auto") == 0)
	{
		this->CommandResetAuto(lpObj, arg, Npc);
		return 1;
	}

	if (lpObj->Interface.use != 0 || lpObj->TradeDuel != 0 || lpObj->State == OBJECT_DELCMD || lpObj->DieRegen != 0 || lpObj->Teleport != 0 || lpObj->PShopOpen != 0 || lpObj->SkillSummonPartyTime != 0)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(90));

		if (Npc >= 0)
		{
			GCChatTargetNewSend(lpObj, Npc, gMessage.GlobalText(90));
		}
		return 0;
	}

	int ResetLevel = gResetTable.GetResetLevel(lpObj->Reset + 1, lpObj->AccountLevel);

	if (((gMasterSkillTree.CheckMasterLevel(lpObj) == 0) ? lpObj->Level : (lpObj->Level + lpObj->MasterLevel)) < ResetLevel)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(91), ResetLevel);

		if (Npc >= 0)
		{
			GCChatTargetNewSend(lpObj, Npc, gMessage.GlobalText(91), ResetLevel);
		}
		return 0;
	}

	DWORD ResetMoney = (DWORD)gResetTable.GetResetMoney(lpObj->Reset + 1, lpObj->AccountLevel);

	if (lpObj->Money < ResetMoney)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(92), ResetMoney);

		if (Npc >= 0)
		{
			GCChatTargetNewSend(lpObj, Npc, gMessage.GlobalText(92), ResetMoney);
		}
		return 0;
	}

	BOOL CheckJewel = (BOOL)gResetTable.CheckResetJewel(lpObj, lpObj->Reset + 1, true);

	if (CheckJewel != TRUE)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "does not meet the jewelry conditions to continue");

		if (Npc >= 0)
		{
			GCChatTargetNewSend(lpObj, Npc, "does not meet the jewelry conditions to continue");
		}
		return 0;
	}

	if (lpObj->Reset >= gServerInfo.m_CommandResetLimit[lpObj->AccountLevel])
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(93), gServerInfo.m_CommandResetLimit[lpObj->AccountLevel]);

		if (Npc >= 0)
		{
			GCChatTargetNewSend(lpObj, Npc, gMessage.GlobalText(93), gServerInfo.m_CommandResetLimit[lpObj->AccountLevel]);
		}
		return 0;
	}

	if (gServerInfo.m_CommandResetCheckItem[lpObj->AccountLevel] != 0)
	{
		for (int n = 0; n < INVENTORY_WEAR_SIZE; n++)
		{
			if (lpObj->Inventory[n].IsItem() != 0)
			{
				gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(94));

				if (Npc >= 0)
				{
					GCChatTargetNewSend(lpObj, Npc, gMessage.GlobalText(94));
				}
				return 0;
			}
		}
	}

	if (((lpObj->CommandManagerTransaction[0] == 0) ? (lpObj->CommandManagerTransaction[0]++) : lpObj->CommandManagerTransaction[0]) != 0)
	{
		return 0;
	}

	SDHP_COMMAND_RESET_SEND pMsg;

	pMsg.header.set(0x0F, 0x00, sizeof(pMsg));

	pMsg.index = lpObj->Index;

	memcpy(pMsg.account, lpObj->Account, sizeof(pMsg.account));

	memcpy(pMsg.name, lpObj->Name, sizeof(pMsg.name));

	gDataServerConnection.DataSend((BYTE*)&pMsg, pMsg.header.size);

	return 0;
}

void CCommandManager::CommandResetAuto(LPOBJ lpObj, char* arg, int Npc) // OK
{
	if (gServerInfo.m_CommandResetAutoEnable[lpObj->AccountLevel] == 0)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(89));

		if (Npc >= 0)
		{
			GCChatTargetNewSend(lpObj, Npc, gMessage.GlobalText(89));
		}
		return;
	}

	if (lpObj->AutoResetEnable == 0)
	{
		lpObj->AutoResetEnable = 1;

		lpObj->AutoResetStats[0] = this->GetNumber(arg, 1);

		lpObj->AutoResetStats[1] = this->GetNumber(arg, 2);

		lpObj->AutoResetStats[2] = this->GetNumber(arg, 3);

		lpObj->AutoResetStats[3] = this->GetNumber(arg, 4);

		lpObj->AutoResetStats[4] = ((lpObj->Class == CLASS_DL) ? this->GetNumber(arg, 5) : 0);

		gCustomAttack.DGCustomAttackResumeSaveSend(lpObj->Index);

		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(116));

		if (Npc >= 0)
		{
			GCChatTargetNewSend(lpObj, Npc, gMessage.GlobalText(116));
		}
	}
	else
	{
		lpObj->AutoResetEnable = 0;

		lpObj->AutoResetStats[0] = 0;

		lpObj->AutoResetStats[1] = 0;

		lpObj->AutoResetStats[2] = 0;

		lpObj->AutoResetStats[3] = 0;

		lpObj->AutoResetStats[4] = 0;

		gCustomAttack.DGCustomAttackResumeSaveSend(lpObj->Index);

		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(117));

		if (Npc >= 0)
		{
			GCChatTargetNewSend(lpObj, Npc, gMessage.GlobalText(117));
		}
	}
}

bool CCommandManager::CommandGMMove(LPOBJ lpObj, char* arg) // OK
{

	char name[11] = { 0 };

	this->GetString(arg, name, sizeof(name), 0);

	int map = this->GetNumber(arg, 1);

	int x = this->GetNumber(arg, 2);

	int y = this->GetNumber(arg, 3);

	LPOBJ lpTarget = gObjFind(name);

	if (lpTarget == 0)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(100));
		return 0;
	}

	gObjTeleport(lpTarget->Index, map, x, y);

	gLog.Output(LOG_COMMAND, "[CommandGMMove][%s][%s] - (Name: %s, Map: %d, X: %d, Y: %d)", lpObj->Account, lpObj->Name, name, map, x, y);

	return 1;
}

bool CCommandManager::CommandGMPost(LPOBJ lpObj, char* arg) // OK
{
	gNotice.GCNoticeSendToAll(0, 0, 0, 0, 0, 0, "[%s] %s", lpObj->Name, arg);

	gLog.Output(LOG_COMMAND, "[CommandGMPost][%s][%s] - (Message: %s)", lpObj->Account, lpObj->Name, arg);

	return 1;
}

bool CCommandManager::CommandTrack(LPOBJ lpObj, char* arg) // OK
{
	char name[11] = { 0 };

	this->GetString(arg, name, sizeof(name), 0);

	LPOBJ lpTarget = gObjFind(name);

	if (lpTarget == 0)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(100));
		return 0;
	}

	gObjTeleport(lpObj->Index, lpTarget->Map, lpTarget->X, lpTarget->Y);

	gLog.Output(LOG_COMMAND, "[CommandTrack][%s][%s] - (Name: %s)", lpObj->Account, lpObj->Name, name);

	return 1;
}

bool CCommandManager::CommandTrace(LPOBJ lpObj, char* arg) // OK
{
	char name[11] = { 0 };

	this->GetString(arg, name, sizeof(name), 0);

	LPOBJ lpTarget = gObjFind(name);

	if (lpTarget == 0)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(100));
		return 0;
	}

	gObjTeleport(lpTarget->Index, lpObj->Map, lpObj->X, lpObj->Y);

	gLog.Output(LOG_COMMAND, "[CommandTrace][%s][%s] - (Name: %s)", lpObj->Account, lpObj->Name, name);

	return 1;
}

bool CCommandManager::CommandDisconnect(LPOBJ lpObj, char* arg) // OK
{
	char name[11] = { 0 };

	this->GetString(arg, name, sizeof(name), 0);

	LPOBJ lpTarget = gObjFind(name);

	if (lpTarget == 0)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(100));
		return 0;
	}

	gObjUserKill(lpTarget->Index);

	gLog.Output(LOG_COMMAND, "[CommandDisconnect][%s][%s] - (Name: %s)", lpObj->Account, lpObj->Name, name);

	return 1;
}

bool CCommandManager::CommandFireworks(LPOBJ lpObj, char* arg) // OK
{
	for (int n = 0; n < 15; n++)
	{
		GCFireworksSend(lpObj, (lpObj->X + (((GetLargeRand() % 5) * 2) - 4)), (lpObj->Y + (((GetLargeRand() % 5) * 2) - 4)));
	}

	gLog.Output(LOG_COMMAND, "[CommandFireworks][%s][%s] - (Map: %d, X: %d, Y: %d)", lpObj->Account, lpObj->Name, lpObj->Map, lpObj->X, lpObj->Y);

	return 1;
}

#include "ItemOptionRate.h"

bool CCommandManager::CommandMake(LPOBJ lpObj, char* arg) // OK
{
	int qtd = 0, section, type, level, skill, luck, option, exc, set, socket;

	BYTE ItemSocketOption[MAX_SOCKET_OPTION] = { 0xFF,0xFF,0xFF,0xFF,0xFF };

	section = this->GetNumber(arg, 0);

	int index = -1;

	if (section < 0)
	{
		qtd = section;
		section = this->GetNumber(arg, 1);
		type = this->GetNumber(arg, 2);
		level = this->GetNumber(arg, 3);
		skill = this->GetNumber(arg, 4);
		luck = this->GetNumber(arg, 5);
		option = this->GetNumber(arg, 6);
		exc = this->GetNumber(arg, 7);
		set = this->GetNumber(arg, 8);
		socket = this->GetNumber(arg, 9);

		index = GET_ITEM(section, type);
	}
	else
	{
		type = this->GetNumber(arg, 1);
		level = this->GetNumber(arg, 2);
		skill = this->GetNumber(arg, 3);
		luck = this->GetNumber(arg, 4);
		option = this->GetNumber(arg, 5);
		exc = this->GetNumber(arg, 6);
		set = this->GetNumber(arg, 7);
		socket = this->GetNumber(arg, 8);

		index = GET_ITEM(section, type);

		if (set < 0)
		{
			set *= -1;

			BYTE ItemSetOption = set;

			gItemOptionRate.GetItemOption5(set, &ItemSetOption);

			gItemOptionRate.MakeSetOption(index, ItemSetOption, &ItemSetOption);

			set = ItemSetOption;

			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "[ItemSetOption: %d]", ItemSetOption);
		}
	}

	for (int n = 0; n < socket && socket <= MAX_SOCKET_OPTION; n++)
	{
		ItemSocketOption[n] = 0xFE;
	}

	(qtd < 0) ? qtd -= qtd * 2 : qtd;



	if ((index >= GET_ITEM(14, 0) && index <= GET_ITEM(14, 6))
		|| (index >= GET_ITEM(14, 35) && index <= GET_ITEM(14, 40))
		|| index == GET_ITEM(14, 70) || index == GET_ITEM(14, 71) || index == GET_ITEM(14, 133))
	{
		level = 0;
	}

	if (qtd > 0)
	{
		for (int n = 0; n < qtd; n++)
		{
			GDCreateItemSend(lpObj->Index, 0xEB, 0, 0, index, level, 0, skill, luck, option, -1, exc, set, 0, 0, ItemSocketOption, 0xFF, 0);
		}
	}
	else
	{
		GDCreateItemSend(lpObj->Index, 0xEB, 0, 0, index, level, 0, skill, luck, option, -1, exc, set, 0, 0, ItemSocketOption, 0xFF, 0);
	}

	gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(107));

	gLog.Output(LOG_COMMAND, "[CommandMake][%s][%s] - (Section: %d, Type: %d, Level: %d, Skill: %d, Luck: %d, Option: %d, Exc: %d, Set: %d, Sockets: %d, [Amount: %d])", lpObj->Account, lpObj->Name, section, type, level, skill, luck, option, exc, set, socket, (qtd > 0) ? qtd : 1);

	return 1;
}

bool CCommandManager::CommandMakeSet(LPOBJ lpObj, char* arg) // OK
{
	int type = this->GetNumber(arg, 0);
	int level = this->GetNumber(arg, 1);
	int skill = this->GetNumber(arg, 2);
	int luck = this->GetNumber(arg, 3);
	int option = this->GetNumber(arg, 4);
	int exc = this->GetNumber(arg, 5);
	int set = this->GetNumber(arg, 6);
	int socket = this->GetNumber(arg, 7);

	BYTE ItemSocketOption[MAX_SOCKET_OPTION] = { 0xFF,0xFF,0xFF,0xFF,0xFF };
	for (int n = 0; n < socket && socket <= MAX_SOCKET_OPTION; n++)
	{
		ItemSocketOption[n] = 0xFE;
	}

	GDCreateItemSend(lpObj->Index, 0xEB, 0, 0, GET_ITEM(7, type), level, 0, skill, luck, option, -1, exc, set, 0, 0, ItemSocketOption, 0xFF, 0);
	GDCreateItemSend(lpObj->Index, 0xEB, 0, 0, GET_ITEM(8, type), level, 0, skill, luck, option, -1, exc, set, 0, 0, ItemSocketOption, 0xFF, 0);
	GDCreateItemSend(lpObj->Index, 0xEB, 0, 0, GET_ITEM(9, type), level, 0, skill, luck, option, -1, exc, set, 0, 0, ItemSocketOption, 0xFF, 0);
	GDCreateItemSend(lpObj->Index, 0xEB, 0, 0, GET_ITEM(10, type), level, 0, skill, luck, option, -1, exc, set, 0, 0, ItemSocketOption, 0xFF, 0);
	GDCreateItemSend(lpObj->Index, 0xEB, 0, 0, GET_ITEM(11, type), level, 0, skill, luck, option, -1, exc, set, 0, 0, ItemSocketOption, 0xFF, 0);

	gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(107));

	gLog.Output(LOG_COMMAND, "[CommandMakeSet][%s][%s] - (Type: %d, Level: %d, Skill: %d, Luck: %d, Option: %d, Exc: %d, Set: %d, Sockets: %d)", lpObj->Account, lpObj->Name, type, level, skill, luck, option, exc, set, socket);

	return 1;
}

bool CCommandManager::CommandDrop(LPOBJ lpObj, char* arg) // OK
{
	int section = this->GetNumber(arg, 0);
	int type = this->GetNumber(arg, 1);
	int level = this->GetNumber(arg, 2);
	int skill = this->GetNumber(arg, 3);
	int luck = this->GetNumber(arg, 4);
	int option = this->GetNumber(arg, 5);
	int exc = this->GetNumber(arg, 6);
	int set = this->GetNumber(arg, 7);
	int socket = this->GetNumber(arg, 8);

	BYTE ItemSocketOption[MAX_SOCKET_OPTION] = { 0xFF,0xFF,0xFF,0xFF,0xFF };
	for (int n = 0; n < socket && socket <= MAX_SOCKET_OPTION; n++)
	{
		ItemSocketOption[n] = 0xFE;
	}


	GDCreateItemSend(lpObj->Index, lpObj->Map, (BYTE)lpObj->X, (BYTE)lpObj->Y, GET_ITEM(section, type), level, 0, skill, luck, option, -1, exc, set, 0, 0, ItemSocketOption, 0xFF, 0);

	gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(107));

	gLog.Output(LOG_COMMAND, "[CommandDrop][%s][%s] - (Section: %d, Type: %d, Level: %d, Skill: %d, Luck: %d, Option: %d, Exc: %d, Set: %d, Sockets: %d)", lpObj->Account, lpObj->Name, section, type, level, skill, luck, option, exc, set, socket);

	return 1;
}

bool CCommandManager::CommandSkin(LPOBJ lpObj, char* arg) // OK
{
	char name[11] = { 0 };

	this->GetString(arg, name, sizeof(name), 0);

	LPOBJ lpTarget = gObjFind(name);

	if (lpTarget == 0)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(100));
		return 0;
	}

	lpTarget->Change = this->GetNumber(arg, 1);

	gObjViewportListProtocolCreate(lpTarget);

	gObjectManager.CharacterUpdateMapEffect(lpTarget);

	gLog.Output(LOG_COMMAND, "[CommandSkin][%s][%s] - (Name: %s, Change: %d)", lpObj->Account, lpObj->Name, name, lpTarget->Change);

	return 1;
}

bool CCommandManager::CommandSetMoney(LPOBJ lpObj, char* arg) // OK
{
	char name[11] = { 0 };

	this->GetString(arg, name, sizeof(name), 0);

	LPOBJ lpTarget = gObjFind(name);

	if (lpTarget == 0)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(100));
		return 0;
	}

	int money = this->GetNumber(arg, 1);

	if (money < 0)
	{
		return 0;
	}

	if (money == 0)
	{
		lpTarget->Money = 0;
	}
	else if (gObjCheckMaxMoney(lpTarget->Index, money) == 0)
	{
		lpTarget->Money = MAX_MONEY;
	}
	else
	{
		lpTarget->Money += money;
	}

	GCMoneySend(lpTarget->Index, lpTarget->Money);

	gLog.Output(LOG_COMMAND, "[CommandSetMoney][%s][%s] - (Name: %s, Money: %d)", lpObj->Account, lpObj->Name, name, money);

	return 1;
}

bool CCommandManager::CommandSetVIP(LPOBJ lpObj, char* arg) // OK
{
	char account[11] = { 0 };

	this->GetString(arg, account, sizeof(account), 0);

	int tipo = this->GetNumber(arg, 1);

	int dias = this->GetNumber(arg, 2);

	if (dias < 0 || tipo < 0)
	{
		return 0;
	}

	//gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,gMessage.GlobalText(519));

	GJAccountLevelSaveSend(lpObj->Index, tipo, (dias * 86400), account);
	GJAccountLevelSend(lpObj->Index, account);


	gLog.Output(LOG_COMMAND, "[CommandSetVIP][%s][%s] - (Account: %s, Tipo: %d, Dias: %d)", lpObj->Account, lpObj->Name, account, tipo, dias);

	return 1;
}

bool CCommandManager::CommandNotice(LPOBJ lpObj, char* arg) // OK
{
	GDGlobalNoticeSend(gMapServerManager.GetMapServerGroup(), 0, 0, 0, 0, 0, 0, arg);

	gLog.Output(LOG_COMMAND, "[CommandNotice][%s][%s] - (Message: %s)", lpObj->Account, lpObj->Name, arg);

	return 1;
}

bool CCommandManager::CommandMasterReset(LPOBJ lpObj, char* arg, int Npc) // OK
{
	if (lpObj->Interface.use != 0 || lpObj->TradeDuel != 0 || lpObj->State == OBJECT_DELCMD || lpObj->DieRegen != 0 || lpObj->Teleport != 0 || lpObj->PShopOpen != 0 || lpObj->SkillSummonPartyTime != 0)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(119));

		if (Npc >= 0)
		{
			GCChatTargetNewSend(lpObj, Npc, gMessage.GlobalText(119));
		}
		return 0;
	}

	int MasterLevel = gMasterResetTable.GetMasterResetLevel((lpObj->MasterReset + 1), lpObj->AccountLevel);

	if (((gMasterSkillTree.CheckMasterLevel(lpObj) == 0) ? lpObj->Level : (lpObj->Level + lpObj->MasterLevel)) < MasterLevel)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(120), MasterLevel);

		if (Npc >= 0)
		{
			GCChatTargetNewSend(lpObj, Npc, gMessage.GlobalText(120), MasterLevel);
		}
		return 0;
	}

	int MasterResetCount = gMasterResetTable.GetMasterResetCount((lpObj->MasterReset + 1), lpObj->AccountLevel);

	if (lpObj->Reset < MasterResetCount)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(121), MasterResetCount);

		if (Npc >= 0)
		{
			GCChatTargetNewSend(lpObj, Npc, gMessage.GlobalText(121), MasterResetCount);
		}
		return 0;
	}

	DWORD ResetMoney = ((DWORD)gMasterResetTable.GetMasterResetMoney((lpObj->MasterReset + 1), lpObj->AccountLevel));

	if (lpObj->Money < ResetMoney)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(122), ResetMoney);

		if (Npc >= 0)
		{
			GCChatTargetNewSend(lpObj, Npc, gMessage.GlobalText(122), ResetMoney);
		}
		return 0;
	}

	BOOL CheckJewel = (BOOL)gMasterResetTable.CheckMasterResetJewel(lpObj, (lpObj->MasterReset + 1), true);

	if (CheckJewel != TRUE)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "does not meet the jewelry conditions to continue");

		if (Npc >= 0)
		{
			GCChatTargetNewSend(lpObj, Npc, "does not meet the jewelry conditions to continue");
		}
		return 0;
	}

	if (lpObj->MasterReset >= gServerInfo.m_CommandMasterResetLimit[lpObj->AccountLevel])
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(123), gServerInfo.m_CommandMasterResetLimit[lpObj->AccountLevel]);

		if (Npc >= 0)
		{
			GCChatTargetNewSend(lpObj, Npc, gMessage.GlobalText(123), gServerInfo.m_CommandMasterResetLimit[lpObj->AccountLevel]);
		}
		return 0;
	}

	if (gServerInfo.m_CommandMasterResetCheckItem[lpObj->AccountLevel] != 0)
	{
		for (int n = 0; n < INVENTORY_WEAR_SIZE; n++)
		{
			if (lpObj->Inventory[n].IsItem() != 0)
			{
				gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(124));

				if (Npc >= 0)
				{
					GCChatTargetNewSend(lpObj, Npc, gMessage.GlobalText(124));
				}
				return 0;
			}
		}
	}

	if (((lpObj->CommandManagerTransaction[0] == 0) ? (lpObj->CommandManagerTransaction[0]++) : lpObj->CommandManagerTransaction[0]) != 0)
	{
		return 0;
	}

	SDHP_COMMAND_MASTER_RESET_SEND pMsg;

	pMsg.header.set(0x0F, 0x01, sizeof(pMsg));

	pMsg.index = lpObj->Index;

	memcpy(pMsg.account, lpObj->Account, sizeof(pMsg.account));

	memcpy(pMsg.name, lpObj->Name, sizeof(pMsg.name));

	gDataServerConnection.DataSend((BYTE*)&pMsg, pMsg.header.size);

	return 0;
}

bool CCommandManager::CommandGuildWar(LPOBJ lpObj, char* arg) // OK
{
	char GuildName[9] = { 0 };

	this->GetString(arg, GuildName, sizeof(GuildName), 0);

	if (strlen(GuildName) >= 1)
	{
		GCGuildWarRequestResult(GuildName, lpObj->Index, 0);
	}

	return 1;
}

bool CCommandManager::CommandBattleSoccer(LPOBJ lpObj, char* arg) // OK
{
	char GuildName[9] = { 0 };

	this->GetString(arg, GuildName, sizeof(GuildName), 0);

	if (strlen(GuildName) >= 1)
	{
		GCGuildWarRequestResult(GuildName, lpObj->Index, 1);
	}

	return 1;
}

bool CCommandManager::CommandRequest(LPOBJ lpObj, char* arg) // OK
{
	char mode[5] = { 0 };

	this->GetString(arg, mode, sizeof(mode), 0);

	if (strcmp(mode, "on") == 0)
	{
		lpObj->Option |= 1;
		lpObj->Option |= 2;
		lpObj->Option &= ~4;

		memset(lpObj->AutoPartyPassword, 0, sizeof(lpObj->AutoPartyPassword));

		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(126));
	}
	else if (strcmp(mode, "off") == 0)
	{
		lpObj->Option &= ~1;
		lpObj->Option &= ~2;
		lpObj->Option &= ~4;

		memset(lpObj->AutoPartyPassword, 0, sizeof(lpObj->AutoPartyPassword));

		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(127));
	}
	else if (strcmp(mode, "auto") == 0)
	{
		lpObj->Option |= 1;
		lpObj->Option |= 2;
		lpObj->Option |= 4;

		memset(lpObj->AutoPartyPassword, 0, sizeof(lpObj->AutoPartyPassword));

		this->GetString(arg, lpObj->AutoPartyPassword, sizeof(lpObj->AutoPartyPassword), 1);

		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(128));
	}
	return 1;
}

bool CCommandManager::CommandHide(LPOBJ lpObj, char* arg) // OK
{
	if (gEffectManager.CheckEffect(lpObj, EFFECT_INVISIBILITY) == 0)
	{
		gEffectManager.AddEffect(lpObj, 0, EFFECT_INVISIBILITY, 0, 0, 0, 0, 0);

		gObjViewportListProtocolDestroy(lpObj);

		gLog.Output(LOG_COMMAND, "[CommandHide][%s][%s] - (State: %d)", lpObj->Account, lpObj->Name, 1);
	}
	else
	{
		gEffectManager.DelEffect(lpObj, EFFECT_INVISIBILITY);

		gObjViewportListProtocolCreate(lpObj);

		gLog.Output(LOG_COMMAND, "[CommandHide][%s][%s] - (State: %d)", lpObj->Account, lpObj->Name, 0);
	}

	return 1;
}

void CCommandManager::CommandAddPointAutoProc(LPOBJ lpObj) // OK
{
	if (lpObj->LevelUpPoint == 0 || lpObj->AutoAddPointCount == 0)
	{
		return;
	}

	int AddStatCount = lpObj->AutoAddPointCount;

	int* stat[5] = { &lpObj->Strength,&lpObj->Dexterity,&lpObj->Vitality,&lpObj->Energy,&lpObj->Leadership };

	for (int n = 0; n < 5; n++)
	{
		if (lpObj->AutoAddPointStats[n] > 0)
		{
			if (AddStatCount == 0)
			{
				break;
			}

			int AddStat = lpObj->LevelUpPoint / AddStatCount;

			AddStat = (((lpObj->LevelUpPoint % AddStatCount) > 0) ? (AddStat + 1) : AddStat);

			AddStat = ((AddStat > lpObj->AutoAddPointStats[n]) ? lpObj->AutoAddPointStats[n] : AddStat);

			AddStat = ((((*stat[n]) + AddStat) > gServerInfo.m_MaxStatPoint[lpObj->AccountLevel]) ? (gServerInfo.m_MaxStatPoint[lpObj->AccountLevel] - (*stat[n])) : AddStat);

			AddStatCount--;

			(*stat[n]) += AddStat;

			lpObj->LevelUpPoint -= AddStat;

			lpObj->AutoAddPointStats[n] -= (((*stat[n]) >= gServerInfo.m_MaxStatPoint[lpObj->AccountLevel]) ? lpObj->AutoAddPointStats[n] : AddStat);

			lpObj->AutoAddPointCount = 0;

			lpObj->AutoAddPointCount = ((lpObj->AutoAddPointStats[0] > 0) ? (lpObj->AutoAddPointCount + 1) : lpObj->AutoAddPointCount);

			lpObj->AutoAddPointCount = ((lpObj->AutoAddPointStats[1] > 0) ? (lpObj->AutoAddPointCount + 1) : lpObj->AutoAddPointCount);

			lpObj->AutoAddPointCount = ((lpObj->AutoAddPointStats[2] > 0) ? (lpObj->AutoAddPointCount + 1) : lpObj->AutoAddPointCount);

			lpObj->AutoAddPointCount = ((lpObj->AutoAddPointStats[3] > 0) ? (lpObj->AutoAddPointCount + 1) : lpObj->AutoAddPointCount);

			lpObj->AutoAddPointCount = ((lpObj->AutoAddPointStats[4] > 0) ? (lpObj->AutoAddPointCount + 1) : lpObj->AutoAddPointCount);
		}
	}

	gObjectManager.CharacterCalcAttribute(lpObj->Index);

	GCNewCharacterInfoSend(lpObj);
}

void CCommandManager::CommandResetAutoProc(LPOBJ lpObj) // OK
{
	if (lpObj->Interface.use != 0 || lpObj->TradeDuel != 0 || lpObj->State == OBJECT_DELCMD || lpObj->DieRegen != 0 || lpObj->Teleport != 0 || lpObj->PShopOpen != 0 || lpObj->SkillSummonPartyTime != 0)
	{
		return;
	}

	int ResetLevel = gResetTable.GetResetLevel(lpObj->Reset + 1, lpObj->AccountLevel);

	if (((gMasterSkillTree.CheckMasterLevel(lpObj) == 0) ? lpObj->Level : (lpObj->Level + lpObj->MasterLevel)) < ResetLevel)
	{
		return;
	}

	DWORD ResetMoney = (DWORD)gResetTable.GetResetMoney(lpObj->Reset + 1, lpObj->AccountLevel);

	if (lpObj->Money < ResetMoney)
	{
		return;
	}

	BOOL CheckJewel = (BOOL)gResetTable.CheckResetJewel(lpObj, lpObj->Reset + 1, false);

	if (CheckJewel != TRUE)
	{
		return;
	}

	if (lpObj->Reset >= gServerInfo.m_CommandResetLimit[lpObj->AccountLevel])
	{
		return;
	}

	COMMAND_LIST CommandData;

	if (gCommand.GetInfo(11, &CommandData) == 1)
	{
		if (CommandData.Enable[lpObj->AccountLevel] == 0)
		{
			return;
		}
	}

	if (gServerInfo.m_CommandResetCheckItem[lpObj->AccountLevel] != 0)
	{
		for (int n = 0; n < INVENTORY_WEAR_SIZE; n++)
		{
			if (lpObj->Inventory[n].IsItem() != 0)
			{
				return;
			}
		}
	}

	if (((lpObj->CommandManagerTransaction[0] == 0) ? (lpObj->CommandManagerTransaction[0]++) : lpObj->CommandManagerTransaction[0]) != 0)
	{
		return;
	}

	SDHP_COMMAND_RESET_SEND pMsg;

	pMsg.header.set(0x0F, 0x00, sizeof(pMsg));

	pMsg.index = lpObj->Index;

	memcpy(pMsg.account, lpObj->Account, sizeof(pMsg.account));

	memcpy(pMsg.name, lpObj->Name, sizeof(pMsg.name));

	gDataServerConnection.DataSend((BYTE*)&pMsg, pMsg.header.size);
}

void CCommandManager::DGCommandResetRecv(SDHP_COMMAND_RESET_RECV* lpMsg) // OK
{
	if (gObjIsAccountValid(lpMsg->index, lpMsg->account) == 0)
	{
		LogAdd(LOG_RED, "[DGCommandResetRecv] Invalid Account [%d](%s)", lpMsg->index, lpMsg->account);
		CloseClient(lpMsg->index);
		return;
	}

	LPOBJ lpObj = &gObj[lpMsg->index];

	lpObj->CommandManagerTransaction[0] = 0;

	if (lpMsg->ResetDay >= ((DWORD)gServerInfo.m_CommandResetLimitDay[lpObj->AccountLevel]))
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(129), gServerInfo.m_CommandResetLimitDay[lpObj->AccountLevel]);
		return;
	}

	if (lpMsg->ResetWek >= ((DWORD)gServerInfo.m_CommandResetLimitWek[lpObj->AccountLevel]))
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(130), gServerInfo.m_CommandResetLimitWek[lpObj->AccountLevel]);
		return;
	}

	if (lpMsg->ResetMon >= ((DWORD)gServerInfo.m_CommandResetLimitMon[lpObj->AccountLevel]))
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(131), gServerInfo.m_CommandResetLimitMon[lpObj->AccountLevel]);
		return;
	}

	DWORD ResetMoney = (DWORD)gResetTable.GetResetMoney(lpObj->Reset + 1, lpObj->AccountLevel);

	lpObj->Money -= ResetMoney;

	gResetTable.GetResetJewel(lpObj, lpObj->Reset + 1);

	//lpObj->Money -= gServerInfo.m_CommandResetMoney[lpObj->AccountLevel];

	GCMoneySend(lpObj->Index, lpObj->Money);

	lpObj->Level = ((gServerInfo.m_CommandResetStartLevel[lpObj->AccountLevel] == -1) ? (lpObj->Level - gServerInfo.m_CommandResetStartLevel[lpObj->AccountLevel]) : gServerInfo.m_CommandResetStartLevel[lpObj->AccountLevel]);

	lpObj->Experience = gLevelExperience[lpObj->Level - 1];

	lpObj->Reset += gBonusManager.GetBonusValue(lpObj, BONUS_INDEX_RESET_AMOUNT, gServerInfo.m_CommandResetCount[lpObj->AccountLevel], -1, -1, -1, -1);

	lpMsg->ResetDay += gBonusManager.GetBonusValue(lpObj, BONUS_INDEX_RESET_AMOUNT, gServerInfo.m_CommandResetCount[lpObj->AccountLevel], -1, -1, -1, -1);

	lpMsg->ResetWek += gBonusManager.GetBonusValue(lpObj, BONUS_INDEX_RESET_AMOUNT, gServerInfo.m_CommandResetCount[lpObj->AccountLevel], -1, -1, -1, -1);

	lpMsg->ResetMon += gBonusManager.GetBonusValue(lpObj, BONUS_INDEX_RESET_AMOUNT, gServerInfo.m_CommandResetCount[lpObj->AccountLevel], -1, -1, -1, -1);

	lpObj->AutoAddPointCount = 0;

	lpObj->AutoAddPointStats[0] = 0;

	lpObj->AutoAddPointStats[1] = 0;

	lpObj->AutoAddPointStats[2] = 0;

	lpObj->AutoAddPointStats[3] = 0;

	lpObj->AutoAddPointStats[4] = 0;

	if (gServerInfo.m_CustomRankUserType == 0)
	{
		gCustomRankUser.CheckUpdate(lpObj);
	}
	else
	{
		gCustomRankUser.GCReqRankLevelUser(lpObj->Index, lpObj->Index);
	}

	if (gServerInfo.m_CommandResetQuest[lpObj->AccountLevel] != 0)
	{
		lpObj->DBClass = ((lpObj->DBClass / 16) * 16);
		lpObj->ChangeUp = lpObj->DBClass % 16;

		gObjectManager.CharacterMakePreviewCharSet(lpObj->Index);

		BYTE Class = (lpObj->ChangeUp * 16);
		Class -= (Class / 32);
		Class += (lpObj->Class * 32);

		gQuest.GCQuestRewardSend(lpObj->Index, 201, Class);

		memset(lpObj->Quest, 0xFF, sizeof(lpObj->Quest));

		gQuest.GCQuestInfoSend(lpObj->Index);
	}

	if (gServerInfo.m_CommandResetSkill[lpObj->AccountLevel] != 0)
	{
		lpObj->MasterLevel = 0;

		lpObj->MasterPoint = 0;

		for (int n = 0; n < MAX_SKILL_LIST; n++)
		{
			lpObj->Skill[n].Clear();
		}

		for (int n = 0; n < MAX_MASTER_SKILL_LIST; n++)
		{
			lpObj->MasterSkill[n].Clear();
		}

		gMasterSkillTree.GCMasterSkillListSend(lpObj->Index);

		gSkillManager.GCSkillListSend(lpObj, 0);

		gObjectManager.CharacterCalcAttribute(lpObj->Index);

		gMasterSkillTree.GCMasterInfoSend(lpObj);
	}

	if (gServerInfo.m_CommandResetType == 1)
	{
		int point = gResetTable.GetResetPoint(lpObj->Reset, lpObj->AccountLevel);

		point = (point * gServerInfo.m_CommandResetPointRate[lpObj->Class]) / 100;

		point += (lpObj->Level - 1) * gServerInfo.m_LevelUpPoint[lpObj->Class];

		point += ((gQuest.CheckQuestListState(lpObj, 2, QUEST_FINISH) == 0) ? 0 : ((lpObj->Level > 220) ? ((lpObj->Level - 220) * gServerInfo.m_PlusStatPoint) : 0));

		point += gQuest.GetQuestRewardLevelUpPoint(lpObj);

		point += lpObj->FruitAddPoint;

		lpObj->LevelUpPoint = point;

		lpObj->Strength = gCharacterManager.m_DefaultClassInfo[lpObj->Class].Strength;
		lpObj->Dexterity = gCharacterManager.m_DefaultClassInfo[lpObj->Class].Dexterity;
		lpObj->Vitality = gCharacterManager.m_DefaultClassInfo[lpObj->Class].Vitality;
		lpObj->Energy = gCharacterManager.m_DefaultClassInfo[lpObj->Class].Energy;
		lpObj->Leadership = gCharacterManager.m_DefaultClassInfo[lpObj->Class].Leadership;
	}

	gResetTable.GetResetPackage(lpObj, lpObj->Reset, lpObj->AccountLevel);

	if (gServerInfo.m_CommandMasterResetType == 1)
	{
		int point = gMasterResetTable.GetMasterResetPoint(lpObj->MasterReset, lpObj->AccountLevel);

		point = (point * gServerInfo.m_CommandMasterResetPointRate[lpObj->Class]) / 100;

		lpObj->LevelUpPoint += point;
	}

	gObjectManager.CharacterCalcAttribute(lpObj->Index);

	GCNewCharacterInfoSend(lpObj);

	GDCharacterInfoSaveSend(lpObj->Index);

	GDResetInfoSaveSend(lpObj->Index, lpMsg->ResetDay, lpMsg->ResetWek, lpMsg->ResetMon);

	gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(95), lpObj->Reset);

	if (gServerInfo.m_CommandResetMove[lpObj->AccountLevel] != 0)
	{
		switch (lpObj->Class)
		{
		case CLASS_DW:
			gObjMoveGate(lpObj->Index, 17);
			break;
		case CLASS_DK:
			gObjMoveGate(lpObj->Index, 17);
			break;
		case CLASS_FE:
			gObjMoveGate(lpObj->Index, 27);
			break;
		case CLASS_MG:
			gObjMoveGate(lpObj->Index, 17);
			break;
		case CLASS_DL:
			gObjMoveGate(lpObj->Index, 17);
			break;
		case CLASS_SU:
			gObjMoveGate(lpObj->Index, 267);
			break;
		case CLASS_RF:
			gObjMoveGate(lpObj->Index, 17);
			break;
		case CLASS_GL:
			gObjMoveGate(lpObj->Index, 17);
			break;
		case CLASS_RW:
			gObjMoveGate(lpObj->Index, 17);
			break;
		case CLASS_SL:
			gObjMoveGate(lpObj->Index, 17);
			break;
		case CLASS_GC:
			gObjMoveGate(lpObj->Index, 17);
			break;
		case CLASS_KUN:
			gObjMoveGate(lpObj->Index, 17);
			break;
		case CLASS_LEM:
			gObjMoveGate(lpObj->Index, 17);
			break;
		case CLASS_KIN:
			gObjMoveGate(lpObj->Index, 17);
			break;
		}
	}

	gResetTable.send_player_info(lpObj, lpObj->Reset + 1, lpObj->AccountLevel);

	gMasterResetTable.send_player_info(lpObj, (lpObj->MasterReset + 1), lpObj->AccountLevel);

	lpObj->AutoAddPointStats[0] = lpObj->AutoResetStats[0];

	lpObj->AutoAddPointStats[1] = lpObj->AutoResetStats[1];

	lpObj->AutoAddPointStats[2] = lpObj->AutoResetStats[2];

	lpObj->AutoAddPointStats[3] = lpObj->AutoResetStats[3];

	lpObj->AutoAddPointStats[4] = lpObj->AutoResetStats[4];

	lpObj->AutoAddPointCount = ((lpObj->AutoAddPointStats[0] > 0) ? (lpObj->AutoAddPointCount + 1) : lpObj->AutoAddPointCount);

	lpObj->AutoAddPointCount = ((lpObj->AutoAddPointStats[1] > 0) ? (lpObj->AutoAddPointCount + 1) : lpObj->AutoAddPointCount);

	lpObj->AutoAddPointCount = ((lpObj->AutoAddPointStats[2] > 0) ? (lpObj->AutoAddPointCount + 1) : lpObj->AutoAddPointCount);

	lpObj->AutoAddPointCount = ((lpObj->AutoAddPointStats[3] > 0) ? (lpObj->AutoAddPointCount + 1) : lpObj->AutoAddPointCount);

	lpObj->AutoAddPointCount = ((lpObj->AutoAddPointStats[4] > 0) ? (lpObj->AutoAddPointCount + 1) : lpObj->AutoAddPointCount);

	this->CommandAddPointAutoProc(lpObj);

	gLog.Output(LOG_COMMAND, "[CommandReset][%s][%s] - (Reset: %d)", lpObj->Account, lpObj->Name, lpObj->Reset);
}

void CCommandManager::DGCommandMasterResetRecv(SDHP_COMMAND_MASTER_RESET_RECV* lpMsg) // OK
{
	if (gObjIsAccountValid(lpMsg->index, lpMsg->account) == 0)
	{
		LogAdd(LOG_RED, "[DGCommandMasterResetRecv] Invalid Account [%d](%s)", lpMsg->index, lpMsg->account);
		CloseClient(lpMsg->index);
		return;
	}

	LPOBJ lpObj = &gObj[lpMsg->index];

	lpObj->CommandManagerTransaction[0] = 0;

	if (lpMsg->MasterResetDay >= ((DWORD)gServerInfo.m_CommandMasterResetLimitDay[lpObj->AccountLevel]))
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(132), gServerInfo.m_CommandMasterResetLimitDay[lpObj->AccountLevel]);
		return;
	}

	if (lpMsg->MasterResetWek >= ((DWORD)gServerInfo.m_CommandMasterResetLimitWek[lpObj->AccountLevel]))
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(133), gServerInfo.m_CommandMasterResetLimitWek[lpObj->AccountLevel]);
		return;
	}

	if (lpMsg->MasterResetMon >= ((DWORD)gServerInfo.m_CommandMasterResetLimitMon[lpObj->AccountLevel]))
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(134), gServerInfo.m_CommandMasterResetLimitMon[lpObj->AccountLevel]);
		return;
	}

	lpObj->Money -= ((DWORD)gMasterResetTable.GetMasterResetMoney((lpObj->MasterReset + 1), lpObj->AccountLevel));

	GCMoneySend(lpObj->Index, lpObj->Money);

	gMasterResetTable.GetMasterResetJewel(lpObj, (lpObj->MasterReset + 1));

	lpObj->Level = ((gServerInfo.m_CommandMasterResetStartLevel[lpObj->AccountLevel] == -1) ? (lpObj->Level - gServerInfo.m_CommandMasterResetStartLevel[lpObj->AccountLevel]) : gServerInfo.m_CommandMasterResetStartLevel[lpObj->AccountLevel]);

	lpObj->Experience = gLevelExperience[lpObj->Level - 1];

	if (gServerInfo.m_CommandMasterResetRemoveReset[lpObj->AccountLevel] > 0)
	{
		lpObj->Reset = (lpObj->Reset - gServerInfo.m_CommandMasterResetRemoveReset[lpObj->AccountLevel]);
	}
	else
	{
		lpObj->Reset = ((gServerInfo.m_CommandMasterResetStartReset[lpObj->AccountLevel] == -1) ? (lpObj->Reset - gServerInfo.m_CommandMasterResetReset[lpObj->AccountLevel]) : gServerInfo.m_CommandMasterResetStartReset[lpObj->AccountLevel]);
	}

	lpObj->MasterReset += gBonusManager.GetBonusValue(lpObj, BONUS_INDEX_MASTER_RESET_AMOUNT, gServerInfo.m_CommandMasterResetCount[lpObj->AccountLevel], -1, -1, -1, -1);

	lpMsg->MasterResetDay += gBonusManager.GetBonusValue(lpObj, BONUS_INDEX_MASTER_RESET_AMOUNT, gServerInfo.m_CommandMasterResetCount[lpObj->AccountLevel], -1, -1, -1, -1);

	lpMsg->MasterResetWek += gBonusManager.GetBonusValue(lpObj, BONUS_INDEX_MASTER_RESET_AMOUNT, gServerInfo.m_CommandMasterResetCount[lpObj->AccountLevel], -1, -1, -1, -1);

	lpMsg->MasterResetMon += gBonusManager.GetBonusValue(lpObj, BONUS_INDEX_MASTER_RESET_AMOUNT, gServerInfo.m_CommandMasterResetCount[lpObj->AccountLevel], -1, -1, -1, -1);

	if (gServerInfo.m_CustomRankUserType == 1)
	{
		gCustomRankUser.CheckUpdate(lpObj);
	}
	else
	{
		gCustomRankUser.GCReqRankLevelUser(lpObj->Index, lpObj->Index);
	}

	if (gServerInfo.m_CommandMasterResetQuest[lpObj->AccountLevel] != 0)
	{
		lpObj->DBClass = ((lpObj->DBClass / 16) * 16);
		lpObj->ChangeUp = lpObj->DBClass % 16;

		gObjectManager.CharacterMakePreviewCharSet(lpObj->Index);

		BYTE Class = (lpObj->ChangeUp * 16);
		Class -= (Class / 32);
		Class += (lpObj->Class * 32);

		gQuest.GCQuestRewardSend(lpObj->Index, 201, Class);

		memset(lpObj->Quest, 0xFF, sizeof(lpObj->Quest));

		gQuest.GCQuestInfoSend(lpObj->Index);
	}

	if (gServerInfo.m_CommandMasterResetSkill[lpObj->AccountLevel] != 0)
	{
		lpObj->MasterLevel = 0;

		lpObj->MasterPoint = 0;

		for (int n = 0; n < MAX_SKILL_LIST; n++)
		{
			lpObj->Skill[n].Clear();
		}

		for (int n = 0; n < MAX_MASTER_SKILL_LIST; n++)
		{
			lpObj->MasterSkill[n].Clear();
		}

		gMasterSkillTree.GCMasterSkillListSend(lpObj->Index);

		gSkillManager.GCSkillListSend(lpObj, 0);

		gObjectManager.CharacterCalcAttribute(lpObj->Index);

		gMasterSkillTree.GCMasterInfoSend(lpObj);
	}

	if (gServerInfo.m_CommandMasterResetType == 1)
	{
		int point = gMasterResetTable.GetMasterResetPoint(lpObj->MasterReset, lpObj->AccountLevel);

		point = (point * gServerInfo.m_CommandMasterResetPointRate[lpObj->Class]) / 100;

		point += (lpObj->Level - 1) * gServerInfo.m_LevelUpPoint[lpObj->Class];

		point += ((gQuest.CheckQuestListState(lpObj, 2, QUEST_FINISH) == 0) ? 0 : ((lpObj->Level > 220) ? ((lpObj->Level - 220) * gServerInfo.m_PlusStatPoint) : 0));

		point += gQuest.GetQuestRewardLevelUpPoint(lpObj);

		point += lpObj->FruitAddPoint;

		lpObj->LevelUpPoint = point;

		lpObj->Strength = gCharacterManager.m_DefaultClassInfo[lpObj->Class].Strength;
		lpObj->Dexterity = gCharacterManager.m_DefaultClassInfo[lpObj->Class].Dexterity;
		lpObj->Vitality = gCharacterManager.m_DefaultClassInfo[lpObj->Class].Vitality;
		lpObj->Energy = gCharacterManager.m_DefaultClassInfo[lpObj->Class].Energy;
		lpObj->Leadership = gCharacterManager.m_DefaultClassInfo[lpObj->Class].Leadership;
	}

	if (gServerInfo.m_CommandResetType == 1)
	{
		int point = gResetTable.GetResetPoint(lpObj->Reset, lpObj->AccountLevel);

		point = (point * gServerInfo.m_CommandResetPointRate[lpObj->Class]) / 100;

		lpObj->LevelUpPoint += point;
	}

	gMasterResetTable.GetMasterResetPackage(lpObj, lpObj->MasterReset, lpObj->AccountLevel);

	gObjectManager.CharacterCalcAttribute(lpObj->Index);

	GCNewCharacterInfoSend(lpObj);

	GDCharacterInfoSaveSend(lpObj->Index);

	GDMasterResetInfoSaveSend(lpObj->Index, lpMsg->MasterResetDay, lpMsg->MasterResetWek, lpMsg->MasterResetMon);

	gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(125), lpObj->MasterReset);

	if (gServerInfo.m_CommandMasterResetMove[lpObj->AccountLevel] != 0)
	{
		switch (lpObj->Class)
		{
		case CLASS_DW:
			gObjMoveGate(lpObj->Index, 17);
			break;
		case CLASS_DK:
			gObjMoveGate(lpObj->Index, 17);
			break;
		case CLASS_FE:
			gObjMoveGate(lpObj->Index, 27);
			break;
		case CLASS_MG:
			gObjMoveGate(lpObj->Index, 17);
			break;
		case CLASS_DL:
			gObjMoveGate(lpObj->Index, 17);
			break;
		case CLASS_SU:
			gObjMoveGate(lpObj->Index, 267);
			break;
		case CLASS_RF:
			gObjMoveGate(lpObj->Index, 17);
			break;
		case CLASS_GL:
			gObjMoveGate(lpObj->Index, 17);
			break;
		case CLASS_RW:
			gObjMoveGate(lpObj->Index, 17);
			break;
		case CLASS_SL:
			gObjMoveGate(lpObj->Index, 17);
			break;
		case CLASS_GC:
			gObjMoveGate(lpObj->Index, 17);
			break;
		case CLASS_KUN:
			gObjMoveGate(lpObj->Index, 17);
			break;
		case CLASS_LEM:
			gObjMoveGate(lpObj->Index, 17);
			break;
		case CLASS_KIN:
			gObjMoveGate(lpObj->Index, 17);
			break;
		}
	}

	gMasterResetTable.send_player_info(lpObj, (lpObj->MasterReset + 1), lpObj->AccountLevel);

	gLog.Output(LOG_COMMAND, "[CommandMasterReset][%s][%s] - (MasterReset: %d)", lpObj->Account, lpObj->Name, lpObj->MasterReset);
}

bool CCommandManager::CommandClearInventory(LPOBJ lpObj, char* arg) // OK
{
	if (lpObj->Lock > 0)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 2, 0, gMessage.GlobalText(778));
		return 0;
	}

	int MaxValue = gItemManager.GetInventoryMaxValue(lpObj);

	for (int i = INVENTORY_WEAR_SIZE; i < MaxValue; i++)
	{
		gItemManager.InventoryDelItem(lpObj->Index, i);
		gItemManager.GCItemDeleteSend(lpObj->Index, i, 1);
	}

	gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(513));

	gLog.Output(LOG_COMMAND, "[Command Clear Iventory] Use for:[%s][%s]", lpObj->Account, lpObj->Name);

	return 1;
}

bool CCommandManager::CommandMarry(LPOBJ lpObj, char* arg) // OK
{
	char mode[11] = { 0 };
	this->GetString(arg, mode, sizeof(mode), 0);


	if (mode[0] == NULL) {
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(606));
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(623), gServerInfo.m_CommandMarryLevel, gServerInfo.m_CommandMarryCost);
		return 0;
	}

	if (strcmp(mode, "to") == 0 && gServerInfo.m_CommandMarryOnlyGM == 0)
	{
		char MarryName[11] = { 0 };
		this->GetString(arg, MarryName, sizeof(MarryName), 1);
		LPOBJ lpTarget = gObjFind(MarryName);
		if (lpTarget == 0)
		{
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(627), MarryName);
			return 0;
		}

		if (strcmp(MarryName, lpObj->Name) == 0)
		{
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(602));
			return 0;
		}

		SDHP_COMMAND_MARRY_SEND pMsg;


		pMsg.header.set(0x0F, 0x02, sizeof(pMsg));


		pMsg.index = lpObj->Index;


		memcpy(pMsg.name, lpObj->Name, sizeof(pMsg.name));


		memcpy(pMsg.marryname, MarryName, sizeof(pMsg.marryname));


		memcpy(pMsg.mode, "marry", sizeof(pMsg.mode));


		gDataServerConnection.DataSend((BYTE*)&pMsg, pMsg.header.size);
	}

	else if (strcmp(mode, "to") == 0 && gServerInfo.m_CommandMarryOnlyGM == 1)
	{

		if (gGameMaster.CheckGameMasterLevel(lpObj, 1) == 0)
		{
			return 0;
		}

		char MarryName1[11] = { 0 };
		char MarryName2[11] = { 0 };
		this->GetString(arg, MarryName1, sizeof(MarryName1), 1);
		this->GetString(arg, MarryName2, sizeof(MarryName2), 2);

		LPOBJ lpTarget1 = gObjFind(MarryName1);
		LPOBJ lpTarget2 = gObjFind(MarryName2);

		if (lpTarget1 == 0)
		{
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(627), MarryName1);
			return 0;
		}

		if (lpTarget2 == 0)
		{
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(627), MarryName2);
			return 0;
		}

		if (strcmp(MarryName1, MarryName2) == 0)
		{
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(602));
			return 0;
		}

		SDHP_COMMAND_MARRY_SEND pMsg;


		pMsg.header.set(0x0F, 0x02, sizeof(pMsg));


		pMsg.index = lpTarget1->Index;


		memcpy(pMsg.name, MarryName1, sizeof(pMsg.name));


		memcpy(pMsg.marryname, MarryName2, sizeof(pMsg.marryname));


		memcpy(pMsg.mode, "marry", sizeof(pMsg.mode));


		gDataServerConnection.DataSend((BYTE*)&pMsg, pMsg.header.size);
	}

	else if (strcmp(mode, "divorce") == 0)
	{
		SDHP_COMMAND_MARRY_SEND pMsg;


		pMsg.header.set(0x0F, 0x02, sizeof(pMsg));


		pMsg.index = lpObj->Index;


		memcpy(pMsg.name, lpObj->Name, sizeof(pMsg.name));


		memcpy(pMsg.marryname, lpObj->Name, sizeof(pMsg.marryname));


		memcpy(pMsg.mode, "divorce", sizeof(pMsg.mode));


		gDataServerConnection.DataSend((BYTE*)&pMsg, pMsg.header.size);
	}


	else if (strcmp(mode, "track") == 0)
	{
		if (lpObj->Money < (DWORD)gServerInfo.m_CommandMarryCost) {
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(620), gServerInfo.m_CommandMarryCost);
			return 0;
		}


		SDHP_COMMAND_MARRY_SEND pMsg;


		pMsg.header.set(0x0F, 0x02, sizeof(pMsg));


		pMsg.index = lpObj->Index;


		memcpy(pMsg.name, lpObj->Name, sizeof(pMsg.name));


		memcpy(pMsg.marryname, lpObj->Name, sizeof(pMsg.marryname));


		memcpy(pMsg.mode, "track", sizeof(pMsg.mode));


		gDataServerConnection.DataSend((BYTE*)&pMsg, pMsg.header.size);
	}


	else if (strcmp(mode, "trace") == 0)
	{
		if (lpObj->Money < (DWORD)gServerInfo.m_CommandMarryCost) {
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(620), gServerInfo.m_CommandMarryCost);
			return 0;
		}


		SDHP_COMMAND_MARRY_SEND pMsg;


		pMsg.header.set(0x0F, 0x02, sizeof(pMsg));


		pMsg.index = lpObj->Index;


		memcpy(pMsg.name, lpObj->Name, sizeof(pMsg.name));


		memcpy(pMsg.marryname, lpObj->Name, sizeof(pMsg.marryname));


		memcpy(pMsg.mode, "trace", sizeof(pMsg.mode));


		gDataServerConnection.DataSend((BYTE*)&pMsg, pMsg.header.size);
	}
	else {
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(606));
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(623), gServerInfo.m_CommandMarryLevel, gServerInfo.m_CommandMarryCost);
		return 0;
	}
	return 0;
}


void CCommandManager::DGCommandMarryRecv(SDHP_COMMAND_MARRY_RECV* lpMsg) // OK
{
	LPOBJ lpObj = &gObj[lpMsg->index];
	LPOBJ lpTarget = gObjFind(lpMsg->marryname);

	if (strcmp(lpMsg->mode, "marry") == 0)
	{
		if (lpMsg->countyou > 0)
		{
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(604));
			return;
		}

		if (lpMsg->counthim > 0)
		{
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(605));
			return;
		}

		int minx = 12;
		int maxx = 15;
		int miny = 23;
		int maxy = 28;
		if (!(lpObj->Map == 2 && lpObj->X >= minx && lpObj->X <= maxx && lpObj->Y >= miny && lpObj->Y <= maxy))
		{
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(621));
			return;
		}

		if (!(lpTarget->Map == 2 && lpTarget->X >= minx && lpTarget->X <= maxx && lpTarget->Y >= miny && lpTarget->Y <= maxy))
		{
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(622));
			return;
		}

		int first_classtype = 0;
		int second_classtype = 0;

		if (
			(lpObj->DBClass >= DB_CLASS_DW && lpObj->DBClass <= DB_CLASS_DW_4) ||
			(lpObj->DBClass >= DB_CLASS_DK && lpObj->DBClass <= DB_CLASS_DK_4) ||
			(lpObj->DBClass >= DB_CLASS_MG && lpObj->DBClass <= DB_CLASS_MG_4) ||
			(lpObj->DBClass >= DB_CLASS_DL && lpObj->DBClass <= DB_CLASS_DL_4) ||
			(lpObj->DBClass >= DB_CLASS_RF && lpObj->DBClass <= DB_CLASS_RF_4) ||
			(lpObj->DBClass >= DB_CLASS_GC && lpObj->DBClass <= DB_CLASS_GC_4) ||
			(lpObj->DBClass >= DB_CLASS_KUM && lpObj->DBClass <= DB_CLASS_KUM_4)
			)
		{
			first_classtype = 0;
		}
		else if (
			(lpObj->DBClass >= DB_CLASS_FE && lpObj->DBClass <= DB_CLASS_FE_4) ||
			(lpObj->DBClass >= DB_CLASS_SU && lpObj->DBClass <= DB_CLASS_SU_4) ||
			(lpObj->DBClass >= DB_CLASS_GL && lpObj->DBClass <= DB_CLASS_GL_4) ||
			(lpObj->DBClass >= DB_CLASS_RW && lpObj->DBClass <= DB_CLASS_RW_4) ||
			(lpObj->DBClass >= DB_CLASS_SL && lpObj->DBClass <= DB_CLASS_SL_4) ||
			(lpObj->DBClass >= DB_CLASS_LEM && lpObj->DBClass <= DB_CLASS_LEM_4) ||
			(lpObj->DBClass >= DB_CLASS_KIN && lpObj->DBClass <= DB_CLASS_KIN_4)
			)
		{
			first_classtype = 1;
		}

		if (
			(lpTarget->DBClass >= DB_CLASS_DW && lpTarget->DBClass <= DB_CLASS_DW_4) ||
			(lpTarget->DBClass >= DB_CLASS_DK && lpTarget->DBClass <= DB_CLASS_DK_4) ||
			(lpTarget->DBClass >= DB_CLASS_MG && lpTarget->DBClass <= DB_CLASS_MG_4) ||
			(lpTarget->DBClass >= DB_CLASS_DL && lpTarget->DBClass <= DB_CLASS_DL_4) ||
			(lpTarget->DBClass >= DB_CLASS_RF && lpTarget->DBClass <= DB_CLASS_RF_4) ||
			(lpTarget->DBClass >= DB_CLASS_GC && lpTarget->DBClass <= DB_CLASS_GC_4) ||
			(lpTarget->DBClass >= DB_CLASS_KUM && lpTarget->DBClass <= DB_CLASS_KUM_4)
			)
		{
			second_classtype = 0;
		}
		else if (
			(lpTarget->DBClass >= DB_CLASS_FE && lpTarget->DBClass <= DB_CLASS_FE_4) ||
			(lpTarget->DBClass >= DB_CLASS_SU && lpTarget->DBClass <= DB_CLASS_SU_4) ||
			(lpTarget->DBClass >= DB_CLASS_GL && lpTarget->DBClass <= DB_CLASS_GL_4) ||
			(lpTarget->DBClass >= DB_CLASS_RW && lpTarget->DBClass <= DB_CLASS_RW_4) ||
			(lpTarget->DBClass >= DB_CLASS_SL && lpTarget->DBClass <= DB_CLASS_SL_4) ||
			(lpTarget->DBClass >= DB_CLASS_LEM && lpTarget->DBClass <= DB_CLASS_LEM_4) ||
			(lpTarget->DBClass >= DB_CLASS_KIN && lpTarget->DBClass <= DB_CLASS_KIN_4)
			)
		{
			second_classtype = 1;
		}

		char fulltext_new[256];
		wsprintf(fulltext_new, gMessage.GlobalText(615), lpMsg->name, lpMsg->marryname);
		if ((first_classtype == 0) && (second_classtype == 0))
		{
			if ((GetTickCount() - lpObj->MarryTimeMsj) > (DWORD)(gServerInfo.m_CommandMarryNoticeDelay * 1000))
			{
				GDGlobalNoticeSend(gMapServerManager.GetMapServerGroup(), 0, 0, 0, 0, 0, 0, gMessage.GlobalText(616));//nohomo allowed
				GDGlobalNoticeSend(gMapServerManager.GetMapServerGroup(), 0, 0, 0, 0, 0, 0, fulltext_new);//nohomo allowed
			}
			lpObj->MarryTimeMsj = GetTickCount();
			return;
		}
		if ((first_classtype == 1) && (second_classtype == 1))
		{
			if ((GetTickCount() - lpObj->MarryTimeMsj) > (DWORD)(gServerInfo.m_CommandMarryNoticeDelay * 1000))
			{
				GDGlobalNoticeSend(gMapServerManager.GetMapServerGroup(), 0, 0, 0, 0, 0, 0, gMessage.GlobalText(616));//nohomo allowed
				GDGlobalNoticeSend(gMapServerManager.GetMapServerGroup(), 0, 0, 0, 0, 0, 0, fulltext_new);//nohomo allowed
			}
			lpObj->MarryTimeMsj = GetTickCount();
			return;
		}
		//
		GDGlobalNoticeSend(gMapServerManager.GetMapServerGroup(), 0, 0, 0, 0, 0, 0, gMessage.GlobalText(614));
		GDMarryInfoSaveSend(lpObj->Index, lpMsg->marryname, lpMsg->mode);
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(603), lpMsg->marryname);
		char fulltext[256];
		wsprintf(fulltext, gMessage.GlobalText(612), lpObj->Name, lpMsg->marryname);
		GDGlobalNoticeSend(gMapServerManager.GetMapServerGroup(), 0, 0, 0, 0, 0, 0, fulltext);
		for (int n = 0; n < 15; n++)
		{
			GCFireworksSend(lpObj, (lpObj->X + (((GetLargeRand() % 5) * 2) - 4)), (lpObj->Y + (((GetLargeRand() % 5) * 2) - 4)));
		}

		this->RemoveRequisites(lpObj, 30);
	}
	else if (strcmp(lpMsg->mode, "divorce") == 0)
	{
		if (lpMsg->countyou == 0)
		{
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(609));
			return;
		}

		time_t timer;
		struct tm y2k = { 0 };
		UINT seconds;
		y2k.tm_hour = 0;   y2k.tm_min = 0; y2k.tm_sec = 0;
		y2k.tm_year = 116; y2k.tm_mon = 0; y2k.tm_mday = 1;

		time(&timer);

		seconds = static_cast<UINT>(difftime(timer, mktime(&y2k)));
		UINT value = seconds - lpMsg->marriedon;

		if (value < (UINT)gServerInfo.m_CommandMarryMinTime)
		{
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(607));
			return;
		}

		LPOBJ lpTarget1 = gObjFind(lpMsg->NameGet1);
		LPOBJ lpTarget2 = gObjFind(lpMsg->NameGet2);

		if (lpTarget1 == 0 && lpTarget2 == 0)
		{
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(624));
			return;
		}
		char fulltext[256];
		GDGlobalNoticeSend(gMapServerManager.GetMapServerGroup(), 0, 0, 0, 0, 0, 0, gMessage.GlobalText(614));

		if (lpTarget1 == 0)
		{
			wsprintf(fulltext, gMessage.GlobalText(613), lpObj->Name, lpMsg->NameGet2);
			GDGlobalNoticeSend(gMapServerManager.GetMapServerGroup(), 0, 0, 0, 0, 0, 0, fulltext);
		}
		if (lpTarget2 == 0)
		{
			wsprintf(fulltext, gMessage.GlobalText(613), lpObj->Name, lpMsg->NameGet1);
			GDGlobalNoticeSend(gMapServerManager.GetMapServerGroup(), 0, 0, 0, 0, 0, 0, fulltext);
		}

		GDMarryInfoSaveSend(lpObj->Index, lpMsg->marryname, lpMsg->mode);
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(608));
	}
	else if (strcmp(lpMsg->mode, "track") == 0)
	{
		if (lpMsg->countyou == 0)
		{
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(609));
			return;
		}
		LPOBJ lpTarget1 = gObjFind(lpMsg->NameGet1);
		LPOBJ lpTarget2 = gObjFind(lpMsg->NameGet2);


		if (lpTarget1 == 0 && lpTarget2 == 0)
		{
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(625));
			return;
		}

		if (lpObj->Level >= gServerInfo.m_CommandMarryLevel)
		{
			if (lpTarget1 == 0)
			{
				gObjTeleport(lpObj->Index, lpTarget2->Map, lpTarget2->X, lpTarget2->Y);
				gObjTeleport(lpTarget2->Index, lpTarget2->Map, lpTarget2->X, lpTarget2->Y);
			}

			if (lpTarget2 == 0)
			{
				gObjTeleport(lpObj->Index, lpTarget1->Map, lpTarget1->X, lpTarget1->Y);
				gObjTeleport(lpTarget1->Index, lpTarget1->Map, lpTarget1->X, lpTarget1->Y);
			}
		}
		else
		{
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(617), gServerInfo.m_CommandMarryLevel);
			return;
		}
		GDMarryInfoSaveSend(lpObj->Index, lpMsg->marryname, lpMsg->mode);
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(610), lpMsg->marryname);
		lpObj->Money -= gServerInfo.m_CommandMarryCost;
		GCMoneySend(lpObj->Index, lpObj->Money);
	}
	else if (strcmp(lpMsg->mode, "trace") == 0)
	{
		if (lpMsg->countyou == 0)
		{
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(609));
			return;
		}

		LPOBJ lpTarget1 = gObjFind(lpMsg->NameGet1);
		LPOBJ lpTarget2 = gObjFind(lpMsg->NameGet2);

		if (lpTarget1 == 0 && lpTarget2 == 0)
		{
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(625));
			return;
		}


		if (lpTarget1 == 0)
		{
			if (lpTarget2->Level >= gServerInfo.m_CommandMarryLevel) {
				gObjTeleport(lpTarget2->Index, lpObj->Map, lpObj->X, lpObj->Y);
				gObjTeleport(lpObj->Index, lpObj->Map, lpObj->X, lpObj->Y);
			}
			else {
				gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(618), gServerInfo.m_CommandMarryLevel);
				return;
			}
		}
		if (lpTarget2 == 0)
		{
			if (lpTarget1->Level >= gServerInfo.m_CommandMarryLevel)
			{
				gObjTeleport(lpTarget1->Index, lpObj->Map, lpObj->X, lpObj->Y);
				gObjViewportListProtocolDestroy(lpObj);
			}
			else
			{
				gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(618), gServerInfo.m_CommandMarryLevel);
				return;
			}
		}
		GDMarryInfoSaveSend(lpObj->Index, lpMsg->marryname, lpMsg->mode);
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(611), lpMsg->marryname);
		lpObj->Money -= gServerInfo.m_CommandMarryCost;
		GCMoneySend(lpObj->Index, lpObj->Money);
	}

	gLog.Output(LOG_COMMAND, "[CommandMarry][%s][%d][%d][%d][%s][%s][%s]", lpObj->Account, lpMsg->countyou, lpMsg->counthim, lpMsg->marriedon, lpMsg->marryname, lpMsg->NameGet1, lpMsg->NameGet2);
}

bool CCommandManager::CommandOpenWarehouse(LPOBJ lpObj) // OK
{
	if (lpObj->Interface.use != 0)
	{
		return 0;
	}

	if (lpObj->TradeDuel != 0)
	{
		return 0;
	}

	if (gServerInfo.m_CommandOpenWareOnlySafeZone == 1 && gMap[lpObj->Map].CheckAttr(lpObj->X, lpObj->Y, 1) == 0)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(631));
		return 0;
	}

	lpObj->Interface.use = 1;
	lpObj->Interface.type = INTERFACE_WAREHOUSE;
	lpObj->Interface.state = 0;

	gWarehouse.GDWarehouseItemSend(lpObj->Index, lpObj->Account);

	return 1;
}

bool CCommandManager::CommandChangeClass(LPOBJ lpObj, char* arg, int Npc) // OK
{
	char classe[4] = { 0 };
	this->GetString(arg, classe, sizeof(classe), 0);

	if (lpObj->Interface.use != 0 || lpObj->TradeDuel != 0 || lpObj->Teleport != 0 || lpObj->DieRegen != 0 || lpObj->PShopOpen != 0)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(659));

		if (Npc >= 0)
		{
			GCChatTargetNewSend(lpObj, Npc, gMessage.GlobalText(659));
		}
		return 0;
	}

	if (classe[0] == NULL)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(634));

		if (Npc >= 0)
		{
			GCChatTargetNewSend(lpObj, Npc, gMessage.GlobalText(634));
		}
		return 0;
	}

	int newclasse = -1;
	int newDBclass = -1;

	if (strcmp(classe, "dw") == 0)
	{
		if (lpObj->Class == CLASS_DW)
		{
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(635));

			if (Npc >= 0)
			{
				GCChatTargetNewSend(lpObj, Npc, gMessage.GlobalText(635));
			}
			return 0;
		}
		if (gServerInfo.m_CommandChangeClassToDW == 0)
		{
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(637));

			if (Npc >= 0)
			{
				GCChatTargetNewSend(lpObj, Npc, gMessage.GlobalText(637));
			}
			return 0;
		}
		newclasse = CLASS_DW;
		newDBclass = DB_CLASS_DW;
	}

	if (strcmp(classe, "dk") == 0)
	{
		if (lpObj->Class == CLASS_DK)
		{
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(635));

			if (Npc >= 0)
			{
				GCChatTargetNewSend(lpObj, Npc, gMessage.GlobalText(635));
			}
			return 0;
		}
		if (gServerInfo.m_CommandChangeClassToDK == 0)
		{
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(637));

			if (Npc >= 0)
			{
				GCChatTargetNewSend(lpObj, Npc, gMessage.GlobalText(637));
			}
			return 0;
		}
		newclasse = CLASS_DK;
		newDBclass = DB_CLASS_DK;
	}

	if (strcmp(classe, "elf") == 0)
	{
		if (lpObj->Class == CLASS_ELF)
		{
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(635));

			if (Npc >= 0)
			{
				GCChatTargetNewSend(lpObj, Npc, gMessage.GlobalText(635));
			}
			return 0;
		}
		if (gServerInfo.m_CommandChangeClassToELF == 0)
		{
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(637));

			if (Npc >= 0)
			{
				GCChatTargetNewSend(lpObj, Npc, gMessage.GlobalText(637));
			}
			return 0;
		}
		newclasse = CLASS_ELF;
		newDBclass = DB_CLASS_FE;
	}

	if (strcmp(classe, "mg") == 0)
	{
		if (lpObj->Class == CLASS_MG)
		{
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(635));

			if (Npc >= 0)
			{
				GCChatTargetNewSend(lpObj, Npc, gMessage.GlobalText(635));
			}
			return 0;
		}
		if (gServerInfo.m_CommandChangeClassToMG == 0)
		{
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(637));

			if (Npc >= 0)
			{
				GCChatTargetNewSend(lpObj, Npc, gMessage.GlobalText(637));
			}
			return 0;
		}
		newclasse = CLASS_MG;
		newDBclass = DB_CLASS_MG;
	}

	if (strcmp(classe, "dl") == 0)
	{
		if (lpObj->Class == CLASS_DL)
		{
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(635));

			if (Npc >= 0)
			{
				GCChatTargetNewSend(lpObj, Npc, gMessage.GlobalText(635));
			}
			return 0;
		}
		if (gServerInfo.m_CommandChangeClassToDL == 0)
		{
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(637));

			if (Npc >= 0)
			{
				GCChatTargetNewSend(lpObj, Npc, gMessage.GlobalText(637));
			}
			return 0;
		}
		newclasse = CLASS_DL;
		newDBclass = DB_CLASS_DL;
	}

	if (strcmp(classe, "su") == 0)
	{
		if (lpObj->Class == CLASS_SU)
		{
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(635));

			if (Npc >= 0)
			{
				GCChatTargetNewSend(lpObj, Npc, gMessage.GlobalText(635));
			}
			return 0;
		}
		if (gServerInfo.m_CommandChangeClassToSU == 0)
		{
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(637));

			if (Npc >= 0)
			{
				GCChatTargetNewSend(lpObj, Npc, gMessage.GlobalText(637));
			}
			return 0;
		}
		newclasse = CLASS_SU;
		newDBclass = DB_CLASS_SU;
	}

#if(GAMESERVER_UPDATE>=601)
	if (strcmp(classe, "rf") == 0)
	{
		if (lpObj->Class == CLASS_RF)
		{
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(635));

			if (Npc >= 0)
			{
				GCChatTargetNewSend(lpObj, Npc, gMessage.GlobalText(635));
			}
			return 0;
		}
		if (gServerInfo.m_CommandChangeClassToRF == 0)
		{
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(637));

			if (Npc >= 0)
			{
				GCChatTargetNewSend(lpObj, Npc, gMessage.GlobalText(637));
			}
			return 0;
		}
		newclasse = CLASS_RF;
		newDBclass = DB_CLASS_RF;
	}
#endif

	if (newclasse < 0)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(634));

		if (Npc >= 0)
		{
			GCChatTargetNewSend(lpObj, Npc, gMessage.GlobalText(634));
		}
		return 0;
	}

	
	int _inv[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 237, 238, 239, 240, 241, 242, -1};

	for (int i = 0; _inv[i] != -1; i++)
	{
		int n = _inv[i];

		if (lpObj->Inventory[n].IsItem() != 0)
		{
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(636));

			if (Npc >= 0)
			{
				GCChatTargetNewSend(lpObj, Npc, gMessage.GlobalText(636));
			}
			return 0;
		}
	}

	if (lpObj->Interface.use != 0)
	{
		return 0;
	}

	if (lpObj->TradeDuel != 0)
	{
		return 0;
	}

	lpObj->Class = newclasse;
	lpObj->DBClass = newDBclass;
	lpObj->ChangeUp = lpObj->DBClass % 16;

	gObjectManager.CharacterMakePreviewCharSet(lpObj->Index);

	BYTE Class = (lpObj->ChangeUp * 16);
	Class -= (Class / 32);
	Class += (lpObj->Class * 32);

	gQuest.GCQuestRewardSend(lpObj->Index, 201, Class);
	memset(lpObj->Quest, 0xFF, sizeof(lpObj->Quest));
	gQuest.GCQuestInfoSend(lpObj->Index);

	lpObj->MasterLevel = 0;
	lpObj->MasterPoint = 0;
	lpObj->MasterExperience = 0;

	for (int n = 0; n < MAX_SKILL_LIST; n++)
	{
		lpObj->Skill[n].Clear();
	}

	for (int n = 0; n < MAX_MASTER_SKILL_LIST; n++)
	{
		lpObj->MasterSkill[n].Clear();
	}

	gEffectManager.ClearAllEffect(lpObj);

	lpObj->CloseCount = 1;
	lpObj->CloseType = 1;

	gLog.Output(LOG_COMMAND, "[CommandChangeClass][%s][%s] - (ClassNum: %d)", lpObj->Account, lpObj->Name, newDBclass);

	return 1;
}

bool CCommandManager::CommandReward(LPOBJ lpObj, char* arg) // OK
{
	char name[11] = { 0 };

	this->GetString(arg, name, sizeof(name), 0);

	if (name[0] == NULL) {
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(643));
		return 0;
	}

	LPOBJ lpTarget = gObjFind(name);

	if (lpTarget == 0)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(642));
		return 0;
	}

	if (strcmp(lpTarget->Name, lpObj->Name) == 0)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(647));
		return 0;
	}

	int type = this->GetNumber(arg, 1);
	int value = this->GetNumber(arg, 2);

	if (type <= 0 || value <= 0)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(643));
		return 0;
	}

	if (type > gServerInfo.m_CommandRewardMaxType)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(644));
		return 0;
	}

	if (value > gServerInfo.m_CommandRewardMaxValue)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(645));
		return 0;
	}


	SDHP_COMMAND_REWARD_SEND pMsg;

	pMsg.header.set(0x0F, 0x03, sizeof(pMsg));

	pMsg.index = lpObj->Index;

	memcpy(pMsg.name, lpTarget->Name, sizeof(pMsg.name));
	memcpy(pMsg.account, lpTarget->Account, sizeof(pMsg.account));

	memcpy(pMsg.nameGM, lpObj->Name, sizeof(pMsg.nameGM));
	memcpy(pMsg.accountGM, lpObj->Account, sizeof(pMsg.accountGM));

	pMsg.Type = type;
	pMsg.Value = value;

	gDataServerConnection.DataSend((BYTE*)&pMsg, pMsg.header.size);

	if (type == 1) gNotice.GCNoticeSendToAll(0, 0, 0, 0, 0, 0, gServerInfo.m_CommandRewardType1Text, lpObj->Name, lpTarget->Name, value);
	if (type == 2) gNotice.GCNoticeSendToAll(0, 0, 0, 0, 0, 0, gServerInfo.m_CommandRewardType2Text, lpObj->Name, lpTarget->Name, value);
	if (type == 3) gNotice.GCNoticeSendToAll(0, 0, 0, 0, 0, 0, gServerInfo.m_CommandRewardType3Text, lpObj->Name, lpTarget->Name, value);
	if (type == 4) gNotice.GCNoticeSendToAll(0, 0, 0, 0, 0, 0, gServerInfo.m_CommandRewardType4Text, lpObj->Name, lpTarget->Name, value);
	if (type == 5) gNotice.GCNoticeSendToAll(0, 0, 0, 0, 0, 0, gServerInfo.m_CommandRewardType5Text, lpObj->Name, lpTarget->Name, value);
	if (type == 6) gNotice.GCNoticeSendToAll(0, 0, 0, 0, 0, 0, gServerInfo.m_CommandRewardType6Text, lpObj->Name, lpTarget->Name, value);
	if (type == 7) gNotice.GCNoticeSendToAll(0, 0, 0, 0, 0, 0, gServerInfo.m_CommandRewardType7Text, lpObj->Name, lpTarget->Name, value);
	if (type == 8) gNotice.GCNoticeSendToAll(0, 0, 0, 0, 0, 0, gServerInfo.m_CommandRewardType8Text, lpObj->Name, lpTarget->Name, value);
	if (type == 9) gNotice.GCNoticeSendToAll(0, 0, 0, 0, 0, 0, gServerInfo.m_CommandRewardType9Text, lpObj->Name, lpTarget->Name, value);
	if (type == 10) gNotice.GCNoticeSendToAll(0, 0, 0, 0, 0, 0, gServerInfo.m_CommandRewardType10Text, lpObj->Name, lpTarget->Name, value);

#if(GAMESERVER_UPDATE<=402) 
	gPcPoint.GDPcPointPointSend(lpTarget->Index);
#endif
	gCashShop.CGCashShopPointRecv(lpTarget->Index);


	gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(646));
	gLog.Output(LOG_COMMAND, "[CommandReward][%s][%s] - (Player: %s, Type: %d, Value: %d)", lpObj->Account, lpObj->Name, lpTarget->Name, type, value);

	return 1;
}

bool CCommandManager::CommandRewardAll(LPOBJ lpObj, char* arg) // OK
{
	int type = this->GetNumber(arg, 0);
	int value = this->GetNumber(arg, 1);

	if (type <= 0 || value <= 0)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(650));
		return 0;
	}

	if (type > gServerInfo.m_CommandRewardAllMaxType)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(644));
		return 0;
	}

	if (value > gServerInfo.m_CommandRewardAllMaxValue)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(645));
		return 0;
	}

	SDHP_COMMAND_REWARDALL_SEND pMsg;

	pMsg.header.set(0x0F, 0x04, sizeof(pMsg));

	pMsg.index = lpObj->Index;

	memcpy(pMsg.nameGM, lpObj->Name, sizeof(pMsg.nameGM));
	memcpy(pMsg.accountGM, lpObj->Account, sizeof(pMsg.accountGM));

	pMsg.Type = type;
	pMsg.Value = value;

	gDataServerConnection.DataSend((BYTE*)&pMsg, pMsg.header.size);

	if (type == 1) gNotice.GCNoticeSendToAll(0, 0, 0, 0, 0, 0, gServerInfo.m_CommandRewardAllType1Text, lpObj->Name, value);
	if (type == 2) gNotice.GCNoticeSendToAll(0, 0, 0, 0, 0, 0, gServerInfo.m_CommandRewardAllType2Text, lpObj->Name, value);
	if (type == 3) gNotice.GCNoticeSendToAll(0, 0, 0, 0, 0, 0, gServerInfo.m_CommandRewardAllType3Text, lpObj->Name, value);
	if (type == 4) gNotice.GCNoticeSendToAll(0, 0, 0, 0, 0, 0, gServerInfo.m_CommandRewardAllType4Text, lpObj->Name, value);
	if (type == 5) gNotice.GCNoticeSendToAll(0, 0, 0, 0, 0, 0, gServerInfo.m_CommandRewardAllType5Text, lpObj->Name, value);
	if (type == 6) gNotice.GCNoticeSendToAll(0, 0, 0, 0, 0, 0, gServerInfo.m_CommandRewardAllType6Text, lpObj->Name, value);
	if (type == 7) gNotice.GCNoticeSendToAll(0, 0, 0, 0, 0, 0, gServerInfo.m_CommandRewardAllType7Text, lpObj->Name, value);
	if (type == 8) gNotice.GCNoticeSendToAll(0, 0, 0, 0, 0, 0, gServerInfo.m_CommandRewardAllType8Text, lpObj->Name, value);
	if (type == 9) gNotice.GCNoticeSendToAll(0, 0, 0, 0, 0, 0, gServerInfo.m_CommandRewardAllType9Text, lpObj->Name, value);
	if (type == 10) gNotice.GCNoticeSendToAll(0, 0, 0, 0, 0, 0, gServerInfo.m_CommandRewardAllType10Text, lpObj->Name, value);

	gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(646));
	gLog.Output(LOG_COMMAND, "[CommandRewardAll][%s][%s] - (Type: %d, Value: %d)", lpObj->Account, lpObj->Name, type, value);

	return 1;
}

bool CCommandManager::CommandReload(LPOBJ lpObj, char* arg) // OK
{

	char name[20] = { 0 };

	this->GetString(arg, name, sizeof(name), 0);

	if (strcmp(name, "cashshop") == 0)
	{
		gServerInfo.ReadCashShopInfo();
	}
	else if (strcmp(name, "chaosmix") == 0)
	{
		gServerInfo.ReadChaosMixInfo();
	}
	else if (strcmp(name, "character") == 0)
	{
		gServerInfo.ReadCharacterInfo();
	}
	else if (strcmp(name, "command") == 0)
	{
		gServerInfo.ReadCommandInfo();
	}
	else if (strcmp(name, "common") == 0)
	{
		gServerInfo.ReadCommonInfo();
	}
	else if (strcmp(name, "custom") == 0)
	{
		gServerInfo.ReadCustomInfo();
	}
	else if (strcmp(name, "event") == 0)
	{
		gServerInfo.ReadEventInfo();
	}
	else if (strcmp(name, "eventitembag") == 0)
	{
		gServerInfo.ReadEventItemBagInfo();
	}
	else if (strcmp(name, "hack") == 0)
	{
		gServerInfo.ReadHackInfo();
	}
	else if (strcmp(name, "item") == 0)
	{
		gServerInfo.ReloadItemTextInfo();
	}
	else if (strcmp(name, "monster") == 0)
	{
		gServerInfo.ReloadMonsterInfo();
	}
	else if (strcmp(name, "move") == 0)
	{
		gServerInfo.ReadMoveInfo();
	}
	else if (strcmp(name, "quest") == 0)
	{
		gServerInfo.ReadQuestInfo();
	}
	else if (strcmp(name, "shop") == 0)
	{
		gServerInfo.ReadShopInfo();
	}
	else if (strcmp(name, "skill") == 0)
	{
		gServerInfo.ReadSkillInfo();
	}
	else if (strcmp(name, "util") == 0)
	{
		gServerInfo.ReadUtilInfo();
	}
	else if (strcmp(name, "bots") == 0)
	{
		gServerInfo.ReloadBotInfo();
	}
	else if (strcmp(name, "all") == 0)
	{
		gServerInfo.ReloadAll();
	}
	else
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(648));
		return 0;
	}

	gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(649));
	return 1;
}

bool CCommandManager::CommandRename(LPOBJ lpObj, char* arg) // OK
{
#if (GAMESERVER_CLIENTE_UPDATE >= 2)

	if (lpObj->Interface.use != 0 || lpObj->TradeDuel != 0 || lpObj->Teleport != 0 || lpObj->DieRegen != 0 || lpObj->PShopOpen != 0)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(659));
		return 0;
	}

	if (lpObj->GuildNumber > 0) //Verifica se o personagem faz parte de uma guild
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(658));
		return 0;
	}

	if (gServerInfo.m_CommandRenameTicket == 1 && lpObj->RenameEnable == 0)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(633));
		return 0;
	}

	DWORD tick = (GetTickCount() - lpObj->RenameTime) / 1000;

	if (tick < ((DWORD)gServerInfo.m_CommandRenameDelay[lpObj->AccountLevel]))
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(653), (gServerInfo.m_CommandRenameDelay[lpObj->AccountLevel] - tick));
		return 0;
	}

	if (strlen(arg) > 10)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(651));
		return 0;
	}

	if (gFilterRename.CheckFilter(arg) == 1)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(817));
		return 0;
	}

	char RenameName[11] = { 0 };
	this->GetString(arg, RenameName, sizeof(RenameName), 0);

	if (RenameName[0] == NULL)
	{
		return 0;
	}

	if (strcmp(RenameName, lpObj->Name) == 0)
	{
		return 0;
	}

	lpObj->RenameActive = 1;

	SDHP_COMMAND_RENAME_SEND pMsg;

	pMsg.header.set(0x0F, 0x05, sizeof(pMsg));

	pMsg.index = lpObj->Index;

	memcpy(pMsg.account, lpObj->Account, sizeof(pMsg.account));

	memcpy(pMsg.name, lpObj->Name, sizeof(pMsg.name));

	memcpy(pMsg.newname, RenameName, sizeof(pMsg.newname));

	gDataServerConnection.DataSend((BYTE*)&pMsg, pMsg.header.size);

	gLog.Output(LOG_COMMAND, "[CommandRename][%s][%s] - (NewName: %s)", lpObj->Account, lpObj->Name, RenameName);

#endif
	return 0;
}

void CCommandManager::DGCommandRenameRecv(SDHP_COMMAND_RENAME_RECV* lpMsg) // OK
{

#if (GAMESERVER_CLIENTE_UPDATE >= 2)
	if (gObjIsAccountValid(lpMsg->index, lpMsg->account) == 0)
	{
		LogAdd(LOG_RED, "[DGCommandRenameRecv] Invalid Account [%d](%s)", lpMsg->index, lpMsg->account);
		CloseClient(lpMsg->index);
		return;
	}

	LPOBJ lpObj = &gObj[lpMsg->index];

	if (lpMsg->result == 0)
	{

		this->RemoveRequisites(lpObj, 38);

		lpObj->RenameTime = GetTickCount();

		memcpy(lpObj->Name, lpMsg->newname, sizeof(lpObj->Name));

		gNotice.GCNoticeSendToAll(0, 0, 0, 0, 0, 0, gMessage.GlobalText(657), lpMsg->name, lpMsg->newname);

		lpObj->RenameActive = 0;
		lpObj->CloseCount = 1;
		lpObj->CloseType = 1;

		LogAdd(LOG_GREEN, "[CommandRename][%s][%s] - Sucess (OldName: %s)", lpObj->Account, lpObj->Name, lpMsg->name);
		return;
	}

	if (lpMsg->result == 1)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(654));
		LogAdd(LOG_BLACK, "[CommandRename][%s][%s] - Name exists (NewName: %s)", lpObj->Account, lpObj->Name, lpMsg->newname);
		return;
	}

	if (lpMsg->result == 2)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(656));
		LogAdd(LOG_RED, "[CommandRename][%s][%s] - Error (NewName: %s)", lpObj->Account, lpObj->Name, lpMsg->newname);
		return;
	}

#endif


}
bool CCommandManager::CommandInfo(LPOBJ lpObj, char* arg) // OK
{
#if (GAMESERVER_CLIENTE_UPDATE >= 2) 

	char name[11] = { 0 };

	this->GetString(arg, name, sizeof(name), 0);

	LPOBJ lpTarget = gObjFind(name);

	if (lpTarget == 0)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(100));
		return 0;
	}

	if (gGameMaster.CheckGameMasterLevel(lpObj, 1) == 1)
	{
		gNotice.GCNoticeSend(lpObj->Index, 0, 0, 0, 0, 0, 0, "Acc: %s", lpTarget->Account);
	}

	GJAccountLevelSend(lpObj->Index, lpTarget->Account);
	gNotice.GCNoticeSend(lpObj->Index, 0, 0, 0, 0, 0, 0, "Level: %d", lpTarget->Level);
	gNotice.GCNoticeSend(lpObj->Index, 0, 0, 0, 0, 0, 0, "Resets: %d", lpTarget->Reset);
	gNotice.GCNoticeSend(lpObj->Index, 0, 0, 0, 0, 0, 0, "MResets: %d", lpTarget->MasterReset);
#endif
	return 1;
}

bool CCommandManager::CommandBlocChar(LPOBJ lpObj, char* arg) // OK
{
#if (GAMESERVER_CLIENTE_UPDATE >= 2 )

	char name[11] = { 0 };

	this->GetString(arg, name, sizeof(name), 0);

	int days = this->GetNumber(arg, 1);

	if (days <= 0)
	{
		return 0;
	}

	LPOBJ lpTarget = gObjFind(name);

	if (lpTarget > 0)
	{
		gObjUserKill(lpTarget->Index);
	}

	SDHP_COMMAND_BLOC_SEND pMsg;

	pMsg.header.set(0x0F, 0x07, sizeof(pMsg));

	pMsg.index = lpObj->Index;

	pMsg.days = days;

	memcpy(pMsg.namebloc, name, sizeof(pMsg.namebloc));

	gDataServerConnection.DataSend((BYTE*)&pMsg, pMsg.header.size);

	if (gServerInfo.m_CommandBlocCharShowMessage == 1)
	{
		gNotice.GCNoticeSendToAll(0, 0, 0, 0, 0, 0, gMessage.GlobalText(841), lpObj->Name, name, days);
	}

	gLog.Output(LOG_COMMAND, "[CommandBlocChar][%s][%s] - (Name: %s)", lpObj->Account, lpObj->Name, name);

#endif
	return 1;
}

bool CCommandManager::CommandBlocCharRecv(SDHP_COMMAND_BLOC_RECV* lpMsg) // OK
{
#if (GAMESERVER_CLIENTE_UPDATE >= 11 )

	if (lpMsg->result != 1)
	{
		gNotice.GCNoticeSend(lpMsg->index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(667));
	}
	else
	{
		gNotice.GCNoticeSend(lpMsg->index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(669));
	}

#endif
	return 1;
}

bool CCommandManager::CommandBlocAcc(LPOBJ lpObj, char* arg) // OK
{
#if (GAMESERVER_CLIENTE_UPDATE >= 2)

	char name[11] = { 0 };

	this->GetString(arg, name, sizeof(name), 0);

	int days = this->GetNumber(arg, 1);

	if (days <= 0)
	{
		return 0;
	}

	LPOBJ lpTarget = gObjFind(name);

	if (lpTarget > 0)
	{
		gObjUserKill(lpTarget->Index);
	}

	SDHP_COMMAND_BLOC_SEND pMsg;

	pMsg.header.set(0x0F, 0x06, sizeof(pMsg));

	pMsg.index = lpObj->Index;

	pMsg.days = days;

	memcpy(pMsg.namebloc, name, sizeof(pMsg.namebloc));

	gDataServerConnection.DataSend((BYTE*)&pMsg, pMsg.header.size);

	if (gServerInfo.m_CommandBlocAccShowMessage == 1)
	{
		gNotice.GCNoticeSendToAll(0, 0, 0, 0, 0, 0, gMessage.GlobalText(842), lpObj->Name, name, days);
	}

	gLog.Output(LOG_COMMAND, "[CommandBlocAcc][%s][%s] - (Acc: %s)", lpObj->Account, lpObj->Name, name);

#endif

	return 1;
}

bool CCommandManager::CommandBlocAccRecv(SDHP_COMMAND_BLOC_RECV* lpMsg) // OK
{
#if (GAMESERVER_CLIENTE_UPDATE >= 11 )

	if (lpMsg->result != 1)
	{
		gNotice.GCNoticeSend(lpMsg->index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(667));
	}
	else
	{
		gNotice.GCNoticeSend(lpMsg->index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(668));
	}

#endif
	return 1;
}

bool CCommandManager::CommandGift(LPOBJ lpObj, char* arg)
{
	SDHP_GIFT_SEND pMsg;

	pMsg.header.set(0x0F, 0x08, sizeof(pMsg));

	pMsg.index = lpObj->Index;

	memcpy(pMsg.account, lpObj->Account, sizeof(pMsg.account));

	gDataServerConnection.DataSend((BYTE*)&pMsg, pMsg.header.size);

	return 0;
}

void CCommandManager::DGCommandGiftRecv(SDHP_GIFT_RECV* lpMsg)
{

#if (GAMESERVER_CLIENTE_UPDATE >= 3)

	LPOBJ lpObj = &gObj[lpMsg->index];

	if (lpMsg->result > 0)
	{
		if (lpMsg->result > gServerInfo.m_CommandGiftLimit)
		{
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(703));
		}
		else
		{
			gItemBagManager.DropItemBySpecialValue(ITEM_BAG_GIFT, lpObj, lpObj->Map, lpObj->X, lpObj->Y);

			GCFireworksSend(lpObj, lpObj->X, lpObj->Y);

			LogAdd(LOG_EVENT, "[CustomGift][%s][%s] GiftNumber: %d", lpObj->Account, lpObj->Name, lpMsg->result);
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(702));

			this->RemoveRequisites(lpObj, 49);
		}

	}
	else
	{
		LogAdd(LOG_RED, "[CustomGift][%s][%s] Gift Error", lpObj->Account, lpObj->Name);
	}

#endif
}

bool CCommandManager::CommandTop(LPOBJ lpObj, char* arg)
{
	SDHP_TOP_SEND pMsg;

	pMsg.header.set(0x0F, 0x09, sizeof(pMsg));

	pMsg.index = lpObj->Index;

	char tipo[10] = { 0 };
	char classe[3] = { 0 };

	this->GetString(arg, tipo, sizeof(tipo), 0);
	this->GetString(arg, classe, sizeof(classe), 1);

	if (strcmp(classe, "dw") == 0)
	{
		pMsg.classe = 0;
	}
	else if (strcmp(classe, "dk") == 0)
	{
		pMsg.classe = 16;
	}
	else if (strcmp(classe, "fe") == 0)
	{
		pMsg.classe = 32;
	}
	else if (strcmp(classe, "mg") == 0)
	{
		pMsg.classe = 48;
	}
	else if (strcmp(classe, "dl") == 0)
	{
		pMsg.classe = 64;
	}
	else if (strcmp(classe, "su") == 0)
	{
		pMsg.classe = 80;
	}
	else if (strcmp(classe, "rf") == 0)
	{
		pMsg.classe = 96;
	}
	else
	{
		pMsg.classe = 999;
	}

	CUSTOMTOP_INFO CustomTopInfo;

	if (gCustomTop.GetInfoByName(tipo, &CustomTopInfo) != 0)
	{
		pMsg.type = CustomTopInfo.Index;
	}
	else
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(733));
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(734));
		return 0;
	}

	gDataServerConnection.DataSend((BYTE*)&pMsg, pMsg.header.size);

	return 1;
}

void CCommandManager::DGCommandTopRecv(SDHP_TOP_RECV* lpMsg)
{
#if (GAMESERVER_CLIENTE_UPDATE >= 3)

	LPOBJ lpObj = &gObj[lpMsg->index];

	CUSTOMTOP_INFO CustomTopInfo;

	if (gCustomTop.GetInfo(lpMsg->type, &CustomTopInfo) != 0)
	{
		if (lpMsg->classe == 0)
		{
			gNotice.GCNoticeSend(lpObj->Index, 0, 0, 0, 0, 0, 0, "%s (DW)", CustomTopInfo.Title);
		}
		else if (lpMsg->classe == 16)
		{
			gNotice.GCNoticeSend(lpObj->Index, 0, 0, 0, 0, 0, 0, "%s (DK)", CustomTopInfo.Title);
		}
		else if (lpMsg->classe == 32)
		{
			gNotice.GCNoticeSend(lpObj->Index, 0, 0, 0, 0, 0, 0, "%s (FE)", CustomTopInfo.Title);
		}
		else if (lpMsg->classe == 48)
		{
			gNotice.GCNoticeSend(lpObj->Index, 0, 0, 0, 0, 0, 0, "%s (MG)", CustomTopInfo.Title);
		}
		else if (lpMsg->classe == 64)
		{
			gNotice.GCNoticeSend(lpObj->Index, 0, 0, 0, 0, 0, 0, "%s (DL)", CustomTopInfo.Title);
		}
		else if (lpMsg->classe == 80)
		{
			gNotice.GCNoticeSend(lpObj->Index, 0, 0, 0, 0, 0, 0, "%s (SU)", CustomTopInfo.Title);
		}
		else if (lpMsg->classe == 96)
		{
			gNotice.GCNoticeSend(lpObj->Index, 0, 0, 0, 0, 0, 0, "%s (RF)", CustomTopInfo.Title);
		}
		else
		{
			gNotice.GCNoticeSend(lpObj->Index, 0, 0, 0, 0, 0, 0, "%s", CustomTopInfo.Title);
		}
	}
	else
	{
		return;
	}

	if (lpMsg->count <= 0)
	{
		gNotice.GCNoticeSend(lpObj->Index, 0, 0, 0, 0, 0, 0, gMessage.GlobalText(735));
		return;
	}

	for (int n = 0; n < lpMsg->count; n++)
	{
		SDHP_TOP_INFO* lpInfo = (SDHP_TOP_INFO*)(((BYTE*)lpMsg) + sizeof(SDHP_TOP_RECV) + (sizeof(SDHP_TOP_INFO) * n));

		gNotice.GCNoticeSend(lpObj->Index, 0, 0, 0, 0, 0, 0, gMessage.GlobalText(736), n + 1, lpInfo->name, lpInfo->value);

	}

#endif
}

bool CCommandManager::CommandReAdd(LPOBJ lpObj, char* arg, int Npc)
{
#if (GAMESERVER_CLIENTE_UPDATE >= 4)
	if (gBuildProfile.HasAny(lpObj))
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "ReAdd bloqueado: apague suas builds salvas primeiro.");
		return 0;
	}

	if (lpObj->Interface.use != 0 || lpObj->Teleport != 0 || lpObj->DieRegen != 0 || lpObj->PShopOpen != 0)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(659));

		if (Npc >= 0)
		{
			GCChatTargetNewSend(lpObj, Npc, gMessage.GlobalText(659));
		}
		return 0;
	}

	lpObj->LevelUpPoint += (lpObj->Strength + lpObj->Dexterity + lpObj->Vitality + lpObj->Energy + lpObj->Leadership - 60);

	lpObj->Strength = 15;
	lpObj->Dexterity = 15;
	lpObj->Vitality = 15;
	lpObj->Energy = 15;
	lpObj->Leadership = 0;

	gObjectManager.CharacterCalcAttribute(lpObj->Index);

	GCNewCharacterInfoSend(lpObj);

	GDCharacterInfoSaveSend(lpObj->Index);

	gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(737));

	gLog.Output(LOG_COMMAND, "[CommandReAdd][%s][%s] - OK", lpObj->Account, lpObj->Name);
#endif
	return 1;
}

bool CCommandManager::CommandHelper(LPOBJ lpObj, char* arg, int Npc)
{
#if (GAMESERVER_CLIENTE_UPDATE >= 6)

	if (lpObj->Map == MAP_ICARUS || CC_MAP_RANGE(lpObj->Map) != 0)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(659));

		if (Npc >= 0)
		{
			GCChatTargetNewSend(lpObj, Npc, gMessage.GlobalText(659));
		}
		return 0;
	}

	if (OBJECT_RANGE(lpObj->SummonIndex) != 0)
	{
		gObjSummonKill(lpObj->Index);
		GCSummonLifeSend(lpObj->Index, 0, 1);
		return 0;
	}

	int index;

	for (int i = 1; i <= gServerInfo.m_CommandHelpMonsterAmount; i++)
	{
		index = gObjAddSummon();

		if (OBJECT_RANGE(index) == 0)
		{
			return 0;
		}

		lpObj->SummonIndex = index;

		LPOBJ lpSummon = &gObj[index];

		lpSummon->X = lpObj->X - 1;
		lpSummon->Y = lpObj->Y + i;
		lpSummon->MTX = lpSummon->X;
		lpSummon->MTY = lpSummon->Y;
		lpSummon->Dir = 2;
		lpSummon->Map = lpObj->Map;

		gObjSetMonster(index, gServerInfo.m_CommandHelpMonsterIndex);

#if(GAMESERVER_UPDATE>=602)
		lpSummon->Life += (float)((__int64)lpSummon->Life * gMasterSkillTree.GetMasterSkillValue(lpObj, MASTER_SKILL_ADD_SUMMON_LIFE)) / 100;
		lpSummon->MaxLife += (float)((__int64)lpSummon->MaxLife * gMasterSkillTree.GetMasterSkillValue(lpObj, MASTER_SKILL_ADD_SUMMON_LIFE)) / 100;
		lpSummon->ScriptMaxLife += (float)((__int64)lpSummon->ScriptMaxLife * gMasterSkillTree.GetMasterSkillValue(lpObj, MASTER_SKILL_ADD_SUMMON_LIFE)) / 100;
		lpSummon->Defense += ((__int64)lpSummon->Defense * gMasterSkillTree.GetMasterSkillValue(lpObj, MASTER_SKILL_ADD_SUMMON_DEFENSE)) / 100;
		lpSummon->PhysiDamageMin += ((__int64)lpSummon->PhysiDamageMin * gMasterSkillTree.GetMasterSkillValue(lpObj, MASTER_SKILL_ADD_SUMMON_DAMAGE)) / 100;
		lpSummon->PhysiDamageMax += ((__int64)lpSummon->PhysiDamageMax * gMasterSkillTree.GetMasterSkillValue(lpObj, MASTER_SKILL_ADD_SUMMON_DAMAGE)) / 100;
#endif
		lpSummon->SummonIndex = lpObj->Index;
		lpSummon->Attribute = 100;
		lpSummon->TargetNumber = -1;
		lpSummon->ActionState.Attack = 0;
		lpSummon->ActionState.Emotion = 0;
		lpSummon->ActionState.EmotionCount = 0;
		lpSummon->PathCount = 0;
		lpSummon->MoveRange = 15;

		GCSummonLifeSend(lpSummon->SummonIndex, (int)lpSummon->Life, (int)lpSummon->MaxLife);
	}

	gLog.Output(LOG_COMMAND, "[CommandHelper][%s][%s] - Used", lpObj->Account, lpObj->Name);

#endif
	return 1;
}

bool CCommandManager::CommandReMaster(LPOBJ lpObj, char* arg, int Npc)
{
#if (GAMESERVER_CLIENTE_UPDATE >= 6)
	if (gObjRebuildMasterSkillTree(lpObj) == 1)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(755));

		if (Npc >= 0)
		{
			GCChatTargetNewSend(lpObj, Npc, gMessage.GlobalText(755));
		}

		gLog.Output(LOG_COMMAND, "[CommandReMaster][%s][%s] - Used", lpObj->Account, lpObj->Name);
		return 1;
	}
	else
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(756));

		if (Npc >= 0)
		{
			GCChatTargetNewSend(lpObj, Npc, gMessage.GlobalText(756));
		}
		return 0;
	}
#endif
	return 1;
}

bool CCommandManager::CommandStartBC(LPOBJ lpObj, char* arg) // OK
{
#if (GAMESERVER_CLIENTE_UPDATE >= 6)
	gBloodCastle.StartBC();

	gLog.Output(LOG_COMMAND, "[CommandStartBC][%s][%s] - Used", lpObj->Account, lpObj->Name);
#endif
	return 1;
}

bool CCommandManager::CommandStartDS(LPOBJ lpObj, char* arg) // OK
{
#if (GAMESERVER_CLIENTE_UPDATE >= 6)
	gDevilSquare.StartDS();
	gLog.Output(LOG_COMMAND, "[CommandStartDS][%s][%s] - Used", lpObj->Account, lpObj->Name);
#endif
	return 1;
}

bool CCommandManager::CommandStartCC(LPOBJ lpObj, char* arg) // OK
{
#if (GAMESERVER_CLIENTE_UPDATE >= 6)
	gChaosCastle.StartCC();
	gLog.Output(LOG_COMMAND, "[CommandStartCC][%s][%s] - Used", lpObj->Account, lpObj->Name);
#endif
	return 1;
}

bool CCommandManager::CommandStartIT(LPOBJ lpObj, char* arg) // OK
{
#if (GAMESERVER_CLIENTE_UPDATE >= 6)
	gIllusionTemple.StartIT();
	gLog.Output(LOG_COMMAND, "[CommandStartIT][%s][%s] - Used", lpObj->Account, lpObj->Name);
#endif
	return 1;
}

bool CCommandManager::CommandStartQuiz(LPOBJ lpObj, char* arg) // OK
{
#if (GAMESERVER_CLIENTE_UPDATE >= 9)
	gCustomQuiz.StartQuiz();
	gLog.Output(LOG_COMMAND, "[CommandStartQuiz][%s][%s] - Used", lpObj->Account, lpObj->Name);
#endif
	return 1;
}

bool CCommandManager::CommandStartDrop(LPOBJ lpObj, char* arg) // OK
{
#if (GAMESERVER_CLIENTE_UPDATE >= 9)
	gCustomEventDrop.StartDrop();
	gLog.Output(LOG_COMMAND, "[CommandStartDrop][%s][%s] - Used", lpObj->Account, lpObj->Name);
#endif
	return 1;
}

bool CCommandManager::CommandStartKing(LPOBJ lpObj, char* arg) // OK
{
#if (GAMESERVER_CLIENTE_UPDATE >= 9)
	gReiDoMU.StartKing();
	gLog.Output(LOG_COMMAND, "[CommandStartKingOfMu][%s][%s] - Used", lpObj->Account, lpObj->Name);
#endif
	return 1;
}

bool CCommandManager::CommandStartTvT(LPOBJ lpObj, char* arg) // OK
{
#if (GAMESERVER_CLIENTE_UPDATE >= 9)
	gTvTEvent.StartTvT();
	gLog.Output(LOG_COMMAND, "[CommandStartTVT][%s][%s] - Used", lpObj->Account, lpObj->Name);
#endif
	return 1;
}

bool CCommandManager::CommandStartGvG(LPOBJ lpObj, char* arg) // OK
{
#if (GAMESERVER_CLIENTE_UPDATE >= 14)
	gGvGEvent.StartGvG();
	gLog.Output(LOG_COMMAND, "[CommandStartGvG][%s][%s] - Used", lpObj->Account, lpObj->Name);
#endif
	return 1;
}

bool CCommandManager::CommandStartInvasion(LPOBJ lpObj, char* arg) // OK
{
#if (GAMESERVER_CLIENTE_UPDATE >= 13)

	int index = this->GetNumber(arg, 0);

	if (index <= 0)
	{
		return 0;
	}

	gInvasionManager.StartInvasion(index);
	gLog.Output(LOG_COMMAND, "[CommandStartInvasion][%s][%s] - Used", lpObj->Account, lpObj->Name);
#endif
	return 1;
}

bool CCommandManager::CommandStartCustomArena(LPOBJ lpObj, char* arg) // OK
{
#if (GAMESERVER_CLIENTE_UPDATE >= 13)

	int index = this->GetNumber(arg, 0);

	if (index <= 0)
	{
		return 0;
	}

	gCustomArena.StartCustomArena(index);
	gLog.Output(LOG_COMMAND, "[CommandStartInvasion][%s][%s] - Used", lpObj->Account, lpObj->Name);
#endif
	return 1;
}



bool CCommandManager::CommandDisablePvp(LPOBJ lpObj, char* arg, int Npc)
{
#if (GAMESERVER_CLIENTE_UPDATE >= 7)
	if (lpObj->DisablePvp == 1)
	{
		lpObj->DisablePvp = 0;
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(772));

		if (Npc >= 0)
		{
			GCChatTargetNewSend(lpObj, Npc, gMessage.GlobalText(772));
		}

		gCustomAttack.DGCustomAttackResumeSaveSend(lpObj->Index);

		return 0;
	}
	else
	{
		lpObj->DisablePvp = 1;

		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(771));

		if (Npc >= 0)
		{
			GCChatTargetNewSend(lpObj, Npc, gMessage.GlobalText(771));
		}

		gLog.Output(LOG_COMMAND, "[CommandDisablePvP][%s][%s] - Used", lpObj->Account, lpObj->Name);

		gCustomAttack.DGCustomAttackResumeSaveSend(lpObj->Index);

		return 1;
	}

#endif
	return 0;
}

bool CCommandManager::CommandLock(LPOBJ lpObj, char* arg)
{
#if (GAMESERVER_CLIENTE_UPDATE >= 7)
	if (lpObj->Lock > 0)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(775));
		return 0;
	}

	int password = this->GetNumber(arg, 0);

	if (password <= 0)
	{
		return 0;
	}

	if (password > 99999999)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(774));
		return 0;
	}

	lpObj->Lock = password;

	gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(773));

	gLog.Output(LOG_COMMAND, "[CommandLock][%s][%s] - Password: %d", lpObj->Account, lpObj->Name, password);

	GJAccountLockSaveSend(lpObj->Index, password);

	return 1;

#endif
	return 0;
}

bool CCommandManager::CommandUnLock(LPOBJ lpObj, char* arg)
{
#if (GAMESERVER_CLIENTE_UPDATE >= 7)
	if (lpObj->Lock <= 0)
	{
		return 0;
	}

	int password = this->GetNumber(arg, 0);

	if (password <= 0)
	{
		return 0;
	}

	if (password > 99999999)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(774));
		return 0;
	}

	if (password != lpObj->Lock)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(777));
		return 0;
	}

	lpObj->Lock = 0;

	gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(776));

	gLog.Output(LOG_COMMAND, "[CommandUnLock][%s][%s] - Password: %d", lpObj->Account, lpObj->Name, password);

	GJAccountLockSaveSend(lpObj->Index, 0);

	return 1;

#endif
	return 0;
}

bool CCommandManager::CommandMoveAll(LPOBJ lpObj, char* arg) // OK
{
#if (GAMESERVER_CLIENTE_UPDATE >= 8)

	int map = this->GetNumber(arg, 0);

	int x = this->GetNumber(arg, 1);

	int y = this->GetNumber(arg, 2);

	for (int n = OBJECT_START_USER; n < MAX_OBJECT; n++)
	{
		if (gObjIsConnectedGP(n) != 0 && gObj[n].Index != lpObj->Index)
		{
			if (gObj[n].Connected > OBJECT_LOGGED)
			{
				gObjTeleport(gObj[n].Index, map, x, y);
			}
		}
	}

	gLog.Output(LOG_COMMAND, "[CommandMoveAll][%s][%s] - (Map: %d, X: %d, Y: %d)", lpObj->Account, lpObj->Name, map, x, y);
#endif
	return 1;
}

bool CCommandManager::CommandMoveGuild(LPOBJ lpObj, char* arg) // OK
{
#if (GAMESERVER_CLIENTE_UPDATE >= 8)
	char name[9] = { 0 };

	this->GetString(arg, name, sizeof(name), 0);

	int map = this->GetNumber(arg, 1);

	int x = this->GetNumber(arg, 2);

	int y = this->GetNumber(arg, 3);

	GUILD_INFO_STRUCT* lpGuild = gGuildClass.SearchGuild(name);

	if (lpGuild == NULL)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(798));
		return 0;
	}

	for (int n = 0; n < MAX_GUILD_USER; n++)
	{
		int iGuildMemberIndex = lpGuild->Index[n];

		if (lpGuild->Use[n] > 0 && iGuildMemberIndex != -1)
		{
			LPOBJ lpObj = &gObj[iGuildMemberIndex];

			if (lpObj == NULL)
			{
				continue;
			}

			if (gObjIsConnected(iGuildMemberIndex) == 0)
			{
				continue;
			}

			gObjTeleport(iGuildMemberIndex, map, x, y);
		}
	}

	gLog.Output(LOG_COMMAND, "[CommandMoveGuild][%s][%s] - (Guild: %s, Map: %d, X: %d, Y: %d)", lpObj->Account, lpObj->Name, name, map, x, y);
#endif
	return 1;
}

bool CCommandManager::CommandSpot(LPOBJ lpObj, char* arg) // OK
{
#if (GAMESERVER_CLIENTE_UPDATE >= 9)

	int monster = this->GetNumber(arg, 0);
	int qtd = this->GetNumber(arg, 1);

	qtd = (qtd > 0) ? qtd : 1;

	for (int n = 0; n < qtd; n++)
	{
		int index = gObjAddMonster(lpObj->Map);

		if (OBJECT_RANGE(index) == 0)
		{
			return 0;
		}

		LPOBJ lpMonster = &gObj[index];

		int px = lpObj->X;
		int py = lpObj->Y;

		if (gObjGetRandomFreeLocation(lpObj->Map, &px, &py, 3, 3, 50) == 0)
		{
			return 0;
		}

		lpMonster->PosNum = -1;
		lpMonster->X = px;
		lpMonster->Y = py;
		lpMonster->TX = px;
		lpMonster->TY = py;
		lpMonster->OldX = px;
		lpMonster->OldY = py;
		lpMonster->StartX = px;
		lpMonster->StartY = py;
		lpMonster->Dir = 1;
		lpMonster->Map = lpObj->Map;
		lpMonster->MonsterDeleteTime = GetTickCount() + 1800000;

		if (gObjSetMonster(index, monster) == 0)
		{
			gObjDel(index);
			return 0;
		}
	}

	gLog.Output(LOG_COMMAND, "[CommandSpot][%s][%s] - (Monster: %d, Qtd: %d)", lpObj->Account, lpObj->Name, monster, qtd);
#endif
	return 1;
}

bool CCommandManager::CommandSetCoin(LPOBJ lpObj, char* arg) // OK
{
#if (GAMESERVER_CLIENTE_UPDATE >= 9)

	char name[11] = { 0 };

	this->GetString(arg, name, sizeof(name), 0);

	LPOBJ lpTarget = gObjFind(name);

	if (lpTarget == 0)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GlobalText(667));
		return 0;
	}

	int coin = this->GetNumber(arg, 1);
	int value = this->GetNumber(arg, 2);

	if (coin == 1)
	{
		GDSetCoinSend(lpTarget->Index, value, 0, 0, "GmSetCoin");
		gLog.Output(LOG_COMMAND, "[CommandSetCoin][%s][%s] - (Coin: %d, Value: %d)", lpObj->Account, lpObj->Name, coin, value);
	}
	if (coin == 2)
	{
		GDSetCoinSend(lpTarget->Index, 0, value, 0, "GmSetCoin");
		gLog.Output(LOG_COMMAND, "[CommandSetCoin][%s][%s] - (Coin: %d, Value: %d)", lpObj->Account, lpObj->Name, coin, value);
	}
	if (coin == 3)
	{
		GDSetCoinSend(lpTarget->Index, 0, 0, value, "GmSetCoin");
		gLog.Output(LOG_COMMAND, "[CommandSetCoin][%s][%s] - (Coin: %d, Value: %d)", lpObj->Account, lpObj->Name, coin, value);
	}

#endif
	return 1;

}

#ifdef EVENT_FIND_PATH_NPC_CLIK
bool CCommandManager::CommandStartFindPathToNpc(LPOBJ lpObj, char* arg)
{
	gEventFindPath.CommandEventStart();
	gLog.Output(LOG_COMMAND, "[CommandStartFindPathToNpc][%s][%s] - Used", lpObj->Account, lpObj->Name);

	return false;
}
#endif // EVENT_FIND_PATH_NPC_CLIK

#ifdef PARTY_PASSWORD_JOIN
bool CCommandManager::CommandSetParty(LPOBJ lpObj, char* arg)
{
	if (gParty.IsParty(lpObj->PartyNumber) != 0)
	{
		lpObj->CurAutoJoinParty = -1;
		lpObj->CurAutoJoinPartyPassword.clear();
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "You are already in a party");
		return false;
	}

	char password[20] = { 0 };

	this->GetString(arg, password, sizeof(password), 0);

	if (strlen(password) < 4)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "Password must be longer than 4 character");
		return false;
	}

	if (gParty.Create(lpObj->Index) != 0)
	{
		gParty.m_PartyInfo[lpObj->PartyNumber].Password.assign(password);

		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "Party Password has been set: %s", password);
	}
	else
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "Could not create new party.");
	}

	gLog.Output(LOG_COMMAND, "[CommandSetParty] (Account: %s, Name: %s, Password: %s)", lpObj->Account, lpObj->Name, password);

	return true;
}
#endif // PARTY_PASSWORD_JOIN

//==============================================================================================
bool CCommandManager::CommandSetLevel(LPOBJ lpObj, char* arg)
{
	// 1. Verifica se quem digitou o comando ? GameMaster (ADM)
	if (gGameMaster.CheckGameMasterLevel(lpObj, 1) == 0)
	{
		return 0;
	}



	char name[11] = { 0 };
	char level_str[10] = { 0 };

	// 2. Pega o primeiro argumento (Nome do Personagem)
	this->GetString(arg, name, sizeof(name), 0);

	// 3. Pega o segundo argumento (Valor do Level)
	this->GetString(arg, level_str, sizeof(level_str), 1);

	// Se o nome estiver vazio, avisa como usar
	if (name[0] == '\0' || level_str[0] == '\0')
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "Use: /setlevel <nome> <nivel>");
		return 0;
	}

	// 4. Procura o Personagem pelo nome no servidor
	LPOBJ lpTarget = gObjFind(name);

	if (lpTarget == 0)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "O personagem %s nao esta online.", name);
		return 0;
	}

	// 5. Converte o texto do level para n?mero
	int level = atoi(level_str);

	// Valida??o de limite (Mu Online padr?o vai at? 400 ou 1000 se for Master)
	if (level <= 0 || level > 1000)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "Digite um nivel valido!");
		return 0;
	}

	// 6. Aplica o Level no ALVO (lpTarget)
	lpTarget->Level = level;
	lpTarget->Experience = 0;

	// 7. Atualiza os dados no Cliente (faz o n?mero mudar na tela do player)
	GCLevelUpSend(lpTarget);

	// 8. Recalcula os status (HP/Mana/Ataque) do alvo para o novo level
	gObjectManager.CharacterCalcAttribute(lpTarget->Index);

	// 9. Avisa o ADM e o Jogador
	gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "Voce alterou o nivel de %s para %d!", name, level);
	gNotice.GCNoticeSend(lpTarget->Index, 1, 0, 0, 0, 0, 0, "Seu nivel foi alterado para %d por um ADM!", level);

	// 10. Grava no Log do GameServer
	gLog.Output(LOG_COMMAND, "[CommandSetLevel][%s][%s] - Set Level of: %s to %d", lpObj->Account, lpObj->Name, name, level);

	return 1;

}
bool CCommandManager::CommandSetStatus(LPOBJ lpObj, char* arg)
{
	if (gGameMaster.CheckGameMasterLevel(lpObj, 1) == 0)
	{
		return 0;
	}

	char name[11] = { 0 };
	this->GetString(arg, name, sizeof(name), 0);

	LPOBJ lpTarget = gObjFind(name);

	if (lpTarget == 0)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "Personagem [%s] offline.", name);
		return 1;
	}

	lpTarget->Strength = (int)this->GetNumber(arg, 1);
	lpTarget->Dexterity = (int)this->GetNumber(arg, 2);
	lpTarget->Vitality = (int)this->GetNumber(arg, 3);
	lpTarget->Energy = (int)this->GetNumber(arg, 4);

	if (lpTarget->Class == 4) // Dark Lord
	{
		lpTarget->Leadership = (int)this->GetNumber(arg, 5);
	}

	gObjectManager.CharacterCalcAttribute(lpTarget->Index);

	GCNewCharacterInfoSend(lpTarget);

	gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "Status de %s definidos!", name);
	gNotice.GCNoticeSend(lpTarget->Index, 1, 0, 0, 0, 0, 0, "Seus status foram alterados por um ADM!");

	return 1;
}


bool CCommandManager::CommandSetKit(LPOBJ lpObj, char* arg)
{
	if (gGameMaster.CheckGameMasterLevel(lpObj, 1) == 0)
	{
		return 0;
	}

	char targetName[11] = { 0 };
	int section = 0, itemID = 0, optionsGroup = 0;

	if (sscanf_s(arg, "%s %d %d %d", targetName, (unsigned int)sizeof(targetName), &section, &itemID, &optionsGroup) < 4)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "Use: /set <nome> <se??o> <id> <1111>");
		return 0;
	}

	LPOBJ lpTarget = gObjFind(targetName);

	if (lpTarget == NULL)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "Jogador %s offline.", targetName);
		return 0;
	}

	// Quebra o n?mero 1111 (Ex: 1111 -> Skill=1, Luck=1, Opt=1, Exc=1)
	int skill = optionsGroup / 1000;
	int luck = (optionsGroup / 100) % 10;
	int opt = (optionsGroup / 10) % 10;
	int exc = (optionsGroup % 10 == 1) ? 63 : 0;

	if (section >= 7 && section <= 11)
	{
		for (int s = 7; s <= 11; s++)
		{
			// Criando o Set Completo via DataServer
			GDCreateItemSend(lpTarget->Index, 0xEB, 0, 0, GET_ITEM(s, itemID), 13, 0, skill, luck, opt, -1, exc, 0, 0, 0, 0, 0, 0);
		}
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "Kit enviado para %s!", targetName);
	}
	else
	{
		// Criando Item ?nico via DataServer
		GDCreateItemSend(lpTarget->Index, 0xEB, 0, 0, GET_ITEM(section, itemID), 13, 0, skill, luck, opt, -1, exc, 0, 0, 0, 0, 0, 0);
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "Item enviado para %s!", targetName);
	}

	return 1;
}


static int CommandBotGetClassFilter(char* className)
{
	if(className == 0 || className[0] == 0)
	{
		return -1;
	}

	if(_stricmp(className,"dw") == 0 || _stricmp(className,"sm") == 0 || _stricmp(className,"gm") == 0 || _stricmp(className,"wiz") == 0)
	{
		return 0;
	}

	if(_stricmp(className,"dk") == 0 || _stricmp(className,"bk") == 0 || _stricmp(className,"bm") == 0)
	{
		return 1;
	}

	if(_stricmp(className,"elf") == 0 || _stricmp(className,"fe") == 0 || _stricmp(className,"me") == 0 || _stricmp(className,"he") == 0)
	{
		return 2;
	}

	if(_stricmp(className,"mg") == 0 || _stricmp(className,"dm") == 0)
	{
		return 3;
	}

	if(_stricmp(className,"dl") == 0 || _stricmp(className,"le") == 0)
	{
		return 4;
	}

	if(_stricmp(className,"sum") == 0 || _stricmp(className,"summoner") == 0 || _stricmp(className,"su") == 0 || _stricmp(className,"bs") == 0 || _stricmp(className,"dim") == 0)
	{
		return 5;
	}

	if(_stricmp(className,"rf") == 0 || _stricmp(className,"fm") == 0)
	{
		return 6;
	}

	return -2;
}
bool CCommandManager::CommandBot(LPOBJ lpObj, char* arg)
{
#if(BOT_PLAYER == 1)
	if (gGameMaster.CheckGameMasterLevel(lpObj, 1) == 0)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "Voce nao tem permissao!");
		return 0;
	}

	int count = this->GetNumber(arg, 0);
	if (count <= 0)
	{
		count = 1;
	}

	char className[16] = { 0 };
	this->GetString(arg, className, sizeof(className), 1);
	int classFilter = CommandBotGetClassFilter(className);

	if(classFilter == -2)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "BotPlayer: classe invalida. Use bk, sm, elf, mg, dl, sum ou rf.");
		return 1;
	}

	int created = gBotPlayer.SpawnBots(count, lpObj->Map, lpObj->X, lpObj->Y, lpObj->Dir, classFilter);
	if(created == -1)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "BotPlayer: sistema desligado ou config nao carregada.");
		gLog.Output(LOG_COMMAND, "[CommandBot][%s][%s] - BotPlayer disabled/not loaded", lpObj->Account, lpObj->Name);
		return 1;
	}
	if(created == -2)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "BotPlayer: nenhum modelo ativo para essa classe no BotPlayer.txt.");
		gLog.Output(LOG_COMMAND, "[CommandBot][%s][%s] - No active bot templates", lpObj->Account, lpObj->Name);
		return 1;
	}
	if(created == -3)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "BotPlayer: limite maximo de bots atingido.");
		gLog.Output(LOG_COMMAND, "[CommandBot][%s][%s] - BotPlayer max limit", lpObj->Account, lpObj->Name);
		return 1;
	}
	if (created == -4)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0,
			"BotPlayer: sem espaco no servidor ou falha ao criar. Veja o log do GameServer.");
		return 1;
	}
	gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0,
		"BotPlayer: %d de %d solicitado(s) criado(s). Capacidade do sistema: %d.", created, count, MAX_BOTPLAYER);
	gLog.Output(LOG_COMMAND, "[CommandBot][%s][%s] - Count: %d Created: %d", lpObj->Account, lpObj->Name, count, created);
	return 1;
#else
	gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "BotPlayer desligado na compilacao.");
	return 0;
#endif
}

bool CCommandManager::CommandKillBot(LPOBJ lpObj, char* arg)
{
#if(BOT_PLAYER == 1)
	if (gGameMaster.CheckGameMasterLevel(lpObj, 1) == 0)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "Voce nao tem permissao!");
		return 0;
	}

	int count = this->GetNumber(arg, 0);
	if (count <= 0)
	{
		count = MAX_BOTPLAYER;
	}

	int killed = gBotPlayer.KillBots(count);
	gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "BotPlayer: %d bot(s) removido(s).", killed);
	gLog.Output(LOG_COMMAND, "[CommandKillBot][%s][%s] - Count: %d Killed: %d", lpObj->Account, lpObj->Name, count, killed);
	return 1;
#else
	return 0;
#endif
}

bool CCommandManager::CommandRegistrar(LPOBJ lpObj, char* arg)
{
	if (gGameMaster.CheckGameMasterLevel(lpObj, 1) == 0)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "Voce nao tem permissao!");
		return 0;
	}

	char conta[11] = { 0 };
	char senha[11] = { 0 };

	if (sscanf_s(arg, "%10s %10s", conta, 11, senha, 11) != 2)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "Uso: /registrar <conta> <senha>");
		return 0;
	}

	if (strlen(conta) < 4 || strlen(senha) < 4)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "Dados invalidos! Conta>=4, Senha>=4");
		return 0;
	}

	// Manda vazio no lugar do num e fone
	GJSendRegOrLostPass(lpObj->Index, 0x01, conta, senha, "", "");
	gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "Enviando registro: %s", conta);
	return 1;
}




     




