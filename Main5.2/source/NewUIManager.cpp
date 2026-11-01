
#include "stdafx.h"
#include "NewUIManager.h"
#include "NewUISystem.h"
#include "NewUIOptionWindow.h"
#include "ZzzOpenglUtil.h"
#include "./Utilities/Log/ErrorReport.h"



using namespace SEASON3B;


namespace
{
	std::map<CNewUIObj*, DWORD> g_NeonInterfaceKeys;
	float NeonClampUIScale(int value)
	{
		if (value < 50) value = 50;
		if (value > 150) value = 150;
		return (float)value / 100.0f;
	}

	bool NeonIsHudObject(DWORD key)
	{
		switch (key)
		{
		case SEASON3B::INTERFACE_MAINFRAME:
		case SEASON3B::INTERFACE_SKILL_LIST:
		case SEASON3B::INTERFACE_ITEM_ENDURANCE_INFO:
		case SEASON3B::INTERFACE_BUFF_WINDOW:
		case SEASON3B::INTERFACE_HERO_POSITION_INFO:
		case SEASON3B::INTERFACE_MASTER_LEVEL:
		case SEASON3B::INTERFACE_HOTKEY:
		case SEASON3B::INTERFACE_PARTY_INFO_WINDOW:
		case SEASON3B::INTERFACE_BLOODCASTLE_TIME:
		case SEASON3B::INTERFACE_CHAOSCASTLE_TIME:
		case SEASON3B::INTERFACE_BATTLE_SOCCER_SCORE:
		case SEASON3B::INTERFACE_MACRO_PROGRESS:
		case SEASON3B::INTERFACE_EVENT_TIME:
			return true;
		default:
			return false;
		}
	}

	bool NeonIsWindowObject(DWORD key)
	{
		if (key >= SEASON3B::INTERFACE_3DRENDERING_CAMERA_BEGIN &&
			key <= SEASON3B::INTERFACE_3DRENDERING_CAMERA_END)
			return false;

		switch (key)
		{
		// HUD/chat/minimap/tooltips/settings are intentionally excluded.
		case SEASON3B::INTERFACE_BEGIN:
		case SEASON3B::INTERFACE_MAINFRAME:
		case SEASON3B::INTERFACE_SKILL_LIST:
		case SEASON3B::INTERFACE_ITEM_ENDURANCE_INFO:
		case SEASON3B::INTERFACE_BUFF_WINDOW:
		case SEASON3B::INTERFACE_HERO_POSITION_INFO:
		case SEASON3B::INTERFACE_MASTER_LEVEL:
		case SEASON3B::INTERFACE_HOTKEY:
		case SEASON3B::INTERFACE_CHATINPUTBOX:
		case SEASON3B::INTERFACE_CHATLOGWINDOW:
		case SEASON3B::INTERFACE_MINI_MAP:
		case SEASON3B::INTERFACE_OPTION:
		// Inventory/character windows enqueue their item models through separate
		// 3D camera UI objects. Raising only the parent frame would draw that frame
		// after its models and make equipped/bag items look hidden underneath it.
		case SEASON3B::INTERFACE_INVENTORY:
		case SEASON3B::INTERFACE_INVENTORY_EXTENSION:
		case SEASON3B::INTERFACE_INVENTORY_JEWEL:
		case SEASON3B::INTERFACE_INVENTORY_CORE:
		case SEASON3B::INTERFACE_CHARACTER:
		case SEASON3B::INTERFACE_TRADE:
		case SEASON3B::INTERFACE_STORAGE:
		case SEASON3B::INTERFACE_STORAGE_EXTENSION:
		case SEASON3B::INTERFACE_MIXINVENTORY:
		case SEASON3B::INTERFACE_MIX_EXPANSION:
		case SEASON3B::INTERFACE_NPCSHOP:
		case SEASON3B::INTERFACE_MYSHOP_INVENTORY:
		case SEASON3B::INTERFACE_PURCHASESHOP_INVENTORY:
		case SEASON3B::INTERFACE_LUCKYITEMWND:
		case SEASON3B::INTERFACE_INGAMESHOP:
		case SEASON3B::INTERFACE_MESSAGEBOX:
		case SEASON3B::INTERFACE_ITEM_EXPLANATION:
		case SEASON3B::INTERFACE_SETITEM_EXPLANATION:
		case SEASON3B::INTERFACE_ITEM_TOOLTIP:
		case SEASON3B::INTERFACE_BLOODCASTLE_TIME:
		case SEASON3B::INTERFACE_CHAOSCASTLE_TIME:
		case SEASON3B::INTERFACE_BATTLE_SOCCER_SCORE:
		case SEASON3B::INTERFACE_PARTY_INFO_WINDOW:
		case SEASON3B::INTERFACE_MACRO_PROGRESS:
			return false;
		default:
			return (key < SEASON3B::INTERFACE_END);
		}
	}

