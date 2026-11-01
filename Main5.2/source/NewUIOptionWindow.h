// NewUIOptionWindow.h: interface for the CNewUIOptionWindow class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_NEWUIOPTIONWINDOW_H__1469FA1D_7C15_4AFE_AD6E_59C303E72BC0__INCLUDED_)
#define AFX_NEWUIOPTIONWINDOW_H__1469FA1D_7C15_4AFE_AD6E_59C303E72BC0__INCLUDED_

#pragma once

#include "NewUIManager.h"
#include "NewUIMyInventory.h"
#include "NewUIQuestProgress.h"
#include "NewUIListGroupButton.h"

namespace SEASON3B
{
	
	class CNewUIOptionWindow  : public CNewUIObj 
	{
#ifdef KJH_ADD_INGAMESHOP_UI_SYSTEM
	public:
#endif // KJH_ADD_INGAMESHOP_UI_SYSTEM
		enum IMAGE_LIST
		{
			IMAGE_MAIN_TABLE_TOP_LEFT = CNewUIInventoryCtrl::IMAGE_ITEM_TABLE_TOP_LEFT,	//. newui_item_table01(L).tga (14,14)
			IMAGE_MAIN_TABLE_TOP_RIGHT = CNewUIInventoryCtrl::IMAGE_ITEM_TABLE_TOP_RIGHT,	//. newui_item_table01(R).tga (14,14)
			IMAGE_MAIN_TABLE_BOTTOM_LEFT = CNewUIInventoryCtrl::IMAGE_ITEM_TABLE_BOTTOM_LEFT,	//. newui_item_table02(L).tga (14,14)
			IMAGE_MAIN_TABLE_BOTTOM_RIGHT = CNewUIInventoryCtrl::IMAGE_ITEM_TABLE_BOTTOM_RIGHT,	//. newui_item_table02(R).tga (14,14)
			IMAGE_MAIN_TABLE_TOP_PIXEL = CNewUIInventoryCtrl::IMAGE_ITEM_TABLE_TOP_PIXEL,			//. newui_item_table03(up).tga (1, 14)
			IMAGE_MAIN_TABLE_BOTTOM_PIXEL = CNewUIInventoryCtrl::IMAGE_ITEM_TABLE_BOTTOM_PIXEL,	//. newui_item_table03(dw).tga (1,14)
			IMAGE_MAIN_TABLE_LEFT_PIXEL = CNewUIInventoryCtrl::IMAGE_ITEM_TABLE_LEFT_PIXEL,		//. newui_item_table03(L).tga (14,1)
			IMAGE_MAIN_TABLE_RIGHT_PIXEL = CNewUIInventoryCtrl::IMAGE_ITEM_TABLE_RIGHT_PIXEL,
			IMAGE_OPTION_FRAME_BACK = CNewUIMessageBoxMng::IMAGE_MSGBOX_BACK,
			IMAGE_OPTION_BTN_CLOSE = CNewUIMessageBoxMng::IMAGE_MSGBOX_BTN_CLOSE,
			IMAGE_OPTION_FRAME_DOWN = CNewUIMyInventory::IMAGE_INVENTORY_BACK_BOTTOM,

			IMAGE_OPTION_FRAME_UP = BITMAP_OPTION_BEGIN,
			IMAGE_OPTION_FRAME_LEFT,
			IMAGE_OPTION_FRAME_RIGHT,
			IMAGE_OPTION_LINE,
			IMAGE_OPTION_POINT,
			IMAGE_OPTION_BTN_CHECK,
			IMAGE_OPTION_EFFECT_BACK,
			IMAGE_OPTION_EFFECT_COLOR,
			IMAGE_OPTION_VOLUME_BACK,
			IMAGE_OPTION_VOLUME_COLOR,
			IMAGE_ND_BTN_L = CNewUIQuestProgress::IMAGE_QP_BTN_L,				// Quest_bt_L.tga	(17,36)
			IMAGE_ND_BTN_R = CNewUIQuestProgress::IMAGE_QP_BTN_R,
			IMAGE_CHECK_LIVE = BITMAP_IMAGE_FRAME_EMU + 10,
			IMAGE_UNCHECK_LIVE = BITMAP_IMAGE_FRAME_EMU + 11,
			IMAGE_FONT_ARROW_LEFT = BITMAP_IMAGE_FRAME_EMU + 12,
			IMAGE_FONT_ARROW_RIGHT = BITMAP_IMAGE_FRAME_EMU + 13,
		};

	public:
		CNewUIOptionWindow();
		virtual ~CNewUIOptionWindow();

		bool Create(CNewUIManager* pNewUIMng, int x, int y);
		void Release();
		
		void SetPos(int x, int y);
		
