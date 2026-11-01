#include "stdafx.h"
#include "TextClien.h"
#include "NewUIDailyReward.h"
#include "WSclient.h"
#include "CGMProtect.h"
#include "CBInterface.h"
#include "NewUICommon.h"
#include "NewUIItemTooltip.h"
#include "NewUISystem.h"
#include "ZzzInventory.h"

using namespace SEASON3B;

CNewUIDailyReward SEASON3B::g_DailyRewardUI;
CNewUILuckyWheel SEASON3B::g_LuckyWheelUI;
static void RenderDailyRewardItem(float x, float y, float width, float height, WORD type, BYTE level, bool rotate)
{
	::RenderItem3D(x, y, width, height, type, ((int)level << 3), 0, 0, false,
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, rotate);
}

CNewUILuckyWheel::CNewUILuckyWheel()
{
	m_Open=false; m_Loaded=false; m_Spinning=false; m_Waiting=false; m_TextureLoaded=false; m_ClaimReady=false; m_Claiming=false; m_Count=0; m_FreeRemaining=0; m_DailyRemaining=0;
	m_Highlight=0; m_Target=0xFF; m_Pool=0; ZeroMemory(m_TabEnabled,sizeof(m_TabEnabled)); m_WCoinCost=0; m_WCoinPCost=0; m_GPCost=0; m_ZenCost=0; m_SpinStart=0; m_LastStep=0;
	m_TicketType=0; m_TicketCount=0; ZeroMemory(m_Rewards,sizeof(m_Rewards)); ZeroMemory(m_Items,sizeof(m_Items));
	m_IconX=m_IconY=m_DragOffsetX=m_DragOffsetY=0.0f; m_IconPositioned=false; m_IconDragging=false; m_IconMoved=false;
}

CNewUILuckyWheel::~CNewUILuckyWheel() { ClearItems(); }

void CNewUILuckyWheel::ClearItems()
{
	// A roleta e o CNewUISystem sao objetos globais. No encerramento do
	// cliente, a ordem dos destrutores pode fazer a roleta chegar aqui depois
	// que o gerenciador de itens ja foi liberado. Nesse caso os itens ja foram
	// eliminados por CNewUIItemMng::DeleteAllItems(), portanto apenas descarte
	// as referencias locais. Chamar DeleteItem por um manager nulo causava o
	// access violation registrado nos dumps (0x007089F6).
	CNewUIItemMng* pItemMng = g_pNewItemMng;
	for (int n = 0; n < LUCKY_WHEEL_MAX_REWARDS; ++n)
	{
		if (m_Items[n] && pItemMng)
		{
			pItemMng->DeleteItem(m_Items[n]);
		}

		m_Items[n] = NULL;
	}
}

bool CNewUILuckyWheel::IsOpen() const { return m_Open; }
bool CNewUILuckyWheel::IsEnabled() const { return (gmProtect != NULL && gmProtect->LuckyWheelEnable); }
bool CNewUILuckyWheel::IsMouseInPanel() const
{
	if(!IsEnabled() || !m_Open) return false; float w=600.0f,h=440.0f,x=((float)GetWindowsX-w)*0.5f,y=((float)GetWindowsY-h)*0.5f;
	return CheckMouseIn(x,y,w,h);
}
bool CNewUILuckyWheel::UpdateKeyEvent()
{
	if(!IsEnabled()) { m_Open=false; return true; }
	if(m_Open && IsPress(VK_ESCAPE)) { Close(); PlayBuffer(SOUND_CLICK01); return false; } return true;
}
void CNewUILuckyWheel::Text(int x,int y,const char* text,int width,int sort,BYTE r,BYTE g,BYTE b)
{
	g_pRenderText->SetFont(g_hFont); g_pRenderText->SetBgColor(0,0,0,0); g_pRenderText->SetTextColor(r,g,b,255);
	g_pRenderText->RenderText(x,y,(char*)text,width,14,sort);
}
void CNewUILuckyWheel::StyledButton(float x,float y,float width,float height,const char* text,bool hover,bool active)
{
	bool down=active && hover && IsRepeat(VK_LBUTTON); float dy=down?2.0f:0.0f;
	// Sombra curta, moldura gunmetal e miolo grafite, no estilo dos atalhos.
	gInterface->DrawBarForm(x+1.0f,y+2.0f,width,height,0.0,0.0,0.0,0.28);
	gInterface->DrawBarForm(x,y+dy,width,height,0.24,0.25,0.27,active?(down?0.76:0.92):0.46);
	gInterface->DrawBarForm(x+2.0f,y+2.0f+dy,width-4.0f,height-4.0f,hover?0.25:0.12,hover?0.26:0.13,hover?0.28:0.15,active?0.90:0.54);
	gInterface->DrawBarForm(x+5.0f,y+5.0f+dy,width-10.0f,height-10.0f,hover?0.12:0.045,hover?0.13:0.05,hover?0.14:0.06,0.82);
	int textY=(int)(y+dy+(height-14.0f)*0.5f);
	g_pRenderText->SetFont(g_hFontBold); g_pRenderText->SetBgColor(0,0,0,0); g_pRenderText->SetTextColor(35,5,2,230);
	g_pRenderText->RenderText((int)x+1,textY+1,(char*)text,(int)width,14,RT3_SORT_CENTER);
	g_pRenderText->SetTextColor(active?(hover?225:190):115,active?(hover?228:194):118,active?(hover?235:202):125,255);
	g_pRenderText->RenderText((int)x,textY,(char*)text,(int)width,14,RT3_SORT_CENTER);
}
void CNewUILuckyWheel::SendInfo()
{
	if(!IsEnabled()) return;
	PMSG_LUCKY_WHEEL_REQUEST_SEND p; p.header.set(0xF3,0x60,sizeof(p)); p.Pool=m_Pool; DataSend((BYTE*)&p,sizeof(p));
}
void CNewUILuckyWheel::SendSpin(BYTE payment)
{
	if(!IsEnabled()) return;
	if(!m_Loaded || m_Spinning || m_Waiting || m_ClaimReady || m_Claiming || !m_Count || !m_DailyRemaining) return;
	PMSG_LUCKY_WHEEL_SPIN_SEND p; p.header.set(0xF3,0x61,sizeof(p)); p.Payment=payment; DataSend((BYTE*)&p,sizeof(p));
	// Aguarda o GameServer confirmar pagamento e premio antes de animar.
	m_Waiting=true; m_Target=0xFF; m_SpinStart=timeGetTime(); m_LastStep=m_SpinStart;
}
void CNewUILuckyWheel::SendClaim()
{
	if(!IsEnabled()) return;
	if(!m_ClaimReady || m_Claiming) return;
	PMSG_LUCKY_WHEEL_CLAIM_SEND p; p.header.set(0xF3,0x62,sizeof(p)); DataSend((BYTE*)&p,sizeof(p)); m_Claiming=true;
}
void CNewUILuckyWheel::Open() { if(!IsEnabled()) { m_Open=false; return; } m_Open=true; m_Loaded=false; SendInfo(); }
void CNewUILuckyWheel::Close() { if(!m_Spinning && !m_ClaimReady && !m_Claiming) m_Open=false; }

