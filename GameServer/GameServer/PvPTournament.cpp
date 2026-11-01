#include "stdafx.h"
#include "PvPTournament.h"
#include "Duel.h"
#include "Notice.h"
#include "Util.h"
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include "ServerInfo.h"
#include "CGMScriptLua.h"
#include "Monster.h"
#include "Gate.h"

CPvPTournament gPvPTournament;

int g_PvPNpcSpawnedIndex = -1;

void RemovePvPTournamentNpc()
{
	if (OBJECT_RANGE(g_PvPNpcSpawnedIndex) != 0)
	{
		gObjDel(g_PvPNpcSpawnedIndex);
		g_PvPNpcSpawnedIndex = -1;
	}
}

// Cria o NPC do torneio dinamicamente em qualquer mapa/coordenada.
// Remove o NPC anterior antes, para nao duplicar quando o script recarrega.
bool SpawnPvPTournamentNpc(int classId, int map, int x, int y, int dir)
{
	RemovePvPTournamentNpc();

	int index = gObjAddMonster(map);

	if (OBJECT_RANGE(index) == 0)
	{
		return false;
	}

	LPOBJ lpObj = &gObj[index];

	lpObj->X = (short)x;
	lpObj->Y = (short)y;
	lpObj->TX = lpObj->X;
	lpObj->TY = lpObj->Y;
	lpObj->OldX = lpObj->X;
	lpObj->OldY = lpObj->Y;
	lpObj->StartX = (BYTE)lpObj->X;
	lpObj->StartY = (BYTE)lpObj->Y;
	lpObj->Dir = (BYTE)dir;
	lpObj->Map = map;

	if (gObjSetMonster(index, classId) == 0)
	{
		gObjDel(index);
		return false;
	}

	g_PvPNpcSpawnedIndex = index;

	return true;
}

int g_PvPEventNpcClass = -1;
int g_PvPBestOfRounds = 3;
bool g_PvPRegistrationOpen = false;

bool g_PvPScheduleEnabled = false;
int g_PvPScheduleDayOfWeek = -1;
int g_PvPScheduleHour = 20;
int g_PvPScheduleMinute = 0;
int g_PvPScheduleRegistrationMinutes = 10;
int g_PvPScheduleBestOf = 3;
bool g_PvPRingEnabled = false;
int g_PvPRingMap = 6;
int g_PvPRingMinX = 150;
int g_PvPRingMinY = 144;
int g_PvPRingMaxX = 160;
int g_PvPRingMaxY = 151;


CPvPTournament::CPvPTournament()
{
	m_State = PVP_STATE_IDLE;
	m_RoundNumber = 0;
	m_ScheduledStartTick = 0;
	m_LastScheduleTriggerMinute = -1;
	m_LastConnectionCheckTick = 0;
	m_LastRingCheckTick = 0;
}

CPvPTournament::~CPvPTournament()
{
}

// ============================================
// Controle vindo do Lua
// ============================================

void CPvPTournament::OpenRegistration()
{
	m_State = PVP_STATE_REGISTRATION;
	m_Registered.clear();
	m_CurrentRoundWinners.clear();
	m_Queue.clear();
	m_Running.clear();
	m_RoundNumber = 0;
	g_PvPRegistrationOpen = true;

	LogAdd(LOG_BLUE, "[PvPTournament] Inscricao aberta.");
}

void CPvPTournament::CloseRegistration()
{
	g_PvPRegistrationOpen = false;
	LogAdd(LOG_BLUE, "[PvPTournament] Inscricao fechada. Total inscritos: %d", (int)m_Registered.size());
}

