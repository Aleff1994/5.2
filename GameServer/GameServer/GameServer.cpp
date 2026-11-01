#include "stdafx.h"
#include "ChatTranslation.h"
#include "GearScoreRuntime.h"
#include "DSProtocol.h"
#include <stdio.h>  // FIX: Para manipulação do arquivo HardwareId.txt
#include <rpc.h>    // FIX: Para as funções de UUID
#pragma comment(lib, "Rpcrt4.lib") // FIX: Biblioteca necessária para o linkador

// Seus includes originais
#include "AIBot.h"
#include "Resource.h"
#include "BloodCastle.h"
#include "CastleDeep.h"
#include "CastleSiege.h"
#include "ChaosCastle.h"
#include "Crywolf.h"
#include "CustomArena.h"
#include "CustomEventDrop.h"
#include "CustomOnlineLottery.h"
#include "CustomQuiz.h"
#include "DevilSquare.h"
#include "EventGvG.h"
#include "EventTvT.h"
#include "FakeBot.h"
#include "GameServer.h"
#include "GameMain.h"
#include "IllusionTemple.h"
#include "InvasionManager.h"
#include "JSProtocol.h"
#include "Message.h"
#include "MiniDump.h"
#include "Notice.h"
#include "Protect.h"
#include "QueueTimer.h"
#include "ServerDisplayer.h"
#include "ServerInfo.h"
#include "SocketManager.h"
#include "SocketManagerUdp.h"
#include "ThemidaSDK.h"
#include "Util.h"
#include "ReiDoMU.h"
#include "EventFindPath.h"
#include "Path.h"
#include <algorithm> // Necessário para o std::remove_if
#include <string>    // Necessário para std::string
#include "CGMScriptLua.h"

HWND hWnd;
HINSTANCE hInst;
TCHAR szTitle[MAX_LOADSTRING];
TCHAR szWindowClass[MAX_LOADSTRING];

//=============================================================================
// FUNÇÃO DE LICENÇA DINÂMICA (Baseada no seu HardwareId.cpp)
//=============================================================================
bool ValidarLicencaDinamica()
{
	// 1. GERA O ID DA MÁQUINA (HARDWARE ID)
	DWORD VolumeSerialNumber;
	GetVolumeInformation("C:\\", 0, 0, &VolumeSerialNumber, 0, 0, 0, 0);

	UUID uuid;
	UuidCreateSequential(&uuid);

	SYSTEM_INFO SystemInfo;
	GetSystemInfo(&SystemInfo);

	DWORD Id1 = VolumeSerialNumber ^ 0x12B586FE;
	DWORD Id2 = *(DWORD*)(&uuid.Data4[2]) ^ 0x5D78A569;
	DWORD Id3 = ((*(WORD*)(&uuid.Data4[6]) & 0xFFFF) | (SystemInfo.wProcessorArchitecture << 16)) ^ 0xF45BC123;
	DWORD Id4 = ((SystemInfo.wProcessorLevel & 0xFFFF) | (SystemInfo.wProcessorRevision << 16)) ^ 0xB542D8E1;

	char MachineID[35] = { 0 };
	wsprintf(MachineID, "%08X-%08X-%08X-%08X", Id1, Id2, Id3, Id4);

	// 2. CAMINHO DO ARQUIVO
	char FullPath[MAX_PATH] = { 0 };
	GetModuleFileName(NULL, FullPath, MAX_PATH);
	char* pos = strrchr(FullPath, '\\');
	if (pos) *pos = '\0';
	strcat_s(FullPath, "\\Data\\GameServerInfo - Common.dat");

	// 3. LEITURA DOS CAMPOS
	char HardwareID_Arquivo[100] = { 0 };
	char Licenca_Arquivo[100] = { 0 };

	GetPrivateProfileString("GameServerInfo", "CustomerHardwareId", "", HardwareID_Arquivo, sizeof(HardwareID_Arquivo), FullPath);

	// LÊ A CHAVE "LicenseID" (O NOME QUE O CLIENTE VÊ)
	GetPrivateProfileString("GameServerInfo", "LicenseID", "", Licenca_Arquivo, sizeof(Licenca_Arquivo), FullPath);

	// 4. LIMPEZA DE ESPAÇOS
	std::string strHWID = HardwareID_Arquivo;
	strHWID.erase(std::remove_if(strHWID.begin(), strHWID.end(), isspace), strHWID.end());

	std::string strLic = Licenca_Arquivo;
	strLic.erase(std::remove_if(strLic.begin(), strLic.end(), isspace), strLic.end());

	// 5. VALIDAÇÃO INDIVIDUAL

	// TESTE 1: VALOR DA LICENÇA (Deve ser escrito EMULATOR no arquivo)
	if (_stricmp("EMULATOR", strLic.c_str()) != 0)
	{
		MessageBox(NULL, "NOME DE LICENCA INVALIDO!\nVERIFIQUE SUA LICENCA COM SUPORTE !", "ERRO: LICENSE ID", MB_OK | MB_ICONSTOP);
		ExitProcess(0);
		return false;
	}

	// TESTE 2: ID DO HARDWARE
	if (_stricmp(MachineID, strHWID.c_str()) != 0)
	{
		FILE* fp = fopen("HardwareId.txt", "w");
		if (fp) { fprintf(fp, "%s", MachineID); fclose(fp); }

		char msg[512];
		wsprintf(msg, "ID DE HARDWARE NAO RECONHECIDO!\nSEU ID: %s\n\nVERIFIQUE SUA LICENCA COM SUPORTE !", MachineID);
		MessageBox(NULL, msg, "ERRO: HARDWARE ID", MB_OK | MB_ICONSTOP);

		ExitProcess(0);
		return false;
	}

	return true;
}

