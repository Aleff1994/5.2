#include "stdafx.h"
#include "../../Utilities/PvpTierPolicy.h"
#include "TextClien.h"
#include "GMHellas.h"
#include "UIManager.h"
#include "PortalMgr.h"
#include "MapManager.h"
#include "NewUISystem.h"
#include "NewUIOptionWindow.h"
#include "CSItemOption.h"
#include "SocketSystem.h"
#include "SkillManager.h"
#include "GIPetManager.h"
#include "UIJewelHarmony.h"
#include "wsclientinline.h"
#include "CGMItemWings.h"
#include "CGMProtect.h"
#include "CGMResetManager.h"
#include "EventEntryLevel.h"
#include "IndependentTierView.h"


#include "../../Utilities/ExcRank/ExcRankTooltip.h"

#include "PersonalShopTitleImp.h"
#include "NewUIItemTooltip.h"

namespace
{
	bool NeonTooltipUsesWindowScale()
	{
		if (g_pOption == NULL)
			return false;

		return g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_INVENTORY) ||
			g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_STORAGE) ||
			g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_MIXINVENTORY) ||
			g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_NPCSHOP) ||
			g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_MYSHOP_INVENTORY) ||
			g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_PURCHASESHOP_INVENTORY) ||
			g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_TRADE);
	}

	void NeonScaleTooltipPoint(int& x, int& y)
	{
		if (!NeonTooltipUsesWindowScale())
			return;

		int value = g_pOption->GetScaleWindows();
		if (value < 50) value = 50;
		if (value > 150) value = 150;
		const float scale = (float)value / 100.0f;
		if (scale == 1.0f)
			return;

		x = (int)(320.0f + (((float)x - 320.0f) * scale));
		y = (int)(240.0f + (((float)y - 240.0f) * scale));
	}
}

int g_iChaosCastleZen[6] = { 25, 80, 150, 250, 400, 650 };
int g_iChaosCastleLevel[12][2] = {
						{ 15, 49 }, { 50, 119 }, { 120, 179 }, { 180, 239 }, { 240, 299 }, { 300, 999 },
						{ 15, 29 }, { 30,  99 }, { 100, 159 }, { 160, 219 }, { 220, 279 }, { 280, 999 }
};

CNewUIItemTooltip::CNewUIItemTooltip()
{
	m_ItemLevel = -1;

#if MAIN_UPDATE > 303
#ifdef CHINESE_LANGUAGE
	this->OpenItemTooltip("Data\\Local\\itemtooltip.bmd");

	this->OpenItemTooltipText("Data\\Local\\ItemTooltipText.bmd");

	this->OpenItemLevelTooltip("Data\\Local\\ItemLevelTooltip.bmd");
#else
	char filename[MAX_PATH];

	sprintf_s(filename, "Data\\Local\\%s\\itemtooltip.bmd", g_strSelectedML.c_str());
	this->OpenItemTooltip(filename);

	sprintf_s(filename, "Data\\Local\\%s\\ItemTooltipText.bmd", g_strSelectedML.c_str());
	this->OpenItemTooltipText(filename);

	sprintf_s(filename, "Data\\Local\\%s\\ItemLevelTooltip.bmd", g_strSelectedML.c_str());
	this->OpenItemLevelTooltip(filename);
#endif // CHINESE_LANGUAGE


#else
	this->OpenItemTooltip("Data\\Local\\itemtooltip.bmd");

	this->OpenItemTooltipText("Data\\Local\\ItemTooltipText.bmd");

	this->OpenItemLevelTooltip("Data\\Local\\ItemLevelTooltip.bmd");
#endif
}

CNewUIItemTooltip::~CNewUIItemTooltip()
{
	m_ItemTooltip.clear();
	m_ItemTooltipText.clear();
	m_ItemTooltipLevel.clear();
}

void CNewUIItemTooltip::OpenFileTooltip()
{
	char folder[MAX_PATH];
#if MAIN_UPDATE > 303 && !defined(CHINESE_LANGUAGE)
	sprintf_s(folder, "Data\\Local\\%s\\", g_strSelectedML.c_str());
#else
	sprintf_s(folder, "Data\\Local\\");
#endif
	const std::string base(folder);
	const std::string itemPath = base + "ItemTooltip.bmd";
	const std::string textPath = base + "ItemTooltipText.bmd";
	const std::string levelPath = base + "ItemLevelTooltip.bmd";
	// Keep the current bank intact if any translated file is unavailable.
	for (const std::string* path : { &itemPath, &textPath, &levelPath })
	{
		const DWORD attributes = GetFileAttributesA(path->c_str());
		if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY))
			return;
	}
	std::vector<_ITEM_TOOLTIP_DATA> items;
	std::vector<_ITEM_TOOLTIP_TEXT> texts;
	std::vector<_ITEM_LEVEL_TOOTIP_DATA> levels;
	if (PackFileDecrypt(itemPath, items, MAX_ITEM, sizeof(_ITEM_TOOLTIP_DATA), 0xE2F1) != MAX_ITEM ||
		PackFileDecrypt(textPath, texts, MAX_TEXT_TOOLTIP, sizeof(_ITEM_TOOLTIP_TEXT), 0xE2F1) <= 0 ||
		PackFileDecrypt(levelPath, levels, MAX_LEVEL_TOOLTIP, sizeof(_ITEM_LEVEL_TOOTIP_DATA), 0xE2F1) <= 0)
		return;

    std::vector<Script_Item> catalog;
    const std::string catalogPath = base + "Item.bmd";
    if (GetFileAttributesA(catalogPath.c_str()) != INVALID_FILE_ATTRIBUTES &&
        PackFileDecrypt(catalogPath, catalog, 0, sizeof(Script_Item), 0xE2F1) > 0)
    {
        std::map<int, std::string> names;
        for (const auto& row : catalog)
            if (row.Type >= 0 && row.Type < MAX_ITEM && row.Name[0])
                names[row.Type] = std::string(row.Name, strnlen(row.Name, sizeof(row.Name)));
        for (auto& row : items)
        {
            const auto name = names.find(GET_ITEM(row.Type, row.Index));
            if (name != names.end())
            {
                const std::string localized = gTextClien.LocalizeItemName(name->second);
                strncpy_s(row.Name, localized.c_str(), _TRUNCATE);
            }
        }
    }
	m_ItemTooltip.swap(items);
	m_ItemTooltipText.swap(texts);
	m_ItemTooltipLevel.swap(levels);
	// Refresh display names only; do not replace item stats or invalidate item pointers.
	for (const auto& item : m_ItemTooltip)
	{
		if (item.Type >= MAX_ITEM_TYPE || item.Index >= MAX_ITEM_INDEX || item.Name[0] == 0)
			continue;
		Script_Item* attribute = GMItemMng->find(GET_ITEM(item.Type, item.Index));
		if (attribute != NULL)
		{
			memcpy(attribute->Name, item.Name, sizeof(attribute->Name));
			attribute->Name[sizeof(attribute->Name) - 1] = 0;
		}
	}

	current_item = NULL;
	m_ItemLevel = -1;
	position_name = -1;
}

void CNewUIItemTooltip::OpenItemTooltip(const char* filename)
{
	m_ItemTooltip.clear();

	int Size = PackFileDecrypt(filename, m_ItemTooltip, MAX_ITEM, sizeof(_ITEM_TOOLTIP_DATA), 0xE2F1);

	if (Size != MAX_ITEM || Size == 0)
	{
		SendMessage(gwinhandle->GethWnd(), WM_DESTROY, 0, 0);
	}
}

void CNewUIItemTooltip::OpenItemTooltipText(const char* filename)
{
	int Size = PackFileDecrypt(filename, m_ItemTooltipText, MAX_TEXT_TOOLTIP, sizeof(_ITEM_TOOLTIP_TEXT), 0xE2F1);

	if (Size == 0)
	{
		SendMessage(gwinhandle->GethWnd(), WM_DESTROY, 0, 0);
	}
}

void CNewUIItemTooltip::OpenItemLevelTooltip(const char* filename)
{
	int Size = PackFileDecrypt(filename, m_ItemTooltipLevel, MAX_LEVEL_TOOLTIP, sizeof(_ITEM_LEVEL_TOOTIP_DATA), 0xE2F1);

	if (Size == 0)
	{
		SendMessage(gwinhandle->GethWnd(), WM_DESTROY, 0, 0);
	}
}

