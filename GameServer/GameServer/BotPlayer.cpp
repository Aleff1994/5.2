#include "stdafx.h"
#include "BotPlayer.h"
#include "Party.h"
#include "Attack.h"
#include "EffectManager.h"
#include "Map.h"
#include "Monster.h"
#include "ObjectManager.h"
#include "Protocol.h"
#include "SkillManager.h"
#include "Util.h"
#include "ItemManager.h"
#include "MemScript.h"
#include "PvPTournament.h"
#include <set>
#include <vector>

#if(BOT_PLAYER == 1)

static bool BotPlayerInitializeObject(int index)
{
    if(!OBJECT_RANGE(index)) return false;
    LPOBJ object=&gObj[index];
    object->ConnectTickCount=GetTickCount();
    object->ShopNumber=-1;object->TargetNumber=-1;object->SummonIndex=-1;object->LastAttackerID=-1;
    object->Connected=OBJECT_ONLINE;object->Live=1;object->State=OBJECT_CREATE;
    object->DieRegen=0;object->Type=OBJECT_BOTS;
    object->BasicAI=0;object->CurrentAI=0;object->CurrentAIState=0;
    object->LastAIRunTime=0;object->GroupNumber=0;object->SubGroupNumber=0;
    object->GroupMemberGuid=-1;object->RegenType=0;object->AttackType=0;
    object->Agro.ResetAll();
    return true;
}

static void BotPlayerLeaveParty(int index)
{
    if (!OBJECT_RANGE(index)) return;
    int party = gObj[index].PartyNumber;
    if (!gParty.IsParty(party)) return;
    gParty.DelMember(party, index);
    if (gParty.GetMemberCount(party) == 0) gParty.Destroy(party);
}


static int BotPlayerGetBaseClass(int dbClass)
{
	// Aceita DBClass completo: 0/1/2=DW/SM/GM, 16/17/18=DK/BK/BM,
	// 32/33/34=Elf/ME/HE etc. Tambem aceita classe base 3..6 sem evolucao.
	if(dbClass < 0)
	{
		return 0;
	}

	if(dbClass >= 16)
	{
		return (dbClass/16);
	}

	if(dbClass <= 2)
	{
		return 0;
	}

	return dbClass;
}

static int BotPlayerGetChangeUp(int dbClass)
{
	if(dbClass < 0)
	{
		return 0;
	}

	if(dbClass >= 16)
	{
		return (dbClass%16);
	}

	if(dbClass <= 2)
	{
		return dbClass;
	}

	return 0;
}
CBotPlayer gBotPlayer;

CBotPlayer::CBotPlayer()
{
	this->Clear();
}

void CBotPlayer::Clear()
{
	this->Enabled = 0;
	this->BotCount = 0;
	this->TemplateCount = 0;
	memset(this->bot,0,sizeof(this->bot));
	for(int n=0;n<MAX_BOTPLAYER;n++)
	{
		this->bot[n].index = -1;
		this->bot[n].CanBeAttacked = 1;
		this->bot[n].SkillNumber = 0;
		this->bot[n].PotionEnabled = 0;
		this->bot[n].PotionPercent = 35;
		this->bot[n].PotionDelay = 3000;
		this->bot[n].PotionRecoverPercent = 35;
		for(int i=0;i<MAX_BOTPLAYER_BODY;i++)
		{
			this->bot[n].body[i].num = -1;
		}
	}
}

void CBotPlayer::RemoveBots()
{
	for(int n=0;n<MAX_BOTPLAYER;n++)
	{
		if(this->bot[n].index >= 0 && gObjIsConnected(this->bot[n].index) != 0)
		{
			BotPlayerLeaveParty(this->bot[n].index);
			gObjDel(this->bot[n].index);
		}
	}
}