	bool NeonIsAlwaysOnTop(DWORD key)
	{
		switch (key)
		{
		case SEASON3B::INTERFACE_MESSAGEBOX:
		case SEASON3B::INTERFACE_ITEM_EXPLANATION:
		case SEASON3B::INTERFACE_SETITEM_EXPLANATION:
		case SEASON3B::INTERFACE_ITEM_TOOLTIP:
			return true;
		default:
			return false;
		}
	}

	bool NeonIsProfileWindow(DWORD key)
	{
		if (key >= SEASON3B::INTERFACE_3DRENDERING_CAMERA_BEGIN &&
			key <= SEASON3B::INTERFACE_3DRENDERING_CAMERA_END)
			return false;

		switch (key)
		{
		case SEASON3B::INTERFACE_BEGIN:
		case SEASON3B::INTERFACE_MAINFRAME:
		case SEASON3B::INTERFACE_SKILL_LIST:
		case SEASON3B::INTERFACE_ITEM_ENDURANCE_INFO:
		case SEASON3B::INTERFACE_BUFF_WINDOW:
		case SEASON3B::INTERFACE_HERO_POSITION_INFO:
		case SEASON3B::INTERFACE_HOTKEY:
		case SEASON3B::INTERFACE_PARTY_INFO_WINDOW:
		case SEASON3B::INTERFACE_BLOODCASTLE_TIME:
		case SEASON3B::INTERFACE_CHAOSCASTLE_TIME:
		case SEASON3B::INTERFACE_BATTLE_SOCCER_SCORE:
		case SEASON3B::INTERFACE_MACRO_PROGRESS:
		case SEASON3B::INTERFACE_EVENT_TIME:
		case SEASON3B::INTERFACE_CHATINPUTBOX:
		case SEASON3B::INTERFACE_CHATLOGWINDOW:
		case SEASON3B::INTERFACE_MINI_MAP:
		// These are permanent renderer layers, not user-opened windows.  Several
		// of them intentionally stay visible and render nothing until needed.
		case SEASON3B::INTERFACE_NAME_WINDOW:
		case SEASON3B::INTERFACE_SIEGEWARFARE:
		case SEASON3B::INTERFACE_SLIDEWINDOW:
		case SEASON3B::INTERFACE_CRYWOLF:
		case SEASON3B::INTERFACE_MESSAGEBOX:
		case SEASON3B::INTERFACE_ITEM_EXPLANATION:
		case SEASON3B::INTERFACE_SETITEM_EXPLANATION:
		case SEASON3B::INTERFACE_ITEM_TOOLTIP:
			return false;
		default:
			return key < SEASON3B::INTERFACE_END;
		}
	}

	void NeonInsertProfileTop(DWORD key, double elapsedMs,
		DWORD keys[3], double times[3])
	{
		for (int rank = 0; rank < 3; ++rank)
		{
			if (elapsedMs <= times[rank])
				continue;

			for (int move = 2; move > rank; --move)
			{
				keys[move] = keys[move - 1];
				times[move] = times[move - 1];
			}
			keys[rank] = key;
			times[rank] = elapsedMs;
			break;
		}
	}

	DWORD NeonFindInterfaceKey(const std::map<DWORD, CNewUIObj*>& table, CNewUIObj* obj)
	{
		(void)table;
		const std::map<CNewUIObj*, DWORD>::const_iterator it = g_NeonInterfaceKeys.find(obj);
		if (it != g_NeonInterfaceKeys.end()) return it->second;
		return 0xFFFFFFFF;
	}

	float NeonScaleForKey(DWORD key)
	{
		if (g_pOption == NULL)
			return 1.0f;

		if (key == SEASON3B::INTERFACE_CHATINPUTBOX ||
			key == SEASON3B::INTERFACE_CHATLOGWINDOW)
			return NeonClampUIScale(g_pOption->GetScaleChat());

		if (key == SEASON3B::INTERFACE_MINI_MAP)
			return NeonClampUIScale(g_pOption->GetScaleMiniMap());

		if (NeonIsHudObject(key))
			return NeonClampUIScale(g_pOption->GetScaleHUD());

		if (NeonIsWindowObject(key))
			return NeonClampUIScale(g_pOption->GetScaleWindows());

		return 1.0f;
	}

	void NeonPivotForKey(DWORD key, float& x, float& y)
	{
		if (NeonIsHudObject(key) || key == SEASON3B::INTERFACE_CHATINPUTBOX ||
			key == SEASON3B::INTERFACE_CHATLOGWINDOW)
		{
			// HUD stays anchored to the lower center of the virtual 640x480 UI.
			x = 320.0f;
			y = 480.0f;
		}
		else if (key == SEASON3B::INTERFACE_MINI_MAP)
		{
			// Mini-map stays anchored to the upper-right corner.
			x = 640.0f;
			y = 0.0f;
		}
		else
		{
			// Normal windows grow/shrink around the screen center.
			x = 320.0f;
			y = 240.0f;
		}
	}