namespace {
// Independent ranked options are displayed below all preserved native rows.
void NeonAppendRankBlock(const IndependentTierViewProtocol::Message& data,int& count) {
 if(data.status!=IndependentTierViewProtocol::Status::Found && data.metadata.options.empty())return;
 const unsigned kind[]={0,92,203,207,97,206,204,91,202,89,88,205,86};
 std::vector<ExcRankTooltip::Line> compact,detailed;
 detailed.emplace_back("------------------------------",TEXT_COLOR_GRAY);

 for(int bit=5;bit>=0;--bit) {
  const auto grade=data.metadata.Get(IndependentItemTiers::Family::Excellent,static_cast<std::uint16_t>(bit));
  if(grade<IndependentItemTiers::Grade::B || grade>IndependentItemTiers::Grade::S || !data.optionTypes[bit])continue;
  char effect[256]={},line[256]={};IndependentTierView::FormatResolvedOption(effect,sizeof(effect),data.optionTypes[bit],data.optionValues[bit]);
  if(!effect[0])continue;
  _snprintf_s(line,sizeof(line),_TRUNCATE,"[%s] %s",IndependentTierView::OptionQualityName(grade),effect);
  const int color=IndependentTierView::OptionQualityColor(grade);
  compact.emplace_back(line,color,true);detailed.emplace_back(line,color,true);
 }
 for(unsigned n=0;n<data.ranked.slots.size();++n) {
  const auto slot=data.ranked.slots[n];if(slot.option<1||slot.option>12)continue;
  char effect[256]={},line[256]={};IndependentTierView::FormatResolvedOption(effect,sizeof(effect),kind[slot.option],data.rankedValues[n]);
  _snprintf_s(line,sizeof(line),_TRUNCATE,"[ %s ] %s",ExcRank::RankName(slot.rank),effect);
  const int color=slot.rank==ExcRank::Rank::Rare?ExcellentRankStyle::Rare:slot.rank==ExcRank::Rank::Legendary?ExcellentRankStyle::Legendary:ExcellentRankStyle::Epic;
  compact.emplace_back(line,color,true);detailed.emplace_back(line,color,true);

 }
 if(data.ranked.attributes[0].id){
  const char* names[]={"",gTextClien.Select("Forca","Strength","Fuerza"),gTextClien.Select("Agilidade","Agility","Agilidad"),gTextClien.Select("Vitalidade","Vitality","Vitalidad"),gTextClien.Select("Energia","Energy","Energia")};
  detailed.emplace_back("------------------------------",TEXT_COLOR_GRAY);
  detailed.emplace_back(gTextClien.Select("Bonus Exclusivo Excelente:","Exclusive Excellent Bonus:","Bonus Exclusivo Excelente:"),TEXT_COLOR_GREEN,true);
  for(auto attribute:data.ranked.attributes)if(attribute.id>=1&&attribute.id<=4){char line[256]={};_snprintf_s(line,sizeof(line),_TRUNCATE,"%s +%u",names[attribute.id],attribute.value);detailed.emplace_back(line,TEXT_COLOR_BLUE);compact.emplace_back(line,TEXT_COLOR_BLUE);}
 }
 if(!ExcRankTooltip::Append(TextList,TextListColor,TextBold,count,detailed))ExcRankTooltip::Append(TextList,TextListColor,TextBold,count,compact);
}
void NeonAppendGearScore(const IndependentTierViewProtocol::Message& data,int& count,int nameRow) {
 if(!data.scoreValid||nameRow<0||nameRow>=count)return;
 const int add=data.gradeEnabled?4:2;if(count+add>90)return;
 const int at=(std::min)(count,nameRow+2);
 memmove(TextList+at+add,TextList+at,(count-at)*sizeof(TextList[0]));memmove(TextListColor+at+add,TextListColor+at,(count-at)*sizeof(TextListColor[0]));memmove(TextBold+at+add,TextBold+at,(count-at)*sizeof(TextBold[0]));
 sprintf_s(TextList[at],sizeof(TextList[at]),gTextClien.Select("Pontuacao Base: %u","Base Score: %u","Puntuacion Base: %u"),data.baseScore);TextListColor[at]=TEXT_COLOR_YELLOW;TextBold[at]=false;
 if(data.gradeEnabled){sprintf_s(TextList[at+1],sizeof(TextList[at+1]),gTextClien.Select("Pontuacao de Grau: %u","Grade Score: %u","Puntuacion de Grado: %u"),data.gradeScore);TextListColor[at+1]=ExcellentRankStyle::Epic;TextBold[at+1]=false;sprintf_s(TextList[at+2],sizeof(TextList[at+2]),"Gear Score: %u",data.baseScore+data.gradeScore);TextListColor[at+2]=TEXT_COLOR_BLUE;TextBold[at+2]=false;}
 sprintf_s(TextList[at+add-1],sizeof(TextList[at+add-1]),"------------------------------");TextListColor[at+add-1]=TEXT_COLOR_GRAY;TextBold[at+add-1]=false;
 count+=add;
}
}

