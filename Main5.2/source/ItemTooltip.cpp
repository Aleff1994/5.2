#include "stdafx.h"
#include "ItemTooltip.h"
#include "Util.h"
#include "CharacterManager.h"
#include "ZzzCharacter.h"
#include "Protocol.h"
#include "CGMProtect.h"
#define ITEM(x, y)					((x * 512) + y)
nCInfo GInfo;

extern std::string g_strSelectedML;

static void LoadLocalizedTooltipValue(const char* section, const char* key, char* output, size_t outputSize)
{
	char path[MAX_PATH];
	char value[254] = { 0 };

	sprintf_s(path, "Data\\Custom\\ItemTooltipLang\\ItemTooltipLang_%s.ini", g_strSelectedML.c_str());
	GetPrivateProfileStringA(section, key, "", value, sizeof(value), path);

	if (value[0] == 0 && _stricmp(g_strSelectedML.c_str(), "Eng") != 0)
	{
		GetPrivateProfileStringA(section, key, "", value, sizeof(value),
			"Data\\Custom\\ItemTooltipLang\\ItemTooltipLang_Eng.ini");
	}

	if (value[0] != 0)
	{
		// The legacy tooltip scripts escape percent signs as "%%".  INI values
		// are copied as plain text, so collapse the escape before rendering.
		char* read = value;
		char* write = value;
		while (*read != 0)
		{
			*write++ = *read;
			if (*read == '%' && read[1] == '%')
			{
				read++;
			}
			read++;
		}
		*write = 0;

		strncpy_s(output, outputSize, value, _TRUNCATE);
	}
}


nCInfo::nCInfo()
{
	this->m_CustomInfo.clear();
	this->m_CustomInfoSet.clear();
	this->m_CustomDescripcionInfo.clear();
}

nCInfo::~nCInfo()
{
}

void nCInfo::loadnInformation(nInformation* info) // OK
{
	this->m_CustomInfo.clear();

	for (int n = 0; n < MaxLineItemToolTip; n++)
	{
		if (info[n].ItemIndexMin == -1 || info[n].ItemIndexMax == -1)
		{
			return;
		}
		//this->m_CustomInfo.insert(std::pair<int, nInformation>(info[n].ItemIndex, info[n]));
		nInformation localized = info[n];
		char key[32];
		sprintf_s(key, "Item%d", localized.Index);
		LoadLocalizedTooltipValue("OptionName", key, localized.OptionName, sizeof(localized.OptionName));
		this->m_CustomInfo.push_back(localized);
	}
}
void nCInfo::loadnInformationSet(nInformation* info) // OK
{
	this->m_CustomInfoSet.clear();

	for (int n = 0; n < MaxLineItemToolTip; n++)
	{
		if (info[n].ItemIndexMin == -1 || info[n].ItemIndexMax == -1)
		{
			return;
		}
		//this->m_CustomInfo.insert(std::pair<int, nInformation>(info[n].ItemIndex, info[n]));
		nInformation localized = info[n];
		char key[32];
		sprintf_s(key, "Set%d", localized.Index);
		LoadLocalizedTooltipValue("OptionName", key, localized.OptionName, sizeof(localized.OptionName));

		// Carrega todos os niveis suportados. A exibicao e filtrada em tempo real
		// pelo limite maximo recebido do MainInfo (gmProtect->max_item_level_on).
		if (localized.Index == 0)
		{
			for (int row = 0; row < 15; ++row)
			{
				localized.TextIndex[row] = -1;
				localized.TextColor[row] = -1;
				localized.LevelItem[row] = -1;
			}

			for (int level = 10; level <= 15; ++level)
			{
				const int row = level - 10;
				localized.TextIndex[row] = 1 + row;
				localized.TextColor[row] = TEXT_COLOR_GRAY;
				localized.LevelItem[row] = level;
			}
		}
		this->m_CustomInfoSet.push_back(localized);
	}
}
void nCInfo::loadnText(nText* info) // OK
{
	this->m_CustomDescripcionInfo.clear();

	for (int n = 0; n < MaxLineItemToolTip; n++)
	{
		if (info[n].Index < 0 || info[n].Index > 7680)
		{
			return;
		}
		nText localized = info[n];
		char key[32];
		sprintf_s(key, "Text%d", localized.Index);
		LoadLocalizedTooltipValue("Text", key, localized.Text, sizeof(localized.Text));
		this->m_CustomDescripcionInfo.insert(std::pair<int, nText>(localized.Index, localized));
	}
}

