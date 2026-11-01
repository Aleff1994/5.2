// NewUIBossRanking.cpp
// Ranking de dano de Boss - aparece so durante o ataque, some 3s depois
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "NewUIBossRanking.h"

extern char* getMonsterName(int type);

using namespace SEASON3B;

CNewUIBossRanking* SEASON3B::g_pBossRanking = NULL;

#define BOSS_RANKING_HIDE_DELAY   3000	//. 3 segundos sem receber pacote novo = esconde

SEASON3B::CNewUIBossRanking::CNewUIBossRanking()
{
	m_bActive = false;
	m_byMonsterClass = 0;
	m_byCount = 0;
	m_dwShowUntil = 0;

	m_PosX = 0.0f;
	m_PosY = 0.0f;
	m_bPosInitialized = false;

	m_bDragging = false;
	m_DragOffsetX = 0.0f;
	m_DragOffsetY = 0.0f;
}

SEASON3B::CNewUIBossRanking::~CNewUIBossRanking()
{
}

void SEASON3B::CNewUIBossRanking::ReceiveRanking(WORD MonsterClass, BYTE Count, BOSS_RANKING_ENTRY* pEntries)
{
	//. Cada pacote novo SUBSTITUI completamente o que estava sendo
	//. mostrado - mesmo se for de um boss diferente do que estava
	//. exibido antes. Isso garante: "trocar de boss e atacar o novo
	//. -> mostra o ranking do novo boss imediatamente".
	m_byMonsterClass = MonsterClass;
	m_byCount = (Count > MAX_BOSS_RANKING) ? MAX_BOSS_RANKING : Count;

	for (int i = 0; i < MAX_BOSS_RANKING; i++)
	{
		if (i < m_byCount)
			m_Entries[i] = pEntries[i];
		else
			memset(&m_Entries[i], 0, sizeof(BOSS_RANKING_ENTRY));
	}

	m_bActive = true;
	m_dwShowUntil = timeGetTime() + BOSS_RANKING_HIDE_DELAY;	//. renova os 3s

	if (!m_bPosInitialized)
	{
		const float PanelW = 220.0f;
		m_PosX = (float)GetWindowsX - PanelW - 10.0f;
		m_PosY = 20.0f;
		m_bPosInitialized = true;
	}
}

static void RenderTextWithShadow(int x, int y, const char* psz, int width, int height, int sort,
	BYTE r, BYTE g, BYTE b, BYTE a)
{
	BYTE shadowAlpha = (BYTE)((int)a * 70 / 100);
	g_pRenderText->SetTextColor(0, 0, 0, shadowAlpha);
	g_pRenderText->RenderText(x + 1, y + 1, (char*)psz, width, height, sort);

	g_pRenderText->SetTextColor(r, g, b, a);
	g_pRenderText->RenderText(x, y, (char*)psz, width, height, sort);
}

bool SEASON3B::CNewUIBossRanking::UpdateMouseEvent()
{
	if (!m_bActive)
		return false;

	//. Sem novo ReceiveRanking ha mais de 3s (parou de atacar, boss
	//. morreu, ou boss teleportou e saiu do viewport) -> esconde.
	//. Nao esconde se estiver sendo arrastado nesse instante.
	if (timeGetTime() > m_dwShowUntil && !m_bDragging)
	{
		m_bActive = false;
		return false;
	}

	if (m_byCount == 0)
		return false;

	const float EntryH = 40.0f;
	const float PanelW = 220.0f;
	const float PanelH = 300.0f;

	// O painel inteiro funciona como area de arraste. Assim o jogador
	// nao precisa acertar somente o titulo para reposicionar o ranking.
	if (SEASON3B::IsPress(VK_LBUTTON) && SEASON3B::CheckMouseIn(m_PosX, m_PosY, PanelW, PanelH) && !m_bDragging)
	{
		m_bDragging = true;
		m_DragOffsetX = MouseX - m_PosX;
		m_DragOffsetY = MouseY - m_PosY;
		return true;
	}
	else if (m_bDragging && MouseLButton)
	{
		float newX = MouseX - m_DragOffsetX;
		float newY = MouseY - m_DragOffsetY;

		if (newX < 0) newX = 0;
		if (newY < 0) newY = 0;
		if (newX + PanelW > GetWindowsX) newX = (float)GetWindowsX - PanelW;
		if (newY + PanelH > GetWindowsY) newY = (float)GetWindowsY - PanelH;

		m_PosX = newX;
		m_PosY = newY;
		return true;
	}
	else if (!MouseLButton)
	{
		m_bDragging = false;
	}

	return SEASON3B::CheckMouseIn(m_PosX, m_PosY, PanelW, PanelH);
}