void CNewUIItemTooltip::RenderItemTooltip(int sx, int sy, ITEM* pItem, bool Sell, int InvenType, bool bItemTextListBoxUse, bool bRender3d)
{
	this->current_item = NULL;

	if (!pItem || pItem->Type == -1)
		return;

 IndependentTierViewProtocol::Message independentTier;
 const bool hasIndependentTier=IndependentTierView::Get().Inspect(pItem,independentTier);
 const bool rich=true; // Reference presentation applies to every item; data remains item-specific.

 this->m_RichTooltip=rich;
 bRender3d=bRender3d || rich;

	// Tooltip/informacoes acompanham exatamente a mesma escala de JANELAS.
	NeonScaleTooltipPoint(sx, sy);

	tm* ExpireTime;

	int ItemType = pItem->Type;

	if (pItem->bPeriodItem == true && pItem->bExpiredPeriod == false)
	{
		_tzset();
		if (pItem->lExpireTime == 0)
			return;

		ExpireTime = localtime((time_t*)&(pItem->lExpireTime));
	}

	this->m_ItemLevel = -1;
	this->position_name = -1;


	int ItemLevel = (pItem->Level >> 3) & 0xF;

	Script_Item* pItemAttr = GMItemMng->find(ItemType);

	if (!pItemAttr)
		return;

	int LineNum = 0;
	int SkipLine = 0;

	_ITEM_TOOLTIP_DATA* tooltipdata = FindTooltip(ItemType);

	this->current_item = pItem;

	if (tooltipdata)
	{
		_ITEM_LEVEL_TOOTIP_DATA* tooltiplevel = NULL;

		if (tooltipdata->ItemLevel >= 0)
		{
			int FindLevel;

			if (ItemType == (ITEM_HELPER + 37))
			{
				switch (pItem->Option1)
				{
				case 1:
					m_ItemLevel = 1;
					break;
				case 2:
					m_ItemLevel = 2;
					break;
				case 4:
					m_ItemLevel = 3;
					break;
				case 8:
					m_ItemLevel = 4;
					break;
				case 16:
					m_ItemLevel = 5;
					break;
				default:
					m_ItemLevel = 0;
					break;
				}

				FindLevel = tooltipdata->ItemLevel + m_ItemLevel;
			}
			else
			{
				FindLevel = tooltipdata->ItemLevel + ItemLevel;
			}
			tooltiplevel = FindLevelTooltip(FindLevel);

			if (!tooltiplevel)
			{
				return;
			}
		}

		memset(TextList, 0, sizeof(TextList));
		memset(TextBold, 0, sizeof(TextBold));
		memset(TextListColor, 0, sizeof(TextListColor));

		int color = TEXT_COLOR_GREEN_BLUE;

		if (IsAbsoluteWeapon(ItemType))
		{
			if (tooltipdata->ItemLevel != -1 && tooltiplevel)
				color = tooltiplevel->ColorName;
			else
				color = tooltipdata->ColorName;
		}
		else if (IsEnableOptionWing(ItemType) == 1)
		{
			if (ItemLevel < 7)
				color = tooltipdata->ColorName;
			else
				color = TEXT_COLOR_YELLOW;
		}
		else if ((pItem->ExtOption % 0x04) != EXT_A_SET_OPTION && (pItem->ExtOption % 0x04) != EXT_B_SET_OPTION)
		{
			if (g_SocketItemMgr.IsSocketItem(pItem) == 1)
			{
				color = TEXT_COLOR_VIOLET;
			}
			else if (pItem->SpecialNum <= 0 || (pItem->Option1 & 63) <= 0)
			{
				if (ItemLevel < 7)
				{
					if (tooltipdata->ItemLevel != -1 && tooltiplevel)
					{
						color = tooltiplevel->ColorName;
					}
					else
					{
						if (pItem->SpecialNum <= 0)
							color = tooltipdata->ColorName;
						else
							color = TEXT_COLOR_BLUE;
					}
				}
				else
				{
					color = TEXT_COLOR_YELLOW;
				}
			}
			else
			{
				color = TEXT_COLOR_GREEN;
			}
		}
		else
		{
			color = TEXT_COLOR_GREEN_BLUE;
		}

		if (ItemType != ITEM_HELPER + 4 && ItemType != ITEM_HELPER + 5)
		{
			if (pItem->Kind3 == ItemKind3::PET_GAIN_EXPERIENCE)
			{
				if ((g_pMyInventory->GetPointedItemIndex()) == EQUIPMENT_HELPER)
				{
					SendRequestPetInfo(PET_TYPE_DARK_HORSE, InvenType, EQUIPMENT_HELPER);
				}
			}

			if (g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_NPCSHOP) && !tooltipdata->NoTaxGold)
			{
				if (!g_pMyInventory->IsInvenItem(ItemType) || IsLuckySetItem(ItemType) && !pItem->Durability)
				{
					char value[100] = { 0 };

					if (Sell)
					{
						unicode::t_char val64[100];

						ConvertGold(ItemValue(pItem, 0), value, 0);
						ConvertGold64(ItemValue(pItem, 0), val64);
						sprintf(TextList[LineNum], GlobalText[1620], val64, value);
					}
					else
					{
						ConvertGold(ItemValue(pItem, 1), value, 0);
						sprintf(TextList[LineNum], GlobalText[63], value);
					}

					TextListColor[LineNum] = color;
					TextBold[LineNum++] = tooltipdata->FontBold;

					sprintf(TextList[LineNum++], "\n");
					SkipLine++;
				}
			}

			if ((InvenType == SEASON3B::TOOLTIP_TYPE_MY_SHOP || InvenType == SEASON3B::TOOLTIP_TYPE_PURCHASE_SHOP) && !IsPersonalShopBan(pItem))
			{
				PERSONAL_ITEM_PRICE_TABLE price_table;

				int price = 0;
				char Text[100] = {0, };
				int indexInv = pItem->x + (COL_PERSONALSHOP_INVEN * pItem->y) + MAX_MY_INVENTORY_INDEX;

				if (GetPersonalItemPrice(indexInv, price_table, g_IsPurchaseShop))
				{
					DWORD gold = 0;
					price = price_table.Price;

					if (price > 0)
					{
						if (price_table.PersonalType == 1)
						{
							ConvertGold(price, Text);
							sprintf(TextList[LineNum], TextGlobal[55], Text);
							gold = CharacterMachine->Coin1;
						}
						else if (price_table.PersonalType == 2)
						{
							ConvertGold(price, Text);
							sprintf(TextList[LineNum], TextGlobal[56], Text);
							gold = CharacterMachine->Coin2;
						}
						else if (price_table.PersonalType == 3)
						{
							ConvertGold(price, Text);
							sprintf(TextList[LineNum], TextGlobal[57], Text);
							gold = CharacterMachine->Coin3;
						}
						else
						{
							ConvertGold(price, Text);
							sprintf(TextList[LineNum], TextGlobal[49], Text);
							gold = CharacterMachine->Gold;
						}

						if (price >= 10000000)
							TextListColor[LineNum] = TEXT_COLOR_RED;
						else if (price >= 1000000)
							TextListColor[LineNum] = TEXT_COLOR_YELLOW;
						else if (price >= 100000)
							TextListColor[LineNum] = TEXT_COLOR_GREEN;
						else
							TextListColor[LineNum] = TEXT_COLOR_WHITE;
						TextBold[LineNum++] = true;

						if ((int)gold < price && g_IsPurchaseShop == PSHOPWNDTYPE_PURCHASE)
						{
							TextListColor[LineNum] = TEXT_COLOR_RED;
							TextBold[LineNum] = true;
							sprintf(TextList[LineNum++], GlobalText[423]);
							sprintf(TextList[LineNum++], "\n");
							SkipLine++;
						}
					}

					price = price_table.JewelOfBless;

					if (price > 0)
					{
						ConvertGold(price, Text);
						sprintf(TextList[LineNum], TextGlobal[61], Text);
						TextListColor[LineNum] = 18;
						TextBold[LineNum++] = true;
					}

					price = price_table.JewelOfSoul;

					if (price > 0)
					{
						ConvertGold(price, Text);
						sprintf(TextList[LineNum], TextGlobal[62], Text);
						TextListColor[LineNum] = 18;
						TextBold[LineNum++] = true;
					}

					price = price_table.JewelOfChaos;

					if (price > 0)
					{
						ConvertGold(price, Text);
						sprintf(TextList[LineNum], TextGlobal[63], Text);
						TextListColor[LineNum] = 18;
						TextBold[LineNum++] = true;
					}

					sprintf(TextList[LineNum++], "\n");
					SkipLine++;
				}
				else if (g_IsPurchaseShop == PSHOPWNDTYPE_SALE)
				{
					TextListColor[LineNum] = TEXT_COLOR_RED;
					TextBold[LineNum] = true;
					sprintf(TextList[LineNum++], GlobalText[1101]);
					sprintf(TextList[LineNum++], "\n");
					SkipLine++;
				}
			}

			char TextName[MAX_PATH];

			memset(TextName, 0, sizeof(TextName));

			if (!g_csItemOption.GetSetItemName(TextName, ItemType, pItem->ExtOption) || IsAbsoluteWeapon(ItemType))
				strcat(TextName, tooltipdata->Name);
			else
				strcat(TextName, tooltipdata->Name);

			if ((pItem->Option1 & 0x3F) <= 0 || IsAbsoluteWeapon(ItemType) || IsEnableOptionWing(ItemType))
			{
				if (tooltipdata->ItemLevel != -1 && tooltiplevel)
				{
					sprintf(TextList[LineNum], tooltiplevel->Name);
				}
				else
				{
					if (ItemLevel && tooltipdata->RenderLevel)
						sprintf(TextList[LineNum], "%s +%d", TextName, ItemLevel);
					else
						sprintf(TextList[LineNum], TextName);
				}
			}
			else if (m_ItemLevel != -1 && tooltiplevel)
			{
				sprintf(TextList[LineNum], tooltiplevel->Name);
				color = TEXT_COLOR_BLUE;
			}
			else
			{
				if (ItemLevel)
					sprintf(TextList[LineNum], "%s %s +%d", GlobalText[620], TextName, ItemLevel);
				else
					sprintf(TextList[LineNum], "%s %s", GlobalText[620], TextName);
			}
			TextListColor[LineNum] = color;
			this->position_name = LineNum;

			if (tooltipdata->ItemLevel != -1 && tooltiplevel)
				TextBold[LineNum++] = tooltiplevel->FontBold;
			else
				TextBold[LineNum++] = tooltipdata->FontBold;

			sprintf(TextList[LineNum++], "\n");
			SkipLine++;

            // Scores are inserted after native options to preserve their row budget.
			if (ItemType == ITEM_HELPER + 18 || ItemType == ITEM_HELPER + 19)
			{
				bool specialClass = (Hero->GetBaseClass() == Magic_Gladiator
					|| Hero->GetBaseClass() == Dark_Lord
					|| Hero->GetBaseClass() == Rage_Fighter
					|| Hero->GetBaseClass() == Grow_Lancer);
				bool bloodCastle = (ItemType == ITEM_HELPER + 18);
				const int (*ranges)[2] = bloodCastle
					? (specialClass ? gEventEntryLevel.m_BloodCastleEntryLevelSpecial : gEventEntryLevel.m_BloodCastleEntryLevelCommon)
					: (specialClass ? gEventEntryLevel.m_DevilSquareEntryLevelSpecial : gEventEntryLevel.m_DevilSquareEntryLevelCommon);
				int roomCount = bloodCastle ? 7 : 6;
				int currentValue = (gmProtect->EventEntryType == 1) ? GMResetManager->GetResetCount() :
					((gmProtect->EventEntryType == 2) ? GMResetManager->GetMasterResetCount() : CharacterAttribute->Level);
				const char* requirementName = (gmProtect->EventEntryType == 1) ? "Reset" :
					((gmProtect->EventEntryType == 2) ? "Master Reset" : ((gmProtect->EventEntryType == 3) ? gTextClien.Select("Livre", "Open", "Libre") : gTextClien.Select("Nivel", "Level", "Nivel")));

				sprintf(TextList[LineNum], "%s          %s", bloodCastle ? "Blood Castle" : "Devil Square", requirementName);
				TextListColor[LineNum] = TEXT_COLOR_WHITE;
				TextBold[LineNum++] = false;

				for (int i = 0; i < roomCount; i++)
				{
					if (gmProtect->EventEntryType == 3)
						sprintf(TextList[LineNum], gTextClien.Select("        %d                 Livre", "        %d                 Open", "        %d                 Libre"), i + 1);
					else
						sprintf(TextList[LineNum], "        %d                 %d~%d", i + 1, ranges[i][0], ranges[i][1]);

					bool selectedRoom = (gmProtect->EventEntryType == 3)
						? (i == roomCount - 1)
						: (currentValue >= ranges[i][0] && currentValue <= ranges[i][1]);
					TextListColor[LineNum] = selectedRoom ? TEXT_COLOR_DARKYELLOW : TEXT_COLOR_WHITE;
					TextBold[LineNum++] = false;
				}

				sprintf(TextList[LineNum++], "\n");
				SkipLine++;
			}
			else if (ItemType == ITEM_HELPER + 29)
			{
				bool specialClass = (Hero->GetBaseClass() == Magic_Gladiator
					|| Hero->GetBaseClass() == Dark_Lord
					|| Hero->GetBaseClass() == Rage_Fighter
					|| Hero->GetBaseClass() == Grow_Lancer);
				const int (*ranges)[2] = specialClass ? gEventEntryLevel.m_ChaosCastleEntryLevelSpecial : gEventEntryLevel.m_ChaosCastleEntryLevelCommon;
				int currentValue = (gmProtect->EventEntryType == 1) ? GMResetManager->GetResetCount() :
					((gmProtect->EventEntryType == 2) ? GMResetManager->GetMasterResetCount() : CharacterAttribute->Level);
				const char* requirementName = (gmProtect->EventEntryType == 1) ? "Reset" :
					((gmProtect->EventEntryType == 2) ? "Master Reset" : ((gmProtect->EventEntryType == 3) ? gTextClien.Select("Livre", "Open", "Libre") : gTextClien.Select("Nivel", "Level", "Nivel")));

				sprintf(TextList[LineNum], "Chaos Castle      %s          Zen", requirementName);
				TextListColor[LineNum] = TEXT_COLOR_WHITE;
				TextBold[LineNum++] = false;

				for (int i = 0; i < 6; i++)
				{
					int zen = gEventEntryLevel.m_ChaosCastleZen[i];
					if (gmProtect->EventEntryType == 3)
						sprintf(TextList[LineNum], gTextClien.Select("        %d                 Livre       %d", "        %d                 Open       %d", "        %d                 Libre       %d"), i + 1, zen);
					else
						sprintf(TextList[LineNum], "        %d             %d~%d       %d", i + 1, ranges[i][0], ranges[i][1], zen);

					if (gmProtect->EventEntryType == 3 || (currentValue >= ranges[i][0] && currentValue <= ranges[i][1]))
					{
						TextListColor[LineNum] = TEXT_COLOR_DARKYELLOW;
					}
					else
					{
						TextListColor[LineNum] = TEXT_COLOR_WHITE;
					}
					TextBold[LineNum++] = false;
				}
				sprintf(TextList[LineNum], "         %d          %s       %d", 7, GlobalText[737], gEventEntryLevel.m_ChaosCastleZen[6]);

				if (Hero->IsMasterLevel() == true)
					TextListColor[LineNum] = TEXT_COLOR_DARKYELLOW;
				else
					TextListColor[LineNum] = TEXT_COLOR_WHITE;

				TextBold[LineNum++] = false;


				sprintf(TextList[LineNum++], "\n");
				SkipLine++;

				sprintf(TextList[LineNum], GlobalText[1157]);
				TextListColor[LineNum] = TEXT_COLOR_DARKBLUE;
				TextBold[LineNum++] = false;
			}
			else if (ItemType == ITEM_POTION + 28)
			{
				LineNum = RenderHellasItemInfo(pItem, LineNum);
			}
			else if (ItemType == ITEM_HELPER + 70)
			{
				if (pItem->Durability == 2)
				{
					sprintf(TextList[LineNum], GlobalText[2605]);
					TextListColor[LineNum] = TEXT_COLOR_BLUE;
					TextBold[LineNum++] = false;
				}
				else if (pItem->Durability == 1)
				{
					sprintf(TextList[LineNum], GlobalText[2606]);
					TextListColor[LineNum] = TEXT_COLOR_BLUE;
					TextBold[LineNum++] = false;

					g_PortalMgr.GetPortalPositionText(TextList[LineNum]);
					TextListColor[LineNum] = TEXT_COLOR_BLUE;
					TextBold[LineNum++] = false;
				}
			}
			else
			{
				_ITEM_TOOLTIP_TEXT* text_info = NULL;

				if (tooltipdata->ItemLevel != -1 && tooltiplevel)
				{
					for (int i = 0; i < 8; ++i)
					{
						int iText = tooltiplevel->TextInfo[i].Text;
						int iColor = tooltiplevel->TextInfo[i].Color;
						int iBool = tooltiplevel->TextInfo[i].FontBold;

						text_info = FindTooltipText(iText);

						if (!text_info || (text_info->iStatus == 18
								|| text_info->iStatus == 19
								|| (text_info->iStatus >= 105 && text_info->iStatus <= 200)
								|| (text_info->iStatus >= 250 && text_info->iStatus != 0xffff)))
						{
							continue;
						}

						char Information[256];

						int Num = text_info->iStatus;
						memset(Information, 0, sizeof(Information));

						bool percent_data = false;
						if (Num != -1)
						{
							SetTooltipValueText(text_info->Description, Information, LineNum, &iColor, Num, pItem, pItemAttr, ItemLevel);
							percent_data = true;
						}
						else
						{
							memcpy(Information, text_info->Description, sizeof(Information));
						}
						LineNum = this->SplitText(Information, 1, LineNum, iColor, iBool, percent_data);
					}
				}
				else
				{
					for (int i = 0; i < 12; ++i)
					{
						int iText = tooltipdata->TextInfo[i].Text;
						int iColor = tooltipdata->TextInfo[i].Color;
						int iBool = tooltipdata->TextInfo[i].FontBold;

						text_info = FindTooltipText(iText);

						if (!text_info 
							|| (text_info->iStatus == 18
								|| text_info->iStatus == 19
								|| (text_info->iStatus >= 105 && text_info->iStatus <= 200)
								|| (text_info->iStatus >= 250 && text_info->iStatus != 0xffff)))
						{
							continue;
						}

						char Information[256];
						int Num = text_info->iStatus;
						memset(Information, 0, sizeof(Information));

						bool isPercent = false;
						if (Num != -1)
						{
							SetTooltipValueText(text_info->Description, Information, LineNum, &iColor, Num, pItem, pItemAttr, ItemLevel);
							isPercent = true;
						}
						else
						{
							memcpy(Information, text_info->Description, sizeof(Information));
						}

						LineNum = this->SplitText(Information, 1, LineNum, iColor, iBool, isPercent);
					}
				}
			}

			if (IsRequireClassRenderItem(ItemType) == 1)
				ItemEquipClass(pItemAttr, &LineNum, &SkipLine);

			LineNum = GInfo.nInformationOP(LineNum, pItem); //custom tool tip



			if (pItem->option_380 && !IsEnableOptionWing(ItemType))
			{
				if (g_pItemAddOptioninfo)
				{
					std::vector<std::string> base_aux;
					g_pItemAddOptioninfo->GetItemAddOtioninfoText(base_aux, ItemType,
                        PvpTierPolicy::Client().Percent(PvpTierPolicy::Level(pItem->PvpTierLevel,true)));
                    if(PvpTierPolicy::Client().enabled && PvpTierPolicy::Client().visual) {
                        sprintf(TextList[LineNum], "PvP +%d", PvpTierPolicy::Level(pItem->PvpTierLevel,true));
                        TextListColor[LineNum]=TEXT_COLOR_REDPURPLE; TextBold[LineNum++]=true;
                    }

					sprintf(TextList[LineNum++], "\n"); SkipLine++;

					for (int i = 0; i < (int)base_aux.size(); ++i)
					{
						strncpy(TextList[LineNum], base_aux[i].c_str(), 100);
						TextListColor[LineNum] = TEXT_COLOR_REDPURPLE;
						TextBold[LineNum++] = true;
					}
					sprintf(TextList[LineNum++], "\n");
					SkipLine++;
					base_aux.shrink_to_fit();
					base_aux.clear();
				}
			}

			if (!g_SocketItemMgr.IsSocketItem(pItem))
			{
				if (pItem->Jewel_Of_Harmony_Option > 0)
				{
					StrengthenItem type = g_pUIJewelHarmonyinfo->GetItemType(ItemType);

					if (type < SI_None)
					{
						if (g_pUIJewelHarmonyinfo->IsHarmonyJewelOption(type, pItem->Jewel_Of_Harmony_Option))
						{
							sprintf(TextList[LineNum++], "\n");
							SkipLine++;

							HARMONYJEWELOPTION harmonyjewel = g_pUIJewelHarmonyinfo->GetHarmonyJewelOptionInfo(type, pItem->Jewel_Of_Harmony_Option);

							if (type == SI_Defense && pItem->Jewel_Of_Harmony_Option == 7)
							{
								sprintf(TextList[LineNum], "%s +%d%%", harmonyjewel.Name, harmonyjewel.HarmonyJewelLevel[pItem->Jewel_Of_Harmony_OptionLevel]);
							}
							else
							{
								sprintf(TextList[LineNum], "%s +%d", harmonyjewel.Name, harmonyjewel.HarmonyJewelLevel[pItem->Jewel_Of_Harmony_OptionLevel]);
							}

							if (ItemLevel < pItem->Jewel_Of_Harmony_OptionLevel)
								TextListColor[LineNum] = TEXT_COLOR_GRAY;
							else
								TextListColor[LineNum] = TEXT_COLOR_YELLOW;

							TextBold[LineNum++] = true;
							sprintf(TextList[LineNum++], "\n");
							SkipLine++;
						}
						else
						{
							sprintf(TextList[LineNum++], "\n");
							SkipLine++;

							sprintf(TextList[LineNum], "%s : %d %d %d"
								, GlobalText[2204]
								, (int)type
								, (int)pItem->Jewel_Of_Harmony_Option
								, (int)pItem->Jewel_Of_Harmony_OptionLevel
							);

							TextListColor[LineNum] = TEXT_COLOR_DARKRED;
							TextBold[LineNum++] = true;

							sprintf(TextList[LineNum], GlobalText[2205]);

							TextListColor[LineNum] = TEXT_COLOR_DARKRED;
							TextBold[LineNum++] = true;
							sprintf(TextList[LineNum++], "\n"); SkipLine++;
						}
					}
				}
			}

			LineNum = g_csItemOption.RenderDefaultOptionText(pItem, LineNum);

			if (pItem->SpecialNum > 0 && (ItemType < ITEM_HELPER + 109 || ItemType > ITEM_HELPER + 115))
			{
				sprintf(TextList[LineNum++], "\n"); SkipLine++;
			}

            // Keep every native row/value. Luck, skill and additional damage
            // precede the actual Excellent options, regardless of storage order.
            bool excellentSeparator=false;
            for (int optionGroup=0; optionGroup<2; ++optionGroup)
            for (int i = 0; i < pItem->SpecialNum && (pItem->Type < ITEM_HELPER + 109 || pItem->Type > ITEM_HELPER + 115 || IsEnableOptionWing(pItem->Type)); i++)
            {
                const int excellentBit=IndependentTierView::ExcellentBit(pItem->Special[i]);
                const bool isExcellent=excellentBit>=0 && (pItem->Option1&(1u<<excellentBit));
                if(isExcellent!=(optionGroup==1))continue;
                if(isExcellent && hasIndependentTier) {
                    const auto grade=independentTier.metadata.Get(IndependentItemTiers::Family::Excellent,static_cast<std::uint16_t>(excellentBit));
                    if(grade>=IndependentItemTiers::Grade::B && grade<=IndependentItemTiers::Grade::S)continue;
                }
                // A native option can add a second explanatory row.
                if(LineNum+3>90)break;
                if(isExcellent && !excellentSeparator) {
                    sprintf_s(TextList[LineNum],sizeof(TextList[LineNum]),"------------------------------");
                    TextListColor[LineNum]=TEXT_COLOR_GRAY;TextBold[LineNum++]=false;
                    excellentSeparator=true;
                }
				int optionColor=TEXT_COLOR_BLUE;bool optionBold=false;
                int iMana;
				gSkillManager.GetSkillInformation(pItem->Special[i], 1, NULL, &iMana, NULL);
				GetSpecialOptionText(pItem->Type, TextList[LineNum], pItem->Special[i], pItem->SpecialValue[i], iMana);
                const int nativeBit=IndependentTierView::ExcellentBit(pItem->Special[i]);
                if(nativeBit>=0 && (pItem->Option1&(1u<<nativeBit)))optionColor=TEXT_COLOR_GRAY;



				TextListColor[LineNum] = optionColor;
				TextBold[LineNum++] = optionBold;


				if (pItem->Special[i] == AT_LUCK)
				{
					sprintf(TextList[LineNum], GlobalText[94], pItem->SpecialValue[i]);
					TextListColor[LineNum] = TEXT_COLOR_BLUE;
					TextBold[LineNum++] = false;
				}
				else if (pItem->Special[i] == AT_SKILL_RIDER)
				{
					sprintf(TextList[LineNum], GlobalText[179]);
					TextListColor[LineNum] = TEXT_COLOR_DARKRED;
					TextBold[LineNum++] = false;
				}
				else if (pItem->Special[i] != 62
					&& pItem->Special[i] != 510
					&& pItem->Special[i] != 516
					&& pItem->Special[i] != 512)
				{
					if (pItem->Special[i] == AT_IMPROVE_DAMAGE && (pItemAttr->Kind2 == ItemKind2::MAGIC_SWORD))
					{
						sprintf(TextList[LineNum], GlobalText[89], pItem->SpecialValue[i]);
						TextListColor[LineNum] = TEXT_COLOR_BLUE;
						TextBold[LineNum++] = false;
					}
				}
				else
				{
					sprintf(TextList[LineNum], GlobalText[1201]);
					TextListColor[LineNum] = TEXT_COLOR_DARKRED;
					TextBold[LineNum++] = false;
				}
			}

			// Bonus de progressao do set: fica abaixo das opcoes azuis do item.
			LineNum = GInfo.nBuildSetBonusRows(LineNum, pItem);

			SkipLine++;

			if (pItem->bPeriodItem == true)
			{
				if (pItem->bExpiredPeriod == true)
				{
					sprintf(TextList[LineNum], GlobalText[3266]);
					TextListColor[LineNum++] = TEXT_COLOR_RED;
				}
				else
				{
					sprintf(TextList[LineNum], GlobalText[3265]);
					TextListColor[LineNum++] = TEXT_COLOR_ORANGE;

					sprintf(TextList[LineNum], "%d-%02d-%02d  %02d:%02d", ExpireTime->tm_year + 1900, ExpireTime->tm_mon + 1, ExpireTime->tm_mday, ExpireTime->tm_hour, ExpireTime->tm_min);
					TextListColor[LineNum++] = TEXT_COLOR_BLUE;
				}
			}

			if (!bItemTextListBoxUse)
			{

				int PointedIndex = g_pMyInventory->GetPointedItemIndex();
				bool bThisisEquippedItem = false;

				if (PointedIndex == -1 || PointedIndex < EQUIPMENT_WEAPON_RIGHT || PointedIndex >= MAX_EQUIPMENT_INDEX)
					bThisisEquippedItem = true;

				LineNum = g_csItemOption.RenderSetOptionListInItem(pItem, LineNum, bThisisEquippedItem);

				LineNum = g_SocketItemMgr.AttachToolTipForSocketItem(pItem, LineNum);


            if(hasIndependentTier){NeonAppendGearScore(independentTier,LineNum,this->position_name);NeonAppendRankBlock(independentTier,LineNum);}

                // The renderer measures and fits the complete frame, including 3D.
				RenderTipTextList(sx, sy, LineNum, 0, RT3_SORT_CENTER, STRP_NONE, TRUE, bRender3d);
				this->current_item = NULL;
			}
			else
			{
                if(hasIndependentTier){NeonAppendGearScore(independentTier,LineNum,this->position_name);NeonAppendRankBlock(independentTier,LineNum);}
				RenderTipTextList(sx, sy, LineNum, 0, RT3_SORT_CENTER, STRP_BOTTOMCENTER, TRUE, bRender3d);
				this->current_item = NULL;
			}
		}
		else
		{
			if (ItemType == ITEM_HELPER + 4)
			{
				if ((g_pMyInventory->GetPointedItemIndex()) == EQUIPMENT_HELPER)
				{
					SendRequestPetInfo(PET_TYPE_DARK_HORSE, InvenType, EQUIPMENT_HELPER);
				}
			}
			else
			{
				if ((g_pMyInventory->GetPointedItemIndex()) == EQUIPMENT_WEAPON_LEFT)
				{
					SendRequestPetInfo(PET_TYPE_DARK_SPIRIT, InvenType, EQUIPMENT_WEAPON_LEFT);
				}
			}

			giPetManager::RenderPetItemInfo(sx, sy, pItem, InvenType);
			this->current_item = NULL;
		}
	}
}

