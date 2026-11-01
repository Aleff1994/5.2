// Party.h: interface for the CParty class.
//
//////////////////////////////////////////////////////////////////////
#pragma once
#include "Protocol.h"
#include "User.h"
#include "Protocol_PartyMatch.h"
#define INCREASE_PARTY_MANAGER
#ifdef INCREASE_PARTY_MANAGER
#define MAX_PARTY_USER 10
#else
#define MAX_PARTY_USER 5
#endif // INCREASE_PARTY_MANAGER
#define MAX_PARTY_DISTANCE 10
//**********************************************//
//************ Client -> GameServer ************//
//**********************************************//
struct PMSG_PARTY_REQUEST_RECV
{
	PBMSG_HEAD header; // C1:40
	BYTE index[2];
};
struct PMSG_PARTY_REQUEST_RESULT_RECV
{
	PBMSG_HEAD header; // C1:41
	BYTE result;
	BYTE index[2];
};
struct PMSG_PARTY_DEL_MEMBER_RECV
{
	PBMSG_HEAD header; // C1:43
	BYTE number;
};
//**********************************************//
//************ GameServer -> Client ************//
//**********************************************//
struct PMSG_PARTY_REQUEST_SEND
{
	PBMSG_HEAD header; // C1:40
	BYTE index[2];
};
struct PMSG_PARTY_RESULT_SEND
{
	PBMSG_HEAD header; // C1:41
	BYTE result;
};
struct PMSG_PARTY_LIST_SEND
{
#ifdef INCREASE_PARTY_MANAGER
	PSWMSG_HEAD header; // C1:42
#else
	PSBMSG_HEAD header; // C1:42
#endif // INCREASE_PARTY_MANAGER
	BYTE count;
};
struct PMSG_PARTY_LIST
{
	char name[10];
	BYTE number;
	BYTE map;
	BYTE x;
	BYTE y;
	DWORD CurLife;
	DWORD MaxLife;
#if(GAMESERVER_UPDATE>=802)
	DWORD ServerCode;
	DWORD CurMana;
	DWORD MaxMana;
#endif
};
struct PMSG_PARTY_LIFE_SEND
{
	PBMSG_HEAD header; // C1:44
	BYTE count;
};
struct PMSG_PARTY_LIFE
{
#if(GAMESERVER_UPDATE>=802)
	BYTE life;
	BYTE mana;
	char name[11];
#else
	BYTE value;
#endif
};
//**********************************************//
//**********************************************//
//**********************************************//
struct PARTY_INFO
{
	int Count;
	int Index[MAX_PARTY_USER];
#ifdef PARTY_PASSWORD_JOIN
	std::string Password;
#endif // PARTY_PASSWORD_JOIN
	bool IsPublic;           // NOVO: true = aparece na janela "Party Disponivel"
	bool HasPublicPwd;       // NOVO: true = exige senha para entrar via essa janela
	char PublicPassword[12]; // NOVO: senha usada na listagem publica

	PARTY_INFO() // NOVO: garante valores iniciais sempre limpos
	{
		Count = 0;
		memset(Index, 0, sizeof(Index));
		IsPublic = false;
		HasPublicPwd = false;
		PublicPassword[0] = '\0';
	}
};
class CParty
{
public:
	CParty();
	virtual ~CParty();
	bool IsParty(int index);
	bool IsLeader(int index, int aIndex);
	bool IsMember(int index, int aIndex);
	int GetMemberCount(int index);
	int GetMemberIndex(int index, int number);
	int GetMemberNumber(int index, int aIndex);
	bool Create(int aIndex);
	bool Destroy(int index);
	bool AddMember(int index, int aIndex);
	bool DelMember(int index, int aIndex);
	void ChangeLeader(int index, int number);
	bool AutoAcceptPartyRequest(LPOBJ lpObj, LPOBJ lpTarget);
#ifdef PARTY_PASSWORD_JOIN
	bool PartyPasswordCheck(int index, std::string password);
#endif // PARTY_PASSWORD_JOIN
	void CGPartyRequestRecv(PMSG_PARTY_REQUEST_RECV* lpMsg, int aIndex);
	void CGPartyRequestResultRecv(PMSG_PARTY_REQUEST_RESULT_RECV* lpMsg, int aIndex);
	void CGPartyListRecv(int aIndex);
	void CGPartyDelMemberRecv(PMSG_PARTY_DEL_MEMBER_RECV* lpMsg, int aIndex);
	void GCPartyResultSend(int aIndex, BYTE result);
	void GCPartyListSend(int index);
	void GCPartyDelMemberSend(int aIndex);
	void GCPartyLifeSend(int index);

	//. ---- NOVO: Party Match ("Party Disponivel") ----
	bool TogglePublic(int aIndex, bool enable, bool usePassword, const char* password, BYTE* outResult);
	int  GetPublicPartyCount();
	bool IsPublic(int index);
	bool CheckPublicPassword(int index, const char* password);
	bool JoinPublicParty(int leaderIndex, int joiningIndex, const char* password, BYTE* outResult);

	void CGPartyMatchListRequestRecv(PMSG_PARTY_MATCH_LIST_REQUEST_RECV* lpMsg, int aIndex);
	void CGPartyMatchToggleRecv(PMSG_PARTY_MATCH_TOGGLE_RECV* lpMsg, int aIndex);
	void CGPartyMatchJoinRecv(PMSG_PARTY_MATCH_JOIN_RECV* lpMsg, int aIndex);

	void GCPartyMatchListSend(int aIndex);
	void GCPartyMatchToggleSend(int aIndex, BYTE result, BYTE enabled);
	void GCPartyMatchJoinSend(int aIndex, BYTE result);

public:
	int m_PartyCount;
	PARTY_INFO m_PartyInfo[MAX_OBJECT];
};
extern CParty gParty;
