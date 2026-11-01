#include "stdafx.h"
#include "TextClien.h"
#include "stdafx.h"
#include "NewUIHuntLog.h"
#include "ZzzInterface.h"
#include "ZzzCharacter.h"
#include "ZzzInfomation.h"
#include "NewUISystem.h"
#include "WSclient.h"
#include "ZzzInventory.h"
#include "CGMProtect.h"

using namespace SEASON3B;
CNewUIHuntLog SEASON3B::g_HuntLog;

CNewUIHuntLog::CNewUIHuntLog() : m_TargetKey(-1), m_LastDamage(0), m_TotalDamage(0), m_ServerLife(0), m_ServerMaxLife(0), m_Zen(0), m_LastRequest(0), m_ServerDamageContribution(0), m_IsBoss(false), m_Experience(0), m_LastRealExperience(0), m_LastRealZen(0), m_LastKillTick(0), m_LastKillX(0), m_LastKillY(0), m_LastDropType(-1), m_CurrentMonsterClass(-1), m_LastKilledMonsterClass(-1), m_DropCount(0), m_CommonPoolCount(0) { ZeroMemory(m_Drops,sizeof(m_Drops)); }
void CNewUIHuntLog::Clear() { m_TargetKey = -1; m_LastDamage = 0; m_TotalDamage = 0; m_ServerLife = m_ServerMaxLife = m_Zen = 0; m_IsBoss = false; m_DropCount = m_CommonPoolCount = 0; }

void CNewUIHuntLog::SendRequest(WORD targetIndex) { PMSG_HUNT_LOG_REQUEST_SEND msg; msg.header.set(0xF3,0xF2,sizeof(msg)); msg.TargetIndex=targetIndex; DataSend((BYTE*)&msg,sizeof(msg)); m_LastRequest=timeGetTime(); }
void CNewUIHuntLog::ReceiveInfo(PMSG_HUNT_LOG_INFO_RECV* info) { if (!info || info->TargetIndex != m_TargetKey) return; m_IsBoss=(info->IsBoss!=0); if (!m_IsBoss) { m_DropCount=m_CommonPoolCount=0; return; } m_CurrentMonsterClass=info->MonsterClass; m_ServerLife=info->Life; m_ServerMaxLife=info->MaxLife; m_Zen=info->Zen; m_Experience=info->Experience; m_ServerDamageContribution=info->DamageContribution; m_DropCount=min((BYTE)HUNT_LOG_MAX_DROPS,info->DropCount); m_CommonPoolCount=info->CommonPoolCount; memcpy(m_Drops,info->Drops,sizeof(m_Drops)); }

void CNewUIHuntLog::OnMonsterDamage(CHARACTER* target, unsigned int damage)
{
	if (!target || target->Object.Kind != KIND_MONSTER) return;
	if (m_TargetKey != target->Key) { m_TargetKey = target->Key; m_TotalDamage = 0; }
	m_LastDamage = damage;
	m_TotalDamage += damage;
}

void CNewUIHuntLog::OnMonsterKilled(CHARACTER* target, unsigned int experience)
{
	if (!target || target->Object.Kind != KIND_MONSTER) return;
	m_LastRealExperience=experience; m_LastRealZen=0; m_LastDropType=-1; m_LastKillTick=timeGetTime();
	m_LastKilledMonsterClass=target->MonsterIndex;
	m_LastKillX=target->PositionX; m_LastKillY=target->PositionY;
}

void CNewUIHuntLog::OnGroundDrop(int itemType, unsigned int value, int x, int y)
{
	if (!m_LastKillTick || timeGetTime()-m_LastKillTick>2500 || abs(x-m_LastKillX)>3 || abs(y-m_LastKillY)>3) return;
	if (itemType==ITEM_POTION+15) m_LastRealZen=value; else m_LastDropType=itemType;
}

