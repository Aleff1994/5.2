// NewUIOptionWindow.cpp: implementation of the CNewUIOptionWindow class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "NewUIOptionWindow.h"
#include "Winmain.h"
#include "NewUISystem.h"
#include "ZzzTexture.h"
#include "DSPlaySound.h"
#include "Input.h"
#include "ZzzOpenData.h"
#include "ZzzInterface.h"
#include "ZzzOpenglUtil.h"
#include "pugixml.hpp"
#include "./ExternalObject/leaf/regkey.h"
#include "TextClien.h"
#include "MultiLanguage.h"
#include "GlobalText.h"
#include "CGMItemMng.h"
#include "ZzzCharacter.h"
#include "ZzzInfomation.h"
#include "CSQuest.h"
#include "QuestMng.h"
#include "SocketSystem.h"
#include "CSItemOption.h"
#include "UIJewelHarmony.h"
#include "UIManager.h"
#include "_GlobalFunctions.h"
using namespace SEASON3B;

extern BOOL g_bVSyncEnabled;
extern int targetIndexFPS;
extern float RealRenderFPS;
extern void RefreshServerWeatherVisuals();
extern CMultiLanguage* pMultiLanguage;
extern bool FogEnable;

// Classic two-pane settings. The content mapping is shared by drawing and input.
static const float PRO_BASE_W=900.f, PRO_BASE_H=640.f, PRO_FRAME_H=PRO_BASE_H;
static const float PRO_NAV_X=30.f, PRO_NAV_Y=95.f, PRO_NAV_W=260.f, PRO_NAV_H=44.f, PRO_NAV_STEP=58.f;
static const float PRO_CLOSE_X=850.f, PRO_CLOSE_Y=8.f, PRO_CLOSE_W=32.f, PRO_CLOSE_H=32.f;
static const float PRO_FOOTER_Y=610.f, PRO_FOOTER_H=42.f;
static float ProContentX(float x) { return x+350.f; }
static float ProContentY(float y) { return y-60.f; }

static float ProUIScale()
{
    float sx = ((float)GetWindowsX - 18.0f) / PRO_BASE_W;
    float sy = ((float)GetWindowsY - 18.0f) / PRO_BASE_H;
    float scale = (sx < sy) ? sx : sy;
    // Keep the complete frame inside the current virtual viewport.
    if (scale > 0.78f) scale = 0.78f;
    if (scale < 0.20f) scale = 0.20f;
    return scale;
}

static int ProPanelWidth()
{
    return (int)(PRO_BASE_W * ProUIScale() + 0.5f);
}

static int ProPanelHeight()
{
    return (int)(PRO_FRAME_H * ProUIScale() + 0.5f);
}

// Anti-Lag usa exatamente as dimensoes e coordenadas estaveis do Look 0.
static const float LOOK4_PANEL_SCALE = 1.0f;
static const int FONT_FAMILY_COUNT = 32;
static const char* FONT_FAMILIES[FONT_FAMILY_COUNT] =
{
	"Arial",
	"Tahoma",
	"Verdana",
	"Segoe UI",
	"Trebuchet MS",
	"Georgia",
	"Calibri",
	"Cambria",
	"Times New Roman",
	"Courier New",
	"Comic Sans MS",
	"Impact",
	"Lucida Console",
	"Lucida Sans Unicode",
	"Palatino Linotype",
	"Century Gothic",
	"Franklin Gothic Medium",
	"Garamond",
	"Book Antiqua",
	"Microsoft Sans Serif",
	"Consolas",
	"Segoe UI Semibold",
	"Segoe UI Black",
	"Arial Black",
	"Bahnschrift",
	"Candara",
	"Corbel",
	"Constantia",
	"Rockwell",
	"Bookman Old Style",
	"Monotype Corsiva",
	"Yu Gothic UI"
};

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SEASON3B::CNewUIOptionWindow::CNewUIOptionWindow()
{
	m_pNewUIMng = NULL;
	m_Pos.x = 0;
	m_Pos.y = 0;

	m_bAutoAttack = true;
	m_bWhisperSound = false;
	m_bSlideHelp = true;
	m_iVolumeLevel = 0;
	m_iRenderLevel = GetPrivateProfileIntA("Renderer", "RenderLevel", 4, ".\\config.ini");
	if (m_iRenderLevel < 0 || m_iRenderLevel > 4) m_iRenderLevel = 4;

	m_RenderEffect = GetPrivateProfileIntA("AntiLag", "RenderEffect", 1, ".\\config.ini") != 0;
	m_RenderShadows = GetPrivateProfileIntA("AntiLag", "RenderShadows", 1, ".\\config.ini") != 0;
	m_RenderPets = GetPrivateProfileIntA("AntiLag", "RenderPets", 1, ".\\config.ini") != 0;
	m_RenderGlow = GetPrivateProfileIntA("AntiLag", "RenderGlow", 1, ".\\config.ini") != 0;
	m_RenderEquipment = GetPrivateProfileIntA("AntiLag", "RenderEquipment", 1, ".\\config.ini") != 0;
	m_RenderTerrain = GetPrivateProfileIntA("AntiLag", "RenderTerrain", 1, ".\\config.ini") != 0;
	m_RenderObjects = GetPrivateProfileIntA("AntiLag", "RenderObjects", 1, ".\\config.ini") != 0;
	m_RenderParticles = GetPrivateProfileIntA("AntiLag", "RenderParticles", 1, ".\\config.ini") != 0;
	m_iLaf0Profile = 1;
	m_iLaf0FpsLimit = GetPrivateProfileIntA("Renderer","FrameLimit",60,".\\config.ini");
	const int validFps[4]={25,30,60,240};
	bool fpsOk=false; for(int i=0;i<4;i++) if(m_iLaf0FpsLimit==validFps[i]) fpsOk=true;
	if(!fpsOk) m_iLaf0FpsLimit=60;
	m_bLaf0VSync = GetPrivateProfileIntA("Renderer", "VSync", 0, ".\\config.ini") != 0;
	m_iLaf0ViewDistance = GetPrivateProfileIntA("AntiLag", "ViewDistance", 1, ".\\config.ini");
	if (m_iLaf0ViewDistance < 0 || m_iLaf0ViewDistance > 2)
		m_iLaf0ViewDistance = 1;
	m_iLaf0TextureQuality = GetPrivateProfileIntA("Renderer", "TextureQuality", 1, ".\\config.ini");
	if (m_iLaf0TextureQuality < 0 || m_iLaf0TextureQuality > 2)
		m_iLaf0TextureQuality = 2;
	m_bLaf0Dragging = false;
	m_iFontFamily = 0;
	m_iLanguage = 0;
	if (_stricmp(g_strSelectedML.c_str(), "Eng") == 0) m_iLanguage = 1;
	else if (_stricmp(g_strSelectedML.c_str(), "Spn") == 0) m_iLanguage = 2;
	char savedFont[LF_FACESIZE] = { 0 };
	GetPrivateProfileStringA("Font", "Family", FONT_FAMILIES[0], savedFont, LF_FACESIZE, ".\\config.ini");
	for (int i = 0; i < FONT_FAMILY_COUNT; ++i)
	{
		if (_stricmp(savedFont, FONT_FAMILIES[i]) == 0)
		{
			m_iFontFamily = i;
			break;
		}
	}
	m_iLaf0DragOffsetX = 0;
	m_iLaf0DragOffsetY = 0;
	m_iProTab = 0;
	m_bProResolutionOpen = false;
	m_bProLanguageOpen = false;
	m_bProFontOpen = false;
	m_iProDraggingSlider = -1;
	m_bProCloseHandled = false;
	m_iSnapResolution = -1;
	m_iInterfaceScale = GetPrivateProfileIntA("Interface", "Scale", 100, ".\\config.ini");
	m_iProFontSize = (FontHeight > 0) ? FontHeight : 13;
	m_iScaleHUD = GetPrivateProfileIntA("Interface", "ScaleHUD", 100, ".\\config.ini");
	m_iScaleChat = GetPrivateProfileIntA("Interface", "ScaleChat", 100, ".\\config.ini");
	m_iScaleMiniMap = GetPrivateProfileIntA("Interface", "ScaleMiniMap", 100, ".\\config.ini");
	m_iScaleWindows = GetPrivateProfileIntA("Interface", "ScaleWindows", 100, ".\\config.ini");
	m_iScaleButtons = 100;
	m_iScaleTexts = GetPrivateProfileIntA("Interface", "ScaleTexts", 100, ".\\config.ini");
	m_bItemRGB = GetPrivateProfileIntA("ItemRGB", "Enabled", 0, ".\\config.ini") != 0;
	m_bItemRGBSelecting = false;
	m_iItemRGBTargetType = GetPrivateProfileIntA("ItemRGB", "TargetType", -1, ".\\config.ini");
	m_iItemColorR = GetPrivateProfileIntA("ItemRGB", "R", 255, ".\\config.ini");
	m_iItemColorG = GetPrivateProfileIntA("ItemRGB", "G", 255, ".\\config.ini");
	m_iItemColorB = GetPrivateProfileIntA("ItemRGB", "B", 255, ".\\config.ini");
	m_bShowMonsterHP = GetPrivateProfileIntA("Gameplay", "ShowMonsterHP", 1, ".\\config.ini") != 0;
	m_bShowPlayerHP = GetPrivateProfileIntA("Gameplay", "ShowPlayerHP", 1, ".\\config.ini") != 0;
	m_bCamera3D = GetPrivateProfileIntA("Common", "Camera3D", 1, ".\\config.ini") != 0;
	m_bMiniMapEnabled = GetPrivateProfileIntA("Common", "MiniMap", 1, ".\\config.ini") != 0;
	m_bServerClock = GetPrivateProfileIntA("Common", "ServerClock", 1, ".\\config.ini") != 0;
	m_bFog = GetPrivateProfileIntA("Common", "Fog", 1, ".\\config.ini") != 0;
	m_bAnisotropy = GetPrivateProfileIntA("Common", "Anisotropy", 1, ".\\config.ini") != 0;
	m_bLinearFilter = GetPrivateProfileIntA("Common", "LinearFilter", 1, ".\\config.ini") != 0;
	m_bSkillEffects = GetPrivateProfileIntA("AntiLag", "SkillEffects", 1, ".\\config.ini") != 0;
	m_bRenderMonstersNPC = GetPrivateProfileIntA("AntiLag", "RenderMonstersNPC", 1, ".\\config.ini") != 0;
	m_bRenderCharacters = GetPrivateProfileIntA("AntiLag", "RenderCharacters", 1, ".\\config.ini") != 0;
	m_bRenderWings = GetPrivateProfileIntA("AntiLag", "RenderWings", 1, ".\\config.ini") != 0;
	m_bGlobalMessages = GetPrivateProfileIntA("AntiLag", "GlobalMessages", 1, ".\\config.ini") != 0;
	m_bMonsterHPName = GetPrivateProfileIntA("Options", "MonsterHPName", 1, ".\\config.ini") != 0;
	m_bMonsterHPLevel = GetPrivateProfileIntA("Options", "MonsterHPLevel", 1, ".\\config.ini") != 0;
	m_bRankInfo = GetPrivateProfileIntA("Options", "RankInfo", 1, ".\\config.ini") != 0;
	m_bGuildLogoName = GetPrivateProfileIntA("Options", "GuildLogoName", 1, ".\\config.ini") != 0;
	m_bReduceZenVisual = GetPrivateProfileIntA("Options", "ReduceZenVisual", 0, ".\\config.ini") != 0;
	FogEnable = m_bFog;
	m_bWorldWeather = GetPrivateProfileIntA("WorldVisual", "Weather", 1, ".\\config.ini") != 0;
	m_bWorldTimeOfDay = GetPrivateProfileIntA("WorldVisual", "TimeOfDay", 1, ".\\config.ini") != 0;
	if (m_iInterfaceScale < 50 || m_iInterfaceScale > 150)
		m_iInterfaceScale = 100;
	if (m_iScaleHUD < 50 || m_iScaleHUD > 150) m_iScaleHUD = 100;
	if (m_iScaleChat < 50 || m_iScaleChat > 150) m_iScaleChat = 100;
	if (m_iScaleMiniMap < 50 || m_iScaleMiniMap > 150) m_iScaleMiniMap = 100;
	if (m_iScaleWindows < 50 || m_iScaleWindows > 150) m_iScaleWindows = 100;
	if (m_iScaleButtons < 50 || m_iScaleButtons > 150) m_iScaleButtons = 100;
	if (m_iScaleTexts < 50 || m_iScaleTexts > 150) m_iScaleTexts = 100;
	if (m_iItemColorR < 0 || m_iItemColorR > 255) m_iItemColorR = 255;
	if (m_iItemColorG < 0 || m_iItemColorG > 255) m_iItemColorG = 255;
	if (m_iItemColorB < 0 || m_iItemColorB > 255) m_iItemColorB = 255;
	if (m_iItemRGBTargetType < -1 || m_iItemRGBTargetType > MAX_ITEM) m_iItemRGBTargetType = -1;
}

SEASON3B::CNewUIOptionWindow::~CNewUIOptionWindow()
{
	Release();
}

bool SEASON3B::CNewUIOptionWindow::Create(CNewUIManager* pNewUIMng, int x, int y)
{
	if (NULL == pNewUIMng)
		return false;

	m_pNewUIMng = pNewUIMng;
	m_pNewUIMng->AddUIObj(SEASON3B::INTERFACE_OPTION, this);

	SetPos(x, y);

	LoadImages();

	SetButtonInfo();

	int savedFontSize = (13 * m_iScaleTexts + 50) / 100;
	if (savedFontSize < 8) savedFontSize = 8;
	if (savedFontSize > 24) savedFontSize = 24;
	gwinhandle->SetFontSize(savedFontSize);
	gwinhandle->SetFontFamily(FONT_FAMILIES[m_iFontFamily]);

	Show(false);

	return true;
}