		bool UpdateMouseEvent();
		bool UpdateKeyEvent();
		bool Update();
		bool Render();

		float GetLayerDepth();	//. 10.5f
		float GetKeyEventOrder();	// 10.f;
		
		void OpenningProcess();
		void ClosingProcess();

		void SetGameOptions(BYTE GameOption);

		void SetAutoAttack(bool bAuto);
		bool IsAutoAttack();
		void SetWhisperSound(bool bSound);
		bool IsWhisperSound();
		void SetSlideHelp(bool bHelp);
		bool IsSlideHelp();
		void SetVolumeLevel(int iVolume);
		int GetVolumeLevel();
		void SetRenderLevel(int iRender);
		int GetRenderLevel();

		void SetRenderEffect(bool bHelp);
		bool GetRenderEffect();
		void SetRenderShadows(bool bHelp);
		bool GetRenderShadows();
		void SetRenderPets(bool bHelp);
		bool GetRenderPets();
		void SetRenderGlow(bool bHelp);
		bool GetRenderGlow();
		void SetRenderEquipment(bool bHelp);
		bool GetRenderEquipment();
		void SetRenderTerrain(bool bHelp);
		bool GetRenderTerrain();
		void SetRenderObjects(bool bHelp);
		bool GetRenderObjects();
		void SetRenderParticles(bool bEnable);
		bool GetRenderParticles();
		bool GetShowMonsterHP();
		bool GetShowPlayerHP();

		bool IsCamera3DEnabled() const { return m_bCamera3D; }
		bool IsMiniMapEnabled() const { return m_bMiniMapEnabled; }
		bool IsServerClockEnabled() const { return m_bServerClock; }
		bool IsFogEnabled() const { return m_bFog; }
		bool IsAnisotropyEnabled() const { return m_bAnisotropy; }
		bool IsLinearFilterEnabled() const { return m_bLinearFilter; }
		bool IsSkillEffectsEnabled() const { return m_bSkillEffects; }
		bool IsMonstersNPCEnabled() const { return m_bRenderMonstersNPC; }
		bool IsCharactersEnabled() const { return m_bRenderCharacters; }
		bool IsWingsEnabled() const { return m_bRenderWings; }
		bool IsGlobalMessagesEnabled() const { return m_bGlobalMessages; }
		bool IsMonsterHPNameEnabled() const { return m_bMonsterHPName; }
		bool IsMonsterHPLevelEnabled() const { return m_bMonsterHPLevel; }
		bool IsRankInfoEnabled() const { return m_bRankInfo; }
		bool IsGuildLogoNameEnabled() const { return m_bGuildLogoName; }
		bool IsReduceZenVisualEnabled() const { return m_bReduceZenVisual; }
		int GetScaleHUD() const { return m_iScaleHUD; }
		int GetScaleChat() const { return m_iScaleChat; }
		int GetScaleMiniMap() const { return m_iScaleMiniMap; }
		int GetScaleWindows() const { return m_iScaleWindows; }
		int GetScaleButtons() const { return 100; }
		int GetScaleTexts() const { return m_iScaleTexts; }
		bool IsItemRGBEnabled() const;
		bool IsItemRGBAdmin() const;
		bool IsItemRGBForType(int itemType) const;
		int GetItemRGBTargetType() const { return m_iItemRGBTargetType; }
		int GetItemColorR() const { return m_iItemColorR; }
		int GetItemColorG() const { return m_iItemColorG; }
		int GetItemColorB() const { return m_iItemColorB; }
		int GetViewDistanceLevel();
		float GetViewDistanceScale();
		int GetTextureQuality();
		void RenderTable(float x, float y, float width, float height);
	private:
		void LoadImages();
		void UnloadImages();
		void SetButtonInfo();
		void RenderFrame();
		void RenderContents();
		void RenderChecked(float x, float y, bool bEnable);
		void RenderButtons();
		void change_resolution();
		void change_fontsize();
		void ChangeFontFamily(int direction);
		void ChangeLanguage(int direction);
		void ReloadRuntimeLanguage();
		void LoadResolution(const char* filename);
		bool UpdateLaf0MouseEvent();
		void RenderLaf0Contents();
		void RenderLaf0Buttons();
		void SetLaf0Profile(int profile);
		void ApplyLaf0Settings();
		void RenderProTabContents();
		bool UpdateProTabMouseEvent();
		void SetProTab(int tab);
		void CycleFpsLimit();
		void SaveInterfaceScale();
		void CaptureProSnapshot();
		void RestoreProSnapshot();
		bool TrySelectItemRGBFromMouse();
	private:
		CNewUIManager*				m_pNewUIMng;
		POINT						m_Pos;
		CNewUIButton m_BtnClose;
		bool m_bAutoAttack;
		bool m_bWhisperSound;
		bool m_bSlideHelp;
		int m_iVolumeLevel;
		int m_iRenderLevel;
		bool m_RenderEffect;
		bool m_RenderShadows;
		bool m_RenderPets;
		bool m_RenderGlow;
		bool m_RenderEquipment;
		bool m_RenderTerrain;
		bool m_RenderObjects;
		bool m_RenderParticles;
		int m_iLaf0Profile;
		int m_iLaf0FpsLimit;
		bool m_bLaf0VSync;
		int m_iLaf0ViewDistance;
		int m_iLaf0TextureQuality;
		int m_iFontFamily;
		int m_iLanguage;
		bool m_bLaf0Dragging;
		int m_iLaf0DragOffsetX;
		int m_iLaf0DragOffsetY;
		int m_iProTab;
		int m_iInterfaceScale;
		int m_iProFontSize;
		int m_iScaleHUD;
		int m_iScaleChat;
		int m_iScaleMiniMap;
		int m_iScaleWindows;
		int m_iScaleButtons;
		int m_iScaleTexts;
		bool m_bItemRGB;
		bool m_bItemRGBSelecting;
		int m_iItemRGBTargetType;
		int m_iItemColorR;
		int m_iItemColorG;
		int m_iItemColorB;
		bool m_bShowMonsterHP;
		bool m_bShowPlayerHP;