	void NeonBeginRenderScale(DWORD key, float scale)
	{
		if (scale == 1.0f)
			return;

		float vx, vy;
		NeonPivotForKey(key, vx, vy);
		const float px = ConvertX(vx);
		const float py = (float)WindowHeight - ConvertY(vy);

		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glTranslatef(px, py, 0.0f);
		glScalef(scale, scale, 1.0f);
		glTranslatef(-px, -py, 0.0f);
	}

	void NeonEndRenderScale(float scale)
	{
		if (scale == 1.0f)
			return;

		glMatrixMode(GL_MODELVIEW);
		glPopMatrix();
	}

	void NeonBeginMouseScale(DWORD key, float scale,
		float& oldMouseX, float& oldMouseY, float& oldRenderX, float& oldRenderY)
	{
		oldMouseX = MouseX;
		oldMouseY = MouseY;
		oldRenderX = MouseRenderX;
		oldRenderY = MouseRenderY;

		if (scale == 1.0f)
			return;

		float vx, vy;
		NeonPivotForKey(key, vx, vy);

		MouseX = vx + ((MouseX - vx) / scale);
		MouseY = vy + ((MouseY - vy) / scale);

		const float px = ConvertX(vx);
		const float py = ConvertY(vy);
		MouseRenderX = px + ((MouseRenderX - px) / scale);
		MouseRenderY = py + ((MouseRenderY - py) / scale);
	}

	void NeonEndMouseScale(float oldMouseX, float oldMouseY, float oldRenderX, float oldRenderY)
	{
		MouseX = oldMouseX;
		MouseY = oldMouseY;
		MouseRenderX = oldRenderX;
		MouseRenderY = oldRenderY;
	}
}

