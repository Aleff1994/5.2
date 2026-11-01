// NewUIGuildInfoWindow.cpp: implementation of the CNewUIGuildInfoWindow class.
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "BoundedPacketText.h"

#include "NewUIMiniMap.h"
#include "NewUISystem.h"
#include "NewUICommonMessageBox.h"
#include "NewUICustomMessageBox.h"
#include "DSPlaySound.h"
#include "wsclientinline.h"
#include "NewUIGuildInfoWindow.h"
#include "NewUIButton.h"
#include "NewUIMyInventory.h"
#include "CSitemOption.h"
#include "MapManager.h"
#include "ZzzAI.h"
#include "ZzzLodTerrain.h"
#include "ZzzInterface.h"
#include "ScaleForm.h"
#include "TextClien.h"
#include "Protocol.h"
#include "CGMCharacter.h"

extern bool CheckCommand(char* Text, bool bMacroText);

using namespace SEASON3B;

extern int TargetX;
extern int TargetY;
extern BYTE m_OccupationState;
extern WORD TerrainWall[TERRAIN_SIZE * TERRAIN_SIZE];
namespace
{
	bool IsFullMapLook()
	{
		return (gmProtect && (gmProtect->LookAndFeel == 0 || gmProtect->LookAndFeel == 3 || gmProtect->LookAndFeel == 4));
	}

	bool IsPhotoMapLook()
	{
		return false; // Reservado para o mapa projetado antigo; o modo atual usa atlas plano.
	}

	bool IsClassicMapLook()
	{
		// Look 0 intentionally shares Look 2 atlas loading, map projection,
		// marker placement and click-coordinate conversion.
		return (gmProtect && (gmProtect->LookAndFeel == 1 || gmProtect->LookAndFeel == 2));
	}

	bool IsFlatMapLook()
	{
		return IsFullMapLook() || IsClassicMapLook();
	}

	bool UsesLook0MapFrame()
	{
		// A moldura ornamental original fica somente nos Looks 1/2.
		// Os Looks 0/3/4 usam o painel plano do mapa completo.
		return IsClassicMapLook();
	}

	// Looks 0/3/4 use the complete completo expanded-map panel.  Looks 1/2
	// keep the existing compact/classic presentation unchanged.
	bool UsesFullMapFrame()
	{
		return IsFullMapLook();
	}

	bool IsCleanMapLook()
	{
		return (gmProtect && gmProtect->LookAndFeel >= 1 && gmProtect->LookAndFeel <= 2);
	}

	void RenderMiniMapCircle(float x, float y, float radius, float red, float green, float blue, float alpha)
	{
		EnableAlphaBlend();
		glDisable(GL_TEXTURE_2D);
		glColor4f(red, green, blue, alpha);
		glBegin(GL_TRIANGLE_FAN);
		glVertex2f(x, y);
		for (int n = 0; n <= 16; ++n)
		{
			const float radians = (6.28318530718f * (float)n) / 16.0f;
			glVertex2f(x + cosf(radians) * radius, y + sinf(radians) * radius);
		}
		glEnd();
		glEnable(GL_TEXTURE_2D);
		glColor4f(1.f, 1.f, 1.f, 1.f);
	}

	// The classic atlas is 512x512 and has a four-pixel ornamental border.
	// Keep that border proportional to the rendered panel instead of applying
	// four screen pixels at every resolution.
	const float CLASSIC_ATLAS_SIZE = 512.0f;
	const float CLASSIC_ATLAS_BORDER = 4.0f;
	struct CLASSIC_MAP_CALIBRATION
	{
		float WorldMinX;
		float WorldMaxX;
		float WorldMinY;
		float WorldMaxY;
		float AtlasLeft;
		float AtlasTop;
		float AtlasRight;
		float AtlasBottom;
	};

	CLASSIC_MAP_CALIBRATION ClassicGetMapCalibration()
	{
		CLASSIC_MAP_CALIBRATION calibration = { 0.0f, 255.0f, 0.0f, 255.0f, 0.0f, 0.0f, 1.0f, 1.0f };
		// The approved colored atlases already use the game's complete 0..255
		// coordinate plane. Keep Stadium, Icarus and Kanturu Remain on that
		// same plane; cropping the image here applies the alignment twice.
		return calibration;
	}

	bool ClassicWorldToRender(float fx,float fy,float sx,float sy,int wx,int wy,float* rx,float* ry)
	{
		if(rx==NULL||ry==NULL||wx<0||wx>=TERRAIN_SIZE||wy<0||wy>=TERRAIN_SIZE)return false;
		const float insetX=sx*(CLASSIC_ATLAS_BORDER/CLASSIC_ATLAS_SIZE);
		const float insetY=sy*(CLASSIC_ATLAS_BORDER/CLASSIC_ATLAS_SIZE);
		const float ux=sx-(insetX*2.0f),uy=sy-(insetY*2.0f);
		if(ux<=0.0f||uy<=0.0f)return false;
		const CLASSIC_MAP_CALIBRATION calibration = ClassicGetMapCalibration();
		if(wx<calibration.WorldMinX||wx>calibration.WorldMaxX||wy<calibration.WorldMinY||wy>calibration.WorldMaxY)return false;
		const float worldWidth=(calibration.WorldMaxX-calibration.WorldMinX)+1.0f;
		const float worldHeight=(calibration.WorldMaxY-calibration.WorldMinY)+1.0f;
		// Render at the centre of the terrain cell. This keeps all 256 cells
		// aligned and makes the inverse cursor conversion an exact round trip.
		const float atlasX=calibration.AtlasLeft+((((wx-calibration.WorldMinX)+0.5f)/worldWidth)*(calibration.AtlasRight-calibration.AtlasLeft));
		const float atlasY=calibration.AtlasTop+((((calibration.WorldMaxY-wy)+0.5f)/worldHeight)*(calibration.AtlasBottom-calibration.AtlasTop));
		*rx=fx+insetX+(atlasX*ux);
		*ry=fy+insetY+(atlasY*uy);
		return true;
	}
	bool ClassicRenderToWorld(float fx,float fy,float sx,float sy,float rx,float ry,int* wx,int* wy)
	{
		if(wx==NULL||wy==NULL)return false;
		const float insetX=sx*(CLASSIC_ATLAS_BORDER/CLASSIC_ATLAS_SIZE);
		const float insetY=sy*(CLASSIC_ATLAS_BORDER/CLASSIC_ATLAS_SIZE);
		const float ux=sx-(insetX*2.0f),uy=sy-(insetY*2.0f);
		const float lx=rx-(fx+insetX),ly=ry-(fy+insetY);
		if(ux<=0.0f||uy<=0.0f||lx<0.0f||ly<0.0f||lx>ux||ly>uy)return false;
		const CLASSIC_MAP_CALIBRATION calibration = ClassicGetMapCalibration();
		const float atlasX=lx/ux;
		const float atlasY=ly/uy;
		if(atlasX<calibration.AtlasLeft||atlasX>calibration.AtlasRight||atlasY<calibration.AtlasTop||atlasY>calibration.AtlasBottom)return false;
		const float normalizedX=(atlasX-calibration.AtlasLeft)/(calibration.AtlasRight-calibration.AtlasLeft);
		const float normalizedY=(atlasY-calibration.AtlasTop)/(calibration.AtlasBottom-calibration.AtlasTop);
		const float worldWidth=(calibration.WorldMaxX-calibration.WorldMinX)+1.0f;
		const float worldHeight=(calibration.WorldMaxY-calibration.WorldMinY)+1.0f;
		*wx=(int)(calibration.WorldMinX+(normalizedX*worldWidth));
		*wy=(int)(calibration.WorldMaxY-(normalizedY*worldHeight)+1.0f);
		if(*wx>(int)calibration.WorldMaxX)*wx=(int)calibration.WorldMaxX;
		if(*wy>(int)calibration.WorldMaxY)*wy=(int)calibration.WorldMaxY;
		return(*wx>=0&&*wx<TERRAIN_SIZE&&*wy>=0&&*wy<TERRAIN_SIZE);
	}

