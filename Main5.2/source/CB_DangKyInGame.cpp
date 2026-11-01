#include "StdAfx.h"
#include "CB_DangKyInGame.h"
#include "NewUISystem.h"
#include "CBInterface.h"
#include "CharacterManager.h"
#include "Util.h"
#include "Protocol.h"
#include "NewUIBase.h"
#include "ZzzInterface.h"
#include "UIMng.h"
using namespace SEASON3B;



CB_DangKyInGame* CB_DangKyInGame::Instance()

{

	static CB_DangKyInGame s_Instance;
	return &s_Instance;
}

CB_DangKyInGame::CB_DangKyInGame()
{
	this->Clear();
}

CB_DangKyInGame::~CB_DangKyInGame()
{
	for (int i = 0; i < TYPE_INPUT_DKTK::eMaxINPUT; i++)
	{
		SAFE_DELETE(CInputData[i]);
	}
	SAFE_DELETE(CInputCaptCha);
}

void CB_DangKyInGame::Clear()
{
	for (int i = 0; i < TYPE_INPUT_DKTK::eMaxINPUT; i++)
	{
		CInputData[i] = NULL;
	}
	CInputCaptCha = NULL;
	this->TimeSendRegTK = 0;
	this->OpenDKTK = false;
}

bool CheckChuoiKyTuDacBiet(const std::string& str)
{
	for (char ch : str)
	{
		if (!isdigit(ch) && !(ch >= 'a' && ch <= 'z') && !(ch >= 'A' && ch <= 'Z'))
		{
			return false;
		}
	}
	return true;
}

void CB_DangKyInGame::OpenOnOff()
{
	if (GetTickCount() - gInterface->Data[eWindow_DangKyInGame].EventTick > 300)
	{
		gInterface->Data[eWindow_DangKyInGame].EventTick = GetTickCount();
		if (gInterface->Data[eWindow_DangKyInGame].OnShow)
		{
			gInterface->Data[eWindow_DangKyInGame].OnShow = 0;
			return;
		}
		gInterface->Data[eWindow_DangKyInGame].OnShow = 1;
	}
}

bool CB_DangKyInGame::RenderWindow(int X, int Y)
{
	if (!gInterface->Data[eWindow_DangKyInGame].OnShow || !gCB_DangKyInGame)
	{
		if (this->OpenDKTK)
		{
			this->Clear();
		}
		return 0;
	}

	this->OpenDKTK = true;
	float WindowW = 262;
	float WindowH = 250;

	static char* mTextStatus[] = { "CONTA:", "SENHA:", "7 Numeros Secretos:", "Telefone:", "Captcha:" };
	static int mInputType[] = { UIOPTION_NOLOCALIZEDCHARACTERS,
								UIOPTION_NOLOCALIZEDCHARACTERS,
								UIOPTION_NUMBERONLY,
								UIOPTION_NUMBERONLY };
	static int mMaxInput[] = { 10, 10, 7, 11 };

	float StartX = (MAX_WIN_WIDTH / 2) - (WindowW / 2);
	float StartY = 150;
	gInterface->Data[eWindow_DangKyInGame].AllowMove = false;

	gInterface->gDrawWindowCustom(&StartX, &StartY, WindowW, WindowH, eWindow_DangKyInGame, "Registrar Conta");

	TextDraw(g_hFontBold, StartX, StartY + 50, 0xFFA200B8, 0x0, WindowW, 0, 3, "O login so pode usar caracteres 0~9, a~z");
	float InputW = 110;
	float InputKhoangCach = 20;
	StartY += 30;

	for (int i = 0; i < TYPE_INPUT_DKTK::eMaxINPUT; i++)
	{
		TextDraw(g_hFontBold, StartX + 30, StartY + 50, 0xFFFFFFFF, 0x0, 100, 0, 1, mTextStatus[i]);
		gInterface->DrawBarForm((StartX + 120) - 3, (StartY + 50) - 3, InputW, 16, 0.0, 0.0, 0.0, 1.0);
		if (gInterface->RenderInputBox(StartX + 120, StartY + 50, InputW, 14, "", this->CInputData[i], (UIOPTIONS)mInputType[i], mMaxInput[i], (i == 1 ? TRUE : FALSE)))
		{
			this->CInputData[i]->SetTextColor(255, 255, 255, 255);
			this->CInputData[i]->SetBackColor(255, 0, 0, 0);
			if (this->CInputData[i + 1]) this->CInputData[i]->SetTabTarget(this->CInputData[i + 1]);
		}
		StartY += InputKhoangCach;
	}

	//===Captcha
	StartY += 20;
	float NTW = 70;
	TextDraw((HFONT)g_hFontBold, StartX + 40, StartY + 50, 0x62FF00FF, 0x0, 100, 0, 1, "Codigo Captcha");
	float CaptChaX = StartX + 105;
	float CaptChaY = StartY + 45;
	if (!CInputCaptCha)
	{
		CInputCaptCha = new CUITextInputBox;
		CInputCaptCha->Init(gwinhandle->GethWnd(), CaptChaX, CaptChaY, 4);
		CInputCaptCha->SetBackColor(0, 0, 0, 0);
		CInputCaptCha->SetTextColor(255, 255, 157, 0);
		CInputCaptCha->SetFont((HFONT)g_hFont);
		CInputCaptCha->SetState(UISTATE_NORMAL);
		CInputCaptCha->SetOption(UIOPTION_NUMBERONLY);
		CInputCaptCha->SetPosition(CaptChaX, CaptChaY);
	}
	else
	{
		gInterface->RenderCaptchaNumber(CaptChaX, CaptChaY, CInputCaptCha, gInterface->vCaptcha.c_str());
	}

	StartY += 30;
	if (gInterface->DrawButton(StartX + 100, StartY + 45, 100, 12, "CONFIRMAR", 80, true))
	{
		this->RequsetDKTK();
	}

	gInterface->DrawMessageBox();
	return 1;
}