void CNewUIItemTooltip::SetTooltipValueText(char* Description, char* information, int NextLine, int* color, int NumText, ITEM* pItem, Script_Item* pItemAtrr, int iItemLevel)
{
 IndependentTierViewProtocol::Message tierBase;const bool tierBaseReady=IndependentTierView::Get().Inspect(pItem,tierBase);
 const auto Base=[&](int value)->int {if(!tierBaseReady || value<=0)return value;const int scaled=static_cast<int>((static_cast<long long>(value)*tierBase.itemPercent+50)/100);return scaled<1?1:scaled;};
	switch (NumText)
	{
	case 0:
	{
		int minindex = 0;
		int maxindex = 0;
		int magicalindex = 0;

		if (iItemLevel >= pItem->Jewel_Of_Harmony_OptionLevel)
		{
			StrengthenCapability SC;
			g_pUIJewelHarmonyinfo->GetStrengthenCapability(&SC, pItem, 1);

			if (SC.SI_isSP)
			{
				minindex = SC.SI_SP.SI_minattackpower;
				maxindex = SC.SI_SP.SI_maxattackpower;
				magicalindex = SC.SI_SP.SI_magicalpower;
			}
		}
		int DamageMin = Base(pItem->DamageMin);
		int DamageMax = Base(pItem->DamageMax);

		if (minindex + DamageMin < maxindex + DamageMax)
			sprintf(information, Description, minindex + DamageMin, maxindex + DamageMax);
		else
			sprintf(information, Description, maxindex + DamageMax, maxindex + DamageMax);

		if (DamageMin > 0)
		{
			if (minindex || maxindex)
			{
				*color = TEXT_COLOR_YELLOW;
			}
			else if ((pItem->Option1 & 63) > 0)
			{
				*color = TEXT_COLOR_BLUE;
			}
		}
	}
	break;
	case 1:
		sprintf(information, Description, pItem->Durability);
		break;
	case 2:
	{
		int maxdefense = 0;
		if (iItemLevel >= pItem->Jewel_Of_Harmony_OptionLevel)
		{
			StrengthenCapability SC;
			g_pUIJewelHarmonyinfo->GetStrengthenCapability(&SC, pItem, 2);

			if (SC.SI_isSD)
			{
				maxdefense = SC.SI_SD.SI_defense;
			}
		}
		sprintf(information, Description, maxdefense + Base(pItem->Defense));
		if (maxdefense)
		{
			*color = TEXT_COLOR_YELLOW;
		}
		else if (pItem->Type >= ITEM_HELM && pItem->Type < ITEM_WING && (pItem->Option1 & 63) > 0)
		{
			*color = TEXT_COLOR_BLUE;
		}
	}
	break;
	case 3:
		sprintf(information, Description, Base(pItem->MagicDefense));
		break;
	case 4:
	{
		int maxDurability = calcMaxDurability(pItem, pItemAtrr, iItemLevel);
		sprintf(information, Description, pItem->Durability, maxDurability);
	}
	break;
	case 5:
	{
		WORD Level = 0;
		if (Hero->IsMasterLevel() == true)
		{
			Level = CharacterAttribute->Level + Master_Level_Data.nMLevel;
		}
		else
		{
			Level = CharacterAttribute->Level;
		}
		sprintf(information, Description, pItem->RequireLevel);
		if (Level < pItem->RequireLevel)
		{
			*color = TEXT_COLOR_RED;
			TextListColor[NextLine - 1] = *color;
		}
		break;
	}
	case 6:
	{
		int iDecNeedStrength = 0;
		int iDecNeedDex = 0;
		CNewUIItemTooltip::SetNeedStrDex(&iDecNeedStrength, &iDecNeedDex, iItemLevel, pItem);
		sprintf(information, Description, pItem->RequireStrength - iDecNeedStrength);
	}
	break;
	case 7:
	{
		int iDecNeedStrength = 0;
		int iDecNeedDex = 0;
		CNewUIItemTooltip::SetNeedStrDex(&iDecNeedStrength, &iDecNeedDex, iItemLevel, pItem);
		sprintf(information, Description, pItem->RequireDexterity - iDecNeedDex);
	}
	break;
	case 8:
		sprintf(information, Description, pItem->RequireVitality);
		break;
	case 9:
		sprintf(information, Description, pItem->RequireEnergy);
		break;
	case 10:
		sprintf(information, Description, pItem->RequireCharisma);
		break;
	case 11:
		sprintf(information, Description, Base(pItem->MagicPower));
		break;
	case 12:
	{
		int DamageMin = Base(pItem->DamageMin);
		int DamageMax = Base(pItem->DamageMax);

		if ((pItem->Type / MAX_ITEM_INDEX) == 15)
		{
			sprintf(information, Description, DamageMin, DamageMax);
		}
		else
		{
			int SkillIndex = pItemAtrr->m_wSkillIndex;

			SKILL_ATTRIBUTE* skillAtt = &SkillAttribute[SkillIndex];

			DamageMin = skillAtt->Damage;
			DamageMax = skillAtt->Damage + (skillAtt->Damage * 0.5f);
			sprintf(information, Description, DamageMin, DamageMax);
		}

		if (DamageMin > 0 && (pItem->Option1 & 63) > 0)
			*color = TEXT_COLOR_BLUE;
	}
	break;
	case 13:
		if (m_ItemLevel == -1 && iItemLevel < 5)
			sprintf(information, "");
		else
			sprintf(information, Description);
		break;
	case 14:
		sprintf(information, Description, iItemLevel + 1);
		break;
	case 15:
		sprintf(information, Description, Base(pItem->SuccessfulBlocking));
		if ((pItem->Option1 & 63) > 0)
			*color = 1;
		break;
	case 16:
		sprintf(information, Description, 10 * pItem->Durability);
		break;
	case 17:
		{
			int iResistance = 0;
			if (pItem->Type == ITEM_HELPER + 174)
			{
				if (pItem->check_status(1))
				{
					iResistance = iItemLevel + 2;
				}
				else if (iItemLevel)
				{
					iResistance = iItemLevel;
				}
				else
				{
					iResistance++;
				}
			}
			else
			{
				iResistance = iItemLevel + 1;
				if (pItem->check_status(1))
					iResistance += iItemLevel + 2;
			}
			sprintf(information, Description, iResistance);
		}
		break;
	case 20:
		{
			PET_INFO* pPetInfo = giPetManager::GetPetInfo(pItem);
			if (pPetInfo->m_dwPetType != 0)
				sprintf(information, Description, pPetInfo->m_dwExp1, pPetInfo->m_dwExp2);
			else
				sprintf(information, Description, 0, 0);
		}
		break;
	case 21:
		{
			PET_INFO* pPetInfo = giPetManager::GetPetInfo(pItem);
			if (pPetInfo->m_dwPetType != 0)
				sprintf(information, Description, pPetInfo->m_wLevel);
			else
				sprintf(information, Description, 0);
		}
		break;
	case 101:
		sprintf(information, Description, pItemAtrr->WeaponSpeed);
		break;
	case 102:
		sprintf(information, Description, pItemAtrr->WalkSpeed);
		break;
	case 103:
	{
		BYTE RankedMuun = pItem->SocketSeedSetOption;
		if (pItemAtrr->IsKind1(ItemKind1::MUUN_INVENTORY) && RankedMuun != 0xFF)
		{
			sprintf(information, Description, "O");
			for (int i = 0; i < RankedMuun; i++)
			{
				strcat(information, "O");
			}
		}
	}
	break;
	case 104:
	{
		BYTE RankedMuun = pItem->SocketSeedSetOption;
		if (pItemAtrr->IsKind1(ItemKind1::MUUN_INVENTORY) && RankedMuun != 0xFF)
		{
			sprintf(information, Description, (pItem->Level >> 3) & 0xF, RankedMuun + 1);
		}
	}
	break;
	case 105:
		//--muun option condition
		break;
	case 106:
		//--muun option condition date
		break;
	case 107:
		//--muun option value time date
		break;
	case 201:
	{
		WORD Level = CharacterAttribute->Level;

		if (Level < pItem->RequireLevel)
		{
			sprintf(information, Description, pItem->RequireLevel - Level);
			*color = TEXT_COLOR_RED;
			TextListColor[NextLine - 1] = *color;
		}
		else
		{
			sprintf(information, "");
		}
	}
	break;
	case 202:
	{
		int iDecNeedStrength = 0;
		int iDecNeedDex = 0;
		SetNeedStrDex(&iDecNeedStrength, &iDecNeedDex, iItemLevel, pItem);

		int iDecStrength = CharacterAttribute->AddStrength + CharacterAttribute->Strength;

		if (iDecStrength >= (pItem->RequireStrength - iDecNeedStrength))
		{
			sprintf(information, "");
			if (iDecNeedStrength) *color = TEXT_COLOR_YELLOW;
		}
		else
		{
			sprintf(information, Description, (pItem->RequireStrength - iDecStrength - iDecNeedStrength));
			*color = TEXT_COLOR_RED;
			TextListColor[NextLine - 1] = *color;
		}
	}
	break;
	case 203:
	{
		int iDecNeedStrength = 0;
		int iDecNeedDex = 0;
		SetNeedStrDex(&iDecNeedStrength, &iDecNeedDex, iItemLevel, pItem);

		int iDecDex = CharacterAttribute->AddDexterity + CharacterAttribute->Dexterity;

		if (iDecDex >= (pItem->RequireDexterity - iDecNeedDex))
		{
			sprintf(information, "");
			if (iDecNeedDex) *color = TEXT_COLOR_YELLOW;
		}
		else
		{
			sprintf(information, Description, (pItem->RequireDexterity - iDecDex - iDecNeedDex));
			*color = TEXT_COLOR_RED;
			TextListColor[NextLine - 1] = *color;
		}
	}
	break;
	case 204:
	{
		int bRequireStat = CharacterAttribute->AddVitality + CharacterAttribute->Vitality;

		if (bRequireStat >= pItem->RequireVitality)
		{
			sprintf(information, "");
		}
		else
		{
			sprintf(information, Description, (pItem->RequireVitality - bRequireStat));
			*color = TEXT_COLOR_RED;
			TextListColor[NextLine - 1] = *color;
		}
	}
	break;
	case 205:
	{
		int bRequireStat = CharacterAttribute->AddEnergy + CharacterAttribute->Energy;

		if (bRequireStat >= pItem->RequireEnergy)
		{
			sprintf(information, "");
		}
		else
		{
			sprintf(information, Description, (pItem->RequireEnergy - bRequireStat));
			*color = TEXT_COLOR_RED;
			TextListColor[NextLine - 1] = *color;
		}
	}
	break;
	case 206:
	{
		int bRequireStat = CharacterAttribute->AddCharisma + CharacterAttribute->Charisma;

		if (bRequireStat >= pItem->RequireCharisma)
		{
			sprintf(information, "");
		}
		else
		{
			sprintf(information, Description, (pItem->RequireCharisma - bRequireStat));
			*color = TEXT_COLOR_RED;
			TextListColor[NextLine - 1] = *color;
		}
	}
	break;
	case 207:
		sprintf(information, Description, iItemLevel + 32);
		break;
	case 208:
		sprintf(information, Description, 2 * iItemLevel + 39);
		break;
	case 209:
		sprintf(information, Description, 2 * iItemLevel + 20);
		break;
	case 210:
		sprintf(information, Description, 2 * iItemLevel + 12);
		break;
	case 211:
		sprintf(information, Description, 2 * iItemLevel + 25);
		break;
	case 212:
		sprintf(information, Description, 2 * iItemLevel + 24);
		break;
	case 213:
		sprintf(information, Description, 2 * iItemLevel + 10);
		break;
	case 214:
		sprintf(information, Description, iItemLevel + 10);
		break;
	case 215:
	{
#ifdef LEM_ADD_SEASON5_PART5_MINIUPDATE_JEWELMIX
		char tCount = COMGEM::CalcCompiledCount(pItem);
		if (tCount > 0)
		{
			int	nJewelIndex = COMGEM::Check_Jewel_Com(pItem->Type);
			if (nJewelIndex != COMGEM::NOGEM)
			{
				sprintf(information, Description, tCount, COMGEM::GetComGemTypeName(pItem->Type));
			}
		}
#endif
	}
	break;
	case 216:
	{
		int iSocketSeedID = GetSocketSeedID(pItem->Type, iItemLevel);

		char szOptionName[64] = { '\0', };

		g_SocketItemMgr.GetSocketOptionNameText(szOptionName, 0, iSocketSeedID);
		sprintf(information, Description, szOptionName);
	}
	break;
	case 217:
	{
		int iSocketSeedID = GetSocketSeedID(pItem->Type, iItemLevel);
		char szOptionValueText[16] = { '\0', };

		g_SocketItemMgr.GetSocketOptionValueText(szOptionValueText, 0, iSocketSeedID, pItem->Type);
		sprintf(information, Description, szOptionValueText);
	}
	break;
	case 218:
		sprintf(information, Description, CharacterAttribute->Level);
		break;
	case 219:
	{
		const ITEM_ADD_OPTION& Item_data = g_pItemAddOptioninfo->GetItemAddOtioninfo(pItem->Type);
		sprintf(information, Description, Item_data.m_byValue1);
	}
	break;
	case 220:
	{
		const ITEM_ADD_OPTION& Item_data = g_pItemAddOptioninfo->GetItemAddOtioninfo(pItem->Type);
		sprintf(information, Description, Item_data.m_byValue2);
	}
	break;
	case 221:
	{
		std::string timetext;
		const ITEM_ADD_OPTION& Item_data = g_pItemAddOptioninfo->GetItemAddOtioninfo(pItem->Type);
		g_StringTime(Item_data.m_Time, timetext, true);
		sprintf(information, Description, timetext.c_str());
	}
	break;
	case 222:
		if (pItem->Durability == 254)
			sprintf(information, Description);
		else
			sprintf(information, "");
		break;
	case 223:
		sprintf(information, Description, 5 - pItem->Durability);
		break;
	case 224:
		if (g_PortalMgr.IsRevivePositionSaved() == 1)
		{
			int iMap = g_PortalMgr.GetReviveWorld();
			int x = g_PortalMgr.GetRevivePosX();
			int y = g_PortalMgr.GetRevivePosY();
			sprintf(information, Description, gMapManager->GetMapName(iMap), x, y);
		}
		else
		{
			sprintf(information, "");
		}
		break;
	case 225:
		sprintf(information, Description, CharacterAttribute->Level >> 1, CharacterAttribute->Level >> 1);
		break;
	case 226:
		sprintf(information, Description, CharacterAttribute->Level / 12, CharacterAttribute->Level / 25);
		break;
	case 227:
		sprintf(information, Description, pItem->SocketCount);
		break;
	case 228:
		sprintf(information, Description, iItemLevel + 21);
		break;
	case 229:
		sprintf(information, Description, iItemLevel + 33);
		break;
	case 230:
		sprintf(information, Description, iItemLevel + 35);
		break;
	case 231:
		sprintf(information, Description, 2 * iItemLevel + 13);
		break;
	case 232:
		sprintf(information, Description, 2 * iItemLevel + 30);
		break;
	case 233:
		sprintf(information, Description, 2 * iItemLevel + 29);
		break;
	case 234:
		sprintf(information, Description, 71);
		break;
	case 235:
		sprintf(information, Description, iItemLevel + 60);
		break;
	case 236:
		sprintf(information, Description, iItemLevel + 80);
		break;
	case 237:
		sprintf(information, Description, iItemLevel + 75);
		break;
	case 238:
		sprintf(information, Description, iItemLevel + 100);
		break;
	case 239:
		sprintf(information, Description, iItemLevel + 55);
		break;
	case 240:
		sprintf(information, Description, 2 * iItemLevel + 43);
		break;
	case 241:
		sprintf(information, Description, 2 * iItemLevel + 37);
		break;
	case 242:
	case 243:
	case 244:
	case 245:
	{
		int value = 100 + pow(iItemLevel, 1.2) * 11.8;

		sprintf(information, Description, value);
	}
	break;
	case 246:
	{
		int value = 140 + pow(iItemLevel, 1.5) * 15;

		if (iItemLevel > 9)
		{
			value += pow(pow((iItemLevel - 10), 1.3) * 5, 1.6);
		}
		sprintf(information, Description, value);
	}
	break;
	case 247:
		sprintf(information, Description, Base(pItem->DamageMin), Base(pItem->DamageMax));
		break;
	case 249:
		sprintf(information, Description, Base(pItem->DamageMin), Base(pItem->DamageMax));
		break;
	case 251:
	{
		switch (pItem->SocketSeedSetOption)
		{
		case 1:
			(*color) = 14;
			break;
		case 2:
			(*color) = 2;
			break;
		case 3:
			(*color) = 1;
			break;
		case 4:
			(*color) = 3;
			break;
		case 5:
			(*color) = 9;
			break;
		}
	}
	break;
	case 252:
	{
		switch (pItem->SocketSeedSetOption)
		{
		case 1:
			(*color) = 14;
			break;
		case 2:
			(*color) = 2;
			break;
		case 3:
			(*color) = 1;
			break;
		case 4:
			(*color) = 3;
			break;
		case 5:
			(*color) = 9;
			break;
		}
		//sprintf(information, Description, iItemLevel + 75);
	}
	break;
	default:
		sprintf(information, Description);
		break;
	}
}