	// Conversao direta da grade 0..255 para personagens, NPCs e mobs.
	// O ponto usa diretamente a grade 0..255: X cresce para a direita e Y e invertido.
	// Nao aplicar inset/borda do atlas aqui, senao os marcadores vivos se deslocam.
	bool DirectWorldToRender(float fx,float fy,float sx,float sy,int wx,int wy,float* rx,float* ry)
	{
		if (!rx || !ry || wx < 0 || wy < 0 || wx >= TERRAIN_SIZE || wy >= TERRAIN_SIZE) return false;
		*rx = fx + (wx * (sx / 256.0f));
		*ry = fy + ((255 - wy) * (sy / 256.0f));
		return true;
	}

	bool DirectRenderToWorld(float fx,float fy,float sx,float sy,float rx,float ry,int* wx,int* wy)
	{
		if (!wx || !wy || sx <= 0.f || sy <= 0.f || rx < fx || ry < fy || rx >= fx + sx || ry >= fy + sy) return false;
		*wx = (int)((rx - fx) * 256.f / sx);
		*wy = 255 - (int)((ry - fy) * 256.f / sy);
		return *wx >= 0 && *wx < 256 && *wy >= 0 && *wy < 256;
	}

	// Os NaviMap dos Looks 0/3/4 possuem o plano do terreno cerca de 2,73%
	// abaixo do atlas geometrico. Mantem marcador, NPCs e clique no mesmo lugar.
	bool FlatWorldToRender(float fx,float fy,float sx,float sy,int wx,int wy,float* rx,float* ry)
	{
		if(!ClassicWorldToRender(fx,fy,sx,sy,wx,wy,rx,ry)) return false;
		if(IsPhotoMapLook()) *ry += sy * 0.0273f;
		return true;
	}

	bool FlatRenderToWorld(float fx,float fy,float sx,float sy,float rx,float ry,int* wx,int* wy)
	{
		if(IsPhotoMapLook()) ry -= sy * 0.0273f;
		return ClassicRenderToWorld(fx,fy,sx,sy,rx,ry,wx,wy);
	}
}

SEASON3B::CNewUIMiniMap::CNewUIMiniMap()
{
	m_pNewUIMng = NULL;

	m_Lenth[0].x = 800;
	m_Lenth[1].x = 1000;
	m_Lenth[2].x = 1200;
	m_Lenth[3].x = 1400;
	m_Lenth[4].x = 1600;
	m_Lenth[5].x = 1800;
	m_Lenth[0].y = 800;
	m_Lenth[1].y = 1000;
	m_Lenth[2].y = 1200;
	m_Lenth[3].y = 1400;
	m_Lenth[4].y = 1600;
	m_Lenth[5].y = 1800;
	m_MiniPos = 0;
	m_bSuccess = false;


	m_TooltipText = "";
	m_TooltipTextColor = -1;

	m_Move = false;
	m_Map = -1;
	m_MoveX = -1;
	m_MoveY = -1;

	m_State = true;

	this->runtime_move();
}

SEASON3B::CNewUIMiniMap::~CNewUIMiniMap()
{
	Release();
}

bool SEASON3B::CNewUIMiniMap::Create(CNewUIManager* pNewUIMng, int x, int y)
{
	if (NULL == pNewUIMng)
		return false;

	m_pNewUIMng = pNewUIMng;
	m_pNewUIMng->AddUIObj(SEASON3B::INTERFACE_MINI_MAP, this);

	this->LoadImages();

#if MAIN_UPDATE > 303
	m_BtnExit.ChangeButtonImgState(true, IMAGE_MINIMAP_INTERFACE + 6, false);
	m_BtnExit.ChangeButtonInfo(x + 610, y + 3, 85, 85);
	m_BtnExit.ChangeToolTipText(GlobalText[1002], false);

	this->SetPos(x, y);
#endif

	this->Zoom(3.f);

	Show(false);

	return true;
}

void SEASON3B::CNewUIMiniMap::Release()
{
	UnloadImages();

	(&m_WayPoint)->clear();

	if (m_pNewUIMng)
	{
		m_pNewUIMng->RemoveUIObj(this);
		m_pNewUIMng = NULL;
	}
}

void SEASON3B::CNewUIMiniMap::LoadImages()
{
	// Esses arquivos são carregados pelo pacote de interface original do Main.
	// O loader desta versão não suporta OZT/OZJ diretamente.
	LoadBitmap("Interface\\mini_map_ui_corner.tga", IMAGE_MINIMAP_INTERFACE + 1, GL_LINEAR);
	LoadBitmap("Interface\\mini_map_ui_line.jpg", IMAGE_MINIMAP_INTERFACE + 2, GL_LINEAR);
	LoadBitmap("Interface\\mini_map_ui_cha.tga", IMAGE_MINIMAP_INTERFACE + 3, GL_LINEAR);
	LoadBitmap("Interface\\mini_map_ui_portal.tga", IMAGE_MINIMAP_INTERFACE + 4, GL_LINEAR);
	LoadBitmap("Interface\\mini_map_ui_npc.tga", IMAGE_MINIMAP_INTERFACE + 5, GL_LINEAR);
	LoadBitmap("Interface\\mini_map_ui_cancel.tga", IMAGE_MINIMAP_INTERFACE + 6, GL_LINEAR);
	LoadBitmap("Interface\\HUD\\minimap_frame_neon.tga", IMAGE_MINIMAP_CLASSIC_FRAME, GL_LINEAR);
	LoadBitmap("FullMiniMap\\map_radar.tga", IMAGE_MINIMAP_CUSTOM_MARKERS, GL_LINEAR);
}

void SEASON3B::CNewUIMiniMap::UnloadImages()
{
	DeleteBitmap(IMAGE_MINIMAP_INTERFACE);
	DeleteBitmap(IMAGE_MINIMAP_INTERFACE + 1);
	DeleteBitmap(IMAGE_MINIMAP_INTERFACE + 2);
	DeleteBitmap(IMAGE_MINIMAP_INTERFACE + 3);
	DeleteBitmap(IMAGE_MINIMAP_INTERFACE + 4);
	DeleteBitmap(IMAGE_MINIMAP_INTERFACE + 5);
	DeleteBitmap(IMAGE_MINIMAP_INTERFACE + 6);
	DeleteBitmap(IMAGE_MINIMAP_CLASSIC_FRAME);
	DeleteBitmap(IMAGE_MINIMAP_CUSTOM_MARKERS);
}

float SEASON3B::CNewUIMiniMap::GetLayerDepth()
{
	return 8.1f;
}

void SEASON3B::CNewUIMiniMap::SetPos(int x, int y)
{
#if MAIN_UPDATE > 303
	m_BtnExit.ChangeButtonInfo(GetWindowsX - 27, 3, 30, 25);
#endif
}

void SEASON3B::CNewUIMiniMap::SetBtnPos(int Num, float x, float y, float nx, float ny)
{
	m_Btn_Loc[Num][0] = x;
	m_Btn_Loc[Num][1] = y;
	m_Btn_Loc[Num][2] = nx;
	m_Btn_Loc[Num][3] = ny;
}

bool SEASON3B::CNewUIMiniMap::UpdateKeyEvent()
{
	if (g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_MINI_MAP))
	{
		if (IsPress(VK_ESCAPE) == true || IsPress(VK_TAB) == true)
		{
			g_pNewUISystem->Hide(SEASON3B::INTERFACE_MINI_MAP);
			PlayBuffer(SOUND_CLICK01);
			return false;
		}
	}

	return true;
}

bool SEASON3B::CNewUIMiniMap::Render()
{
	if (g_pOption != NULL && !g_pOption->IsMiniMapEnabled()) return true;
	EnableAlphaTest();
	glColor4f(1.f, 1.f, 1.f, 1.f);

	if (m_bSuccess)
	{
		runtime_render_map(true);
	}

	DisableAlphaBlend();

	return m_bSuccess;
}