void CNewUILuckyWheel::ReceiveInfo(PMSG_LUCKY_WHEEL_INFO_RECV* p)
{
	if(!IsEnabled()) { ClearItems(); m_Open=false; m_Loaded=false; return; }
	ClearItems(); m_Open=true; m_Loaded=p->Enabled!=0; m_Count=min((BYTE)LUCKY_WHEEL_MAX_REWARDS,p->RewardCount);
	m_FreeRemaining=p->FreeRemaining; m_DailyRemaining=p->DailyRemaining; m_WCoinCost=p->WCoinCost; m_GPCost=p->GoblinPointCost;
	m_Pool=p->Pool; memcpy(m_TabEnabled,p->TabEnabled,sizeof(m_TabEnabled)); m_WCoinPCost=p->WCoinPCost;
	m_ZenCost=p->ZenCost; m_TicketType=p->TicketType; m_TicketCount=p->TicketCount; memcpy(m_Rewards,p->Reward,sizeof(m_Rewards));
	for(int n=0;n<m_Count;n++) if(m_Rewards[n].Kind==0)
	{
		BYTE option1=(m_Rewards[n].Skill?0x80:0)|(m_Rewards[n].Luck?0x04:0);
		BYTE level=(BYTE)((m_Rewards[n].Level<<3)|(m_Rewards[n].Option&7));
		BYTE extended=(BYTE)(m_Rewards[n].Excellent|m_Rewards[n].Ancient);
		m_Items[n]=g_pNewItemMng->CreateItem(m_Rewards[n].ItemType,level,255,option1,extended,m_Rewards[n].ItemOptionEx,m_Rewards[n].Harmony,m_Rewards[n].Socket);
	}
}

void CNewUILuckyWheel::ReceiveResult(PMSG_LUCKY_WHEEL_RESULT_RECV* p)
{
	if(!IsEnabled()) { m_Open=false; m_Waiting=false; m_Spinning=false; return; }
	m_Waiting=false;
	if(!p->Success) { m_Spinning=false; m_Target=0xFF; SendInfo(); return; }
	m_Target=(p->RewardIndex<m_Count)?p->RewardIndex:0; m_Spinning=true; m_SpinStart=timeGetTime(); m_LastStep=m_SpinStart; SendInfo();
}

void CNewUILuckyWheel::ReceiveClaim(PMSG_LUCKY_WHEEL_CLAIM_RECV* p)
{
	if(!IsEnabled()) { m_Open=false; m_Claiming=false; return; }
	m_Claiming=false;
	if(p->Success) { m_ClaimReady=false; m_Target=0xFF; SendInfo(); }
}