void CPvPTournament::StartTournament()
{
	if (m_State != PVP_STATE_REGISTRATION)
	{
		LogAdd(LOG_RED, "[PvPTournament] Nao ha inscricao aberta para iniciar.");
		return;
	}

	// Revalida no momento do inicio. Indices desconectados, bots e contas
	// repetidas nao podem iniciar nem influenciar a chave do torneio.
	std::vector<int> validPlayers;
	for (size_t n = 0; n < m_Registered.size(); n++)
	{
		int aIndex = m_Registered[n];
		if (!this->IsValidPlayer(aIndex))
		{
			continue;
		}

		bool duplicateAccount = false;
		for (size_t i = 0; i < validPlayers.size(); i++)
		{
			if (_strnicmp(gObj[validPlayers[i]].Account, gObj[aIndex].Account, 10) == 0)
			{
				duplicateAccount = true;
				break;
			}
		}

		if (!duplicateAccount)
		{
			validPlayers.push_back(aIndex);
		}
	}
	m_Registered.swap(validPlayers);

	if ((int)m_Registered.size() < 2)
	{
		LogAdd(LOG_RED, "[PvPTournament] Inscritos insuficientes (%d). Minimo: 2.", (int)m_Registered.size());
		m_State = PVP_STATE_IDLE;
		g_PvPRegistrationOpen = false;
		m_Registered.clear();
		return;
	}

	g_PvPRegistrationOpen = false;
	m_State = PVP_STATE_RUNNING;
	m_RoundNumber = 1;

	gNotice.GCNoticeSendToAll(0, 1, 100, 100, RGB(255, 215, 0), 100, "%s", "O Torneio PvP vai comecar!");

	this->StartNextRound();
}

void CPvPTournament::CancelTournament()
{
	bool hadRealParticipants = false;
	for (size_t n = 0; n < m_Registered.size(); n++)
	{
		if (this->IsValidPlayer(m_Registered[n]))
		{
			hadRealParticipants = true;
			break;
		}
	}

	// Encerra qualquer duelo em andamento do torneio, sem premiar ninguem
	for (size_t n = 0; n < m_Running.size(); n++)
	{
		if (OBJECT_RANGE(m_Running[n].PlayerA) != 0)
		{
			gDuel.ResetDuel(&gObj[m_Running[n].PlayerA]);
			gDuel.GCDuelEndSend(m_Running[n].PlayerA, 0);
		}
		if (OBJECT_RANGE(m_Running[n].PlayerB) != 0)
		{
			gDuel.ResetDuel(&gObj[m_Running[n].PlayerB]);
			gDuel.GCDuelEndSend(m_Running[n].PlayerB, 0);
		}
	}

	m_State = PVP_STATE_IDLE;
	g_PvPRegistrationOpen = false;
	m_Registered.clear();
	m_CurrentRoundWinners.clear();
	m_Queue.clear();
	m_Running.clear();

	if (hadRealParticipants)
	{
		gNotice.GCNoticeSendToAll(0, 1, 100, 100, RGB(255, 150, 150), 100, "%s", "O Torneio PvP foi cancelado.");
	}
	LogAdd(LOG_BLUE, "[PvPTournament] Torneio cancelado manualmente.");
}

// ============================================
// Jogador clicando no NPC
// ============================================

bool CPvPTournament::JoinPlayer(LPOBJ lpObj)
{
	if (lpObj == 0 || !this->IsValidPlayer(lpObj->Index))
	{
		return false;
	}

	if (m_State != PVP_STATE_REGISTRATION || g_PvPRegistrationOpen == false)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "Inscricao para o Torneio PvP nao esta aberta no momento.");
		return false;
	}

	// Ja inscrito?
	for (size_t n = 0; n < m_Registered.size(); n++)
	{
		if (m_Registered[n] == lpObj->Index ||
			(this->IsValidPlayer(m_Registered[n]) && _strnicmp(gObj[m_Registered[n]].Account, lpObj->Account, 10) == 0))
		{
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "Esta conta ja esta inscrita no Torneio PvP.");
			return false;
		}
	}

	m_Registered.push_back(lpObj->Index);

	gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "Voce entrou no Torneio PvP! Aguarde o inicio.");

	this->MovePlayerToGate(lpObj->Index, PVP_WAIT_GATE);

	return true;
}

// ============================================
// Logica de rodadas / bracket
// ============================================