bool SEASON3B::CNewUIMiniMap::Update()
{
	return true;
}

void SEASON3B::CNewUIMiniMap::LoadImages(const char* Filename)
{
	char Fname[300];
	if (IsFullMapLook())
	{
		// O completo usa o atlas plano Data\\Custom\\Maps\\WorldN.ozt.
		// Exclusivo dos Looks 0/3/4; os Looks 1/2 preservam seus mapas.
		sprintf_s(Fname, "Data\\FullMiniMap\\World%d.ozt", World + 1);
		FILE* fullMapSource = fopen(Fname, "rb");
		if (fullMapSource != NULL)
		{
			fclose(fullMapSource);
			DeleteBitmap(IMAGE_MINIMAP_INTERFACE);
			sprintf_s(Fname, "FullMiniMap\\World%d.tga", World + 1);
			LoadBitmap(Fname, IMAGE_MINIMAP_INTERFACE, GL_LINEAR);
			m_bSuccess = true;
			return;
		}
		// Atlas dedicado aos Looks completos; Looks 1/2 continuam com seus mapas originais.
		sprintf_s(Fname, "Data\\FullMiniMap\\World%d.ozt", World + 1);
		FILE* fullMapFile = fopen(Fname, "rb");
		if (fullMapFile != NULL)
		{
			fclose(fullMapFile);
			DeleteBitmap(IMAGE_MINIMAP_INTERFACE);
			sprintf_s(Fname, "FullMiniMap\\World%d.tga", World + 1);
			LoadBitmap(Fname, IMAGE_MINIMAP_INTERFACE, GL_LINEAR);
			m_bSuccess = true;
			return;
		}
		// Nem todo mapa possui atlas dedicado. Prefere o mapa plano correspondente
		// world map for those worlds; never stretch a projected NaviMap as an atlas.
		sprintf_s(Fname, "Data\\%s\\mini_map.ozt", Filename);
		fullMapFile = fopen(Fname, "rb");
		if (fullMapFile != NULL)
		{
			fclose(fullMapFile);
			DeleteBitmap(IMAGE_MINIMAP_INTERFACE);
			sprintf_s(Fname, "%s\\mini_map.tga", Filename);
			LoadBitmap(Fname, IMAGE_MINIMAP_INTERFACE, GL_LINEAR);
			m_bSuccess = true;
			return;
		}
	}

	// Look 1/2 uses the colored classic map as a flat atlas. Projection is not
	// applied to this image: applying it a second time was what distorted roads,
	// holes, markers and click coordinates on screen.
	const bool classicLook = IsClassicMapLook();
	if (classicLook)
	{
		sprintf(Fname, "Data\\MiniMap\\map%d.ozj", World + 1);
		FILE* coloredMap = fopen(Fname, "rb");
		if (coloredMap != NULL)
		{
			fclose(coloredMap);
#if MAIN_UPDATE > 303
			g_pItemEnduranceInfo->HideInterfaceWorld(false);
#endif
			DeleteBitmap(IMAGE_MINIMAP_INTERFACE);
			sprintf(Fname, "MiniMap\\map%d.jpg", World + 1);
			LoadBitmap(Fname, IMAGE_MINIMAP_INTERFACE, GL_LINEAR);
			m_bSuccess = true;
			return;
		}
	}

	sprintf(Fname, "Data\\Interface\\HUD\\NaviMap\\Navimap%02d.ozt", World + 1);
	FILE* pFile = fopen(Fname, "rb");

#if MAIN_UPDATE > 303
	g_pItemEnduranceInfo->HideInterfaceWorld(false);
#endif
	DeleteBitmap(IMAGE_MINIMAP_INTERFACE);

	if (pFile == NULL)
	{
		// Classic clients store the real map image inside Data\\WorldXX\\mini_map.OZT.
		sprintf(Fname, "Data\\%s\\mini_map.ozt", Filename);
		pFile = fopen(Fname, "rb");
		if (pFile == NULL)
		{
			m_bSuccess = false;
			return;
		}
		fclose(pFile);
		sprintf(Fname, "%s\\mini_map.tga", Filename);
		LoadBitmap(Fname, IMAGE_MINIMAP_INTERFACE, GL_LINEAR);
		m_bSuccess = true;
	}
	else
	{
		fclose(pFile);
		sprintf(Fname, "Interface\\HUD\\NaviMap\\Navimap%02d.tga", World + 1);
		LoadBitmap(Fname, IMAGE_MINIMAP_INTERFACE, GL_LINEAR);
		m_bSuccess = true;
	}

#if MAIN_UPDATE > 303
#ifdef CHINESE_LANGUAGE
	sprintf(Fname, "Data\\Local\\Minimap\\Minimap_%s.bmd", Filename);
#else
	sprintf(Fname, "Data\\Local\\%s\\Minimap\\Minimap_%s_%s.bmd", g_strSelectedML.c_str(), Filename, g_strSelectedML.c_str());
#endif // CHINESE_LANGUAGE
#else
	sprintf(Fname, "Data\\Local\\Minimap\\Minimap_%s.bmd", Filename);
#endif

	m_MinimapData.clear();

	FILE* fp = fopen(Fname, "rb");

	if (fp != NULL)
	{
		int Size = sizeof(MINI_MAP);
		BYTE* Buffer = new BYTE[Size * MAX_MINI_MAP_DATA + 45];
		fread(Buffer, (Size * MAX_MINI_MAP_DATA) + 45, 1, fp);

		DWORD dwCheckSum;
		fread(&dwCheckSum, sizeof(DWORD), 1, fp);
		fclose(fp);

		if (dwCheckSum != GenerateCheckSum2(Buffer, (Size * MAX_MINI_MAP_DATA) + 45, 0x2BC1))
		{
			char Text[256];
			sprintf(Text, "%s - File corrupted.", Fname);
			g_ErrorReport.Write(Text);
			MessageBox(gwinhandle->GethWnd(), Text, NULL, MB_OK);
			SendMessage(gwinhandle->GethWnd(), WM_DESTROY, 0, 0);
		}
		else
		{
			BYTE* pSeek = Buffer;

			for (int i = 0; i < MAX_MINI_MAP_DATA; i++)
			{
				MINI_MAP info;
				BuxConvert(pSeek, Size);
				memcpy(&info, pSeek, Size);

				if (info.Name[0] != '\0' && info.Kind > 0 && info.Kind < 7)
				{
					m_MinimapData.push_back(info);
				}
				pSeek += Size;
			}
		}

		delete[] Buffer;
	}
}

bool SEASON3B::CNewUIMiniMap::IsSuccess()
{
	return m_bSuccess;
}

bool SEASON3B::CNewUIMiniMap::UpdateMouseEvent()
{
	if (g_pOption != NULL && !g_pOption->IsMiniMapEnabled()) return true;
	const bool flatLook = IsFlatMapLook();
	if (flatLook && IsPress(VK_LBUTTON)
		&& MouseRenderX >= (m_RenderFrameX + m_RenderSizeX - 34.f)
		&& MouseRenderX <= (m_RenderFrameX + m_RenderSizeX)
		&& MouseRenderY >= m_RenderFrameY
		&& MouseRenderY <= (m_RenderFrameY + 38.f))
	{
		g_pNewUISystem->Hide(SEASON3B::INTERFACE_MINI_MAP);
		PlayBuffer(SOUND_CLICK01);
		return false;
	}

	if (this->runtime_update_target())
	{
		return false;
	}

	return true;



#if MAIN_UPDATE > 303
	bool ret = true;

	if (m_BtnExit.UpdateMouseEvent() == true)
	{
		g_pNewUISystem->Hide(SEASON3B::INTERFACE_MINI_MAP);
		return true;
	}

	if (IsPress(VK_LBUTTON))
	{
		ret = Check_Mouse(MouseX, MouseY);
		if (ret == false)
		{
			PlayBuffer(SOUND_CLICK01);
		}

	}

	if (CheckMouseIn(0, 0, GetWindowsX, GetWindowsY))
	{
		return false;
	}
	return ret;
#endif
}