bool nGetTextNum(int TextIndex)
{
	std::map<int, nText>::iterator it = GInfo.m_CustomDescripcionInfo.find(TextIndex);

	if (it != GInfo.m_CustomDescripcionInfo.end())
	{
		return 1;
	}

	return 0;
}
char* nGetTextLine(int TextIndex)
{
	std::map<int, nText>::iterator it = GInfo.m_CustomDescripcionInfo.find(TextIndex);

	if (it != GInfo.m_CustomDescripcionInfo.end())
	{
		return it->second.Text;
	}

	return "no found";
}
extern	char    TextList[90][100];
extern	int     TextListColor[90];
extern	int     TextBold[90];
bool CheckItemSlotOpt(int IndexItem, int Level, int Exl)
{
	BYTE byFirstClass = gCharacterManager.GetBaseClass(Hero->Class);
	int firstSlot = ((byFirstClass == 3 || byFirstClass == 6) ? EQUIPMENT_ARMOR : EQUIPMENT_HELM);

	for (int slot = firstSlot; slot <= EQUIPMENT_BOOTS; slot++)
	{
		ITEM* equipped = &CharacterMachine->Equipment[slot];
		int itemLevel = (equipped->Level >> 3) & 15;
		int additionalOption = (equipped->Level & 3) + (4 * ((equipped->Option1 & 0x40) >> 6));
		bool hasLuck = ((equipped->Level & 4) != 0);
		bool isExcellent = ((equipped->Option1 & 0x3F) != 0);

		if (equipped->Type != ITEM((slot + 5), IndexItem) || itemLevel < Level ||
			!hasLuck || additionalOption < 4 || (Exl > 0 && !isExcellent))
		{
			return false;
		}
	}

	return true;
}
#include "CBInterface.h"
int nCInfo::nInformationOP(int TextNum, ITEM* ItemSearch)
{
	if (gBuildSetBonusEnabled == false)
	{
		return TextNum;
	}

	//std::map<int, nInformation>::iterator it = GInfo.m_CustomInfo.find(ItemSearch);

	//if (it != GInfo.m_CustomInfo.end())
	//{

	//	wsprintf(TextList[TextNum], it->second.OptionName);
	//	TextListColor[TextNum] = it->second.OptionColor;
	//	TextBold[TextNum++] = 1;

	//	wsprintf(TextList[TextNum++], "\n");

	//	for (int i = 0; i < 15; i++)
	//	{
	//		if (it->second.TextIndex[i] == -1) { continue; }
	//		wsprintf(TextList[TextNum], "%s", nGetTextLine(it->second.TextIndex[i]));
	//		TextListColor[TextNum] = it->second.TextColor[i];
	//		TextBold[TextNum++] = 0;
	//	}
	//}
	//wsprintf(TextList[TextNum++], "\n");
	bool SetColorGray = false;

	for (int i = 0; i < this->m_CustomInfoSet.size(); i++)
	{
		if (this->m_CustomInfoSet[i].ItemIndexMin == -1 || this->m_CustomInfoSet[i].ItemIndexMax == -1) continue;

		int itemGroup = (ItemSearch->Type / 512);
		if (itemGroup < 7 || itemGroup > 11) continue;
		int GetIndexItemSet = (ItemSearch->Type % 512);

		//gInterface.DrawMessage(1, "Get Set %d ~ %d/%d ", ItemSearch->Type, this->m_CustomInfoSet[i].ItemIndexMin, this->m_CustomInfoSet[i].ItemIndexMax);
		if (GetIndexItemSet >= this->m_CustomInfoSet[i].ItemIndexMin && GetIndexItemSet <= this->m_CustomInfoSet[i].ItemIndexMax)
		{

			wsprintf(TextList[TextNum], this->m_CustomInfoSet[i].OptionName);
			TextListColor[TextNum] = this->m_CustomInfoSet[i].OptionColor;
			TextBold[TextNum++] = 1;

			wsprintf(TextList[TextNum++], "\n");

			// O conjunto principal exibe o titulo aqui, mas as linhas cinzas
			// sao desenhadas depois das opcoes azuis do item.
			if (this->m_CustomInfoSet[i].Index == 0)
			{
				continue;
			}

			for (int ib = 0; ib < 15; ib++)
			{
				if (!nGetTextNum(this->m_CustomInfoSet[i].TextIndex[ib])) { continue; }

				if (CheckItemSlotOpt(GetIndexItemSet, this->m_CustomInfoSet[i].LevelItem[ib], this->m_CustomInfoSet[i].Exc))
				{
					//g_Console.AddMessage(1, "Class %d Set %d CountSlot  %d Exl %d %d ", byFirstClass, GetIndexItemSet, CountSlot, (int)ItemSearch->ExcellentOption, it->second.Exc);
					SetColorGray = false;
				}
				else
				{
					SetColorGray = true;
				}

				wsprintf(TextList[TextNum], "%s", nGetTextLine(this->m_CustomInfoSet[i].TextIndex[ib]));

				TextListColor[TextNum] = (SetColorGray) ? 10 : this->m_CustomInfoSet[i].TextColor[ib];
				TextBold[TextNum++] = 0;
			}
			wsprintf(TextList[TextNum++], "\n");
		}
	}

	for (int iT = 0; iT < this->m_CustomInfo.size(); iT++)
	{
		int Level = (ItemSearch->Level >> 3) & 15;
		if (ItemSearch->Type < this->m_CustomInfo[iT].ItemIndexMin || ItemSearch->Type > this->m_CustomInfo[iT].ItemIndexMax) continue;
		if ((Level < this->m_CustomInfo[iT].LevelMin && this->m_CustomInfo[iT].LevelMin != -1) || (Level > this->m_CustomInfo[iT].LevelMax && this->m_CustomInfo[iT].LevelMax != -1))		continue;
		if (this->m_CustomInfo[iT].Anc != -1 && (ItemSearch->ExtOption % 0x04) == 0)		continue;



		wsprintf(TextList[TextNum], this->m_CustomInfo[iT].OptionName);
		TextListColor[TextNum] = this->m_CustomInfo[iT].OptionColor;
		TextBold[TextNum++] = 1;

		wsprintf(TextList[TextNum++], "\n");

		//gInterface.DrawMessage(1, "Get %d ~ %d ", ItemSearch->Type, (int)((ItemSearch->Level >> 3) & 15));

		for (int ibx = 0; ibx < 15; ibx++)
		{
			if (!nGetTextNum(this->m_CustomInfo[iT].TextIndex[ibx])) { continue; }

			if ((this->m_CustomInfo[iT].LevelItem[ibx] > (int)((ItemSearch->Level >> 3) & 15) && this->m_CustomInfo[iT].LevelItem[ibx] != -1) || ((int)ItemSearch->Option1 < this->m_CustomInfo[iT].Exc && this->m_CustomInfo[iT].Exc != -1))
			{
				SetColorGray = true;
			}
			else
			{
				SetColorGray = false;
			}
			wsprintf(TextList[TextNum], "%s", nGetTextLine(this->m_CustomInfo[iT].TextIndex[ibx]));

			TextListColor[TextNum] = (SetColorGray) ? 10 : this->m_CustomInfo[iT].TextColor[ibx];
			TextBold[TextNum++] = 0;
		}
		wsprintf(TextList[TextNum++], "\n");


	}

	return TextNum;
}