void CPvPTournament::StartNextRound()
{
	m_Queue.clear();
	m_Running.clear();

	std::vector<int> players = (m_RoundNumber == 1) ? m_Registered : m_CurrentRoundWinners;
	m_CurrentRoundWinners.clear();

	// Remove jogadores desconectados antes de sortear
	std::vector<int> validPlayers;
	for (size_t n = 0; n < players.size(); n++)
	{
		if (this->IsValidPlayer(players[n]))
		{
			validPlayers.push_back(players[n]);
		}
	}
	players = validPlayers;

	if (players.size() == 1)
	{
		// So sobrou 1 -> campeao
		this->FinishTournament(players[0]);
		return;
	}

	if (players.empty())
	{
		LogAdd(LOG_RED, "[PvPTournament] Ninguem restante, cancelando torneio.");
		m_State = PVP_STATE_IDLE;
		g_PvPRegistrationOpen = false;
		m_Registered.clear();
		m_CurrentRoundWinners.clear();
		m_Queue.clear();
		m_Running.clear();
		return;
	}

	// Embaralha
	for (size_t n = players.size(); n > 1; n--)
	{
		size_t i = (size_t)(GetLargeRand() % n);
		std::swap(players[n - 1], players[i]);
	}

	gNotice.GCNoticeSendToAll(0, 1, 100, 100, RGB(100, 255, 100), 100, "Rodada %d do Torneio PvP comecando! (%d jogadores)", m_RoundNumber, (int)players.size());

	// Numero impar: um jogador sorteado avanca automaticamente (bye).
	if (players.size() % 2 != 0)
	{
		int byePlayer = players.back();
		players.pop_back();
		m_CurrentRoundWinners.push_back(byePlayer);
		this->MovePlayerToGate(byePlayer, PVP_WINNER_GATE);
		gNotice.GCNoticeSendToAll(0, 1, 100, 100, RGB(200, 200, 255), 100,
			"%s passou automaticamente nesta rodada.", gObj[byePlayer].Name);
	}

	for (size_t i = 0; i + 1 < players.size(); i += 2)
	{
		PVP_MATCH match;
		match.PlayerA = players[i];
		match.PlayerB = players[i + 1];
		match.Finished = false;
		m_Queue.push_back(match);
	}

	for (size_t n = 0; n < m_Queue.size(); n++)
	{
		char nameA[11] = "???";
		char nameB[11] = "???";

		if (OBJECT_RANGE(m_Queue[n].PlayerA) != 0) { memcpy(nameA, gObj[m_Queue[n].PlayerA].Name, 10); }
		if (OBJECT_RANGE(m_Queue[n].PlayerB) != 0) { memcpy(nameB, gObj[m_Queue[n].PlayerB].Name, 10); }

		gNotice.GCNoticeSendToAll(0, 1, 100, 100, RGB(200, 200, 255), 100, "Confronto: %s vs %s", nameA, nameB);
	}

	this->TryStartQueuedMatches();
}

void CPvPTournament::TryStartQueuedMatches()
{
	// O torneio usa uma unica arena: sempre existe no maximo um duelo ativo.
	if (!m_Running.empty())
	{
		return;
	}

	while (!m_Queue.empty())
	{
		PVP_MATCH match = m_Queue.front();
		m_Queue.erase(m_Queue.begin());

		bool validA = this->IsValidPlayer(match.PlayerA);
		bool validB = this->IsValidPlayer(match.PlayerB);

		if (!validA && !validB)
		{
			LogAdd(LOG_RED, "[PvPTournament] Dupla removida: os dois jogadores desconectaram.");
			continue;
		}

		if (!validA || !validB)
		{
			int winner = validA ? match.PlayerA : match.PlayerB;
			m_CurrentRoundWinners.push_back(winner);
			this->MovePlayerToGate(winner, PVP_WINNER_GATE);
			gNotice.GCNoticeSendToAll(0, 1, 100, 100, RGB(255, 200, 100), 100,
				"%s avancou por ausencia do adversario.", gObj[winner].Name);
			continue;
		}

		gServerInfo.m_DuelMaxScore = ((g_PvPBestOfRounds / 2) + 1);
		gDuel.StartDuelBit(&gObj[match.PlayerA], &gObj[match.PlayerB]);
		this->MovePlayerToGate(match.PlayerA, PVP_ARENA_GATE_A);
		this->MovePlayerToGate(match.PlayerB, PVP_ARENA_GATE_B);
		m_Running.push_back(match);
		return;
	}

	// Nao restou luta nesta rodada (por exemplo, todos os adversarios desconectaram).
	if (m_State == PVP_STATE_RUNNING)
	{
		m_RoundNumber++;
		this->StartNextRound();
	}
}

bool CPvPTournament::IsValidPlayer(int aIndex)
{
	return (OBJECT_RANGE(aIndex) != 0 && gObjIsConnectedGP(aIndex) != 0 &&
		gObj[aIndex].Type == OBJECT_USER && gObj[aIndex].Account[0] != 0 && gObj[aIndex].Name[0] != 0);
}

