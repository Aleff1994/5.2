#pragma once
#include "User.h"
#include <vector>
#include <string>

#define PVP_ARENA_GATE_A 1010
#define PVP_ARENA_GATE_B 1011
#define PVP_WAIT_GATE 1012
#define PVP_LOSER_GATE 1013
#define PVP_WINNER_GATE 1014
// ============================================
// Configuracao (editavel via Lua)
// ============================================
extern int g_PvPEventNpcClass;       // Class do NPC que abre o evento (-1 = desativado)
extern int g_PvPBestOfRounds;        // ex: 3 = melhor de 3 (precisa vencer 2)
extern bool g_PvPRegistrationOpen;   // true = jogadores podem entrar

// ---- Agendamento automatico (configuravel via Lua: SetPvPSchedule) ----
extern bool g_PvPScheduleEnabled;
extern int g_PvPScheduleDayOfWeek;          // 0=domingo ... 6=sabado, -1 = todo dia
extern int g_PvPScheduleHour;               // 0-23
extern int g_PvPScheduleMinute;             // 0-59
extern int g_PvPScheduleRegistrationMinutes; // quanto tempo a inscricao fica aberta antes de comecar
extern int g_PvPScheduleBestOf;             // bestOf usado no torneio agendado
extern bool g_PvPRingEnabled;
extern int g_PvPRingMap;
extern int g_PvPRingMinX;
extern int g_PvPRingMinY;
extern int g_PvPRingMaxX;
extern int g_PvPRingMaxY;
bool SpawnPvPTournamentNpc(int classId, int map, int x, int y, int dir);
void RemovePvPTournamentNpc();

enum ePvPTournamentState
{
	PVP_STATE_IDLE = 0,          // nada acontecendo
	PVP_STATE_REGISTRATION = 1,  // inscricao aberta
	PVP_STATE_RUNNING = 2,       // torneio rodando
};

struct PVP_MATCH
{
	int PlayerA;
	int PlayerB;
	bool Finished;
};

class CPvPTournament
{
public:
	CPvPTournament();
	~CPvPTournament();

	void MainProc(); // chamado a cada tick do server

	// ---- Controle vindo do Lua ----
	void OpenRegistration();
	void CloseRegistration();
	void StartTournament();
	void CancelTournament();

	// ---- Jogador ----
	bool JoinPlayer(LPOBJ lpObj);

	// ---- Chamado pelo Duel.cpp quando um duelo do torneio termina ----
	void OnMatchFinished(int winnerIndex, int loserIndex);

	int GetState() { return m_State; }
	bool IsTournamentMatch(int indexA, int indexB);
	int GetMatchOpponent(int aIndex);

private:
	int m_State;
	std::vector<int> m_Registered;      // indices inscritos (antes de comecar)
	std::vector<int> m_CurrentRoundWinners; // quem ja venceu na rodada atual
	std::vector<PVP_MATCH> m_Queue;     // partidas pendentes (fila de arena)
	std::vector<PVP_MATCH> m_Running;   // partidas rodando agora (max = MAX_DUEL_ARENA)
	int m_RoundNumber;
	DWORD m_LastConnectionCheckTick;    // limita a checagem de desconexao
	DWORD m_LastRingCheckTick;          // limita a fiscalizacao do ringue

	DWORD m_ScheduledStartTick;        // quando fechar inscricao e comecar automaticamente (0 = nao agendado)
	int m_LastScheduleTriggerMinute;   // evita disparar 2x no mesmo minuto

	void StartNextRound();
	void TryStartQueuedMatches();
	bool IsInRunningMatches(int aIndex);
	bool IsValidPlayer(int aIndex);
	bool MovePlayerToGate(int aIndex, int gate);
	void EnforceRingIsolation();
	void FinishTournament(int championIndex);
};

extern CPvPTournament gPvPTournament;

// Chamado a cada "tick" do server (igual LuaEventCheckExpire)
void PvPTournamentMainProc();

// Chamado pelo NpcTalk.cpp quando o jogador clica no NPC configurado
void PvPTournamentJoin(LPOBJ lpObj);
