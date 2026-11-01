#include "stdafx.h"
#include "BuildProfileUI.h"
#include "CBInterface.h"
#include "CGMProtect.h"
#include "NewUISystem.h"
#include "UIControls.h"
#include "ZzzInterface.h"
#include "ZzzScene.h"
#include "ZzzTexture.h"
#include "wsclientinline.h"

using namespace SEASON3B;

CBuildProfileUI::CBuildProfileUI()
	: m_Open(false), m_Selected(0), m_LastClick(0), m_TextureAttempted(false), m_TextureLoaded(false), m_IconTextureAttempted(false),
	m_IconX(0.0f), m_IconY(0.0f), m_DragOffsetX(0.0f), m_DragOffsetY(0.0f), m_IconPositioned(false), m_IconDragging(false), m_IconMoved(false)
{
}

CBuildProfileUI* CBuildProfileUI::Instance()
{
	static CBuildProfileUI instance;
	return &instance;
}

bool CBuildProfileUI::Button(float x, float y, float width, float height, const char* text, bool selected, bool textured)
{
	const bool hover = (SEASON3B::CheckMouseIn(x, y, width, height) == 1);
	const DWORD now = GetTickCount();
	const bool pressed = hover && ((GetKeyState(VK_LBUTTON) & 0x8000) != 0);
	const float pressY = pressed ? 2.0f : 0.0f;
	const float inset = textured ? 3.0f : 0.0f;
	const float bx=x+inset, by=y+inset, bw=width-(inset*2.0f), bh=height-(inset*2.0f);

	if (hover)
	{
		gInterface->SetBlockCur(TRUE);
	}

	// Logo-button em metal escuro: sombra curta, filete bronze e centro gunmetal.
	gInterface->DrawBarForm(bx+1.0f,by+2.0f,bw,bh,0.0,0.0,0.0,0.20);
	gInterface->DrawBarForm(bx,by+pressY,bw,bh,selected?0.52:0.25,selected?0.32:0.24,selected?0.07:0.20,pressed?0.76:0.90);
	gInterface->DrawBarForm(bx+2.0f,by+2.0f+pressY,bw-4.0f,bh-4.0f,hover?0.24:0.11,hover?0.25:0.12,hover?0.27:0.13,0.88);

	HFONT buttonFont = (width <= 20.0f && height <= 20.0f) ? g_hFont : g_hFontBold;
	TextDraw(buttonFont, x+1.0f, y+pressY+1.0f+((height-10.0f)*0.5f), 0x161616E0,
		0x00000000, width, 0, 3, text);
	TextDraw(buttonFont, x, y+pressY+((height-10.0f)*0.5f), selected?0xFFE08AFF:(hover?0xE8E2D7FF:0xB9B5ADFF),
		0x00000000, width, 0, 3, text);

	if (hover && (GetKeyState(VK_LBUTTON) & 0x8000) && (now - m_LastClick > 300))
	{
		m_LastClick = now;
		gInterface->SetBlockCurDelay(350);
		PlayBuffer(25, 0, 0);
		return true;
	}

	return false;
}

void CBuildProfileUI::Execute(const char* action)
{
	static const char* profileNames[] = { "pvp", "pve", "pvm" };
	char command[64];
	wsprintf(command, "/build %s %s", action, profileNames[m_Selected]);
	SendChat(command);
}