const char* SEASON3B::GetNewUIInterfaceProfileName(DWORD key)
{
	if (key == 0xFFFFFFFF)
		return "None";
	if (key >= INTERFACE_3DRENDERING_CAMERA_BEGIN &&
		key <= INTERFACE_3DRENDERING_CAMERA_END)
		return "UI3D";

	switch (key)
	{
	case INTERFACE_FRIEND: return "Friend";
	case INTERFACE_MOVEMAP: return "MoveMap";
	case INTERFACE_PARTY: return "Party";
	case INTERFACE_MYQUEST: return "MyQuest";
	case INTERFACE_NPCQUEST: return "NpcQuest";
	case INTERFACE_GUILDINFO: return "Guild";
	case INTERFACE_TRADE: return "Trade";
	case INTERFACE_STORAGE: return "Storage";
	case INTERFACE_MIXINVENTORY: return "ChaosMix";
	case INTERFACE_COMMAND: return "Command";
	case INTERFACE_PET: return "Pet";
	case INTERFACE_NPCSHOP: return "NpcShop";
	case INTERFACE_INVENTORY: return "Inventory";
	case INTERFACE_MYSHOP_INVENTORY: return "PersonalShop";
	case INTERFACE_PURCHASESHOP_INVENTORY: return "PurchaseShop";
	case INTERFACE_CHARACTER: return "Character";
	case INTERFACE_NPCBREEDER: return "NpcBreeder";
	case INTERFACE_SERVERDIVISION: return "ServerDivision";
	case INTERFACE_DEVILSQUARE: return "DevilSquare";
	case INTERFACE_BLOODCASTLE: return "BloodCastle";
	case INTERFACE_NPCGUILDMASTER: return "GuildMasterNpc";
	case INTERFACE_GUARDSMAN: return "Guardsman";
	case INTERFACE_SENATUS: return "Senatus";
	case INTERFACE_GATEKEEPER: return "Gatekeeper";
	case INTERFACE_GATESWITCH: return "GateSwitch";
	case INTERFACE_CATAPULT: return "Catapult";
	case INTERFACE_REFINERY: return "Refinery";
	case INTERFACE_REFINERYINFO: return "RefineryInfo";
	case INTERFACE_KANTURU2ND_ENTERNPC: return "KanturuNpc";
	case INTERFACE_CURSEDTEMPLE_NPC: return "CursedTempleNpc";
	case INTERFACE_CURSEDTEMPLE_GAMESYSTEM: return "CursedTemple";
	case INTERFACE_CURSEDTEMPLE_RESULT: return "CursedTempleResult";
	case INTERFACE_CHATINPUTBOX: return "ChatInput";
	case INTERFACE_WINDOW_MENU: return "WindowMenu";
	case INTERFACE_OPTION: return "Options";
	case INTERFACE_HELP: return "Help";
	case INTERFACE_ITEM_EXPLANATION: return "ItemExplanation";
	case INTERFACE_SETITEM_EXPLANATION: return "SetExplanation";
	case INTERFACE_QUICK_COMMAND: return "QuickCommand";
	case INTERFACE_KANTURU_INFO: return "KanturuInfo";
	case INTERFACE_CHATLOGWINDOW: return "ChatLog";
	case INTERFACE_SLIDEWINDOW: return "SlideWindow";
	case INTERFACE_MESSAGEBOX: return "MessageBox";
	case INTERFACE_DUEL_WINDOW: return "Duel";
	case INTERFACE_CRYWOLF: return "Crywolf";
	case INTERFACE_NAME_WINDOW: return "NameWindow";
	case INTERFACE_SIEGEWARFARE: return "SiegeWarfare";
	case INTERFACE_MAINFRAME: return "MainFrame";
	case INTERFACE_SKILL_LIST: return "SkillList";
	case INTERFACE_ITEM_ENDURANCE_INFO: return "ItemEndurance";
	case INTERFACE_BUFF_WINDOW: return "BuffWindow";
	case INTERFACE_MASTER_LEVEL: return "MasterLevel";
	case INTERFACE_GOLD_BOWMAN: return "GoldBowman";
	case INTERFACE_GOLD_BOWMAN_LENA: return "Lena";
	case INTERFACE_LUCKYCOIN_REGISTRATION: return "LuckyCoinRegister";
	case INTERFACE_EXCHANGE_LUCKYCOIN: return "LuckyCoinExchange";
	case INTERFACE_DUELWATCH: return "DuelWatch";
	case INTERFACE_DUELWATCH_MAINFRAME: return "DuelWatchMain";
	case INTERFACE_DUELWATCH_USERLIST: return "DuelWatchUsers";
	case INTERFACE_INGAMESHOP: return "InGameShop";
	case INTERFACE_DOPPELGANGER_NPC: return "DoppelgangerNpc";
	case INTERFACE_DOPPELGANGER_FRAME: return "Doppelganger";
	case INTERFACE_QUEST_PROGRESS: return "QuestProgress";
	case INTERFACE_QUEST_PROGRESS_ETC: return "QuestProgressEtc";
	case INTERFACE_EMPIREGUARDIAN_NPC: return "EmpireGuardianNpc";
	case INTERFACE_EMPIREGUARDIAN_TIMER: return "EmpireGuardianTimer";
	case INTERFACE_MINI_MAP: return "MiniMap";
	case INTERFACE_NPC_DIALOGUE: return "NpcDialogue";
	case INTERFACE_GENSRANKING: return "GensRanking";
	case INTERFACE_UNITEDMARKETPLACE_NPC_JULIA: return "MarketJulia";
	case INTERFACE_LUCKYITEMWND: return "LuckyItem";
	case INTERFACE_INVENTORY_EXTENSION: return "InventoryExtension";
	case INTERFACE_STORAGE_EXTENSION: return "StorageExtension";
	case INTERFACE_MACRO_OFICIAL: return "MuHelper";
	case INTERFACE_MACRO_OFICIAL_SUB: return "MuHelperSub";
	case INTERFACE_MACRO_PROGRESS: return "MuHelperProgress";
	case INTERFACE_HOTKEY: return "HotKey";
	case INTERFACE_INVENTORY_JEWEL: return "JewelBank";
#ifdef EFFECT_MNG_HANDLE
	case INTERFACE_EFFECT_MANAGER: return "EffectManager";
#endif
	case INTERFACE_RECONNECT: return "Reconnect";
	case INTERFACE_CUSTOM_MENU: return "CustomMenu";
	case INTERFACE_SHOW_VIP: return "Vip";
	case INTERFACE_COMMAND_LIST: return "CommandList";
	case INTERFACE_EVENT_TIME: return "EventTime";
	case INTERFACE_RANKING_TOP: return "Ranking";
	case INTERFACE_INVENTORY_CORE: return "InventoryCore";
	case INTERFACE_CHAT_BLOCKED: return "ChatBlocked";
	case INTERFACE_MIX_EXPANSION: return "MixExpansion";
	case INTERFACE_ITEM_TOOLTIP: return "ItemTooltip";
	default: return "Unknown";
	}
}


SEASON3B::CNewUIManager::CNewUIManager() 
{
	m_pActiveMouseUIObj = NULL;
	m_pActiveKeyUIObj = NULL;
	m_pRaisedWindowUIObj = NULL;
	m_bLayerOrderDirty = true;
	m_bKeyEventOrderDirty = true;
	ZeroMemory(&m_FrameProfileSnapshot, sizeof(m_FrameProfileSnapshot));
#ifdef PBG_MOD_STAMINA_UI
	m_nShowUICnt =0;
#endif //PBG_MOD_STAMINA_UI
}

SEASON3B::CNewUIManager::~CNewUIManager() 
{ 
	RemoveAllUIObjs(); 
}

void SEASON3B::CNewUIManager::AddUIObj(DWORD dwKey, CNewUIObj* pUIObj)
{
	type_map_uibase::iterator mi = m_mapUI.find(dwKey);
	if(mi == m_mapUI.end())
	{
		m_vecUI.push_back(pUIObj);
		m_mapUI.insert( type_map_uibase::value_type(dwKey, pUIObj) );
		g_NeonInterfaceKeys[pUIObj] = dwKey;
		m_bLayerOrderDirty = true;
		m_bKeyEventOrderDirty = true;
	}
}