void CNewUILuckyWheel::Render()
{
	if(!IsEnabled()) { m_Open=false; return; }
	if(SceneFlag!=MAIN_SCENE) { m_Open=false; return; }
	const float iconSize=28.0f;
	if(!m_IconPositioned) { m_IconX=(float)GetWindowsX-39.0f; m_IconY=125.0f; m_IconPositioned=true; }
	float iconX=m_IconX,iconY=m_IconY;
	if(!m_TextureLoaded)
	{
		LoadBitmap("Interface\\LuckyWheel\\lucky_wheel_panel.tga",BITMAP_LUCKY_WHEEL_PANEL,GL_LINEAR);
		LoadBitmap("Interface\\LauncherLetters\\launcher_lw_gunmetal.tga",BITMAP_LUCKY_WHEEL_LOGO,GL_LINEAR);
		m_TextureLoaded=true;
	}
	EnableAlphaTest(); glColor4f(1,1,1,1);
	RenderBitmap(BITMAP_LUCKY_WHEEL_LOGO,iconX,iconY,iconSize,iconSize,0,0,1,1,true,true,0);
	bool iconHover=CheckMouseIn(iconX,iconY,iconSize,iconSize);
	if(iconHover && IsPress(VK_LBUTTON)) { m_IconDragging=true; m_IconMoved=false; m_DragOffsetX=MouseX-iconX; m_DragOffsetY=MouseY-iconY; }
	if(m_IconDragging && IsRepeat(VK_LBUTTON)) { float nx=MouseX-m_DragOffsetX,ny=MouseY-m_DragOffsetY; if(fabsf(nx-m_IconX)>2.0f || fabsf(ny-m_IconY)>2.0f) m_IconMoved=true; m_IconX=max(0.0f,min(nx,(float)GetWindowsX-iconSize)); m_IconY=max(0.0f,min(ny,(float)GetWindowsY-iconSize)); }
	if(m_IconDragging && IsRelease(VK_LBUTTON)) { m_IconDragging=false; if(!m_IconMoved) { if(m_Open) Close(); else Open(); } }
	if(!m_Open) return;

	float w=600.0f,h=440.0f,x=((float)GetWindowsX-w)*0.5f,y=((float)GetWindowsY-h)*0.5f;
	EnableAlphaTest(); glColor4f(1,1,1,1); RenderBitmap(BITMAP_LUCKY_WHEEL_PANEL,x,y,w,h,0,0,1,1,true,true,0);
	g_pRenderText->SetFont(g_hFontBold); Text((int)x,(int)y+12,"LUCKY WHEEL",(int)w,RT3_SORT_CENTER,205,210,218);
	if(CheckMouseIn(x+w-35,y+12,22,22)) { Text((int)x+w-35,(int)y+15,"X",22,RT3_SORT_CENTER,255,120,80); if(IsRelease(VK_LBUTTON)) { Close(); return; } }
	const char* tabName[4]={"WCoinC","WCoinP",gTextClien.Select("GRATIS", "FREE", "GRATIS"),"GOBLIN"};
	int visibleTabs=0;
	for(int tab=0;tab<2;tab++) if(m_TabEnabled[tab])
	{
		float tx=x+45.0f+visibleTabs*100.0f,ty=y+40.0f; bool over=CheckMouseIn(tx,ty,92,25);
		bool selected=(tab==m_Pool);
		// Abas opacas: base metalica solida para nao misturar com os premios ao fundo.
		gInterface->DrawBarForm(tx,ty,92,25,0.34,0.36,0.40,1.0);
		gInterface->DrawBarForm(tx+2.0f,ty+2.0f,88,21,selected?0.22:(over?0.19:0.145),selected?0.23:(over?0.20:0.155),selected?0.26:(over?0.23:0.18),1.0);
		gInterface->DrawBarForm(tx+5.0f,ty+5.0f,82,15,selected?0.105:0.075,selected?0.11:0.08,selected?0.125:0.095,1.0);
		gInterface->DrawBarForm(tx+3.0f,ty+2.0f,86,1.0f,0.63,0.66,0.72,1.0);
		if(selected) { gInterface->DrawBarForm(tx+3.0f,ty+21.0f,86,2,0.72,0.75,0.82,1.0); }
		Text((int)tx,(int)ty+7,tabName[tab],92,RT3_SORT_CENTER,selected?238:(over?225:205),selected?240:(over?228:208),selected?246:(over?236:218));
		if(over && IsRelease(VK_LBUTTON) && !m_Spinning && !m_Waiting && !m_ClaimReady && !m_Claiming && tab!=m_Pool) { m_Pool=(BYTE)tab; m_Loaded=false; SendInfo(); return; }
		visibleTabs++;
	}

	DWORD now=timeGetTime();
	if(m_Spinning && m_Count)
	{
		DWORD elapsed=now-m_SpinStart; DWORD interval=(elapsed<1200)?65:((elapsed<2200)?120:220);
		if(now-m_LastStep>=interval) { m_Highlight=(m_Highlight+1)%m_Count; m_LastStep=now; PlayBuffer(SOUND_CLICK01); }
		if(m_Target!=0xFF && elapsed>2600 && m_Highlight==m_Target) { m_Spinning=false; m_ClaimReady=true; }
	}
	if(m_Waiting && now-m_SpinStart>5000) { m_Waiting=false; SendInfo(); }

	const float cx=x+215.0f,cy=y+220.0f;
	const float radius=(m_Count>12)?132.0f:125.0f,slotW=(m_Count>12)?44.0f:52.0f,slotH=(m_Count>12)?34.0f:38.0f;
	int hovered=-1;
	for(int n=0;n<m_Count;n++)
	{
		float angle=(-1.5707963f)+(6.2831853f*(float)n/(float)m_Count); float sx=cx+cosf(angle)*radius-slotW*0.5f; float sy=cy+sinf(angle)*radius-slotH*0.5f;
		bool active=(n==m_Highlight); bool glowActive=active && (m_Spinning || m_ClaimReady); bool hover=CheckMouseIn(sx,sy,slotW,slotH); if(hover) hovered=n;
		int trail=(m_Highlight-n+m_Count)%m_Count;
		if(glowActive || (m_Spinning && trail>0 && trail<=2))
		{
			float glow=glowActive?(0.72f+0.20f*(sinf((float)now*0.012f)+1.0f)*0.5f):((trail==1)?0.32f:0.15f);
			float ex=sx-3.0f,ey=sy-3.0f,ew=slotW+6.0f,eh=slotH+6.0f;
			// Aro luminoso sem fundo solido; os dois anteriores formam um rastro curto.
			gInterface->DrawBarForm(ex,ey,ew,2.0f,0.60,0.68,0.82,glow);
			gInterface->DrawBarForm(ex,ey+eh-2.0f,ew,2.0f,0.60,0.68,0.82,glow);
			gInterface->DrawBarForm(ex,ey,2.0f,eh,0.60,0.68,0.82,glow);
			gInterface->DrawBarForm(ex+ew-2.0f,ey,2.0f,eh,0.60,0.68,0.82,glow);
			if(glowActive)
			{
				int spark=(int)((now/75)%4);
				float sparkX=(spark==0)?ex-2.0f:((spark==1)?ex+ew-6.0f:((spark==2)?ex+ew-6.0f:ex-2.0f));
				float sparkY=(spark==0)?ey-2.0f:((spark==1)?ey-2.0f:((spark==2)?ey+eh-3.0f:ey+eh-3.0f));
				gInterface->DrawBarForm(sparkX,sparkY,8.0f,4.0f,0.92,0.95,1.0,0.96);
				gInterface->DrawBarForm(sparkX+2.0f,sparkY-2.0f,4.0f,8.0f,0.72,0.82,1.0,0.68);
			}
		}
	}

	// Todos os premios 3D compartilham a mesma configuracao de camera e o mesmo
	// depth clear. Antes cada slot reiniciava o OpenGL inteiro a cada quadro.
	bool hasItemReward=false;
	for(int n=0;n<m_Count;n++) if(m_Rewards[n].Kind==0 && m_Items[n]) { hasItemReward=true; break; }
	if(hasItemReward)
	{
		SEASON3B::begin3D();
		for(int n=0;n<m_Count;n++) if(m_Rewards[n].Kind==0 && m_Items[n])
		{
			float angle=(-1.5707963f)+(6.2831853f*(float)n/(float)m_Count);
			float sx=cx+cosf(angle)*radius-slotW*0.5f;
			float sy=cy+sinf(angle)*radius-slotH*0.5f;
			bool active=(n==m_Highlight);
			bool hover=CheckMouseIn(sx,sy,slotW,slotH);
			bool glowActive=active && (m_Spinning || m_ClaimReady);
			float pulse=glowActive?(2.0f+(sinf((float)now*0.008f)+1.0f)*1.5f):0.0f;
			float itemW=30.0f+pulse,itemH=(slotH-4.0f)+pulse;
			RenderDailyRewardItem(sx+(slotW-itemW)*0.5f,sy+2.0f-(pulse*0.5f),itemW,itemH,m_Rewards[n].ItemType,m_Rewards[n].Level,hover || active);
		}
		SEASON3B::endrender3D();
	}

	// Valores ficam por cima dos slots e dos modelos, preservando a leitura.
	for(int n=0;n<m_Count;n++) if(m_Rewards[n].Kind!=0 || !m_Items[n])
	{
		float angle=(-1.5707963f)+(6.2831853f*(float)n/(float)m_Count);
		float sx=cx+cosf(angle)*radius-slotW*0.5f;
		float sy=cy+sinf(angle)*radius-slotH*0.5f;
		bool active=(n==m_Highlight);
		char value[24]; sprintf_s(value,"x %u",m_Rewards[n].Amount);
		Text((int)sx,(int)sy+12,value,(int)slotW,RT3_SORT_CENTER,active?225:175,active?228:180,active?235:188);
	}

	bool centerHover=CheckMouseIn(cx-43,cy-17,86,34);
	StyledButton(cx-43,cy-17,86,34,m_Waiting?gTextClien.Select("AGUARDE", "WAIT", "ESPERA"):(m_Spinning?gTextClien.Select("GIRANDO", "SPINNING", "GIRANDO"):(m_ClaimReady?gTextClien.Select("RESGATE", "CLAIM", "RECLAMAR"):gTextClien.Select("GIRAR", "SPIN", "GIRAR"))),centerHover,!m_Waiting && !m_Spinning && !m_ClaimReady);
	if(!m_Spinning && !m_Waiting && CheckMouseIn(cx-43,cy-17,86,34) && IsRelease(VK_LBUTTON)) SendSpin(m_Pool);
	gInterface->DrawBarForm(x+420,y+76,166,270,0.025,0.018,0.012,0.46);
	Text((int)x+425,(int)y+91,tabName[m_Pool],156,RT3_SORT_CENTER,200,205,214);
	DWORD selectedCost=(m_Pool==0)?m_WCoinCost:((m_Pool==1)?m_WCoinPCost:((m_Pool==3)?m_GPCost:0));
	const char* selectedCurrency=(m_Pool==0)?"WCoinC":((m_Pool==1)?"WCoinP":((m_Pool==3)?"Goblin Point":gTextClien.Select("Giro diario", "Daily spin", "Giro diario")));
	char costText[64]; sprintf_s(costText,gTextClien.Select("Custo: %u %s", "Cost: %u %s", "Costo: %u %s"),selectedCost,selectedCurrency);
	Text((int)x+425,(int)y+126,costText,156,RT3_SORT_CENTER,190,195,202);
	Text((int)x+425,(int)y+154,gTextClien.Select("Premios e chances exclusivos", "Exclusive rewards and odds", "Premios y chances exclusivos"),156,RT3_SORT_CENTER,155,160,168);
	float payX=x+426.0f,payY=y+190.0f; bool payHover=CheckMouseIn(payX,payY,154,38);
	StyledButton(payX,payY,154,38,gTextClien.Select("GIRAR AGORA", "SPIN NOW", "GIRAR AHORA"),payHover,!m_Spinning && !m_Waiting && !m_ClaimReady && !m_Claiming);
	if(payHover && IsRelease(VK_LBUTTON)) SendSpin(m_Pool);
	if(m_TabEnabled[2] && m_FreeRemaining>0)
	{
		float freeY=y+239.0f; bool freeHover=CheckMouseIn(payX,freeY,154,34);
		StyledButton(payX,freeY,154,34,gTextClien.Select("USAR GIRO GRATIS", "USE FREE SPIN", "USAR GIRO GRATIS"),freeHover,!m_Spinning && !m_Waiting && !m_ClaimReady && !m_Claiming);
		if(freeHover && IsRelease(VK_LBUTTON)) SendSpin(2);
	}
	if(m_ClaimReady || m_Claiming)
	{
		float claimX=x+426.0f,claimY=y+284.0f; bool claimHover=CheckMouseIn(claimX,claimY,154,36);
		StyledButton(claimX,claimY,154,36,m_Claiming?gTextClien.Select("ENTREGANDO...", "DELIVERING...", "ENTREGANDO..."):gTextClien.Select("RESGATAR ITEM", "CLAIM ITEM", "RECLAMAR OBJETO"),claimHover,!m_Claiming);
		if(claimHover && IsRelease(VK_LBUTTON)) SendClaim();
	}
	char status[96]; sprintf_s(status,gTextClien.Select("Gratis: %u  |  Restantes: %u", "Free: %u  |  Remaining: %u", "Gratis: %u  |  Restantes: %u"),m_FreeRemaining,m_DailyRemaining);
	Text((int)x+421,(int)y+358,status,164,RT3_SORT_CENTER,185,190,198);
	gInterface->DrawBarForm(x+30.0f,y+366.0f,370.0f,50.0f,0.018,0.012,0.01,0.30);
	Text((int)x+65,(int)y+372,gTextClien.Select("PREMIO SELECIONADO", "SELECTED REWARD", "PREMIO SELECCIONADO"),300,RT3_SORT_CENTER,175,180,188);
	Text((int)x+65,(int)y+395,(hovered>=0)?m_Rewards[hovered].Name:m_Rewards[m_Highlight].Name,300,RT3_SORT_CENTER,210,215,222);
	if(hovered>=0 && m_Items[hovered]) g_pNewItemTooltip->RenderItemTooltip(MouseX+12,MouseY,m_Items[hovered],false);
}

