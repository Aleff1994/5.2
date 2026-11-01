// ============================================================
// PROTOCOLO - "Party Disponivel" / Party Match
// Head principal: 0xBA (livre, confirmado no projeto)
// Segue o mesmo padrao PSBMSG_HEAD (C1/C2 + head + subh) usado
// em outros pacotes com sub-codigo, como 0xB1 e 0xB2 no
// Protocol.h original.
//
// Este bloco deve ser colado dentro do Protocol.h, na secao de
// structs (pode ir perto do PMSG_BOSS_RANKING_SEND, no final).
// ============================================================

// === PARTY MATCH ("Party Disponivel") - head 0xBA ===
#define MAX_PARTY_MATCH_LIST   20   //. quantas parties no maximo retornamos numa unica resposta
#define PARTY_MATCH_PWD_LEN    11   //. mesmo tamanho usado em outras senhas do projeto (ex: storage password)

//. ----------------------------------------------------------
//. 0xBA:01  (Client -> GameServer)
//. Pedido de lista de parties publicas. Sem corpo extra: so o
//. header já basta, mas deixamos a struct para manter o padrao
//. e facilitar extensao futura (ex: filtro por mapa).
//. ----------------------------------------------------------
struct PMSG_PARTY_MATCH_LIST_REQUEST_RECV
{
	PSBMSG_HEAD header; // C1:BA:01
};

//. ----------------------------------------------------------
//. 0xBA:02  (GameServer -> Client)
//. Resposta com a lista de parties publicas no momento.
//. ----------------------------------------------------------
struct PMSG_PARTY_MATCH_ENTRY
{
	char  LeaderName[11];   //. nome do lider (mesmo tamanho usado em Name[11] no OBJECTSTRUCT)
	WORD  LeaderLevel;      //. level do lider
	BYTE  LeaderClass;      //. classe do lider (campo Class do OBJECTSTRUCT, cabe em BYTE)
	BYTE  MapNumber;        //. mapa atual do lider (campo Map do OBJECTSTRUCT)
	BYTE  MemberCount;      //. quantos estao na party agora
	BYTE  MaxMembers;       //. MAX_PARTY_USER no momento (10 ou 5, conforme o define)
	BYTE  HasPassword;      //. 0 = sem senha, 1 = com senha
	DWORD PartyLeaderIndex; //. index (aIndex) do lider no servidor - usado depois para o pedido de entrada
};

struct PMSG_PARTY_MATCH_LIST_SEND
{
	PSWMSG_HEAD header; // C2:BA:02  (usa WORD de tamanho pois a lista pode passar de 255 bytes)
	BYTE count;
	// seguido de 'count' structs PMSG_PARTY_MATCH_ENTRY
};

//. ----------------------------------------------------------
//. 0xBA:03  (Client -> GameServer)
//. Pedido para ativar/desativar a visibilidade publica da
//. PROPRIA party (so funciona se quem mandou for o lider -
//. checagem feita no servidor, nunca confiar no cliente).
//. ----------------------------------------------------------
struct PMSG_PARTY_MATCH_TOGGLE_RECV
{
	PSBMSG_HEAD header; // C1:BA:03
	BYTE  enable;                    //. 1 = tornar publica, 0 = tornar privada
	BYTE  usePassword;                //. 1 = exigir senha para entrar, 0 = livre
	char  password[PARTY_MATCH_PWD_LEN]; //. senha em texto puro (so usado se usePassword = 1)
};

//. ----------------------------------------------------------
//. 0xBA:04  (GameServer -> Client)
//. Confirma o resultado do pedido de ativar/desativar.
//. ----------------------------------------------------------
struct PMSG_PARTY_MATCH_TOGGLE_SEND
{
	PSBMSG_HEAD header; // C1:BA:04
	BYTE result; //. ver enum PARTY_MATCH_TOGGLE_RESULT abaixo
	BYTE enabled; //. estado atual depois do pedido (1 = publica, 0 = privada)
};

enum PARTY_MATCH_TOGGLE_RESULT
{
	PARTY_MATCH_TOGGLE_OK            = 0,
	PARTY_MATCH_TOGGLE_NOT_LEADER    = 1, //. quem pediu nao e o lider da party
	PARTY_MATCH_TOGGLE_NO_PARTY      = 2, //. quem pediu nao esta em nenhuma party
};

//. ----------------------------------------------------------
//. 0xBA:05  (Client -> GameServer)
//. Pedido para entrar numa party da lista.
//. ----------------------------------------------------------
struct PMSG_PARTY_MATCH_JOIN_RECV
{
	PSBMSG_HEAD header; // C1:BA:05
	DWORD PartyLeaderIndex;             //. veio de PMSG_PARTY_MATCH_ENTRY.PartyLeaderIndex
	char  password[PARTY_MATCH_PWD_LEN]; //. preenchido pelo cliente se a entry tinha HasPassword = 1
};

//. ----------------------------------------------------------
//. 0xBA:06  (GameServer -> Client)
//. Resultado do pedido de entrada.
//. ----------------------------------------------------------
struct PMSG_PARTY_MATCH_JOIN_SEND
{
	PSBMSG_HEAD header; // C1:BA:06
	BYTE result; //. ver enum PARTY_MATCH_JOIN_RESULT abaixo
};

enum PARTY_MATCH_JOIN_RESULT
{
	PARTY_MATCH_JOIN_OK             = 0,
	PARTY_MATCH_JOIN_WRONG_PASSWORD = 1,
	PARTY_MATCH_JOIN_PARTY_FULL     = 2,
	PARTY_MATCH_JOIN_PARTY_NOT_FOUND= 3, //. a party listada nao existe mais (lider saiu/desconectou)
	PARTY_MATCH_JOIN_NOT_PUBLIC     = 4, //. a party deixou de ser publica entre a listagem e o clique
	PARTY_MATCH_JOIN_ALREADY_IN_PARTY = 5, //. quem pediu ja esta em uma party
};

// ----------------------------------------------------------
// Prototipos das funcoes de tratamento (declarar tambem no
// Protocol.h junto dos outros CG.../GC...)
// ----------------------------------------------------------
void CGPartyMatchListRequestRecv(PMSG_PARTY_MATCH_LIST_REQUEST_RECV* lpMsg, int aIndex);
void CGPartyMatchToggleRecv(PMSG_PARTY_MATCH_TOGGLE_RECV* lpMsg, int aIndex);
void CGPartyMatchJoinRecv(PMSG_PARTY_MATCH_JOIN_RECV* lpMsg, int aIndex);

void GCPartyMatchListSend(int aIndex);
void GCPartyMatchToggleSend(int aIndex, BYTE result, BYTE enabled);
void GCPartyMatchJoinSend(int aIndex, BYTE result);