void CNewUIHuntLog::Render()
{
	if (!gmProtect->HuntLogEnable) { Clear(); return; }
	if (SelectedCharacter < 0) { Clear(); return; }
	CHARACTER* target = gmCharacters->GetCharacter(SelectedCharacter);
	if (!target || !target->GetLive() || target->Object.Kind != KIND_MONSTER) { Clear(); return; }
	if (m_TargetKey != target->Key) { m_TargetKey = target->Key; m_LastDamage = 0; m_TotalDamage = 0; m_IsBoss=false; m_DropCount=m_CommonPoolCount=0; m_LastRequest=0; }
	if (timeGetTime()-m_LastRequest >= 1000) SendRequest((WORD)target->Key);
	if (!m_IsBoss) return;

	const float width = 235.0f, height = 190.0f + (float)(min((int)m_DropCount,4)*14);
	const float x = (float)GetWindowsX - width - 18.0f, y = 185.0f;
	EnableAlphaTest();
	glColor4f(0.02f, 0.02f, 0.02f, 0.78f); RenderColor(x, y, width, height); EndRenderColor();
	glColor4f(0.55f, 0.12f, 0.08f, 0.95f); RenderColor(x, y, width, 20.0f); EndRenderColor();
	glColor4f(1.f, 1.f, 1.f, 1.f);

	char text[256];
	g_pRenderText->SetFont(g_hFontBold); g_pRenderText->SetBgColor(0, 0, 0, 0); g_pRenderText->SetTextColor(255, 220, 120, 255);
	sprintf_s(text, gTextClien.Select("%s - Nivel %u", "%s - Level %u", "%s - Nivel %u"), getMonsterName(target->MonsterIndex), (unsigned int)target->MonsterLevel);
	g_pRenderText->RenderText((int)x + 6, (int)y + 4, text, (int)width - 12, 14, RT3_SORT_CENTER);
	g_pRenderText->SetFont(g_hFont); g_pRenderText->SetTextColor(240, 240, 240, 255);
	DWORD life=m_ServerMaxLife?m_ServerLife:(DWORD)target->m_LifeCur, maxLife=m_ServerMaxLife?m_ServerMaxLife:(DWORD)target->m_LifeMax;
	sprintf_s(text, "HP: %u / %u", life, maxLife); g_pRenderText->RenderText((int)x + 10, (int)y + 28, text);
	sprintf_s(text, gTextClien.Select("Ultimo dano: %u", "Last damage: %u", "Ultimo dano: %u"), m_LastDamage); g_pRenderText->RenderText((int)x + 10, (int)y + 44, text);
	sprintf_s(text, gTextClien.Select("Seu dano no servidor: %u", "Your server damage: %u", "Tu dano en el servidor: %u"), m_ServerDamageContribution); g_pRenderText->RenderText((int)x + 10, (int)y + 60, text);
	g_pRenderText->SetTextColor(255, 215, 90, 255); if(m_LastRealZen) sprintf_s(text,gTextClien.Select("Ultimo Zen real: %u", "Last Zen received: %u", "Ultimo Zen recibido: %u"),m_LastRealZen); else sprintf_s(text,gTextClien.Select("Zen: definido no sorteio da morte", "Zen: rolled on death", "Zen: sorteado al morir")); g_pRenderText->RenderText((int)x+10,(int)y+79,text);
	g_pRenderText->SetTextColor(170,220,255,255); sprintf_s(text,gTextClien.Select("EXP acumulada agora: %llu", "Current accumulated EXP: %llu", "EXP acumulada actual: %llu"),m_Experience); g_pRenderText->RenderText((int)x+10,(int)y+95,text);
	g_pRenderText->SetTextColor(150,255,170,255); if(m_LastKillTick && m_LastKilledMonsterClass==m_CurrentMonsterClass) sprintf_s(text,gTextClien.Select("EXP real deste mob: %llu", "EXP received from this monster: %llu", "EXP recibida de este monstruo: %llu"),m_LastRealExperience); else sprintf_s(text,gTextClien.Select("EXP real: mate este tipo de mob", "EXP: defeat this monster type", "EXP: derrota este tipo de monstruo")); g_pRenderText->RenderText((int)x+10,(int)y+111,text);
	g_pRenderText->SetTextColor(190, 220, 255, 255); g_pRenderText->RenderText((int)x+10,(int)y+129,gTextClien.Select("Drops especiais configurados:", "Configured special drops:", "Drops especiales configurados:"));
	if (!m_DropCount) { g_pRenderText->SetTextColor(170,170,170,255); g_pRenderText->RenderText((int)x+18,(int)y+146,gTextClien.Select("Nenhum drop especial", "No special drops", "Sin drops especiales")); }
	for (int n=0;n<m_DropCount && n<4;n++) { char itemName[100]={0}; GetItemName(m_Drops[n].ItemIndex,0,itemName); g_pRenderText->SetTextColor(235,235,235,255); if(m_Drops[n].Rate) sprintf_s(text,"- %s (%.2f%%)",itemName,m_Drops[n].Rate/100.0f); else sprintf_s(text,"- %s",itemName); g_pRenderText->RenderText((int)x+18,(int)y+146+n*14,text,(int)width-26); }
	int lineY=146+min((int)m_DropCount,4)*14; if (m_DropCount>4) { sprintf_s(text,gTextClien.Select("+ %d especiais", "+ %d special drops", "+ %d especiales"),m_DropCount-4); g_pRenderText->SetTextColor(170,170,170,255); g_pRenderText->RenderText((int)x+18,(int)y+lineY,text); lineY+=14; }
	sprintf_s(text,gTextClien.Select("Pool comum aleatorio: %u itens", "Random common pool: %u items", "Grupo comun aleatorio: %u objetos"),(unsigned)m_CommonPoolCount); g_pRenderText->SetTextColor(255,190,110,255); g_pRenderText->RenderText((int)x+10,(int)y+lineY,text,(int)width-18);
	if (m_LastDropType>=0) { char lastName[100]={0}; GetItemName(m_LastDropType,0,lastName); sprintf_s(text,gTextClien.Select("Ultimo drop real: %s", "Last item received: %s", "Ultimo objeto recibido: %s"),lastName); g_pRenderText->SetTextColor(130,255,210,255); g_pRenderText->RenderText((int)x+10,(int)y+lineY+14,text,(int)width-18); }
}