CNewUIDailyReward::CNewUIDailyReward()
{
	m_Open = false;
	m_TextureLoaded = false;
	m_IconLoaded = false;
	m_ButtonLoaded = false;
	m_DataLoaded = false;
	m_Month = 0;
	m_Today = 0;
	m_DaysInMonth = 0;
	m_StartWeekday = 1;
	m_CanClaimToday = 0;
	m_BonusAvailable = 0;
	m_BonusClaimed = 0;
	m_LastRequest = 0;
	m_IconX=m_IconY=m_DragOffsetX=m_DragOffsetY=0.0f; m_IconPositioned=false; m_IconDragging=false; m_IconMoved=false;
	memset(m_ClaimedDays, 0, sizeof(m_ClaimedDays));
	memset(m_RewardKind, 0, sizeof(m_RewardKind));
	memset(m_RewardItemType, 0, sizeof(m_RewardItemType));
	memset(m_RewardLevel, 0, sizeof(m_RewardLevel));
	memset(m_RewardAmount, 0, sizeof(m_RewardAmount));
}

bool CNewUIDailyReward::IsOpen() const { return m_Open; }
bool CNewUIDailyReward::IsMouseInPanel() const
{
	if (!m_Open) return false;
	const float panelW = 580.0f;
	const float panelH = 430.0f;
	const float x = ((float)GetWindowsX - panelW) * 0.5f;
	const float y = ((float)GetWindowsY - panelH) * 0.5f;
	return CheckMouseIn(x, y, panelW, panelH);
}