void CNewUIItemTooltip::SetNeedStrDex(int* iDecNeedStrength, int* iDecNeedDex, int iItemLevel, ITEM* pItem)
{
	if (iItemLevel >= pItem->Jewel_Of_Harmony_OptionLevel)
	{
		StrengthenCapability SC;
		g_pUIJewelHarmonyinfo->GetStrengthenCapability(&SC, pItem, 0);

		if (SC.SI_isNB)
		{
			*iDecNeedStrength = SC.SI_NB.SI_force;
			*iDecNeedDex = SC.SI_NB.SI_activity;
		}
	}
	if (pItem->SocketCount > 0)
	{
		for (int i = 0; i < pItem->SocketCount; ++i)
		{
			if (pItem->SocketSeedID[i] == 38)
			{
				int iReqStrengthDown = g_SocketItemMgr.GetSocketOptionValue(pItem, i);
				*iDecNeedStrength += iReqStrengthDown;
			}
			else if (pItem->SocketSeedID[i] == 39)
			{
				int iReqDexterityDown = g_SocketItemMgr.GetSocketOptionValue(pItem, i);
				*iDecNeedDex += iReqDexterityDown;
			}
		}
	}
}

int CNewUIItemTooltip::GetSocketSeedID(__int16 Type, int iLevel)
{
	int bySocketSeedID = 0;
	int iCategoryIndex = (Type - (ITEM_WING + 59));

	if (!InBounds(iCategoryIndex, 1, 6))
	{
		iCategoryIndex = (Type - (ITEM_WING + 100)) % 6 + 1;
	}

	switch (iCategoryIndex)
	{
	case 1:
		bySocketSeedID = iLevel;
		break;
	case 2:
		bySocketSeedID = iLevel + 10;
		break;
	case 3:
		bySocketSeedID = iLevel + 16;
		break;
	case 4:
		bySocketSeedID = iLevel + 21;
		break;
	case 5:
		bySocketSeedID = iLevel + 29;
		break;
	case 6:
		bySocketSeedID = iLevel + 34;
		break;
	}
	return bySocketSeedID;
}