bool CPvPTournament::MovePlayerToGate(int aIndex, int gate)
{
	if (!this->IsValidPlayer(aIndex))
	{
		return false;
	}

	// gObjMoveGate recusa o movimento enquanto RegenOk estiver ativo.
	// Zera o estado antes de cada deslocamento para os dois lutadores
	// receberem exatamente o gate configurado pelo Lua.
	gObj[aIndex].RegenOk = 0;
	if (gObjMoveGate(aIndex, gate) == 0)
	{
		LogAdd(LOG_RED, "[PvPTournament] Falha ao mover %s (conta %s) para o gate %d.",
			gObj[aIndex].Name, gObj[aIndex].Account, gate);
		return false;
	}

	return true;
}

bool CPvPTournament::IsInRunningMatches(int aIndex)
{
	for (size_t n = 0; n < m_Running.size(); n++)
	{
		if (m_Running[n].PlayerA == aIndex || m_Running[n].PlayerB == aIndex)
		{
			return true;
		}
	}
	return false;
}

void CPvPTournament::EnforceRingIsolation()
{
	if (!g_PvPRingEnabled || m_State != PVP_STATE_RUNNING || m_Running.empty())
	{
		return;
	}

	const PVP_MATCH& match = m_Running.front();
	const int fighters[2] = { match.PlayerA, match.PlayerB };

	for (int n = 0; n < 2; n++)
	{
		const int aIndex = fighters[n];
		if (!this->IsValidPlayer(aIndex))
		{
			continue;
		}

		LPOBJ lpObj = &gObj[aIndex];
		if (lpObj->Map != g_PvPRingMap || lpObj->X < g_PvPRingMinX || lpObj->X > g_PvPRingMaxX ||
			lpObj->Y < g_PvPRingMinY || lpObj->Y > g_PvPRingMaxY)
		{
			this->MovePlayerToGate(aIndex, (n == 0) ? PVP_ARENA_GATE_A : PVP_ARENA_GATE_B);
		}
	}

	for (int aIndex = OBJECT_START_USER; aIndex < MAX_OBJECT; aIndex++)
	{
		if (aIndex == match.PlayerA || aIndex == match.PlayerB || !this->IsValidPlayer(aIndex))
		{
			continue;
		}

		LPOBJ lpObj = &gObj[aIndex];
		if (lpObj->Type == OBJECT_USER && lpObj->Map == g_PvPRingMap &&
			lpObj->X >= g_PvPRingMinX && lpObj->X <= g_PvPRingMaxX &&
			lpObj->Y >= g_PvPRingMinY && lpObj->Y <= g_PvPRingMaxY)
		{
			this->MovePlayerToGate(aIndex, PVP_WAIT_GATE);
		}
	}
}

bool CPvPTournament::IsTournamentMatch(int indexA, int indexB)
{
	if (m_State != PVP_STATE_RUNNING)
	{
		return false;
	}
	for (size_t n = 0; n < m_Running.size(); n++)
	{
		if ((m_Running[n].PlayerA == indexA && m_Running[n].PlayerB == indexB) ||
			(m_Running[n].PlayerA == indexB && m_Running[n].PlayerB == indexA))
		{
			return true;
		}
	}
	return false;
}

void CPvPTournament::OnMatchFinished(int winnerIndex, int loserIndex)
{
	if (m_State != PVP_STATE_RUNNING)
	{
		return;
	}

	for (size_t n = 0; n < m_Running.size(); n++)
	{
		if ((m_Running[n].PlayerA == winnerIndex && m_Running[n].PlayerB == loserIndex) ||
			(m_Running[n].PlayerA == loserIndex && m_Running[n].PlayerB == winnerIndex))
		{
			m_Running.erase(m_Running.begin() + n);
			break;
		}
	}

	m_CurrentRoundWinners.push_back(winnerIndex);

	if (OBJECT_RANGE(loserIndex) != 0 && gObjIsConnected(loserIndex) != 0)
	{
		this->MovePlayerToGate(loserIndex, PVP_LOSER_GATE);
	}

	if (OBJECT_RANGE(winnerIndex) != 0 && gObjIsConnected(winnerIndex) != 0)
	{
		this->MovePlayerToGate(winnerIndex, PVP_WINNER_GATE);
	}

	this->TryStartQueuedMatches();
}