static char* BotPlayerTrim(char* text)
{
	while(*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n')
	{
		text++;
	}

	int len = (int)strlen(text);

	while(len > 0 && (text[len-1] == ' ' || text[len-1] == '\t' || text[len-1] == '\r' || text[len-1] == '\n'))
	{
		text[len-1] = 0;
		len--;
	}

	return text;
}

void CBotPlayer::Load(char* path)
{
	this->RemoveBots();
	this->Clear();

	FILE* fp = 0;

	if(fopen_s(&fp,path,"rt") != 0 || fp == 0)
	{
		ErrorMessageBox("BotPlayer file not found: %s",path);
		return;
	}

	int section = -1;
	int MaxTemplateIndex = -1;
	char line[512];

	while(fgets(line,sizeof(line),fp) != 0)
	{
		char* comment = strstr(line,"//");

		if(comment != 0)
		{
			comment[0] = 0;
		}

		char* data = BotPlayerTrim(line);

		if(data[0] == 0)
		{
			continue;
		}

		if(_stricmp(data,"end") == 0)
		{
			section = -1;
			continue;
		}

		if(section < 0)
		{
			section = atoi(data);
			continue;
		}

		if(section == 0)
		{
			int Enabled = 0;
			int BotCount = 0;

			if(sscanf_s(data,"%d %d",&Enabled,&BotCount) == 2)
			{
				this->Enabled = ((Enabled == 0) ? 0 : 1);
				this->BotCount = ((BotCount > MAX_BOTPLAYER) ? MAX_BOTPLAYER : BotCount);
			}
		}
		else if(section == 1)
		{
			int BotNum = 0;
			int Class = 0;
			int Enabled = 0;
			int PKLevel = 0;
			int Level = 0;
			char Name[16] = {0};
			int Map = 0;
			int X = 0;
			int Y = 0;
			int Dir = 0;
			int AttackEnabled = 0;
			int AttackPlayers = 0;
			int AttackMonsters = 0;
			int SearchRange = 0;
			int AttackRange = 0;
			int AttackDelay = 0;
			int MoveEnabled = 0;
			int MoveDelay = 0;
			int TalkEnabled = 0;
			int TalkDelay = 0;

			int count = sscanf_s(data,"%d %d %d %d %d \"%15[^\"]\" %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
				&BotNum,&Class,&Enabled,&PKLevel,&Level,Name,(unsigned int)sizeof(Name),&Map,&X,&Y,&Dir,&AttackEnabled,&AttackPlayers,&AttackMonsters,&SearchRange,&AttackRange,&AttackDelay,&MoveEnabled,&MoveDelay,&TalkEnabled,&TalkDelay);

			if(count != 20)
			{
				count = sscanf_s(data,"%d %d %d %d %d %15s %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
					&BotNum,&Class,&Enabled,&PKLevel,&Level,Name,(unsigned int)sizeof(Name),&Map,&X,&Y,&Dir,&AttackEnabled,&AttackPlayers,&AttackMonsters,&SearchRange,&AttackRange,&AttackDelay,&MoveEnabled,&MoveDelay,&TalkEnabled,&TalkDelay);
			}

			if(count == 20 && BotNum >= 0 && BotNum < MAX_BOTPLAYER)
			{
				BOTPLAYER_INFO* lpInfo = &this->bot[BotNum];
				lpInfo->Class = Class;
				lpInfo->Enabled = Enabled;
				lpInfo->PKLevel = PKLevel;
				lpInfo->Level = Level;
				strncpy_s(lpInfo->Name,Name,sizeof(lpInfo->Name));
				lpInfo->Map = (BYTE)Map;
				lpInfo->X = (BYTE)X;
				lpInfo->Y = (BYTE)Y;
				lpInfo->Dir = (BYTE)Dir;
				lpInfo->AttackEnabled = AttackEnabled;
				lpInfo->AttackPlayers = AttackPlayers;
				lpInfo->AttackMonsters = AttackMonsters;
				lpInfo->SearchRange = SearchRange;
				lpInfo->AttackRange = AttackRange;
				lpInfo->AttackDelay = AttackDelay;
				lpInfo->MoveEnabled = MoveEnabled;
				lpInfo->MoveDelay = MoveDelay;
				lpInfo->TalkEnabled = TalkEnabled;
				lpInfo->TalkDelay = TalkDelay;

				if(lpInfo->Enabled != 0 && BotNum > MaxTemplateIndex)
				{
					MaxTemplateIndex = BotNum;
				}
			}
		}
		else if(section == 2)
		{
			int BotNum = 0;
			int X = 0;
			int Y = 0;

			if(sscanf_s(data,"%d %d %d",&BotNum,&X,&Y) == 3 && BotNum >= 0 && BotNum < MAX_BOTPLAYER)
			{
				BOTPLAYER_INFO* lpInfo = &this->bot[BotNum];

				if(lpInfo->RouteCount < MAX_BOTPLAYER_ROUTE)
				{
					lpInfo->route[lpInfo->RouteCount].x = (BYTE)X;
					lpInfo->route[lpInfo->RouteCount].y = (BYTE)Y;
					lpInfo->RouteCount++;
				}
			}
		}
		else if(section == 3)
		{
			int BotNum = 0;
			int Slot = 0;
			int Type = 0;
			int Index = 0;
			int Level = 0;
			int Opt = 0;

			if(sscanf_s(data,"%d %d %d %d %d %d",&BotNum,&Slot,&Type,&Index,&Level,&Opt) == 6 && BotNum >= 0 && BotNum < MAX_BOTPLAYER && Slot >= 0 && Slot < MAX_BOTPLAYER_BODY)
			{
				this->bot[BotNum].body[Slot].num = GET_ITEM(Type,Index);
				this->bot[BotNum].body[Slot].level = Level;
				this->bot[BotNum].body[Slot].opt = Opt;
				this->bot[BotNum].body[Slot].Enabled = 1;
			}
		}
		else if(section == 4)
		{
			int BotNum = 0;
			char Message[128] = {0};
			int count = sscanf_s(data,"%d \"%127[^\"]\"",&BotNum,Message,(unsigned int)sizeof(Message));

			if(count == 2 && BotNum >= 0 && BotNum < MAX_BOTPLAYER && this->bot[BotNum].MessageCount < MAX_BOTPLAYER_MESSAGE)
			{
				strncpy_s(this->bot[BotNum].message[this->bot[BotNum].MessageCount],Message,sizeof(this->bot[BotNum].message[this->bot[BotNum].MessageCount]));
				this->bot[BotNum].MessageCount++;
			}
		}
		else if(section == 5)
		{
			int BotNum = 0;
			int SkillNumber = 0;
			int CanBeAttacked = 0;
			int Strength = 0;
			int Dexterity = 0;
			int Vitality = 0;
			int Energy = 0;
			int Leadership = 0;
			int Life = 0;
			int Mana = 0;

			if(sscanf_s(data,"%d %d %d %d %d %d %d %d %d %d",&BotNum,&SkillNumber,&CanBeAttacked,&Strength,&Dexterity,&Vitality,&Energy,&Leadership,&Life,&Mana) == 10 && BotNum >= 0 && BotNum < MAX_BOTPLAYER)
			{
				BOTPLAYER_INFO* lpInfo = &this->bot[BotNum];
				lpInfo->SkillNumber = SkillNumber;
				lpInfo->CanBeAttacked = CanBeAttacked;
				lpInfo->Strength = Strength;
				lpInfo->Dexterity = Dexterity;
				lpInfo->Vitality = Vitality;
				lpInfo->Energy = Energy;
				lpInfo->Leadership = Leadership;
				lpInfo->Life = Life;
				lpInfo->Mana = Mana;
			}
		}
		else if(section == 6)
		{
			int BotNum = 0;
			int PotionEnabled = 0;
			int PotionPercent = 0;
			int PotionDelay = 0;
			int PotionRecoverPercent = 0;

			if(sscanf_s(data,"%d %d %d %d %d",&BotNum,&PotionEnabled,&PotionPercent,&PotionDelay,&PotionRecoverPercent) == 5 && BotNum >= 0 && BotNum < MAX_BOTPLAYER)
			{
				BOTPLAYER_INFO* lpInfo = &this->bot[BotNum];
				lpInfo->PotionEnabled = PotionEnabled;
				lpInfo->PotionPercent = PotionPercent;
				lpInfo->PotionDelay = PotionDelay;
				lpInfo->PotionRecoverPercent = PotionRecoverPercent;
			}
		}
		else if(section == 7)
		{
			int BotNum = 0;
			int BuffEnabled = 0;
			int BuffSkillNumber = 0;
			int BuffTargetMode = 0;
			int BuffRange = 0;
			int BuffPower = 0;
			int BuffTime = 0;
			int BuffDelay = 0;

			if(sscanf_s(data,"%d %d %d %d %d %d %d %d",&BotNum,&BuffEnabled,&BuffSkillNumber,&BuffTargetMode,&BuffRange,&BuffPower,&BuffTime,&BuffDelay) == 8 && BotNum >= 0 && BotNum < MAX_BOTPLAYER)
			{
				BOTPLAYER_INFO* lpInfo = &this->bot[BotNum];
				lpInfo->BuffEnabled = BuffEnabled;
				lpInfo->BuffSkillNumber = BuffSkillNumber;
				lpInfo->BuffTargetMode = BuffTargetMode;
				lpInfo->BuffRange = BuffRange;
				lpInfo->BuffPower = BuffPower;
				lpInfo->BuffTime = BuffTime;
				lpInfo->BuffDelay = BuffDelay;
			}
		}
	}

	fclose(fp);

	this->TemplateCount = ((MaxTemplateIndex >= 0) ? (MaxTemplateIndex+1) : this->BotCount);
	this->TemplateCount = ((this->TemplateCount > MAX_BOTPLAYER) ? MAX_BOTPLAYER : this->TemplateCount);

	LogAdd(LOG_GREEN,"[BotPlayer] Config loaded. Enabled=%d Count=%d",this->Enabled,this->TemplateCount);
}
void CBotPlayer::MakeBot()
{
	if(this->Enabled == 0)
	{
		return;
	}

	for(int n=0;n<this->TemplateCount;n++)
	{
		if(this->bot[n].Enabled != 0)
		{
			this->CreateBot(n);
		}
	}
}

bool CBotPlayer::CreateBot(int botNum)
{
	if(this->bot[botNum].index >= 0 && gObjIsConnected(this->bot[botNum].index) != 0)
	{
		BotPlayerLeaveParty(this->bot[botNum].index);
		gObjDel(this->bot[botNum].index);
		this->bot[botNum].index = -1;
	}

	int result = gObjAddSummon();
	if(result < 0)
	{
		LogAdd(LOG_RED,"[BotPlayer] Nao foi possivel criar bot %d",botNum);
		return false;
	}

	BOTPLAYER_INFO* lpInfo = &this->bot[botNum];
	lpInfo->index = result;
	lpInfo->LastTargetSearchTick = 0;
	lpInfo->TargetSearchEmpty = false;

	gObj[result].PosNum = -1;
	gObj[result].X = lpInfo->X;
	gObj[result].Y = lpInfo->Y;
	gObj[result].OldX = lpInfo->X;
	gObj[result].OldY = lpInfo->Y;
	gObj[result].TX = lpInfo->X;
	gObj[result].TY = lpInfo->Y;
	gObj[result].MTX = lpInfo->X;
	gObj[result].MTY = lpInfo->Y;
	gObj[result].Dir = lpInfo->Dir;
	gObj[result].Map = lpInfo->Map;
	gObj[result].Live = 1;
	gObj[result].PathCount = 0;
	gObj[result].IsBot = 3;

	if(BotPlayerInitializeObject(result) == false)
	{
		gObjDel(result);
		lpInfo->index = -1;
		return false;
	}

	gObj[result].ChangeUp = BotPlayerGetChangeUp(lpInfo->Class);
	gObj[result].Class = BotPlayerGetBaseClass(lpInfo->Class);
	gObj[result].DBClass = lpInfo->Class;
	gObj[result].Level = lpInfo->Level;
	gObj[result].PKLevel = lpInfo->PKLevel;
	gObj[result].Inventory = new CItem[INVENTORY_SIZE];
	gObj[result].Inventory1 = gObj[result].Inventory;
	gObj[result].InventoryMap = new BYTE[INVENTORY_SIZE];
	gObj[result].InventoryMap1 = gObj[result].InventoryMap;

	for(int i=0;i<INVENTORY_SIZE;i++)
	{
		gObj[result].Inventory[i].Clear();
		gObj[result].Inventory1[i].Clear();
	}

	memset(&gObj[result].InventoryMap[0],(BYTE)-1,INVENTORY_SIZE);
	memset(&gObj[result].InventoryMap1[0],(BYTE)-1,INVENTORY_SIZE);

	strncpy_s(gObj[result].Name,lpInfo->Name,sizeof(gObj[result].Name));

	for(int i=0;i<MAX_BOTPLAYER_BODY;i++)
	{
		this->InsertItem(result,&lpInfo->body[i],i);
	}

	gObj[result].Inventory1 = gObj[result].Inventory;
	gObj[result].InventoryMap1 = gObj[result].InventoryMap;
	gObjectManager.CharacterMakePreviewCharSet(result);
	if(lpInfo->SkillNumber > 0)
	{
		gSkillManager.AddSkill(&gObj[result],lpInfo->SkillNumber,0);
	}
	if(lpInfo->BuffSkillNumber > 0)
	{
		gSkillManager.AddSkill(&gObj[result],lpInfo->BuffSkillNumber,0);
	}
	this->SetBotStats(result,botNum);
	GCActionSend(&gObj[result],ACTION_SALUTE1,result,result);
	return true;
}

void CBotPlayer::SetBotStats(int aIndex,int botNum)
{
	LPOBJ lpObj = &gObj[aIndex];
	BOTPLAYER_INFO* lpInfo = &this->bot[botNum];

	// BotPlayer: limpa bonus herdados de item/efeito e deixa o status do bot vir do TXT.
	lpObj->AddStrength = 0;
	lpObj->AddDexterity = 0;
	lpObj->AddVitality = 0;
	lpObj->AddEnergy = 0;
	lpObj->AddLeadership = 0;
	lpObj->AddLife = 0;
	lpObj->AddMana = 0;
	lpObj->AddShield = 0;
	lpObj->AddBP = 0;
	memset(&lpObj->EffectOption,0,sizeof(lpObj->EffectOption));

	lpObj->Strength = lpInfo->Strength;
	lpObj->Dexterity = lpInfo->Dexterity;
	lpObj->Vitality = lpInfo->Vitality;
	lpObj->Energy = lpInfo->Energy;
	lpObj->Leadership = lpInfo->Leadership;

	int Strength = lpObj->Strength + lpObj->AddStrength;
	int Dexterity = lpObj->Dexterity + lpObj->AddDexterity;
	int Vitality = lpObj->Vitality + lpObj->AddVitality;
	int Energy = lpObj->Energy + lpObj->AddEnergy;
	int Leadership = lpObj->Leadership + lpObj->AddLeadership;

	// BotPlayer: zera os atributos derivados antes de recalcular.
	// Sem isso, um slot reaproveitado pode manter restos de dano/defesa/velocidade antigos
	// e dar a impressao de que o bot herdou status do player ou de outro objeto.
	lpObj->PhysiDamageMin = 0;
	lpObj->PhysiDamageMax = 0;
	lpObj->PhysiDamageMinRight = 0;
	lpObj->PhysiDamageMaxRight = 0;
	lpObj->PhysiDamageMinLeft = 0;
	lpObj->PhysiDamageMaxLeft = 0;
	lpObj->MagicDamageMin = 0;
	lpObj->MagicDamageMax = 0;
	lpObj->CurseDamageMin = 0;
	lpObj->CurseDamageMax = 0;
	lpObj->AttackSuccessRate = 0;
	lpObj->AttackSuccessRatePvP = 0;
	lpObj->DefenseSuccessRate = 0;
	lpObj->DefenseSuccessRatePvP = 0;
	lpObj->Defense = 0;
	lpObj->PhysiSpeed = 0;
	lpObj->MagicSpeed = 0;

	// BotPlayer: calcula os atributos principais de forma segura, sem mudar o Type para OBJECT_USER.
	// Isso evita crash no GS e faz Forca/Agilidade/Vitalidade/Energia do TXT pesarem como player.
	if(lpObj->Class == CLASS_DW)
	{
		lpObj->PhysiDamageMinRight = Strength / 8;
		lpObj->PhysiDamageMaxRight = Strength / 4;
		lpObj->MagicDamageMin = Energy / 9;
		lpObj->MagicDamageMax = Energy / 4;
	}
	else if(lpObj->Class == CLASS_DK)
	{
		lpObj->PhysiDamageMinRight = Strength / 6;
		lpObj->PhysiDamageMaxRight = Strength / 4;
		lpObj->MagicDamageMin = Energy / 9;
		lpObj->MagicDamageMax = Energy / 4;
	}
	else if(lpObj->Class == CLASS_FE || lpObj->Class == CLASS_SU)
	{
		lpObj->PhysiDamageMinRight = (Strength + Dexterity) / 7;
		lpObj->PhysiDamageMaxRight = (Strength + Dexterity) / 4;
		lpObj->MagicDamageMin = Energy / 9;
		lpObj->MagicDamageMax = Energy / 4;
	}
	else if(lpObj->Class == CLASS_MG || lpObj->Class == CLASS_DL)
	{
		lpObj->PhysiDamageMinRight = (Strength / 6) + (Energy / 12);
		lpObj->PhysiDamageMaxRight = (Strength / 4) + (Energy / 8);
		lpObj->MagicDamageMin = Energy / 9;
		lpObj->MagicDamageMax = Energy / 4;
	}
	else if(lpObj->Class == CLASS_RF)
	{
		lpObj->PhysiDamageMinRight = (Strength / 7) + (Vitality / 15);
		lpObj->PhysiDamageMaxRight = (Strength / 5) + (Vitality / 12);
		lpObj->MagicDamageMin = Energy / 9;
		lpObj->MagicDamageMax = Energy / 4;
	}
	else
	{
		lpObj->PhysiDamageMinRight = Strength / 6;
		lpObj->PhysiDamageMaxRight = Strength / 4;
		lpObj->MagicDamageMin = Energy / 9;
		lpObj->MagicDamageMax = Energy / 4;
	}

	lpObj->PhysiDamageMinLeft = lpObj->PhysiDamageMinRight;
	lpObj->PhysiDamageMaxLeft = lpObj->PhysiDamageMaxRight;
	lpObj->CurseDamageMin = lpObj->MagicDamageMin;
	lpObj->CurseDamageMax = lpObj->MagicDamageMax;
	lpObj->AttackSuccessRate = ((lpObj->Level + lpObj->MasterLevel) * 5) + (Dexterity * 3) + Strength;
	lpObj->AttackSuccessRatePvP = ((lpObj->Level + lpObj->MasterLevel) * 3) + (Dexterity * 3);
	lpObj->DefenseSuccessRate = Dexterity / 3;
	lpObj->DefenseSuccessRatePvP = ((lpObj->Level + lpObj->MasterLevel) / 2) + (Dexterity / 2);
	lpObj->Defense = Dexterity / 4;
	lpObj->PhysiSpeed = Dexterity / 15;
	lpObj->MagicSpeed = Dexterity / 15;
	// BotPlayer: reforca acerto PvP usando os status configurados.
	// No MU o dano sobe com Forca/Energia, mas o MISS depende muito de AttackSuccessRatePvP.
	// Assim, se voce colocar muita Forca no bot, ele tambem ganha acerto para teste de PvP.
	int BotHitRate = ((lpObj->Level + lpObj->MasterLevel) * 10) + ((lpObj->Strength + lpObj->Dexterity) * 5);
	BotHitRate = ((BotHitRate < 1000) ? 1000 : BotHitRate);

	if(lpObj->AttackSuccessRate < BotHitRate) { lpObj->AttackSuccessRate = BotHitRate; }
	if(lpObj->AttackSuccessRatePvP < BotHitRate) { lpObj->AttackSuccessRatePvP = BotHitRate; }

	int BotPhysiDamageMin = (lpObj->PhysiDamageMinRight + lpObj->PhysiDamageMinLeft);
	int BotPhysiDamageMax = (lpObj->PhysiDamageMaxRight + lpObj->PhysiDamageMaxLeft);

	if(lpObj->PhysiDamageMin < BotPhysiDamageMin) { lpObj->PhysiDamageMin = BotPhysiDamageMin; }
	if(lpObj->PhysiDamageMax < BotPhysiDamageMax) { lpObj->PhysiDamageMax = BotPhysiDamageMax; }

	lpObj->MaxLife = ((lpInfo->Life > 0) ? lpInfo->Life : ((lpObj->MaxLife < 1000) ? 1000 : lpObj->MaxLife));
	lpObj->Life = lpObj->MaxLife;
	lpObj->MaxShield = ((lpObj->MaxShield < (lpObj->MaxLife/2)) ? (lpObj->MaxLife/2) : lpObj->MaxShield);
	lpObj->Shield = lpObj->MaxShield;
	lpObj->MaxMana = ((lpInfo->Mana > 0) ? lpInfo->Mana : ((lpObj->MaxMana < 100) ? 100 : lpObj->MaxMana));
	lpObj->Mana = lpObj->MaxMana;
	lpObj->PhysiDamageMin = ((lpObj->PhysiDamageMin < 100) ? 100 : lpObj->PhysiDamageMin);
	lpObj->PhysiDamageMax = ((lpObj->PhysiDamageMax < 150) ? 150 : lpObj->PhysiDamageMax);
	lpObj->AttackRange = lpInfo->AttackRange;
	lpObj->MoveRange = 4;
	lpObj->TargetNumber = -1;
	lpObj->ActionState.Attack = 0;
	lpObj->ActionState.Move = 0;

}

void CBotPlayer::InsertItem(int aIndex, BOTPLAYER_BODY_ITEM* lpItem, int slot)
{
	if(lpItem->Enabled == 0 || lpItem->num < 0 || slot < 0 || slot >= MAX_BOTPLAYER_BODY)
	{
		return;
	}

	ITEM_INFO itemInfo;
	if (gItemManager.GetInfo(lpItem->num, &itemInfo) == 0)
	{
		// Um catalogo desatualizado nao deve virar um falso erro de classe nem
		// gerar uma linha para cada copia do mesmo bot.
		static std::set<int> missingItems;
		if (missingItems.insert(lpItem->num).second)
		{
			LogAdd(LOG_RED, "[BotPlayer] Item %d do BotPlayer.txt nao existe no catalogo Item.txt carregado", lpItem->num);
		}
		return;
	}

	CItem item;
	item.m_Level = lpItem->level;
	// Armas/escudos recebem a opcao Skill; armaduras mantem somente Luck/Option.
	item.m_Option1 = ((slot == 0 || slot == 1) ? 1 : 0);
	item.m_Option2 = 1;
	item.m_Option3 = lpItem->opt;
	item.m_Durability = 255.0f;
	item.m_JewelOfHarmonyOption = 0;
	item.m_ItemOptionEx = 0;
	memset(item.m_SocketOption,0xFF,sizeof(item.m_SocketOption));
	item.Convert(lpItem->num,item.m_Option1,item.m_Option2,item.m_Option3,item.m_NewOption,item.PackedSetOption(),item.m_JewelOfHarmonyOption,item.m_ItemOptionEx,item.m_SocketOption,item.m_SocketOptionBonus);
	if (item.m_Index < 0)
	{
		LogAdd(LOG_RED, "[BotPlayer] Falha ao converter item %d do bot %s", lpItem->num, gObj[aIndex].Name);
		return;
	}

	if(gItemManager.CheckItemRequireClass(&gObj[aIndex],item.m_Index) == 0)
	{
		LogAdd(LOG_RED,"[BotPlayer] Item %d ignorado no bot %s: classe nao permitida",item.m_Index,gObj[aIndex].Name);
		return;
	}

	gObj[aIndex].Inventory[slot] = item;
}

void CBotPlayer::MainProc()
{
	if(this->Enabled == 0)
	{
		return;
	}

	for(int n=0;n<this->BotCount;n++)
	{
		BOTPLAYER_INFO* lpInfo = &this->bot[n];
		if(lpInfo->Enabled == 0 || OBJECT_RANGE(lpInfo->index) == 0 || gObjIsConnected(lpInfo->index) == 0)
		{
			continue;
		}

		LPOBJ lpBot = &gObj[lpInfo->index];
		if(lpBot->Live == 0)
		{
			continue;
		}

		if (lpBot->Teleport != 0 || lpBot->Interface.use != 0) continue;
		this->PotionBot(lpInfo,lpBot);
		this->BuffBot(lpInfo,lpBot);

		int target = this->SearchTarget(lpInfo,lpBot);
		if(target >= 0)
		{
			this->AttackTarget(lpInfo,lpBot,target);
		}
		else
		{
			this->MoveBot(lpInfo,lpBot);
		}

		this->TalkBot(lpInfo,lpBot);
	}
}

int CBotPlayer::SearchTarget(BOTPLAYER_INFO* lpInfo, LPOBJ lpBot)
{
	if(lpInfo->AttackEnabled == 0)
	{
		return -1;
	}

	// Dentro da cidade/safe zone o bot nao ataca nem revida.
	if(gMap[lpBot->Map].CheckAttr(lpBot->X,lpBot->Y,1) != 0)
	{
		lpBot->LastAttackerID = -1;
		return -1;
	}

	int target = lpBot->LastAttackerID;
	int tournamentOpponent = gPvPTournament.GetMatchOpponent(lpBot->Index);

	// Dentro de uma luta oficial o bot ataca o adversario sorteado sem esperar apanhar.
	if(OBJECT_RANGE(tournamentOpponent) != 0 && gObjIsConnected(tournamentOpponent) != 0 && gObj[tournamentOpponent].Live != 0 && gObj[tournamentOpponent].Map == lpBot->Map)
	{
		return tournamentOpponent;
	}

	// Player: continua passivo igual player tranquilo, so revida se player bater nele.
	if(OBJECT_RANGE(target) != 0 && target != lpBot->Index && gObjIsConnected(target) != 0 && gObj[target].Live != 0 && gObj[target].Map == lpBot->Map)
	{
		if(gMap[gObj[target].Map].CheckAttr(gObj[target].X,gObj[target].Y,1) == 0)
		{
			if(((gObj[target].Type == OBJECT_USER || gObj[target].Type == OBJECT_BOTS) && lpInfo->AttackPlayers != 0) || (gObj[target].Type == OBJECT_MONSTER && lpInfo->AttackMonsters != 0))
			{
				int distance = (int)sqrt((float)(((lpBot->X-gObj[target].X)*(lpBot->X-gObj[target].X))+((lpBot->Y-gObj[target].Y)*(lpBot->Y-gObj[target].Y))));

				if(distance <= lpInfo->SearchRange)
				{
					return target;
				}
			}
		}
	}

	lpBot->LastAttackerID = -1;

	// Monstro: se tiver AttackMonsters=1, o bot procura o monstro mais perto e ataca sozinho.
	// Isso deixa ele mais parecido com player farmando fora da cidade, sem sair batendo em player do nada.
	if(lpInfo->AttackMonsters == 0)
	{
		return -1;
	}

	// Empty-area polling is bounded; retaliation above remains immediate.
	const DWORD searchTick = GetTickCount();
	if (lpInfo->TargetSearchEmpty && searchTick-lpInfo->LastTargetSearchTick < 500)
		return -1;
	lpInfo->LastTargetSearchTick = searchTick;
	int bestTarget = -1;
	int bestDistance = lpInfo->SearchRange + 1;

	for(int n=OBJECT_START_MONSTER;n < MAX_OBJECT_MONSTER;n++)
	{
		if(gObjIsConnected(n) == 0 || gObj[n].Live == 0 || gObj[n].Type != OBJECT_MONSTER || gObj[n].Map != lpBot->Map)
		{
			continue;
		}

		// Reject distant objects before terrain lookup and square root.
		if (abs(lpBot->X-gObj[n].X) > lpInfo->SearchRange ||
			abs(lpBot->Y-gObj[n].Y) > lpInfo->SearchRange) continue;
		if(gMap[gObj[n].Map].CheckAttr(gObj[n].X,gObj[n].Y,1) != 0)
		{
			continue;
		}

		int distance = (int)sqrt((float)(((lpBot->X-gObj[n].X)*(lpBot->X-gObj[n].X))+((lpBot->Y-gObj[n].Y)*(lpBot->Y-gObj[n].Y))));

		if(distance <= lpInfo->SearchRange && distance < bestDistance)
		{
			bestTarget = n;
			bestDistance = distance;
		}
	}

	lpInfo->TargetSearchEmpty = bestTarget < 0;
	return bestTarget;
}
void CBotPlayer::AttackTarget(BOTPLAYER_INFO* lpInfo, LPOBJ lpBot, int target)
{
	DWORD tick = GetTickCount();

	if(gMap[lpBot->Map].CheckAttr(lpBot->X,lpBot->Y,1) != 0 || gMap[gObj[target].Map].CheckAttr(gObj[target].X,gObj[target].Y,1) != 0)
	{
		lpBot->LastAttackerID = -1;
		return;
	}
	int distance = (int)sqrt((float)(((lpBot->X-gObj[target].X)*(lpBot->X-gObj[target].X))+((lpBot->Y-gObj[target].Y)*(lpBot->Y-gObj[target].Y))));

	if(distance > lpInfo->AttackRange)
	{
		if(tick - lpInfo->LastMoveTick >= (DWORD)lpInfo->MoveDelay)
		{
			if (lpBot->PathCur < lpBot->PathCount &&
				lpBot->MTX == gObj[target].X && lpBot->MTY == gObj[target].Y) return;
			lpBot->MTX = gObj[target].X;
			lpBot->MTY = gObj[target].Y;
			lpBot->ActionState.Move = 1;
			PathFindMoveMsgSend(lpBot);
			lpInfo->LastMoveTick = tick;
		}
		return;
	}

	int attackDelay = lpInfo->AttackDelay;
	int agilityReduction = (lpBot->Dexterity / 6);
	attackDelay -= agilityReduction;
	if(attackDelay < 250) { attackDelay = 250; }
	if(attackDelay > lpInfo->AttackDelay) { attackDelay = lpInfo->AttackDelay; }

	if(tick - lpInfo->LastAttackTick < (DWORD)attackDelay)
	{
		return;
	}

	lpBot->Dir = GetPathPacketDirPos(gObj[target].X-lpBot->X,gObj[target].Y-lpBot->Y);
	CSkill* lpSkill = ((lpInfo->SkillNumber > 0) ? gSkillManager.GetSkill(lpBot,lpInfo->SkillNumber) : 0);
	GCActionSend(lpBot,ACTION_ATTACK1,lpBot->Index,target);
	gAttack.Attack(lpBot,&gObj[target],lpSkill,1,0,0,0,0);
	lpInfo->LastAttackTick = tick;
}

void CBotPlayer::MoveBot(BOTPLAYER_INFO* lpInfo, LPOBJ lpBot)
{
	if(lpInfo->MoveEnabled == 0 || lpInfo->RouteCount <= 0 || lpInfo->RouteCount > MAX_BOTPLAYER_ROUTE)
	{
		return;
	}

	DWORD tick = GetTickCount();
	if(tick - lpInfo->LastMoveTick < (DWORD)lpInfo->MoveDelay)
	{
		return;
	}

	if (lpInfo->CurrentRoute < 0 || lpInfo->CurrentRoute >= lpInfo->RouteCount)
		lpInfo->CurrentRoute = 0;
	BOTPLAYER_ROUTE* route = &lpInfo->route[lpInfo->CurrentRoute];
	if (lpBot->X == route->x && lpBot->Y == route->y)
	{
		lpInfo->CurrentRoute = (lpInfo->CurrentRoute+1) % lpInfo->RouteCount;
		route = &lpInfo->route[lpInfo->CurrentRoute];
	}
	if (lpBot->PathCur < lpBot->PathCount && lpBot->MTX == route->x && lpBot->MTY == route->y)
		return;
	lpBot->MTX = route->x;
	lpBot->MTY = route->y;
	lpBot->ActionState.Move = 1;
	PathFindMoveMsgSend(lpBot);
	lpInfo->LastMoveTick = tick;
}

void CBotPlayer::TalkBot(BOTPLAYER_INFO* lpInfo, LPOBJ lpBot)
{
	if(lpInfo->TalkEnabled == 0 || lpInfo->MessageCount <= 0)
	{
		return;
	}

	DWORD tick = GetTickCount();
	if(tick - lpInfo->LastTalkTick < (DWORD)lpInfo->TalkDelay)
	{
		return;
	}

	this->SendBotChat(lpBot,lpInfo->message[lpInfo->CurrentMessage]);
	lpInfo->CurrentMessage = ((lpInfo->CurrentMessage+1) >= lpInfo->MessageCount) ? 0 : (lpInfo->CurrentMessage+1);
	lpInfo->LastTalkTick = tick;
}


void CBotPlayer::PotionBot(BOTPLAYER_INFO* lpInfo, LPOBJ lpBot)
{
	if(lpInfo->PotionEnabled == 0 || lpBot->MaxLife + lpBot->AddLife <= 0)
	{
		return;
	}

	DWORD tick = GetTickCount();
	if(tick - lpInfo->LastPotionTick < (DWORD)lpInfo->PotionDelay)
	{
		return;
	}

	const float maxLife = lpBot->MaxLife + lpBot->AddLife;
	int lifePercent = (int)((lpBot->Life * 100.0f) / maxLife);
	if(lifePercent > lpInfo->PotionPercent)
	{
		// Synthetic BotPlayer uses its configured recovery budget, unlike the
		// real-account FakeBoot which consumes inventory potions.
		const float maxMana = lpBot->MaxMana + lpBot->AddMana;
		if (maxMana > 0 && lpBot->Mana * 100.0f / maxMana < 40.0f)
		{
			float recoverMana = maxMana * lpInfo->PotionRecoverPercent / 100.0f;
			if (recoverMana < 1.0f) recoverMana = 1.0f;
			lpBot->Mana += recoverMana;
			if (lpBot->Mana > maxMana) lpBot->Mana = maxMana;
			GCManaSend(lpBot->Index, 0xFF, (int)lpBot->Mana, lpBot->BP);
			lpInfo->LastPotionTick = tick;
		}
		return;
	}

	int recover = (int)((maxLife * lpInfo->PotionRecoverPercent) / 100);
	if(recover < 1)
	{
		recover = 1;
	}

	lpBot->Life += recover;
	if(lpBot->Life > maxLife)
	{
		lpBot->Life = maxLife;
	}

	if(lpBot->MaxShield > 0)
	{
		lpBot->Shield += (recover/2);
		if(lpBot->Shield > lpBot->MaxShield)
		{
			lpBot->Shield = lpBot->MaxShield;
		}
	}

	GCLifeSend(lpBot->Index,0xFF,(int)lpBot->Life,lpBot->Shield);
	lpInfo->LastPotionTick = tick;
}

void CBotPlayer::BuffBot(BOTPLAYER_INFO* lpInfo, LPOBJ lpBot)
{
	if(lpInfo->BuffEnabled == 0 || lpInfo->BuffSkillNumber <= 0)
	{
		return;
	}

	// Dentro da cidade/safe zone o bot nao usa buff automatico.
	if(gMap[lpBot->Map].CheckAttr(lpBot->X,lpBot->Y,1) != 0)
	{
		return;
	}

	DWORD tick = GetTickCount();
	int delay = ((lpInfo->BuffDelay < 1000) ? 1000 : lpInfo->BuffDelay);

	if(tick - lpInfo->LastBuffTick < (DWORD)delay)
	{
		return;
	}

	// Failed/no-target attempts also wait, avoiding a full scan every 100ms.
	lpInfo->LastBuffTick = tick;
	int target = this->FindBuffTarget(lpInfo,lpBot);

	if(OBJECT_RANGE(target) == 0 || gObjIsConnected(target) == 0)
	{
		return;
	}

	if(this->ApplyBotBuff(lpInfo,lpBot,&gObj[target]) != 0)
	{
		lpInfo->LastBuffTick = tick;
	}
}

int CBotPlayer::FindBuffTarget(BOTPLAYER_INFO* lpInfo, LPOBJ lpBot)
{
	if(lpInfo->BuffTargetMode == 0)
	{
		return lpBot->Index;
	}

	int range = ((lpInfo->BuffRange <= 0) ? 6 : lpInfo->BuffRange);
	int bestTarget = -1;
	int bestDistance = 999;

	for(int n=OBJECT_START_USER;n<MAX_OBJECT;n++)
	{
		if(gObjIsConnectedGP(n) == 0 || gObj[n].Map != lpBot->Map || gObj[n].Live == 0
			|| gMap[gObj[n].Map].CheckAttr(gObj[n].X,gObj[n].Y,1) != 0)
		{
			continue;
		}

		int distance = (int)sqrt((float)(((lpBot->X-gObj[n].X)*(lpBot->X-gObj[n].X))+((lpBot->Y-gObj[n].Y)*(lpBot->Y-gObj[n].Y))));

		if(distance <= range && distance < bestDistance)
		{
			bestTarget = n;
			bestDistance = distance;
		}
	}

	return ((bestTarget >= 0) ? bestTarget : lpBot->Index);
}

bool CBotPlayer::ApplyBotBuff(BOTPLAYER_INFO* lpInfo, LPOBJ lpBot, LPOBJ lpTarget)
{
	if(gMap[lpBot->Map].CheckAttr(lpBot->X,lpBot->Y,1) != 0 ||
		gMap[lpTarget->Map].CheckAttr(lpTarget->X,lpTarget->Y,1) != 0)
	{
		return false;
	}

	int power = ((lpInfo->BuffPower <= 0) ? 50 : lpInfo->BuffPower);
	int time = ((lpInfo->BuffTime <= 0) ? 60 : lpInfo->BuffTime);
	int duration = time * 60;

	GCActionSend(lpBot,ACTION_RESPECT1,lpBot->Index,lpTarget->Index);

	switch(lpInfo->BuffSkillNumber)
	{
		case SKILL_MANA_SHIELD:
		{
			gSkillManager.GCSkillAttackSend(lpBot,SKILL_MANA_SHIELD,lpTarget->Index,1);
			GCManaSend(lpTarget->Index,0xFF,(int)lpTarget->Mana,lpTarget->BP);
			gEffectManager.AddEffect(lpTarget,1,EFFECT_MANA_SHIELD,duration,power,0,0,0);
			return true;
		}
		case SKILL_GREATER_LIFE:
		{
			gSkillManager.GCSkillAttackSend(lpBot,SKILL_GREATER_LIFE,lpTarget->Index,1);
			GCLifeSend(lpTarget->Index,0xFF,(int)lpTarget->Life,lpTarget->Shield);
			gEffectManager.AddEffect(lpTarget,1,EFFECT_GREATER_LIFE,duration,power,0,0,0);
			return true;
		}
		case SKILL_GREATER_DEFENSE:
		{
			gSkillManager.GCSkillAttackSend(lpBot,SKILL_GREATER_DEFENSE,lpTarget->Index,1);
			gEffectManager.AddEffect(lpTarget,1,EFFECT_GREATER_DEFENSE,duration,power,0,0,0);
			return true;
		}
		case SKILL_GREATER_DAMAGE:
		{
			gSkillManager.GCSkillAttackSend(lpBot,SKILL_GREATER_DAMAGE,lpTarget->Index,1);
			gEffectManager.AddEffect(lpTarget,1,EFFECT_GREATER_DAMAGE,duration,power,0,0,0);
			return true;
		}
		case SKILL_GREATER_CRITICAL_DAMAGE:
		{
			gSkillManager.GCSkillAttackSend(lpBot,SKILL_GREATER_CRITICAL_DAMAGE,lpTarget->Index,1);
			gEffectManager.AddEffect(lpTarget,1,EFFECT_GREATER_CRITICAL_DAMAGE,duration,power,0,0,0);
			return true;
		}
		case SKILL_DAMAGE_REFLECT:
		{
			gSkillManager.GCSkillAttackSend(lpBot,SKILL_DAMAGE_REFLECT,lpTarget->Index,1);
			gEffectManager.AddEffect(lpTarget,1,EFFECT_DAMAGE_REFLECT,duration,power,0,0,0);
			return true;
		}
	}

	return false;
}
void CBotPlayer::SendBotChat(LPOBJ lpBot, char* message)
{
	for(int n=OBJECT_START_USER;n<MAX_OBJECT;n++)
	{
		if(gObjIsConnectedGP(n) == 0 || gObj[n].Map != lpBot->Map)
		{
			continue;
		}
		int distance = (int)sqrt((float)(((lpBot->X-gObj[n].X)*(lpBot->X-gObj[n].X))+((lpBot->Y-gObj[n].Y)*(lpBot->Y-gObj[n].Y))));
		if(distance <= 15)
		{
			GCChatTargetSend(&gObj[n],lpBot->Index,message);
		}
	}
}



bool CBotPlayer::CanAttackBot(int aIndex)
{
	for(int n=0;n<this->BotCount;n++)
	{
		if(this->bot[n].Enabled != 0 && this->bot[n].index == aIndex)
		{
			return (this->bot[n].CanBeAttacked != 0);
		}
	}
	return false;
}

int CBotPlayer::SpawnBots(int count,int map,int x,int y,int dir,int classFilter)
{
	if(this->Enabled == 0)
	{
		return -1;
	}

	if(count < 1) { count = 1; }
	if(count > MAX_BOTPLAYER) { count = MAX_BOTPLAYER; }

	// Keep template snapshots off the small Windows thread stack.
	std::vector<BOTPLAYER_INFO> templateList(MAX_BOTPLAYER);
	int templateCount = 0;

	int scanCount = this->TemplateCount;
	if(scanCount <= 0 || scanCount > this->BotCount)
	{
		scanCount = this->BotCount;
	}
	if(scanCount > MAX_BOTPLAYER)
	{
		scanCount = MAX_BOTPLAYER;
	}

	for(int n=0;n<scanCount;n++)
	{
		if(this->bot[n].Enabled == 0)
		{
			continue;
		}

		if(classFilter >= 0 && BotPlayerGetBaseClass(this->bot[n].Class) != classFilter)
		{
			continue;
		}

		templateList[templateCount++] = this->bot[n];
	}

	if(templateCount <= 0)
	{
		LogAdd(LOG_RED,"[BotPlayer] /boot sem modelo. Enabled=%d BotCount=%d TemplateCount=%d Scan=%d ClassFilter=%d Slot0(E=%d C=%d) Slot1(E=%d C=%d) Slot2(E=%d C=%d)",
			this->Enabled,this->BotCount,this->TemplateCount,scanCount,classFilter,
			this->bot[0].Enabled,this->bot[0].Class,this->bot[1].Enabled,this->bot[1].Class,this->bot[2].Enabled,this->bot[2].Class);
		return -2;
	}

	int created = 0;
	bool capacityFull = false;
	bool allocationFailed = false;

	int randomOrder[MAX_BOTPLAYER];
	for(int n=0;n<templateCount;n++)
	{
		randomOrder[n] = n;
	}

	for(int n=templateCount;n>1;n--)
	{
		int randomIndex = (GetLargeRand()%n);
		int temp = randomOrder[n-1];
		randomOrder[n-1] = randomOrder[randomIndex];
		randomOrder[randomIndex] = temp;
	}

	for(int n=0;n<count;n++)
	{
		int slot = this->BotCount;
		// Preserve the configured templates; recycle removed dynamic bots.
		for (int freeSlot = this->TemplateCount; freeSlot < this->BotCount; ++freeSlot)
		{
			if (!OBJECT_RANGE(this->bot[freeSlot].index) ||
				!gObjIsConnected(this->bot[freeSlot].index))
			{
				slot = freeSlot;
				break;
			}
		}
		if (slot >= MAX_BOTPLAYER) { capacityFull = true; break; }
		const int previousBotCount = this->BotCount;
		int templateIndex = randomOrder[n%templateCount];

		// A cada ciclo completo, muda novamente a ordem das classes/modelos.
		if(n > 0 && (n%templateCount) == 0)
		{
			for(int i=templateCount;i>1;i--)
			{
				int randomIndex = (GetLargeRand()%i);
				int temp = randomOrder[i-1];
				randomOrder[i-1] = randomOrder[randomIndex];
				randomOrder[randomIndex] = temp;
			}
			templateIndex = randomOrder[0];
		}

		this->bot[slot] = templateList[templateIndex];
		this->bot[slot].Enabled = 1;
		this->bot[slot].index = -1;
		this->bot[slot].Map = (BYTE)map;
		this->bot[slot].X = (BYTE)(x + (n%5));
		this->bot[slot].Y = (BYTE)(y + ((n/5)%5));
		this->bot[slot].Dir = (BYTE)dir;
		this->bot[slot].LastAttackTick = 0;
		this->bot[slot].LastMoveTick = 0;
		this->bot[slot].LastTalkTick = 0;
		this->bot[slot].LastPotionTick = 0;
		this->bot[slot].LastBuffTick = 0;
		this->bot[slot].CurrentRoute = 0;
		this->bot[slot].CurrentMessage = 0;
		sprintf_s(this->bot[slot].Name,sizeof(this->bot[slot].Name),"%.6s%04d",templateList[templateIndex].Name,slot+1);

		if (slot >= this->BotCount) this->BotCount = slot + 1;

		if(this->CreateBot(slot) != 0)
		{
			created++;
			if(g_PvPRegistrationOpen != false)
			{
				gPvPTournament.JoinPlayer(&gObj[this->bot[slot].index]);
			}
		}
		else
		{
			this->bot[slot].Enabled = 0;
			this->bot[slot].index = -1;
			this->BotCount = previousBotCount;
			allocationFailed = true;
			break;
		}
	}

	if(created == 0 && allocationFailed) return -4;
	if(created == 0 && capacityFull)
	{
		return -3;
	}

	return created;
}

void CBotPlayer::JoinOpenPvPTournament()
{
	if(g_PvPRegistrationOpen == false)
	{
		return;
	}

	for(int n=0;n<this->BotCount;n++)
	{
		if(this->bot[n].Enabled != 0 && OBJECT_RANGE(this->bot[n].index) != 0 && gObjIsConnected(this->bot[n].index) != 0)
		{
			gPvPTournament.JoinPlayer(&gObj[this->bot[n].index]);
		}
	}
}

int CBotPlayer::KillBots(int count)
{
	int killed = 0;

	for(int n=0;n<this->BotCount;n++)
	{
		if(count > 0 && killed >= count)
		{
			break;
		}

		if(this->bot[n].index >= 0 && gObjIsConnected(this->bot[n].index) != 0)
		{
			BotPlayerLeaveParty(this->bot[n].index);
			gObjDel(this->bot[n].index);
			this->bot[n].index = -1;
			killed++;
		}
	}

	return killed;
}

int CBotPlayer::FindNearestBot(int aIndex)
{
	if(this->Enabled == 0 || gObjIsConnectedGP(aIndex) == 0)
	{
		return -1;
	}

	int best = -1;
	int bestDistance = 9999;
	LPOBJ lpObj = &gObj[aIndex];

	for(int n=0;n<this->BotCount;n++)
	{
		int index = this->bot[n].index;
		if(this->bot[n].Enabled == 0 || OBJECT_RANGE(index) == 0 || gObjIsConnected(index) == 0 || gObj[index].Live == 0)
		{
			continue;
		}

		if(gObj[index].Map != lpObj->Map)
		{
			continue;
		}

		int distance = (int)sqrt((float)(((lpObj->X-gObj[index].X)*(lpObj->X-gObj[index].X))+((lpObj->Y-gObj[index].Y)*(lpObj->Y-gObj[index].Y))));
		if(distance < bestDistance)
		{
			best = index;
			bestDistance = distance;
		}
	}

	return best;
}

int CBotPlayer::FindBotByName(char* name)
{
	if(this->Enabled == 0 || name == 0 || name[0] == 0)
	{
		return -1;
	}

	for(int n=0;n<this->BotCount;n++)
	{
		int index = this->bot[n].index;
		if(this->bot[n].Enabled != 0 && OBJECT_RANGE(index) != 0 && gObjIsConnected(index) != 0 && _stricmp(gObj[index].Name,name) == 0)
		{
			return index;
		}
	}

	return -1;
}

bool CBotPlayer::IsBotPlayer(int aIndex)
{
	for(int n=0;n<this->BotCount;n++)
	{
		if(this->bot[n].Enabled != 0 && this->bot[n].index == aIndex)
		{
			return true;
		}
	}
	return false;
}

#endif
