bool CNewUIDailyReward::UpdateKeyEvent()
{
	if (m_Open && IsPress(VK_ESCAPE))
	{
		Close();
		PlayBuffer(SOUND_CLICK01);
		return false;
	}
	return true;
}

bool CNewUIDailyReward::IsEnabled() const
{
	return gmProtect->DailyRewardEnable;
}

void CNewUIDailyReward::SendRequest(BYTE subCode)
{
	PMSG_DAILY_REWARD_REQUEST_SEND pMsg;
	BYTE requestSubCode = (subCode == 0x21) ? 0xEA : ((subCode == 0x23) ? 0xEF : 0xF1);
	pMsg.header.set(0xF3, requestSubCode, sizeof(pMsg));
	DataSend((BYTE*)&pMsg, pMsg.header.size);
}

void CNewUIDailyReward::Open()
{
	if (!IsEnabled()) return;
	m_Open = true;
	m_DataLoaded = false;
	m_LastRequest = timeGetTime();
	SendRequest(0x21);
}

void CNewUIDailyReward::Close() { m_Open = false; }

void CNewUIDailyReward::ReceiveInfo(PMSG_DAILY_REWARD_INFO_RECV* lpMsg)
{
	m_DataLoaded = true;
	m_Month = lpMsg->Month;
	m_Today = lpMsg->Today;
	m_DaysInMonth = min((BYTE)DAILY_REWARD_MAX_DAYS, lpMsg->DaysInMonth);
	m_StartWeekday = (lpMsg->StartWeekday >= 1 && lpMsg->StartWeekday <= 7) ? lpMsg->StartWeekday : 1;
	m_CanClaimToday = lpMsg->CanClaimToday;
	memcpy(m_ClaimedDays, lpMsg->ClaimedDays, sizeof(m_ClaimedDays));
	memcpy(m_RewardKind, lpMsg->RewardKind, sizeof(m_RewardKind));
	memcpy(m_RewardItemType, lpMsg->RewardItemType, sizeof(m_RewardItemType));
	memcpy(m_RewardLevel, lpMsg->RewardLevel, sizeof(m_RewardLevel));
	memcpy(m_RewardAmount, lpMsg->RewardAmount, sizeof(m_RewardAmount));
	m_BonusAvailable = lpMsg->BonusAvailable;
	m_BonusClaimed = lpMsg->BonusClaimed;
}