void CNewUIItemTooltip::ItemEquipClass(Script_Item* pItemAttr, int* NextLine, int* SkipNum)
{
	int iTextColor;

	if (pItemAttr)
	{
		BYTE byFirstClass = Hero->GetBaseClass();
		BYTE _byStepClass = Hero->GetStepClass();

		TextListColor[*NextLine + 2] = TEXT_COLOR_WHITE;
		TextListColor[*NextLine + 3] = TEXT_COLOR_WHITE;

		sprintf_s(TextList[*NextLine], "\n");
		++(*NextLine);
		++(*SkipNum);

		// The game server for this client supports only the original seven
		// classes through Rage Fighter.  Item data still has 15 slots because
		// it is shared with newer seasons; rendering those slots creates empty
		// "Can be equipped by" lines in the tooltip.
		const int kSupportedClassCount = Rage_Fighter + 1;
		int iCount = 0;

		for (int i = 0; i < kSupportedClassCount; ++i)
		{
			if (pItemAttr->RequireClass[i] == 1)
				++iCount;
		}

		if (iCount == kSupportedClassCount)
			return;

		for (int i = 0; i < kSupportedClassCount; ++i)
		{
			BYTE byRequireClass = pItemAttr->RequireClass[i];

			if (byRequireClass == 0)
				continue;

			if (i == byFirstClass && byRequireClass <= _byStepClass)
			{
				iTextColor = TEXT_COLOR_WHITE;
			}
			else
			{
				iTextColor = TEXT_COLOR_DARKRED;
			}

			int NameClass = 2305;

			switch (i)
			{
			case Dark_Wizard:
				if (byRequireClass == 1)
				{
					NameClass = 20;
				}
				else if (byRequireClass == 2)
				{
					NameClass = 25;
				}
				else if (byRequireClass == 3)
				{
					NameClass = 1669;
				}
				else if (byRequireClass == 4)
				{
					NameClass = 3186;
				}
				break;
			case Dark_Knight:
				if (byRequireClass == 1)
				{
					NameClass = 21;
				}
				else if (byRequireClass == 2)
				{
					NameClass = 26;
				}
				else if (byRequireClass == 3)
				{
					NameClass = 1668;
				}
				else if (byRequireClass == 4)
				{
					NameClass = 3187;
				}
				break;
			case Fairy_Elf:
				if (byRequireClass == 1)
				{
					NameClass = 22;
				}
				else if (byRequireClass == 2)
				{
					NameClass = 27;
				}
				else if (byRequireClass == 3)
				{
					NameClass = 1670;
				}
				else if (byRequireClass == 4)
				{
					NameClass = 3188;
				}
				break;
			case Magic_Gladiator:
				if (byRequireClass == 1)
				{
					NameClass = 23;
				}
				else if (byRequireClass == 2)
				{
					NameClass = 2305;
				}
				else if (byRequireClass == 3)
				{
					NameClass = 1671;
				}
				else if (byRequireClass == 4)
				{
					NameClass = 3189;
				}
				break;
			case Dark_Lord:
				if (byRequireClass == 1)
				{
					NameClass = 24;
				}
				else if (byRequireClass == 2)
				{
					NameClass = 2305;
				}
				else if (byRequireClass == 3)
				{
					NameClass = 1672;
				}
				else if (byRequireClass == 4)
				{
					NameClass = 3190;
				}
				break;
			case Summoner:
				if (byRequireClass == 1)
				{
					NameClass = 1687;
				}
				else if (byRequireClass == 2)
				{
					NameClass = 1688;
				}
				else if (byRequireClass == 3)
				{
					NameClass = 1689;
				}
				else if (byRequireClass == 4)
				{
					NameClass = 3191;
				}
				break;
			case Rage_Fighter:
				if (byRequireClass == 1)
				{
					NameClass = 3150;
				}
				else if (byRequireClass == 2)
				{
					NameClass = 2305;
				}
				else if (byRequireClass == 3)
				{
					NameClass = 3151;
				}
				else if (byRequireClass == 4)
				{
					NameClass = 3192;
				}
				break;
			case Grow_Lancer:
				if (byRequireClass == 1)
				{
					NameClass = 3175;
				}
				else if (byRequireClass == 2)
				{
					NameClass = 2305;
				}
				else if (byRequireClass == 3)
				{
					NameClass = 3176;
				}
				else if (byRequireClass == 4)
				{
					NameClass = 3177;
				}
				break;
			case Runer_Wizzard:
				if (byRequireClass == 1)
				{
					NameClass = 3179;
				}
				else if (byRequireClass == 2)
				{
					NameClass = 3181;
				}
				else if (byRequireClass == 3)
				{
					NameClass = 3182;
				}
				else if (byRequireClass == 4)
				{
					NameClass = 3183;
				}
				break;
			case Slayer:
				if (byRequireClass == 1)
				{
					NameClass = 3193;
				}
				else if (byRequireClass == 2)
				{
					NameClass = 3194;
				}
				else if (byRequireClass == 3)
				{
					NameClass = 3195;
				}
				else if (byRequireClass == 4)
				{
					NameClass = 3196;
				}
				break;
			case Gun_Crusher:
				if (byRequireClass == 1)
				{
					NameClass = 3200;
				}
				else if (byRequireClass == 2)
				{
					NameClass = 3201;
				}
				else if (byRequireClass == 3)
				{
					NameClass = 3202;
				}
				else if (byRequireClass == 4)
				{
					NameClass = 3203;
				}
				break;
			case Mage_Kundun:
				if (byRequireClass == 1)
				{
					NameClass = 3218;
				}
				else if (byRequireClass == 2)
				{
					NameClass = 3219;
				}
				else if (byRequireClass == 3)
				{
					NameClass = 3220;
				}
				else if (byRequireClass == 4)
				{
					NameClass = 3221;
				}
				break;
			case Mage_Lemuria:
				if (byRequireClass == 1)
				{
					NameClass = 3213;
				}
				else if (byRequireClass == 2)
				{
					NameClass = 3214;
				}
				else if (byRequireClass == 3)
				{
					NameClass = 3215;
				}
				else if (byRequireClass == 4)
				{
					NameClass = 3216;
				}
				break;
			case Illusion_Knight:
				if (byRequireClass == 1)
				{
					NameClass = 3209;
				}
				else if (byRequireClass == 2)
				{
					NameClass = 3210;
				}
				else if (byRequireClass == 3)
				{
					NameClass = 3211;
				}
				else if (byRequireClass == 4)
				{
					NameClass = 3212;
				}
				break;
			case Alquimista1:
			{
				const char* nomeClasse = "Alquimista";
				if (byRequireClass == 2)
					nomeClasse = "Alquimista Evoluido";
				else if (byRequireClass == 3)
					nomeClasse = "Mestre Alquimista";
				else if (byRequireClass == 4)
					nomeClasse = "Grande Alquimista";

				sprintf_s(TextList[(*NextLine)], GlobalText[61], nomeClasse);
				TextListColor[(*NextLine)] = iTextColor;
				TextBold[(*NextLine)++] = 0;
				continue;
			}
			}

			sprintf_s(TextList[(*NextLine)], GlobalText[61], GlobalText[NameClass]);
			TextListColor[(*NextLine)] = iTextColor;
			TextBold[(*NextLine)++] = 0;
		}
	}
}