void SEASON3B::CNewUIOptionWindow::SetButtonInfo()
{
	//m_BtnClose.ChangeTextBackColor(RGBA(255, 255, 255, 0));
	//m_BtnClose.ChangeButtonImgState(true, IMAGE_OPTION_BTN_CLOSE, true);
	//m_BtnClose.ChangeButtonInfo(m_Pos.x + 68, m_Pos.y + 209, 54, 30);
	//m_BtnClose.ChangeImgColor(BUTTON_STATE_UP, RGBA(255, 255, 255, 255));
	//m_BtnClose.ChangeImgColor(BUTTON_STATE_DOWN, RGBA(255, 255, 255, 255));

	resolutionList.Create(m_Pos.x + 20.f, m_Pos.y + 50.f, 4);

	fonttextList.Create(m_Pos.x + 20.f, m_Pos.y + 175.f, 3);

	LoadResolution("Data\\Resolutions.xml");
}

void SEASON3B::CNewUIOptionWindow::Release()
{
	UnloadImages();

	if (m_pNewUIMng)
	{
		m_pNewUIMng->RemoveUIObj(this);
		m_pNewUIMng = NULL;
	}
}

void SEASON3B::CNewUIOptionWindow::SetPos(int x, int y)
{
	if (gmProtect != NULL)
	{
		const int maxX=(std::max)(0,(int)GetWindowsX-ProPanelWidth());
        const int maxY=(std::max)(0,(int)GetWindowsY-ProPanelHeight());
        m_Pos.x=(std::max)(0,(std::min)(x,maxX));
        m_Pos.y=(std::max)(0,(std::min)(y,maxY));
		m_bProResolutionOpen = false;
		m_bProLanguageOpen = false;
		m_bProFontOpen = false;
		m_iProDraggingSlider = -1;
		return;
	}

	float RenderFrameX = pos_center() + 190;
	float RenderFrameY = PositionY_In_The_Mid(0) + y;

	m_Pos.x = RenderFrameX;
	m_Pos.y = RenderFrameY;

	resolutionList.SetPosition(m_Pos.x + 20.f, m_Pos.y + 56.f);

	fonttextList.SetPosition(m_Pos.x + 20.f, m_Pos.y + 179.f);
}

bool SEASON3B::CNewUIOptionWindow::UpdateMouseEvent()
{
	if (gmProtect != NULL)
		return UpdateLaf0MouseEvent();

	float RenderFrameX = m_Pos.x;
	float RenderFrameY = m_Pos.y;

	//if (m_BtnClose.UpdateMouseEvent() == true)
	//{
	//	g_pNewUISystem->Hide(SEASON3B::INTERFACE_OPTION);
	//	return false;
	//}

	if (SEASON3B::IsPress(VK_LBUTTON) && CheckMouseIn(RenderFrameX + 210.0, RenderFrameY + 54.0, 15, 15))
	{
		m_RenderEffect = !m_RenderEffect;
	}
	if (SEASON3B::IsPress(VK_LBUTTON) && CheckMouseIn(RenderFrameX + 210.0, RenderFrameY + 70.0, 15, 15))
	{
		m_RenderShadows = !m_RenderShadows;
	}
	if (SEASON3B::IsPress(VK_LBUTTON) && CheckMouseIn(RenderFrameX + 210.0, RenderFrameY + 86.0, 15, 15))
	{
		m_RenderPets = !m_RenderPets;
	}

	if (SEASON3B::IsPress(VK_LBUTTON) && CheckMouseIn(RenderFrameX + 210.0, RenderFrameY + 102.0, 15, 15))
	{
		m_RenderGlow = !m_RenderGlow;
	}
	if (SEASON3B::IsPress(VK_LBUTTON) && CheckMouseIn(RenderFrameX + 210.0, RenderFrameY + 118.0, 15, 15))
	{
		m_RenderEquipment = !m_RenderEquipment;
	}
	if (SEASON3B::IsPress(VK_LBUTTON) && CheckMouseIn(RenderFrameX + 210.0, RenderFrameY + 134.0, 15, 15))
	{
		m_RenderTerrain = !m_RenderTerrain;
	}
	if (SEASON3B::IsPress(VK_LBUTTON) && CheckMouseIn(RenderFrameX + 210.0, RenderFrameY + 150.0, 15, 15))
	{
		m_RenderObjects = !m_RenderObjects;
	}

	if (CheckMouseIn(RenderFrameX + 108 - 8, RenderFrameY + 190, 124 + 8, 16))
	{
		int iOldValue = m_iVolumeLevel;
		if (MouseWheel > 0)
		{
			MouseWheel = 0;
			m_iVolumeLevel++;
			if (m_iVolumeLevel > 10)
			{
				m_iVolumeLevel = 10;
			}
		}
		else if (MouseWheel < 0)
		{
			MouseWheel = 0;
			m_iVolumeLevel--;
			if (m_iVolumeLevel < 0)
			{
				m_iVolumeLevel = 0;
			}
		}
		if (SEASON3B::IsRepeat(VK_LBUTTON))
		{
			int x = MouseX - (RenderFrameX + 108);
			if (x < 0)
			{
				m_iVolumeLevel = 0;
			}
			else
			{
				float fValue = (10.f * x) / 124.f;
				m_iVolumeLevel = (int)fValue + 1;
			}
		}

		if (iOldValue != m_iVolumeLevel)
		{
			SetEffectVolumeLevel(m_iVolumeLevel);
		}
	}
	if (CheckMouseIn(RenderFrameX + 100, RenderFrameY + 224, 141, 29))
	{
		if (SEASON3B::IsRepeat(VK_LBUTTON))
		{
			int x = MouseX - (RenderFrameX + 85);
			float fValue = (5.f * x) / 141.f;
			m_iRenderLevel = (int)fValue;
		}
	}

	if (resolutionList.UpdateMouseEvent())
	{
		change_resolution();
	}

	if (fonttextList.UpdateMouseEvent())
	{
		change_fontsize();
	}

	if (SEASON3B::IsPress(VK_LBUTTON) && CheckMouseIn(RenderFrameX + 103.f, RenderFrameY + 166.f, 12.f, 15.f))
	{
		ChangeFontFamily(-1);
		return false;
	}
	if (SEASON3B::IsPress(VK_LBUTTON) && CheckMouseIn(RenderFrameX + 213.f, RenderFrameY + 166.f, 12.f, 15.f))
	{
		ChangeFontFamily(1);
		return false;
	}

	return !CheckMouseIn(RenderFrameX, RenderFrameY, 260.0, 270.0);
}

bool SEASON3B::CNewUIOptionWindow::UpdateKeyEvent()
{
	if (g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_OPTION) == true)
	{
		if (SEASON3B::IsPress(VK_ESCAPE) == true)
		{
			if (gmProtect != NULL)
			{
				RestoreProSnapshot();
				m_bProCloseHandled = true;
			}
			g_pNewUISystem->Hide(SEASON3B::INTERFACE_OPTION);
			PlayBuffer(SOUND_CLICK01);
			return false;
		}
	}

	return true;
}

bool SEASON3B::CNewUIOptionWindow::Update()
{
	return true;
}

bool SEASON3B::CNewUIOptionWindow::Render()
{
	EnableAlphaTest();

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	RenderFrame();

	RenderContents();

	RenderButtons();

	DisableAlphaBlend();

	return true;
}

float SEASON3B::CNewUIOptionWindow::GetLayerDepth()	//. 10.5f
{
	return 10.5f;
}

float SEASON3B::CNewUIOptionWindow::GetKeyEventOrder()	// 10.f;
{
	return 10.0f;
}

void SEASON3B::CNewUIOptionWindow::CaptureProSnapshot()
{
	m_iSnapResolution = resolutionList.GetCurrent();
	m_bSnapAutoAttack = m_bAutoAttack;
	m_bSnapWhisperSound = m_bWhisperSound;
	m_bSnapSlideHelp = m_bSlideHelp;
	m_iSnapVolumeLevel = m_iVolumeLevel;
	m_iSnapRenderLevel = m_iRenderLevel;
	m_bSnapRenderEffect = m_RenderEffect;
	m_bSnapRenderShadows = m_RenderShadows;
	m_bSnapRenderPets = m_RenderPets;
	m_bSnapRenderGlow = m_RenderGlow;
	m_bSnapRenderEquipment = m_RenderEquipment;
	m_bSnapRenderTerrain = m_RenderTerrain;
	m_bSnapRenderObjects = m_RenderObjects;
	m_bSnapRenderParticles = m_RenderParticles;
	m_iSnapFpsLimit = m_iLaf0FpsLimit;
	m_bSnapVSync = m_bLaf0VSync;
	m_iSnapViewDistance = m_iLaf0ViewDistance;
	m_iSnapTextureQuality = m_iLaf0TextureQuality;
	m_iSnapFontFamily = m_iFontFamily;
	m_iSnapLanguage = m_iLanguage;
	m_iSnapScaleHUD = m_iScaleHUD;
	m_iSnapScaleChat = m_iScaleChat;
	m_iSnapScaleMiniMap = m_iScaleMiniMap;
	m_iSnapScaleWindows = m_iScaleWindows;
	m_iSnapScaleButtons = 100;
	m_iSnapScaleTexts = m_iScaleTexts;
	m_bSnapItemRGB = m_bItemRGB;
	m_iSnapItemRGBTargetType = m_iItemRGBTargetType;
	m_iSnapItemColorR = m_iItemColorR;
	m_iSnapItemColorG = m_iItemColorG;
	m_iSnapItemColorB = m_iItemColorB;
	m_bSnapShowMonsterHP = m_bShowMonsterHP;
	m_bSnapShowPlayerHP = m_bShowPlayerHP;
	m_bSnapCamera3D = m_bCamera3D;

	m_bSnapMiniMapEnabled=m_bMiniMapEnabled; m_bSnapServerClock=m_bServerClock; m_bSnapFog=m_bFog;
	m_bSnapAnisotropy=m_bAnisotropy; m_bSnapLinearFilter=m_bLinearFilter; m_bSnapSkillEffects=m_bSkillEffects;
	m_bSnapRenderMonstersNPC=m_bRenderMonstersNPC; m_bSnapRenderCharacters=m_bRenderCharacters; m_bSnapRenderWings=m_bRenderWings;
	m_bSnapGlobalMessages=m_bGlobalMessages; m_bSnapMonsterHPName=m_bMonsterHPName; m_bSnapMonsterHPLevel=m_bMonsterHPLevel;
	m_bSnapRankInfo=m_bRankInfo; m_bSnapGuildLogoName=m_bGuildLogoName; m_bSnapReduceZenVisual=m_bReduceZenVisual;
	m_bSnapWorldWeather = m_bWorldWeather;
	m_bSnapWorldTimeOfDay = m_bWorldTimeOfDay;
}

void SEASON3B::CNewUIOptionWindow::RestoreProSnapshot()
{
	m_bAutoAttack = m_bSnapAutoAttack;
	m_bWhisperSound = m_bSnapWhisperSound;
	m_bSlideHelp = m_bSnapSlideHelp;
	m_iVolumeLevel = m_iSnapVolumeLevel;
	m_iRenderLevel = m_iSnapRenderLevel;
	m_RenderEffect = m_bSnapRenderEffect;
	m_RenderShadows = m_bSnapRenderShadows;
	m_RenderPets = m_bSnapRenderPets;
	m_RenderGlow = m_bSnapRenderGlow;
	m_RenderEquipment = m_bSnapRenderEquipment;
	m_RenderTerrain = m_bSnapRenderTerrain;
	m_RenderObjects = m_bSnapRenderObjects;
	m_RenderParticles = m_bSnapRenderParticles;
	m_iLaf0FpsLimit = m_iSnapFpsLimit;
	m_bLaf0VSync = m_bSnapVSync;
	m_iLaf0ViewDistance = m_iSnapViewDistance;
	m_iLaf0TextureQuality = m_iSnapTextureQuality;
	m_iFontFamily = m_iSnapFontFamily;
	m_iLanguage = m_iSnapLanguage;
	m_iScaleHUD = m_iSnapScaleHUD;
	m_iScaleChat = m_iSnapScaleChat;
	m_iScaleMiniMap = m_iSnapScaleMiniMap;
	m_iScaleWindows = m_iSnapScaleWindows;
	m_iScaleButtons = 100;
	m_iScaleTexts = m_iSnapScaleTexts;
	m_bItemRGB = m_bSnapItemRGB;
	m_bItemRGBSelecting = false;
	m_iItemRGBTargetType = m_iSnapItemRGBTargetType;
	m_iItemColorR = m_iSnapItemColorR;
	m_iItemColorG = m_iSnapItemColorG;
	m_iItemColorB = m_iSnapItemColorB;
	m_bShowMonsterHP = m_bSnapShowMonsterHP;
	m_bShowPlayerHP = m_bSnapShowPlayerHP;
	m_bCamera3D = m_bSnapCamera3D;

	m_bMiniMapEnabled=m_bSnapMiniMapEnabled; m_bServerClock=m_bSnapServerClock; m_bFog=m_bSnapFog;
	m_bAnisotropy=m_bSnapAnisotropy; m_bLinearFilter=m_bSnapLinearFilter; m_bSkillEffects=m_bSnapSkillEffects;
	m_bRenderMonstersNPC=m_bSnapRenderMonstersNPC; m_bRenderCharacters=m_bSnapRenderCharacters; m_bRenderWings=m_bSnapRenderWings;
	m_bGlobalMessages=m_bSnapGlobalMessages; m_bMonsterHPName=m_bSnapMonsterHPName; m_bMonsterHPLevel=m_bSnapMonsterHPLevel;
	m_bRankInfo=m_bSnapRankInfo; m_bGuildLogoName=m_bSnapGuildLogoName; m_bReduceZenVisual=m_bSnapReduceZenVisual; FogEnable=m_bFog;
	m_bWorldWeather = m_bSnapWorldWeather;
	m_bWorldTimeOfDay = m_bSnapWorldTimeOfDay;

	if (m_iSnapResolution >= 0 && resolutionList.GetCurrent() != m_iSnapResolution)
	{
		resolutionList.SetCurrent(m_iSnapResolution);
		change_resolution();
	}

	SetEffectVolumeLevel(m_iVolumeLevel);
	gmProtect->ajust_fps_render = (BYTE)m_iLaf0FpsLimit;
	g_bVSyncEnabled = m_bLaf0VSync ? TRUE : FALSE;
	if (IsVSyncAvailable())
	{
		if (m_bLaf0VSync) EnableVSync();
		else DisableVSync();
	}

	int fontSize = (13 * m_iScaleTexts + 50) / 100;
	if (fontSize < 8) fontSize = 8;
	if (fontSize > 24) fontSize = 24;
	gwinhandle->SetFontSize(fontSize);
	gwinhandle->SetFontFamily(FONT_FAMILIES[m_iFontFamily]);

	static const char* LANG_CODE[3] = { "Por", "Eng", "Spn" };
	g_strSelectedML = LANG_CODE[m_iLanguage];
	ReloadRuntimeLanguage();
}