void SEASON3B::CNewUIManager::RemoveUIObj(DWORD dwKey)
{
	type_map_uibase::iterator mi = m_mapUI.find(dwKey);
	if(mi != m_mapUI.end())
	{
		type_vector_uibase::iterator vi = std::find(m_vecUI.begin(), m_vecUI.end(), (*mi).second);
		if(vi != m_vecUI.end())
		{
			m_vecUI.erase(vi);
			m_bLayerOrderDirty = true;
			m_bKeyEventOrderDirty = true;
		}
		if (m_pRaisedWindowUIObj == (*mi).second)
			m_pRaisedWindowUIObj = NULL;
		g_NeonInterfaceKeys.erase((*mi).second);
		m_mapUI.erase(mi);
	}
}

void SEASON3B::CNewUIManager::RemoveUIObj(CNewUIObj* pUIObj)
{
	type_map_uibase::iterator mi = m_mapUI.begin();
	for(; mi != m_mapUI.end(); mi++)
	{
		if((*mi).second == pUIObj)
		{
			g_NeonInterfaceKeys.erase(pUIObj);
			m_mapUI.erase(mi);
			break;
		}
	}
		
	type_vector_uibase::iterator vi = std::find(m_vecUI.begin(), m_vecUI.end(), pUIObj);
	if(vi != m_vecUI.end())
	{
		m_vecUI.erase(vi);
		m_bLayerOrderDirty = true;
		m_bKeyEventOrderDirty = true;
	}
	if (m_pRaisedWindowUIObj == pUIObj)
		m_pRaisedWindowUIObj = NULL;
}

void SEASON3B::CNewUIManager::RemoveAllUIObjs()
{
#if defined(_DEBUG)
	
	{
		unsigned int uiUIManageCNT = m_mapUI.size();
		
		type_map_uibase::iterator mi = m_mapUI.begin();
		for( ; mi != m_mapUI.end(); ++mi )
		{
			DWORD dwKey = (*mi).first;
			CNewUIObj* pUIObj = (*mi).second;
			if( pUIObj != NULL )
			{
				__TraceF(TEXT("UIKEY(%d) : mapUI \n"), uiUIManageCNT, dwKey);
			}
		}
		
		type_vector_uibase::iterator vi = m_vecUI.begin();
		for( ; vi < m_vecUI.end(); ++vi )
		{
			CNewUIObj* pUIObj = (*vi);
			if( pUIObj != NULL )
			{
				__TraceF(TEXT("vecUI \n"), uiUIManageCNT);
			}
		}
	}

#endif // defined(_DEBUG)
	m_vecUI.clear();
	m_vecKeyEventUI.clear();
	m_mapUI.clear();
	g_NeonInterfaceKeys.clear();
	m_pRaisedWindowUIObj = NULL;
	m_bLayerOrderDirty = true;
	m_bKeyEventOrderDirty = true;
}

void SEASON3B::CNewUIManager::EnsureLayerOrder()
{
	if (!m_bLayerOrderDirty)
		return;

	// Layer depths do not change while an object is registered.  Sorting only
	// when the collection changes avoids repeating the same work every frame.
	std::stable_sort(m_vecUI.begin(), m_vecUI.end(), CompareLayerDepth);
	m_bLayerOrderDirty = false;
}

void SEASON3B::CNewUIManager::EnsureKeyEventOrder()
{
	if (!m_bKeyEventOrderDirty)
		return;

	// Key-event priority changes only when UI objects are registered or removed.
	// Keep a separate cached order so key dispatch no longer reorders the render
	// vector and forces two full sorts on every frame.
	m_vecKeyEventUI = m_vecUI;
	std::stable_sort(m_vecKeyEventUI.begin(), m_vecKeyEventUI.end(), CompareKeyEventOrder);
	m_bKeyEventOrderDirty = false;
}

CNewUIObj* SEASON3B::CNewUIManager::FindUIObj(DWORD dwKey)
{
	type_map_uibase::iterator mi = m_mapUI.find(dwKey);
	if(mi != m_mapUI.end())
		return (*mi).second;
	return NULL;
}