void SEASON3B::CNewUIBossRanking::Render()
{
	if (!m_bActive || m_byCount == 0)
		return;

	if (timeGetTime() > m_dwShowUntil && !m_bDragging)
	{
		m_bActive = false;
		return;
	}

	const float EntryH = 40.0f;
	const float PanelW = 220.0f;
	const float HeaderH = 75.0f;
	const float PadX = 22.0f;
	const float PanelH = 300.0f;

	float X = m_PosX;
	float Y = m_PosY;

	//. Fade nos ultimos instantes antes de sumir (so quando nao esta
	//. sendo arrastado)
	float fAlpha = 1.0f;
	if (!m_bDragging)
	{
		DWORD dwRemain = m_dwShowUntil - timeGetTime();
		if (dwRemain < 800)
			fAlpha = (float)dwRemain / 800.0f;
	}

	EnableAlphaTest();
	glColor4f(1.0f, 1.0f, 1.0f, fAlpha);
	RenderBitmap(BITMAP_BOSSRANKING_FRAME, X, Y, PanelW, PanelH,
		0.0f, 0.0f, 1.0f, 1.0f, true, true, 0.0f);
	EnableAlphaTest();

	char szTitle[32];
	char* pszBossName = getMonsterName(m_byMonsterClass);
	sprintf(szTitle, "%s", pszBossName);

	g_pRenderText->SetFont(g_hFontBold);
	g_pRenderText->SetBgColor(0, 0, 0, 0);
	g_pRenderText->SetTextColor(255, 220, 120, (BYTE)(255.0f * fAlpha));
	g_pRenderText->RenderText((int)(X + PadX), (int)(Y + 24), szTitle, (int)(PanelW - PadX * 2), 18, RT3_SORT_CENTER);

	static const DWORD RANK_COLORS[5] =
	{
		0xE8C766, 0xC8C8D0, 0xB08858, 0xA0A0A8, 0xA0A0A8,
	};

	for (int i = 0; i < m_byCount; i++)
	{
		float entryY = Y + HeaderH + 5.0f + i * EntryH;

		char szRank[4];
		sprintf(szRank, "%d", i + 1);

		DWORD col = RANK_COLORS[i];
		BYTE cr = (col >> 16) & 0xFF;
		BYTE cg = (col >> 8) & 0xFF;
		BYTE cb = (col) & 0xFF;

		g_pRenderText->SetFont(g_hFontBold);
		g_pRenderText->SetTextColor(cr, cg, cb, (BYTE)(230.0f * fAlpha));
		g_pRenderText->RenderText((int)(X + PadX), (int)(entryY + 1), szRank, 16, (int)EntryH, RT3_SORT_LEFT);

		g_pRenderText->SetFont(g_hFont);
		g_pRenderText->SetTextColor(210, 210, 215, (BYTE)(220.0f * fAlpha));
		g_pRenderText->RenderText((int)(X + PadX + 18), (int)(entryY + 1), m_Entries[i].Name, 94, (int)EntryH, RT3_SORT_LEFT);

		char szDmg[32];
		DWORD dmg = m_Entries[i].Damage;
		if (dmg >= 1000000)
			sprintf(szDmg, "%u.%03u.%03u", dmg / 1000000, (dmg % 1000000) / 1000, dmg % 1000);
		else if (dmg >= 1000)
			sprintf(szDmg, "%u.%03u", dmg / 1000, dmg % 1000);
		else
			sprintf(szDmg, "%u", dmg);

		g_pRenderText->SetTextColor(cr, cg, cb, (BYTE)(220.0f * fAlpha));
		const int DamageX = (int)(X + 136.0f);
		const int DamageW = (int)(PanelW - 136.0f - PadX);
		g_pRenderText->RenderText(DamageX, (int)(entryY + 1), szDmg, DamageW, (int)EntryH, RT3_SORT_RIGHT);
	}
}