void SEASON3B::CNewUIOptionWindow::OpenningProcess()
{
	if (gmProtect != NULL)
	{
		SetPos(((int)GetWindowsX - ProPanelWidth()) / 2, ((int)GetWindowsY - ProPanelHeight()) / 2);
		m_bLaf0Dragging = false;
		m_bProResolutionOpen = false;
		m_bProLanguageOpen = false;
		m_bProFontOpen = false;
		m_iProDraggingSlider = -1;
		m_bProCloseHandled = false;

		m_iLaf0FpsLimit = GetPrivateProfileIntA("Renderer", "FrameLimit", 60, ".\\config.ini");
		const int fpsAllowed[4] = { 25,30,60,240 };
		bool fpsOk = false;
		for (int i = 0; i < 4; ++i)
			if (m_iLaf0FpsLimit == fpsAllowed[i]) fpsOk = true;
		if (!fpsOk) m_iLaf0FpsLimit = 60;

		m_bLaf0VSync = GetPrivateProfileIntA("Renderer", "VSync", g_bVSyncEnabled ? 1 : 0, ".\\config.ini") != 0;
		m_RenderEffect = GetPrivateProfileIntA("AntiLag", "RenderEffect", m_RenderEffect ? 1 : 0, ".\\config.ini") != 0;
		m_RenderShadows = GetPrivateProfileIntA("AntiLag", "RenderShadows", m_RenderShadows ? 1 : 0, ".\\config.ini") != 0;
		m_RenderPets = GetPrivateProfileIntA("AntiLag", "RenderPets", m_RenderPets ? 1 : 0, ".\\config.ini") != 0;
		m_RenderGlow = GetPrivateProfileIntA("AntiLag", "RenderGlow", m_RenderGlow ? 1 : 0, ".\\config.ini") != 0;
		m_RenderEquipment = GetPrivateProfileIntA("AntiLag", "RenderEquipment", m_RenderEquipment ? 1 : 0, ".\\config.ini") != 0;
		m_RenderTerrain = GetPrivateProfileIntA("AntiLag", "RenderTerrain", m_RenderTerrain ? 1 : 0, ".\\config.ini") != 0;
		m_RenderObjects = GetPrivateProfileIntA("AntiLag", "RenderObjects", m_RenderObjects ? 1 : 0, ".\\config.ini") != 0;
		m_RenderParticles = GetPrivateProfileIntA("AntiLag", "RenderParticles", m_RenderParticles ? 1 : 0, ".\\config.ini") != 0;
		m_iLaf0ViewDistance = GetPrivateProfileIntA("AntiLag", "ViewDistance", m_iLaf0ViewDistance, ".\\config.ini");
		if (m_iLaf0ViewDistance < 0 || m_iLaf0ViewDistance > 2) m_iLaf0ViewDistance = 1;
		m_iRenderLevel = GetPrivateProfileIntA("Renderer", "RenderLevel", m_iRenderLevel, ".\\config.ini");
		if (m_iRenderLevel < 0 || m_iRenderLevel > 4) m_iRenderLevel = 4;
		m_iLaf0TextureQuality = GetPrivateProfileIntA("Renderer", "TextureQuality", m_iLaf0TextureQuality, ".\\config.ini");
		if (m_iLaf0TextureQuality < 0 || m_iLaf0TextureQuality > 2) m_iLaf0TextureQuality = 2;

		m_iScaleHUD = GetPrivateProfileIntA("Interface", "ScaleHUD", m_iScaleHUD, ".\\config.ini");
		m_iScaleChat = GetPrivateProfileIntA("Interface", "ScaleChat", m_iScaleChat, ".\\config.ini");
		m_iScaleMiniMap = GetPrivateProfileIntA("Interface", "ScaleMiniMap", m_iScaleMiniMap, ".\\config.ini");
		m_iScaleWindows = GetPrivateProfileIntA("Interface", "ScaleWindows", m_iScaleWindows, ".\\config.ini");
		m_iScaleButtons = GetPrivateProfileIntA("Interface", "ScaleButtons", m_iScaleButtons, ".\\config.ini");
		m_iScaleTexts = GetPrivateProfileIntA("Interface", "ScaleTexts", m_iScaleTexts, ".\\config.ini");
		m_bItemRGB = GetPrivateProfileIntA("ItemRGB", "Enabled", m_bItemRGB ? 1 : 0, ".\\config.ini") != 0;
		m_bItemRGBSelecting = false;
		m_iItemRGBTargetType = GetPrivateProfileIntA("ItemRGB", "TargetType", m_iItemRGBTargetType, ".\\config.ini");
		m_iItemColorR = GetPrivateProfileIntA("ItemRGB", "R", m_iItemColorR, ".\\config.ini");
		m_iItemColorG = GetPrivateProfileIntA("ItemRGB", "G", m_iItemColorG, ".\\config.ini");
		m_iItemColorB = GetPrivateProfileIntA("ItemRGB", "B", m_iItemColorB, ".\\config.ini");
		if (m_iScaleHUD < 50 || m_iScaleHUD > 150) m_iScaleHUD = 100;
		if (m_iScaleChat < 50 || m_iScaleChat > 150) m_iScaleChat = 100;
		if (m_iScaleMiniMap < 50 || m_iScaleMiniMap > 150) m_iScaleMiniMap = 100;
		if (m_iScaleWindows < 50 || m_iScaleWindows > 150) m_iScaleWindows = 100;
		if (m_iScaleButtons < 50 || m_iScaleButtons > 150) m_iScaleButtons = 100;
		if (m_iScaleTexts < 50 || m_iScaleTexts > 150) m_iScaleTexts = 100;
		if (m_iItemColorR < 0 || m_iItemColorR > 255) m_iItemColorR = 255;
		if (m_iItemColorG < 0 || m_iItemColorG > 255) m_iItemColorG = 255;
		if (m_iItemColorB < 0 || m_iItemColorB > 255) m_iItemColorB = 255;
		if (m_iItemRGBTargetType < -1 || m_iItemRGBTargetType > MAX_ITEM) m_iItemRGBTargetType = -1;

		m_bShowMonsterHP = GetPrivateProfileIntA("Gameplay", "ShowMonsterHP", m_bShowMonsterHP ? 1 : 0, ".\\config.ini") != 0;
		m_bShowPlayerHP = GetPrivateProfileIntA("Gameplay", "ShowPlayerHP", m_bShowPlayerHP ? 1 : 0, ".\\config.ini") != 0;
		m_bCamera3D = CameraFactorPtr->IsEnable();
		m_bMiniMapEnabled=GetPrivateProfileIntA("Common","MiniMap",m_bMiniMapEnabled?1:0,".\\config.ini")!=0;
		m_bServerClock=GetPrivateProfileIntA("Common","ServerClock",m_bServerClock?1:0,".\\config.ini")!=0;
		m_bFog=GetPrivateProfileIntA("Common","Fog",m_bFog?1:0,".\\config.ini")!=0;
		m_bAnisotropy=GetPrivateProfileIntA("Common","Anisotropy",m_bAnisotropy?1:0,".\\config.ini")!=0;
		m_bLinearFilter=GetPrivateProfileIntA("Common","LinearFilter",m_bLinearFilter?1:0,".\\config.ini")!=0;
		m_bSkillEffects=GetPrivateProfileIntA("AntiLag","SkillEffects",m_bSkillEffects?1:0,".\\config.ini")!=0;
		m_bRenderMonstersNPC=GetPrivateProfileIntA("AntiLag","RenderMonstersNPC",m_bRenderMonstersNPC?1:0,".\\config.ini")!=0;
		m_bRenderCharacters=GetPrivateProfileIntA("AntiLag","RenderCharacters",m_bRenderCharacters?1:0,".\\config.ini")!=0;
		m_bRenderWings=GetPrivateProfileIntA("AntiLag","RenderWings",m_bRenderWings?1:0,".\\config.ini")!=0;
		m_bGlobalMessages=GetPrivateProfileIntA("AntiLag","GlobalMessages",m_bGlobalMessages?1:0,".\\config.ini")!=0;
		m_bMonsterHPName=GetPrivateProfileIntA("Options","MonsterHPName",m_bMonsterHPName?1:0,".\\config.ini")!=0;
		m_bMonsterHPLevel=GetPrivateProfileIntA("Options","MonsterHPLevel",m_bMonsterHPLevel?1:0,".\\config.ini")!=0;
		m_bRankInfo=GetPrivateProfileIntA("Options","RankInfo",m_bRankInfo?1:0,".\\config.ini")!=0;
		m_bGuildLogoName=GetPrivateProfileIntA("Options","GuildLogoName",m_bGuildLogoName?1:0,".\\config.ini")!=0;
		m_bReduceZenVisual=GetPrivateProfileIntA("Options","ReduceZenVisual",m_bReduceZenVisual?1:0,".\\config.ini")!=0;
		FogEnable=m_bFog;
		m_bWorldWeather = GetPrivateProfileIntA("WorldVisual", "Weather", m_bWorldWeather ? 1 : 0, ".\\config.ini") != 0;
		m_bWorldTimeOfDay = GetPrivateProfileIntA("WorldVisual", "TimeOfDay", m_bWorldTimeOfDay ? 1 : 0, ".\\config.ini") != 0;

		m_iLanguage = 0;
		if (_stricmp(g_strSelectedML.c_str(), "Eng") == 0) m_iLanguage = 1;
		else if (_stricmp(g_strSelectedML.c_str(), "Spn") == 0) m_iLanguage = 2;

		CaptureProSnapshot();
	}
}

void SEASON3B::CNewUIOptionWindow::ClosingProcess()
{
	if (gmProtect != NULL)
	{
		// OK/Cancel/X handle themselves. Any external close behaves like Cancel.
		if (!m_bProCloseHandled)
			RestoreProSnapshot();
		m_bProCloseHandled = false;
		return;
	}

	WritePrivateProfileStringA("AntiLag", "RenderShadows", m_RenderShadows ? "1" : "0", ".\\config.ini");
	WritePrivateProfileStringA("AntiLag", "RenderPets", m_RenderPets ? "1" : "0", ".\\config.ini");
	WritePrivateProfileStringA("AntiLag", "RenderGlow", m_RenderGlow ? "1" : "0", ".\\config.ini");
	SaveOptions();
}

void SEASON3B::CNewUIOptionWindow::SetGameOptions(BYTE GameOption)
{
	if ((GameOption & AUTOATTACK_ON) == AUTOATTACK_ON)
	{
		this->SetAutoAttack(true);
	}
	else
	{
		this->SetAutoAttack(false);
	}

	if ((GameOption & WHISPER_SOUND_ON) == WHISPER_SOUND_ON)
	{
		this->SetWhisperSound(true);
	}
	else
	{
		this->SetWhisperSound(false);
	}

	if ((GameOption & SLIDE_HELP_ON) == SLIDE_HELP_ON)
	{
		this->SetSlideHelp(true);
	}
	else
	{
		this->SetSlideHelp(false);
	}

	if ((GameOption & RENDER_EFFECT_ON) == RENDER_EFFECT_ON)
	{
		this->SetRenderEffect(true);
	}
	else
	{
		this->SetRenderEffect(false);
	}

	if ((GameOption & RENDER_EQUIPMENT_ON) == RENDER_EQUIPMENT_ON)
	{
		this->SetRenderEquipment(true);
	}
	else
	{
		this->SetRenderEquipment(false);
	}

	if ((GameOption & RENDER_TERRAIN_ON) == RENDER_TERRAIN_ON)
	{
		this->SetRenderTerrain(true);
	}
	else
	{
		this->SetRenderTerrain(false);
	}

	if ((GameOption & RENDER_OBJECTS_ON) == RENDER_OBJECTS_ON)
	{
		this->SetRenderObjects(true);
	}
	else
	{
		this->SetRenderObjects(false);
	}
}