bool SEASON3B::CNewUIMiniMap::Check_Mouse(int mx, int my)
{
	return true;
}

bool SEASON3B::CNewUIMiniMap::Check_Btn(int mx, int my)
{
#if MAIN_UPDATE > 303
	for (int i = 0; i < MAX_MINI_MAP_DATA && m_MinimapData[i].Kind > 0; i++)
	{
		if (mx > m_Btn_Loc[i][0] && mx < (m_Btn_Loc[i][0] + m_Btn_Loc[i][2]) && my > m_Btn_Loc[i][1] && my < (m_Btn_Loc[i][1] + m_Btn_Loc[i][3]))
		{
			SIZE Fontsize;
			m_TooltipText = (unicode::t_string)m_MinimapData[i].Name;
			g_pRenderText->SetFont(g_hFont);
			g_pMultiLanguage->_GetTextExtentPoint32(g_pRenderText->GetFontDC(), m_TooltipText.c_str(), m_TooltipText.size(), &Fontsize);

			Fontsize.cx = Fontsize.cx / g_fScreenRate_x;
			Fontsize.cy = Fontsize.cy / g_fScreenRate_y;

			int x = m_Btn_Loc[i][0] + ((m_Btn_Loc[i][2] / 2) - (Fontsize.cx / 2));
			int y = m_Btn_Loc[i][1] + m_Btn_Loc[i][3] + 2;

			y = m_Btn_Loc[i][1] - (Fontsize.cy + 2);

			DWORD backuptextcolor = g_pRenderText->GetTextColor();
			DWORD backuptextbackcolor = g_pRenderText->GetBgColor();

			g_pRenderText->SetTextColor(RGBA(255, 255, 255, 255));
			g_pRenderText->SetBgColor(RGBA(0, 0, 0, 180));
			g_pRenderText->RenderText(x, y, m_TooltipText.c_str(), Fontsize.cx + 6, 0, RT3_SORT_CENTER);

			g_pRenderText->SetTextColor(backuptextcolor);
			g_pRenderText->SetBgColor(backuptextbackcolor);

			return true;
		}
	}
#endif
	return false;
}

bool SEASON3B::CNewUIMiniMap::movement_automatic()
{
	if (this->IsMoving())
	{
		if (this->m_Map != World || Hero->Dead != 0)
		{
			this->StopMove();
			return false;
		}

		auto pos = (&m_WayPoint)->front();

		float distance = sqrt(pow((float)(Hero->PositionX - pos.first), 2) + pow((float)(Hero->PositionY - pos.second), 2));

		if (distance < 2)
		{
			(&m_WayPoint)->pop_front();

			if (!(&m_WayPoint)->empty())
			{
				pos = (&m_WayPoint)->front();

				// Do not resend movement while the character is already walking. Repeated
				// SendMove packets made map navigation look unnaturally fast/jumpy.
				if (Hero->Movement == 0 && PathFinding2(Hero->PositionX, Hero->PositionY, pos.first, pos.second, &Hero->Path, 0.f))
				{
					Hero->Movement = true;
					SendMove(Hero, &Hero->Object);
				}

				gfxNotice::addLongMovementText(gTextClien.TextClien_Khac[2], 20, 150, 2000); //"Auto-Moving"
			}
		}
		else if (distance >= 12.f)
		{
			this->StopMove();
		}
		else if (Hero->Movement == 0 && PathFinding2(Hero->PositionX, Hero->PositionY, pos.first, pos.second, &Hero->Path, 0.f))
		{
			if (checkNormalizer)
			{
				Hero->Movement = true;
				SendMove(Hero, &Hero->Object);
				gfxNotice::addLongMovementText(gTextClien.TextClien_Khac[2], 20, 150, 2000); //"Auto-Moving"
			}
		}

		return false;
	}

	return true;
}

void SEASON3B::CNewUIMiniMap::OpenningProcess()
{
	if (IsFlatMapLook())
	{
		CStreamPacketEngine packet;
		packet.Init(0xC1, 0xFA);
		packet << (BYTE)0x01;
		packet.Send();
	}
}

void SEASON3B::CNewUIMiniMap::ClosingProcess()
{
}

bool SEASON3B::CNewUIMiniMap::IsMoving()
{
	return !(&m_WayPoint)->empty();
}

void SEASON3B::CNewUIMiniMap::StopMove()
{
	if (this->IsMoving())
	{
		(&m_WayPoint)->clear();

		if (Hero->Movement)
		{
			SetPlayerStop(Hero);
		}
	}
}

float SEASON3B::CNewUIMiniMap::GetZoom()
{
	return this->m_RenderZoom;
}

void SEASON3B::CNewUIMiniMap::Zoom(float value)
{
	// Below 3x the projected map no longer covers the framed viewport and the
	// game world becomes visible around it. Keep the useful zoom range only.
	this->m_RenderZoom = max(3.f, min(this->m_RenderZoom + value, 6.f));
}

void SEASON3B::CNewUIMiniMap::runtime_move()
{
	if (this->IsVisible())
	{
		const bool photoLook = IsPhotoMapLook();
		const bool compact = IsFlatMapLook();
		if (compact)
		{
			// Reference layout: a large square atlas in the center, leaving enough
			// of the 3D world visible around it to preserve gameplay context.
			const float available = min((float)WindowWidth - 250.f, (float)WindowHeight - 190.f);
			// Looks 0/3/4: a moldura acompanha o mapa, um pouco maior sem ocupar
			// toda a tela. O recorte continua limitado ao interior dela.
			const float mapSize = max(340.f, min((UsesLook0MapFrame() || IsFullMapLook()) ? 490.f : 620.f, available));
			this->m_RenderSizeX = mapSize;
			this->m_RenderSizeY = mapSize;
			this->m_RenderFrameX = ((float)WindowWidth - this->m_RenderSizeX) * 0.5f;
			this->m_RenderFrameY = ((float)WindowHeight - this->m_RenderSizeY) * 0.5f;
		}
		else
		{
			const float width = GetWindowsX - 140.f;
			const float height = GetWindowsY - 166.f;
			this->m_RenderSizeX = width * g_fScreenRate_x;
			this->m_RenderSizeY = height * g_fScreenRate_y;
			this->m_RenderFrameX = ((GetWindowsX - width) * 0.5f) * g_fScreenRate_x;
			this->m_RenderFrameY = ((GetWindowsY - height) * 0.5f) * g_fScreenRate_y;
		}
	}
	else
	{
		this->m_RenderFrameX = (GetWindowsX - 128.f) * g_fScreenRate_x;
		this->m_RenderFrameY = (15.f * g_fScreenRate_y);
		this->m_RenderSizeX = (128.f * g_fScreenRate_x);
		this->m_RenderSizeY = (128.f * g_fScreenRate_y);
	}
}