bool SEASON3B::CNewUIManager::UpdateMouseEvent()
{
	m_pActiveMouseUIObj = NULL;
	EnsureLayerOrder();

	// Give the last clicked movable window first chance to keep an active drag.
	// Without this, a window below it can steal the mouse in an overlap area.
	if (m_pRaisedWindowUIObj != NULL && m_pRaisedWindowUIObj->IsVisible())
	{
		const DWORD neonKey = NeonFindInterfaceKey(m_mapUI, m_pRaisedWindowUIObj);
		const float neonScale = NeonScaleForKey(neonKey);
		float oldMouseX, oldMouseY, oldRenderX, oldRenderY;
		NeonBeginMouseScale(neonKey, neonScale, oldMouseX, oldMouseY, oldRenderX, oldRenderY);
		const bool result = m_pRaisedWindowUIObj->UpdateMouseEvent();
		NeonEndMouseScale(oldMouseX, oldMouseY, oldRenderX, oldRenderY);
		if (!result)
		{
			m_pActiveMouseUIObj = m_pRaisedWindowUIObj;
			return false;
		}
	}

	// m_vecUI is kept in ascending layer order; walking it backwards replaces
	// the old full sort that was performed on every mouse-update frame.
	for (int index = (int)m_vecUI.size() - 1; index >= 0; --index)
	{
		CNewUIObj* obj_backup = m_vecUI[index];
		if (obj_backup == m_pRaisedWindowUIObj)
			continue;

		if(obj_backup->IsVisible()) 
		{
			const DWORD neonKey = NeonFindInterfaceKey(m_mapUI, obj_backup);
			const float neonScale = NeonScaleForKey(neonKey);
			float oldMouseX, oldMouseY, oldRenderX, oldRenderY;
			NeonBeginMouseScale(neonKey, neonScale, oldMouseX, oldMouseY, oldRenderX, oldRenderY);
			bool bResult = obj_backup->UpdateMouseEvent();
			NeonEndMouseScale(oldMouseX, oldMouseY, oldRenderX, oldRenderY);
			
			if (std::find(m_vecUI.begin(), m_vecUI.end(), obj_backup) == m_vecUI.end())
			{
				break;
			}

			if( bResult == false )
			{
				m_pActiveMouseUIObj = obj_backup;
				if ((MouseLButton || MouseLButtonPush) && NeonIsWindowObject(neonKey))
					m_pRaisedWindowUIObj = obj_backup;
				return false;
			}

		}
	}

	return true;
}

bool SEASON3B::CNewUIManager::UpdateKeyEvent()
{
	m_pActiveKeyUIObj = NULL;
	EnsureKeyEventOrder();
	
	const HWND hWnd = GetFocus();
	type_vector_uibase::iterator vi = m_vecKeyEventUI.begin();
	for(; vi != m_vecKeyEventUI.end(); vi++)
	{
		HWND hRelatedWnd = (*vi)->GetRelatedWnd();
		if(NULL == hRelatedWnd)
		{
			hRelatedWnd = gwinhandle->GethWnd();
		}

		if((*vi)->IsEnabled() && hWnd == hRelatedWnd)
		{
			if(false == (*vi)->UpdateKeyEvent())
			{
				m_pActiveKeyUIObj = (*vi);
				return false;		//. stop calling UpdateKeyEvent functions
			}
		}
	}
	return true;
}

bool SEASON3B::CNewUIManager::Update()
{
	EnsureLayerOrder();
	
	type_vector_uibase::iterator vi = m_vecUI.begin();
	for(; vi != m_vecUI.end(); vi++)
	{
		if((*vi)->IsEnabled()) 
		{
			if(false == (*vi)->Update())
			{
				return false;		//. stop calling Update functions
			}
		}
	}

	return true;
}

