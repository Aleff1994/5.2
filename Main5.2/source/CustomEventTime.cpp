#include "stdafx.h"
#include "CustomEventTime.h"
#include "CBInterface.h"
#include "Util.h"
#include "TextClien.h"
#include "NewUISystem.h"
#include "UIControls.h"

#include "wsclientinline.h"

using namespace SEASON3B;


CCustomEventTime* CCustomEventTime::Instance()
{
	static CCustomEventTime s_Instance;
	return &s_Instance;
}


CCustomEventTime::CCustomEventTime()
{
}

void CCustomEventTime::Init()
{
	this->Click = false;
	this->mNewDataEventTime.clear();
	this->MaxListData = 0;
	this->count = 0;
	this->EventTimeLoad = 0;
	this->EventTimeEnable = 0;
	this->EventTimeTickCount = GetTickCount();
	this->Page = 0;
	this->Category = 0;
	this->Arena = 0;
}


void CCustomEventTime::Load(CUSTOM_EVENT_INFO* info) // OK
{
	for (int n = 0; n < MAX_EVENTTIME; n++)
	{
		this->SetInfo(info[n]);
	}
}

void CCustomEventTime::SetInfo(CUSTOM_EVENT_INFO info) // OK
{
	if (info.Index < 0 || info.Index >= MAX_EVENTTIME)
	{
		return;
	}

	this->m_CustomEventInfo[info.Index] = info;
}

void CCustomEventTime::ClearCustomEventTime() // OK
{
	for (int n = 0; n < MAX_EVENTTIME; n++)
	{
		gCustomEventTime[n].time = -1;
	}
	this->count = 0;
	this->EventTimeEnable = 0;
}
void CCustomEventTime::OpenTestWindow(int Page)
{
	gInterface->Data[eWindowEventTime].OnShow = true;

	PMSG_CUSTOM_EVENTTIME_SEND pMsg;

	pMsg.header.set(0xF3, 0xE8, sizeof(pMsg));

	pMsg.Page = Page;
	pMsg.Category = (BYTE)this->Category;

	DataSend((BYTE*)&pMsg, pMsg.header.size);

};
void CCustomEventTime::OnOffWindow() // OK
{
	gInterface->Data[eWindowEventTime].OpenClose();
	if (gInterface->Data[eWindowEventTime].OnShow)
	{
		g_CustomEventTime->ClearCustomEventTime();
		g_CustomEventTime->OpenTestWindow();
	}
}
void CCustomEventTime::GCReqEventTime(PMSG_CUSTOM_EVENTTIME_RECV* lpMsg) // OK
{
	this->count = lpMsg->count;
	this->mNewDataEventTime.clear();
	this->MaxListData = lpMsg->MaxList;
	for (int n = 0; n < lpMsg->count; n++)
	{
		CUSTOM_EVENTTIME_DATA* lpInfo = (CUSTOM_EVENTTIME_DATA*)(((BYTE*)lpMsg) + sizeof(PMSG_CUSTOM_EVENTTIME_RECV) + (sizeof(CUSTOM_EVENTTIME_DATA) * n));

		//this->gCustomEventTime[n].index = lpInfo->index;
		//this->gCustomEventTime[n].time = lpInfo->time;
		//g_Console.AddMessage(1, "Debug %d (%d) %s Max %d", lpInfo->index, lpInfo->time, lpInfo->NameEvent, this->MaxListData);
		this->mNewDataEventTime.push_back(*lpInfo);
		//if (this->gCustomEventTime[n].index >= 28 && this->gCustomEventTime[n].time != -1) this->Arena = 1;
	}

	this->EventTimeEnable = 1;
}