bool SEASON3B::CNewUIMiniMap::runtime_update_target()
{
	if (SEASON3B::CheckMouseIn(this->m_RenderFrameX, this->m_RenderFrameY, this->m_RenderSizeX, this->m_RenderSizeY, false))
	{
		if (MouseWheel != 0)
		{
			this->Zoom(0.5f * ((MouseWheel < 0) ? -1 : 1));
			MouseWheel = 0;
		}

#if MAIN_UPDATE > 303
		if (this->IsVisible() && SEASON3B::IsRelease(VK_LBUTTON))
		{
			int Tx = 0, Ty = 0;
			const float mouseTx = MouseRenderX - (this->m_RenderFrameX + (this->m_RenderSizeX * 0.5f));
			const float mouseTy = MouseRenderY - (this->m_RenderFrameY + (this->m_RenderSizeY * 0.5f));
			if (this->runtime_calc_target(mouseTx, mouseTy, &Tx, &Ty))
			{
				this->StopMove();
				// Always ask the GameServer about minimap teleport.  The server is the
				// authority and rejects normal accounts.  This also covers administrators
				// registered in GameMaster.txt whose CtlCode was not mirrored to the client.
				this->SendGMTeleportRequest((BYTE)Tx, (BYTE)Ty);

				if (!IsGMCharacter())
				{
					// Normal players retain click-move while the unauthorized teleport
					// request is safely ignored by the GameServer.
					this->m_MoveX = Tx;
					this->m_MoveY = Ty;
					this->m_Map = World;
					this->FindPath(Hero->PositionX, Hero->PositionY, Tx, Ty);
				}
			}
			return true;
		}
#else
		if (SEASON3B::IsRelease(VK_LBUTTON))
		{
			if (this->IsMoving())
			{
				this->StopMove();
			}
			else
			{
				int Tx = 0, Ty = 0;

				const float mouseTx = MouseRenderX - (this->m_RenderFrameX + (this->m_RenderSizeX * 0.5f));
				const float mouseTy = MouseRenderY - (this->m_RenderFrameY + (this->m_RenderSizeY * 0.5f));

				if (this->runtime_calc_target(mouseTx, mouseTy, &Tx, &Ty))
				{
					this->m_MoveX = Tx;
					this->m_MoveY = Ty;
					this->m_Map = World;

					gfxNotice::addLongMovementText("Auto-Moving", 20, 150, 2000);
					this->FindPath(Hero->PositionX, Hero->PositionY, this->m_MoveX, this->m_MoveY);
				}
			}
		}
		return true;
#endif
	}
	return false;
}