void ThreadFreeVRAM(LPVOID lpThreadParameter)
{
	const DWORD sec_avg_max = 5000;
	HANDLE hThread = GetCurrentThread();
	SetThreadPriority(hThread, THREAD_PRIORITY_LOWEST);
	while (TRUE)
	{
		Sleep(sec_avg_max);
		SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
	}
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	VM_START;

	// FIX: Chama a validação logo no início para proteger as Files
	if (ValidarLicencaDinamica() == false)
	{
		return 0;
	}

	CMiniDump::Start();

	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_GAMESERVER, szWindowClass, MAX_LOADSTRING);

	MyRegisterClass(hInstance);

	if (InitInstance(hInstance, nCmdShow) == 0)
	{
		return 0;
	}

	HANDLE hThread = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)ThreadFreeVRAM, 0, 0, 0);

	SetLargeRand();

	gServerInfo.ReadStartupInfo("GameServerInfo", ".\\Data\\GameServerInfo - Common.dat");
    GearScoreRuntime::Load();
#if GAMESERVER_LICENSE == 1
#if(PROTECT_STATE==1)
#if(GAMESERVER_UPDATE>=801)
	gProtect.StartAuth(AUTH_SERVER_TYPE_S8_GAME_SERVER);
#elif(GAMESERVER_UPDATE>=601)
	gProtect.StartAuth(AUTH_SERVER_TYPE_S6_GAME_SERVER);
#elif(GAMESERVER_UPDATE>=401)
	gProtect.StartAuth(AUTH_SERVER_TYPE_S4_GAME_SERVER);
#else
	gProtect.StartAuth(AUTH_SERVER_TYPE_S2_GAME_SERVER);
