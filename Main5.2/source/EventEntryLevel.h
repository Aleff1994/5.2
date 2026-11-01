#pragma once

class CEventEntryLevel
{
public:
	CEventEntryLevel();
	bool Init();

	bool BloodCastleEnabled() const { return m_bBloodCastle; }
	bool DevilSquareEnabled() const { return m_bDevilSquare; }
	bool ChaosCastleEnabled() const { return m_bChaosCastle; }
	bool IllusionTempleEnabled() const { return m_bIllusionTemple; }
	int CalculateEntryLevel(int eventType, bool specialClass, int characterLevel, int resetCount,
		int masterResetCount, bool thirdClass, int entryType) const;

	int m_BloodCastleEntryLevelCommon[7][2];
	int m_BloodCastleEntryLevelSpecial[7][2];
	int m_DevilSquareEntryLevelCommon[6][2];
	int m_DevilSquareEntryLevelSpecial[6][2];
	int m_ChaosCastleEntryLevelCommon[6][2];
	int m_ChaosCastleEntryLevelSpecial[6][2];
	int m_ChaosCastleZen[7];
	int m_IllusionTempleEntryLevelMin[5];
	int m_IllusionTempleEntryLevelMax[5];

private:
	bool m_bBloodCastle;
	bool m_bDevilSquare;
	bool m_bChaosCastle;
	bool m_bIllusionTemple;
};

enum EVENT_ENTRY_EVENT_TYPE
{
	EVENT_ENTRY_BLOOD_CASTLE = 0,
	EVENT_ENTRY_DEVIL_SQUARE = 1,
	EVENT_ENTRY_CHAOS_CASTLE = 2,
};

extern CEventEntryLevel gEventEntryLevel;