bool SEASON3B::CNewUIManager::Render()
{
	EnsureLayerOrder();
	
	type_vector_uibase::iterator vi = m_vecUI.begin();
	static const bool profileUI = GetPrivateProfileIntA("Renderer", "Stats", 0, ".\\config.ini") != 0;
	const auto managerStart = profileUI ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point();
	double topMs[3] = { 0.0, 0.0, 0.0 };
	DWORD topKeys[3] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };
	double topWindowMs[3] = { 0.0, 0.0, 0.0 };
	DWORD topWindowKeys[3] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };
	DWORD visibleWindowKeys[UI_PROFILE_MAX_VISIBLE_WINDOWS] = {};
	int visibleWindowCount = 0;
	double windowMs = 0.0;
	int topIndex[3] = { -1, -1, -1 };
	int visibleIndex = 0;

	glAlphaFunc(GL_GREATER, 0.0000000);

	for(; vi != m_vecUI.end(); vi++)
	{
		if (*vi == m_pRaisedWindowUIObj && (*vi)->IsVisible())
			continue;
		const DWORD objectKey = NeonFindInterfaceKey(m_mapUI, *vi);
		if (m_pRaisedWindowUIObj != NULL && m_pRaisedWindowUIObj->IsVisible() &&
			NeonIsAlwaysOnTop(objectKey) && (*vi)->IsVisible())
			continue;

		if((*vi)->IsVisible()) 
		{
			const auto objectStart = profileUI ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point();
			const DWORD neonKey = objectKey;
			const float neonScale = NeonScaleForKey(neonKey);
			NeonBeginRenderScale(neonKey, neonScale);
			(*vi)->Render();
			NeonEndRenderScale(neonScale);
			if (profileUI)
			{
				const double elapsedMs = std::chrono::duration<double, std::milli>(
					std::chrono::steady_clock::now() - objectStart).count();
				for (int rank = 0; rank < 3; ++rank)
				{
					if (elapsedMs > topMs[rank])
					{
						for (int move = 2; move > rank; --move)
						{
							topKeys[move] = topKeys[move - 1];
							topMs[move] = topMs[move - 1];
							topIndex[move] = topIndex[move - 1];
						}
						topKeys[rank] = neonKey;
						topMs[rank] = elapsedMs;
						topIndex[rank] = visibleIndex;
						break;
					}
				}
				if (NeonIsProfileWindow(neonKey))
				{
					windowMs += elapsedMs;
					NeonInsertProfileTop(neonKey, elapsedMs, topWindowKeys, topWindowMs);
					if (visibleWindowCount < UI_PROFILE_MAX_VISIBLE_WINDOWS)
						visibleWindowKeys[visibleWindowCount++] = neonKey;
				}
			}
			++visibleIndex;
		}
	}

	// Draw the actively clicked/dragged window after the other windows so its
	// visual order matches its mouse-input order while windows overlap.
	if (m_pRaisedWindowUIObj != NULL && m_pRaisedWindowUIObj->IsVisible())
	{
		const auto objectStart = profileUI ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point();
		const DWORD neonKey = NeonFindInterfaceKey(m_mapUI, m_pRaisedWindowUIObj);
		const float neonScale = NeonScaleForKey(neonKey);
		NeonBeginRenderScale(neonKey, neonScale);
		m_pRaisedWindowUIObj->Render();
		NeonEndRenderScale(neonScale);
		if (profileUI)
		{
			const double elapsedMs = std::chrono::duration<double, std::milli>(
				std::chrono::steady_clock::now() - objectStart).count();
			for (int rank = 0; rank < 3; ++rank)
			{
				if (elapsedMs > topMs[rank])
				{
					for (int move = 2; move > rank; --move)
					{
						topKeys[move] = topKeys[move - 1];
						topMs[move] = topMs[move - 1];
						topIndex[move] = topIndex[move - 1];
					}
					topKeys[rank] = neonKey;
					topMs[rank] = elapsedMs;
					topIndex[rank] = visibleIndex;
					break;
				}
			}
			if (NeonIsProfileWindow(neonKey))
			{
				windowMs += elapsedMs;
				NeonInsertProfileTop(neonKey, elapsedMs, topWindowKeys, topWindowMs);
				if (visibleWindowCount < UI_PROFILE_MAX_VISIBLE_WINDOWS)
					visibleWindowKeys[visibleWindowCount++] = neonKey;
			}
		}
		++visibleIndex;
	}

	// Modal boxes and item tooltips must remain above the raised normal window.
	if (m_pRaisedWindowUIObj != NULL && m_pRaisedWindowUIObj->IsVisible())
	{
		for (type_vector_uibase::iterator top = m_vecUI.begin(); top != m_vecUI.end(); ++top)
		{
			const DWORD topKey = NeonFindInterfaceKey(m_mapUI, *top);
			if (!(*top)->IsVisible() || !NeonIsAlwaysOnTop(topKey))
				continue;
			const float topScale = NeonScaleForKey(topKey);
			NeonBeginRenderScale(topKey, topScale);
			(*top)->Render();
			NeonEndRenderScale(topScale);
			++visibleIndex;
		}
	}

	glAlphaFunc(GL_GREATER, 0.2500000);

	if (profileUI)
	{
		const double totalMs = std::chrono::duration<double, std::milli>(
			std::chrono::steady_clock::now() - managerStart).count();
		m_FrameProfileSnapshot.TotalMs = totalMs;
		m_FrameProfileSnapshot.WindowMs = windowMs;
		m_FrameProfileSnapshot.VisibleWindowCount = visibleWindowCount;
		for (int rank = 0; rank < 3; ++rank)
		{
			m_FrameProfileSnapshot.TopKeys[rank] = topKeys[rank];
			m_FrameProfileSnapshot.TopMs[rank] = topMs[rank];
			m_FrameProfileSnapshot.TopWindowKeys[rank] = topWindowKeys[rank];
			m_FrameProfileSnapshot.TopWindowMs[rank] = topWindowMs[rank];
		}
		for (int window = 0; window < UI_PROFILE_MAX_VISIBLE_WINDOWS; ++window)
			m_FrameProfileSnapshot.VisibleWindowKeys[window] =
				(window < visibleWindowCount) ? visibleWindowKeys[window] : 0xFFFFFFFF;
		static DWORD nextDetailLog = 0;
		const DWORD detailNow = GetTickCount();
		if (totalMs >= 8.0 && (nextDetailLog == 0 || static_cast<LONG>(detailNow - nextDetailLog) >= 0))
		{
			nextDetailLog = detailNow + 250;
			HANDLE file = CreateFileA(".\\RendererUIDetail.log", FILE_APPEND_DATA,
				FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			if (file != INVALID_HANDLE_VALUE)
			{
				char line[512];
				const int length = _snprintf_s(line, sizeof(line), _TRUNCATE,
					"Time=%lu TotalMs=%.2f Visible=%d Top1Key=%lu Top1Index=%d Top1Ms=%.2f Top2Key=%lu Top2Index=%d Top2Ms=%.2f Top3Key=%lu Top3Index=%d Top3Ms=%.2f\r\n",
					detailNow, totalMs, visibleIndex,
					topKeys[0], topIndex[0], topMs[0], topKeys[1], topIndex[1], topMs[1],
					topKeys[2], topIndex[2], topMs[2]);
				DWORD written = 0;
				WriteFile(file, line, length, &written, NULL);
				CloseHandle(file);
			}
		}
	}
	else
	{
		ZeroMemory(&m_FrameProfileSnapshot, sizeof(m_FrameProfileSnapshot));
	}

	return true;
}