void SEASON3B::CNewUIMiniMap::runtime_render_map(bool rendername)
{
	float Matrix[3][4];
	vec3_t rot, p1, p2, angle;

	this->runtime_move();

	glEnable(GL_SCISSOR_TEST);
	if(UsesFullMapFrame() || UsesLook0MapFrame())
	{
		// O mapa ocupa todo o painel e a arte da moldura e desenhada por cima.
		// O mesmo retangulo e usado nos modos grande e pequeno, eliminando as
		// margens pretas sem alterar a escala dos marcadores ou dos cliques.
		glScissor((GLsizei)this->m_RenderFrameX,
			(GLsizei)(WindowHeight-(this->m_RenderFrameY+this->m_RenderSizeY)),
			(GLsizei)this->m_RenderSizeX,(GLsizei)this->m_RenderSizeY);
	}
	else
	{
		glScissor((GLsizei)(this->m_RenderFrameX - 14.f),
			(GLsizei)(WindowHeight - (this->m_RenderFrameY + this->m_RenderSizeY + 24.f)),
			(GLsizei)(this->m_RenderSizeX + 28.f), (GLsizei)(this->m_RenderSizeY + 38.f));
	}

	// The color atlas must be fully opaque. Besides improving readability, this
	// prevents characters/effects from bleeding through the map.

	float _ratio = (MAX_MAP_SIZE / (float)(TERRAIN_SIZE * TERRAIN_SCALE));
	// A abertura ornamental e menor que o painel externo. Nos Looks 0/3/4,
	// reduz levemente somente o atlas projetado para ele caber com folga.
	const float renderZoom = this->m_RenderZoom * (IsPhotoMapLook() ? 1.15f : 1.0f);

	p1[0] = ((-Hero->GetPositionY()) * _ratio) * renderZoom;
	p1[1] = ((-Hero->GetPositionX()) * _ratio) * renderZoom;
	p1[2] = 0.f;

	Vector(40.f, 45.f, -30.f, rot);

	AngleMatrix(rot, Matrix);
	VectorRotate(p1, Matrix, p2);

	Vector(40.f, -45.f, 30.f, angle);

	const float Tx = this->m_RenderFrameX + (this->m_RenderSizeX * 0.5f);
	const float Ty = this->m_RenderFrameY + (this->m_RenderSizeY * 0.5f);
	const bool classicLook = IsClassicMapLook();
	const bool fullMapLook = IsFullMapLook();
	const bool photoLook = IsPhotoMapLook();
	const bool flatLook = classicLook || fullMapLook || photoLook;

	if (classicLook || fullMapLook)
	{
		// Solid plate immediately behind the atlas. Some legacy OZJ loaders keep
		// blending enabled even for JPEG data; this guarantees a non-transparent map.
		glColor4f(0.018f, 0.020f, 0.024f, 1.0f);
		RenderColor(this->m_RenderFrameX, this->m_RenderFrameY,
			this->m_RenderSizeX, this->m_RenderSizeY, 0.0, false, false);
		EndRenderColor();
		DisableAlphaBlend();
		glColor4f(1.f, 1.f, 1.f, 1.f);
		RenderBitmap(IMAGE_MINIMAP_INTERFACE, this->m_RenderFrameX, this->m_RenderFrameY,
			this->m_RenderSizeX, this->m_RenderSizeY, 0.f, 0.f, 1.f, 1.f, false, false, 1.0f);
		EnableAlphaBlend();
	}
	else if (photoLook)
	{
		// Restaura a projecao original: o mapa acompanha o personagem e a
		// coordenada visual permanece sincronizada com o terreno do jogo.
		glColor4f(0.008f,0.012f,0.014f,1.0f);
		RenderColor(this->m_RenderFrameX,this->m_RenderFrameY,
			this->m_RenderSizeX,this->m_RenderSizeY,0.0,false,false);
		EndRenderColor();
		RenderBitmapLocalProjection(IMAGE_MINIMAP_INTERFACE,Tx+p2[0],Ty+p2[1],
			512.f*renderZoom,512.f*renderZoom,angle,0.f,0.f,1.f,1.f,false);
	}
	else
	{
		RenderBitmapLocalProjection(IMAGE_MINIMAP_INTERFACE, Tx + p2[0], Ty + p2[1], (512 * this->m_RenderZoom), (512 * this->m_RenderZoom), angle, 0.f, 0.f, 1.f, 1.f, false);
	}

    // Nearest-four NPC markers; portals and nearby mobs have separate paths.
    int npcCount=0;
    for(size_t i=0;i<m_DynamicMapData.size() && npcCount<4;++i)
    {
        if(m_DynamicMapData[i].Kind!=5)continue;
        runtime_render_objet(npcCount,m_DynamicMapData[i].Location[0],m_DynamicMapData[i].Location[1],1,5,0.f,m_DynamicMapData[i].Name,rot);
        ++npcCount;
    }

    // Portal icons are independent from the maximum-four NPC selection.
    for(size_t i=0;i<m_DynamicMapData.size();++i){
        const MINI_MAP& portal=m_DynamicMapData[i];
        if(portal.Kind!=4)continue;
        runtime_render_objet(-1,portal.Location[0],portal.Location[1],1,4,0.f,portal.Name,rot);
    }

	// Preserve nearby live mob markers; NPCs use the separate maximum-four list.
	// Usa o gerenciador de personagens nativo desta source.
	if (this->IsVisible() && flatLook)
	{
		for (int index = 0; index < MAX_CHARACTERS_CLIENT; ++index)
		{
			CHARACTER* character = gmCharacters->GetCharacter(index);
			if (character == NULL)
			{
				continue;
			}
			OBJECT* object = &character->Object;

			if (!object->Live || object->Kind != KIND_MONSTER)
			{
				continue;
			}

			if (character->PositionX < 0 || character->PositionX >= TERRAIN_SIZE ||
				character->PositionY < 0 || character->PositionY >= TERRAIN_SIZE)
			{
				continue;
			}

			float markerX = 0.0f;
			float markerY = 0.0f;
			if (!DirectWorldToRender(m_RenderFrameX, m_RenderFrameY, m_RenderSizeX, m_RenderSizeY,
				character->PositionX, character->PositionY, &markerX, &markerY))
			{
				continue;
			}

			if (markerX < m_RenderFrameX || markerX > m_RenderFrameX + m_RenderSizeX ||
				markerY < m_RenderFrameY || markerY > m_RenderFrameY + m_RenderSizeY)
			{
				continue;
			}

			const bool monster = (object->Kind == KIND_MONSTER);
			if (fullMapLook || classicLook)
			{
				// Mesmo sistema em todos os Looks planos: vermelho para mob/boss
				// e verde para NPC, inclusive nos Looks classicos 1 e 2.
				EnableAlphaTest();
				glColor4f(1.f, 1.f, 1.f, 1.f);
				RenderBitmapRotate(IMAGE_MINIMAP_CUSTOM_MARKERS,
					markerX, markerY, 10.f, 10.f, 0.f,
					0.1349999905f, monster ? 0.f : 0.06599999219f,
					0.1030000001f, 0.05200000107f, false);
			}
			else
			{
				RenderMiniMapCircle(markerX, markerY, 5.0f, 0.02f, 0.02f, 0.02f, 0.95f);
				RenderMiniMapCircle(markerX, markerY, 3.2f,
					monster ? 1.00f : 0.12f,
					monster ? 0.10f : 1.00f,
					monster ? 0.08f : 0.22f, 1.00f);
			}

			if (MouseRenderX >= markerX - 6.f && MouseRenderX <= markerX + 6.f &&
				MouseRenderY >= markerY - 6.f && MouseRenderY <= markerY + 6.f)
			{
				char tooltip[160];
				sprintf_s(tooltip, "%s (%d, %d)", character->ID, character->PositionX, character->PositionY);
				g_pRenderText->SetTextColor(CLRDW_WHITE);
				g_pRenderText->SetBgColor(0, 0, 0, 210);
				g_pRenderText->RenderText((int)((markerX + 9.f) / g_fScreenRate_x),
					(int)((markerY - 14.f) / g_fScreenRate_y), tooltip,
					(int)(180.f / g_fScreenRate_x), 0, RT3_SORT_LEFT);
			}
		}
	}


	glDisable(GL_SCISSOR_TEST);

	if (flatLook)
	{
		// Moldura unica, reta e fixa, incluindo cabecalho e rodape.
		const float outerX=this->m_RenderFrameX-6.f;
		const float outerY=this->m_RenderFrameY-20.f;
		const float outerW=this->m_RenderSizeX+12.f;
		const float outerH=this->m_RenderSizeY+38.f;
		if(!UsesFullMapFrame())
		{
		EnableAlphaBlend();
		glColor4f(0.025f, 0.027f, 0.031f, 0.98f);
		RenderColor(this->m_RenderFrameX - 6.f, this->m_RenderFrameY - 20.f,
			this->m_RenderSizeX + 12.f, 20.f, 0.0, false, false);
		RenderColor(this->m_RenderFrameX - 6.f, this->m_RenderFrameY + this->m_RenderSizeY,
			this->m_RenderSizeX + 12.f, 18.f, 0.0, false, false);
		RenderColor(this->m_RenderFrameX - 6.f, this->m_RenderFrameY,
			6.f, this->m_RenderSizeY, 0.0, false, false);
		RenderColor(this->m_RenderFrameX + this->m_RenderSizeX,
			this->m_RenderFrameY, 6.f, this->m_RenderSizeY, 0.0, false, false);
		// Filete interno bronze, discreto e alinhado ao limite do mapa.
		glColor4f(0.40f,0.43f,0.45f,1.0f);
		RenderColor(this->m_RenderFrameX - 2.f, this->m_RenderFrameY - 2.f,
			this->m_RenderSizeX + 4.f, 2.f, 0.0, false, false);
		RenderColor(this->m_RenderFrameX - 2.f, this->m_RenderFrameY + this->m_RenderSizeY,
			this->m_RenderSizeX + 4.f, 2.f, 0.0, false, false);
		RenderColor(this->m_RenderFrameX - 2.f, this->m_RenderFrameY,
			2.f, this->m_RenderSizeY, 0.0, false, false);
		RenderColor(this->m_RenderFrameX + this->m_RenderSizeX,
			this->m_RenderFrameY, 2.f, this->m_RenderSizeY, 0.0, false, false);
		glColor4f(0.075f,0.08f,0.095f,1.0f);
		RenderColor(outerX,outerY,outerW,2.f,0.0,false,false);
		RenderColor(outerX,outerY+outerH-2.f,outerW,2.f,0.0,false,false);
		RenderColor(outerX,outerY,2.f,outerH,0.0,false,false);
		RenderColor(outerX+outerW-2.f,outerY,2.f,outerH,0.0,false,false);
		glColor4f(0.48f,0.51f,0.53f,1.0f);
		RenderColor(outerX,outerY,12.f,3.f,0.0,false,false);
		RenderColor(outerX+outerW-12.f,outerY,12.f,3.f,0.0,false,false);
		RenderColor(outerX,outerY+outerH-3.f,12.f,3.f,0.0,false,false);
		RenderColor(outerX+outerW-12.f,outerY+outerH-3.f,12.f,3.f,0.0,false,false);
		EndRenderColor();
		}
		if(UsesFullMapFrame())
		{
			EnableAlphaTest(); glColor4f(1.f,1.f,1.f,1.f);
			RenderBitmapLocalRotate2(IMAGE_MINIMAP_CLASSIC_FRAME,
				this->m_RenderFrameX+(this->m_RenderSizeX*0.5f),
				this->m_RenderFrameY+(this->m_RenderSizeY*0.5f),
				this->m_RenderSizeX+20.f,this->m_RenderSizeY+20.f,0.f,
				0.f,0.f,1.f,1.f,false);
		}

		// The compact HUD minimap already has the map name and coordinates in its
		// footer.  Draw this yellow title only in the expanded map window.
		if (rendername)
		{
			g_pRenderText->SetTextColor(245, 201, 92, 255);
			g_pRenderText->SetBgColor(0, 0, 0, 0);
			const float titleX = this->m_RenderFrameX;
			const float titleWidth = this->m_RenderSizeX;
			g_pRenderText->RenderText((int)(titleX / g_fScreenRate_x),
				(int)(((UsesFullMapFrame()?this->m_RenderFrameY+7.f:this->m_RenderFrameY-17.f)) / g_fScreenRate_y), gMapManager->GetMapName(World),
				(int)(titleWidth / g_fScreenRate_x), 0, RT3_SORT_CENTER);
			if (MouseRenderX >= m_RenderFrameX && MouseRenderX < m_RenderFrameX + m_RenderSizeX &&
				MouseRenderY >= m_RenderFrameY && MouseRenderY < m_RenderFrameY + m_RenderSizeY)
			{
				const float mx = MouseRenderX - (m_RenderFrameX + m_RenderSizeX * 0.5f);
				const float my = MouseRenderY - (m_RenderFrameY + m_RenderSizeY * 0.5f);
				int coordinateX = 0, coordinateY = 0;
				bool hasCoordinates = false;
				if (classicLook || fullMapLook)
				{
					hasCoordinates = fullMapLook
						? DirectRenderToWorld(m_RenderFrameX, m_RenderFrameY, m_RenderSizeX, m_RenderSizeY,
							MouseRenderX, MouseRenderY, &coordinateX, &coordinateY)
						: ClassicRenderToWorld(m_RenderFrameX, m_RenderFrameY, m_RenderSizeX, m_RenderSizeY,
							MouseRenderX, MouseRenderY, &coordinateX, &coordinateY);
				}
				else
				{
					// Invert the same 2D projection used by the atlas and live dots.
					// This is constant-time; the click pathfinder is intentionally not run every frame.
					float matrix[3][4];
					vec3_t projectionAngle;
					Vector(40.f, 45.f, -30.f, projectionAngle);
					AngleMatrix(projectionAngle, matrix);
					const float determinant = matrix[0][0] * matrix[1][1] - matrix[0][1] * matrix[1][0];
					const float pixelScale = TERRAIN_SCALE * (MAX_MAP_SIZE / (float)(TERRAIN_SIZE * TERRAIN_SCALE)) * renderZoom;
					if (fabsf(determinant) > 0.0001f && pixelScale > 0.f)
					{
						const float projectedY = (mx * matrix[1][1] - my * matrix[0][1]) / determinant;
						const float projectedX = (my * matrix[0][0] - mx * matrix[1][0]) / determinant;
						coordinateX = (int)(Hero->PositionX + projectedX / pixelScale);
						coordinateY = (int)(Hero->PositionY + projectedY / pixelScale);
						hasCoordinates = coordinateX >= 0 && coordinateX < TERRAIN_SIZE && coordinateY >= 0 && coordinateY < TERRAIN_SIZE;
					}
				}
				if (hasCoordinates)
				{
					char coordinates[64];
					sprintf_s(coordinates, gTextClien.Select("Coordenadas: %d, %d", "Coordinates: %d, %d", "Coordenadas: %d, %d"), coordinateX, coordinateY);
					g_pRenderText->SetTextColor(75, 240, 235, 255);
					g_pRenderText->RenderText((int)((m_RenderFrameX + 10.f) / g_fScreenRate_x),
						(int)((m_RenderFrameY + 7.f) / g_fScreenRate_y), coordinates,
						(int)(180.f / g_fScreenRate_x), 0, RT3_SORT_LEFT);
				}
			}
		}
	}

	float Rot = (((BYTE)((Hero->Object.Angle[2] + 22.5f) / 360.f * 8.f + 1.f) % 8) + 2) * 45.0;
	float heroX = Tx;
	float heroY = Ty;
	// A seta do jogador usa a mesma grade dos pontos vivos. Assim seta, NPC,
	// mob e boss nunca ficam em sistemas de coordenadas diferentes.
	if (flatLook && !DirectWorldToRender(m_RenderFrameX,m_RenderFrameY,m_RenderSizeX,m_RenderSizeY,
		Hero->PositionX,Hero->PositionY,&heroX,&heroY))
	{
		heroX=Tx;
		heroY=Ty;
	}
	// The stock minimap arrow rotates with the character in every flat-map look.
	if (flatLook)
	{
		// Dedicated character marker: smaller than the animated atlas cursor and
		// transparent, avoiding the white square visible in Looks 1/2/3.
		EnableAlphaTest();
		glColor4f(1.f, 1.f, 1.f, 1.f);
		RenderBitmapLocalRotate2(IMAGE_MINIMAP_INTERFACE + 3, heroX, heroY,
			14.f, 14.f, Rot, 0.f, 0.f, 1.f, 1.f, false);
	}
	else if (photoLook)
	{
		RenderFrameAnimation2(BITMAP_ITEM_ENDURANCE_INFO_BEGIN + 13, Tx, Ty,
			32.f,32.f,Rot,32.0/512.0,32.0/128.0,1.25,10,30,false);
	}
	else
	{
		RenderFrameAnimation2(BITMAP_ITEM_ENDURANCE_INFO_BEGIN + 13, heroX, heroY,
			32.f, 32.f, Rot, 32.0 / 512.0, 32.0 / 128.0, 1.25, 10, 30, false);
	}
	if (flatLook)
	{
		char mapPosition[80];
		// Sempre mostra a posicao real do personagem, a mesma usada pela seta.
		sprintf_s(mapPosition, "%s   X: %d   Y: %d", gMapManager->GetMapName(World), Hero->PositionX, Hero->PositionY);
		g_pRenderText->SetTextColor(255, 238, 144, 255);
		g_pRenderText->SetBgColor(0, 0, 0, 220);
		const float footerX = this->m_RenderFrameX;
		const float footerWidth = this->m_RenderSizeX;
		g_pRenderText->RenderText((int)(footerX / g_fScreenRate_x),
			(int)(((UsesFullMapFrame()?this->m_RenderFrameY+this->m_RenderSizeY-19.f:this->m_RenderFrameY+this->m_RenderSizeY+4.f)) / g_fScreenRate_y), mapPosition,
			(int)(footerWidth / g_fScreenRate_x), 0, RT3_SORT_CENTER);
	}
	//RenderBitmapRotate(IMAGE_MINIMAP_INTERFACE + 1, Tx, Ty, 40.f, 40.f, Hero->Object.Angle[2] + 135.f, 0.f, 0.f, 32.f / 64.f, 32.f / 256.f, false);
}