bool CB_DangKyInGame::RequsetDKTK()
{
	if (!CInputData[Account] || !CInputData[Pass] || !CInputData[Snonumber] || !CInputData[Phone] || !CInputCaptCha)
	{
		gInterface->OpenMessageBox("Erro", "Erro interno da janela!");
		return 0;
	}

	char szID[MAX_ID_SIZE + 1] = { 0, };
	char szPass[MAX_PASSWORD_SIZE + 1] = { 0, };
	char szSno[7 + 1] = { 0, };
	char szSDT[11 + 1] = { 0, };
	char CGetTextCaptCha[5] = { 0, };

	this->CInputData[Account]->GetText(szID, MAX_ID_SIZE + 1);
	this->CInputData[Pass]->GetText(szPass, MAX_PASSWORD_SIZE + 1);
	this->CInputData[Snonumber]->GetText(szSno, 7 + 1);
	this->CInputData[Phone]->GetText(szSDT, 11 + 1);
	this->CInputCaptCha->GetText(CGetTextCaptCha, MAX_ID_SIZE + 1);

	if (strlen(szID) < 1)
	{
		gInterface->OpenMessageBox("Erro", "Digite o Login");
		return 0;
	}
	if (strlen(szPass) < 1)
	{
		gInterface->OpenMessageBox("Erro", "Digite a Senha");
		return 0;
	}
	if (strlen(szSno) < 7)
	{
		gInterface->OpenMessageBox("Erro", "Digite os 7 numeros secretos");
		return 0;
	}
	if (strlen(szSDT) < 10)
	{
		gInterface->OpenMessageBox("Erro", "Digite o Telefone");
		return 0;
	}
	if (!CheckChuoiKyTuDacBiet(szID) || !CheckChuoiKyTuDacBiet(szPass))
	{
		gInterface->OpenMessageBox("Erro", "Login nao pode conter caracteres especiais");
		return 0;
	}

	std::string CaptchaInput(CGetTextCaptCha);
	if (!gInterface->check_Captcha(gInterface->vCaptcha, CaptchaInput))
	{
		gInterface->OpenMessageBox("Erro", "Codigo Captcha incorreto");
		gInterface->vCaptcha = gInterface->generateCaptcha(4);
		return 0;
	}

	PMSG_REGISTER_MAIN_SEND pMsg;
	ZeroMemory(&pMsg, sizeof(pMsg));
	pMsg.header.set(0xD3, 0x05, sizeof(pMsg));
	pMsg.TypeSend = 0x01;

	strncpy_s(pMsg.account, szID, sizeof(pMsg.account) - 1);
	strncpy_s(pMsg.password, szPass, sizeof(pMsg.password) - 1);
	strncpy_s(pMsg.numcode, szSno, sizeof(pMsg.numcode) - 1);
	strncpy_s(pMsg.sodienthoai, szSDT, sizeof(pMsg.sodienthoai) - 1);

	DataSend((LPBYTE)&pMsg, pMsg.header.size);

	return 1;
}

void CB_DangKyInGame::RecvKQRegInGame(XULY_CGPACKET* lpMsg)
{
	if (!lpMsg)
	{
		return;
	}

	int TypeKQNotice = lpMsg->ThaoTac;
	gInterface->vCaptcha = gInterface->generateCaptcha(4);

	char szID[MAX_ID_SIZE + 1] = { 0 };
	char szPass[MAX_PASSWORD_SIZE + 1] = { 0 };

	if (this->CInputData[Account] && this->CInputData[Pass])
	{
		this->CInputData[Account]->GetText(szID, MAX_ID_SIZE + 1);
		this->CInputData[Pass]->GetText(szPass, MAX_PASSWORD_SIZE + 1);
	}

	switch (TypeKQNotice)
	{
	case 1: // SUCESSO
	{
		gInterface->OpenMessageBox("Sucesso", "Conta criada com sucesso!\nLogin: %s", szID);
		if (strlen(szID) > 0 && strlen(szPass) > 0)
		{
			CUIMng& rUIMng = CUIMng::Instance();
			rUIMng.m_LoginWin.GetIDInputBox()->SetText(szID);
			rUIMng.m_LoginWin.GetPassInputBox()->SetText(szPass);
		}
	}
	break;
	case 0: // CONTA OU TELEFONE JÁ EXISTE
	case 2: // CONTA OU TELEFONE JÁ EXISTE - SEU GS USA ESSE
		gInterface->OpenMessageBox("Erro", "Conta ou telefone ja cadastrado!\nTente outros dados.");
		break;
	default:
		gInterface->OpenMessageBox("Erro", "Erro do servidor! Codigo: %d", TypeKQNotice);
		break;
	}
}