#endif
#endif
#endif // GAMESERVER_LICENSE == 1

	char buff[256];

	wsprintf(buff, "[%s] %s (ON: %d) %s", GAMESERVER_VERSION, gServerInfo.m_ServerName, gObjTotalUser, GAMESERVER_CLIENT);

	SetWindowText(hWnd, buff);

	gServerDisplayer.Init(hWnd);

	WSADATA wsa;


	if (WSAStartup(MAKEWORD(2, 2), &wsa) == 0)
	{
		if (gSocketManager.Start((WORD)gServerInfo.m_ServerPort) == 0)
		{
			LogAdd(LOG_RED, "Could not start GameServer");
		}
		else
		{
			GameMainInit(hWnd);

			JoinServerConnect(WM_JOIN_SERVER_MSG_PROC);

			DataServerConnect(WM_DATA_SERVER_MSG_PROC);

			gSocketManagerUdp.Connect(gServerInfo.m_ConnectServerAddress, (WORD)gServerInfo.m_ConnectServerPort);

			SetTimer(hWnd, WM_TIMER_1000, 1000, 0);

			SetTimer(hWnd, WM_TIMER_10000, 10000, 0);

			gQueueTimer.CreateTimer(QUEUE_TIMER_MONSTER, 100, &QueueTimerCallback);

			gQueueTimer.CreateTimer(QUEUE_TIMER_MONSTER_MOVE, 100, &QueueTimerCallback);

			gQueueTimer.CreateTimer(QUEUE_TIMER_MONSTER_AI, 100, &QueueTimerCallback);

			gQueueTimer.CreateTimer(QUEUE_TIMER_MONSTER_AI_MOVE, 100, &QueueTimerCallback);

			gQueueTimer.CreateTimer(QUEUE_TIMER_EVENT, 100, &QueueTimerCallback);

			gQueueTimer.CreateTimer(QUEUE_TIMER_VIEWPORT, 1000, &QueueTimerCallback);

			gQueueTimer.CreateTimer(QUEUE_TIMER_FIRST, 1000, &QueueTimerCallback);

			gQueueTimer.CreateTimer(QUEUE_TIMER_CLOSE, 1000, &QueueTimerCallback);
#if(GAMESERVER_UPDATE>=701)
			gQueueTimer.CreateTimer(QUEUE_TIMER_MATH_AUTHENTICATOR, 10000, &QueueTimerCallback);
#endif
			gQueueTimer.CreateTimer(QUEUE_TIMER_ACCOUNT_LEVEL, 60000, &QueueTimerCallback);

			gQueueTimer.CreateTimer(QUEUE_TIMER_PICK_COMMAND, 6000, &QueueTimerCallback);

#if(PROTECT_STATE==0)
			gQueueTimer.CreateTimer(QUEUE_TIMER_AUTH_COMMAND, 600000,&QueueTimerCallback);
#endif
		}
	}
	else
	{
		LogAdd(LOG_RED, "WSAStartup() failed with error: %d", WSAGetLastError());
	}

	gServerDisplayer.PaintAllInfo();

	gServerDisplayer.PaintName();

	SetTimer(hWnd, WM_TIMER_2000, 2000, 0);

	HACCEL hAccelTable = LoadAccelerators(hInstance, (LPCTSTR)IDC_GAMESERVER);

	MSG msg;

	while (GetMessage(&msg, 0, 0, 0) != 0)
	{
		if (TranslateAccelerator(msg.hwnd, hAccelTable, &msg) == 0)
		{
			TranslateMessage(&msg);
			DispatchMessageA(&msg);
		}
	}

	if (hThread) CloseHandle(hThread);

	CMiniDump::Clean();

	VM_END

	return msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance) // OK
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = (WNDPROC)WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, (LPCTSTR)IDI_GAMESERVER);
	wcex.hCursor = LoadCursor(0, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = (LPCSTR)IDC_GAMESERVER;
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, (LPCTSTR)IDI_SMALL);

	return RegisterClassEx(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow) // OK
{
	hInst = hInstance;

	hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW | WS_THICKFRAME, CW_USEDEFAULT, 0, 980, 750, 0, 0, hInstance, 0);

	if (hWnd == 0)
	{
		return 0;
	}

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);
	return 1;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) // OK
{
	const char ClassName[] = "MainWindowClass";

	HWND hWndStatusBar;

	switch (message)
	{
	case WM_CREATE:
	{
		hWndStatusBar = CreateWindowEx(0, STATUSCLASSNAME, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | CCS_BOTTOM, 0, 0, 0, 0, hWnd, (HMENU)IDC_STATUSBAR, (HINSTANCE)GetWindowLong(hWnd, GWL_HINSTANCE), NULL);

		int iStatusWidths[] = { 190,270,360,450,580,-1 };

		char text[256];

		SendMessage(hWndStatusBar, SB_SETPARTS, 6, (LPARAM)iStatusWidths);

		wsprintf(text, "Gameserver %s - Update %d ", GAMESERVER_NAME, GAMESERVER_DISPLAY_UPDATE);

		SendMessage(hWndStatusBar, SB_SETTEXT, 0, (LPARAM)text);

		wsprintf(text, "OffStore: %d", gObjOffStore);

		SendMessage(hWndStatusBar, SB_SETTEXT, 1, (LPARAM)text);

		wsprintf(text, "OffAttack: %d", gObjOffAttack);

		SendMessage(hWndStatusBar, SB_SETTEXT, 2, (LPARAM)text);

		wsprintf(text, "Bots Buffer: %d", gObjTotalBot);

		SendMessage(hWndStatusBar, SB_SETTEXT, 3, (LPARAM)text);


		wsprintf(text, "Monsters: %d/%d", gObjTotalMonster, MAX_OBJECT_MONSTER);

		SendMessage(hWndStatusBar, SB_SETTEXT, 4, (LPARAM)text);

		SendMessage(hWndStatusBar, SB_SETTEXT, 5, (LPARAM)NULL);

		ShowWindow(hWndStatusBar, SW_SHOW);
	}
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDM_ONLINEUSERS:
			DialogBox(hInst, (LPCTSTR)IDD_ONLINEUSER, hWnd, (DLGPROC)UserOnline);
			break;
		case IDM_ABOUT:
			DialogBox(hInst, (LPCTSTR)IDD_ABOUTBOX, hWnd, (DLGPROC)About);
			break;
		case IDM_EXIT:
			if (MessageBox(0, "Are you sure to terminate GameServer?", "Ask terminate server", MB_YESNO | MB_ICONQUESTION) == IDYES)
			{
				DestroyWindow(hWnd);
			}
			break;
		case IDM_FILE_ALLUSERLOGOUT:
			gObjAllLogOut();
			break;
		case IDM_FILE_ALLUSERDISCONNECT:
			gObjAllDisconnect();
			break;
		case IDM_FILE_1MINUTESERVERCLOSE:
			if (gCloseMsg == 0)
			{
				gCloseMsg = 1;
				gCloseMsgTime = 60;
				gNotice.GCNoticeSendToAll(0, 0, 0, 0, 0, 0, gMessage.GlobalText(487));
			}
			break;
		case IDM_FILE_3MINUTESERVERCLOSE:
			if (gCloseMsg == 0)
			{
				gCloseMsg = 1;
				gCloseMsgTime = 180;
				gNotice.GCNoticeSendToAll(0, 0, 0, 0, 0, 0, gMessage.GlobalText(488));
			}
			break;
		case IDM_FILE_5MINUTESERVERCLOSE:
			if (gCloseMsg == 0)
			{
				gCloseMsg = 1;
				gCloseMsgTime = 300;
				gNotice.GCNoticeSendToAll(0, 0, 0, 0, 0, 0, gMessage.GlobalText(489));
			}
			break;
		case IDM_RELOAD_RELOADCASHSHOP:
			gServerInfo.ReadCashShopInfo();
			break;
		case IDM_RELOAD_RELOADCHAOSMIX:
			gServerInfo.ReadChaosMixInfo();
			break;
		case IDM_RELOAD_RELOADCHARACTER:
			gServerInfo.ReadCharacterInfo();
			break;
		case IDM_RELOAD_RELOADCOMMAND:
			gServerInfo.ReadCommandInfo();
			break;
		case IDM_RELOAD_RELOADCOMMON:
			gServerInfo.ReadCommonInfo();
			break;
		case IDM_RELOAD_RELOADCUSTOM:
			gServerInfo.ReadCustomInfo();
			break;
		case IDM_RELOAD_RELOADEVENT:
			gServerInfo.ReadEventInfo();
			break;
		case IDM_RELOAD_RELOADEVENTITEMBAG:
			gServerInfo.ReadEventItemBagInfo();
			break;
		case IDM_RELOAD_RELOADHACK:
			gServerInfo.ReadHackInfo();
			break;
		case IDM_RELOAD_RELOADITEM:
			gServerInfo.ReloadItemTextInfo();
			break;
		case IDM_RELOAD_RELOADMONSTER:
			gServerInfo.ReloadMonsterInfo();
			break;
		case IDM_RELOAD_RELOADMOVE:
			gServerInfo.ReadMoveInfo();
			break;
		case IDM_RELOAD_RELOADQUEST:
			gServerInfo.ReadQuestInfo();
			break;
		case IDM_RELOAD_RELOADSHOP:
			gServerInfo.ReadShopInfo();
			break;
		case IDM_RELOAD_RELOADSKILL:
			gServerInfo.ReadSkillInfo();
			break;
		case IDM_RELOAD_RELOADUTIL:
			gServerInfo.ReadUtilInfo();
			break;
		case IDM_RELOAD_RELOADLUA:
			if (g_pScriptLua != nullptr)
			{
				delete g_pScriptLua;
				g_pScriptLua = nullptr;
			}
			g_pScriptLua = new CGMScriptLua();
			{
				int loaded = g_pScriptLua->LoadFolder(".\\lua\\scripts");
				LogAdd(LOG_BLUE, "[Lua] %d script(s) carregado(s).", loaded);
			}
			break;
		case IDM_RELOAD_RELOADBOTS: //MC bots
			gServerInfo.ReloadBotInfo(); //MC bots
			break;
		case IDM_FAKEBOOT_CONNECTALL:
		{
			int queued = gFakeBot.ConnectAll();
			LogAdd((queued < 0) ? LOG_RED : LOG_GREEN, "[FakeBoot] Connect All: %d connection(s) scheduled", queued);
			break;
		}
		case IDM_FAKEBOOT_DISCONNECTALL:
		{
			int disconnected = gFakeBot.DisconnectAll("menu disconnect all");
			LogAdd(LOG_GREEN, "[FakeBoot] Disconnect All: %d disconnected", disconnected);
			break;
		}
		case IDM_RELOAD_RELOADFAKEBOOT:
			gFakeBot.Load(gPath.GetFullPath("Custom\\FakeBoot.txt"));
			LogAdd(LOG_GREEN, "[FakeBoot] Reload completed from menu");
			break;
		case IDM_RELOAD_RELOADALL:
			gServerInfo.ReloadAll();
			break;
		case IDM_STARTONLINELOTTERY_LORENCIA:
			gCustomOnlineLottery.StartNow(0);
			break;
		case IDM_STARTONLINELOTTERY_DEVIAS:
			gCustomOnlineLottery.StartNow(2);
			break;
		case IDM_STARTONLINELOTTERY_NORIA:
			gCustomOnlineLottery.StartNow(3);
			break;
		case IDM_STARTONLINELOTTERY_ELBELAND:
			gCustomOnlineLottery.StartNow(51);
			break;
		case IDM_STARTBC:
			gBloodCastle.StartBC();
			break;
		case IDM_STARTDS:
			gDevilSquare.StartDS();
			break;
		case IDM_STARTCC:
			gChaosCastle.StartCC();
			break;
		case IDM_STARTIT:
			gIllusionTemple.StartIT();
			break;
		case IDM_STARTQUIZ:
			gCustomQuiz.StartQuiz();
			break;
		case IDM_STARTDROP:
			gCustomEventDrop.StartDrop();
			break;
		case IDM_STARTKING:
			gReiDoMU.StartKing();
			break;
		case IDM_STARTTVT:
			gTvTEvent.StartTvT();
			break;
		case IDM_STARTGVG:
			gGvGEvent.StartGvG();
			break;
		case IDM_STARTFINDPATHTONPC:
#ifdef EVENT_FIND_PATH_NPC_CLIK
			gEventFindPath.CommandEventStart();
#endif // EVENT_FIND_PATH_NPC_CLIK
			break;
		case IDM_INVASION0:
			gInvasionManager.StartInvasion(0);
			break;
		case IDM_INVASION1:
			gInvasionManager.StartInvasion(1);
			break;
		case IDM_INVASION2:
			gInvasionManager.StartInvasion(2);
			break;
		case IDM_INVASION3:
			gInvasionManager.StartInvasion(3);
			break;
		case IDM_INVASION4:
			gInvasionManager.StartInvasion(4);
			break;
		case IDM_INVASION5:
			gInvasionManager.StartInvasion(5);
			break;
		case IDM_INVASION6:
			gInvasionManager.StartInvasion(6);
			break;
		case IDM_INVASION7:
			gInvasionManager.StartInvasion(7);
			break;
		case IDM_INVASION8:
			gInvasionManager.StartInvasion(8);
			break;
		case IDM_INVASION9:
			gInvasionManager.StartInvasion(9);
			break;
		case IDM_INVASION10:
			gInvasionManager.StartInvasion(10);
			break;
		case IDM_INVASION11:
			gInvasionManager.StartInvasion(11);
			break;
		case IDM_INVASION12:
			gInvasionManager.StartInvasion(12);
			break;
		case IDM_INVASION13:
			gInvasionManager.StartInvasion(13);
			break;
		case IDM_CA0:
			gCustomArena.StartCustomArena(0);
			break;
		case IDM_CA1:
			gCustomArena.StartCustomArena(1);
			break;
		case IDM_CA2:
			gCustomArena.StartCustomArena(2);
			break;
		case IDM_CA3:
			gCustomArena.StartCustomArena(3);
			break;
		case IDM_CA4:
			gCustomArena.StartCustomArena(4);
			break;
		case IDM_CA5:
			gCustomArena.StartCustomArena(5);
			break;
		case IDM_CA6:
			gCustomArena.StartCustomArena(6);
			break;
		case IDM_CA7:
			gCustomArena.StartCustomArena(7);
			break;
		case IDM_CA8:
			gCustomArena.StartCustomArena(8);
			break;
		case IDM_CA9:
			gCustomArena.StartCustomArena(9);
			break;
		case IDM_CA10:
			gCustomArena.StartCustomArena(10);
			break;
		case IDM_CA11:
			gCustomArena.StartCustomArena(11);
			break;
		case IDM_CA12:
			gCustomArena.StartCustomArena(12);
			break;
		case IDM_CA13:
			gCustomArena.StartCustomArena(13);
			break;
		case IDM_STARTCS:
#if(GAMESERVER_TYPE==1)
			gCastleSiege.StartCS();
#endif
			break;
		case IDM_STARTCW:
			gCrywolf.StartCW();
			break;
		case IDM_STARTLD:
			gCastleDeep.StartLD();
			break;
		case ID_TOOLS_EXPORTITEM:
			gServerInfo.ExportItemBMD();
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;
	case WM_CLOSE:
		if (MessageBox(0, "Close GameServer?", "GameServer", MB_OKCANCEL) == IDOK)
		{
			DestroyWindow(hWnd);
		}
		break;
	case ChatTranslation::WakeMessage:
        ChatTranslation::Get().Tick(hWnd);
        break;
	case WM_TIMER:
		switch (wParam)
		{
		case WM_TIMER_1000:
            ChatTranslation::Get().Tick(hWnd);
            IndependentTierMetadataTick();
			GJServerUserInfoSend();
			ConnectServerInfoSend();
			break;
		case WM_TIMER_2000:
			gObjCountProc();
			gServerDisplayer.Run();
#if(AI_BOT == 1)
			gAIBot.ProcessPendingResponses();
#endif
			break;
		case WM_TIMER_10000:
			JoinServerReconnect(hWnd, WM_JOIN_SERVER_MSG_PROC);
			DataServerReconnect(hWnd, WM_DATA_SERVER_MSG_PROC);
			break;
		}
		break;
	case WM_JOIN_SERVER_MSG_PROC:
		JoinServerMsgProc(wParam, lParam);
		break;
	case WM_DATA_SERVER_MSG_PROC:
		DataServerMsgProc(wParam, lParam);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}

LRESULT CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) // OK
{
	switch (message)
	{
	case WM_INITDIALOG:
		return 1;
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return 1;
		}
		break;
	}

	return 0;
}

