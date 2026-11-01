
#ifndef _NEWUIMANAGER_H_
#define _NEWUIMANAGER_H_

#pragma once

#pragma warning(disable : 4786)
#include <string>
#include <vector>
#include <map>
#include <algorithm>

#include "NewUIBase.h"

namespace SEASON3B
{
	enum { UI_PROFILE_MAX_VISIBLE_WINDOWS = 24 };

	struct SUIFrameProfileSnapshot
	{
		double TotalMs;
		double WindowMs;
		DWORD TopKeys[3];
		double TopMs[3];
		DWORD TopWindowKeys[3];
		double TopWindowMs[3];
		DWORD VisibleWindowKeys[UI_PROFILE_MAX_VISIBLE_WINDOWS];
		int VisibleWindowCount;
	};

	const char* GetNewUIInterfaceProfileName(DWORD dwKey);

	class CNewUIManager
	{
		typedef std::vector<CNewUIObj*> type_vector_uibase;
		typedef std::map<DWORD, CNewUIObj*> type_map_uibase;

		type_vector_uibase	m_vecUI;		//. for rendering and updating
		type_vector_uibase	m_vecKeyEventUI;	// cached key-dispatch order
		type_map_uibase		m_mapUI;		//. for managing

		CNewUIObj*	m_pActiveMouseUIObj, * m_pActiveKeyUIObj;
		CNewUIObj*	m_pRaisedWindowUIObj;
		bool		m_bLayerOrderDirty;
		bool		m_bKeyEventOrderDirty;
		SUIFrameProfileSnapshot m_FrameProfileSnapshot;
#ifdef PBG_MOD_STAMINA_UI
		int m_nShowUICnt;
#endif //PBG_MOD_STAMINA_UI
	public:
		CNewUIManager();
		~CNewUIManager();

		void AddUIObj(DWORD dwKey, CNewUIObj* pUIObj);
		void RemoveUIObj(DWORD dwKey);
		void RemoveUIObj(CNewUIObj* pUIObj);
		void RemoveAllUIObjs();

		void ReleaseAllUIObj();

		CNewUIObj* FindUIObj(DWORD dwKey);

		bool UpdateMouseEvent();
		bool UpdateKeyEvent();
		bool Update();
		bool Render();
		const SUIFrameProfileSnapshot& GetFrameProfileSnapshot() const;

		CNewUIObj* GetActiveMouseUIObj();
		CNewUIObj* GetActiveKeyUIObj();
		void ResetActiveUIObj();
		
		bool IsInterfaceVisible(DWORD dwKey);
		bool IsInterfaceEnabled(DWORD dwKey);

		void ShowInterface(DWORD dwKey, bool bShow = true);
		
		void EnableInterface(DWORD dwKey, bool bEnable = true);
		void ShowAllInterfaces(bool bShow = true);
		void EnableAllInterfaces(bool bEnable = true);

		void UpdateInterface(DWORD dwKey, double WindowsX, double WindowsY);
#ifdef PBG_MOD_STAMINA_UI
		int GetShowUICnt();
#endif //PBG_MOD_STAMINA_UI

	protected:
		void EnsureLayerOrder();
		void EnsureKeyEventOrder();
		static bool CompareLayerDepth(INewUIBase* pObj1,INewUIBase* pObj2);
		static bool CompareLayerDepthReverse(INewUIBase* pObj1,INewUIBase* pObj2);
		static bool CompareKeyEventOrder(INewUIBase* pObj1,INewUIBase* pObj2);
	};
}

#endif // _NEWUIMANAGER_H_