void CBuildProfileUI::Draw()
{
	if (!gmProtect->BuildProfileEnable)
	{
		m_Open = false;
		return;
	}

	if (SceneFlag != MAIN_SCENE)
	{
		m_Open = false;
		return;
	}

	// O atalho fica sozinho no canto. Se uma janela padrao ocupar a tela,
	// escondemos o atalho para ele nunca ficar desenhado por cima dela.
	bool standardWindowOpen = false;
	// Nao percorra os IDs numericamente: existem lacunas e o ChatWindow fica
	// ativo durante o jogo, o que escondia o atalho B permanentemente.
	static const int blockingWindows[] =
	{
		CB_Interface::FriendList, CB_Interface::MoveList, CB_Interface::Party,
		CB_Interface::Quest, CB_Interface::NPC_Devin, CB_Interface::Guild,
		CB_Interface::Trade, CB_Interface::Warehouse, CB_Interface::ChaosBox,
		CB_Interface::CommandWindow, CB_Interface::PetInfo, CB_Interface::Shop,
		CB_Interface::Inventory, CB_Interface::Store, CB_Interface::OtherStore,
		CB_Interface::Character, CB_Interface::DevilSquare, CB_Interface::BloodCastle,
		CB_Interface::CreateGuild, CB_Interface::GuardNPC, CB_Interface::SeniorNPC,
		CB_Interface::GuardNPC2, CB_Interface::CastleGateSwitch, CB_Interface::CatapultNPC,
		CB_Interface::CrywolfGate, CB_Interface::IllusionTemple, CB_Interface::HeroList,
		CB_Interface::Options, CB_Interface::Help, CB_Interface::SkillTree,
		CB_Interface::GoldenArcher1, CB_Interface::GoldenArcher2,
		CB_Interface::LuckyCoin1, CB_Interface::LuckyCoin2, CB_Interface::NPC_Duel,
		CB_Interface::NPC_Titus, CB_Interface::CashShop, CB_Interface::Lugard,
		CB_Interface::QuestList1, CB_Interface::QuestList2, CB_Interface::Jerint,
		CB_Interface::FullMap, CB_Interface::NPC_Dialog, CB_Interface::GensInfo,
		CB_Interface::NPC_Julia, CB_Interface::NPC_ChaosMix,
		CB_Interface::ExpandInventory, CB_Interface::ExpandWarehouse,
		CB_Interface::MuHelper
	};
	for (int i = 0; i < (int)_countof(blockingWindows); ++i)
	{
		if (gInterface->CheckWindow(blockingWindows[i]))
		{
			standardWindowOpen = true;
			break;
		}
	}
	if (standardWindowOpen)
	{
		m_Open = false;
		return;
	}

	const float iconSize=28.0f;
	if(!m_IconPositioned) { m_IconX=MAX_WIN_WIDTH-39.0f; m_IconY=195.0f; m_IconPositioned=true; }
	if(!m_IconTextureAttempted) { LoadBitmap("Interface\\LauncherLetters\\launcher_b_gunmetal.tga",BITMAP_BUILD_PROFILE_ICON,GL_LINEAR); m_IconTextureAttempted=true; }
	EnableAlphaTest(); glColor4f(1,1,1,1); RenderBitmap(BITMAP_BUILD_PROFILE_ICON,m_IconX,m_IconY,iconSize,iconSize,0,0,1,1,true,true,0);
	bool iconHover=SEASON3B::CheckMouseIn(m_IconX,m_IconY,iconSize,iconSize)==1;
	if(iconHover) gInterface->SetBlockCur(TRUE);
	if(iconHover && SEASON3B::IsPress(VK_LBUTTON)) { m_IconDragging=true; m_IconMoved=false; m_DragOffsetX=MouseX-m_IconX; m_DragOffsetY=MouseY-m_IconY; }
	if(m_IconDragging && SEASON3B::IsRepeat(VK_LBUTTON)) { float nx=MouseX-m_DragOffsetX,ny=MouseY-m_DragOffsetY; if(fabsf(nx-m_IconX)>2.0f || fabsf(ny-m_IconY)>2.0f) m_IconMoved=true; m_IconX=max(0.0f,min(nx,MAX_WIN_WIDTH-iconSize)); m_IconY=max(0.0f,min(ny,MAX_WIN_HEIGHT-iconSize)); }
	if(m_IconDragging && SEASON3B::IsRelease(VK_LBUTTON)) { m_IconDragging=false; if(!m_IconMoved) m_Open=!m_Open; }

	if (!m_Open)
	{
		return;
	}

	const float width = 300.0f;
	const float height = 165.0f;
	const float x = (MAX_WIN_WIDTH - width) * 0.5f;
	const float y = 142.0f;

	if (SEASON3B::CheckMouseIn(x, y, width, height))
	{
		gInterface->SetBlockCur(TRUE);
	}

	if (!m_TextureAttempted)
	{
		m_TextureAttempted = true;
		m_TextureLoaded = LoadBitmap("Interface\\HUD\\build_profile_panel.tga", BITMAP_BUILD_PROFILE_PANEL, GL_LINEAR, GL_CLAMP_TO_EDGE, false);
	}
	if (m_TextureLoaded)
	{
		// The production texture is stored in a 512x256 power-of-two canvas.
		// Its visible 466px panel starts at x=23; sampling the entire canvas
		// squeezed the frame and shifted every recess away from the live buttons.
		const float textureU = 23.0f / 512.0f;
		const float textureUWidth = 466.0f / 512.0f;
		RenderBitmap(BITMAP_BUILD_PROFILE_PANEL, x, y, width, height,
			textureU, 0.0f, textureUWidth, 1.0f, true, true, 1.0f);
	}
	else
	{
		gInterface->DrawBarForm(x, y, width, height, 0.025, 0.028, 0.035, 0.97);
		gInterface->DrawBarForm(x + 4.0f, y + 4.0f, width - 8.0f, 25.0f, 0.18, 0.12, 0.04, 0.98);
	}
	TextDraw(g_hFontBold, x+60.0f, y+20.0f, 0xFFE29AFF, 0x00000000, 170.0f, 0, 3, "PERFIS DE BUILD");

	// Coordinates below follow the actual recesses in the 466x256 useful
	// texture area, scaled to this 300x165 panel.
	if (Button(x+241.0f, y+18.0f, 27.0f, 20.0f, "X", false, m_TextureLoaded))
	{
		m_Open = false;
		return;
	}

	const float slotY = y + 47.0f;
	const float slotWidth = 78.0f;
	const float slotHeight = 36.0f;
	if (Button(x+28.0f, slotY, slotWidth, slotHeight, "PVP", m_Selected==0, m_TextureLoaded)) m_Selected=0;
	if (Button(x+111.0f, slotY, slotWidth, slotHeight, "PVE", m_Selected==1, m_TextureLoaded)) m_Selected=1;
	if (Button(x+194.0f, slotY, slotWidth, slotHeight, "PVM", m_Selected==2, m_TextureLoaded)) m_Selected=2;

	static const char* selectedText[] = { "PVP selecionado", "PVE selecionado", "PVM selecionado" };
	TextDraw(g_hFontBold, x+92.0f, y+92.0f, 0xFFE08AFF, 0x00000000,
		116.0f, 0, 3, selectedText[m_Selected]);

	const float actionY = y + 117.0f;
	const float actionHeight = 32.0f;
	if (Button(x+28.0f, actionY, slotWidth, actionHeight, "SALVAR", false, m_TextureLoaded)) Execute("salvar");
	if (Button(x+111.0f, actionY, slotWidth, actionHeight, "ATIVAR", false, m_TextureLoaded)) Execute("usar");
	if (Button(x+194.0f, actionY, slotWidth, actionHeight, "APAGAR", false, m_TextureLoaded)) Execute("apagar");
}
