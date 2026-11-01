// NewUIBossLifeBar.h
// Barra de vida de Boss customizada (port do BossLifeBar.lua para C++)
//////////////////////////////////////////////////////////////////////
#pragma once

#include "NewUIBase.h"

namespace SEASON3B
{
	class CNewUIBossLifeBar
	{
	public:
		CNewUIBossLifeBar();
		~CNewUIBossLifeBar();

		void LoadImages();
		void UnloadImages();

		// Chame essa funcao sempre que o jogo souber o HP% atual do monstro.
		// iMonsterType = MonsterIndex do monstro (275=Kundun, 600=Astelia, 598=Medusa, etc)
		// iHPRate      = vida em porcentagem (0 a 100)
		// iSDRate      = shield/SD em porcentagem (0 a 100), opcional
		void SetBossInfo(int iMonsterType, int iHPRate, int iSDRate = 0);

		// Chame quando o boss morrer / sair de range para esconder a barra
		void ClearBossInfo();

		// Chame todo frame, no loop de render da interface
		void Render();

	private:
		bool IsBoss(int iMonsterType);

		int  m_iMonsterType;
		int  m_iHPRate;
		int  m_iSDRate;
		bool m_bActive;

		// Adicione estas duas linhas aqui:
		int   m_iTargetMonsterIndex;
		DWORD m_dwLastUpdate;
		// Animacao / efeitos visuais
		float m_fDisplayHPRate;  // valor exibido (anima suavemente até m_iHPRate)
		int   m_iLastHPRate;     // valor anterior, pra detectar dano
		float m_fDamageFlash;    // 0..1, decai com o tempo apos tomar dano
		DWORD m_dwLastTick;      // ultimo timeGetTime(), para delta time

		static const int BOSS_LIST[];
		static const int BOSS_LIST_COUNT;
	};

	extern CNewUIBossLifeBar* g_pBossLifeBar;
}