void SEASON3B::CNewUIOptionWindow::LoadImages()
{
	LoadBitmap("Interface\\newui_button_close.tga", IMAGE_OPTION_BTN_CLOSE, GL_LINEAR);
	LoadBitmap("Interface\\newui_msgbox_back.jpg", IMAGE_OPTION_FRAME_BACK, GL_LINEAR);
	LoadBitmap("Interface\\newui_item_back03.tga", IMAGE_OPTION_FRAME_DOWN, GL_LINEAR);
	LoadBitmap("Interface\\newui_option_top.tga", IMAGE_OPTION_FRAME_UP, GL_LINEAR);
	LoadBitmap("Interface\\newui_option_back06(L).tga", IMAGE_OPTION_FRAME_LEFT, GL_LINEAR);
	LoadBitmap("Interface\\newui_option_back06(R).tga", IMAGE_OPTION_FRAME_RIGHT, GL_LINEAR);
	LoadBitmap("Interface\\newui_option_line.jpg", IMAGE_OPTION_LINE, GL_LINEAR);
	LoadBitmap("Interface\\newui_option_point.tga", IMAGE_OPTION_POINT, GL_LINEAR);
	LoadBitmap("Interface\\newui_option_check.tga", IMAGE_OPTION_BTN_CHECK, GL_LINEAR);
	LoadBitmap("Interface\\newui_option_effect03.tga", IMAGE_OPTION_EFFECT_BACK, GL_LINEAR);
	LoadBitmap("Interface\\newui_option_effect04.tga", IMAGE_OPTION_EFFECT_COLOR, GL_LINEAR);
	LoadBitmap("Interface\\newui_option_volume01.tga", IMAGE_OPTION_VOLUME_BACK, GL_LINEAR);
	LoadBitmap("Interface\\newui_option_volume02.tga", IMAGE_OPTION_VOLUME_COLOR, GL_LINEAR);

	LoadBitmap("Interface\\HUD\\checkbox.tga", IMAGE_CHECK_LIVE);
	LoadBitmap("Interface\\HUD\\uncheckbox.tga", IMAGE_UNCHECK_LIVE);
	LoadBitmap("Interface\\HUD\\button_arrow_L.tga", IMAGE_FONT_ARROW_LEFT, GL_LINEAR, GL_CLAMP_TO_EDGE);
	LoadBitmap("Interface\\HUD\\button_arrow_R.tga", IMAGE_FONT_ARROW_RIGHT, GL_LINEAR, GL_CLAMP_TO_EDGE);
	LoadBitmap("Interface\\SettingsPro\\settings_frame.tga", BITMAP_SETTINGSPRO_FRAME, GL_LINEAR, GL_CLAMP_TO_EDGE);
	LoadBitmap("Interface\\SettingsPro\\nav_active.tga", BITMAP_SETTINGSPRO_NAV_ACTIVE, GL_LINEAR, GL_CLAMP_TO_EDGE);
	LoadBitmap("Interface\\SettingsPro\\dropdown.tga", BITMAP_SETTINGSPRO_DROPDOWN, GL_LINEAR, GL_CLAMP_TO_EDGE);
	LoadBitmap("Interface\\SettingsPro\\switch_on.tga", BITMAP_SETTINGSPRO_SWITCH_ON, GL_LINEAR, GL_CLAMP_TO_EDGE);
	LoadBitmap("Interface\\SettingsPro\\switch_off.tga", BITMAP_SETTINGSPRO_SWITCH_OFF, GL_LINEAR, GL_CLAMP_TO_EDGE);
	LoadBitmap("Interface\\SettingsPro\\minus.tga", BITMAP_SETTINGSPRO_MINUS, GL_LINEAR, GL_CLAMP_TO_EDGE);
	LoadBitmap("Interface\\SettingsPro\\plus.tga", BITMAP_SETTINGSPRO_PLUS, GL_LINEAR, GL_CLAMP_TO_EDGE);
	LoadBitmap("Interface\\SettingsPro\\slider.tga", BITMAP_SETTINGSPRO_SLIDER, GL_LINEAR, GL_CLAMP_TO_EDGE);
	LoadBitmap("Interface\\SettingsPro\\slider_knob.tga", BITMAP_SETTINGSPRO_KNOB, GL_LINEAR, GL_CLAMP_TO_EDGE);
	LoadBitmap("Interface\\SettingsPro\\button.tga", BITMAP_SETTINGSPRO_BUTTON, GL_LINEAR, GL_CLAMP_TO_EDGE);
	LoadBitmap("Interface\\SettingsPro\\button_hover.tga", BITMAP_SETTINGSPRO_BUTTON_HOVER, GL_LINEAR, GL_CLAMP_TO_EDGE);
	LoadBitmap("Interface\\SettingsPro\\close.tga", BITMAP_SETTINGSPRO_CLOSE, GL_LINEAR, GL_CLAMP_TO_EDGE);
	LoadBitmap("Interface\\SettingsPro\\server_time.tga", BITMAP_SERVER_TIME_PANEL, GL_LINEAR, GL_CLAMP_TO_EDGE);
	// O painel avancado continua disponivel nos Looks 0 e 4, mas cada Look
	// carrega seu proprio arquivo. Assim a arte da interface pode evoluir sem
	// substituir a interface compartilhada pelo Look 0.
	if (gmProtect != NULL && gmProtect->LookAndFeel == 4)
	{
		LoadBitmap("Interface\\Look-4\\laf0_antilag_panel.tga", BITMAP_LAF0_ANTILAG_PANEL, GL_LINEAR, GL_CLAMP_TO_EDGE);
	}
	else
	{
		LoadBitmap("Interface\\laf0_antilag_panel.tga", BITMAP_LAF0_ANTILAG_PANEL, GL_LINEAR, GL_CLAMP_TO_EDGE);
	}
}

void SEASON3B::CNewUIOptionWindow::UnloadImages()
{
	DeleteBitmap(BITMAP_LAF0_ANTILAG_PANEL);
	DeleteBitmap(BITMAP_SETTINGSPRO_FRAME);
	DeleteBitmap(BITMAP_SETTINGSPRO_NAV_ACTIVE);
	DeleteBitmap(BITMAP_SETTINGSPRO_DROPDOWN);
	DeleteBitmap(BITMAP_SETTINGSPRO_SWITCH_ON);
	DeleteBitmap(BITMAP_SETTINGSPRO_SWITCH_OFF);
	DeleteBitmap(BITMAP_SETTINGSPRO_MINUS);
	DeleteBitmap(BITMAP_SETTINGSPRO_PLUS);
	DeleteBitmap(BITMAP_SETTINGSPRO_SLIDER);
	DeleteBitmap(BITMAP_SETTINGSPRO_KNOB);
	DeleteBitmap(BITMAP_SETTINGSPRO_BUTTON);
	DeleteBitmap(BITMAP_SETTINGSPRO_BUTTON_HOVER);
	DeleteBitmap(BITMAP_SETTINGSPRO_CLOSE);
	DeleteBitmap(BITMAP_SERVER_TIME_PANEL);
	DeleteBitmap(IMAGE_FONT_ARROW_LEFT);
	DeleteBitmap(IMAGE_FONT_ARROW_RIGHT);
	DeleteBitmap(IMAGE_OPTION_BTN_CLOSE);
	DeleteBitmap(IMAGE_OPTION_FRAME_BACK);
	DeleteBitmap(IMAGE_OPTION_FRAME_DOWN);
	DeleteBitmap(IMAGE_OPTION_FRAME_UP);
	DeleteBitmap(IMAGE_OPTION_FRAME_LEFT);
	DeleteBitmap(IMAGE_OPTION_FRAME_RIGHT);
	DeleteBitmap(IMAGE_OPTION_LINE);
	DeleteBitmap(IMAGE_OPTION_POINT);
	DeleteBitmap(IMAGE_OPTION_BTN_CHECK);
	DeleteBitmap(IMAGE_OPTION_EFFECT_BACK);
	DeleteBitmap(IMAGE_OPTION_EFFECT_COLOR);
	DeleteBitmap(IMAGE_OPTION_VOLUME_BACK);
	DeleteBitmap(IMAGE_OPTION_VOLUME_COLOR);
}

void SEASON3B::CNewUIOptionWindow::RenderFrame()
{
    if(gmProtect != NULL)
    {
        const float ps=ProUIScale(),x=static_cast<float>(m_Pos.x),y=static_cast<float>(m_Pos.y);
        EnableAlphaTest();glColor4f(1.f,1.f,1.f,1.f);
        window_backmsg(x,y,static_cast<float>(ProPanelWidth()),static_cast<float>(ProPanelHeight()));
        RenderTable(x+20*ps,y+12*ps,860*ps,48*ps);
        RenderTable(x+20*ps,y+72*ps,300*ps,472*ps);
        RenderTable(x+334*ps,y+72*ps,546*ps,472*ps);
        RenderTable(x+20*ps,y+552*ps,300*ps,64*ps);
        for(int i=0;i<3;++i) {
            const float nx=x+PRO_NAV_X*ps,ny=y+(PRO_NAV_Y+i*PRO_NAV_STEP)*ps;
            if(i==m_iProTab) {
                glColor4f(0.10f,0.23f,0.34f,0.95f);RenderColor(nx+3*ps,ny+3*ps,(PRO_NAV_W-6)*ps,(PRO_NAV_H-6)*ps,0.f,0);EndRenderColor();glColor4f(1.f,1.f,1.f,1.f);
            }
            RenderTable(nx,ny,PRO_NAV_W*ps,PRO_NAV_H*ps);
        }
        const float fx[3]={20.f,174.f,328.f};
        for(int i=0;i<3;++i)window_backmsg(x+ProContentX(fx[i])*ps,y+ProContentY(PRO_FOOTER_Y)*ps,150*ps,PRO_FOOTER_H*ps);
        RenderBitmap(BITMAP_SETTINGSPRO_CLOSE,x+PRO_CLOSE_X*ps,y+PRO_CLOSE_Y*ps,PRO_CLOSE_W*ps,PRO_CLOSE_H*ps,0.f,0.f,1.f,1.f,true,true,0.f);
        return;
    }

	window_backmsg(m_Pos.x, m_Pos.y, 260.f, 270.f);

	// Faixas desenhadas por codigo: mantem o cliente leve e todos os elementos
	// presos a mesma origem da janela durante o movimento.
	RenderTable(m_Pos.x + 10.f, m_Pos.y + 25.f, 240.f, 25.f);

	RenderTable(m_Pos.x + 15.f, m_Pos.y + 52.f, 75.f, 108.f);

	RenderTable(m_Pos.x + 15.f, m_Pos.y + 174.f, 75.f, 81.f);

	int Bitmap_point_draw = CNewUICursedTempleSystem::IMAGE_CURSEDTEMPLESYSTEM_MINIMAPICON_HOLYITEM_PC;

	RenderBitmap(Bitmap_point_draw, ((float)m_Pos.x + 18.f), (float)m_Pos.y + 39.f, 8.f, 8.f, 0.f, 0.f, 14.f / 16.f, 14.f / 16.f);

	RenderBitmap(Bitmap_point_draw, ((float)m_Pos.x + 18.f), (float)m_Pos.y + 161.f, 8.f, 8.f, 0.f, 0.f, 14.f / 16.f, 14.f / 16.f);

	resolutionList.Render();

	fonttextList.Render();
}


void SEASON3B::CNewUIOptionWindow::RenderContents()
{
	if (gmProtect != NULL)
	{
		RenderLaf0Contents();
		return;
	}

	float RenderFrameX = m_Pos.x;
	float RenderFrameY = m_Pos.y;

	g_pRenderText->SetBgColor(0);
	g_pRenderText->SetFont(g_hFontBold);
	g_pRenderText->SetTextColor(CLRDW_GOLD);

	// Título principal carregado conforme o idioma selecionado.
	g_pRenderText->RenderText(RenderFrameX + 130, RenderFrameY + 10, gTextClien.TextClien_Khac[10], 0, 0, 8);

	// Sub-t?tulos (Original: gTextClien.TextClien_Khac[4] e [5])
	g_pRenderText->RenderText(RenderFrameX + 30, RenderFrameY + 39, gTextClien.TextClien_Khac[11]);
	g_pRenderText->RenderText(RenderFrameX + 30, RenderFrameY + 161, gTextClien.TextClien_Khac[12]);

	g_pRenderText->SetFont(g_hFont);
	g_pRenderText->SetTextColor(255, 255, 255, 255);
	g_pRenderText->SetBgColor(0);

	// --- OP??ES DE SISTEMA ---
	g_pRenderText->RenderText(RenderFrameX + 100, RenderFrameY + 54, gTextClien.TextClien_Khac[13], 0, 15);
	g_pRenderText->RenderText(RenderFrameX + 100, RenderFrameY + 70, gTextClien.TextClien_Khac[14], 0, 15);
	g_pRenderText->RenderText(RenderFrameX + 100, RenderFrameY + 86, gTextClien.TextClien_Khac[15], 0, 15);

	// --- OP??ES ANTI-LAG (Onde dava NULL nas gTextClien.TextClien_Khac[6..9]) ---
	g_pRenderText->RenderText(RenderFrameX + 100, RenderFrameY + 102, gTextClien.TextClien_Khac[16], 0, 15);
	g_pRenderText->RenderText(RenderFrameX + 100, RenderFrameY + 118, gTextClien.TextClien_Khac[17], 0, 15);
	g_pRenderText->RenderText(RenderFrameX + 100, RenderFrameY + 134, gTextClien.TextClien_Khac[18], 0, 15);
	g_pRenderText->RenderText(RenderFrameX + 100, RenderFrameY + 150, gTextClien.TextClien_Khac[19], 0, 15);
	g_pRenderText->SetTextColor(255, 224, 145, 255);
	char fontLabel[64];
	sprintf_s(fontLabel, "%s", FONT_FAMILIES[m_iFontFamily]);
	RenderImage(IMAGE_FONT_ARROW_LEFT, RenderFrameX + 103.f, RenderFrameY + 166.f,
		11.f, 14.f, 0.f, 0.f, 1.f, 1.f);
	g_pRenderText->RenderText(RenderFrameX + 116, RenderFrameY + 167, fontLabel, 96, 15, RT3_SORT_CENTER);
	RenderImage(IMAGE_FONT_ARROW_RIGHT, RenderFrameX + 213.f, RenderFrameY + 166.f,
		11.f, 14.f, 0.f, 0.f, 1.f, 1.f);

	// --- V?DEO & SOM ---
	g_pRenderText->RenderText(RenderFrameX + 100, RenderFrameY + 180, gTextClien.TextClien_Khac[20]);
	g_pRenderText->RenderText(RenderFrameX + 100, RenderFrameY + 214, gTextClien.TextClien_Khac[21]);
}