void CPvPTournament::FinishTournament(int championIndex)
{
	m_State = PVP_STATE_IDLE;

	if (OBJECT_RANGE(championIndex) != 0 && gObjIsConnected(championIndex) != 0)
	{
		gNotice.GCNoticeSendToAll(0, 1, 100, 100, RGB(255, 215, 0), 100, "%s venceu o Torneio PvP!", gObj[championIndex].Name);

		LogAdd(LOG_BLUE, "[PvPTournament] Campeao: %s", gObj[championIndex].Name);

		extern void Lua_OnPvPChampion(int championIndex);
		Lua_OnPvPChampion(championIndex);
	}

	m_Registered.clear();
	m_CurrentRoundWinners.clear();
	m_Queue.clear();
	m_Running.clear();
}

// ============================================
// Funcoes globais chamadas de fora
// ============================================

void PvPTournamentMainProc()
{
	gPvPTournament.MainProc();
}

void CPvPTournament::MainProc()
{
	if (m_State == PVP_STATE_RUNNING && (GetTickCount() - m_LastRingCheckTick) >= 500)
	{
		m_LastRingCheckTick = GetTickCount();
		this->EnforceRingIsolation();
	}

	// Se alguem desconectar durante o 1x1, o adversario conectado avanca.
	if (m_State == PVP_STATE_RUNNING && !m_Running.empty() &&
		(GetTickCount() - m_LastConnectionCheckTick) >= 1000)
	{
		m_LastConnectionCheckTick = GetTickCount();
		PVP_MATCH match = m_Running.front();
		bool validA = this->IsValidPlayer(match.PlayerA);
		bool validB = this->IsValidPlayer(match.PlayerB);

		if (!validA || !validB)
		{
			m_Running.clear();
			if (validA || validB)
			{
				int winner = validA ? match.PlayerA : match.PlayerB;
				int loser = validA ? match.PlayerB : match.PlayerA;
				this->OnMatchFinished(winner, loser);
			}
			else
			{
				this->TryStartQueuedMatches();
			}
		}
	}

	// ---- Auto-abertura da inscricao, no horario configurado via Lua ----
	if (g_PvPScheduleEnabled && m_State == PVP_STATE_IDLE)
	{
		time_t rawTime = time(0);
		struct tm timeInfo;
		localtime_s(&timeInfo, &rawTime);

		bool dayMatches = (g_PvPScheduleDayOfWeek == -1) || (g_PvPScheduleDayOfWeek == timeInfo.tm_wday);

		if (dayMatches && timeInfo.tm_hour == g_PvPScheduleHour && timeInfo.tm_min == g_PvPScheduleMinute)
		{
			int currentMinuteKey = (timeInfo.tm_yday * 1440) + (timeInfo.tm_hour * 60) + timeInfo.tm_min;

			if (m_LastScheduleTriggerMinute != currentMinuteKey)
			{
				m_LastScheduleTriggerMinute = currentMinuteKey;

				g_PvPBestOfRounds = g_PvPScheduleBestOf;

				this->OpenRegistration();

				gNotice.GCNoticeSendToAll(0, 1, 100, 100, RGB(255, 215, 0), 100,
					"Inscricao do Torneio PvP aberta! Fecha em %d minutos.", g_PvPScheduleRegistrationMinutes);

				m_ScheduledStartTick = GetTickCount() + (DWORD)(g_PvPScheduleRegistrationMinutes * 60000);
			}
		}
	}

	// ---- Auto-inicio do torneio, quando o tempo de inscricao acabar ----
	if (m_State == PVP_STATE_REGISTRATION && m_ScheduledStartTick != 0 && GetTickCount() >= m_ScheduledStartTick)
	{
		m_ScheduledStartTick = 0;
		this->StartTournament();
	}
}

void PvPTournamentJoin(LPOBJ lpObj)
{
	gPvPTournament.JoinPlayer(lpObj);
}

// ============================================
// Gancho que o Lua usa para decidir o premio do campeao
// ============================================
void Lua_OnPvPChampion(int championIndex)
{
	if (g_pScriptLua != nullptr)
	{
		g_pScriptLua->CallPvPChampion(championIndex);
	}
}

int CPvPTournament::GetMatchOpponent(int aIndex)
{
	if (m_State != PVP_STATE_RUNNING)
	{
		return -1;
	}

	for (size_t n = 0; n < m_Running.size(); n++)
	{
		if (m_Running[n].PlayerA == aIndex) { return m_Running[n].PlayerB; }
		if (m_Running[n].PlayerB == aIndex) { return m_Running[n].PlayerA; }
	}

	return -1;
}
