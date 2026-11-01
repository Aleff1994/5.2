#include "stdafx.h"
#include "Protocol.h"
#include "WSclient.h"
#include "NewUISystem.h"
#include "wsclientinline.h"
#include "SkillManager.h"
#include "CharacterManager.h"
#if (CB_GETMIXRATE)
#include "CB_GetMixRate.h"
#endif
#include "CustomEventTime.h"
#include "CB_DangKyInGame.h"
#include "CDirection.h"
#include "NewUIStorageInventory.h"
#include "NewUIMiniMap.h"
#include "IndependentTierView.h"
#include "ExcellentEquippedView.h"

bool gBuildSetBonusEnabled = true;

BOOL ProtocolCoreEx(BYTE head, BYTE* lpMsg, int size, int key) // OK
{
 if(size>=4 && lpMsg[0]==0xC1 && (head==0xF1 || (head==0xF3 && (lpMsg[3]==0 || lpMsg[3]==3)))){IndependentTierView::Get().Reset();ExcellentEquippedView::Get().Reset();}
	switch (head)
	{
#if(CB_GETMIXRATE)
	case 0x88:
		if (gCB_GetMixRate) gCB_GetMixRate->GCRecvMixInfo(lpMsg, size);
		break;
#endif
	case 0xF3:
		switch (((lpMsg[0] == 0xC1) ? lpMsg[3] : lpMsg[4]))
		{
		case 0x6C:
			ExcellentEquippedView::Get().Receive(lpMsg,size);
			return 1;
		case 0x63:
			IndependentTierView::Get().Receive(lpMsg,size);
			return 1;
		case 0xF4:
		{
			PMSG_BOSS_SPAWN_CAMERA_RECV* config = (PMSG_BOSS_SPAWN_CAMERA_RECV*)lpMsg;
			const int fixedSize = sizeof(PSWMSG_HEAD) + (sizeof(int) * 4);
			if (size < fixedSize)
				return 1;

			const int packetEntries = (size - fixedSize) / sizeof(BOSS_SPAWN_CAMERA_NETWORK_INFO);
			int count = min(packetEntries,
				min(MAX_BOSS_SPAWN_CAMERA_NETWORK, max(0, config->Count)));

			g_Direction.BeginBossCameraConfig(config->Enabled != 0,
				config->ApproachMilliseconds, config->ReturnMilliseconds);

			for (int index = 0; index < count; ++index)
			{
				BOSS_SPAWN_CAMERA_NETWORK_INFO* boss = &config->Boss[index];
				g_Direction.AddBossCameraConfig(boss->Enabled, boss->MonsterIndex,
					boss->MapNumber, boss->DurationSeconds, boss->CameraLevel);
			}
			return 1;
		}
		case 0xF5:
		{
			if (size >= sizeof(PMSG_BOSS_SPAWN_CAMERA_START_RECV))
			{
				PMSG_BOSS_SPAWN_CAMERA_START_RECV* start =
					(PMSG_BOSS_SPAWN_CAMERA_START_RECV*)lpMsg;
				g_Direction.StartBossCameraFromServer(start->MonsterIndex,
					start->ObjectIndex, start->MapNumber, start->X, start->Y,
					start->DurationSeconds, start->CameraLevel);
			}
			return 1;
		}
		case 0xF6:
		{
			if (size >= sizeof(PMSG_WAREHOUSE_PAGE_INFO_RECV))
			{
				PMSG_WAREHOUSE_PAGE_INFO_RECV* info = (PMSG_WAREHOUSE_PAGE_INFO_RECV*)lpMsg;
				SEASON3B::UpdateStoragePageInfo(info->CurrentWarehouse, info->MaxWarehouse, info->RequestId);
			}
			return 1;
		}
		case 0xF7:
		{
			if (size >= sizeof(PMSG_BUILD_SET_BONUS_RECV))
			{
				PMSG_BUILD_SET_BONUS_RECV* info = (PMSG_BUILD_SET_BONUS_RECV*)lpMsg;
				gBuildSetBonusEnabled = (info->Enabled != 0);
			}
			return 1;
		}
		case 0xE8:
			g_CustomEventTime->GCReqEventTime((PMSG_CUSTOM_EVENTTIME_RECV*)lpMsg);
			return 1;
		}
		break;
	case 0xD3:
		switch (((lpMsg[0] == 0xC1) ? lpMsg[3] : lpMsg[4]))
		{
#if(CB_DANGKYINGAME)
		case 0x05:
			gCB_DangKyInGame->RecvKQRegInGame((XULY_CGPACKET*)lpMsg);
			break;
#endif
		}
		break;
	case 0xFA:
		if (((lpMsg[0] == 0xC1) ? lpMsg[3] : lpMsg[4]) == 0x02 && g_pNewUIMiniMap)
		{
			g_pNewUIMiniMap->ReceiveDynamicMapInfo(lpMsg, size);
			return 1;
		}
		break;

	}
	return false;
}



