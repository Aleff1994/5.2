// NewUIBossRanking.h
// Ranking de dano de Boss - aparece so durante o ataque
//////////////////////////////////////////////////////////////////////
#pragma once
#include "NewUIBase.h"
#define MAX_BOSS_RANKING 5

namespace SEASON3B
{
	struct BOSS_RANKING_ENTRY
	{
		char  Name[10];
		DWORD Damage;
	};

	class CNewUIBossRanking
	{
	public:
		CNewUIBossRanking();
		~CNewUIBossRanking();

		// Chamado quando recebe o pacote do servidor (so chega
		// enquanto alguem esta causando dano no boss). Cada chamada
		// "renova" a exibicao por mais 3 segundos.
		void ReceiveRanking(WORD MonsterClass, BYTE Count, BOSS_RANKING_ENTRY* pEntries);

		// Chamado todo frame, antes do Render (drag + checagem do timeout)
		bool UpdateMouseEvent();

		// Chamado todo frame no loop de render
		void Render();

	private:
		bool               m_bActive;
		WORD               m_byMonsterClass;
		BYTE               m_byCount;
		BOSS_RANKING_ENTRY m_Entries[MAX_BOSS_RANKING];
		DWORD              m_dwShowUntil;	// timestamp ate quando mostrar (renovado a cada ReceiveRanking)

		float  m_PosX;
		float  m_PosY;
		bool   m_bPosInitialized;

		bool   m_bDragging;
		float  m_DragOffsetX;
		float  m_DragOffsetY;
	};

	extern CNewUIBossRanking* g_pBossRanking;
}