BOOL CNewUIItemTooltip::IsAbsoluteWeapon(__int16 Type)
{
	int WeaponKind3 = GMItemMng->GetKind3(Type);
	
	if (WeaponKind3 == ItemKind3::ARCHANGEL_WEAPON || WeaponKind3 == ItemKind3::BLESSED_ARCHANGEL_WEAPON)
		return true;
	
	return false;
}


BOOL CNewUIItemTooltip::IsEnableOptionWing(__int16 Type)
{
	int WeaponKind2 = GMItemMng->GetKind2(Type);

	if (WeaponKind2 == ItemKind2::WINGS_LVL_1
		|| WeaponKind2 == ItemKind2::WINGS_LVL_2
		|| WeaponKind2 == ItemKind2::WINGS_LVL_3
		|| WeaponKind2 == ItemKind2::WINGS_LVL_4
		|| WeaponKind2 == ItemKind2::LORD_CAPE
		|| WeaponKind2 == ItemKind2::CLOAK_LVL_2
		|| WeaponKind2 == ItemKind2::MONSTER_WINGS
		|| WeaponKind2 == ItemKind2::WINGS_CONQUERORS
		|| WeaponKind2 == ItemKind2::WINGS_ANGEL_AND_DEVIL
		|| WeaponKind2 == ItemKind2::WINGS_LVL_4
		|| WeaponKind2 == ItemKind2::WINGS_POWER
		
		)
	{
		return true;
	}

	return GMItemWings->Find(Type) != NULL;
}

_ITEM_TOOLTIP_DATA* CNewUIItemTooltip::FindTooltip(int _Type) const
{
	auto it = std::find_if(m_ItemTooltip.begin(), m_ItemTooltip.end(),
		[_Type](const _ITEM_TOOLTIP_DATA& data)
		{
			return _Type == GET_ITEM(data.Type, data.Index);
		});

	return (it != m_ItemTooltip.end()) ? const_cast<_ITEM_TOOLTIP_DATA*>(&(*it)) : NULL;
}

_ITEM_TOOLTIP_TEXT* CNewUIItemTooltip::FindTooltipText(int _Index) const
{
	auto it = std::find_if(m_ItemTooltipText.begin(), m_ItemTooltipText.end(),
		[_Index](const _ITEM_TOOLTIP_TEXT& data)
		{
			return _Index == data.index;
		});


	return (it != m_ItemTooltipText.end()) ? const_cast<_ITEM_TOOLTIP_TEXT*>(&(*it)) : NULL;
}

_ITEM_LEVEL_TOOTIP_DATA* CNewUIItemTooltip::FindLevelTooltip(int _Index) const
{
	auto it = std::find_if(m_ItemTooltipLevel.begin(), m_ItemTooltipLevel.end(),
		[_Index](const _ITEM_LEVEL_TOOTIP_DATA& data)
		{
			return _Index == data.index;
		});

	return (it != m_ItemTooltipLevel.end()) ? const_cast<_ITEM_LEVEL_TOOTIP_DATA*>(&(*it)) : NULL;
}

bool CNewUIItemTooltip::InBounds(int index, int min, int max)
{
	bool success;

	if (index >= min && index <= max)
		success = true;
	else
		success = false;

	return success;
}