void CCustomEventTime::DrawEventTimePanelWindow()
{
	if (!gInterface->Data[eWindowEventTime].OnShow)
	{
		return;
	}

	const float MainWidth = 280.0f;
	const float MainHeight = 284.0f;
	float StartX = (MAX_WIN_WIDTH - MainWidth) / 2.0f;
	float StartY = 86.0f;

	// Mantem moldura, textos e botoes presos a mesma origem durante o arraste.
	if (!gInterface->Data[eWindowEventTime].FirstLoad)
	{
		gInterface->Data[eWindowEventTime].Width = MainWidth;
		gInterface->Data[eWindowEventTime].Height = MainHeight;
		gInterface->Data[eWindowEventTime].X = StartX;
		gInterface->Data[eWindowEventTime].Y = StartY;
		gInterface->Data[eWindowEventTime].FirstLoad = true;
	}

	if (gInterface->Data[eWindowEventTime].AllowMove)
	{
		gInterface->SyncMoveWindow(eWindowEventTime);
	}

	StartX = gInterface->Data[eWindowEventTime].X;
	StartY = gInterface->Data[eWindowEventTime].Y;

	if (SEASON3B::CheckMouseIn(StartX, StartY, MainWidth, MainHeight))
	{
		gInterface->SetBlockCur(TRUE);
		gInterface->Data[eWindowEventTime].Hover = true;
	}
	else
	{
		gInterface->Data[eWindowEventTime].Hover = false;
	}

	const float ContentX = StartX + 14.0f;
	const float TabY = StartY + 35.0f;
	const float HeaderY = StartY + 57.0f;
	const float RowsY = StartY + 76.0f;
	const float RowHeight = 14.0f;
	const float EventWidth = 94.0f;
	const float LocalWidth = 86.0f;
	const float TimeWidth = 72.0f;
	const DWORD Now = GetTickCount();

	// Moldura moderna carregada de uma textura dedicada.
	SEASON3B::RenderImageF(IMAGE_EVENT_AGENDA_PANEL, StartX - 2.0f, StartY - 2.0f, MainWidth + 4.0f, MainHeight + 4.0f, 0.0, 0.0, 512.0, 512.0);
	// O titulo fica dentro da faixa central da moldura, sem encostar no relevo superior.
	TextDraw(g_hFontBold, StartX, StartY + 17.0f, 0xF4D88BFF, 0x00000000, MainWidth, 0, 3, "AGENDA");
	// Fechar.
	// Centros convertidos da textura 512x512 para o painel 280x284.
	// Isso mantem o simbolo preso ao quadrado desenhado na propria moldura.
	const float CloseX = StartX + 243.0f;
	const float CloseY = StartY + 24.0f;
	const bool CloseHover = (SEASON3B::CheckMouseIn(CloseX, CloseY, 20.0f, 20.0f) == 1);
	// A textura ja possui o quadrado do botao; desenhamos somente o simbolo sobre ele.
	TextDraw(g_hFontBold, CloseX, CloseY + 6.0f, CloseHover ? 0xFFFFFFFF : 0xE8C97EFF, 0x00000000, 20.0f, 0, 3, "X");
	if (CloseHover && (GetKeyState(VK_LBUTTON) & 0x8000) && (Now - gInterface->Data[eWindowEventTime].EventTick > 350))
	{
		gInterface->Data[eWindowEventTime].EventTick = Now;
		gInterface->SetBlockCurDelay(500);
		gInterface->Data[eWindowEventTime].Close();
		PlayBuffer(25, 0, 0);
		return;
	}

	// Abas textuais: nenhum icone ou retrato e usado. Cada aba solicita ao GS
	// somente sua categoria, mantendo pagina e contagem independentes.
	const float TabWidth = (MainWidth - 30.0f) * 0.5f;
	const float EventTabX = ContentX;
	const float InvasionTabX = ContentX + TabWidth + 2.0f;
	const bool EventTabHover = (SEASON3B::CheckMouseIn(EventTabX, TabY, TabWidth, 18.0f) == 1);
	const bool InvasionTabHover = (SEASON3B::CheckMouseIn(InvasionTabX, TabY, TabWidth, 18.0f) == 1);
	const bool pegasusLook = (gmProtect->LookAndFeel == 0 || gmProtect->LookAndFeel == 4);
	const double activeBlue = pegasusLook ? 0.24 : 0.10;
	gInterface->DrawBarForm(EventTabX, TabY, TabWidth, 18.0f,
		this->Category == 0 ? 0.20 : 0.065, this->Category == 0 ? 0.10 : 0.065,
		this->Category == 0 ? activeBlue : 0.065, 0.96);
	gInterface->DrawBarForm(InvasionTabX, TabY, TabWidth, 18.0f,
		this->Category == 1 ? (pegasusLook ? 0.06 : 0.20) : 0.065,
		this->Category == 1 ? (pegasusLook ? 0.13 : 0.10) : 0.065,
		this->Category == 1 ? (pegasusLook ? 0.25 : activeBlue) : 0.065, 0.96);
	TextDraw(g_hFontBold, EventTabX, TabY + 5.0f, this->Category == 0 ? 0xF4D88BFF : 0xAFA58FFF, 0, TabWidth, 0, 3, "EVENTOS");
	TextDraw(g_hFontBold, InvasionTabX, TabY + 5.0f, this->Category == 1 ? 0xF4D88BFF : 0xAFA58FFF, 0, TabWidth, 0, 3, "INVASOES");
	if ((GetKeyState(VK_LBUTTON) & 0x8000) && (Now - gInterface->Data[eWindowEventTime].EventTick > 350))
	{
		int newCategory = this->Category;
		if (EventTabHover) newCategory = 0;
		else if (InvasionTabHover) newCategory = 1;
		if (newCategory != this->Category)
		{
			this->Category = newCategory;
			this->Page = 0;
			this->ClearCustomEventTime();
			this->OpenTestWindow(0);
			gInterface->Data[eWindowEventTime].EventTick = Now;
			gInterface->SetBlockCurDelay(500);
			PlayBuffer(25, 0, 0);
		}
	}

	// Cabecalho limpo.
	gInterface->DrawBarForm(ContentX, HeaderY, MainWidth - 28.0f, 17.0f, 0.12, 0.095, 0.045, 0.96);
	gInterface->DrawBarForm(ContentX, HeaderY + 16.0f, MainWidth - 28.0f, 1.0f, 0.78, 0.54, 0.17, 0.80);
	TextDraw(g_hFontBold, ContentX, HeaderY + 5.0f, 0xEED184FF, 0x00000000, EventWidth, 0, 3, this->Category == 1 ? "INVASAO" : "EVENTO");
	TextDraw(g_hFontBold, ContentX + EventWidth, HeaderY + 5.0f, 0xEED184FF, 0x00000000, LocalWidth, 0, 3, this->Category == 1 ? "MAPA" : "SERVIDOR");
	TextDraw(g_hFontBold, ContentX + EventWidth + LocalWidth, HeaderY + 5.0f, 0xEED184FF, 0x00000000, TimeWidth, 0, 3, "TEMPO");

	if (this->EventTimeEnable == 1)
	{
		if ((Now - this->EventTimeTickCount) > 1000)
		{
			for (int i = 0; i < (int)this->mNewDataEventTime.size(); ++i)
			{
				if (this->mNewDataEventTime[i].time > 0)
				{
					--this->mNewDataEventTime[i].time;
				}
			}
			this->EventTimeTickCount = Now;
		}

		int line = 0;
		for (int i = 0; i < (int)this->mNewDataEventTime.size() && line < 12; ++i)
		{
			char timeText[30];
			DWORD timeColor = 0xF2E9D7FF;
			const int totalSeconds = this->mNewDataEventTime[i].time;

			if (totalSeconds <= -1)
			{
				wsprintf(timeText, "Desativado");
				timeColor = 0xFF786EFF;
			}
			else if (totalSeconds == 0)
			{
				wsprintf(timeText, "Ativo");
				timeColor = 0x66FF8AFF;
			}
			else if (totalSeconds >= 86400)
			{
				wsprintf(timeText, "%d dia(s)+", totalSeconds / 86400);
			}
			else
			{
				wsprintf(timeText, "%02d:%02d:%02d", totalSeconds / 3600, (totalSeconds / 60) % 60, totalSeconds % 60);
				if (totalSeconds < 300)
				{
					timeColor = 0xB7FF64FF;
				}
			}

			const char* eventName = this->mNewDataEventTime[i].NameEvent;
			const char* location = this->mNewDataEventTime[i].DesString;

			// Exibe o nome original enviado pelo servidor. Para invasoes, o tempo e
			// o nome sao atualizados pelo InvasionManager atraves do gEventName.

			if (_stricmp(location, "Sub_1") == 0) location = "Servidor 1";
			else if (_stricmp(location, "Sub_2") == 0) location = "Servidor 2";
			else if (_stricmp(location, "Sub_3") == 0) location = "Servidor 3";

			const float RowY = RowsY + (line * RowHeight);
			const double rowShade = ((line % 2) == 0) ? 0.055 : 0.085;
			gInterface->DrawBarForm(ContentX, RowY, MainWidth - 28.0f, RowHeight - 1.0f, rowShade, rowShade, rowShade + 0.008, 0.84);

			TextDraw(g_hFont, ContentX + 4.0f, RowY + 3.0f, 0xF3E6CBFF, 0x00000000, EventWidth - 8.0f, 0, 1, "%s", eventName);
			TextDraw(g_hFont, ContentX + EventWidth, RowY + 3.0f, 0xBFD6D2FF, 0x00000000, LocalWidth, 0, 3, "%s", location);
			TextDraw(g_hFont, ContentX + EventWidth + LocalWidth, RowY + 3.0f, timeColor, 0x00000000, TimeWidth, 0, 3, "%s", timeText);
			++line;
		}
	}
	else
	{
		const char* loadingText = "Carregando.";
		if (this->EventTimeLoad == 1) loadingText = "Carregando..";
		else if (this->EventTimeLoad == 2) loadingText = "Carregando...";
		else if (this->EventTimeLoad == 3) loadingText = "Carregando....";
		else if (this->EventTimeLoad == 4) loadingText = "Carregando.....";
		this->EventTimeLoad = (this->EventTimeLoad + 1) % 5;
		TextDraw(g_hFontBold, StartX, StartY + 132.0f, 0xE9CC82FF, 0x00000000, MainWidth, 0, 3, loadingText);
	}

	const int TotalPages = ((this->MaxListData + 11) / 12 > 0) ? ((this->MaxListData + 11) / 12) : 1;
	const float FooterY = StartY + MainHeight - 25.0f;
	gInterface->DrawBarForm(ContentX, FooterY - 4.0f, MainWidth - 28.0f, 1.0f, 0.42, 0.31, 0.12, 0.62);
	TextDraw(g_hFont, StartX, FooterY + 4.0f, 0xD7C28FFF, 0x00000000, MainWidth, 0, 3, "Pagina %d de %d", this->Page + 1, TotalPages);
	const float ArrowY = StartY + 251.0f;
	// A moldura classica possui os dois quadrados de navegacao mais proximos
	// do centro. Looks Pegasus/alternativos preservam a posicao anterior.
	const bool classicAgenda = (gmProtect->LookAndFeel == 1 || gmProtect->LookAndFeel == 2);
	const float PrevX = StartX + (classicAgenda ? 72.0f : 20.0f);
	const float NextX = StartX + (classicAgenda ? 182.0f : 239.0f);
	const bool PrevHover = (this->Page > 0 && SEASON3B::CheckMouseIn(PrevX, ArrowY, 21.0f, 21.0f) == 1);
	const bool NextHover = (this->Page + 1 < TotalPages && SEASON3B::CheckMouseIn(NextX, ArrowY, 21.0f, 21.0f) == 1);
	// Os quadrados das setas tambem fazem parte da moldura; evite uma segunda caixa deslocada.
	TextDraw(g_hFontBold, PrevX, ArrowY + 6.0f, this->Page > 0 ? 0xF0D488FF : 0x746C5AFF, 0x00000000, 21.0f, 0, 3, "<");
	TextDraw(g_hFontBold, NextX, ArrowY + 6.0f, this->Page + 1 < TotalPages ? 0xF0D488FF : 0x746C5AFF, 0x00000000, 21.0f, 0, 3, ">");

	if ((GetKeyState(VK_LBUTTON) & 0x8000) && (Now - gInterface->Data[eWindowEventTime].EventTick > 350))
	{
		if (PrevHover)
		{
			gInterface->Data[eWindowEventTime].EventTick = Now;
			--this->Page;
			this->OpenTestWindow(this->Page);
			gInterface->SetBlockCurDelay(500);
		}
		else if (NextHover)
		{
			gInterface->Data[eWindowEventTime].EventTick = Now;
			++this->Page;
			this->OpenTestWindow(this->Page);
			gInterface->SetBlockCurDelay(500);
		}
	}
}
