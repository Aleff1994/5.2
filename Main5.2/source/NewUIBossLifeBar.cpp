// NewUIBossLifeBar.cpp
// Barra de vida de Boss customizada
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "NewUIBossLifeBar.h"
#include <math.h>

extern char* getMonsterName(int type);

using namespace SEASON3B;

CNewUIBossLifeBar* SEASON3B::g_pBossLifeBar = NULL;

// =====================================================================
// LISTA DE BOSSES
// Para adicionar/remover um boss, so editar essa lista.
// O numero e o "MonsterIndex" do monstro (o mesmo numero usado
// no NpcName.txt / monster.bmd do servidor).
// =====================================================================
const int CNewUIBossLifeBar::BOSS_LIST[] =
{
	275, // Kundun
	600, // Astelia
	598, // Medusa
	// 123, // <- exemplo: descomente e troque o numero pra adicionar outro boss
};
const int CNewUIBossLifeBar::BOSS_LIST_COUNT = sizeof(CNewUIBossLifeBar::BOSS_LIST) / sizeof(int);

SEASON3B::CNewUIBossLifeBar::CNewUIBossLifeBar()
{
	m_iMonsterType = -1;
	m_iHPRate = 0;
	m_iSDRate = 0;
	m_bActive = false;

	m_fDisplayHPRate = 0.0f;
	m_iLastHPRate = 100;
	m_fDamageFlash = 0.0f;
	m_dwLastTick = timeGetTime();
}

SEASON3B::CNewUIBossLifeBar::~CNewUIBossLifeBar()
{
}

void SEASON3B::CNewUIBossLifeBar::LoadImages()
{
	// As texturas sao carregadas uma unica vez em OpenInterface().
}

void SEASON3B::CNewUIBossLifeBar::UnloadImages()
{
}

bool SEASON3B::CNewUIBossLifeBar::IsBoss(int iMonsterType)
{
	for (int i = 0; i < BOSS_LIST_COUNT; ++i)
	{
		if (BOSS_LIST[i] == iMonsterType)
			return true;
	}
	return false;
}

void SEASON3B::CNewUIBossLifeBar::SetBossInfo(int iMonsterType, int iHPRate, int iSDRate)
{
	if (!IsBoss(iMonsterType) || iHPRate <= 0)
	{
		m_bActive = false;
		return;
	}

	// Detecta perda de HP para disparar o "flash de dano"
	if (m_bActive && m_iMonsterType == iMonsterType && iHPRate < m_iLastHPRate)
		m_fDamageFlash = 1.0f;

	// Se o boss acabou de aparecer (ou troca de boss), comeca a animacao do valor atual
	if (!m_bActive || m_iMonsterType != iMonsterType)
		m_fDisplayHPRate = (float)iHPRate;

	m_iMonsterType = iMonsterType;
	m_iLastHPRate = iHPRate;
	m_iHPRate = iHPRate;
	m_iSDRate = iSDRate;
	m_bActive = true;
}

void SEASON3B::CNewUIBossLifeBar::ClearBossInfo()
{
	m_bActive = false;
}

void SEASON3B::CNewUIBossLifeBar::Render()
{
	if (!m_bActive || m_iHPRate <= 0)
		return;

	// === Delta time ===
	DWORD dwNow = timeGetTime();
	float fDelta = (dwNow - m_dwLastTick) / 1000.0f;
	if (fDelta < 0.0f) fDelta = 0.0f;
	if (fDelta > 0.1f) fDelta = 0.1f;
	m_dwLastTick = dwNow;

	// Anima suavemente o valor exibido até o HP real
	float fSpeed = 8.0f * fDelta;
	if (fSpeed > 1.0f) fSpeed = 1.0f;
	m_fDisplayHPRate += ((float)m_iHPRate - m_fDisplayHPRate) * fSpeed;

	// Flash de dano decai com o tempo
	if (m_fDamageFlash > 0.0f)
	{
		m_fDamageFlash -= fDelta * 3.0f;
		if (m_fDamageFlash < 0.0f) m_fDamageFlash = 0.0f;
	}

	float HpPercent = m_fDisplayHPRate / 100.0f;
	if (HpPercent < 0.0f) HpPercent = 0.0f;
	if (HpPercent > 1.0f) HpPercent = 1.0f;

	// === Pisca a barra inteira quando o boss esta quase morrendo (<15%) ===
	float fBlink = 1.0f;
	if (HpPercent < 0.15f)
		fBlink = 0.35f + (sinf((float)dwNow * 0.015f) + 1.0f) * 0.325f; // ~0.35 a 1.0

	// === Dimensoes e posicao da arte ===
	const float BW = 360.0f;
	const float BH = 90.0f;
	const float FillX = 36.0f;
	const float FillY = 33.0f;
	const float FillW = 288.0f;
	const float FillH = 38.0f;

	float X = ((float)GetWindowsX / 2.0f) - (BW / 2.0f);
	float Y = 42.0f;

	EnableAlphaTest();
	glColor4f(1.0f, 1.0f, 1.0f, fBlink);

	if (HpPercent > 0.0f)
	{
		RenderBitmap(BITMAP_BOSSLIFE_FILL, X + FillX, Y + FillY,
			FillW * HpPercent, FillH, 0.0f, 0.0f, HpPercent, 1.0f, true, true, 0.0f);
	}

	RenderBitmap(BITMAP_BOSSLIFE_FRAME, X, Y, BW, BH, 0.0f, 0.0f, 1.0f, 1.0f, true, true, 0.0f);
	EnableAlphaTest();

	// === Texto ===
	char* pszName = getMonsterName(m_iMonsterType);

	g_pRenderText->SetFont(g_hFontBold);
	g_pRenderText->SetBgColor(0, 0, 0, 0);

	// Nome (sombra + dourado), acima da barra
	g_pRenderText->SetTextColor(0, 0, 0, (BYTE)(200.0f * fBlink));
	g_pRenderText->RenderText((int)(X + 1), (int)(Y - 11), pszName, (int)BW, 16, RT3_SORT_CENTER);

	g_pRenderText->SetTextColor(255, 220, 120, (BYTE)(255.0f * fBlink));
	g_pRenderText->RenderText((int)X, (int)(Y - 12), pszName, (int)BW, 16, RT3_SORT_CENTER);

	// Porcentagem, dentro da barra
	char szPercent[16];
	sprintf(szPercent, "%d%%", m_iHPRate);

	g_pRenderText->SetTextColor(0, 0, 0, (BYTE)(200.0f * fBlink));
	g_pRenderText->RenderText((int)(X + 1), (int)(Y + 45), szPercent, (int)BW, 14, RT3_SORT_CENTER);

	g_pRenderText->SetTextColor(255, 255, 255, (BYTE)(255.0f * fBlink));
	g_pRenderText->RenderText((int)X, (int)(Y + 44), szPercent, (int)BW, 14, RT3_SORT_CENTER);
}