		bool m_bCamera3D;
		bool m_bMiniMapEnabled;
		bool m_bServerClock;
		bool m_bFog;
		bool m_bAnisotropy;
		bool m_bLinearFilter;
		bool m_bSkillEffects;
		bool m_bRenderMonstersNPC;
		bool m_bRenderCharacters;
		bool m_bRenderWings;
		bool m_bGlobalMessages;
		bool m_bMonsterHPName;
		bool m_bMonsterHPLevel;
		bool m_bRankInfo;
		bool m_bGuildLogoName;
		bool m_bReduceZenVisual;
		bool m_bWorldWeather;
		bool m_bWorldTimeOfDay;
		bool m_bProResolutionOpen;
		bool m_bProLanguageOpen;
		bool m_bProFontOpen;
		int m_iProDraggingSlider;
		bool m_bProCloseHandled;
		int m_iSnapResolution;
		bool m_bSnapAutoAttack;
		bool m_bSnapWhisperSound;
		bool m_bSnapSlideHelp;
		int m_iSnapVolumeLevel;
		int m_iSnapRenderLevel;
		bool m_bSnapRenderEffect;
		bool m_bSnapRenderShadows;
		bool m_bSnapRenderPets;
		bool m_bSnapRenderGlow;
		bool m_bSnapRenderEquipment;
		bool m_bSnapRenderTerrain;
		bool m_bSnapRenderObjects;
		bool m_bSnapRenderParticles;
		int m_iSnapFpsLimit;
		bool m_bSnapVSync;
		int m_iSnapViewDistance;
		int m_iSnapTextureQuality;
		int m_iSnapFontFamily;
		int m_iSnapLanguage;
		int m_iSnapScaleHUD;
		int m_iSnapScaleChat;
		int m_iSnapScaleMiniMap;
		int m_iSnapScaleWindows;
		int m_iSnapScaleButtons;
		int m_iSnapScaleTexts;
		bool m_bSnapItemRGB;
		int m_iSnapItemRGBTargetType;
		int m_iSnapItemColorR;
		int m_iSnapItemColorG;
		int m_iSnapItemColorB;
		bool m_bSnapShowMonsterHP;
		bool m_bSnapShowPlayerHP;

		bool m_bSnapCamera3D;
		bool m_bSnapMiniMapEnabled;
		bool m_bSnapServerClock;
		bool m_bSnapFog;
		bool m_bSnapAnisotropy;
		bool m_bSnapLinearFilter;
		bool m_bSnapSkillEffects;
		bool m_bSnapRenderMonstersNPC;
		bool m_bSnapRenderCharacters;
		bool m_bSnapRenderWings;
		bool m_bSnapGlobalMessages;
		bool m_bSnapMonsterHPName;
		bool m_bSnapMonsterHPLevel;
		bool m_bSnapRankInfo;
		bool m_bSnapGuildLogoName;
		bool m_bSnapReduceZenVisual;
		bool m_bSnapWorldWeather;
		bool m_bSnapWorldTimeOfDay;
		std::vector<std::string> m_ProResolutions;
		CNewUIListGroupButton resolutionList;
		CNewUIListGroupButton fonttextList;
	};
}

#endif // !defined(AFX_NEWUIOPTIONWINDOW_H__1469FA1D_7C15_4AFE_AD6E_59C303E72BC0__INCLUDED_)