void SEASON3B::CNewUIOptionWindow::RenderChecked(float RenderFrameX, float RenderFrameY, bool bEnable)
{
	if (bEnable)
		RenderImage(IMAGE_CHECK_LIVE, RenderFrameX, RenderFrameY, 15.0, 15.0, 0.0, 0.0, 0.75, 0.75);
	else
		RenderImage(IMAGE_UNCHECK_LIVE, RenderFrameX, RenderFrameY, 15.0, 15.0, 0.0, 0.0, 0.75, 0.75);
}

void SEASON3B::CNewUIOptionWindow::RenderButtons()
{
	if (gmProtect != NULL)
	{
		RenderLaf0Buttons();
		return;
	}

	float RenderFrameX = m_Pos.x;
	float RenderFrameY = m_Pos.y;

	RenderChecked(RenderFrameX + 210.0, RenderFrameY + 54.0, !m_RenderEffect);

	RenderChecked(RenderFrameX + 210.0, RenderFrameY + 70.0, !m_RenderShadows);

	RenderChecked(RenderFrameX + 210.0, RenderFrameY + 86.0, !m_RenderPets);

	RenderChecked(RenderFrameX + 210.0, RenderFrameY + 102.0, !m_RenderGlow);

	RenderChecked(RenderFrameX + 210.0, RenderFrameY + 118.0, !m_RenderEquipment);

	RenderChecked(RenderFrameX + 210.0, RenderFrameY + 134.0, !m_RenderTerrain);

	RenderChecked(RenderFrameX + 210.0, RenderFrameY + 150.0, !m_RenderObjects);


	RenderImage(IMAGE_OPTION_VOLUME_BACK, RenderFrameX + 108, RenderFrameY + 190, 124.f, 16.f);

	if (m_iVolumeLevel > 0)
	{
		RenderImage(IMAGE_OPTION_VOLUME_COLOR, RenderFrameX + 108, RenderFrameY + 190, 124.f * 0.1f * (m_iVolumeLevel), 16.f);
	}

	RenderImage(IMAGE_OPTION_EFFECT_BACK, RenderFrameX + 100, RenderFrameY + 224, 141.f, 29.f);

	if (m_iRenderLevel >= 0)
	{
		RenderImage(IMAGE_OPTION_EFFECT_COLOR, RenderFrameX + 100, RenderFrameY + 224, 141.f * 0.2f * (m_iRenderLevel + 1), 29.f);
	}
}

bool SEASON3B::CNewUIOptionWindow::UpdateLaf0MouseEvent()
{
	return UpdateProTabMouseEvent();
}

void SEASON3B::CNewUIOptionWindow::SetProTab(int tab)
{
	if(tab>=0 && tab<3) m_iProTab=tab;
	m_bProResolutionOpen=false;
	m_bProLanguageOpen=false;
	m_bProFontOpen=false;
	m_bItemRGBSelecting=false;
	m_iProDraggingSlider=-1;
}

void SEASON3B::CNewUIOptionWindow::CycleFpsLimit()
{
	const int values[4]={25,30,60,240};
	int idx=0; for(int i=0;i<4;++i) if(m_iLaf0FpsLimit==values[i]) idx=i;
	m_iLaf0FpsLimit=values[(idx+1)%4];
}

void SEASON3B::CNewUIOptionWindow::SaveInterfaceScale()
{
	char v[16]; sprintf_s(v,"%d",m_iInterfaceScale);
	WritePrivateProfileStringA("Interface","Scale",v,".\\config.ini");
}

bool SEASON3B::CNewUIOptionWindow::UpdateProTabMouseEvent()
{
    const float x=(float)m_Pos.x, y=(float)m_Pos.y, ps=ProUIScale();
#define HX(v) (x+ProContentX(v)*ps)
#define HY(v) (y+ProContentY(v)*ps)
#define HS(v) ((v)*ps)

    // Listas abertas ficam acima dos demais controles e usam exatamente
    // as mesmas coordenadas usadas no render.
    if(m_iProTab==0 && m_bProLanguageOpen)
    {
        if(SEASON3B::IsPress(VK_LBUTTON))
        {
            for(int i=0;i<3;++i)
            {
                if(CheckMouseIn(HX(205.f),HY(479.f+i*26.f),HS(250.f),HS(26.f)))
                {
                    m_iLanguage=i;
                    static const char* code[3]={"Por","Eng","Spn"};
                    g_strSelectedML=code[m_iLanguage];
                    ReloadRuntimeLanguage();
                    m_bProLanguageOpen=false;
                    PlayBuffer(SOUND_CLICK01);
                    return false;
                }
            }
            if(!CheckMouseIn(HX(205.f),HY(445.f),HS(250.f),HS(112.f)))
                m_bProLanguageOpen=false;
        }
        return false;
    }

    if(m_iProTab==0 && m_bProResolutionOpen)
    {
        int visible=(int)m_ProResolutions.size(); if(visible>3)visible=3;
        int first=resolutionList.GetCurrent()-1; if(first<0)first=0;
        int mf=(int)m_ProResolutions.size()-visible; if(first>mf)first=mf; if(first<0)first=0;
        if(SEASON3B::IsPress(VK_LBUTTON))
        {
            for(int i=0;i<visible;++i)
            {
                if(CheckMouseIn(HX(205.f),HY(529.f+i*24.f),HS(250.f),HS(24.f)))
                {
                    resolutionList.SetCurrent(first+i);
                    m_bProResolutionOpen=false;
                    change_resolution();
                    PlayBuffer(SOUND_CLICK01);
                    return false;
                }
            }
            if(!CheckMouseIn(HX(205.f),HY(495.f),HS(250.f),HS(34.f+visible*24.f)))
                m_bProResolutionOpen=false;
        }
        return false;
    }

    // Arraste apenas pelo titulo. Abas e X nao fazem a janela andar.
    if(SEASON3B::IsPress(VK_LBUTTON) &&
       CheckMouseIn(x+62.f*ps,y+8.f*ps,780.f*ps,38.f*ps) &&
       !CheckMouseIn(x+PRO_CLOSE_X*ps,y+PRO_CLOSE_Y*ps,HS(PRO_CLOSE_W),HS(PRO_CLOSE_H)))
    {
        m_bLaf0Dragging=true;
        m_iLaf0DragOffsetX=(int)MouseX-m_Pos.x;
        m_iLaf0DragOffsetY=(int)MouseY-m_Pos.y;
        return false;
    }
    if(m_bLaf0Dragging && MouseLButton)
    {
        SetPos((int)MouseX-m_iLaf0DragOffsetX,(int)MouseY-m_iLaf0DragOffsetY);
        return false;
    }
    if(!MouseLButton) m_bLaf0Dragging=false;

    if(SEASON3B::IsPress(VK_LBUTTON))
    {
        if(CheckMouseIn(x+PRO_CLOSE_X*ps,y+PRO_CLOSE_Y*ps,HS(PRO_CLOSE_W),HS(PRO_CLOSE_H)))
        {
            RestoreProSnapshot();
            m_bProCloseHandled=true;
            g_pNewUISystem->Hide(SEASON3B::INTERFACE_OPTION);
            PlayBuffer(SOUND_CLICK01);
            return false;
        }

        for(int i=0;i<3;++i)
        {
            if(CheckMouseIn(x+PRO_NAV_X*ps,y+(PRO_NAV_Y+i*PRO_NAV_STEP)*ps,HS(PRO_NAV_W),HS(PRO_NAV_H)))
            {
                SetProTab(i);
                PlayBuffer(SOUND_CLICK01);
                return false;
            }
        }

        if(m_iProTab==0)
        {
            const float rows[6]={166.f,203.f,240.f,277.f,356.f,393.f};
            bool* values[6]={&m_bCamera3D,&m_bMiniMapEnabled,&m_bServerClock,&m_bFog,&m_bAnisotropy,&m_bLinearFilter};
            for(int i=0;i<6;++i)
            {
                if(CheckMouseIn(HX(40.f),HY(rows[i]),HS(425.f),HS(34.f)))
                {
                    *values[i]=!*values[i];
                    if(i==3) FogEnable=m_bFog;
                    PlayBuffer(SOUND_CLICK01);
                    return false;
                }
            }
            if(CheckMouseIn(HX(205.f),HY(445.f),HS(250.f),HS(34.f)))
            {
                m_bProLanguageOpen=!m_bProLanguageOpen;
                m_bProResolutionOpen=false;
                PlayBuffer(SOUND_CLICK01);
                return false;
            }
            if(CheckMouseIn(HX(205.f),HY(495.f),HS(250.f),HS(34.f)))
            {
                m_bProResolutionOpen=!m_bProResolutionOpen;
                m_bProLanguageOpen=false;
                PlayBuffer(SOUND_CLICK01);
                return false;
            }
            if(CheckMouseIn(HX(205.f),HY(545.f),HS(250.f),HS(34.f)))
            {
                CycleFpsLimit();
                gmProtect->ajust_fps_render=(BYTE)m_iLaf0FpsLimit;
                targetIndexFPS=(m_iLaf0FpsLimit==25)?0:((m_iLaf0FpsLimit==30)?1:((m_iLaf0FpsLimit==60)?2:3));
                PlayBuffer(SOUND_CLICK01);
                return false;
            }
        }
        else if(m_iProTab==1)
        {
            bool* v[10]={&m_RenderParticles,&m_RenderEffect,&m_bSkillEffects,&m_RenderGlow,&m_bRenderMonstersNPC,&m_bRenderCharacters,&m_RenderObjects,&m_RenderShadows,&m_bRenderWings,&m_bGlobalMessages};
            for(int i=0;i<5;++i)
            {
                const float yy=177.f+i*66.f;
                if(CheckMouseIn(HX(34.f),HY(yy-10.f),HS(205.f),HS(42.f))){*v[i]=!*v[i];PlayBuffer(SOUND_CLICK01);return false;}
            }
            for(int i=5;i<10;++i)
            {
                const float yy=177.f+(i-5)*66.f;
                if(CheckMouseIn(HX(258.f),HY(yy-10.f),HS(208.f),HS(42.f))){*v[i]=!*v[i];PlayBuffer(SOUND_CLICK01);return false;}
            }
        }
        else
        {
            bool* v[7]={&m_bShowPlayerHP,&m_bShowMonsterHP,&m_bMonsterHPName,&m_bMonsterHPLevel,&m_bRankInfo,&m_bGuildLogoName,&m_bReduceZenVisual};
            for(int i=0;i<4;++i)
            {
                const float yy=184.f+i*78.f;
                if(CheckMouseIn(HX(34.f),HY(yy-10.f),HS(205.f),HS(44.f))){*v[i]=!*v[i];PlayBuffer(SOUND_CLICK01);return false;}
            }
            for(int i=4;i<7;++i)
            {
                const float yy=184.f+(i-4)*78.f;
                if(CheckMouseIn(HX(258.f),HY(yy-10.f),HS(208.f),HS(44.f))){*v[i]=!*v[i];PlayBuffer(SOUND_CLICK01);return false;}
            }
            if(CheckMouseIn(HX(258.f),HY(408.f),HS(208.f),HS(44.f)))
            {
                gmProtect->m_RenderCharacterName=!gmProtect->m_RenderCharacterName;
                PlayBuffer(SOUND_CLICK01);
                return false;
            }
        }

        const float fx[3]={20.f,174.f,328.f}; const float fw=150.f;
        for(int i=0;i<3;++i)
        {
            if(CheckMouseIn(HX(fx[i]),HY(PRO_FOOTER_Y),HS(fw),HS(PRO_FOOTER_H)))
            {
                if(i==0){ApplyLaf0Settings();CaptureProSnapshot();}
                else if(i==1){SetLaf0Profile(1);}
                else {ApplyLaf0Settings();m_bProCloseHandled=true;g_pNewUISystem->Hide(SEASON3B::INTERFACE_OPTION);}
                PlayBuffer(SOUND_CLICK01);
                return false;
            }
        }
    }

    return !CheckMouseIn(x,y,(float)ProPanelWidth(),(float)ProPanelHeight());
#undef HS
#undef HY
#undef HX
}
void SEASON3B::CNewUIOptionWindow::RenderLaf0Contents()
{
	RenderProTabContents();
}