void SEASON3B::CNewUIMiniMap::ReceiveDynamicMapInfo(BYTE* packet, int size)
{
	if (packet == NULL || size < (int)sizeof(PMSG_DYNAMIC_MINIMAP_HEAD_RECV))
	{
		return;
	}

	PMSG_DYNAMIC_MINIMAP_HEAD_RECV* head = (PMSG_DYNAMIC_MINIMAP_HEAD_RECV*)packet;
	if (head->map != (BYTE)World)
	{
		return;
	}
	if (head->reset != 0)
	{
		m_DynamicMapData.clear();
	}

	const int available = (size - sizeof(PMSG_DYNAMIC_MINIMAP_HEAD_RECV)) / sizeof(PMSG_DYNAMIC_MINIMAP_ENTRY_RECV);
	const int count = min((int)head->count, available);
	PMSG_DYNAMIC_MINIMAP_ENTRY_RECV* entry = (PMSG_DYNAMIC_MINIMAP_ENTRY_RECV*)(packet + sizeof(PMSG_DYNAMIC_MINIMAP_HEAD_RECV));
	for (int i = 0; i < count; ++i)
	{
        if((entry[i].kind!=1 && entry[i].kind!=2) || m_DynamicMapData.size()>=68)continue;
        size_t sameKind=0;
        for(const auto& marker:m_DynamicMapData)if(marker.Kind==(entry[i].kind==1?5:4))++sameKind;
        if(sameKind>=static_cast<size_t>(entry[i].kind==1?4:64))continue;
		MINI_MAP info = {};
		info.Kind = entry[i].kind==1?5:4;
		info.Location[0] = entry[i].x;
		info.Location[1] = entry[i].y;
		BoundedPacketText::Copy(info.Name, entry[i].text);
		m_DynamicMapData.push_back(info);
	}
}

void SEASON3B::CNewUIMiniMap::SendGMTeleportRequest(BYTE x, BYTE y)
{
	PMSG_MINIMAP_GM_TELEPORT_SEND pMsg = {};
	pMsg.header.set(0xFA, 0x03, sizeof(pMsg));
	pMsg.map = (BYTE)World;
	pMsg.x = x;
	pMsg.y = y;
	DataSend((BYTE*)&pMsg, pMsg.header.size);
}

