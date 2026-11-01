// ---
#include "Protocol.h"
// ---
#define MAX_RANK  10
// ---
struct CUSTOM_RANKING
{
	int index;
	char Name[20];
	char Col1[20];
	char Col2[20];
};
struct CUSTOM_RANKING_DATA
{
	char szName[20];
	BYTE Class;
	BYTE Vip;
	int Score;
};
//**********************************************//
//********** GameServer -> DataServer **********//
//**********************************************//
struct SDHP_CUSTOM_RANKING_SEND
{
	PBMSG_HEAD header;
	WORD index;
	WORD type;
};
//**********************************************//
//********** DataServer -> GameServer **********//
//**********************************************//
struct SDHP_CUSTOM_RANKING_RECV
{
	PWMSG_HEAD header;
	int index;
	int type;
	int count;
};
//**********************************************//
//********** GameServer -> Cliente    **********//
//**********************************************//
struct PMSG_CUSTOM_RANKING_SEND
{
	PSWMSG_HEAD header;
	int RankIndex;
	char rankname[20];
	char col1[20];
	char col2[20];
	int count;
};
struct PMSG_CUSTOM_RANKING_COUNT_SEND
{
	PSBMSG_HEAD header; // C1:BF:51
	int count;
};
//**********************************************//
//********** Cliente -> GameServer    **********//
//**********************************************//
struct PMSG_CUSTOM_RANKING_COUNT_RECV
{
	PSBMSG_HEAD header; // C1:BF:51
};
struct PMSG_CUSTOM_RANKING_RECV
{
	PSBMSG_HEAD header; // C1:BF:51
	BYTE type;
};

// ==================== NOVO: Photo/Preview do Ranking ====================

//**********************************************//
//****** GameServer -> DataServer (Photo) *******//
//**********************************************//
struct SDHP_CUSTOM_RANKING_PHOTO_SEND
{
	PBMSG_HEAD header;
	WORD index;
	char Name[11];
};

//**********************************************//
//****** DataServer -> GameServer (Photo) *******//
//**********************************************//
struct SDHP_CUSTOM_RANKING_PHOTO_RECV
{
	PWMSG_HEAD header;
	int index;
	BYTE result;      // 1 = encontrado, 0 = nao encontrado
	BYTE Class;
	BYTE Inventory[12][5]; // contrato legado da foto do ranking
};

//**********************************************//
//****** Cliente -> GameServer (Photo Req) ******//
//**********************************************//
struct PMSG_CUSTOM_RANKING_PHOTO_RECV
{
	PSBMSG_HEAD header; // C1:E9
	char Name[11];
};

//**********************************************//
//****** GameServer -> Cliente (Photo Send) *****//
//**********************************************//
struct PMSG_CUSTOM_RANKING_PHOTO_SEND
{
	PSWMSG_HEAD header; // C1:F3:E9
	BYTE result;
	BYTE Class;
	DWORD Equipment[12];
};

// ==========================================================================

// ---
class CCustomRanking
{
public:
	void Load(char* path);
	void GCReqRanking(int Index, PMSG_CUSTOM_RANKING_RECV* pMsg);
	void GCReqRankingCount(int Index, PMSG_CUSTOM_RANKING_COUNT_RECV* lpMsg);
	// ---
	int GetRankIndex(int aIndex);
	void CheckUpdate(LPOBJ lpObj);
	void GDCustomRankingRecv(BYTE* ReceiveBuffer);

	// ==================== NOVO ====================
	void GCReqRankingPhoto(int Index, PMSG_CUSTOM_RANKING_PHOTO_RECV* lpMsg);
	void GDCustomRankingPhotoRecv(BYTE* ReceiveBuffer);
	// ================================================

private:
	int m_count;
	// ---
	CUSTOM_RANKING r_Data[MAX_RANK];
};
extern CCustomRanking gCustomRanking;
// ---