void SEASON3B::CNewUIOptionWindow::RenderProTabContents()
{
    const float ps=ProUIScale();
    const int x=m_Pos.x,y=m_Pos.y; char value[128];
	EnableAlphaTest();
	glColor4f(1.f,1.f,1.f,1.f);
#define RX(v) (x+(int)(ProContentX(v)*ps+0.5f))
#define RY(v) (y+(int)(ProContentY(v)*ps+0.5f))
#define RS(v) ((int)((v)*ps+0.5f))
#define TR(pt,en,es) ((m_iLanguage==1)?(en):((m_iLanguage==2)?(es):(pt)))
#define LABEL(xx,yy,txt,w) do{g_pRenderText->SetFont(g_hFont);g_pRenderText->SetTextColor(232,230,223,255);g_pRenderText->RenderText(RX(xx),RY(yy),txt,RS(w),0,RT3_SORT_LEFT);}while(0)
#define VALUE(xx,yy,txt,w) do{g_pRenderText->SetFont(g_hFontBold);g_pRenderText->SetTextColor(250,224,184,255);g_pRenderText->RenderText(RX(xx),RY(yy),txt,RS(w),0,RT3_SORT_CENTER);}while(0)
#define SWITCH(xx,yy,v) RenderImage((v)?IMAGE_CHECK_LIVE:IMAGE_UNCHECK_LIVE,(float)RX(xx),(float)RY(yy),(float)RS(28),(float)RS(28),0,0,0.75f,0.75f)
#define DROP(xx,yy,w) do{RenderTable((float)RX(xx),(float)RY(yy),(float)RS(w),(float)RS(34));g_pRenderText->SetTextColor(180,180,180,255);g_pRenderText->RenderText(RX((xx)+(w)-24),RY((yy)+10),"v",RS(16),0,RT3_SORT_CENTER);}while(0)
#define SECTION(xx,yy,txt,w) do{g_pRenderText->SetFont(g_hFontBold);g_pRenderText->SetTextColor(245,205,104,255);g_pRenderText->RenderText(RX(xx),RY(yy),txt,RS(150),0,RT3_SORT_LEFT);EnableAlphaTest();glColor4f(0.70f,0.50f,0.24f,0.80f);RenderColor((float)RX((xx)+105),(float)RY((yy)+8),(float)RS((w)-105),1.0f,0.0,false,true);EndRenderColor();glColor4f(1.f,1.f,1.f,1.f);}while(0)

    const char* pt[3]={"COMUM","ANTI-LAG","OPCOES"};
    const char* en[3]={"GENERAL","ANTI-LAG","OPTIONS"};
    const char* es[3]={"COMUN","ANTI-LAG","OPCIONES"};
    const char** tabs=(m_iLanguage==1)?en:((m_iLanguage==2)?es:pt);

    g_pRenderText->SetBgColor(0,0,0,0);g_pRenderText->SetFont(g_hFontBold);
    g_pRenderText->SetTextColor(235,229,215,255);
    g_pRenderText->RenderText(x+(int)(60*ps),y+(int)(28*ps),TR("Configuracoes avancadas","Advanced settings","Configuracion avanzada"),RS(760),0,RT3_SORT_CENTER);
    for(int i=0;i<3;++i) {
        g_pRenderText->SetFont(i==m_iProTab?g_hFontBold:g_hFont);
        g_pRenderText->SetTextColor(i==m_iProTab?245:180,i==m_iProTab?207:180,i==m_iProTab?105:180,255);
        g_pRenderText->RenderText(x+(int)(PRO_NAV_X*ps),y+(int)((PRO_NAV_Y+i*PRO_NAV_STEP+14)*ps),tabs[i],RS(PRO_NAV_W),0,RT3_SORT_CENTER);
    }
    g_pRenderText->SetFont(g_hFontBold);g_pRenderText->SetTextColor(128,125,116,255);
    g_pRenderText->RenderText(x+(int)(45*ps),y+(int)(352*ps),"NEON MU",RS(250),0,RT3_SORT_CENTER);
    g_pRenderText->SetFont(g_hFont);g_pRenderText->SetTextColor(176,171,160,255);
    const char* hint[3]={TR("Camera, filtros e resolucao","Camera, filters and resolution","Camara, filtros y resolucion"),TR("Efeitos e desempenho","Effects and performance","Efectos y rendimiento"),TR("Informacoes durante o jogo","Information during gameplay","Informacion durante el juego")};
    g_pRenderText->RenderText(x+(int)(32*ps),y+(int)(388*ps),hint[m_iProTab],RS(276),0,RT3_SORT_CENTER);
    g_pRenderText->RenderText(x+(int)(32*ps),y+(int)(575*ps),TR("Aplicar salva suas preferencias","Apply saves your preferences","Aplicar guarda tus preferencias"),RS(276),0,RT3_SORT_CENTER);

    if(m_iProTab==0)
    {
        static const char* lang[3]={"PORTUGUES (BR)","ENGLISH","ESPANOL"};
        SECTION(38,145,TR("EXIBICAO","DISPLAY","PANTALLA"),420);
        LABEL(50,177,TR("CAMERA 3D","3D CAMERA","CAMARA 3D"),340); SWITCH(430,169,m_bCamera3D);
        LABEL(50,214,TR("MINIMAPA","MINI-MAP","MINIMAPA"),340); SWITCH(430,206,m_bMiniMapEnabled);
        LABEL(50,251,TR("HORA DO SERVIDOR","SERVER TIME","HORA DEL SERVIDOR"),340); SWITCH(430,243,m_bServerClock);
        LABEL(50,288,TR("NEBLINA","FOG","NIEBLA"),340); SWITCH(430,280,m_bFog);

        SECTION(38,330,TR("FILTROS GRAFICOS","GRAPHIC FILTERS","FILTROS GRAFICOS"),420);
        LABEL(50,367,TR("FILTRO ANISOTROPICO","ANISOTROPIC FILTER","FILTRO ANISOTROPICO"),340); SWITCH(430,359,m_bAnisotropy);
        LABEL(50,404,TR("FILTRO LINEAR","LINEAR FILTER","FILTRO LINEAL"),340); SWITCH(430,396,m_bLinearFilter);

        g_pRenderText->SetFont(g_hFontBold); g_pRenderText->SetTextColor(245,205,104,255);
        g_pRenderText->RenderText(RX(45),RY(454),TR("IDIOMA","LANGUAGE","IDIOMA"),RS(145),0,RT3_SORT_LEFT);
        DROP(205,445,250); VALUE(211,456,lang[m_iLanguage],230);

        g_pRenderText->SetFont(g_hFontBold); g_pRenderText->SetTextColor(245,205,104,255);
        g_pRenderText->RenderText(RX(45),RY(504),TR("RESOLUCAO","RESOLUTION","RESOLUCION"),RS(145),0,RT3_SORT_LEFT);
        DROP(205,495,250); VALUE(211,506,resolutionList.as_string().c_str(),230);

        g_pRenderText->SetFont(g_hFontBold); g_pRenderText->SetTextColor(245,205,104,255);
        g_pRenderText->RenderText(RX(45),RY(554),"FPS",RS(145),0,RT3_SORT_LEFT);
        if(m_iLaf0FpsLimit==240) sprintf_s(value,"MAX FPS"); else sprintf_s(value,"%d FPS",m_iLaf0FpsLimit);
        DROP(205,545,250); VALUE(211,556,value,230);

        if(m_bProLanguageOpen)
        {
			EnableAlphaTest();
			RenderColor((float)RX(199),(float)RY(475),(float)RS(262),(float)RS(84),0.98f,1,true);
			EndRenderColor();
			glColor4f(1.f,1.f,1.f,1.f);
            for(int i=0;i<3;++i)
            {
                bool over=CheckMouseIn((float)RX(205),(float)RY(479+i*26),RS(250),RS(26));
				RenderBitmap(over?BITMAP_SETTINGSPRO_BUTTON_HOVER:BITMAP_SETTINGSPRO_BUTTON,(float)RX(205),(float)RY(479+i*26),(float)RS(250),(float)RS(26),0,0,140.0f/256.0f,40.0f/64.0f,true,true,0);
                VALUE(211,486+i*26,lang[i],230);
            }
        }
        if(m_bProResolutionOpen)
        {
            int vis=(int)m_ProResolutions.size();if(vis>3)vis=3;
            int first=resolutionList.GetCurrent()-1;if(first<0)first=0;int mf=(int)m_ProResolutions.size()-vis;if(first>mf)first=mf;if(first<0)first=0;
			EnableAlphaTest();
			RenderColor((float)RX(199),(float)RY(525),(float)RS(262),(float)RS(vis>0?(vis*24+6):6),0.98f,1,true);
			EndRenderColor();
			glColor4f(1.f,1.f,1.f,1.f);
            for(int i=0;i<vis;++i)
            {
                bool over=CheckMouseIn((float)RX(205),(float)RY(529+i*24),RS(250),RS(24));
				RenderBitmap(over?BITMAP_SETTINGSPRO_BUTTON_HOVER:BITMAP_SETTINGSPRO_BUTTON,(float)RX(205),(float)RY(529+i*24),(float)RS(250),(float)RS(24),0,0,140.0f/256.0f,40.0f/64.0f,true,true,0);
                VALUE(211,535+i*24,m_ProResolutions[first+i].c_str(),230);
            }
        }
    }
    else if(m_iProTab==1)
    {
        SECTION(38,145,TR("ANTI-LAG","ANTI-LAG","ANTI-LAG"),420);
        const char* n[10]={TR("EFEITOS ESTATICOS","STATIC EFFECTS","EFECTOS ESTATICOS"),TR("EFEITOS DINAMICOS","DYNAMIC EFFECTS","EFECTOS DINAMICOS"),TR("EFEITOS DE SKILLS","SKILL EFFECTS","EFECTOS DE SKILLS"),TR("GLOW","GLOW","GLOW"),TR("MOBS / NPCS","MOBS / NPCS","MOBS / NPCS"),TR("PERSONAGENS","CHARACTERS","PERSONAJES"),TR("OBJETOS DO MAPA","MAP OBJECTS","OBJETOS DEL MAPA"),TR("SOMBRAS","SHADOWS","SOMBRAS"),TR("ASAS","WINGS","ALAS"),TR("MENSAGENS GLOBAIS","GLOBAL MESSAGES","MENSAJES GLOBALES")};
        bool v[10]={m_RenderParticles,m_RenderEffect,m_bSkillEffects,m_RenderGlow,m_bRenderMonstersNPC,m_bRenderCharacters,m_RenderObjects,m_RenderShadows,m_bRenderWings,m_bGlobalMessages};
        for(int i=0;i<5;++i){const float yy=177.f+i*66.f; LABEL(42,yy,n[i],158); SWITCH(208,yy-8,v[i]);}
        for(int i=5;i<10;++i){const float yy=177.f+(i-5)*66.f; LABEL(266,yy,n[i],160); SWITCH(438,yy-8,v[i]);}
    }
    else
    {
        SECTION(38,145,TR("OPCOES DE JOGO","GAME OPTIONS","OPCIONES DE JUEGO"),420);
        const char* left[4]={TR("HP DOS JOGADORES","PLAYER HP","HP DE JUGADORES"),TR("HP DOS MONSTROS","MONSTER HP","HP DE MONSTRUOS"),TR("NOME DO MONSTRO","MONSTER NAME","NOMBRE DEL MONSTRUO"),TR("LEVEL DO MONSTRO","MONSTER LEVEL","NIVEL DEL MONSTRUO")};
        bool lv[4]={m_bShowPlayerHP,m_bShowMonsterHP,m_bMonsterHPName,m_bMonsterHPLevel};
        for(int i=0;i<4;++i){const float yy=184.f+i*78.f; LABEL(42,yy,left[i],158); SWITCH(208,yy-8,lv[i]);}
        const char* right[4]={TR("INFORMACOES DE RANK","RANK INFORMATION","INFORMACION DE RANK"),TR("LOGO DA GUILD","GUILD LOGO","LOGO DE GUILD"),TR("REDUZIR ZEN NO CHAO","REDUCE GROUND ZEN","REDUCIR ZEN EN SUELO"),TR("NOMES DOS PERSONAGENS","CHARACTER NAMES","NOMBRES DE PERSONAJES")};
        bool rv[4]={m_bRankInfo,m_bGuildLogoName,m_bReduceZenVisual,gmProtect->m_RenderCharacterName!=0};
        for(int i=0;i<4;++i){const float yy=184.f+i*78.f; LABEL(266,yy,right[i],160); SWITCH(438,yy-8,rv[i]);}
    }

    const float fx[3]={20.f,174.f,328.f}; const float fw=150.f;
    const char* ft[3]={TR("APLICAR","APPLY","APLICAR"),TR("PADRAO","DEFAULT","PREDETERMINADO"),TR("FECHAR","CLOSE","CERRAR")};
    for(int i=0;i<3;++i)
    {
        bool over=CheckMouseIn((float)RX(fx[i]),(float)RY(PRO_FOOTER_Y),RS(fw),RS(PRO_FOOTER_H));
		g_pRenderText->SetFont(g_hFontBold);
        g_pRenderText->SetTextColor(over?255:250,over?239:224,over?205:184,255);
		g_pRenderText->RenderText(RX(fx[i]),RY(PRO_FOOTER_Y+14),ft[i],RS(fw),0,RT3_SORT_CENTER);
    }

#undef SECTION
#undef DROP
#undef SWITCH
#undef VALUE
#undef LABEL
#undef TR
#undef RS
#undef RY
#undef RX
}
void SEASON3B::CNewUIOptionWindow::RenderLaf0Buttons()
{
	// Controles desenhados diretamente pela aba ativa.
}