int CNewUIItemTooltip::SplitText(char* text, int a2, int TextNum, int Color, int Bold, bool a6)
{
	if (NULL == text)
	{
		return TextNum;
	}
	char buf[0xA00] = { 0 };
	int nLine = 0;
	if (a2)
	{
		if (1 == a2)
		{
			nLine = ::DivideStringByPixel(buf, 10, 256, text, 200, true, '#');
		}
	}
	else
	{
		nLine = ::DivideStringByPixel(buf, 10, 256, text, 150, true, '#');
	}
	for (int i = 0; i < nLine; i++)
	{
		TextListColor[TextNum] = Color;
		TextBold[TextNum] = Bold;
		std::string str(&buf[256 * i]);
		if (true == a6)
		{
			for (int j = str.find("%", 0); j != -1; j = str.find("%", j + 2))
			{
				str.insert(j, "%");
			}
		}
		unicode::_sprintf(TextList[TextNum++], str.data());
	}

	return TextNum;
}

void CNewUIItemTooltip::CreateItemName(ITEM_t* pItemMap)
{
	ITEM* pItem = &pItemMap->Item;

	if (!pItem || pItem->Type == -1)
		return;

	int iLevelTooltip = -1;
	int ItemLevel = (pItem->Level >> 3) & 0xF;
	int ItemOption = (pItem->Option1);
	int ItemExtOption = (pItem->ExtOption);

	Script_Item* pItemAttr = GMItemMng->find(pItem->Type);

	if (!pItemAttr)
		return;

	if (pItem->Type == ITEM_POTION + 15)
	{
		sprintf(pItemMap->Name, "%s %d", pItemAttr->Name, pItem->Level);
		pItemMap->RenderColor = TEXT_COLOR_YELLOW;
		pItemMap->RenderTextBold = TRUE;
	}
	else
	{
		_ITEM_TOOLTIP_DATA* tooltipdata = FindTooltip(pItem->Type);

		if (tooltipdata)
		{
			_ITEM_LEVEL_TOOTIP_DATA* tooltiplevel = NULL;

			if (tooltipdata->ItemLevel >= 0)
			{
				int FindLevel;

				if (pItem->Type == ITEM_HELPER + 37)
				{
					switch (ItemOption)
					{
					case 1:
						iLevelTooltip = 1;
						break;
					case 2:
						iLevelTooltip = 2;
						break;
					case 4:
						iLevelTooltip = 3;
						break;
					case 8:
						iLevelTooltip = 4;
						break;
					case 16:
						iLevelTooltip = 5;
						break;
					default:
						iLevelTooltip = 0;
						break;
					}

					FindLevel = tooltipdata->ItemLevel + iLevelTooltip;
				}
				else
				{
					FindLevel = tooltipdata->ItemLevel + ItemLevel;
				}
				tooltiplevel = FindLevelTooltip(FindLevel);

				if (!tooltiplevel)
				{
					return;
				}
			}

			int color = TEXT_COLOR_GREEN_BLUE;

			if (IsAbsoluteWeapon(pItem->Type))
			{
				if (tooltipdata->ItemLevel == -1)
				{
					color = tooltipdata->ColorName;

				}
				else if (!tooltiplevel)
				{
					color = tooltiplevel->ColorName;
				}
			}
			else if (IsEnableOptionWing(pItem->Type) == 1)
			{
				if (ItemLevel < 7)
					color = tooltipdata->ColorName;
				else
					color = TEXT_COLOR_YELLOW;
			}
			else if ((pItem->ExtOption % 0x04) != EXT_A_SET_OPTION && (pItem->ExtOption % 0x04) != EXT_B_SET_OPTION)
			{
				if (g_SocketItemMgr.IsSocketItem(pItem) == 1)
				{
					color = TEXT_COLOR_VIOLET;
				}
				else if (pItem->SpecialNum <= 0 || (pItem->Option1 & 63) <= 0)
				{
					if (ItemLevel < 7)
					{
						if (tooltipdata->ItemLevel == -1)
						{
							if (pItem->SpecialNum <= 0)
								color = tooltipdata->ColorName;
							else
								color = TEXT_COLOR_BLUE;
						}
						else if (!tooltiplevel)
						{
							color = tooltiplevel->ColorName;
						}
					}
					else
					{
						color = TEXT_COLOR_YELLOW;
					}
				}
				else
				{
					color = TEXT_COLOR_GREEN;
				}
			}
			else
			{
				color = TEXT_COLOR_GREEN_BLUE;
			}

			pItemMap->RenderColor = color;

			if (pItem->Type != ITEM_HELPER + 4 && pItem->Type != ITEM_HELPER + 5)
			{
				char TextName[100];

				memset(TextName, 0, sizeof(TextName));

				if (!g_csItemOption.GetSetItemName(TextName, pItem->Type, pItem->ExtOption) || IsAbsoluteWeapon(pItem->Type))
				{
					strcat(TextName, tooltipdata->Name);
				}
				else
				{
					strcat(TextName, tooltipdata->Name);
				}

				if ((pItem->Option1 & 0x3F) <= 0 || IsAbsoluteWeapon(pItem->Type) || IsEnableOptionWing(pItem->Type))
				{
					if (tooltipdata->ItemLevel != -1 && tooltiplevel && tooltiplevel->Name[0] != '\0')
					{
						sprintf(pItemMap->Name, tooltiplevel->Name);
					}
					else
					{
						if (ItemLevel && tooltipdata->RenderLevel)
							sprintf(pItemMap->Name, "%s +%d", TextName, ItemLevel);
						else
							sprintf(pItemMap->Name, TextName);
					}
				}
				else if (iLevelTooltip == -1)
				{
					if (ItemLevel)
						sprintf(pItemMap->Name, "%s %s +%d", GlobalText[620], TextName, ItemLevel);
					else
						sprintf(pItemMap->Name, "%s %s", GlobalText[620], TextName);
					pItemMap->RenderColor = TEXT_COLOR_GREEN;
				}
				else
				{
					if (tooltiplevel && tooltiplevel->Name[0] != '\0')
					{
						sprintf(pItemMap->Name, tooltiplevel->Name);
					}
					color = TEXT_COLOR_BLUE;
				}

				if ((pItem->Level >> 7) & 1)
				{
					if (pItem->Type != ITEM_HELPER + 3)
					{
						strcat(pItemMap->Name, GlobalText[176]);
					}
					else
					{
						strcat(pItemMap->Name, " +");
						strcat(pItemMap->Name, GlobalText[179]);
					}
				}

				if ((pItem->Level & 3) || ((pItem->Option1 >> 6) & 1))
					strcat(pItemMap->Name, GlobalText[177]);

				if ((pItem->Level >> 2) & 1)
					strcat(pItemMap->Name, GlobalText[178]);


				if ((pItem->ExtOption % 0x04) != EXT_A_SET_OPTION && (pItem->ExtOption % 0x04) != EXT_B_SET_OPTION && g_SocketItemMgr.IsSocketItem(pItem) == false)
				{
					if (!IsAbsoluteWeapon(pItem->Type) && !IsEnableOptionWing(pItem->Type))
					{
						if ((ItemOption & 63) > 0)
						{
							color = TEXT_COLOR_GREEN;
						}
						else if(ItemLevel < 7)
						{
							if ((pItem->Level & 3) || ((ItemOption >> 6) & 1))
								color = TEXT_COLOR_BLUE;

							if ((pItem->Level >> 2) & 1)
								color = TEXT_COLOR_BLUE;
						}
					}
				}

				pItemMap->RenderColor = color;

				if (tooltipdata->ItemLevel != -1 && tooltiplevel)
					pItemMap->RenderTextBold = tooltiplevel->FontBold;
				else
					pItemMap->RenderTextBold = tooltipdata->FontBold;
			}
		}
		else
		{
			pItemMap->RenderColor = TEXT_COLOR_WHITE;
			sprintf(pItemMap->Name, "%s +%d", pItemAttr->Name, pItem->Level);
		}
	}
}

void CNewUIItemTooltip::CreateItemName(ITEM* pItem, char* ItemName)
{
	char TextName[100];

	int ItemType = pItem->Type;

	int ItemLevel = (pItem->Level >> 3) & 0xF;

	Script_Item* pItemAttr = GMItemMng->find(ItemType);

	if (!pItemAttr)
		return;

	_ITEM_TOOLTIP_DATA* tooltipdata = FindTooltip(ItemType);

	m_ItemLevel = -1;

	if (tooltipdata)
	{
		_ITEM_LEVEL_TOOTIP_DATA* tooltiplevel = NULL;

		if (tooltipdata->ItemLevel >= 0)
		{
			int FindLevel;

			if (ItemType == ITEM_HELPER + 37)
			{
				switch (pItem->Option1)
				{
				case 1:
					m_ItemLevel = 1;
					break;
				case 2:
					m_ItemLevel = 2;
					break;
				case 4:
					m_ItemLevel = 3;
					break;
				case 8:
					m_ItemLevel = 4;
					break;
				case 16:
					m_ItemLevel = 5;
					break;
				default:
					m_ItemLevel = 0;
					break;
				}

				FindLevel = tooltipdata->ItemLevel + m_ItemLevel;
			}
			else
			{
				FindLevel = tooltipdata->ItemLevel + ItemLevel;
			}

			tooltiplevel = FindLevelTooltip(FindLevel);

			if (!tooltiplevel)
			{
				return;
			}
		}
		memset(TextName, 0, sizeof(TextName));

		if (!g_csItemOption.GetSetItemName(TextName, ItemType, pItem->ExtOption) || IsAbsoluteWeapon(ItemType))
		{
			strcat(TextName, tooltipdata->Name);
		}
		else
		{
			strcat(TextName, tooltipdata->Name);
		}

		if ((pItem->Option1 & 0x3F) <= 0 || IsAbsoluteWeapon(ItemType) || IsEnableOptionWing(ItemType))
		{
			if (tooltipdata->ItemLevel != -1 && tooltiplevel)
			{
				sprintf(ItemName, tooltiplevel->Name);
			}
			else
			{
				if (ItemLevel && tooltipdata->RenderLevel)
					sprintf(ItemName, "%s +%d", TextName, ItemLevel);
				else
					sprintf(ItemName, TextName);
			}
		}
		else if (tooltiplevel)
		{
			sprintf(ItemName, tooltiplevel->Name);
		}
		else
		{
			if (ItemLevel)
				sprintf(ItemName, "%s %s +%d", GlobalText[620], TextName, ItemLevel);
			else
				sprintf(ItemName, "%s %s", GlobalText[620], TextName);
		}
	}
}
