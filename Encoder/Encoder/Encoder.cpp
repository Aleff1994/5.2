#include "stdafx.h"
#include "Util.h"
#include "CCRC32.h"
#include "ThemidaSDK.h"
#include "supportingfeature.h"
#include "ItemToolTip.h"
#include "../../Utilities/IndependentTierSettingsExport.h"

// The Win32 profile APIs used below do not recognize the first section of an
// UTF-8 BOM INI file. Notepad++ commonly saves MainInfo.ini in that format,
// making [MainInfo] silently fall back to its defaults (MaxOfInstance = 2).
// Remove only the optional BOM before reading; the INI contents and the two
// configuration files remain independent.
static bool NormalizeUtf8BomForProfile(const char* filePath)
{
	FILE* fp = nullptr;
	if (fopen_s(&fp, filePath, "rb") != 0 || fp == nullptr)
	{
		return false;
	}

	fseek(fp, 0, SEEK_END);
	const long fileSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	if (fileSize < 3)
	{
		fclose(fp);
		return true;
	}

	std::vector<BYTE> buffer(static_cast<size_t>(fileSize));
	const size_t bytesRead = fread(buffer.data(), 1, buffer.size(), fp);
	fclose(fp);

	if (bytesRead != buffer.size())
	{
		return false;
	}

	if (buffer[0] != 0xEF || buffer[1] != 0xBB || buffer[2] != 0xBF)
	{
		return true;
	}

	std::string tempPath = std::string(filePath) + ".tmp";
	if (fopen_s(&fp, tempPath.c_str(), "wb") != 0 || fp == nullptr)
	{
		return false;
	}

	const size_t contentSize = buffer.size() - 3;
	const size_t bytesWritten = fwrite(buffer.data() + 3, 1, contentSize, fp);
	fflush(fp);
	fclose(fp);

	if (bytesWritten != contentSize ||
		MoveFileExA(tempPath.c_str(), filePath,
			MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == FALSE)
	{
		DeleteFileA(tempPath.c_str());
		return false;
	}

	printf("[Info] UTF-8 BOM removido de %s para leitura correta do INI.\n", filePath);
	return true;
}

typedef struct
{
	char CustomerName[32];
	char IpAddress[32];
	WORD IpAddressPort;
	char ClientVersion[8];
	char ClientSerial[17];
	char WindowName[32];
	char ScreenShotPath[50];
}MAIN_INFO_ENV;

typedef struct 
{
	DWORD m_MaxInstance;

	DWORD AntiHackCRC32;
	char AntiHackName[32];
	DWORD ReShaderCRC32;
	char ReShaderName[32];
	char PluginName1[32];
	char PluginName2[32];
	char PluginName3[32];
	char PluginName4[32];
	char PluginName5[32];
	UINT HelperActiveAlert;
	UINT HelperActiveLevel;
	UINT MaxAttackSpeed[MAX_CLASS];
	UINT ReconnectTime;
	BYTE RemoveClass;
	BYTE ScreenType;
	BYTE LookAndFeel;
	BYTE ChatEmoticon;
	BYTE PickItemTextType;
	BYTE CastleSkillEnable;
	BYTE CharInfoBalloonType;
	BYTE InventoryExtension;
	BYTE MaxInventoryExtension;
	bool ServerNameDefault;
	bool WareHouseExtension;
	bool CashShopInGame;
	bool MuHelperOficial;
	bool shutdown_master_skill;
	bool WinMasterSkill;
	bool InvasionInfoTime;
	bool shutdown_tooltipold;
	bool m_RenderItemNameDrop;
	bool m_RenderCharacterName;
	bool shutdown_item_earring;
	bool Enable_Earring;
	bool Enable_Costumen;
	bool Enable_flag_nat;
	bool shutdown_item_core;
	bool shutdown_item_flag;
	bool shutdown_item_muun;
	bool shutdown_item_eagle;
	//------------------------------------
	BYTE SceneLogin;
	BYTE SceneCharacter;
	BYTE m_RenderMuLogo;
	BYTE RankUserSwitch;
	BYTE PartyHpBarType;
	bool RenderGuildLogo;
	bool EnableLoading;
	bool EnableWingRun;
	bool RankUserShowOverHead;
	bool RankUserOnlyOnSafeZone;
	bool RankUserShowReset;
	bool RankUserShowMasterReset;
	bool RankUserNeedAltKey;
	bool MenuSwitch;
	BYTE MenuTypeButton;
	bool MenuButtonVipShop;
	bool MenuButtonRankTop;
	bool MenuButtonCommand;
	bool MenuButtonOptions;
	bool MenuButtonEventTime;
	bool WindowsVipShop;
	bool WindowsRankTop;
	bool WindowsCommand;
	bool WindowsEventTime;
	bool WindowsJewelInventory;
	bool WindowsResetInfo;
	BYTE SwitchPersonalShopType;
	bool CustomOffStoreEnable;
	bool CustomStoreEnableJoB;
	bool CustomStoreEnableJoS;
	bool CustomStoreEnableJoC;
	bool CustomStoreEnableCoin1;
	bool CustomStoreEnableCoin2;
	bool CustomStoreEnableCoin3;
	bool remember_account;
	bool trade_system_pro;
	BYTE shutdown_popup;
	BYTE ajust_fps_render;
	BYTE max_item_level_on;
#ifdef SHUTDOWN_LEVEL_WING3
	BYTE shutdown_level_wing3;
#endif // SHUTDOWN_LEVEL_WING3

	//------------------------------------
	bool ShowAdvanceInfo;
	bool BossRankingEnable;
	bool ExibirSempre;
	bool ExibirSoParty;
	char TitleWCoinC[16];
	char TitleWCoinP[16];
	char TitleGoblin[16];
	char TitleReset[16];
	char TitleMReset[16];
	char TitleVipFree[16];
	char TitleVip1[16];
	char TitleVip2[16];
	char TitleVip3[16];
	char TitleVip4[16];
	BYTE InventoryPageMode;
	BYTE InventoryPageKeepOldExpansion;
	BYTE InventoryPageMax;
	BYTE InventoryPage2Currency;
	DWORD InventoryPage2Price;
	BYTE InventoryPage3Currency;
	DWORD InventoryPage3Price;
	BYTE InventoryPage4Currency;
	DWORD InventoryPage4Price;
	BYTE InventoryPage5Currency;
	DWORD InventoryPage5Price;
	BYTE WarehousePageMode;
	BYTE WarehousePageMax;
	BYTE HideBuffButton;
	BYTE VipWarehouseButton;
	// Campos novos ficam no final para preservar o layout binario existente.
	bool BuildProfileEnable;
	bool DailyRewardEnable;
	bool HuntLogEnable;
	BYTE EventEntryType;
	bool EnableBootsRunInSafeZone;
	bool LuckyWheelEnable;
} MAIN_FILE_INFO;

struct TEXT_FILE_INFO
{
	nInformation m_TRSTooltipData[MaxLineItemToolTip];
	nInformation m_TRSTooltipSetData[MaxLineItemToolTip];
	nText m_TRSTooltipText[MaxLineItemToolTip];
};
void EncoderText()
{
	//===Text Info
	TEXT_FILE_INFO info;
	memset(&info, 0, sizeof(info));

	GInfo.Load("ItemManager\\CustomItemToolTip.txt");
	GInfo.Load2("ItemManager\\CustomTextTooltip.txt");

	memcpy(info.m_TRSTooltipData, GInfo.n_TRSTooltipData, sizeof(info.m_TRSTooltipData));
	memcpy(info.m_TRSTooltipSetData, GInfo.n_TRSTooltipSetData, sizeof(info.m_TRSTooltipSetData));
	memcpy(info.m_TRSTooltipText, GInfo.n_TRSTooltipText, sizeof(info.m_TRSTooltipText));

	for (int n = 0; n < sizeof(TEXT_FILE_INFO); n++)
	{
		((BYTE*)&info)[n] ^= (BYTE)(0xCA ^ LOBYTE(n));
		((BYTE*)&info)[n] += (BYTE)(0x95 ^ HIBYTE(n));
	}


	HANDLE file = CreateFile("..\\Client_2\\Data\\Local\\CBTextInfo.bin", GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_ARCHIVE, 0);

	if (file == INVALID_HANDLE_VALUE)
	{
		return;
	}

	DWORD OutSize = 0;

	if (WriteFile(file, &info, sizeof(TEXT_FILE_INFO), &OutSize, 0) == 0)
	{
		CloseHandle(file);
		return;
	}

	CloseHandle(file);
}
void EncoderEnviron()
{
	MAIN_INFO_ENV info;
	wchar_t wFilename[MAX_PATH];
	wchar_t WindowName[MAX_PATH];

	memset(&info, 0, sizeof(info));

	// Lendo configura��es do MainConnect.ini
	GetPrivateProfileString("MainInfo", "CustomerName", "", info.CustomerName, sizeof(info.CustomerName), ".\\MainConnect.ini");
	GetPrivateProfileString("MainInfo", "IpAddress", "", info.IpAddress, sizeof(info.IpAddress), ".\\MainConnect.ini");
	info.IpAddressPort = GetPrivateProfileInt("MainInfo", "IpAddressPort", 44405, ".\\MainConnect.ini");
	GetPrivateProfileString("MainInfo", "ClientVersion", "", info.ClientVersion, sizeof(info.ClientVersion), ".\\MainConnect.ini");
	GetPrivateProfileString("MainInfo", "ClientSerial", "", info.ClientSerial, sizeof(info.ClientSerial), ".\\MainConnect.ini");
	GetPrivateProfileString("MainInfo", "ScreenShotPath", "", info.ScreenShotPath, sizeof(info.ScreenShotPath), ".\\MainConnect.ini");

	// Lendo strings em formato Wide (Unicode)
	GetPrivateProfileStringW(L"MainInfo", L"WindowName", L"", WindowName, sizeof(WindowName) / sizeof(wchar_t), L".\\MainConnect.ini");
	GetPrivateProfileStringW(L"MainInfo", L"PathExport", L"", wFilename, sizeof(wFilename) / sizeof(wchar_t), L".\\MainConnect.ini");

	if (info.CustomerName[0] != '\0')
	{
		std::wstring tempPath(wFilename);
		std::wstring tempName(WindowName);

		// Converte o nome da janela de WString para UTF8
		strncpy_s(info.WindowName, WStringToUTF8(tempName).c_str(), sizeof(info.WindowName));

		if (tempPath.empty())
		{
			// Caso 1: PathExport vazio - Gera na pasta do Encoder
			//CreateDirectory("Premium", NULL);
			PackFileEncrypt("Premium\\Premium.next", (BYTE*)&info, 1, sizeof(MAIN_INFO_ENV), 0x5A18u, false, true);

			printf("[Sucesso] Arquivo gerado localmente: Premium\\Premium.next\n");
		}
		else
		{
			// Caso 2: PathExport preenchido (ex: ..\Client_2)
			RemoveQuotes(tempPath);

			// Cria a pasta Premium dentro do destino
			std::wstring folderPath = tempPath + L"\\Premium";
			CreateDirectoryW(folderPath.c_str(), NULL);

			// Define o caminho completo do arquivo .next
			std::wstring filePath = folderPath + L"\\Premium.next";

			// Encripta e salva no destino final
			PackFileEncrypt(WStringToUTF8(filePath), (BYTE*)&info, 1, sizeof(MAIN_INFO_ENV), 0x5A18u, false, true);

			printf("[Sucesso] Arquivo exportado para: %S\n", filePath.c_str());
		}
	}
	else
	{
		printf("[Erro] CustomerName vazio no MainConnect.ini!\n");
	}
}


// Export tier settings separately: preserve the existing data.pak binary ABI.
static void ExportTierSettings(const std::string& output,bool server=false)
{
    if(!IndependentTierSettingsExport::Export(".\\MainInfo.ini",output.c_str(),server))
        printf("Tier export warning: invalid life scale or write failure: %s\n",output.c_str());
}

void SaveMainFileInfo(const MAIN_FILE_INFO& mainInfo, const std::string& filePath)
{
	std::vector<CUSTOM_MAPE_INFO> CustomMap;
	std::vector<CUSTOM_ITEM_INFO> CustomItem;
	std::vector<CUSTOM_JEWEL_INFO> CustomJewelInfo;
	std::vector<CUSTOM_IMAGE_INFO> CustomImagen;
	std::vector<CUSTOM_WING_INFO> CustomWings;
	std::vector<WINGS_PHYSICS_INFO> CustomCapes;
	std::vector<CUSTOM_PET_STACK> CustomPetItem;
	std::vector<CUSTOM_ITEM_STACK> CustomStacks;
	std::vector<CUSTOM_SMOKE_STACK> CustomSmokeItem;
	std::vector<CUSTOM_MONSTER_INFO> CustomMonster;
	std::vector<CUSTOM_NPC_NAME> CustomNpcName;
	std::vector<CUSTOM_EFFECT_STACK> CustomEffectDinamic;
	std::vector<CUSTOM_EFFECT_STACK> CustomEffectStatics;
	std::vector<CUSTOM_EFFECT_STACK> CustomPetEffectDinamic;
	std::vector<CUSTOM_EFFECT_STACK> CustomPetEffectStatics;
	std::vector<RENDER_MESH_EFFECT> CustomPetRenderMesh;//[MAX_CUSTOM_EFFECT];
	std::vector<RENDER_MESH_EFFECT> CustomMonsterRenderMesh;//[MAX_CUSTOM_EFFECT];
	std::vector<TEMPLATE_MEMBERSHIP> CustomBuyVip;
	std::vector<CUSTOM_COMMAND_INFO> CustomCommand;
	std::vector<CHARACTER_NAME_EFFECT> CustomCharNameWave;
	std::vector<RENDER_MESH_EFFECT_DEFAULT> CustomDefaultRenderMesh;

	//------------------------------------------------------------------------------------------------------------
	runtime_unique_document_file("Local\\CustomItem.txt", CustomItem, MAX_CUSTOM_ITEM);

	runtime_unique_document_file("Local\\CustomJewel.txt", CustomJewelInfo, MAX_CUSTOM_STACK);
	

	runtime_unique_document_file("Local\\CustomWorld.txt", CustomMap, MAX_CUSTOM_MAPE);

	runtime_unique_document_file("Local\\CustomImage.txt", CustomImagen, MAX_CUSTOM_IMAGE);

	runtime_unique_document_file("Local\\CustomMonster.txt", CustomMonster, MAX_CUSTOM_MONSTER);

	runtime_unique_document_file("Local\\CustomWing.txt", CustomWings, MAX_CUSTOM_WINGS);

	runtime_unique_document_file("Local\\CustomStack.txt", CustomStacks, MAX_CUSTOM_STACK);

	runtime_unique_document_file("Local\\CustomPet.txt", CustomPetItem, MAX_CUSTOM_PET);

	runtime_unique_document_file("Local\\CustomEffectDynamic.txt", CustomEffectDinamic, MAX_CUSTOM_EFFECT);

	runtime_unique_document_file("Local\\CustomEffectStatic.txt", CustomEffectStatics, MAX_CUSTOM_EFFECT);

	runtime_unique_document_file("Local\\CustomPetEffectDynamic.txt", CustomPetEffectDinamic, MAX_CUSTOM_EFFECT);

	runtime_unique_document_file("Local\\CustomPetEffectStatic.txt", CustomPetEffectStatics, MAX_CUSTOM_EFFECT);

	runtime_unique_document_file("Local\\CustomItemRenderMesh.txt", CustomPetRenderMesh, MAX_CUSTOM_EFFECT, RENDER_MESH_OBJ_ITEM);

	runtime_kapocha_document_file("Local\\CEffectRenderMesh.txt", CustomPetRenderMesh, MAX_CUSTOM_EFFECT);

	runtime_unique_document_file("Local\\CustomMonsterRenderMesh.txt", CustomMonsterRenderMesh, MAX_CUSTOM_EFFECT, RENDER_MESH_OBJ_MONSTER);

	runtime_unique_document_file("Local\\CustomSmokeItem.txt", CustomSmokeItem, MAX_CUSTOM_SMOKE);

	runtime_unique_document_file("Local\\CustomNpcName.txt", CustomNpcName, MAX_CUSTOM_NPC_NAME);

	runtime_unique_document_file("Local\\CustomBuyVip.txt", CustomBuyVip, MAX_MEMBERSHIP_VIP);

	runtime_unique_document_file("Local\\CustomCommand.txt", CustomCommand, MAX_CUSTOM_COMMAND);

	runtime_unique_document_file("Local\\CustomNameWave.txt", CustomCharNameWave, MAX_CHARACTER_NAME);

	runtime_unique_document_file("Local\\CustomRenderEffect.txt", CustomDefaultRenderMesh, MAX_CUSTOM_EFFECT);

	runtime_unique_document_file("Local\\CustomCape.txt", CustomCapes, MAX_CUSTOM_WINGS);
	//------------------------------------------------------------------------------------------------------------

	FILE* fp = fopen(filePath.c_str(), "wb");

	if (fp == nullptr)
	{
		throw std::runtime_error("No se pudo abrir el archivo para lectura.");
	}

	BuxConvert((BYTE*)&mainInfo, sizeof(MAIN_FILE_INFO));

	DWORD dwCheckSum = GenerateCheckSum2((BYTE*)&mainInfo, sizeof(MAIN_FILE_INFO), 0x5A18);

	fwrite(&dwCheckSum, sizeof(DWORD), 1u, fp);
	fwrite(&mainInfo, sizeof(MAIN_FILE_INFO), 1, fp);  // Escribe el tama�o del vector

	// Serializar cada vector
	writeVector(fp, CustomMap);
	writeVector(fp, CustomItem);
	writeVector(fp, CustomJewelInfo);
	writeVector(fp, CustomImagen);
	writeVector(fp, CustomWings);
	writeVector(fp, CustomCapes);
	writeVector(fp, CustomPetItem);
	writeVector(fp, CustomStacks);
	writeVector(fp, CustomSmokeItem);
	writeVector(fp, CustomMonster);
	writeVector(fp, CustomNpcName);
	writeVector(fp, CustomEffectDinamic);
	writeVector(fp, CustomEffectStatics);
	writeVector(fp, CustomPetEffectDinamic);
	writeVector(fp, CustomPetEffectStatics);

	writeVector(fp, CustomPetRenderMesh);
	writeVector(fp, CustomMonsterRenderMesh);

	writeVector(fp, CustomBuyVip);
	writeVector(fp, CustomCommand);
	writeVector(fp, CustomCharNameWave);
	writeVector(fp, CustomDefaultRenderMesh);

	fclose(fp);
}

void EncoderInformation()
{
	MAIN_FILE_INFO info;

	wchar_t wFilename[MAX_PATH];

	memset(&info, 0, sizeof(info));

	info.m_MaxInstance = GetPrivateProfileInt("MainInfo", "MaxOfInstance", 2, ".\\MainInfo.ini");

	GetPrivateProfileString("MainInfo", "PluginAntiHack", "", info.AntiHackName, sizeof(info.AntiHackName), ".\\MainInfo.ini");

	GetPrivateProfileString("MainInfo", "PluginReShader", "", info.ReShaderName, sizeof(info.ReShaderName), ".\\MainInfo.ini");

	GetPrivateProfileStringW(L"MainInfo", L"PathExport", L"", wFilename, sizeof(wFilename) / sizeof(wchar_t), L".\\MainInfo.ini");

	info.HelperActiveAlert = GetPrivateProfileInt("HelperInfo", "HelperActiveAlert", 0, ".\\MainInfo.ini");

	info.HelperActiveLevel = GetPrivateProfileInt("HelperInfo", "HelperActiveLevel", 80, ".\\MainInfo.ini");

	info.ReconnectTime = GetPrivateProfileInt("ReconnectInfo", "ReconnectTime", 0, ".\\MainInfo.ini");

	//[CharacterInfo]
	info.MaxAttackSpeed[0] = GetPrivateProfileInt("CharacterInfo", "DWMaxAttackSpeed", 65000, ".\\MainInfo.ini");

	info.MaxAttackSpeed[1] = GetPrivateProfileInt("CharacterInfo", "DKMaxAttackSpeed", 65000, ".\\MainInfo.ini");

	info.MaxAttackSpeed[2] = GetPrivateProfileInt("CharacterInfo", "FEMaxAttackSpeed", 65000, ".\\MainInfo.ini");

	info.MaxAttackSpeed[3] = GetPrivateProfileInt("CharacterInfo", "MGMaxAttackSpeed", 65000, ".\\MainInfo.ini");

	info.MaxAttackSpeed[4] = GetPrivateProfileInt("CharacterInfo", "DLMaxAttackSpeed", 65000, ".\\MainInfo.ini");

	info.MaxAttackSpeed[5] = GetPrivateProfileInt("CharacterInfo", "SUMaxAttackSpeed", 65000, ".\\MainInfo.ini");

	info.MaxAttackSpeed[6] = GetPrivateProfileInt("CharacterInfo", "RFMaxAttackSpeed", 65000, ".\\MainInfo.ini");

	//[Interface]
	info.ServerNameDefault = GetPrivateProfileInt("Interface", "ServerDefaultName", 1, ".\\MainInfo.ini");

	info.ScreenType = GetPrivateProfileInt("Interface", "ScreenType", 0, ".\\MainInfo.ini");

	info.LookAndFeel = GetPrivateProfileInt("Interface", "LookAndFeel", 0, ".\\MainInfo.ini");

	info.ChatEmoticon = GetPrivateProfileInt("Interface", "ChatEmoticon", 0, ".\\MainInfo.ini");

	info.PickItemTextType = GetPrivateProfileInt("Interface", "PickItemTextType", 0, ".\\MainInfo.ini");

	info.CharInfoBalloonType = GetPrivateProfileInt("Interface", "CharInfo", 0, ".\\MainInfo.ini");

	info.InventoryExtension = GetPrivateProfileInt("Interface", "InventoryExtension", 0, ".\\MainInfo.ini");

	info.MaxInventoryExtension = GetPrivateProfileInt("Interface", "MaxInventoryExtension", 2, ".\\MainInfo.ini");

	info.WareHouseExtension = GetPrivateProfileInt("Interface", "WareHouseExtension", 0, ".\\MainInfo.ini");

	info.CashShopInGame = GetPrivateProfileInt("Interface", "CashShopInGame", 0, ".\\MainInfo.ini");

	info.MuHelperOficial = GetPrivateProfileInt("Interface", "OficialMuHelper", 0, ".\\MainInfo.ini");

	info.WinMasterSkill = GetPrivateProfileInt("Interface", "MasterSkill", 0, ".\\MainInfo.ini");

	info.InvasionInfoTime = GetPrivateProfileInt("Interface", "InvasionInfoTime", 0, ".\\MainInfo.ini");

	info.shutdown_tooltipold = GetPrivateProfileInt("Interface", "RenderItemToolip", 0, ".\\MainInfo.ini");

	info.m_RenderItemNameDrop = GetPrivateProfileInt("Interface", "RenderItemNameDrop", 0, ".\\MainInfo.ini");

	info.m_RenderCharacterName = GetPrivateProfileInt("Interface", "RenderCharacterName", 0, ".\\MainInfo.ini");

	info.Enable_Earring = GetPrivateProfileInt("Interface", "RenderEquipmentEarring", 0, ".\\MainInfo.ini");
	// The inventory renderer uses shutdown_item_earring as its actual feature
	// gate. Keep both fields synchronized for compatibility with older data.pak.
	info.shutdown_item_earring = info.Enable_Earring;

	info.Enable_Costumen = GetPrivateProfileInt("Interface", "RenderEquipmentCostume", 0, ".\\MainInfo.ini");

	info.Enable_flag_nat = GetPrivateProfileInt("Interface", "RenderEquipmentFlagNat", 0, ".\\MainInfo.ini");
	// The flag slot and its item handling are guarded by shutdown_item_flag.
	info.shutdown_item_flag = info.Enable_flag_nat;

	info.shutdown_item_muun = GetPrivateProfileInt("Interface", "RenderEquipmentPetMuun", 0, ".\\MainInfo.ini");

	info.shutdown_item_eagle = GetPrivateProfileInt("Interface", "RenderEquipmentPetEagle", 0, ".\\MainInfo.ini");
	//[Custom]
	info.RemoveClass = GetPrivateProfileInt("Custom", "RemoveClass", 0, ".\\MainInfo.ini");

	info.RenderGuildLogo = GetPrivateProfileInt("Custom", "RenderGuildLogo", 0, ".\\MainInfo.ini");

	info.EnableLoading = GetPrivateProfileInt("Custom", "EnableLoading", 0, ".\\MainInfo.ini");

	info.EnableWingRun = GetPrivateProfileInt("Custom", "EnableWingRun", 0, ".\\MainInfo.ini");

	info.m_RenderMuLogo = GetPrivateProfileInt("Custom", "PrintLogo", 0, ".\\MainInfo.ini");

	info.CastleSkillEnable = GetPrivateProfileInt("Custom", "EnableCsSkillsAllMaps", 0, ".\\MainInfo.ini");

	info.SceneLogin = GetPrivateProfileInt("Custom", "SceneLogin", 0, ".\\MainInfo.ini");

	info.SceneCharacter = GetPrivateProfileInt("Custom", "SceneCharacter", 0, ".\\MainInfo.ini");

	info.ajust_fps_render = GetPrivateProfileInt("Custom", "FpsRenderMax", 30, ".\\MainInfo.ini");

	info.shutdown_popup = GetPrivateProfileInt("Custom", "FpsWindowsOption", 1, ".\\MainInfo.ini");

	//------------------------------------------------------------------------------------------------------------

	info.RankUserSwitch = GetPrivateProfileInt("Custom", "RankUserSwitch", 0, ".\\MainInfo.ini");

	info.RankUserShowOverHead = GetPrivateProfileInt("Custom", "RankUserShowOverHead", 0, ".\\MainInfo.ini");

	info.RankUserOnlyOnSafeZone = GetPrivateProfileInt("Custom", "RankUserOnlyOnSafeZone", 1, ".\\MainInfo.ini");

	info.RankUserShowReset = GetPrivateProfileInt("Custom", "RankUserShowReset", 1, ".\\MainInfo.ini");

	info.RankUserShowMasterReset = GetPrivateProfileInt("Custom", "RankUserShowMasterReset", 1, ".\\MainInfo.ini");

	info.RankUserNeedAltKey = GetPrivateProfileInt("Custom", "RankUserNeedAltKey", 0, ".\\MainInfo.ini");

	info.PartyHpBarType = GetPrivateProfileInt("Custom", "PartyHpBarType", 1, ".\\MainInfo.ini");

	//------------------------------------------------------------------------------------------------------------

	info.MenuSwitch = GetPrivateProfileInt("Custom", "CustomMenuSwitch", 0, ".\\MainInfo.ini");

	info.MenuTypeButton = GetPrivateProfileInt("Custom", "CustomMenuType", 0, ".\\MainInfo.ini");

	info.MenuButtonVipShop = GetPrivateProfileInt("Custom", "EnableVipShopButton", 0, ".\\MainInfo.ini");

	info.MenuButtonRankTop = GetPrivateProfileInt("Custom", "EnableRankingButton", 0, ".\\MainInfo.ini");

	info.MenuButtonCommand = GetPrivateProfileInt("Custom", "EnableCommandButton", 0, ".\\MainInfo.ini");

	info.MenuButtonOptions = GetPrivateProfileInt("Custom", "EnableOptionButton", 0, ".\\MainInfo.ini");

	info.MenuButtonEventTime = GetPrivateProfileInt("Custom", "EnableEventTimeButton", 0, ".\\MainInfo.ini");

	info.WindowsVipShop = GetPrivateProfileInt("Custom", "EnableWindowsVipShop", 0, ".\\MainInfo.ini");

	info.WindowsRankTop = GetPrivateProfileInt("Custom", "EnableWindowsRankTop", 0, ".\\MainInfo.ini");

	info.WindowsCommand = GetPrivateProfileInt("Custom", "EnableWindowsCommand", 0, ".\\MainInfo.ini");

	info.WindowsEventTime = GetPrivateProfileInt("Custom", "EnableWindowsEventTime", 0, ".\\MainInfo.ini");

	info.SwitchPersonalShopType = GetPrivateProfileInt("Custom", "SwitchPersonalShopType", 0, ".\\MainInfo.ini");

	info.CustomOffStoreEnable = GetPrivateProfileInt("Custom", "EnableOffStoreButtom", 0, ".\\MainInfo.ini");

	info.CustomStoreEnableJoB = GetPrivateProfileInt("Custom", "EnableStoreBlessButtom", 0, ".\\MainInfo.ini");

	info.CustomStoreEnableJoS = GetPrivateProfileInt("Custom", "EnableStoreSoulButtom", 0, ".\\MainInfo.ini");

	info.CustomStoreEnableJoC = GetPrivateProfileInt("Custom", "EnableStoreChaosButtom", 0, ".\\MainInfo.ini");

	info.CustomStoreEnableCoin1 = GetPrivateProfileInt("Custom", "EnableStoreCoin1Buttom", 0, ".\\MainInfo.ini");

	info.CustomStoreEnableCoin2 = GetPrivateProfileInt("Custom", "EnableStoreCoin2Buttom", 0, ".\\MainInfo.ini");

	info.CustomStoreEnableCoin3 = GetPrivateProfileInt("Custom", "EnableStoreCoin3Buttom", 0, ".\\MainInfo.ini");

	info.WindowsJewelInventory = GetPrivateProfileInt("Custom", "EnableJewelOfInventory", 0, ".\\MainInfo.ini");

	info.WindowsResetInfo = GetPrivateProfileInt("Custom", "EnableWindowsResetInfo", 0, ".\\MainInfo.ini");

	info.remember_account = GetPrivateProfileInt("Custom", "EnableRememberAccount", 0, ".\\MainInfo.ini");

	info.trade_system_pro = GetPrivateProfileInt("Custom", "EnableTradeCoin", 0, ".\\MainInfo.ini");

	info.max_item_level_on = GetPrivateProfileInt("Custom", "MaxItemLevelOn", 15, ".\\MainInfo.ini");

#ifdef SHUTDOWN_LEVEL_WING3
	info.shutdown_level_wing3 = GetPrivateProfileInt("Custom", "ShutdownLevelWing3", 0, ".\\MainInfo.ini");
#endif // SHUTDOWN_LEVEL_WING3
	//------------------------------------------------------------------------------------------------------------

	info.ShowAdvanceInfo = GetPrivateProfileInt("Custom", "ShowAdvanceInfo", 0, ".\\MainInfo.ini");
	info.BossRankingEnable = GetPrivateProfileInt("Custom", "BossRankingEnable", 0, ".\\MainInfo.ini");
	info.BuildProfileEnable = GetPrivateProfileInt("Interface", "BuildProfile", 1, ".\\MainInfo.ini") != 0;
	info.DailyRewardEnable = GetPrivateProfileInt("Interface", "DailyReward", 0, ".\\MainInfo.ini") != 0;
	info.HuntLogEnable = GetPrivateProfileInt("Custom", "HuntLogEnable", 1, ".\\MainInfo.ini") != 0;
	info.EventEntryType = (BYTE)GetPrivateProfileInt("EventSettings", "EventEntryType", 0, ".\\MainInfo.ini");
	info.EnableBootsRunInSafeZone = GetPrivateProfileInt("Custom", "EnableBootsRunInSafeZone", 1, ".\\MainInfo.ini") != 0;
	info.LuckyWheelEnable = GetPrivateProfileInt("Interface", "LuckyWheel", 0, ".\\MainInfo.ini") != 0;
    ExportTierSettings(".\\TierSystem.server.ini",true);
	info.ExibirSempre = GetPrivateProfileInt("Custom", "ExibirSempre", 0, ".\\MainInfo.ini") != 0;
	info.ExibirSoParty = GetPrivateProfileInt("Custom", "ExibirSoParty", 0, ".\\MainInfo.ini") != 0;

	GetPrivateProfileString("TitleInfo", "WCoinC", "Cash", info.TitleWCoinC, sizeof(info.TitleWCoinC), ".\\MainInfo.ini");
	GetPrivateProfileString("TitleInfo", "WCoinP", "Gold", info.TitleWCoinP, sizeof(info.TitleWCoinP), ".\\MainInfo.ini");
	GetPrivateProfileString("TitleInfo", "Goblin", "GP", info.TitleGoblin, sizeof(info.TitleGoblin), ".\\MainInfo.ini");
	GetPrivateProfileString("TitleInfo", "Reset", "R", info.TitleReset, sizeof(info.TitleReset), ".\\MainInfo.ini");
	GetPrivateProfileString("TitleInfo", "MReset", "MR", info.TitleMReset, sizeof(info.TitleMReset), ".\\MainInfo.ini");
	GetPrivateProfileString("TitleInfo", "VipFree", "Free", info.TitleVipFree, sizeof(info.TitleVipFree), ".\\MainInfo.ini");
	GetPrivateProfileString("TitleInfo", "Vip1", "VIP Bronze", info.TitleVip1, sizeof(info.TitleVip1), ".\\MainInfo.ini");
	GetPrivateProfileString("TitleInfo", "Vip2", "VIP Prata", info.TitleVip2, sizeof(info.TitleVip2), ".\\MainInfo.ini");
	GetPrivateProfileString("TitleInfo", "Vip3", "VIP Ouro", info.TitleVip3, sizeof(info.TitleVip3), ".\\MainInfo.ini");
	GetPrivateProfileString("TitleInfo", "Vip4", "VIP DIAMANTE", info.TitleVip4, sizeof(info.TitleVip4), ".\\MainInfo.ini");

	info.InventoryPageMode = GetPrivateProfileInt("Interface", "InventoryPageMode", 0, ".\\MainInfo.ini");
	info.InventoryPageKeepOldExpansion = GetPrivateProfileInt("Interface", "InventoryPageKeepOldExpansion", 1, ".\\MainInfo.ini");
	info.InventoryPageMax = GetPrivateProfileInt("Interface", "InventoryPageMax", 1, ".\\MainInfo.ini");
	info.InventoryPage2Currency = GetPrivateProfileInt("Interface", "InventoryPage2Currency", 1, ".\\MainInfo.ini");
	info.InventoryPage2Price = GetPrivateProfileInt("Interface", "InventoryPage2Price", 0, ".\\MainInfo.ini");
	info.InventoryPage3Currency = GetPrivateProfileInt("Interface", "InventoryPage3Currency", 2, ".\\MainInfo.ini");
	info.InventoryPage3Price = GetPrivateProfileInt("Interface", "InventoryPage3Price", 0, ".\\MainInfo.ini");
	info.InventoryPage4Currency = GetPrivateProfileInt("Interface", "InventoryPage4Currency", 3, ".\\MainInfo.ini");
	info.InventoryPage4Price = GetPrivateProfileInt("Interface", "InventoryPage4Price", 0, ".\\MainInfo.ini");
	info.InventoryPage5Currency = GetPrivateProfileInt("Interface", "InventoryPage5Currency", 0, ".\\MainInfo.ini");
	info.InventoryPage5Price = GetPrivateProfileInt("Interface", "InventoryPage5Price", 0, ".\\MainInfo.ini");
	info.WarehousePageMode = GetPrivateProfileInt("Interface", "WarehousePageMode", 0, ".\\MainInfo.ini");
	info.WarehousePageMax = GetPrivateProfileInt("Interface", "WarehousePageMax", 1, ".\\MainInfo.ini");
	info.HideBuffButton = GetPrivateProfileInt("Interface", "HideBuffButton", 0, ".\\MainInfo.ini");
	info.VipWarehouseButton = GetPrivateProfileInt("Interface", "VipWarehouseButton", 0, ".\\MainInfo.ini");
	CCRC32 CRC32;

	if (CRC32.FileCRC(info.AntiHackName, &info.AntiHackCRC32, 1024) == 0)
	{
		info.AntiHackCRC32 = 0;
	}

	if (CRC32.FileCRC(info.ReShaderName, &info.ReShaderCRC32, 1024) == 0)
	{
		info.ReShaderCRC32 = 0;
	}

	std::wstring tempPath(wFilename);

	RemoveQuotes(tempPath); // Eliminar comillas del string temporal si existen


	if (tempPath.empty())
	{
		//CreateDirectory("Release", 0);
		CreateDirectory("Premium\\Data", 0);
		SaveMainFileInfo(info, "Premium\\Data\\data.pak");
        ExportTierSettings(".\\Premium\\Data\\TierSystem.ini");
	}
	else
	{
		tempPath += L"\\Data\\data.pak";
		SaveMainFileInfo(info, WStringToUTF8(tempPath));
        std::wstring tierPath=tempPath.substr(0,tempPath.size()-8)+L"TierSystem.ini";
        ExportTierSettings(WStringToUTF8(tierPath));
	}
}

int _tmain(int argc, _TCHAR* argv[]) // OK
{
	CLEAR_START;

	ENCODE_START;

	if (!NormalizeUtf8BomForProfile(".\\MainInfo.ini") ||
		!NormalizeUtf8BomForProfile(".\\MainConnect.ini"))
	{
		printf("[Erro] Nao foi possivel preparar os arquivos INI.\n");
		return 1;
	}

	EncoderEnviron();

	EncoderInformation();

	EncoderText();

	ENCODE_END

	CLEAR_END

	return 0;
}
