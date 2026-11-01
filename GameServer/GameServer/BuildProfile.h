#pragma once

#include <map>
#include <string>
#include "User.h"

struct BUILD_PROFILE_INFO
{
	char Account[11];
	char Name[11];
	int Slot;
	int Strength;
	int Dexterity;
	int Vitality;
	int Energy;
	int Leadership;
};

class CBuildProfile
{
public:
	CBuildProfile();
	bool Command(LPOBJ lpObj, const char* action, const char* slotName);
	bool HasAny(LPOBJ lpObj);

private:
	void Load();
	bool Save();
	int GetSlot(const char* name) const;
	const char* GetSlotName(int slot) const;
	std::string MakeKey(const char* account, const char* name, int slot) const;
	void Notice(LPOBJ lpObj, const char* format, ...);

	bool m_Loaded;
	std::map<std::string, BUILD_PROFILE_INFO> m_Profiles;
};

extern CBuildProfile gBuildProfile;