void CNewUIDailyReward::ReceiveResult(PMSG_DAILY_REWARD_RESULT_RECV* lpMsg)
{
	if (lpMsg->Success) SendRequest(0x21);
}

void CNewUIDailyReward::RenderTextLine(int x, int y, const char* text, int width, int sort, BYTE r, BYTE g, BYTE b)
{
	g_pRenderText->SetFont(g_hFont);
	g_pRenderText->SetBgColor(0, 0, 0, 0);
	g_pRenderText->SetTextColor(r, g, b, 255);
	g_pRenderText->RenderText(x, y, (char*)text, width, 14, sort);
}

void CNewUIDailyReward::RenderLauncher()
{
	if (SceneFlag != MAIN_SCENE) return;
	if (!m_IconLoaded)
	{
		LoadBitmap("Interface\\LauncherLetters\\launcher_pd_gunmetal.tga", BITMAP_DAILY_REWARD_ICON, GL_LINEAR);
		m_IconLoaded = true;
	}

	const float size = 28.0f;
	if(!m_IconPositioned) { m_IconX=(float)GetWindowsX-39.0f; m_IconY=160.0f; m_IconPositioned=true; }
	const float x = m_IconX;
	const float y = m_IconY;
	const bool hover = CheckMouseIn(x, y, size, size);
	const bool pending = (m_CanClaimToday != 0 || m_BonusAvailable != 0);

	EnableAlphaTest();
	glColor4f(1, 1, 1, 1);
	RenderBitmap(BITMAP_DAILY_REWARD_ICON, x, y, size, size, 0, 0, 1, 1, true, true, 0);

	if (pending)
	{
		RenderTextLine((int)x + 22, (int)y + 1, "!", 10, RT3_SORT_CENTER, 255, 245, 120);
	}
	if (hover)
	{
		RenderTextLine((int)x - 105, (int)y + 10, gTextClien.Select("PREMIO DIARIO", "DAILY REWARD", "PREMIO DIARIO"), 100, RT3_SORT_RIGHT, 245, 215, 145);
	}
	if(hover && IsPress(VK_LBUTTON)) { m_IconDragging=true; m_IconMoved=false; m_DragOffsetX=MouseX-x; m_DragOffsetY=MouseY-y; }
	if(m_IconDragging && IsRepeat(VK_LBUTTON)) { float nx=MouseX-m_DragOffsetX,ny=MouseY-m_DragOffsetY; if(fabsf(nx-m_IconX)>2.0f || fabsf(ny-m_IconY)>2.0f) m_IconMoved=true; m_IconX=max(0.0f,min(nx,(float)GetWindowsX-size)); m_IconY=max(0.0f,min(ny,(float)GetWindowsY-size)); }
	if(m_IconDragging && IsRelease(VK_LBUTTON)) { m_IconDragging=false; if(!m_IconMoved) { if(m_Open) Close(); else Open(); } }
}