int nCInfo::nBuildSetBonusRows(int TextNum, ITEM* ItemSearch)
{
	if (gBuildSetBonusEnabled == false || ItemSearch == NULL)
	{
		return TextNum;
	}

	const int itemGroup = (ItemSearch->Type / 512);
	if (itemGroup < 7 || itemGroup > 11)
	{
		return TextNum;
	}

	const int itemIndex = (ItemSearch->Type % 512);
	for (int i = 0; i < (int)this->m_CustomInfoSet.size(); ++i)
	{
		const nInformation& info = this->m_CustomInfoSet[i];
		if (info.Index != 0 || info.ItemIndexMin == -1 || info.ItemIndexMax == -1)
		{
			continue;
		}
		if (itemIndex < info.ItemIndexMin || itemIndex > info.ItemIndexMax)
		{
			continue;
		}

		// Separa visualmente as opcoes azuis das linhas de progressao do set.
		wsprintf(TextList[TextNum++], "\n");
		int maxVisibleLevel = 13;
		if (gmProtect != NULL)
		{
			maxVisibleLevel = (int)gmProtect->max_item_level_on;
		}
		if (maxVisibleLevel < 10) maxVisibleLevel = 10;
		if (maxVisibleLevel > 15) maxVisibleLevel = 15;

		for (int row = 0; row < 6; ++row)
		{
			if (info.LevelItem[row] < 10 || info.LevelItem[row] > maxVisibleLevel)
			{
				continue;
			}
			if (!nGetTextNum(info.TextIndex[row]))
			{
				continue;
			}

			const bool active = CheckItemSlotOpt(itemIndex, info.LevelItem[row], info.Exc);
			wsprintf(TextList[TextNum], "%s", nGetTextLine(info.TextIndex[row]));
			TextListColor[TextNum] = active ? info.TextColor[row] : TEXT_COLOR_GRAY;
			TextBold[TextNum++] = 0;
		}
		break;
	}

	return TextNum;
}