void SEASON3B::CNewUIOptionWindow::SetLaf0Profile(int profile)
{
	m_iLaf0Profile = profile;

	if (profile == 0) // desempenho
	{
		m_RenderEffect = false;
		m_RenderShadows = false;
		m_RenderPets = false;
		m_RenderGlow = false;
		m_RenderEquipment = false;
		m_RenderTerrain = true;
		m_RenderObjects = false;
		m_RenderParticles = false;
		m_iRenderLevel = 0;
		m_iLaf0FpsLimit = 60;
		m_bLaf0VSync = false;
		m_iLaf0ViewDistance = 0;
		m_iLaf0TextureQuality = 0;
	}
	else if (profile == 2) // qualidade
	{
		m_RenderEffect = true;
		m_RenderShadows = true;
		m_RenderPets = true;
		m_RenderGlow = true;
		m_RenderEquipment = true;
		m_RenderTerrain = true;
		m_RenderObjects = true;
		m_RenderParticles = true;
		m_iRenderLevel = 4;
		m_iLaf0FpsLimit = 60;
		m_bLaf0VSync = true;
		m_iLaf0ViewDistance = 2;
		m_iLaf0TextureQuality = 2;
	}
	else // PADRAO
	{
		m_RenderEffect = true;
		m_RenderShadows = true;
		m_RenderPets = true;
		m_RenderGlow = true;
		m_RenderEquipment = true;
		m_RenderTerrain = true;
		m_RenderObjects = true;
		m_RenderParticles = true;
		m_iRenderLevel = 4;
		m_iLaf0FpsLimit = 60;
		m_bLaf0VSync = false;
		m_iLaf0ViewDistance = 1;
		m_iLaf0TextureQuality = 2;
		m_iScaleHUD = 100;
		m_iScaleChat = 100;
		m_iScaleMiniMap = 100;
		m_iScaleWindows = 100;
		m_iScaleButtons = 100;
		m_iScaleTexts = 100;
		m_bItemRGB = false;
		m_bItemRGBSelecting = false;
		m_iItemRGBTargetType = -1;
		m_iItemColorR = 255;
		m_iItemColorG = 255;
		m_iItemColorB = 255;
		m_iFontFamily = 0;
		m_bAutoAttack = true;
		m_bWhisperSound = false;
		m_bSlideHelp = true;
		m_bShowMonsterHP = true;
		m_bShowPlayerHP = true;
		m_bCamera3D = true;
		m_bMiniMapEnabled=true; m_bServerClock=true; m_bFog=true; m_bAnisotropy=true; m_bLinearFilter=true;
		m_bSkillEffects=true; m_bRenderMonstersNPC=true; m_bRenderCharacters=true; m_bRenderWings=true; m_bGlobalMessages=true;
		m_bMonsterHPName=true; m_bMonsterHPLevel=true; m_bRankInfo=true; m_bGuildLogoName=true; m_bReduceZenVisual=false; FogEnable=true;
		m_iVolumeLevel = 5;

		gwinhandle->SetFontSize(13);
		gwinhandle->SetFontFamily(FONT_FAMILIES[m_iFontFamily]);
		SetEffectVolumeLevel(m_iVolumeLevel);
	}

	// Profiles/default preview immediately; Apply/OK is still responsible for persistence.
	gmProtect->ajust_fps_render = (BYTE)m_iLaf0FpsLimit;
	targetIndexFPS = (m_iLaf0FpsLimit == 25) ? 0 : ((m_iLaf0FpsLimit == 30) ? 1 : ((m_iLaf0FpsLimit == 60) ? 2 : 3));
	g_bVSyncEnabled = m_bLaf0VSync ? TRUE : FALSE;
	if (IsVSyncAvailable())
	{
		if (m_bLaf0VSync) EnableVSync();
		else DisableVSync();
	}
}

void SEASON3B::CNewUIOptionWindow::ApplyLaf0Settings()
{
	int scaledFontSize=(13*m_iScaleTexts+50)/100;
	if(scaledFontSize<8) scaledFontSize=8;
	if(scaledFontSize>24) scaledFontSize=24;
	gwinhandle->SetFontSize(scaledFontSize);
	gwinhandle->SetFontFamily(FONT_FAMILIES[m_iFontFamily]);

	WritePrivateProfileStringA("AntiLag", "RenderEffect", m_RenderEffect ? "1" : "0", ".\\config.ini");
	WritePrivateProfileStringA("AntiLag", "RenderShadows", m_RenderShadows ? "1" : "0", ".\\config.ini");
	WritePrivateProfileStringA("AntiLag", "RenderPets", m_RenderPets ? "1" : "0", ".\\config.ini");
	WritePrivateProfileStringA("AntiLag", "RenderGlow", m_RenderGlow ? "1" : "0", ".\\config.ini");
	WritePrivateProfileStringA("AntiLag", "RenderEquipment", m_RenderEquipment ? "1" : "0", ".\\config.ini");
	WritePrivateProfileStringA("AntiLag", "RenderTerrain", m_RenderTerrain ? "1" : "0", ".\\config.ini");
	WritePrivateProfileStringA("AntiLag", "RenderObjects", m_RenderObjects ? "1" : "0", ".\\config.ini");
	WritePrivateProfileStringA("AntiLag", "RenderParticles", m_RenderParticles ? "1" : "0", ".\\config.ini");
	WritePrivateProfileStringA("Gameplay", "ShowMonsterHP", m_bShowMonsterHP ? "1" : "0", ".\\config.ini");
	WritePrivateProfileStringA("Gameplay", "ShowPlayerHP", m_bShowPlayerHP ? "1" : "0", ".\\config.ini");
	WritePrivateProfileStringA("Common","Camera3D",m_bCamera3D?"1":"0",".\\config.ini");
	if (CameraFactorPtr->IsEnable() != m_bCamera3D) CameraFactorPtr->Toggle();
	WritePrivateProfileStringA("Common","MiniMap",m_bMiniMapEnabled?"1":"0",".\\config.ini");
	WritePrivateProfileStringA("Common","ServerClock",m_bServerClock?"1":"0",".\\config.ini");
	WritePrivateProfileStringA("Common","Fog",m_bFog?"1":"0",".\\config.ini");
	WritePrivateProfileStringA("Common","Anisotropy",m_bAnisotropy?"1":"0",".\\config.ini");
	WritePrivateProfileStringA("Common","LinearFilter",m_bLinearFilter?"1":"0",".\\config.ini");
	WritePrivateProfileStringA("AntiLag","SkillEffects",m_bSkillEffects?"1":"0",".\\config.ini");
	WritePrivateProfileStringA("AntiLag","RenderMonstersNPC",m_bRenderMonstersNPC?"1":"0",".\\config.ini");
	WritePrivateProfileStringA("AntiLag","RenderCharacters",m_bRenderCharacters?"1":"0",".\\config.ini");
	WritePrivateProfileStringA("AntiLag","RenderWings",m_bRenderWings?"1":"0",".\\config.ini");
	WritePrivateProfileStringA("AntiLag","GlobalMessages",m_bGlobalMessages?"1":"0",".\\config.ini");
	WritePrivateProfileStringA("Options","MonsterHPName",m_bMonsterHPName?"1":"0",".\\config.ini");
	WritePrivateProfileStringA("Options","MonsterHPLevel",m_bMonsterHPLevel?"1":"0",".\\config.ini");
	WritePrivateProfileStringA("Options","RankInfo",m_bRankInfo?"1":"0",".\\config.ini");
	WritePrivateProfileStringA("Options","GuildLogoName",m_bGuildLogoName?"1":"0",".\\config.ini");
	WritePrivateProfileStringA("Options","ReduceZenVisual",m_bReduceZenVisual?"1":"0",".\\config.ini");
	FogEnable=m_bFog;
	WritePrivateProfileStringA("WorldVisual", "Weather", m_bWorldWeather ? "1" : "0", ".\\config.ini");
	WritePrivateProfileStringA("WorldVisual", "TimeOfDay", m_bWorldTimeOfDay ? "1" : "0", ".\\config.ini");
	RefreshServerWeatherVisuals();
	char uiScaleBuf[16];
	sprintf_s(uiScaleBuf,"%d",m_iScaleHUD); WritePrivateProfileStringA("Interface","ScaleHUD",uiScaleBuf,".\\config.ini");
	sprintf_s(uiScaleBuf,"%d",m_iScaleChat); WritePrivateProfileStringA("Interface","ScaleChat",uiScaleBuf,".\\config.ini");
	sprintf_s(uiScaleBuf,"%d",m_iScaleMiniMap); WritePrivateProfileStringA("Interface","ScaleMiniMap",uiScaleBuf,".\\config.ini");
	sprintf_s(uiScaleBuf,"%d",m_iScaleWindows); WritePrivateProfileStringA("Interface","ScaleWindows",uiScaleBuf,".\\config.ini");
	sprintf_s(uiScaleBuf,"%d",100); WritePrivateProfileStringA("Interface","ScaleButtons",uiScaleBuf,".\\config.ini");
	sprintf_s(uiScaleBuf,"%d",m_iScaleTexts); WritePrivateProfileStringA("Interface","ScaleTexts",uiScaleBuf,".\\config.ini");
	WritePrivateProfileStringA("ItemRGB", "Enabled", (m_bItemRGB && m_iItemRGBTargetType >= 0) ? "1" : "0", ".\\config.ini");
	sprintf_s(uiScaleBuf,"%d",m_iItemRGBTargetType); WritePrivateProfileStringA("ItemRGB","TargetType",uiScaleBuf,".\\config.ini");
	sprintf_s(uiScaleBuf,"%d",m_iItemColorR); WritePrivateProfileStringA("ItemRGB","R",uiScaleBuf,".\\config.ini");
	sprintf_s(uiScaleBuf,"%d",m_iItemColorG); WritePrivateProfileStringA("ItemRGB","G",uiScaleBuf,".\\config.ini");
	sprintf_s(uiScaleBuf,"%d",m_iItemColorB); WritePrivateProfileStringA("ItemRGB","B",uiScaleBuf,".\\config.ini");

	char value[16];
	sprintf_s(value, "%d", m_iLaf0FpsLimit);
	WritePrivateProfileStringA("Renderer", "FrameLimit", value, ".\\config.ini");
	WritePrivateProfileStringA("Renderer", "VSync", m_bLaf0VSync ? "1" : "0", ".\\config.ini");
	sprintf_s(value, "%d", m_iLaf0ViewDistance);
	WritePrivateProfileStringA("AntiLag", "ViewDistance", value, ".\\config.ini");
	sprintf_s(value, "%d", m_iLaf0TextureQuality);
	WritePrivateProfileStringA("Renderer", "TextureQuality", value, ".\\config.ini");
	sprintf_s(value, "%d", m_iRenderLevel);
	WritePrivateProfileStringA("Renderer", "RenderLevel", value, ".\\config.ini");

	gmProtect->ajust_fps_render = (BYTE)m_iLaf0FpsLimit;
	targetIndexFPS = (m_iLaf0FpsLimit == 25) ? 0 : ((m_iLaf0FpsLimit == 30) ? 1 : ((m_iLaf0FpsLimit == 60) ? 2 : 3));
	g_bVSyncEnabled = m_bLaf0VSync ? TRUE : FALSE;
	if (IsVSyncAvailable())
	{
		if (m_bLaf0VSync)
			EnableVSync();
		else
			DisableVSync();
	}

	WritePrivateProfileStringA("Font", "Family", FONT_FAMILIES[m_iFontFamily], ".\\config.ini");

	static const char* LANG_CODE[3] = { "Por", "Eng", "Spn" };
	g_strSelectedML = LANG_CODE[m_iLanguage];
	HKEY hLanguageKey;
	DWORD languageDisposition;
	if (RegCreateKeyExA(HKEY_CURRENT_USER, "SOFTWARE\\Webzen\\Mu2\\Config", 0, NULL, 0,
		KEY_WRITE, NULL, &hLanguageKey, &languageDisposition) == ERROR_SUCCESS)
	{
		const char* selectedLanguage = LANG_CODE[m_iLanguage];
		RegSetValueExA(hLanguageKey, "LangSelection", 0, REG_SZ,
			(const BYTE*)selectedLanguage, (DWORD)strlen(selectedLanguage) + 1);
		RegCloseKey(hLanguageKey);
	}

	leaf::CRegKey volumeKey;
	volumeKey.SetKey(leaf::CRegKey::_HKEY_CURRENT_USER, "SOFTWARE\\Webzen\\Mu2\\Config");
	volumeKey.WriteDword("VolumeLevel", (DWORD)m_iVolumeLevel);

	SaveOptions();
}

void SEASON3B::CNewUIOptionWindow::ReloadRuntimeLanguage()
{
	// The custom INI text bank can always be reloaded safely.
	gTextClien.Load();

	// Portuguese, English and Spanish share the same 1252 code page in this client,
	// but rebuilding CMultiLanguage also updates GetLanguage() immediately.
	if (pMultiLanguage != NULL)
	{
		delete pMultiLanguage;
		pMultiLanguage = NULL;
	}
	pMultiLanguage = new CMultiLanguage(g_strSelectedML);

	if (g_pNewUISystem != NULL && g_pNewUISystem->GetUI_NewItemTooltip() != NULL)
		g_pNewUISystem->GetUI_NewItemTooltip()->OpenFileTooltip();

    // Validate both text banks before replacing active UI labels.
    const std::string textFolder = "Data\\Local\\" + g_strSelectedML + "\\";
    CGlobalText coreTexts, extraTexts;
    if (coreTexts.Load(textFolder + "Text.bmd", CGlobalText::LD_USA_CANADA_TEXTS | CGlobalText::LD_FOREIGN_TEXTS) &&
        extraTexts.Load(textFolder + "GlobalText.bmd", CGlobalText::LD_USA_CANADA_TEXTS | CGlobalText::LD_FOREIGN_TEXTS))
    {
        GlobalText.Swap(coreTexts);
        TextGlobal.Swap(extraTexts);
    }

    // These catalogs were formerly refreshed only at client startup.
    // Run only on a language change, never inside the render/update loop.
    if (SceneFlag == MAIN_SCENE)
    {
        const std::string folder = "Data\\Local\\" + g_strSelectedML + "\\";
        ReloadLocalizedSkillNames((folder + "Skill.bmd").c_str());
        ReloadLocalizedMonsterNames((folder + "NpcName.txt").c_str());
        SEASON3B::CMoveCommandData::ReloadNames(folder + "Movereq.bmd");
        g_csQuest.ReloadQuestNames((folder + "Quest.bmd").c_str());
        g_QuestMng.ReloadLanguageWords();
        if (g_pNPCDialogue) g_pNPCDialogue->ReloadLanguage();
        g_SocketItemMgr.ReloadOptionNames((folder + "SocketItem.bmd").c_str());
        g_csItemOption.ReloadSetNames((folder + "ItemSetOption.bmd").c_str());
        if (g_BuffSystem) TheBuffInfo().ReloadLanguage();
        if (g_pUIJewelHarmonyinfo) g_pUIJewelHarmonyinfo->ReloadOptionNames((folder + "JewelOfHarmonyOption.bmd").c_str());
        if (g_pSlideHelpMgr) g_pSlideHelpMgr->ReloadLanguage((folder + "Slide.bmd").c_str());
        std::vector<BYTE> dialog;
        if (ReadLocalizedTable((folder + "Dialog.bmd").c_str(), dialog, MAX_DIALOG, sizeof(DIALOG_SCRIPT)))
            memcpy(g_DialogScript, dialog.data(), dialog.size());
        std::vector<BYTE> masterTooltip;
        if (g_pMasterSkillTree && ReadLocalizedTable((folder + "MasterSkillTooltip.bmd").c_str(),
            masterTooltip, MAX_MASTER_SKILL_DATA, sizeof(MASTER_SKILL_TOOLTIP), 0x2BC1))
            g_pMasterSkillTree->OpenMasterSkillTooltip((folder + "MasterSkillTooltip.bmd").c_str());
    }
}