LRESULT CALLBACK UserOnline(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) // OK
{
	switch (message)
	{
	case WM_INITDIALOG:

		hWndComboBox = GetDlgItem(hDlg, IDC_LIST1);

		if (!hWndComboBox)
		{
			MessageBox(hDlg, "Could not create the combo box", "Failed Control Creation", MB_OK);
			return FALSE;
		}

		for (int n = OBJECT_START_USER; n < MAX_OBJECT; n++)
		{
			if (gObj[n].Connected >= OBJECT_LOGGED && gObj[n].Type == OBJECT_USER)
			{
				char fulltext[30];
				wsprintf(fulltext, "%s (%s)", gObj[n].Account, gObj[n].Name);

				int pos = SendMessage(hWndComboBox, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>((LPCTSTR)fulltext));
				SendMessage(hWndComboBox, LB_SETITEMDATA, pos, (LPARAM)gObj[n].Account);
			}
		}

		return 1;
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return 1;
		}

		switch (LOWORD(wParam))
		{
		case IDC_BUTTONDC:

			int itemIndex = (int)SendMessage(hWndComboBox, LB_GETCURSEL, (WPARAM)0, (LPARAM)0);

			if (itemIndex == LB_ERR)
			{
				return 0;
			}

			// Getdata
			char* s = (char*)SendMessage(hWndComboBox, LB_GETITEMDATA, itemIndex, 0);

			for (int n = OBJECT_START_USER; n < MAX_OBJECT; n++)
			{
				if (gObj[n].Connected >= OBJECT_LOGGED && strcmp(gObj[n].Account, s) == 0)
				{
					LPOBJ lpObj = &gObj[n];

					gObjUserKill(lpObj->Index);
					MessageBox(hDlg, "Account disconnect sucefully", "Confirm", MB_OK);
					break;
				}
			}

			return 1;
		}
		break;
	}

	return 0;
}