const SEASON3B::SUIFrameProfileSnapshot& SEASON3B::CNewUIManager::GetFrameProfileSnapshot() const
{
	return m_FrameProfileSnapshot;
}

CNewUIObj* SEASON3B::CNewUIManager::GetActiveMouseUIObj()
{ 
	return m_pActiveMouseUIObj; 
}

CNewUIObj* SEASON3B::CNewUIManager::GetActiveKeyUIObj()
{ 
	return m_pActiveKeyUIObj; 
}

void SEASON3B::CNewUIManager::ResetActiveUIObj()
{
	m_pActiveMouseUIObj = NULL;
	m_pActiveKeyUIObj = NULL;
}

bool SEASON3B::CNewUIManager::IsInterfaceVisible(DWORD dwKey)
{
	CNewUIObj* pObj = FindUIObj(dwKey);
	if(NULL == pObj)
	{
		return false;
	}
	return pObj->IsVisible();
}

bool SEASON3B::CNewUIManager::IsInterfaceEnabled(DWORD dwKey)
{
	CNewUIObj* pObj = FindUIObj(dwKey);
	if(NULL == pObj)
		return false;
	return pObj->IsEnabled();
}

void SEASON3B::CNewUIManager::ShowInterface(DWORD dwKey, bool bShow/* = true*/)
{
	CNewUIObj* pObj = FindUIObj(dwKey);
	if(NULL != pObj)
		pObj->Show(bShow);
}

void SEASON3B::CNewUIManager::EnableInterface(DWORD dwKey, bool bEnable/* = true*/)
{
	CNewUIObj* pObj = FindUIObj(dwKey);
	if(NULL != pObj)
		pObj->Enable(bEnable);
}

void SEASON3B::CNewUIManager::ShowAllInterfaces(bool bShow/* = true*/)
{
	type_map_uibase::iterator mi = m_mapUI.begin();
	for(; mi != m_mapUI.end(); mi++)
		(*mi).second->Show(bShow);
}

void SEASON3B::CNewUIManager::EnableAllInterfaces(bool bEnable/* = true*/)
{
	type_map_uibase::iterator mi = m_mapUI.begin();
	for(; mi != m_mapUI.end(); mi++)
		(*mi).second->Show(bEnable);
}

void SEASON3B::CNewUIManager::UpdateInterface(DWORD dwKey, double WindowsX, double WindowsY)
{
	CNewUIObj* pObj = FindUIObj(dwKey);
	if (NULL != pObj)
		pObj->EventOrderWindows(WindowsX, WindowsY);
}

namespace
{
    float NeonInventoryLayerDepth(INewUIBase* object)
    {
        // A single numeric key keeps sorting transitive, including camera layers.
        // Inventory background precedes trade (2.1) and storage (2.2).
        for (std::map<CNewUIObj*, DWORD>::const_iterator it = g_NeonInterfaceKeys.begin();
             it != g_NeonInterfaceKeys.end(); ++it)
        {
            if (it->first == object && it->second == SEASON3B::INTERFACE_INVENTORY)
                return 2.0f;
        }
        return object->GetLayerDepth();
    }
}

bool SEASON3B::CNewUIManager::CompareLayerDepth(INewUIBase* pObj1, INewUIBase* pObj2)
{
    return NeonInventoryLayerDepth(pObj1) < NeonInventoryLayerDepth(pObj2);
}

bool SEASON3B::CNewUIManager::CompareLayerDepthReverse(INewUIBase* pObj1, INewUIBase* pObj2)
{
    return CompareLayerDepth(pObj2, pObj1);
}

bool SEASON3B::CNewUIManager::CompareKeyEventOrder(INewUIBase* pObj1,INewUIBase* pObj2)
{
	return pObj1->GetKeyEventOrder() > pObj2->GetKeyEventOrder();
}

#ifdef PBG_MOD_STAMINA_UI
int SEASON3B::CNewUIManager::GetShowUICnt()
{
	int m_nShowUICnt=0;
	// ÀÏºÎ Æ¯Á¤ ÀÎÅÍÆäÀÌ½º°¡ ¸î°³ ¿­·ÁÀÖ´ÂÁö
	for(int i=INTERFACE_PARTY; i<INTERFACE_CHARACTER+1; ++i)
	{
		if(IsInterfaceVisible(i))
			m_nShowUICnt++;
	}
	return m_nShowUICnt;
}
#endif //PBG_MOD_STAMINA_UI