bool SEASON3B::CNewUIOptionWindow::IsItemRGBAdmin() const
{
	if (Hero == NULL)
		return false;

	return ((Hero->CtlCode & CTLCODE_08OPERATOR) != 0 ||
		(Hero->CtlCode & CTLCODE_20OPERATOR) != 0);
}

bool SEASON3B::CNewUIOptionWindow::IsItemRGBEnabled() const
{
	return (m_bItemRGB && m_iItemRGBTargetType >= 0 && IsItemRGBAdmin());
}

bool SEASON3B::CNewUIOptionWindow::IsItemRGBForType(int itemType) const
{
	return (IsItemRGBEnabled() && itemType == m_iItemRGBTargetType);
}

bool SEASON3B::CNewUIOptionWindow::TrySelectItemRGBFromMouse()
{
	if (!IsItemRGBAdmin() || g_pMyInventory == NULL ||
		!g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_INVENTORY))
		return false;

	float scale = (float)m_iScaleWindows / 100.0f;
	if (scale < 0.50f) scale = 0.50f;
	if (scale > 1.50f) scale = 1.50f;

	float mouseX = MouseX;
	float mouseY = MouseY;
	if (scale != 1.0f)
	{
		mouseX = 320.0f + ((mouseX - 320.0f) / scale);
		mouseY = 240.0f + ((mouseY - 240.0f) / scale);
	}

	ITEM* pSelectedItem = NULL;
	int invenIndex = 0;
	pSelectedItem = g_pMyInventory->FindItemMousePt((int)mouseX, (int)mouseY, &invenIndex);

	if (pSelectedItem == NULL)
	{
		const int pointedIndex = g_pMyInventory->GetPointedItemIndex();
		if (pointedIndex >= 0 && pointedIndex < MAX_EQUIPMENT_INDEX)
		{
			ITEM* pEquipment = &CharacterMachine->Equipment[pointedIndex];
			if (pEquipment->Type >= 0)
				pSelectedItem = pEquipment;
		}
	}

	if (pSelectedItem == NULL || pSelectedItem->Type < 0)
		return false;

	m_iItemRGBTargetType = pSelectedItem->Type;
	m_bItemRGB = true;
	m_bItemRGBSelecting = false;
	PlayBuffer(SOUND_CLICK01);
	return true;
}

void SEASON3B::CNewUIOptionWindow::ChangeLanguage(int direction)
{
	static const char* LANG_CODE[3] = { "Por", "Eng", "Spn" };
	m_iLanguage += direction;
	if (m_iLanguage < 0) m_iLanguage = 2;
	if (m_iLanguage > 2) m_iLanguage = 0;

	g_strSelectedML = LANG_CODE[m_iLanguage];
	ReloadRuntimeLanguage();
	PlayBuffer(SOUND_CLICK01);
}

void SEASON3B::CNewUIOptionWindow::ChangeFontFamily(int direction)
{
	m_iFontFamily += direction;
	if (m_iFontFamily < 0)
		m_iFontFamily = FONT_FAMILY_COUNT - 1;
	else if (m_iFontFamily >= FONT_FAMILY_COUNT)
		m_iFontFamily = 0;

	gwinhandle->SetFontFamily(FONT_FAMILIES[m_iFontFamily]);
	PlayBuffer(SOUND_CLICK01);
}

bool SEASON3B::CNewUIOptionWindow::GetShowMonsterHP()
{
	return m_bShowMonsterHP;
}

bool SEASON3B::CNewUIOptionWindow::GetShowPlayerHP()
{
	return m_bShowPlayerHP;
}

void SEASON3B::CNewUIOptionWindow::RenderTable(float x, float y, float width, float height)
{
	EnableAlphaTest();

	glColor4f(0.0, 0.0, 0.0, 0.40000001);

	RenderColor((x + 3), (y + 2), (width - 7), (height - 7), 0.0, 0);

	EndRenderColor();

#if MAIN_UPDATE <= 603
	RenderImage(IMAGE_MAIN_TABLE_TOP_LEFT, x, y, 14.0, 14.0);

	RenderImage(IMAGE_MAIN_TABLE_TOP_RIGHT, (x + width - 14), y, 14.0, 14.0);

	RenderImage(IMAGE_MAIN_TABLE_BOTTOM_LEFT, x, (y + height - 14), 14.0, 14.0);

	RenderImage(IMAGE_MAIN_TABLE_BOTTOM_RIGHT, (x + width - 14), (y + height - 14), 14.0, 14.0);

	RenderImage(IMAGE_MAIN_TABLE_TOP_PIXEL, (x + 6), y, (width - 12), 14.0);

	RenderImage(IMAGE_MAIN_TABLE_RIGHT_PIXEL, (x + width - 14), (y + 6), 14.0, (height - 14));

	RenderImage(IMAGE_MAIN_TABLE_BOTTOM_PIXEL, (x + 6), (y + height - 14), (width - 12), 14.0);

	RenderImage(IMAGE_MAIN_TABLE_LEFT_PIXEL, x, (y + 6), 14.0, (height - 14));
#endif
}

void SEASON3B::CNewUIOptionWindow::SetAutoAttack(bool bAuto)
{
	m_bAutoAttack = bAuto;
}

bool SEASON3B::CNewUIOptionWindow::IsAutoAttack()
{
	return m_bAutoAttack;
}

void SEASON3B::CNewUIOptionWindow::SetWhisperSound(bool bSound)
{
	m_bWhisperSound = bSound;
}

bool SEASON3B::CNewUIOptionWindow::IsWhisperSound()
{
	return m_bWhisperSound;
}

void SEASON3B::CNewUIOptionWindow::SetSlideHelp(bool bHelp)
{
	m_bSlideHelp = bHelp;
}

bool SEASON3B::CNewUIOptionWindow::IsSlideHelp()
{
	return m_bSlideHelp;
}

void SEASON3B::CNewUIOptionWindow::SetVolumeLevel(int iVolume)
{
	m_iVolumeLevel = iVolume;
}

int SEASON3B::CNewUIOptionWindow::GetVolumeLevel()
{
	return m_iVolumeLevel;
}

void SEASON3B::CNewUIOptionWindow::SetRenderLevel(int iRender)
{
	m_iRenderLevel = iRender;
}

int SEASON3B::CNewUIOptionWindow::GetRenderLevel()
{
	return m_iRenderLevel;
}

void SEASON3B::CNewUIOptionWindow::SetRenderEffect(bool bHelp)
{
	m_RenderEffect = bHelp;
}

bool SEASON3B::CNewUIOptionWindow::GetRenderEffect()
{
	return m_RenderEffect;
}

void SEASON3B::CNewUIOptionWindow::SetRenderShadows(bool bHelp)
{
	m_RenderShadows = bHelp;
}

bool SEASON3B::CNewUIOptionWindow::GetRenderShadows()
{
	return m_RenderShadows;
}

void SEASON3B::CNewUIOptionWindow::SetRenderPets(bool bHelp)
{
	m_RenderPets = bHelp;
}

bool SEASON3B::CNewUIOptionWindow::GetRenderPets()
{
	return m_RenderPets;
}

void SEASON3B::CNewUIOptionWindow::SetRenderGlow(bool bHelp)
{
	m_RenderGlow = bHelp;
}

bool SEASON3B::CNewUIOptionWindow::GetRenderGlow()
{
	return m_RenderGlow;
}

void SEASON3B::CNewUIOptionWindow::SetRenderEquipment(bool bHelp)
{
	m_RenderEquipment = bHelp;
}

bool SEASON3B::CNewUIOptionWindow::GetRenderEquipment()
{
	return m_RenderEquipment;
}

void SEASON3B::CNewUIOptionWindow::SetRenderTerrain(bool bHelp)
{
	m_RenderTerrain = bHelp;
}

bool SEASON3B::CNewUIOptionWindow::GetRenderTerrain()
{
	return m_RenderTerrain;
}

void SEASON3B::CNewUIOptionWindow::SetRenderObjects(bool bHelp)
{
	m_RenderObjects = bHelp;
}

bool SEASON3B::CNewUIOptionWindow::GetRenderObjects()
{
	return m_RenderObjects;
}

void SEASON3B::CNewUIOptionWindow::SetRenderParticles(bool bEnable)
{
	m_RenderParticles = bEnable;
}

bool SEASON3B::CNewUIOptionWindow::GetRenderParticles()
{
	return m_RenderParticles;
}

int SEASON3B::CNewUIOptionWindow::GetViewDistanceLevel()
{
	return m_iLaf0ViewDistance;
}

float SEASON3B::CNewUIOptionWindow::GetViewDistanceScale()
{
	
	const float scales[3] = { 0.55f, 0.78f, 1.0f };
	return scales[m_iLaf0ViewDistance];
}

int SEASON3B::CNewUIOptionWindow::GetTextureQuality()
{
	return m_iLaf0TextureQuality;
}

void SEASON3B::CNewUIOptionWindow::change_resolution()
{
	int index = gwinhandle->GetDisplayIndex(resolutionList.as_string());

	if (m_Resolution != index)
	{
		leaf::CRegKey regkey;

		regkey.SetKey(leaf::CRegKey::_HKEY_CURRENT_USER, "SOFTWARE\\Webzen\\Mu2\\Config");

		if (regkey.WriteDword("Resolution", index))
		{
			m_Resolution = index;

			double backupWidth = gwinhandle->GetScreenX();

			double backupHight = gwinhandle->GetScreenY();

			gwinhandle->SetDisplayIndex(index, false);

			CameraFactorPtr->Init();

			OpenFont();

			ClearInput(TRUE);

			CInput& rInput = CInput::Instance();

			rInput.Create(gwinhandle->GethWnd(), WindowWidth, WindowHeight);

			g_pMoveCommandWindow->SetPos(1, 1);

			this->SetPos(0, 70);

			g_pNewUISystem->RenderFrameUpdate(backupWidth, backupHight);

			g_pNewUI3DRenderMng->Reload3DEffectObject(WindowWidth, WindowHeight);
		}
	}
}

void SEASON3B::CNewUIOptionWindow::change_fontsize()
{
	int fontsize = fonttextList.as_interget();

	if (fontsize != FontHeight)
	{
		gwinhandle->SetFontSize(fontsize);

		pugi::xml_document file;
		pugi::xml_parse_result res = file.load_file("Data\\Resolutions.xml");

		if (res.status == pugi::status_ok)
		{
			pugi::xml_node root = file.child("ResolutionsInfo");

			for (pugi::xml_node child = root.child("Resolution"); child; child = child.next_sibling())
			{
				std::string text_name = child.attribute("name").as_string();

				resolutionList.push_back(text_name);

				if (gwinhandle->GetDisplayIndex() == child.attribute("index").as_int())
				{
					child.attribute("font_size").set_value(fontsize);

					file.save_file("Data\\Resolutions.xml");
					return;
				}
			}
		}
	}
}

void SEASON3B::CNewUIOptionWindow::LoadResolution(const char* filename)
{
	m_ProResolutions.clear();
	pugi::xml_document file;
	pugi::xml_parse_result res = file.load_file(filename);

	if (res.status != pugi::status_ok)
	{
		return;
	}

	int fontSize = 13;
	int fontindex = 0;

	pugi::xml_node root = file.child("ResolutionsInfo");

	for (pugi::xml_node child_io = root.child("Resolution"); child_io; child_io = child_io.next_sibling())
	{
		std::string text_name = child_io.attribute("name").as_string();

		resolutionList.push_back(text_name);
		m_ProResolutions.push_back(text_name);

		if (gwinhandle->GetDisplayIndex() == child_io.attribute("index").as_int())
		{
			fontSize = child_io.attribute("font_size").as_int();
		}
	}

	root = file.child("FontSize");

	int i = 0;

	for (pugi::xml_node child_io = root.child("size"); child_io; child_io = child_io.next_sibling(), i++)
	{
		std::string text_name = child_io.attribute("fontsize").as_string();

		fonttextList.push_back(text_name);

		if (fontSize == child_io.attribute("fontsize").as_int())
		{
			fontindex = i;
		}
	}

	fonttextList.SetCurrent(fontindex);

	resolutionList.SetCurrent(gwinhandle->GetDisplayIndex());
}