void SEASON3B::CNewUIMiniMap::runtime_render_objet(int i, int x, int y, int group, int type, float angle, const char* name, const vec3_t angles)
{
	vec3_t p1, p2;
	float Matrix[3][4];

	if (group == 0 && type == 0)
	{
		type = 7;
	}

	const float su = (32.f / 64.f) * ((type - 1) % 2);
	const float sv = (32.f / 256.f) + ((32.f / 256.f) * ((type - 1) / 2));
	const bool classicLook = IsClassicMapLook();
	const bool fullMapLook = IsFullMapLook();
	if (classicLook || fullMapLook)
	{
		float mapX=0.0f,mapY=0.0f;
		const bool positioned = fullMapLook
			? DirectWorldToRender(m_RenderFrameX,m_RenderFrameY,m_RenderSizeX,m_RenderSizeY,x,y,&mapX,&mapY)
			: ClassicWorldToRender(m_RenderFrameX,m_RenderFrameY,m_RenderSizeX,m_RenderSizeY,x,y,&mapX,&mapY);
		if (!positioned) return;
		// Keep only navigation markers (NPC/portal) and the original route icon.
		// All three use the stock MU atlas so no square/custom cursor is introduced.
		if (type != 4 && type != 5 && type != 6)return;
		EnableAlphaTest();
		glColor4f(1.f, 1.f, 1.f, 1.f);
        if(type==4){
            RenderBitmapRotate(IMAGE_MINIMAP_INTERFACE+4,mapX,mapY,13.f,13.f,0.f,0.f,0.f,1.f,1.f,false);
        }
        else if (type == 5 && (fullMapLook || classicLook))
		{
			// NPC dinamico usa o mesmo marcador verde dos Looks completos.
			RenderBitmapRotate(IMAGE_MINIMAP_CUSTOM_MARKERS,
				mapX, mapY, 10.f, 10.f, 0.f,
				0.1349999905f, 0.06599999219f,
				0.1030000001f, 0.05200000107f, false);
		}
		else
		{
			RenderBitmapLocalRotate2(IMAGE_MINIMAP_INTERFACE + 1, mapX, mapY,
				(type == 6) ? 14.f : 18.f, (type == 6) ? 14.f : 18.f,
				angle, su, sv, 32.f / 64.f, 32.f / 256.f, false);
		}
		if (name && name[0] && MouseRenderX >= mapX - 11.f && MouseRenderX <= mapX + 11.f && MouseRenderY >= mapY - 11.f && MouseRenderY <= mapY + 11.f)
		{
			char tooltip[160];
			// The object name comes from the active client/server language data.
			// Avoid hard-coded category words so changing Lang changes the tooltip too.
			sprintf_s(tooltip, "%s (%d, %d)", name, x, y);
			g_pRenderText->SetTextColor(CLRDW_WHITE);
			g_pRenderText->SetBgColor(0, 0, 0, 210);
			g_pRenderText->RenderText((int)((mapX + 12.f) / g_fScreenRate_x),
				(int)((mapY - 10.f) / g_fScreenRate_y), tooltip,
				(int)(190.f / g_fScreenRate_x), 0, RT3_SORT_LEFT);
		}
		return;
	}
	const float renderZoom = m_RenderZoom * (IsPhotoMapLook() ? 1.15f : 1.0f);
	const float ratio = (MAX_MAP_SIZE / (float)(TERRAIN_SIZE * TERRAIN_SCALE)) * renderZoom;

	p1[0] = (((y * TERRAIN_SCALE) - Hero->GetPositionY()) * ratio);
	p1[1] = (((x * TERRAIN_SCALE) - Hero->GetPositionX()) * ratio);
	p1[2] = 0.f;

	AngleMatrix(angles, Matrix);
	VectorRotate(p1, Matrix, p2);

	const float Tx = this->m_RenderFrameX + (this->m_RenderSizeX * 0.5f) + p2[0];
	const float Ty = this->m_RenderFrameY + (this->m_RenderSizeY * 0.5f) + p2[1];

	// Scissoring hides markers outside the minimap but does not avoid their CPU
	// and draw-call cost. Reject them before submitting any OpenGL work.
	const float markerHalfSize = 13.f;
	if (Tx < this->m_RenderFrameX - markerHalfSize ||
		Tx > this->m_RenderFrameX + this->m_RenderSizeX + markerHalfSize ||
		Ty < this->m_RenderFrameY - markerHalfSize ||
		Ty > this->m_RenderFrameY + this->m_RenderSizeY + markerHalfSize)
	{
		return;
	}

	// NPC: ponto verde pequeno e limpo. Portais e demais marcadores continuam
	// usando os simbolos originais do atlas.
    if(type==4){
        RenderBitmapLocalRotate2(IMAGE_MINIMAP_INTERFACE+4,Tx,Ty,16.f,16.f,0.f,0.f,0.f,1.f,1.f,false);
    }
    else if (IsPhotoMapLook() && type == 5)
	{
		RenderMiniMapCircle(Tx,Ty,4.2f,0.01f,0.10f,0.02f,1.0f);
		RenderMiniMapCircle(Tx,Ty,2.8f,0.18f,1.00f,0.24f,1.0f);
	}
	else
	{
		RenderBitmapLocalRotate2(IMAGE_MINIMAP_INTERFACE + 1, Tx, Ty, 26.f, 26.f, angle, su, sv, 32.f / 64.f, 32.f / 256.f, false);
	}

	if (name && name[0])
	{
		if (i >= 0 && i < MAX_MINI_MAP_DATA)
		{
			m_Btn_Loc[i][0] = (Tx);
			m_Btn_Loc[i][1] = (Ty - 25.f);
			m_Btn_Loc[i][2] = 26.f;
			m_Btn_Loc[i][3] = 26.f;
		}
		if (MouseRenderX >= Tx - 13.f && MouseRenderX <= Tx + 13.f && MouseRenderY >= Ty - 13.f && MouseRenderY <= Ty + 13.f)
		{
			char tooltip[160];
			sprintf_s(tooltip, "%s (%d, %d)", name, x, y);
			g_pRenderText->SetTextColor(CLRDW_WHITE);
			g_pRenderText->SetBgColor(0, 0, 0, 210);
			g_pRenderText->RenderText((int)Tx + 14, (int)Ty - 12, tooltip, 190, 0, RT3_SORT_LEFT);
		}
	}
}

void SEASON3B::CNewUIMiniMap::runtime_render_name(int i, const char* name)
{
	if (name && name[0])
	{
		g_pRenderText->RenderTextClipped(m_Btn_Loc[i][0], m_Btn_Loc[i][1], name, 0, 0, 8, 0);
	}
}

bool SEASON3B::CNewUIMiniMap::runtime_calc_target(float mx, float my, int* Tx, int* Ty)
{
	const bool classicLook = IsClassicMapLook();
	const bool fullMapLook = IsFullMapLook();
	if (classicLook || fullMapLook)
	{
		const float renderX=m_RenderFrameX+(m_RenderSizeX*0.5f)+mx;
		const float renderY=m_RenderFrameY+(m_RenderSizeY*0.5f)+my;
		int mapX=0,mapY=0;
		const bool positioned = fullMapLook
			? DirectRenderToWorld(m_RenderFrameX,m_RenderFrameY,m_RenderSizeX,m_RenderSizeY,renderX,renderY,&mapX,&mapY)
			: ClassicRenderToWorld(m_RenderFrameX,m_RenderFrameY,m_RenderSizeX,m_RenderSizeY,renderX,renderY,&mapX,&mapY);
		if (!positioned) return false;
		int targetX = mapX;
		int targetY = mapY;
		// Preserve the exact atlas coordinate. FindPath validates normal-player
		// movement and the GameServer independently authorizes GM/ADM teleport.
		// Moving the target to a nearby tile here desynchronizes Stadium/Icarus
		// from the point selected on the minimap.
		(*Tx) = targetX;
		(*Ty) = targetY;
		return true;
	}

	float matrix[3][4];
	vec3_t p1, p2, r;

	Vector(40.f, 45.f, -30.f, r);
	AngleMatrix(r, matrix);

	const float renderZoom = m_RenderZoom * (IsPhotoMapLook() ? 1.15f : 1.0f);
	const float ratio = TERRAIN_SCALE * (MAX_MAP_SIZE / (float)(TERRAIN_SIZE * TERRAIN_SCALE)) * renderZoom;

	float bestDistance = 999999.0f;
	int bestX = -1;
	int bestY = -1;

	for (int y = 0; y < TERRAIN_SIZE; y++)
	{
		for (int x = 0; x < TERRAIN_SIZE; x++)
		{
			p1[0] = (y - Hero->PositionY) * ratio;
			p1[1] = (x - Hero->PositionX) * ratio;
			p1[2] = 0.f;

			VectorRotate(p1, matrix, p2);

			const float dx = mx - p2[0];
			const float dy = my - p2[1];
			const float distance = (dx * dx) + (dy * dy);

			if (distance < bestDistance)
			{
				bestDistance = distance;
				bestX = x;
				bestY = y;
			}
		}
	}

	// Reject clicks that are outside the projected map and coordinates that the
	// terrain itself marks as blocked/no-ground. This keeps movement and GM
	// teleport on the same walkable cells used by the game.
	if (bestX < 0 || bestY < 0 || bestDistance > 64.0f)
	{
		return false;
	}

	const WORD attribute = TerrainWall[(bestY * TERRAIN_SIZE) + bestX];
	if ((attribute & TW_NOMOVE) == TW_NOMOVE || (attribute & TW_NOGROUND) == TW_NOGROUND)
	{
		return false;
	}

	(*Tx) = bestX;
	(*Ty) = bestY;
	return true;
}