void CNewUIDailyReward::Render()
{
	if (!IsEnabled())
	{
		m_Open = false;
		return;
	}

	RenderLauncher();
	if (!m_Open) return;

	const float panelW = 580.0f;
	const float panelH = 430.0f;
	const float x = ((float)GetWindowsX - panelW) * 0.5f;
	const float y = ((float)GetWindowsY - panelH) * 0.5f;
	if (!m_TextureLoaded)
	{
		LoadBitmap("Interface\\DailyReward\\daily_reward_panel.tga", BITMAP_DAILY_REWARD_PANEL, GL_LINEAR);
		m_TextureLoaded = true;
	}
	if (!m_ButtonLoaded)
	{
		LoadBitmap("Interface\\HUD\\newui_btn_empty.tga", BITMAP_DAILY_REWARD_BUTTON, GL_LINEAR);
		m_ButtonLoaded = true;
	}

	EnableAlphaTest();
	RenderBitmap(BITMAP_DAILY_REWARD_PANEL, x, y, panelW, panelH, 0, 0, 1, 1, true, true, 0);
	glColor4f(1, 1, 1, 1);

	g_pRenderText->SetFont(g_hFontBold);
	g_pRenderText->SetBgColor(0, 0, 0, 0);
	g_pRenderText->SetTextColor(235, 200, 120, 255);
	g_pRenderText->RenderText((int)x, (int)y + 14, gTextClien.Select("PREMIO DIARIO", "DAILY REWARD", "PREMIO DIARIO"), (int)panelW, 18, RT3_SORT_CENTER);
	const char* monthNames[] = { gTextClien.Select("JANEIRO", "JANUARY", "ENERO"), gTextClien.Select("FEVEREIRO", "FEBRUARY", "FEBRERO"), gTextClien.Select("MARCO", "MARCH", "MARZO"), gTextClien.Select("ABRIL", "APRIL", "ABRIL"), gTextClien.Select("MAIO", "MAY", "MAYO"), gTextClien.Select("JUNHO", "JUNE", "JUNIO"), gTextClien.Select("JULHO", "JULY", "JULIO"), gTextClien.Select("AGOSTO", "AUGUST", "AGOSTO"), gTextClien.Select("SETEMBRO", "SEPTEMBER", "SEPTIEMBRE"), gTextClien.Select("OUTUBRO", "OCTOBER", "OCTUBRE"), gTextClien.Select("NOVEMBRO", "NOVEMBER", "NOVIEMBRE"), gTextClien.Select("DEZEMBRO", "DECEMBER", "DICIEMBRE") };
	if (m_Month >= 1 && m_Month <= 12)
	{
		RenderTextLine((int)x, (int)y + 31, monthNames[m_Month - 1], (int)panelW, RT3_SORT_CENTER, 200, 178, 125);
	}

	const float closeX = x + panelW - 38.0f;
	const float closeY = y + 13.0f;
	RenderTextLine((int)closeX, (int)closeY, "X", 20, RT3_SORT_CENTER, 240, 180, 120);
	if (CheckMouseIn(closeX, closeY, 20, 20) && IsRelease(VK_LBUTTON)) { Close(); return; }

	const char* names[] = { gTextClien.Select("DOM", "SUN", "DOM"), gTextClien.Select("SEG", "MON", "LUN"), gTextClien.Select("TER", "TUE", "MAR"), gTextClien.Select("QUA", "WED", "MIE"), gTextClien.Select("QUI", "THU", "JUE"), gTextClien.Select("SEX", "FRI", "VIE"), gTextClien.Select("SAB", "SAT", "SAB") };
	const float gridX = x + 25.0f;
	const float gridY = y + 70.0f;
	const float cellW = 72.0f;
	const float cellH = 43.0f;
	const float gap = 6.0f;
	for (int col = 0; col < 7; ++col)
		RenderTextLine((int)(gridX + col * (cellW + gap)), (int)y + 48, names[col], (int)cellW, RT3_SORT_CENTER, 210, 190, 150);

	int hoveredReward = -1;
	for (int day = 1; day <= m_DaysInMonth; ++day)
	{
		int cell = (m_StartWeekday - 1) + day - 1;
		int row = cell / 7;
		int col = cell % 7;
		float cx = gridX + col * (cellW + gap);
		float cy = gridY + row * (cellH + gap);
		bool claimed = m_ClaimedDays[day - 1] != 0;
		bool today = day == m_Today;
		bool missed = day < m_Today && !claimed;
		// Status discreto: resgatado em verde suave, hoje dourado,
		// perdido em grafite e futuro em cinza escuro.
		if (claimed) gInterface->DrawBarForm(cx, cy, cellW, cellH, 0.05, 0.14, 0.07, 0.92);
		else if (today) gInterface->DrawBarForm(cx, cy, cellW, cellH, 0.35, 0.24, 0.05, 0.95);
		else if (missed) gInterface->DrawBarForm(cx, cy, cellW, cellH, 0.11, 0.11, 0.12, 0.92);
		else gInterface->DrawBarForm(cx, cy, cellW, cellH, 0.07, 0.07, 0.08, 0.90);
		char label[24];
		sprintf_s(label, "%d%s", day, claimed ? " OK" : (missed ? " X" : ""));
		RenderTextLine((int)cx + 4, (int)cy + 3, label, (int)cellW - 8, RT3_SORT_LEFT,
			claimed ? 140 : (today ? 255 : 205), claimed ? 240 : (today ? 215 : 205), claimed ? 120 : (today ? 100 : 205));

		const int rewardIndex = day - 1;
		if (CheckMouseIn(cx, cy, cellW, cellH)) hoveredReward = rewardIndex;

	}

	// Renderize os itens depois de todos os fundos e numa unica passagem 3D.
	// Isso evita dezenas de trocas de camera/depth buffer no mesmo quadro.
	bool hasDailyItem = false;
	for (int day = 1; day <= m_DaysInMonth; ++day)
	{
		const int rewardIndex = day - 1;
		if (m_RewardKind[rewardIndex] == 0 && m_RewardItemType[rewardIndex] != 0xFFFF)
		{
			hasDailyItem = true;
			break;
		}
	}
	if (hasDailyItem) SEASON3B::begin3D();
	for (int day = 1; day <= m_DaysInMonth; ++day)
	{
		const int cell = (m_StartWeekday - 1) + day - 1;
		const int row = cell / 7;
		const int col = cell % 7;
		const float cx = gridX + col * (cellW + gap);
		const float cy = gridY + row * (cellH + gap);
		const int rewardIndex = day - 1;

		if (m_RewardKind[rewardIndex] == 0 && m_RewardItemType[rewardIndex] != 0xFFFF)
		{
			RenderDailyRewardItem(cx + 20.0f, cy + 9.0f, 32.0f, 29.0f,
				m_RewardItemType[rewardIndex], m_RewardLevel[rewardIndex], rewardIndex == hoveredReward);
		}
	}
	if (hasDailyItem) SEASON3B::endrender3D();

	// Quantidades sao 2D e precisam permanecer sobre os modelos.
	for (int day = 1; day <= m_DaysInMonth; ++day)
	{
		const int cell = (m_StartWeekday - 1) + day - 1;
		const int row = cell / 7;
		const int col = cell % 7;
		const float cx = gridX + col * (cellW + gap);
		const float cy = gridY + row * (cellH + gap);
		const int rewardIndex = day - 1;
		char amount[16];
		sprintf_s(amount, "x %u", (unsigned int)m_RewardAmount[rewardIndex]);
		RenderTextLine((int)cx + 39, (int)cy + 27, amount, 29, RT3_SORT_RIGHT, 235, 220, 175);
	}

	const float bonusY = y + 370.0f;
	char progress[64];
	int claimedCount = 0;
	for (int n = 0; n < m_DaysInMonth; ++n) if (m_ClaimedDays[n]) ++claimedCount;
	sprintf_s(progress, gTextClien.Select("ASSIDUIDADE: %d/%d dias%s", "ATTENDANCE: %d/%d days%s", "ASISTENCIA: %d/%d dias%s"), claimedCount, m_DaysInMonth, m_BonusClaimed ? gTextClien.Select(" - RECEBIDO", " - CLAIMED", " - RECIBIDO") : "");
	RenderTextLine((int)x + 25, (int)bonusY + 10, progress, 310, RT3_SORT_LEFT, 230, 200, 130);

	const float rewardPreviewX = x + 375.0f;
	const float btnX = x + 425.0f;
	const float btnY = bonusY;
	const bool bonusButton = m_BonusAvailable != 0;
	const bool dailyButton = m_CanClaimToday != 0;
	const bool buttonEnabled = m_DataLoaded && (dailyButton || bonusButton);
	const bool buttonHover = buttonEnabled && CheckMouseIn(btnX, btnY, 120, 32);
	const bool buttonPressed = buttonHover && ((GetKeyState(VK_LBUTTON) & 0x8000) != 0);
	const float buttonPressY = buttonPressed ? 2.0f : 0.0f;
	gInterface->DrawBarForm(btnX+1.0f,btnY+2.0f,120,32,0.0,0.0,0.0,0.24);
	gInterface->DrawBarForm(btnX,btnY+buttonPressY,120,32,buttonEnabled?0.30:0.14,buttonEnabled?0.20:0.14,buttonEnabled?0.07:0.14,buttonPressed?0.76:0.94);
	gInterface->DrawBarForm(btnX+2.0f,btnY+2.0f+buttonPressY,116,28,buttonHover?0.22:0.09,buttonHover?0.12:0.06,0.025,buttonEnabled?0.90:0.52);
	gInterface->DrawBarForm(btnX+5.0f,btnY+5.0f+buttonPressY,110,22,buttonHover?0.15:0.055,0.035,0.015,buttonEnabled?0.84:0.42);

	if (m_DataLoaded && m_Today >= 1 && m_Today <= m_DaysInMonth)
	{
		const int rewardIndex = m_Today - 1;
		gInterface->DrawBarForm(rewardPreviewX, btnY, 42, 32, 0.08, 0.08, 0.08, 0.92);
		if (m_RewardKind[rewardIndex] == 0 && m_RewardItemType[rewardIndex] != 0xFFFF)
		{
			SEASON3B::begin3D();
			RenderDailyRewardItem(rewardPreviewX + 5.0f, btnY + 2.0f, 32.0f, 27.0f,
				m_RewardItemType[rewardIndex], m_RewardLevel[rewardIndex],
				CheckMouseIn(rewardPreviewX, btnY, 42, 32));
			SEASON3B::endrender3D();
		}
		else
		{
			RenderTextLine((int)rewardPreviewX, (int)btnY + 9, "WC", 42, RT3_SORT_CENTER, 245, 205, 70);
		}
	}
	const char* buttonText = !m_DataLoaded ? gTextClien.Select("CARREGANDO...", "LOADING...", "CARGANDO...") : (bonusButton ? gTextClien.Select("RESGATAR BONUS", "CLAIM BONUS", "RECLAMAR BONUS") : (dailyButton ? gTextClien.Select("RESGATAR HOJE", "CLAIM TODAY", "RECLAMAR HOY") : gTextClien.Select("JA RESGATADO", "ALREADY CLAIMED", "YA RECLAMADO")));
	g_pRenderText->SetFont(g_hFontBold); g_pRenderText->SetBgColor(0,0,0,0); g_pRenderText->SetTextColor(20,5,2,220);
	g_pRenderText->RenderText((int)btnX+1,(int)(btnY+10+buttonPressY),(char*)buttonText,120,14,RT3_SORT_CENTER);
	g_pRenderText->SetTextColor(buttonEnabled?245:145,buttonEnabled?205:145,buttonEnabled?110:145,255);
	g_pRenderText->RenderText((int)btnX,(int)(btnY+9+buttonPressY),(char*)buttonText,120,14,RT3_SORT_CENTER);
	if (CheckMouseIn(btnX, btnY, 120, 32) && IsRelease(VK_LBUTTON))
	{
		if (m_DataLoaded && bonusButton) SendRequest(0x25);
		else if (m_DataLoaded && dailyButton) SendRequest(0x23);
	}

	if (hoveredReward >= 0 && hoveredReward < m_DaysInMonth &&
		m_RewardKind[hoveredReward] == 0 && m_RewardItemType[hoveredReward] != 0xFFFF)
	{
		ITEM tooltipItem;
		ZeroMemory(&tooltipItem, sizeof(tooltipItem));
		tooltipItem.Type = m_RewardItemType[hoveredReward];
		tooltipItem.Level = (BYTE)(m_RewardLevel[hoveredReward] << 3);
		tooltipItem.Durability = (float)max((int)m_RewardAmount[hoveredReward], 1);
		tooltipItem.Option1 = 0;
		tooltipItem.ExtOption = 0;
		g_pNewItemTooltip->RenderItemTooltip(MouseX + 12, MouseY, &tooltipItem, false);
	}
}
