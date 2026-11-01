///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "UIManager.h"
#include "GuildCache.h"
#include "ZzzOpenglUtil.h"
#include "ZzzBMD.h"
#include "ZzzInfomation.h"
#include "ZzzObject.h"
#include "ZzzCharacter.h"
#include "ZzzLodTerrain.h"
#include "ZzzInterface.h"
#include "ZzzInventory.h"
#include "ZzzTexture.h"
#include "ZzzOpenData.h"
#include "ZzzScene.h"
#include "ZzzEffect.h"
#include "ZzzAI.h"
#include "DSPlaySound.h"
#include "wsclientinline.h"
#include "SMD.h"
#include "Local.h"
#include "MatchEvent.h"
#include "PhysicsManager.h"
#include "./Utilities/Log/ErrorReport.h"
#include "CSQuest.h"
#include "PersonalShopTitleImp.h"
#include "uicontrols.h"
#include "GOBoid.h"
#include "GMHellas.h"
#include "CSItemOption.h"
#include "GMBattleCastle.h"
#include "GMHuntingGround.h"
#include "GMAida.h"
#include "GMCrywolf1st.h"
#include "npcBreeder.h"
#include "CSPetSystem.h"
#include "GIPetManager.h"
#include "CComGem.h"
#include "UIMapName.h"	// rozy
#include "./Time/Timer.h"
#include "Input.h"
#include "UIMng.h"
#include "LoadingScene.h"
#include "CDirection.h"
#include "GM_Kanturu_3rd.h"
#include "Renderer/RenderBackend.h"
#include "Renderer/Modern/ModernRenderBackend.h"
#include "Renderer/Modern/RenderStats.h"
#include "Renderer/Modern/ShadowRenderer.h"
#include "Renderer/Modern/PostProcess.h"
#include <algorithm>
#include <vector>
#undef min
#ifdef MOVIE_DIRECTSHOW
#include <dshow.h>
#include "MovieScene.h"
#endif // MOVIE_DIRECTSHOW
#include "Event.h"
#include "./Utilities/Log/muConsoleDebug.h"
#include "MixMgr.h"
#include "GameCensorship.h"
#include "GM3rdChangeUp.h"
#include "NewUISystem.h"
#include "NewUICommonMessageBox.h"
#include "PartyManager.h"
#include "w_CursedTemple.h"
#include "CameraMove.h"
#include "w_MapHeaders.h"
#include "w_PetProcess.h"
#include "PortalMgr.h"
#include "ServerListManager.h"
#include "ProtocolSend.h"
#include "MapManager.h"
#include "CGMProtect.h"
#include "CAIController.h"
#include "CameraManager.h"
#include "CGMHeadChat.h"
#include "ConnectVersionHex.h"

// Defined by ZzzLodTerrain.cpp; this source does not export the array in its header.
extern vec3_t TerrainNormal[TERRAIN_SIZE * TERRAIN_SIZE];
extern float RealRenderFPS;
extern bool IsLorenciaRainPuddle(float x, float y);
extern void AddLorenciaRainRipple(float x, float y);
extern void UpdateRealRenderFPSCounter();

static void RenderMuHelperRangeRing(float radius, float red, float green, float blue)
{
	const float rangeScale = (radius / TERRAIN_SCALE) * 2.0f;
	const float rotation = (float)(timeGetTime() % 18000) * (360.0f / 18000.0f);
	const float pulse = 0.78f + sinf((float)timeGetTime() * 0.0022f) * 0.12f;

	vec3_t outerLight;
	Vector(red * pulse, green * pulse, blue * pulse, outerLight);
	RenderTerrainAlphaBitmap(BITMAP_MAGIC_CIRCLE,
		Hero->Object.Position[0], Hero->Object.Position[1],
		rangeScale, rangeScale, outerLight, rotation, 0.72f, 1.5f);

	vec3_t innerLight;
	Vector(1.0f, 0.86f, 0.38f, innerLight);
	RenderTerrainAlphaBitmap(BITMAP_MAGIC_CIRCLE,
		Hero->Object.Position[0], Hero->Object.Position[1],
		rangeScale * 0.94f, rangeScale * 0.94f, innerLight,
		-rotation * 0.72f, 0.38f, 1.8f);
}
static void RenderMuHelperRanges()
{
	static bool rangeTextureLoaded = false;
	if (!rangeTextureLoaded)
	{
		LoadBitmap("Effect\\mhoujin_R.jpg", BITMAP_MAGIC_CIRCLE, GL_LINEAR, GL_CLAMP_TO_EDGE);
		rangeTextureLoaded = true;
	}
	if (Hero == NULL || g_pNewUISystem == NULL ||
		!g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_MACRO_OFICIAL))
	{
		return;
	}

	SEASON3B::CNewUIMacroMain* macroWindow = g_pNewUISystem->GetUI_pNewUIMacroWindow();
	if (macroWindow == NULL)
	{
		return;
	}

	const bool pickupTab = (macroWindow->GetCurrentTab() == 1);
	const int range = pickupTab ? macroWindow->GetPickupRange() : macroWindow->GetHuntingRange();
	if (range < 1 || range > 8)
	{
		return;
	}

	const float radius = (float)range * 100.0f;

	glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_LINE_BIT | GL_DEPTH_BUFFER_BIT | GL_TEXTURE_BIT);
	glEnable(GL_TEXTURE_2D);
	glDisable(GL_LIGHTING);
	glDisable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_BLEND);
	// Blend aditivo: o fundo preto da textura nao aparece.
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	glDepthMask(GL_FALSE);
	if (pickupTab)
	{
		// Obtaining: circulo dourado luminoso, conforme a referencia.
		RenderMuHelperRangeRing(radius, 1.00f, 0.82f, 0.28f);
	}
	else
	{
		// Hunting: circulo dourado luminoso, conforme a referencia.
		RenderMuHelperRangeRing(radius, 1.00f, 0.82f, 0.28f);
	}

	glDepthMask(GL_TRUE);
	glPopAttrib();
}

extern CUITextInputBox* g_pSingleTextInputBox;
extern CUITextInputBox* g_pSinglePasswdInputBox;
extern int g_iChatInputType;
extern BOOL g_bUseChatListBox;
extern DWORD g_dwMouseUseUIID;
extern DWORD g_dwActiveUIID;
extern DWORD g_dwKeyFocusUIID;
extern CUIMapName* g_pUIMapName;
extern bool HighLight;
extern CTimer* g_pTimer;

#ifdef MOVIE_DIRECTSHOW
extern CMovieScene* g_pMovieScene;
#endif // MOVIE_DIRECTSHOW


bool	g_bTimeCheck = false;
int 	g_iBackupTime = 0;

float	g_fMULogoAlpha = 0;


// extern CGuildCache g_GuildCache;

extern float g_fSpecialHeight;

short   g_shCameraLevel = 0;

int g_iLengthAuthorityCode = 20;

WORD g_ServerPort = 44405;
char* szServerIpAddress = "192.168.0.104";

#ifdef MOVIE_DIRECTSHOW
int  SceneFlag = MOVIE_SCENE;
#else // MOVIE_DIRECTSHOW
int  SceneFlag = WEBZEN_SCENE;
#endif // MOVIE_DIRECTSHOW

int MoveSceneFrame = 0;
int frame_scene_desplace = 0;
extern int g_iKeyPadEnable;


CPhysicsManager g_PhysicsManager;


char* g_lpszMp3[NUM_MUSIC] =
{
	"data\\music\\Pub.mp3",
	"data\\music\\Mutheme.mp3",
	"data\\music\\Church.mp3",
	"data\\music\\Devias.mp3",
	"data\\music\\Noria.mp3",
	"data\\music\\Dungeon.mp3",
	"data\\music\\atlans.mp3",
	"data\\music\\icarus.mp3",
	"data\\music\\tarkan.mp3",
	"data\\music\\lost_tower_a.mp3",
	"data\\music\\lost_tower_b.mp3",
	"data\\music\\kalima.mp3",
	"data\\music\\castle.mp3",
	"data\\music\\charge.mp3",
	"data\\music\\lastend.mp3",
	"data\\music\\huntingground.mp3",
	"data\\music\\Aida.mp3",
	"data\\music\\crywolf1st.mp3",
	"data\\music\\crywolf_ready-02.ogg",
	"data\\music\\crywolf_before-01.ogg",
	"data\\music\\crywolf_back-03.ogg",
	"data\\music\\main_theme.mp3",
	"data\\music\\kanturu_1st.mp3",
	"data\\music\\kanturu_2nd.mp3",
	"data\\music\\KanturuMayaBattle.mp3",
	"data\\music\\KanturuNightmareBattle.mp3",
	"data\\music\\KanturuTower.mp3",
	"data\\music\\BalgasBarrack.mp3",
	"data\\music\\BalgasRefuge.mp3",
	"data\\music\\cursedtemplewait.mp3",
	"data\\music\\cursedtempleplay.mp3",
	"data\\music\\elbeland.mp3",
	"data\\music\\login_theme.mp3",
	"data\\music\\SwampOfCalmness.mp3",
	"data\\music\\Raklion.mp3",
	"data\\music\\Raklion_Hatchery.mp3",
	"data\\music\\Santa_Village.mp3",
	"data\\music\\DuelArena.mp3",
	"data\\music\\PK_Field.mp3",
	"data\\music\\ImperialGuardianFort.mp3",
	"data\\music\\ImperialGuardianFort.mp3",
	"data\\music\\ImperialGuardianFort.mp3",
	"data\\music\\ImperialGuardianFort.mp3",
	"data\\music\\iDoppelganger.mp3",
	"data\\music\\iDoppelganger.mp3",
	"data\\music\\Karutan_A.mp3",
	"data\\music\\Karutan_B.mp3",
};

extern char Mp3FileName[256];

#define MAX_LENGTH_CMB	( 38)

DWORD   g_dwWaitingStartTick;
int     g_iRequestCount;

int     g_iMessageTextStart = 0;
char    g_cMessageTextCurrNum = 0;
char    g_cMessageTextNum = 0;
int     g_iNumLineMessageBoxCustom;
char    g_lpszMessageBoxCustom[NUM_LINE_CMB][MAX_LENGTH_CMB];
int     g_iCustomMessageBoxButton[NUM_BUTTON_CMB][NUM_PAR_BUTTON_CMB];

int     g_iCustomMessageBoxButton_Cancel[NUM_PAR_BUTTON_CMB];

int		g_iCancelSkillTarget = 0;

#define NUM_LINE_DA		( 1)
int g_iCurrentDialogScript = -1;
int g_iNumAnswer = 0;
char g_lpszDialogAnswer[MAX_ANSWER_FOR_DIALOG][NUM_LINE_DA][MAX_LENGTH_CMB];

int    CameraWalkCut;
int    CurrentCameraCount = -1;
int    CurrentCameraWalkType = 0;
int    CurrentCameraNumber = 0;
vec3_t CurrentCameraPosition;
vec3_t CurrentCameraAngle;
float  CurrentCameraWalkDelta[6];
float  CameraWalk[] =
{
	 0.f,-1000.f,500.f,-80.f,0.f, 0.f,
	 0.f,-1100.f,500.f,-80.f,0.f, 0.f,
	 0.f,-1100.f,500.f,-80.f,0.f, 0.f,
	 0.f,-1100.f,500.f,-80.f,0.f, 0.f,
	 0.f,-1100.f,500.f,-80.f,0.f, 0.f,
	200.f,-800.f,250.f,-87.f,0.f, -10.f,
};

bool MenuCancel = true;
bool EnableSocket = false;
bool InitLogIn = false;
bool InitLoading = false;
bool InitCharacterScene = false;
bool InitMainScene = false;
int  MenuY = 480;
int  MenuX = -200;
extern char LogInID[MAX_ID_SIZE + 1];
extern char m_ExeVersion[11];


extern DWORD g_dwBKConv;
extern DWORD g_dwBKSent;
extern BOOL g_bIMEBlock;

bool EnableMainRender = false;
extern int HeroKey;

int MenuStateCurrent = MENU_SERVER_LIST;
int MenuStateNext = MENU_SERVER_LIST;
int DeleteGuildIndex = -1;
int  ErrorMessage = NULL;
int	 ErrorMessageNext = NULL;

int SelectedHero = -1;

int TestTime = 0;
float g_Luminosity;
extern int GetServerWeatherVisual();
extern float g_fScreenRate_x;
extern float g_fScreenRate_y;
extern double g_TwistWeaponRenderMs;
extern int g_TwistWeaponDrawCount;

namespace
{
	bool g_HasServerVisualTime = false;
	bool g_ServerVisualTimeAutomatic = true;
	int g_ServerVisualSeconds = 12 * 3600;
	DWORD g_ServerVisualTimeReceivedAt = 0;
	int g_ServerClockSeconds = 0;
	DWORD g_ServerClockReceivedAt = 0;
	bool g_HasServerClock = false;
	float g_AtmosphereHour = 12.0f;
	float g_AtmosphereRain = 0.0f;
	float g_AtmosphereStorm = 0.0f;
	float g_AtmosphereDust = 0.0f;
	float g_AtmosphereSnow = 0.0f;
	float g_ServerSnowCoverVisual = 0.0f;
	float g_AtmosphereFog = 0.0f;
	float g_AtmosphereCloud = 0.0f;
	float g_ServerSkyHorizonColor[3] = {0.60f, 0.68f, 0.76f};
	DWORD g_AtmosphereUpdatedAt = 0;
	bool g_AtmosphereTimeEnabled = true;
	bool g_AtmosphereDynamicSunShadows = true;
	float g_AtmosphereShadowAngle = 225.0f;
	float g_AtmosphereShadowLength = 0.42f;
	float g_AtmosphereShadowOpacity = 1.0f;
	float g_AtmosphereSunDirX = -0.50f;
	float g_AtmosphereSunDirY = -0.80f;
	float g_AtmosphereSunDirZ = -0.35f;
	float g_AtmosphereShadowMinLength = 0.18f;
	float g_AtmosphereShadowMaxLength = 0.78f;
	float g_AtmosphereNightShadowOpacity = 0.12f;

	float ClampAtmosphere(float value, float low, float high)
	{
		if (value < low) return low;
		if (value > high) return high;
		return value;
	}

	void UpdateServerSunState()
	{
		const float pi = 3.14159265358979323846f;
		const float hour = g_AtmosphereHour;
		const float solar = sinf((hour - 6.0f) * pi / 12.0f);
		const float altitude = ClampAtmosphere(solar, 0.0f, 1.0f);
		float daylight = ClampAtmosphere((solar + 0.08f) / 0.38f, 0.0f, 1.0f);
		daylight = daylight * daylight * (3.0f - 2.0f * daylight);

		// Keep the old 225 degree direction around noon, but rotate continuously
		// from sunrise to sunset so legacy projected shadows follow the sun.
		float angle = 225.0f + (hour - 12.0f) * 15.0f;
		while (angle < 0.0f) angle += 360.0f;
		while (angle >= 360.0f) angle -= 360.0f;
		g_AtmosphereShadowAngle = angle;

		const float minLength = ClampAtmosphere(g_AtmosphereShadowMinLength, 0.08f, 0.85f);
		const float maxLength = ClampAtmosphere(g_AtmosphereShadowMaxLength, minLength, 1.30f);
		g_AtmosphereShadowLength = maxLength - altitude * (maxLength - minLength);

		const float weatherLoss =
			g_AtmosphereRain * 0.18f + g_AtmosphereStorm * 0.42f +
			g_AtmosphereSnow * 0.20f + g_AtmosphereFog * 0.36f +
			g_AtmosphereDust * 0.24f + g_AtmosphereCloud * 0.18f;
		const float night = ClampAtmosphere(g_AtmosphereNightShadowOpacity, 0.0f, 0.45f);
		g_AtmosphereShadowOpacity =
			ClampAtmosphere((night + (1.0f - night) * daylight) * (1.0f - weatherLoss), 0.03f, 1.0f);

		// Direction from the light towards the world. Horizontal direction matches
		// the projected-shadow cast direction; Z becomes steeper around noon.
		const float radians = angle * pi / 180.0f;
		const float vertical = 0.22f + altitude * 0.78f;
		const float horizontal = sqrtf(fmaxf(0.0f, 1.0f - vertical * vertical));
		g_AtmosphereSunDirX = cosf(radians) * horizontal;
		g_AtmosphereSunDirY = sinf(radians) * horizontal;
		g_AtmosphereSunDirZ = -vertical;
	}

	void UpdateServerAtmosphere()
	{
		const DWORD tick = GetTickCount();
		static DWORD lastOptionRead = 0;
		if (lastOptionRead == 0 || tick - lastOptionRead >= 1000)
		{
			g_AtmosphereTimeEnabled = GetPrivateProfileIntA("WorldVisual", "TimeOfDay", 1, ".\\config.ini") != 0;
			g_AtmosphereDynamicSunShadows = GetPrivateProfileIntA("WorldVisual", "DynamicSunShadows", 1, ".\\config.ini") != 0;
			g_AtmosphereShadowMinLength = ClampAtmosphere(GetPrivateProfileIntA("WorldVisual", "SunShadowMinLength", 18, ".\\config.ini") * 0.01f, 0.08f, 0.85f);
			g_AtmosphereShadowMaxLength = ClampAtmosphere(GetPrivateProfileIntA("WorldVisual", "SunShadowMaxLength", 78, ".\\config.ini") * 0.01f, g_AtmosphereShadowMinLength, 1.30f);
			g_AtmosphereNightShadowOpacity = ClampAtmosphere(GetPrivateProfileIntA("WorldVisual", "NightShadowOpacity", 12, ".\\config.ini") * 0.01f, 0.0f, 0.45f);
			lastOptionRead = tick;
		}
		SYSTEMTIME localTime = {};
		GetLocalTime(&localTime);
		const DWORD elapsed = g_ServerVisualTimeAutomatic ? (tick - g_ServerVisualTimeReceivedAt) / 1000 : 0;
		const float targetHour = !g_AtmosphereTimeEnabled ? 12.0f :
			(g_HasServerVisualTime ? ((g_ServerVisualSeconds + elapsed) % 86400) / 3600.0f :
			localTime.wHour + localTime.wMinute / 60.0f);
		const int weather = GetServerWeatherVisual();
		const int type = weather >> 4;
		const float targetRain = (type == 1 || type == 2) ? (weather & 15) / 15.0f : 0.0f;
		const float targetStorm = type == 2 ? targetRain : 0.0f;
		const float targetDust = type == 3 ? (weather & 15) / 15.0f : 0.0f;
		const float targetSnow = type == 4 ? (weather & 15) / 15.0f : 0.0f;
		const float targetFog = type == 5 ? (weather & 15) / 15.0f : 0.0f;
		const float targetCloud = type == 6 ? (weather & 15) / 15.0f : 0.0f;
		// Ao trocar de mapa, limpa imediatamente a atmosfera anterior. O pacote
		// seguinte define o clima do novo mapa; assim a neve de Devias nunca
		// permanece congelando Lorencia ou outro mapa.
		static int previousWorld = -1;
		if (previousWorld != -1 && previousWorld != World)
		{
			g_AtmosphereRain = 0.0f;
			g_AtmosphereStorm = 0.0f;
			g_AtmosphereDust = 0.0f;
			g_AtmosphereSnow = 0.0f;
			g_AtmosphereFog = 0.0f;
			g_AtmosphereCloud = 0.0f;
			g_ServerSnowCoverVisual = 0.0f;
		}
		previousWorld = World;
		if (g_AtmosphereUpdatedAt == 0)
		{
			g_AtmosphereHour = targetHour;
			g_AtmosphereRain = targetRain;
			g_AtmosphereStorm = targetStorm;
			g_AtmosphereDust = targetDust;
			g_AtmosphereSnow = targetSnow;
			g_AtmosphereFog = targetFog;
			g_AtmosphereCloud = targetCloud;
			g_AtmosphereUpdatedAt = tick;
			UpdateServerSunState();
			return;
		}
		const float dt = fminf((tick - g_AtmosphereUpdatedAt) * 0.001f, 0.10f);
		g_AtmosphereUpdatedAt = tick;
		const float timeStep = fminf(dt * 0.85f, 1.0f);
		const float weatherStep = fminf(dt * 1.20f, 1.0f);
		float difference = targetHour - g_AtmosphereHour;
		if (difference > 12.0f) difference -= 24.0f;
		if (difference < -12.0f) difference += 24.0f;
		g_AtmosphereHour += difference * timeStep;
		if (g_AtmosphereHour < 0.0f) g_AtmosphereHour += 24.0f;
		if (g_AtmosphereHour >= 24.0f) g_AtmosphereHour -= 24.0f;
		g_AtmosphereRain += (targetRain - g_AtmosphereRain) * weatherStep;
		g_AtmosphereStorm += (targetStorm - g_AtmosphereStorm) * weatherStep;
		g_AtmosphereDust += (targetDust - g_AtmosphereDust) * weatherStep;
		g_AtmosphereSnow += (targetSnow - g_AtmosphereSnow) * weatherStep;
		g_AtmosphereFog += (targetFog - g_AtmosphereFog) * weatherStep;
		g_AtmosphereCloud += (targetCloud - g_AtmosphereCloud) * weatherStep;
		UpdateServerSunState();
	}

	float ServerWeatherHash01(unsigned value)
	{
		value ^= value >> 16;
		value *= 0x7feb352dU;
		value ^= value >> 15;
		value *= 0x846ca68bU;
		value ^= value >> 16;
		return (value & 0x00FFFFFFU) / 16777215.0f;
	}

	bool IsServerWaterSurface(float x, float y)
	{
		const int tx = (int)floorf(x / TERRAIN_SCALE);
		const int ty = (int)floorf(y / TERRAIN_SCALE);
		if (tx < 0 || ty < 0 || tx >= TERRAIN_SIZE || ty >= TERRAIN_SIZE) return false;
		const int index = TERRAIN_INDEX_REPEAT(tx, ty);
		return IsLorenciaRainPuddle(x, y) || (TerrainWall[index] & TW_WATER) != 0 ||
			TerrainMappingLayer1[index] == 5 ||
			(TerrainMappingLayer2[index] == 5 && TerrainMappingAlpha[index] > 0.55f);
	}

	// Six-face sky assets, with the existing procedural sky as fallback.
	GLuint g_ServerSkyFaces[6] = {};
	int g_ServerSkyWorld = -1;
	bool g_ServerSkyAttempted = false;
	const char* g_ServerSkyFaceNames[6] = {
		"Back.bmp", "Bottom.bmp", "Front.bmp", "Left.bmp", "Right.bmp", "Top.bmp"
	};

	void ReleaseServerSkyBoxTextures()
	{
		for (int i = 0; i < 6; ++i)
		{
			if (g_ServerSkyFaces[i]) glDeleteTextures(1, &g_ServerSkyFaces[i]);
			g_ServerSkyFaces[i] = 0;
		}
		g_ServerSkyWorld = -1;
		g_ServerSkyAttempted = false;
	}

	bool LoadServerSkyBMP(const char* path, GLuint& texture)
	{
		FILE* file = nullptr;
		if (fopen_s(&file, path, "rb") != 0 || !file) return false;
		BITMAPFILEHEADER fh = {};
		BITMAPINFOHEADER ih = {};
		const bool headerOk = fread(&fh, sizeof(fh), 1, file) == 1 &&
			fread(&ih, sizeof(ih), 1, file) == 1 && fh.bfType == 0x4D42 &&
			ih.biSize >= sizeof(ih) && ih.biCompression == BI_RGB &&
			(ih.biBitCount == 8 || ih.biBitCount == 24 || ih.biBitCount == 32) &&
			ih.biWidth > 0 && ih.biWidth <= 4096 && ih.biHeight != 0 &&
			ih.biHeight >= -4096 && ih.biHeight <= 4096;
		if (!headerOk) { fclose(file); return false; }
		const int width = ih.biWidth;
		const int height = ih.biHeight < 0 ? -ih.biHeight : ih.biHeight;
		const int bpp = ih.biBitCount / 8;
		const int stride = ((width * bpp + 3) / 4) * 4;
		std::vector<RGBQUAD> palette;
		if (ih.biBitCount == 8)
		{
			const DWORD count = ih.biClrUsed ? ih.biClrUsed : 256;
			if (count > 256) { fclose(file); return false; }
			palette.resize(count);
			if (fseek(file, sizeof(fh) + ih.biSize, SEEK_SET) != 0 ||
				fread(palette.data(), sizeof(RGBQUAD), count, file) != count)
			{ fclose(file); return false; }
		}
		if (fseek(file, fh.bfOffBits, SEEK_SET) != 0) { fclose(file); return false; }
		std::vector<unsigned char> row(stride);
		std::vector<unsigned char> rgba(static_cast<size_t>(width) * height * 4);
		for (int y = 0; y < height; ++y)
		{
			if (fread(row.data(), 1, stride, file) != static_cast<size_t>(stride))
			{ fclose(file); return false; }
			const int dstY = ih.biHeight > 0 ? y : height - 1 - y;
			for (int x = 0; x < width; ++x)
			{
				unsigned char* dst = &rgba[(static_cast<size_t>(dstY) * width + x) * 4];
				const unsigned char* src = &row[x * bpp];
				if (bpp == 1)
				{
					if (src[0] >= palette.size()) { fclose(file); return false; }
					const RGBQUAD& c = palette[src[0]];
					dst[0] = c.rgbRed; dst[1] = c.rgbGreen; dst[2] = c.rgbBlue; dst[3] = 255;
				}
				else
				{
					dst[0] = src[2]; dst[1] = src[1]; dst[2] = src[0];
					dst[3] = bpp == 4 ? src[3] : 255;
				}
			}
		}
		fclose(file);
		glGenTextures(1, &texture);
		if (!texture) return false;
		glBindTexture(GL_TEXTURE_2D, texture);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA,
			GL_UNSIGNED_BYTE, rgba.data());
		glBindTexture(GL_TEXTURE_2D, 0);
		return true;
	}

	bool RenderServerSkyBox(const float* upper, const float* lower)
	{
		// Keep live option changes without querying the INI on every rendered frame.
		static bool optionLoaded = false;
		static bool enabled = true;
		static DWORD lastOptionRead = 0;
		const DWORD tick = GetTickCount();
		if (!optionLoaded || tick - lastOptionRead >= 1000)
		{
			enabled = GetPrivateProfileIntA("WorldVisual", "SkyBox", 1, ".\\config.ini") != 0;
			lastOptionRead = tick;
			optionLoaded = true;
		}
		if (!enabled)
			return false;
		if (World != g_ServerSkyWorld)
		{
			ReleaseServerSkyBoxTextures();
			g_ServerSkyWorld = World;
		}
		if (!g_ServerSkyAttempted)
		{
			g_ServerSkyAttempted = true;
			for (int i = 0; i < 6; ++i)
			{
				char path[MAX_PATH] = {};
				sprintf_s(path, ".\\Data\\Custom\\Sky\\World%d\\%s", World + 1, g_ServerSkyFaceNames[i]);
				LoadServerSkyBMP(path, g_ServerSkyFaces[i]);
			}
		}
		for (int i = 0; i < 6; ++i)
			if (!g_ServerSkyFaces[i]) return false;

		// Keep the cube inside this client's camera far plane. Reference implementation uses
		// 3000 units, but several Main 5.2 maps have a shorter view distance.
		const float size = fminf(3000.0f, CameraViewFar * 0.60f);
		const float top = size * 0.50f, bottom = -size * (350.0f / 3000.0f);
		const float brightness = ClampAtmosphere(
			(upper[0] + upper[1] + upper[2] + lower[0] + lower[1] + lower[2]) / 3.0f,
			0.25f, 1.0f);
		glPushAttrib(GL_ALL_ATTRIB_BITS);
		glPushMatrix();
		glTranslatef(CameraPosition[0], CameraPosition[1], CameraPosition[2]);
		glDisable(GL_LIGHTING); glDisable(GL_FOG); glDisable(GL_CULL_FACE);
		glDisable(GL_ALPHA_TEST); glDisable(GL_BLEND); glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);
		glEnable(GL_TEXTURE_2D);
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glColor3f(brightness, brightness, brightness);
		const float v[8][3] = {
			{-size,-size,bottom},{size,-size,bottom},{size,size,bottom},{-size,size,bottom},
			{-size,-size,top},{size,-size,top},{size,size,top},{-size,size,top}
		};
		const int faces[6][4] = {
			{4,5,1,0}, {3,2,1,0}, {7,6,2,3},
			{4,7,3,0}, {5,6,2,1}, {7,6,5,4}
		};
		const float uv[6][4][2] = {
			{{0,1},{1,1},{1,0},{0,0}},
			{{1,1},{0,1},{0,0},{1,0}},
			{{1,1},{0,1},{0,0},{1,0}},
			{{1,1},{0,1},{0,0},{1,0}},
			{{0,1},{1,1},{1,0},{0,0}},
			{{1,0},{0,0},{0,1},{1,1}}
		};
		for (int face = 0; face < 6; ++face)
		{
			glBindTexture(GL_TEXTURE_2D, g_ServerSkyFaces[face]);
			glBegin(GL_QUADS);
			for (int corner = 0; corner < 4; ++corner)
			{
				glTexCoord2fv(uv[face][corner]);
				glVertex3fv(v[faces[face][corner]]);
			}
			glEnd();
		}
		glPopMatrix();
		glPopAttrib();
		return true;
	}

	void RenderServerVisualSky()
	{
		// Use the reference client's day/night palette on outdoor maps without
		// importing its World1 snowy cubemap into unrelated locations.
		static DWORD lastOptionRead = 0;
		static bool enabled = true;
		static bool cloudsEnabled = true;
		static bool starsEnabled = true;
		const DWORD tick = GetTickCount();
		if (lastOptionRead == 0 || tick - lastOptionRead >= 1000)
		{
			enabled = GetPrivateProfileIntA("WorldVisual", "Sky", 1, ".\\config.ini") != 0;
			cloudsEnabled = GetPrivateProfileIntA("WorldVisual", "SkyClouds", 1, ".\\config.ini") != 0;
			starsEnabled = GetPrivateProfileIntA("WorldVisual", "SkyStars", 1, ".\\config.ini") != 0;
			lastOptionRead = tick;
		}
		const bool outdoor = World == WD_0LORENCIA || World == WD_2DEVIAS ||
			World == WD_3NORIA || World == WD_8TARKAN || World == WD_33AIDA ||
			World == WD_34CRYWOLF_1ST || World == WD_35CRYWOLF_2ND;
		if (!outdoor || !enabled)
			return;

		const float hour = g_AtmosphereHour;
		const float top[4][3] = {
			{0.018f, 0.030f, 0.100f}, {0.235f, 0.375f, 0.660f},
			{0.195f, 0.475f, 0.850f}, {0.110f, 0.160f, 0.390f}
		};
		const float horizon[4][3] = {
			{0.050f, 0.095f, 0.205f}, {0.930f, 0.500f, 0.285f},
			{0.720f, 0.850f, 0.970f}, {0.930f, 0.350f, 0.190f}
		};
		const int phase = (int)(hour / 6.0f) % 4;
		float blend = (hour - phase * 6.0f) / 6.0f;
		blend = blend * blend * (3.0f - 2.0f * blend);
		const int next = (phase + 1) % 4;
		const float dark = 0.20f * g_AtmosphereRain + 0.35f * g_AtmosphereStorm +
			0.28f * g_AtmosphereDust + 0.16f * g_AtmosphereSnow + 0.30f * g_AtmosphereFog +
			0.15f * g_AtmosphereCloud;
		float upper[3], lower[3];
		for (int i = 0; i < 3; ++i)
		{
			upper[i] = (top[phase][i] + (top[next][i] - top[phase][i]) * blend) * (1.0f - dark);
			lower[i] = (horizon[phase][i] + (horizon[next][i] - horizon[phase][i]) * blend) * (1.0f - dark * 0.72f);
		}
		if (World == WD_2DEVIAS)
		{
			upper[0] *= 0.86f; lower[0] *= 0.90f;
			upper[2] *= 1.10f; lower[2] *= 1.08f;
		}
		else if (World == WD_8TARKAN)
		{
			upper[2] *= 0.80f; lower[2] *= 0.82f;
			lower[0] *= 1.05f;
		}
		if (g_AtmosphereDust > 0.01f)
		{
			const float dust = g_AtmosphereDust * 0.52f;
			const float dustTop[3] = {0.47f, 0.31f, 0.17f};
			const float dustHorizon[3] = {0.67f, 0.48f, 0.29f};
			for (int i = 0; i < 3; ++i)
			{
				upper[i] += (dustTop[i] - upper[i]) * dust;
				lower[i] += (dustHorizon[i] - lower[i]) * dust;
			}
		}
		if (g_AtmosphereSnow > 0.01f)
		{
			const float snow = g_AtmosphereSnow * 0.28f;
			const float snowTop[3] = {0.32f, 0.42f, 0.58f};
			const float snowHorizon[3] = {0.60f, 0.68f, 0.76f};
			for (int i = 0; i < 3; ++i)
			{
				upper[i] += (snowTop[i] - upper[i]) * snow;
				lower[i] += (snowHorizon[i] - lower[i]) * snow;
			}
		}
		if (g_AtmosphereFog > 0.01f)
		{
			const float fog = g_AtmosphereFog * 0.62f;
			const float fogColor[3] = {0.47f, 0.51f, 0.56f};
			for (int i = 0; i < 3; ++i)
			{
				upper[i] += (fogColor[i] - upper[i]) * fog * 0.70f;
				lower[i] += (fogColor[i] - lower[i]) * fog;
			}
		}
		for (int i = 0; i < 3; ++i)
			g_ServerSkyHorizonColor[i] = lower[i];
		const bool texturedSky = RenderServerSkyBox(upper, lower);
		glPushAttrib(GL_ALL_ATTRIB_BITS);
		BeginBitmap();
		glDisable(GL_TEXTURE_2D);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_BLEND);
		if (!texturedSky)
		{
			glBegin(GL_QUADS);
			glColor3fv(lower); glVertex2f(0.0f, 0.0f); glVertex2f((float)WindowWidth, 0.0f);
			glColor3fv(upper); glVertex2f((float)WindowWidth, (float)WindowHeight); glVertex2f(0.0f, (float)WindowHeight);
			glEnd();
		}

		// Cheap deterministic stars. They fade out during daylight and bad weather,
		// and are drawn before the cloud layer so storms naturally cover them.
		const float dayLightForStars = fmaxf(0.0f, 1.0f - fabsf(hour - 12.0f) / 12.0f);
		const float starNight = ClampAtmosphere((0.42f - dayLightForStars) / 0.42f, 0.0f, 1.0f);
		const float starWeather = ClampAtmosphere(1.0f - g_AtmosphereRain * 0.75f - g_AtmosphereStorm -
			g_AtmosphereFog * 0.90f - g_AtmosphereSnow * 0.40f - g_AtmosphereDust * 0.75f, 0.0f, 1.0f);
		const float starAlpha = starNight * starWeather;
		if (!texturedSky && starsEnabled && starAlpha > 0.01f)
		{
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);
			glPointSize(1.4f);
			glBegin(GL_POINTS);
			for (int i = 0; i < 96; ++i)
			{
				const float sx = ServerWeatherHash01(i * 31U + 7U) * WindowWidth;
				const float sy = WindowHeight * (0.40f + ServerWeatherHash01(i * 47U + 19U) * 0.58f);
				const float twinkle = 0.55f + 0.45f * sinf(tick * 0.0012f + i * 1.73f);
				glColor4f(0.78f, 0.86f, 1.0f, starAlpha * (0.28f + 0.52f * twinkle));
				glVertex2f(sx, sy);
			}
			glEnd();
			glPointSize(1.0f);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}
		if (cloudsEnabled)
		{
			// Camadas suaves e baratas, sem textura ou alocacao por quadro.
			// A chuva fecha o ceu; a nevoa inferior liga as nuvens ao horizonte.
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			const float cover = fminf(1.0f, 0.24f + g_AtmosphereRain * 0.48f +
				g_AtmosphereStorm * 0.27f + g_AtmosphereDust * 0.35f + g_AtmosphereCloud * 0.55f);
			const float drift = tick * 0.001f * (2.2f + g_AtmosphereStorm * 3.5f);
			const float dayLight = fmaxf(0.0f, 1.0f - fabsf(hour - 12.0f) / 12.0f);
			float cloudColor[3];
			for (int c = 0; c < 3; ++c)
				cloudColor[c] = lower[c] * (0.70f + dayLight * 0.27f);
			for (int layer = 0; layer < 2; ++layer)
			{
				const float y = WindowHeight * (0.43f + layer * 0.17f);
				const float height = WindowHeight * (0.045f + layer * 0.018f);
				const float step = WindowWidth * 0.24f;
				for (int n = -2; n < 7; ++n)
				{
					const float x = n * step + fmodf(drift * (layer + 1), step);
					const float wobble = (ServerWeatherHash01((unsigned)(n + 5 + layer * 19)) - 0.5f) * height;
					glBegin(GL_TRIANGLE_FAN);
					glColor4f(cloudColor[0], cloudColor[1], cloudColor[2], cover * 0.22f);
					glVertex2f(x, y + wobble);
					glColor4f(cloudColor[0], cloudColor[1], cloudColor[2], 0.0f);
					for (int k = 0; k <= 12; ++k)
					{
						const float angle = k * 6.2831853f / 12.0f;
						glVertex2f(x + cosf(angle) * step * 0.78f,
							y + wobble + sinf(angle) * height);
					}
					glEnd();
				}
			}
			glBegin(GL_QUADS);
			glColor4f(lower[0], lower[1], lower[2], cover * 0.24f);
			glVertex2f(0.0f, WindowHeight * 0.18f);
			glVertex2f((float)WindowWidth, WindowHeight * 0.18f);
			glColor4f(lower[0], lower[1], lower[2], 0.0f);
			glVertex2f((float)WindowWidth, WindowHeight * 0.37f);
			glVertex2f(0.0f, WindowHeight * 0.37f);
			glEnd();
		}
		EndBitmap();
		glPopAttrib();
	}

	bool BeginServerHorizonFog(bool& wasEnabled)
	{
		static DWORD lastOptionRead = 0;
		static bool enabled = true;
		static bool lorenciaLook = false;
		static int startPercent = 67;
		static int endPercent = 102;
		const DWORD tick = GetTickCount();
		if (lastOptionRead == 0 || tick - lastOptionRead >= 1000)
		{
			enabled = GetPrivateProfileIntA("WorldLighting", "HorizonFog", 1, ".\\config.ini") != 0;
			lorenciaLook = GetPrivateProfileIntA("VisualTrial", "LorenciaLook", 0, ".\\config.ini") != 0;
			startPercent = GetPrivateProfileIntA("WorldLighting", "HorizonFogStart", 67, ".\\config.ini");
			endPercent = GetPrivateProfileIntA("WorldLighting", "HorizonFogEnd", 102, ".\\config.ini");
			if (startPercent < 35) startPercent = 35;
			if (startPercent > 95) startPercent = 95;
			if (endPercent < startPercent + 10) endPercent = startPercent + 10;
			if (endPercent > 150) endPercent = 150;
			lastOptionRead = tick;
		}
		if (g_RenderBackend.GetModernBackend() != nullptr ||
			!enabled)
			return false;
		const bool outdoor = World == WD_0LORENCIA || World == WD_2DEVIAS ||
			World == WD_3NORIA || World == WD_8TARKAN || World == WD_33AIDA ||
			World == WD_34CRYWOLF_1ST || World == WD_35CRYWOLF_2ND;
		if (!outdoor) return false;
		float color[4] = {
			g_ServerSkyHorizonColor[0],
			g_ServerSkyHorizonColor[1],
			g_ServerSkyHorizonColor[2],
			1.0f
		};
		const float weather = fminf(1.0f, g_AtmosphereFog * 0.55f +
			g_AtmosphereStorm * 0.20f + g_AtmosphereDust * 0.25f);
		const bool useLorenciaLook = lorenciaLook && World == WD_0LORENCIA;
		if (useLorenciaLook)
		{
			const float neutral = (color[0] + color[1] + color[2]) / 3.0f;
			for (int c = 0; c < 3; ++c)
				color[c] = color[c] * 0.80f + neutral * 0.20f;
		}
		const float startRatio = startPercent * 0.01f * (useLorenciaLook ? 0.90f : 1.0f);
		const float fogStart = CameraViewFar * startRatio * (1.0f - weather * 0.30f);
		const float fogEnd = CameraViewFar * (endPercent * 0.01f) * (1.0f - weather * 0.18f);
		wasEnabled = glIsEnabled(GL_FOG) == GL_TRUE;
		glPushAttrib(GL_FOG_BIT);
		glFogi(GL_FOG_MODE, GL_LINEAR);
		glFogfv(GL_FOG_COLOR, color);
		glFogf(GL_FOG_START, fogStart);
		glFogf(GL_FOG_END, fogEnd);
		glEnable(GL_FOG);
		return true;
	}

	void RenderServerWeatherPrecipitation()
	{
		if (Hero == nullptr || g_AtmosphereRain < 0.01f) return;
		const float intensity = g_AtmosphereRain;
		const float stormMix = g_AtmosphereStorm;
		const DWORD tick = GetTickCount();
		static DWORD lastParticleOptionRead = 0;
		static int rainParticleMax = 360;
		if (lastParticleOptionRead == 0 || tick - lastParticleOptionRead >= 1000)
		{
			rainParticleMax = GetPrivateProfileIntA("WorldVisual", "RainParticleMax", 360, ".\\config.ini");
			if (rainParticleMax < 80) rainParticleMax = 80;
			if (rainParticleMax > 650) rainParticleMax = 650;
			lastParticleOptionRead = tick;
		}
		const int desiredCount = (int)((420.0f + 230.0f * stormMix) * intensity);
		int adaptiveMax = rainParticleMax;
		if (RealRenderFPS > 0.0f && RealRenderFPS < 38.0f) adaptiveMax = 120;
		else if (RealRenderFPS > 0.0f && RealRenderFPS < 48.0f) adaptiveMax = 200;
		const int count = (desiredCount < adaptiveMax) ? desiredCount : adaptiveMax;
		if (count < 1) return;
		const float now = tick * 0.001f;
		if (stormMix > 0.01f)
		{
			// Storm has its own subdued sky tint and brief lightning, unlike normal rain.
			const DWORD flashPhase = tick % 6500;
			const float flash = (flashPhase < 105 || (flashPhase >= 180 && flashPhase < 245)) ? 0.27f * stormMix : 0.0f;
			BeginBitmap();
			EnableAlphaBlend();
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glColor4f(0.025f, 0.04f, 0.075f, 0.19f * stormMix);
			RenderColor(0.0f, 0.0f, (float)WindowWidth, (float)WindowHeight, 0.0f, 0, false);
			if (flash > 0.0f)
			{
				glColor4f(0.72f, 0.82f, 1.0f, flash);
				RenderColor(0.0f, 0.0f, (float)WindowWidth, (float)WindowHeight, 0.0f, 0, false);
			}
			EndRenderColor();
			DisableAlphaBlend();
			EndBitmap();
		}
		const float px = Hero->Object.Position[0];
		const float py = Hero->Object.Position[1];
		const float pz = Hero->Object.Position[2];
		const float radius = 900.0f + 150.0f * stormMix;
		// Reach rivers below bridges and cliffs, not only the ground near the hero.
		const float travelRange = 1550.0f;
		const float speed = 680.0f + 220.0f * stormMix;
		const float wind = 0.10f + 0.35f * stormMix;
		static DWORD lastImpactOptionRead = 0;
		static bool rainImpactsEnabled = true;
		static bool lorenciaWaterTrial = false;
		static float rainImpactStrength = 0.70f;
		if (lastImpactOptionRead == 0 || tick - lastImpactOptionRead >= 1000)
		{
			rainImpactsEnabled = GetPrivateProfileIntA("WaterVisual", "RainImpacts", 1, ".\\config.ini") != 0;
			lorenciaWaterTrial = GetPrivateProfileIntA("WaterVisual", "LorenciaTrial", 0, ".\\config.ini") != 0;
			int strength = GetPrivateProfileIntA("WaterVisual", "RainImpactStrength", 70, ".\\config.ini");
			if (strength < 0) strength = 0;
			if (strength > 100) strength = 100;
			rainImpactStrength = strength * 0.01f;
			lastImpactOptionRead = tick;
		}
		const bool waterTrial = lorenciaWaterTrial && World == WD_0LORENCIA && SceneFlag == MAIN_SCENE;
		const bool showImpacts = rainImpactsEnabled || waterTrial;
		struct RainImpact { float x, y, z; DWORD born; bool water; };
		static RainImpact impacts[192] = {};
		static float previousZ[650] = {};
		static float previousX[650] = {};
		static float previousY[650] = {};
		static int previousWorld = -1;
		static int nextImpact = 0;
		static DWORD lastRainTick = 0;
		if (previousWorld != World || (lastRainTick != 0 && tick - lastRainTick > 250))
		{
			memset(impacts, 0, sizeof(impacts));
			memset(previousZ, 0, sizeof(previousZ));
			previousWorld = World;
		}
		lastRainTick = tick;
		glPushAttrib(GL_ALL_ATTRIB_BITS);
		glDisable(GL_TEXTURE_2D);
		glDisable(GL_LIGHTING);
		glDisable(GL_ALPHA_TEST);
		glDisable(GL_CULL_FACE);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);
		glLineWidth(1.0f);
		glBegin(GL_LINES);
		for (int i = 0; i < count; ++i)
		{
			const float variety = ServerWeatherHash01(i * 23U + 43U);
			const float dropSpeed = speed * (0.82f + variety * 0.36f);
			const float travel = fmodf(now * dropSpeed + ServerWeatherHash01(i * 7U + 131U) * travelRange, travelRange);
			const float x = px + (ServerWeatherHash01(i * 3U + 17U) * 2.0f - 1.0f) * radius + travel * wind;
			const float y = py + (ServerWeatherHash01(i * 5U + 71U) * 2.0f - 1.0f) * radius;
			const float z = pz + 450.0f - travel;
			const float ground = RequestTerrainHeight(x, y);
			if (showImpacts && i % 2 == 0 && previousZ[i] > ground && z <= ground &&
				previousZ[i] - z < 100.0f && fabsf(x - previousX[i]) < 100.0f &&
				fabsf(y - previousY[i]) < 100.0f)
			{
				const float fraction = (previousZ[i] - ground) / (previousZ[i] - z);
				RainImpact& hit = impacts[nextImpact];
				hit.x = previousX[i] + (x - previousX[i]) * fraction;
				hit.y = previousY[i] + (y - previousY[i]) * fraction;
				hit.water = IsServerWaterSurface(hit.x, hit.y);
                if (hit.water) AddLorenciaRainRipple(hit.x, hit.y);
				hit.z = RequestTerrainHeight(hit.x, hit.y) + (hit.water ? 6.0f : 2.5f);
				hit.born = tick;
				nextImpact = (nextImpact + 1) % 192;
			}
			previousZ[i] = z;
			previousX[i] = x;
			previousY[i] = y;
			if (z <= ground) continue;
			const float length = 31.0f + 15.0f * stormMix + variety * (22.0f + 6.0f * stormMix);
			const float distance = fmaxf(fabsf(x - px), fabsf(y - py)) / radius;
			const float alpha = (0.30f + 0.07f * stormMix) * (1.0f - distance * 0.42f) * (0.65f + variety * 0.35f);
			glColor4f(0.68f, 0.79f, 0.91f, alpha);
			glVertex3f(x, y, z);
			glVertex3f(x - length * wind, y, z + length);
		}
		glEnd();
		// A ondulacao fica na posicao do cruzamento real, mesmo se a camera mover.
		glLineWidth(1.5f);
		glBegin(GL_LINES);
		int drawnImpacts = 0;
		const int impactBudget = waterTrial ? ((RealRenderFPS > 0.f && RealRenderFPS < 58.f) ? 16 : 48) : 192;
		for (int i = 0; showImpacts && i < 192 && drawnImpacts < impactBudget; ++i)
		{
			const RainImpact& hit = impacts[i];
			if (hit.born == 0) continue;
			const float age = (tick - hit.born) * 0.001f;
			const float lifetime = hit.water ? 0.52f : 0.14f;
			if (age > lifetime) continue;
			if (waterTrial && !rainImpactsEnabled && !hit.water) continue;
            // Lorencia rain already perturbs shader normals: do not draw white line circles.
            if (waterTrial && hit.water) continue;
			++drawnImpacts;
			const float sx = hit.x, sy = hit.y, sz = hit.z;
			const float spread = hit.water ? (6.0f + age * 120.0f) : (2.0f + age * 54.0f);
			const float diagonal = spread * 0.7071f;
			const float fade = 1.0f - age / lifetime;
			if (hit.water) glColor4f(0.88f, 0.96f, 1.0f, fade * intensity * rainImpactStrength);
			else glColor4f(0.70f, 0.82f, 0.93f, fade * intensity * 0.48f);
			if (hit.water)
			{
				for (int segment = 0; segment < 16; ++segment)
				{
					const float a = segment * 6.2831853f / 16.0f;
					const float b = (segment + 1) * 6.2831853f / 16.0f;
					glVertex3f(sx + cosf(a) * spread, sy + sinf(a) * spread, sz);
					glVertex3f(sx + cosf(b) * spread, sy + sinf(b) * spread, sz);
				}
				if (age < 0.16f)
				{
					const float splashHeight = (1.0f - age / 0.16f) * 15.0f;
					glVertex3f(sx - 3.0f, sy, sz); glVertex3f(sx - 5.0f, sy, sz + splashHeight);
					glVertex3f(sx + 3.0f, sy, sz); glVertex3f(sx + 5.0f, sy, sz + splashHeight);
				}
			}
			else
			{
				glVertex3f(sx + spread, sy, sz); glVertex3f(sx + diagonal, sy + diagonal, sz);
				glVertex3f(sx, sy + spread, sz); glVertex3f(sx - diagonal, sy + diagonal, sz);
				glVertex3f(sx - spread, sy, sz); glVertex3f(sx - diagonal, sy - diagonal, sz);
				glVertex3f(sx, sy - spread, sz); glVertex3f(sx + diagonal, sy - diagonal, sz);
			}
			if (!hit.water && age < 0.08f)
			{
				glVertex3f(sx - 2.0f, sy, sz); glVertex3f(sx - 4.0f, sy, sz + fade * 12.0f);
				glVertex3f(sx + 2.0f, sy, sz); glVertex3f(sx + 4.0f, sy, sz + fade * 12.0f);
			}
		}
		glEnd();
		glPopAttrib();
	}

	void RenderServerSandstorm()
	{
		if (Hero == nullptr || g_AtmosphereDust < 0.01f) return;
		const float strength = g_AtmosphereDust;
		const float now = GetTickCount() * 0.001f;
		const float px = Hero->Object.Position[0];
		const float py = Hero->Object.Position[1];
		const float pz = Hero->Object.Position[2];
		const int count = (int)(240.0f * strength);
		glPushAttrib(GL_ALL_ATTRIB_BITS);
		glDisable(GL_TEXTURE_2D);
		glDisable(GL_LIGHTING);
		glDisable(GL_ALPHA_TEST);
		glDisable(GL_CULL_FACE);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);
		glLineWidth(1.0f);
		glBegin(GL_LINES);
		for (int i = 0; i < count; ++i)
		{
			const float drift = fmodf(now * (310.0f + ServerWeatherHash01(i * 17U) * 180.0f) +
				ServerWeatherHash01(i * 13U + 5U) * 1600.0f, 1600.0f);
			const float x = px - 800.0f + drift;
			const float y = py + (ServerWeatherHash01(i * 7U + 31U) * 2.0f - 1.0f) * 700.0f;
			const float z = pz + 15.0f + ServerWeatherHash01(i * 11U + 91U) * 350.0f;
			const float length = 8.0f + ServerWeatherHash01(i * 19U + 57U) * 22.0f;
			glColor4f(0.78f, 0.59f, 0.36f, strength * 0.20f);
			glVertex3f(x, y, z);
			glVertex3f(x - length, y - length * 0.14f, z - length * 0.06f);
		}
		glEnd();
		glPopAttrib();
		BeginBitmap();
		EnableAlphaBlend();
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glColor4f(0.38f, 0.26f, 0.14f, strength * 0.08f);
		RenderColor(0.0f, 0.0f, (float)WindowWidth, (float)WindowHeight, 0.0f, 0, false);
		EndRenderColor();
		DisableAlphaBlend();
		EndBitmap();
	}

	float CachedSnowTerrainHeight(int tx, int ty)
	{
		// Cada canto do terreno mantem a altura ate mudar de mapa. Girar a
		// camera nao precisa repetir milhares de consultas ao relevo.
		struct Cell { int world, x, y; float height; };
		static Cell cells[54][54] = {};
		Cell& cell = cells[ty % 54][tx % 54];
		if (cell.world != World + 1 || cell.x != tx || cell.y != ty)
		{
			cell.world = World + 1;
			cell.x = tx;
			cell.y = ty;
			cell.height = RequestTerrainHeight(tx * TERRAIN_SCALE, ty * TERRAIN_SCALE);
		}
		return cell.height;
	}

	void RenderServerSnowCover()
	{
		static DWORD lastOptionRead = 0;
		static DWORD lastUpdate = 0;
		static int lastWorld = -1;
		static bool enabled = true;
		static int radius = 14;
		static float cover = 0.f;
		const DWORD tick = GetTickCount();
		if (lastOptionRead == 0 || tick - lastOptionRead >= 1000)
		{
			enabled = GetPrivateProfileIntA("WorldVisual", "SnowCover", 1, ".\\config.ini") != 0;
			radius = GetPrivateProfileIntA("WorldVisual", "SnowCoverRadius", 14, ".\\config.ini");
			if (radius < 8) radius = 8;
			if (radius > 26) radius = 26;
			lastOptionRead = tick;
		}
		if (!enabled || Hero == nullptr) { g_ServerSnowCoverVisual = 0.f; return; }
		const bool nativeSnow = World == WD_2DEVIAS;
		// Mesmo em Devias, a cobertura obedece ao clima escolhido no servidor.
		const float base = 0.f;
		if (lastWorld == -1)
		{
			cover = base;
			lastUpdate = tick;
		}
		// Nao zera a neve ao trocar de mapa: a cobertura acumulada derrete
		// gradualmente, inclusive nos modelos 3D que consultam este valor.
		lastWorld = World;
		const float dt = fminf(0.1f, (tick - lastUpdate) * 0.001f);
		lastUpdate = tick;
		const float target = fminf(1.f, base + (1.f - base) * g_AtmosphereSnow);
		const float seconds = target > cover ? 7.f : (nativeSnow ? 180.f : 45.f);
		cover += (target - cover) * fminf(1.f, dt / seconds);
		g_ServerSnowCoverVisual = cover;
		if (cover < 0.02f) return;

		const int cx = (int)(Hero->Object.Position[0] / TERRAIN_SCALE);
		const int cy = (int)(Hero->Object.Position[1] / TERRAIN_SCALE);
		glPushAttrib(GL_ALL_ATTRIB_BITS);
		glDisable(GL_TEXTURE_2D);
		glDisable(GL_LIGHTING);
		glDisable(GL_ALPHA_TEST);
		glDisable(GL_CULL_FACE);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);
		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(-1.f, -1.f);
		glBegin(GL_QUADS);
		for (int dy = -radius; dy <= radius; ++dy)
		{
			for (int dx = -radius; dx <= radius; ++dx)
			{
				if (dx * dx + dy * dy > radius * radius) continue;
				const int tx = cx + dx, ty = cy + dy;
				if (tx < 0 || ty < 0 || tx >= TERRAIN_SIZE - 1 || ty >= TERRAIN_SIZE - 1) continue;
				const int index = ty * TERRAIN_SIZE + tx;
				if ((TerrainWall[index] & TW_NOGROUND) == TW_NOGROUND ||
					TerrainMappingLayer1[index] == 5 || TerrainMappingLayer2[index] == 5) continue;
				if (World == WD_2DEVIAS && (TerrainMappingLayer1[index] == 3 || TerrainMappingLayer1[index] >= 10)) continue;
				// A opacidade por quadra deixava bordas quadradas muito visiveis.
				// Interpola pelas normais dos quatro cantos, preservando o relevo.
				float cornerAlpha[4];
				const int cornerX[4] = {tx, tx + 1, tx + 1, tx};
				const int cornerY[4] = {ty, ty, ty + 1, ty + 1};
				float maxAlpha = 0.f;
				for (int corner = 0; corner < 4; ++corner)
				{
					const int cornerIndex = cornerY[corner] * TERRAIN_SIZE + cornerX[corner];
					float slope = (TerrainNormal[cornerIndex][2] - 0.26f) / 0.56f;
					slope = fminf(1.f, fmaxf(0.f, slope));
					slope = slope * slope * (3.f - 2.f * slope);
					const float variation = 0.91f + 0.09f * ServerWeatherHash01(
						(unsigned)(cornerX[corner] * 73856093u) ^ (unsigned)(cornerY[corner] * 19349663u));
					cornerAlpha[corner] = (nativeSnow ? 0.62f : 0.48f) *
						fminf(1.f, cover * slope * variation);
					maxAlpha = fmaxf(maxAlpha, cornerAlpha[corner]);
				}
				if (maxAlpha < 0.015f) continue;
				const float x = tx * TERRAIN_SCALE, y = ty * TERRAIN_SCALE;
				const float s = TERRAIN_SCALE;
				glColor4f(0.90f, 0.94f, 1.0f, cornerAlpha[0]);
				glVertex3f(x, y, CachedSnowTerrainHeight(tx, ty) + 1.4f);
				glColor4f(0.90f, 0.94f, 1.0f, cornerAlpha[1]);
				glVertex3f(x + s, y, CachedSnowTerrainHeight(tx + 1, ty) + 1.4f);
				glColor4f(0.90f, 0.94f, 1.0f, cornerAlpha[2]);
				glVertex3f(x + s, y + s, CachedSnowTerrainHeight(tx + 1, ty + 1) + 1.4f);
				glColor4f(0.90f, 0.94f, 1.0f, cornerAlpha[3]);
				glVertex3f(x, y + s, CachedSnowTerrainHeight(tx, ty + 1) + 1.4f);
			}
		}
		glEnd();
		glPopAttrib();
	}

	void RenderServerSnow()
	{
		static DWORD lastOptionRead = 0;
		static bool enabled = true;
		static int maxParticles = 320;
		const DWORD tick = GetTickCount();
		if (lastOptionRead == 0 || tick - lastOptionRead >= 1000)
		{
			enabled = GetPrivateProfileIntA("WorldVisual", "Snow", 1, ".\\config.ini") != 0;
			maxParticles = GetPrivateProfileIntA("WorldVisual", "SnowParticleMax", 320, ".\\config.ini");
			if (maxParticles < 60) maxParticles = 60;
			if (maxParticles > 600) maxParticles = 600;
			lastOptionRead = tick;
		}
		if (!enabled || Hero == nullptr || g_AtmosphereSnow < 0.01f) return;

		const float intensity = g_AtmosphereSnow;
		const float px = Hero->Object.Position[0];
		const float py = Hero->Object.Position[1];
		const float pz = Hero->Object.Position[2];
		const float now = tick * 0.001f;
		const int desiredCount = (int)(180.0f + 420.0f * intensity);
		const int count = (desiredCount < maxParticles) ? desiredCount : maxParticles;
		const float radius = 850.0f + 180.0f * intensity;
		const float fallRange = 850.0f;
		const float wind = 35.0f + 145.0f * intensity;

		glPushAttrib(GL_ALL_ATTRIB_BITS);
		glDisable(GL_TEXTURE_2D);
		glDisable(GL_LIGHTING);
		glDisable(GL_ALPHA_TEST);
		glDisable(GL_CULL_FACE);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);
		glPointSize(intensity > 0.82f ? 2.5f : 2.0f);
		glBegin(GL_POINTS);
		for (int i = 0; i < count; ++i)
		{
			const float seedA = ServerWeatherHash01(i * 13U + 17U);
			const float seedB = ServerWeatherHash01(i * 29U + 71U);
			const float speed = 65.0f + seedA * 85.0f + intensity * 55.0f;
			const float travel = fmodf(now * speed + seedB * fallRange, fallRange);
			const float swirl = sinf(now * (0.55f + seedA) + i * 0.61f) * (18.0f + 35.0f * intensity);
			const float windDrift = fmodf(now * wind + seedB * radius * 2.0f, radius * 2.0f) - radius;
			const float x = px + (seedA * 2.0f - 1.0f) * radius + windDrift * 0.35f + swirl;
			const float y = py + (seedB * 2.0f - 1.0f) * radius + cosf(now * 0.7f + i) * 22.0f;
			const float z = pz - 80.0f + fallRange - travel;
			// Keep this pass cheap: unlike rain impacts, snow does not need an
			// exact terrain collision for every flake.
			if (z <= pz - 110.0f) continue;
			const float alpha = (0.34f + seedB * 0.32f) * intensity;
			glColor4f(0.88f, 0.94f, 1.0f, alpha);
			glVertex3f(x, y, z);
		}
		glEnd();
		glPointSize(1.0f);
		glPopAttrib();

		if (intensity > 0.80f)
		{
			BeginBitmap();
			EnableAlphaBlend();
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glColor4f(0.68f, 0.76f, 0.86f, (intensity - 0.80f) * 0.28f);
			RenderColor(0.0f, 0.0f, (float)WindowWidth, (float)WindowHeight, 0.0f, 0, false);
			EndRenderColor();
			DisableAlphaBlend();
			EndBitmap();
		}
	}

	void RenderServerFog()
	{
		static DWORD lastOptionRead = 0;
		static bool enabled = true;
		const DWORD tick = GetTickCount();
		if (lastOptionRead == 0 || tick - lastOptionRead >= 1000)
		{
			enabled = GetPrivateProfileIntA("WorldVisual", "Fog", 1, ".\\config.ini") != 0;
			lastOptionRead = tick;
		}
		if (!enabled || g_AtmosphereFog < 0.01f) return;

		const float strength = g_AtmosphereFog;
		const float drift = tick * 0.000015f;
		BeginBitmap();
		EnableAlphaBlend();
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		for (int layer = 0; layer < 4; ++layer)
		{
			const float phase = fmodf(drift * (layer + 1) + layer * 0.23f, 1.0f);
			const float y = WindowHeight * (0.10f + layer * 0.15f + phase * 0.035f);
			const float h = WindowHeight * (0.17f + layer * 0.025f);
			const float alpha = strength * (0.055f + layer * 0.012f);
			glColor4f(0.58f, 0.62f, 0.66f, alpha);
			RenderColor(0.0f, y, (float)WindowWidth, h, 0.0f, 0, false);
		}
		EndRenderColor();
		DisableAlphaBlend();
		EndBitmap();
	}

	void UpdateAndRenderSurfaceTracks()
	{
		static DWORD lastOptionRead = 0;
		static bool enabled = true;
		const DWORD tick = GetTickCount();
		if (lastOptionRead == 0 || tick - lastOptionRead >= 1000)
		{
			enabled = GetPrivateProfileIntA("WorldVisual", "Footprints", 1, ".\\config.ini") != 0;
			lastOptionRead = tick;
		}
		if (!enabled || Hero == nullptr) return;
		const bool snow = World == WD_2DEVIAS;
		const bool sand = World == WD_8TARKAN;
		if (!snow && !sand) return;
		struct SurfaceTrack { float x, y, z, angle, width, length; DWORD born; };
		static SurfaceTrack tracks[96] = {};
		static int nextTrack = 0, previousWorld = -1, nextFoot = 0;
		static bool anchored = false;
		static float anchorX = 0.0f, anchorY = 0.0f;
		struct MonsterTrackAnchor { float x, y; bool valid; bool nextFoot; };
		static MonsterTrackAnchor monsterAnchors[MAX_CHARACTERS_CLIENT] = {};
		static DWORD lastMonsterUpdate = 0;
		if (previousWorld != World)
		{
			memset(tracks, 0, sizeof(tracks));
			memset(monsterAnchors, 0, sizeof(monsterAnchors));
			previousWorld = World;
			anchored = false;
		}
		const float hx = Hero->Object.Position[0];
		const float hy = Hero->Object.Position[1];
		if (!anchored)
		{
			anchorX = hx; anchorY = hy; anchored = true;
		}
		const float dx = hx - anchorX, dy = hy - anchorY;
		const float distance2 = dx * dx + dy * dy;
		const bool heroFlying = (!Hero->SafeZone && Hero->Wing.Type != -1) ||
			Hero->Object.movementType == Movement::Dinorant ||
			Hero->Object.movementType == Movement::Uniria;
		if (distance2 > 42.0f * 42.0f)
		{
			anchorX = hx; anchorY = hy;
			const float ground = RequestTerrainHeight(hx, hy);
			if (!heroFlying && distance2 < 220.0f * 220.0f &&
				fabsf(Hero->Object.Position[2] - ground) < 75.0f)
			{
				const float angle = Hero->Object.Angle[2] * 3.14159265f / 180.0f;
				const float side = nextFoot ? 1.0f : -1.0f;
				nextFoot ^= 1;
				SurfaceTrack& mark = tracks[nextTrack];
				mark.x = hx + cosf(angle) * side * 8.0f;
				mark.y = hy + sinf(angle) * side * 8.0f;
				mark.z = RequestTerrainHeight(mark.x, mark.y) + 2.5f;
				mark.angle = angle;
				mark.width = sand ? 17.0f : 18.0f;
				mark.length = sand ? 28.0f : 30.0f;
				mark.born = tick;
				nextTrack = (nextTrack + 1) % 96;
			}
		}
		// Mobs e chefes proximos deixam marcas na mesma neve/areia.
		// A varredura e limitada a cada 150 ms e no maximo 12 marcas por ciclo.
		if (tick - lastMonsterUpdate >= 150)
		{
			lastMonsterUpdate = tick;
			int added = 0;
			for (int i = 0; i < MAX_CHARACTERS_CLIENT && added < 12; ++i)
			{
				CHARACTER* monster = gmCharacters->GetCharacter(i);
				MonsterTrackAnchor& anchor = monsterAnchors[i];
				if (monster == nullptr || !monster->Object.Live || monster->Object.Kind != KIND_MONSTER)
				{
					anchor.valid = false;
					continue;
				}
				const float x = monster->Object.Position[0], y = monster->Object.Position[1];
				const float vx = x - hx, vy = y - hy;
				if (vx * vx + vy * vy > 650.0f * 650.0f)
				{
					anchor.valid = false;
					continue;
				}
				if (!anchor.valid)
				{
					anchor.x = x; anchor.y = y; anchor.valid = true;
					continue;
				}
				const float dx = x - anchor.x, dy = y - anchor.y;
				const float moved = dx * dx + dy * dy;
				if (moved < 42.0f * 42.0f) continue;
				anchor.x = x; anchor.y = y;
				if (moved > 220.0f * 220.0f || IsServerWaterSurface(x, y)) continue;
				const float ground = RequestTerrainHeight(x, y);
				if (fabsf(monster->Object.Position[2] - ground) > 85.0f) continue;
				const float angle = monster->Object.Angle[2] * 3.14159265f / 180.0f;
				const float size = fminf(fmaxf(monster->Object.Scale, 0.7f), 2.4f);
				const float side = anchor.nextFoot ? 1.0f : -1.0f;
				anchor.nextFoot = !anchor.nextFoot;
				SurfaceTrack& mark = tracks[nextTrack];
				mark.x = x + cosf(angle) * side * 8.0f * size;
				mark.y = y + sinf(angle) * side * 8.0f * size;
				mark.z = RequestTerrainHeight(mark.x, mark.y) + 2.5f;
				mark.angle = angle;
				mark.width = (sand ? 17.0f : 18.0f) * size;
				mark.length = (sand ? 28.0f : 30.0f) * size;
				mark.born = tick;
				nextTrack = (nextTrack + 1) % 96;
				++added;
			}
		}
		glPushAttrib(GL_ALL_ATTRIB_BITS);
		glDisable(GL_TEXTURE_2D);
		glDisable(GL_LIGHTING);
		glDisable(GL_CULL_FACE);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);
		glBegin(GL_TRIANGLES);
		for (int i = 0; i < 96; ++i)
		{
			const SurfaceTrack& mark = tracks[i];
			if (mark.born == 0 || tick - mark.born >= 34000) continue;
			const float mx = mark.x - hx, my = mark.y - hy;
			if (mx * mx + my * my > 900.0f * 900.0f) continue;
			const float fade = 1.0f - (tick - mark.born) / 34000.0f;
			if (snow) glColor4f(0.16f, 0.20f, 0.28f, fade * 0.42f);
			else glColor4f(0.26f, 0.17f, 0.09f, fade * 0.36f);
			const float rightX = cosf(mark.angle), rightY = sinf(mark.angle);
			const float forwardX = sinf(mark.angle), forwardY = -cosf(mark.angle);
			for (int part = 0; part < 10; ++part)
			{
				const float a = part * 6.2831853f / 10.0f;
				const float b = (part + 1) * 6.2831853f / 10.0f;
				const float ax = cosf(a) * mark.width * 0.5f, ay = sinf(a) * mark.length * 0.5f;
				const float bx = cosf(b) * mark.width * 0.5f, by = sinf(b) * mark.length * 0.5f;
				glVertex3f(mark.x, mark.y, mark.z);
				glVertex3f(mark.x + rightX * ax + forwardX * ay, mark.y + rightY * ax + forwardY * ay, mark.z);
				glVertex3f(mark.x + rightX * bx + forwardX * by, mark.y + rightY * bx + forwardY * by, mark.z);
			}
		}
		glEnd();
		glPopAttrib();
	}

	void RenderServerClock()
	{
		if (!g_HasServerClock || (g_pOption != NULL && !g_pOption->IsServerClockEnabled()))
			return;
		// Mantem a janela de configuracoes limpa: o relogio nao e desenhado
		// por tras dela. Ao fechar o menu, volta na posicao salva.
		if (g_pNewUISystem != NULL && g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_OPTION))
			return;

		static bool loaded = false;
		static bool dragging = false;
		static int x = 0, y = 0;
		static float offsetX = 0.0f, offsetY = 0.0f;
		const int width = 132;
		const int height = 36;
		// MouseX/MouseY ja estao no mesmo espaco virtual 640x480 usado pela UI.
		// MouseRender dividido pela escala gerava uma segunda conversao e fazia
		// a area clicavel ficar distante da imagem do relogio.
		const float mouseX = (float)MouseX;
		const float mouseY = (float)MouseY;

		if (!loaded)
		{
			const int layoutVersion = GetPrivateProfileIntA("WorldVisual", "ClockLayoutVersion", 0, ".\\config.ini");
			if (layoutVersion < 2)
			{
				// Migra uma unica vez a posicao do relogio antigo. Encosta na
				// direita e deixa a lista de eventos do topo livre.
				x = 640 - width - 8;
				y = 52;
				char value[16];
				sprintf_s(value, "%d", x);
				WritePrivateProfileStringA("WorldVisual", "ClockX", value, ".\\config.ini");
				sprintf_s(value, "%d", y);
				WritePrivateProfileStringA("WorldVisual", "ClockY", value, ".\\config.ini");
				WritePrivateProfileStringA("WorldVisual", "ClockLayoutVersion", "2", ".\\config.ini");
			}
			else
			{
				x = GetPrivateProfileIntA("WorldVisual", "ClockX", 640 - width - 8, ".\\config.ini");
				y = GetPrivateProfileIntA("WorldVisual", "ClockY", 52, ".\\config.ini");
			}
			loaded = true;
		}

		const bool hover = (mouseX >= x && mouseX < x + width && mouseY >= y && mouseY < y + height);
		if (hover)
			MouseOnWindow = true;

		if (!dragging && hover && (SEASON3B::IsPress(VK_LBUTTON) || MouseLButtonPush))
		{
			dragging = true;
			offsetX = mouseX - x;
			offsetY = mouseY - y;
			MouseLButtonPush = false;
		}

		if (dragging)
		{
			MouseOnWindow = true;
			if (MouseLButton)
			{
				x = (int)(mouseX - offsetX);
				y = (int)(mouseY - offsetY);
			}
			else
			{
				dragging = false;
				char value[16];
				sprintf_s(value, "%d", x);
				WritePrivateProfileStringA("WorldVisual", "ClockX", value, ".\\config.ini");
				sprintf_s(value, "%d", y);
				WritePrivateProfileStringA("WorldVisual", "ClockY", value, ".\\config.ini");
			}
		}

		const int seconds = (g_ServerClockSeconds + (GetTickCount() - g_ServerClockReceivedAt) / 1000) % 86400;
		char timeLabel[16];
		sprintf_s(timeLabel, "%02d:%02d:%02d", seconds / 3600, (seconds / 60) % 60, seconds % 60);
		char clockLabel[40];
		sprintf_s(clockLabel, "SERVIDOR  %s", timeLabel);

		// Painel exclusivo do relogio: mesma familia visual do menu, mas sem
		// reaproveitar dropdown e sem esticar seus cantos.
		EnableAlphaTest();
		glColor4f(1.f, 1.f, 1.f, 1.f);
		RenderBitmap(BITMAP_SERVER_TIME_PANEL,
			(float)x, (float)y, (float)width, (float)height,
			0.f, 0.f, 340.0f / 512.0f, 96.0f / 128.0f, true, true, 0.f);

		g_pRenderText->SetFont(g_hFontBold);
		g_pRenderText->SetBgColor(0, 0, 0, 0);
		g_pRenderText->SetTextColor(232, 190, 91, 255);
		g_pRenderText->RenderFont(x + 5, y + 11, "S", 22, 12, RT3_SORT_CENTER);
		if (hover || dragging)
			g_pRenderText->SetTextColor(255, 238, 196, 255);
		else
			g_pRenderText->SetTextColor(245, 229, 202, 255);
		g_pRenderText->SetFont(g_hFont);
		g_pRenderText->RenderFont(x + 31, y + 12, clockLabel, 94, 11, RT3_SORT_CENTER);
	}

	void RenderServerVisualTimeTint()
	{
		if (g_PostProcessPipeline.IsCapturing()) return;
		// The terrain now receives the night lighting directly. Do not draw a
		// fullscreen dark layer over items, characters and the interface.
		static DWORD lastOptionRead = 0;
		static bool sceneNightOnly = true;
		const DWORD tick = GetTickCount();
		if (lastOptionRead == 0 || tick - lastOptionRead >= 1000)
		{
			sceneNightOnly = GetPrivateProfileIntA("WorldLighting", "SceneNightOnly", 1, ".\\config.ini") != 0;
			lastOptionRead = tick;
		}
		if (sceneNightOnly) return;
		if (!g_AtmosphereTimeEnabled)
		{
			return;
		}

		const float hour = g_AtmosphereHour;
		const float pi = 3.14159265358979323846f;
		float daylight = (cosf((hour - 12.0f) * pi / 12.0f) + 1.0f) * 0.5f;
		float opacity = (1.0f - daylight) * 0.32f;
		if (opacity < 0.005f)
		{
			return;
		}

		BeginBitmap();
		EnableAlphaBlend();
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glColor4f(0.045f, 0.075f, 0.15f, opacity);
		RenderColor(0.0f, 0.0f, (float)WindowWidth, (float)WindowHeight, 0.0f, 0, false);
		EndRenderColor();
		DisableAlphaBlend();
		EndBitmap();
	}
}

void ShutdownServerSkyBox()
{
	ReleaseServerSkyBoxTextures();
}

float GetServerVisualHour()
{
	return g_AtmosphereHour;
}

bool GetServerVisualSunShadow(float& angleDegrees, float& lengthScale, float& opacityScale)
{
	if (!g_AtmosphereTimeEnabled || !g_AtmosphereDynamicSunShadows)
		return false;
	angleDegrees = g_AtmosphereShadowAngle;
	lengthScale = g_AtmosphereShadowLength;
	opacityScale = g_AtmosphereShadowOpacity;
	return true;
}

void GetServerVisualSunDirection(float& x, float& y, float& z)
{
	x = g_AtmosphereSunDirX;
	y = g_AtmosphereSunDirY;
	z = g_AtmosphereSunDirZ;
}

void GetServerAtmosphereVisual(float& hour, float& rain, float& storm, float& dust, float& snow, float& fog)
{
	hour = g_AtmosphereHour;
	rain = g_AtmosphereRain;
	storm = g_AtmosphereStorm;
	dust = g_AtmosphereDust;
	snow = g_AtmosphereSnow;
	fog = g_AtmosphereFog;
}

float GetServerSnowCoverVisual()
{
	return g_ServerSnowCoverVisual;
}

void SetServerVisualTime(bool automatic, int hour, int minute, int second)
{
	if (hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59)
	{
		return;
	}
	g_ServerVisualTimeAutomatic = automatic;
	g_ServerVisualSeconds = hour * 3600 + minute * 60 + second;
	g_ServerVisualTimeReceivedAt = GetTickCount();
	g_HasServerVisualTime = true;
}

void SetServerClockTime(int hour, int minute, int second)
{
	if (hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59)
		return;
	g_ServerClockSeconds = hour * 3600 + minute * 60 + second;
	g_ServerClockReceivedAt = GetTickCount();
	g_HasServerClock = true;
}
BYTE byte_E8CB13;
int TimeRemain = 40;

extern steady_clock_ ChatTime;
extern int  GrabScreen;
extern int g_iNoMouseTime;
extern bool g_bEnterPressed;
extern int WaterTextureNumber;
extern void RenderLorenciaWaterReflections();

bool MoveMainCamera();
void MoveCharacter(CHARACTER* c, OBJECT* o);
extern GLvoid KillGLWindow(GLvoid);
extern BOOL Util_CheckOption(char* lpszCommandLine, unsigned char cOption, char* lpszString);

void StopMusic()
{
	for (int i = 0; i < NUM_MUSIC; ++i)
	{
		StopMp3(g_lpszMp3[i]);
	}
}

void SetCreateMaxClass(BYTE MaxClass)
{
	byte_E8CB13 = MaxClass;
}

BYTE GetCreateMaxClass()
{
	return byte_E8CB13;
}

bool CheckAbuseFilter(char* Text, bool bCheckSlash)
{
	if (bCheckSlash == true)
	{
		if (Text[0] == '/')
		{
			return false;
		}
	}

	int icntText = 0;
	char TmpText[2048];
	for (int i = 0; i < (int)strlen(Text); ++i)
	{
		if (Text[i] != 32)
		{
			TmpText[icntText] = Text[i];
			icntText++;
		}
	}
	TmpText[icntText] = 0;

	for (int i = 0; i < AbuseFilterNumber; i++)
	{
		if (FindText(TmpText, AbuseFilter[i]))
		{
			return true;
		}
	}
	return false;
}

bool CheckAbuseNameFilter(char* Text)
{
	int icntText = 0;
	char TmpText[256];
	for (int i = 0; i < (int)strlen(Text); ++i)
	{
		if (Text[i] != 32)
		{
			TmpText[icntText] = Text[i];
			icntText++;
		}
	}
	TmpText[icntText] = 0;

	for (int i = 0; i < AbuseNameFilterNumber; i++)
	{
		if (FindText(TmpText, AbuseNameFilter[i]))
		{
			return true;
		}
	}
	return false;
}

bool CheckName()
{
	if (CheckAbuseNameFilter(InputText[0]) || CheckAbuseFilter(InputText[0]) ||
		FindText(InputText[0], " ") || FindText(InputText[0], "��") ||
		FindText(InputText[0], ".") || FindText(InputText[0], "��") || FindText(InputText[0], "��") ||
		FindText(InputText[0], "Webzen") || FindText(InputText[0], "WebZen") || FindText(InputText[0], "webzen") || FindText(InputText[0], "WEBZEN") ||
		FindText(InputText[0], GlobalText[457]) || FindText(InputText[0], GlobalText[458]))
		return true;
	return false;
}

#ifdef MOVIE_DIRECTSHOW
void MovieScene(HDC hDC)
{
	if (g_pMovieScene->GetPlayNum() == 0)
	{
		g_pMovieScene->InitOpenGLClear(hDC);

		g_pMovieScene->Initialize_DirectShow(gwinhandle->GethWnd(), MOVIE_FILE_WMV);

		if (g_pMovieScene->IsFile() == FALSE || g_pMovieScene->IsFailDirectShow() == TRUE)
		{
			g_pMovieScene->Destroy();
			SAFE_DELETE(g_pMovieScene);
			SceneFlag = WEBZEN_SCENE;
			return;
		}

		g_pMovieScene->PlayMovie();

		if (g_pMovieScene->IsEndMovie())
		{
			g_pMovieScene->Destroy();
			SAFE_DELETE(g_pMovieScene);
			SceneFlag = WEBZEN_SCENE;
			return;
		}
		else
		{
			if (HIBYTE(GetAsyncKeyState(VK_ESCAPE)) == 128 || HIBYTE(GetAsyncKeyState(VK_RETURN)) == 128)
			{
				g_pMovieScene->Destroy();
				SAFE_DELETE(g_pMovieScene);
				SceneFlag = WEBZEN_SCENE;
				return;
			}
		}
	}
	else
	{
		g_pMovieScene->Destroy();
		SAFE_DELETE(g_pMovieScene);
		SceneFlag = WEBZEN_SCENE;
		return;
	}
}
#endif // MOVIE_DIRECTSHOW

bool InitWebzenScene = false;

void CreateWebzenScene()
{
	CUIMng& rUIMng = CUIMng::Instance();

	OpenFont();
	ClearInput();

	FogEnable = false;

	LoadBitmap("Interface\\New_lo_back_01.jpg", BITMAP_TITLE, GL_LINEAR);
	LoadBitmap("Interface\\New_lo_back_02.jpg", BITMAP_TITLE + 1, GL_LINEAR);
	LoadBitmap("Interface\\MU_TITLE.tga", BITMAP_TITLE + 2, GL_LINEAR);
	LoadBitmap("Interface\\lo_121518.tga", BITMAP_TITLE + 3, GL_LINEAR);
	LoadBitmap("Interface\\New_lo_webzen_logo.tga", BITMAP_TITLE + 4, GL_LINEAR);
	LoadBitmap("Interface\\lo_back_s5_03.jpg", BITMAP_TITLE + 6, GL_LINEAR);
	LoadBitmap("Interface\\lo_back_s5_04.jpg", BITMAP_TITLE + 7, GL_LINEAR);
	LoadBitmap("Interface\\HUD\\lo_download.jpg", BITMAP_TITLE + 5, GL_LINEAR, GL_REPEAT);

	if (rand() % 100 <= 70)
	{
		LoadBitmap("Interface\\lo_back_im01.jpg", BITMAP_TITLE + 8, GL_LINEAR);
		LoadBitmap("Interface\\lo_back_im02.jpg", BITMAP_TITLE + 9, GL_LINEAR);
		LoadBitmap("Interface\\lo_back_im03.jpg", BITMAP_TITLE + 10, GL_LINEAR);
		LoadBitmap("Interface\\lo_back_im04.jpg", BITMAP_TITLE + 11, GL_LINEAR);
		LoadBitmap("Interface\\lo_back_im05.jpg", BITMAP_TITLE + 12, GL_LINEAR);
		LoadBitmap("Interface\\lo_back_im06.jpg", BITMAP_TITLE + 13, GL_LINEAR);
	}
	else
	{
		LoadBitmap("Interface\\lo_back_s5_im01.jpg", BITMAP_TITLE + 8, GL_LINEAR);
		LoadBitmap("Interface\\lo_back_s5_im02.jpg", BITMAP_TITLE + 9, GL_LINEAR);
		LoadBitmap("Interface\\lo_back_s5_im03.jpg", BITMAP_TITLE + 10, GL_LINEAR);
		LoadBitmap("Interface\\lo_back_s5_im04.jpg", BITMAP_TITLE + 11, GL_LINEAR);
		LoadBitmap("Interface\\lo_back_s5_im05.jpg", BITMAP_TITLE + 12, GL_LINEAR);
		LoadBitmap("Interface\\lo_back_s5_im06.jpg", BITMAP_TITLE + 13, GL_LINEAR);
	}

	rUIMng.CreateTitleSceneUI();


#ifdef SOCKET_LAUNCHER
	GMConnectHex->Initialize();

	GMConnectHex->SetUpdateVersion(true);

	if (CreateSocket(szServerIpAddress, g_ServerPort))
	{
		SendRequestServerHWID();
		SendRequestServerUpdate(GMConnectHex->GetClientVersion());
	}
	else
	{
		GMConnectHex->SetUpdateVersion(false);
	}
#endif // SOCKET_LAUNCHER
}

void WebzenScene(HDC hDC)
{
	if (!InitWebzenScene)
	{
		CreateWebzenScene();
		InitWebzenScene = true;
	}

#ifdef SOCKET_LAUNCHER
	if (GMConnectHex->GetUpdateVersion() == false)
	{
		CUIMng& rUIMng = CUIMng::Instance();

		LoadBitmap("Interface\\lo_lo.jpg", BITMAP_TITLE + 5, GL_LINEAR, GL_REPEAT);

		EnableAlphaTest();

		OpenBasicData(hDC);

		g_pNewUISystem->LoadMainSceneInterface();

		CUIMng::Instance().RenderTitleSceneUI(hDC, 11, 12);

		rUIMng.ReleaseTitleSceneUI();

		DeleteBitmap(BITMAP_TITLE);
		DeleteBitmap(BITMAP_TITLE + 1);
		DeleteBitmap(BITMAP_TITLE + 2);
		DeleteBitmap(BITMAP_TITLE + 3);
		DeleteBitmap(BITMAP_TITLE + 4);
		DeleteBitmap(BITMAP_TITLE + 5);

		for (int i = 6; i < 14; ++i)
			DeleteBitmap(BITMAP_TITLE + i);

		g_ErrorReport.Write("> Loading ok.\r\n");

		SceneFlag = LOG_IN_SCENE;
	}
	else
	{
		GMConnectHex->RenderInterface(hDC);

		//if (SocketClient.GetSocket() == INVALID_SOCKET)
		//{
		//}
	}
#else
	CUIMng& rUIMng = CUIMng::Instance();

	EnableAlphaTest();

	OpenBasicData(hDC);

	g_pNewUISystem->LoadMainSceneInterface();

	CUIMng::Instance().RenderTitleSceneUI(hDC, 11, 12);

	rUIMng.ReleaseTitleSceneUI();
	DeleteBitmap(BITMAP_TITLE);
	DeleteBitmap(BITMAP_TITLE + 1);
	DeleteBitmap(BITMAP_TITLE + 2);
	DeleteBitmap(BITMAP_TITLE + 3);
	DeleteBitmap(BITMAP_TITLE + 4);
	DeleteBitmap(BITMAP_TITLE + 5);

	for (int i = 6; i < 14; ++i)
		DeleteBitmap(BITMAP_TITLE + i);

	g_ErrorReport.Write("> Loading ok.\r\n");

	SceneFlag = LOG_IN_SCENE;
#endif // SOCKET_LAUNCHER
}

void DeleteCharacter()
{
	CHARACTER* pCharacter = gmCharacters->GetCharacter(SelectedHero);

	if (pCharacter)
	{
		if (g_iChatInputType == 1)
		{
			g_pSinglePasswdInputBox->GetText(InputText[0]);
			g_pSinglePasswdInputBox->SetText(NULL);
			g_pSinglePasswdInputBox->SetState(UISTATE_HIDE);
		}
		SendRequestDeleteCharacter(pCharacter->ID, InputText[0]);

		MenuStateCurrent = MENU_DELETE_LEFT;
		MenuStateNext = MENU_NEW_DOWN;
		PlayBuffer(SOUND_MENU01);

		ClearInput();
		InputEnable = false;
	}
	SelectedHero = -1;
}

bool IsEnterPressed()
{
	return g_bEnterPressed;
}

void SetEnterPressed(bool enterpressed) {
	g_bEnterPressed = enterpressed;
}

BOOL CheckOptionMouseClick(int iOptionPos_y, BOOL bPlayClickSound = TRUE)
{
	if (CheckMouseIn((gwinhandle->GetScreenX() - 120) / 2, 30 + iOptionPos_y, 120, 22) && MouseLButtonPush)
	{
		MouseLButtonPush = false;
		MouseUpdateTime = 0;
		MouseUpdateTimeMax = 6;
		if (bPlayClickSound == TRUE) PlayBuffer(SOUND_CLICK01);
		return TRUE;
	}
	return FALSE;
}

int SeparateTextIntoLines(const char* lpszText, char* lpszSeparated, int iMaxLine, int iLineSize)
{
	int iLine = 0;
	const char* lpLineStart = lpszText;
	char* lpDst = lpszSeparated;
	const char* lpSpace = NULL;
	int iMbclen = 0;
	for (const char* lpSeek = lpszText; *lpSeek; lpSeek += iMbclen, lpDst += iMbclen)
	{
		iMbclen = _mbclen((unsigned char*)lpSeek);
		if (iMbclen + (int)(lpSeek - lpLineStart) >= iLineSize)
		{
			if (lpSpace && (int)(lpSeek - lpSpace) < ((10 < iLineSize / 2) ? 10 : iLineSize / 2))
			{
				lpDst -= (lpSeek - lpSpace - 1);
				lpSeek = lpSpace + 1;
			}

			lpLineStart = lpSeek;
			*lpDst = '\0';
			if (iLine >= iMaxLine - 1)
			{
				break;
			}
			++iLine;
			lpDst = lpszSeparated + iLine * iLineSize;
			lpSpace = NULL;
		}

		memcpy(lpDst, lpSeek, iMbclen);
		if (*lpSeek == ' ')
		{
			lpSpace = lpSeek;
		}
	}
	*lpDst = '\0';

	return (iLine + 1);
}

void SetEffectVolumeLevel(int level)
{
	if (level > 9)
		level = 9;
	if (level < 0)
		level = 0;

	if (level == 0)
	{
		SetMasterVolume(-10000);
	}
	else
	{
		long vol = -2000 * log10(10.f / float(level));
		SetMasterVolume(vol);
	}
}

void SetViewPortLevel(int Wheel)
{
	// Este bloco aqui � do Zoom (C�mera)
	if ((HIBYTE(GetAsyncKeyState(VK_CONTROL)) == 128))
	{
		if (Wheel > 0)
			g_shCameraLevel--;
		else if (Wheel < 0)
			g_shCameraLevel++;

		MouseWheel = 0;

		if (g_shCameraLevel > 4)
			g_shCameraLevel = 4;
		if (g_shCameraLevel < 0)
			g_shCameraLevel = 0;
	} // <-- A chave do CONTROL fecha aqui

	// COLOQUE AQUI FORA:
	//TeleportMapaADM();
}

void RenderInfomation3D()
{
	bool Success = false;

	if (((ErrorMessage == MESSAGE_TRADE_CHECK || ErrorMessage == MESSAGE_CHECK) && AskYesOrNo == 1)
		|| ErrorMessage == MESSAGE_USE_STATE
		|| ErrorMessage == MESSAGE_USE_STATE2)
	{
		Success = true;
	}

	if (ErrorMessage == MESSAGE_TRADE_CHECK && AskYesOrNo == 5)
	{
		Success = true;
	}
	if (ErrorMessage == MESSAGE_PERSONALSHOP_WARNING)
	{
		Success = true;
	}

	if (Success)
	{
		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glLoadIdentity();
		glViewport2(0, 0, WindowWidth, WindowHeight);
		gluPerspective2(1.f, (float)(WindowWidth) / (float)(WindowHeight), CameraViewNear, CameraViewFar);
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadIdentity();
		GetOpenGLMatrix(CameraMatrix);
		EnableDepthTest();
		EnableDepthMask();

		float Width, Height;
		float x = (gwinhandle->GetScreenX() - 150) / 2;
		float y;
		if (ErrorMessage == MESSAGE_TRADE_CHECK)
		{
			y = 60 + 55;
		}
		else
		{
			y = 60 + 55;
		}

		Width = 40.f; Height = 60.f;
		int iRenderType = ErrorMessage;
		if (AskYesOrNo == 5)
			iRenderType = MESSAGE_USE_STATE;
		switch (iRenderType)
		{
		case MESSAGE_USE_STATE:
		case MESSAGE_USE_STATE2:
		case MESSAGE_PERSONALSHOP_WARNING:
			RenderItem3D(x, y, Width, Height, TargetItem.Type, TargetItem.Level, TargetItem.Option1, TargetItem.ExtOption, true);
			break;

		default:
			RenderItem3D(x, y, Width, Height, PickItem.Type, PickItem.Level, PickItem.Option1, PickItem.ExtOption, true);
			break;
		}

		glMatrixMode(GL_MODELVIEW);
		glPopMatrix();
		glMatrixMode(GL_PROJECTION);
		glPopMatrix();
		UpdateMousePositionn();
	}
}

void RenderInfomation()
{
	RenderNotices();

	CUIMng::Instance().Render();

	if (SceneFlag == LOG_IN_SCENE || SceneFlag == CHARACTER_SCENE)
	{
		RenderCursor();
	}

	RenderInfomation3D();
}

BOOL ShowCheckBox(int num, int index, int message)
{
	if (message == MESSAGE_USE_STATE || message == MESSAGE_USE_STATE2)
	{
		char Name[50] = { 0, };
		if (TargetItem.Type == ITEM_HELPER + 15)
		{
			switch ((TargetItem.Level >> 3) & 15)
			{
			case 0:sprintf(Name, "%s", GlobalText[168]); break;
			case 1:sprintf(Name, "%s", GlobalText[169]); break;
			case 2:sprintf(Name, "%s", GlobalText[167]); break;
			case 3:sprintf(Name, "%s", GlobalText[166]); break;
			case 4:sprintf(Name, "%s", GlobalText[1900]); break;
			}
		}

		if (message == MESSAGE_USE_STATE2)
			sprintf(g_lpszMessageBoxCustom[0], "( %s%s )", Name, GlobalText[1901]);
		else
			sprintf(g_lpszMessageBoxCustom[0], "( %s )", Name);

		num++;
		for (int i = 1; i < num; ++i)
		{
			sprintf(g_lpszMessageBoxCustom[i], GlobalText[index]);
		}
		g_iNumLineMessageBoxCustom = num;
	}
	else if (message == MESSAGE_PERSONALSHOP_WARNING)
	{
		char szGold[256];
		ConvertGold(InputGold, szGold);
		sprintf(g_lpszMessageBoxCustom[0], GlobalText[index], szGold);

		for (int i = 1; i < num; ++i)
		{
			sprintf(g_lpszMessageBoxCustom[i], GlobalText[index + i]);
		}
		g_iNumLineMessageBoxCustom = num;
	}
	else if (message == MESSAGE_CHAOS_CASTLE_CHECK)
	{
		g_iNumLineMessageBoxCustom = 0;
		for (int i = 0; i < num; ++i)
		{
			g_iNumLineMessageBoxCustom += SeparateTextIntoLines(GlobalText[index + i], g_lpszMessageBoxCustom[g_iNumLineMessageBoxCustom], NUM_LINE_CMB, MAX_LENGTH_CMB);
		}
	}
	else if (message == MESSAGE_GEM_INTEGRATION3)
	{
		char tBuf[MAX_GLOBAL_TEXT_STRING];
		char tLines[2][30];
		for (int t = 0; t < 2; ++t) memset(tLines[t], 0, 20);
		g_iNumLineMessageBoxCustom = 0;
		if (COMGEM::isComMode())
		{
			if (COMGEM::m_cGemType == 0) sprintf(tBuf, GlobalText[1809], GlobalText[1806], COMGEM::m_cCount);
			else sprintf(tBuf, GlobalText[1809], GlobalText[1807], COMGEM::m_cCount);

			g_iNumLineMessageBoxCustom += SeparateTextIntoLines(tBuf,
				tLines[g_iNumLineMessageBoxCustom], 2, 30);

			for (int t = 0; t < 2; ++t) strcpy(g_lpszMessageBoxCustom[t], tLines[t]);

			sprintf(g_lpszMessageBoxCustom[g_iNumLineMessageBoxCustom], GlobalText[1810], COMGEM::m_iValue);
			++g_iNumLineMessageBoxCustom;

		}
		else
		{
			int t_GemLevel = COMGEM::GetUnMixGemLevel() + 1;
			if (COMGEM::m_cGemType == 0) sprintf(tBuf, GlobalText[1813], GlobalText[1806], t_GemLevel);
			else sprintf(tBuf, GlobalText[1813], GlobalText[1807], t_GemLevel);

			g_iNumLineMessageBoxCustom += SeparateTextIntoLines(tBuf,
				tLines[g_iNumLineMessageBoxCustom], 2, 30);

			for (int t = 0; t < 2; ++t) strcpy(g_lpszMessageBoxCustom[t], tLines[t]);

			sprintf(g_lpszMessageBoxCustom[g_iNumLineMessageBoxCustom], GlobalText[1814], COMGEM::m_iValue);
			++g_iNumLineMessageBoxCustom;
		}
	}
	else if (message == MESSAGE_CANCEL_SKILL)
	{
		char tBuf[MAX_GLOBAL_TEXT_STRING];
		sprintf(tBuf, "%s%s", SkillAttribute[index].Name, GlobalText[2046]);
		g_iNumLineMessageBoxCustom = SeparateTextIntoLines(tBuf, g_lpszMessageBoxCustom[0], 2, MAX_LENGTH_CMB);
		g_iCancelSkillTarget = index;
	}
	else
	{
		for (int i = 0; i < num; ++i)
		{
			strcpy(g_lpszMessageBoxCustom[i], GlobalText[index + i]);
		}
		g_iNumLineMessageBoxCustom = num;
	}

	ZeroMemory(g_iCustomMessageBoxButton, NUM_BUTTON_CMB * NUM_PAR_BUTTON_CMB * sizeof(int));

	int iOkButton[5] = { 1,  21, 90, 70, 21 };
	int iCancelButton[5] = { 3, 120, 90, 70, 21 };

	if (message == MESSAGE_USE_STATE2)
	{
		iOkButton[1] = 22;
		iOkButton[2] = 92;	// y
		iOkButton[3] = 49;
		iOkButton[4] = 16;

		iCancelButton[1] = 82;
		iCancelButton[2] = 92;	// y
		iCancelButton[3] = 49;
		iCancelButton[4] = 16;

		g_iCustomMessageBoxButton_Cancel[0] = 5;
		g_iCustomMessageBoxButton_Cancel[1] = 142;	// x
		g_iCustomMessageBoxButton_Cancel[2] = 92;	// y
		g_iCustomMessageBoxButton_Cancel[3] = 49;	// width
		g_iCustomMessageBoxButton_Cancel[4] = 16;	// height
	}

	if (message == MESSAGE_CHAOS_CASTLE_CHECK)
	{
		iOkButton[2] = 120;
		iCancelButton[2] = 120;
	}

	memcpy(g_iCustomMessageBoxButton[0], iOkButton, 5 * sizeof(int));
	memcpy(g_iCustomMessageBoxButton[1], iCancelButton, 5 * sizeof(int));

	return true;
}

void MoveCharacterCamera(vec3_t Origin, vec3_t Position, vec3_t Angle)
{
	vec3_t TransformPosition;
	CameraAngle[0] = 0.f;
	CameraAngle[1] = 0.f;
	CameraAngle[2] = Angle[2];
	float Matrix[3][4];
	AngleMatrix(CameraAngle, Matrix);
	VectorIRotate(Position, Matrix, TransformPosition);
	VectorAdd(Origin, TransformPosition, CameraPosition);
	CameraAngle[0] = Angle[0];
}

void MoveCamera()
{
	if (CCameraMove::GetInstancePtr()->IsTourMode())
	{
		return;
	}

	if (CurrentCameraCount == -1)
	{
		for (int i = 0; i < 3; i++)
		{
			CurrentCameraPosition[i] = CameraWalk[i];
			CurrentCameraAngle[i] = CameraWalk[i + 3];
		}
		CurrentCameraNumber = 1;
		CurrentCameraWalkType = 1;

		for (int i = 0; i < 3; i++)
		{
			CurrentCameraWalkDelta[i] = (CameraWalk[CurrentCameraNumber * 6 + i] - CurrentCameraPosition[i]) / 128;
			CurrentCameraWalkDelta[i + 3] = (CameraWalk[CurrentCameraNumber * 6 + i + 3] - CurrentCameraAngle[i]) / 128;
		}
	}
	CurrentCameraCount++;
	if ((CameraWalkCut == 0 && CurrentCameraCount >= 40) || (CameraWalkCut > 0 && CurrentCameraCount >= 128))
	{
		CurrentCameraCount = 0;
		if (CameraWalkCut == 0)
		{
			CameraWalkCut = 1;
		}
		else
		{
			if (SceneFlag == LOG_IN_SCENE)
			{
				CurrentCameraNumber = rand() % 4 + 1;
				CurrentCameraWalkType = rand() % 2;
			}
			else
			{
				CurrentCameraNumber = 5;
				CurrentCameraWalkType = 0;
			}
		}
		for (int i = 0; i < 3; i++)
		{
			CurrentCameraWalkDelta[i] = (CameraWalk[CurrentCameraNumber * 6 + i] - CurrentCameraPosition[i]) / 128;
			CurrentCameraWalkDelta[i + 3] = (CameraWalk[CurrentCameraNumber * 6 + i + 3] - CurrentCameraAngle[i]) / 128;
		}
	}
	if (CurrentCameraWalkType == 0)
	{
		for (int i = 0; i < 3; i++)
		{
			CurrentCameraPosition[i] += (CameraWalk[CurrentCameraNumber * 6 + i] - CurrentCameraPosition[i]) / 6;
			CurrentCameraAngle[i] += (CameraWalk[CurrentCameraNumber * 6 + i + 3] - CurrentCameraAngle[i]) / 6;
		}
	}
	else
	{
		for (int i = 0; i < 2; i++)
		{
			CurrentCameraPosition[i] += CurrentCameraWalkDelta[i];
		}
	}
	CameraFOV = 45.f;
	vec3_t Position;
	Vector(0.f, 0.f, 0.f, Position);
	MoveCharacterCamera(Position, CurrentCameraPosition, CurrentCameraAngle);
}

void StartGame()
{
	CHARACTER* pCharacter = gmCharacters->GetCharacter(SelectedHero);

	if (pCharacter)
	{
		if (CTLCODE_01BLOCKCHAR & pCharacter->CtlCode)
		{
			CUIMng::Instance().PopUpMsgWin(MESSAGE_BLOCKED_CHARACTER);
		}
		else
		{
			CharacterAttribute->Level = pCharacter->Level;
			CharacterAttribute->Class = pCharacter->Class;
			CharacterAttribute->Skin = pCharacter->Skin;

			::strcpy(CharacterAttribute->Name, pCharacter->ID);

			::ReleaseCharacterSceneData();
			InitLoading = false;
			SceneFlag = LOADING_SCENE;
		}
	}
}

CHARACTER* CreateHeroLogIn(int Index, int Class, int Skin, float fScale, vec3_t Angle, vec3_t Position)
{
	CHARACTER* pCha = CreateHero(Index, Class, Skin);

	if (pCha)
	{
		OBJECT* o = &pCha->Object;

		o->Scale = fScale;
		VectorCopy(Angle, o->Angle);
		VectorCopy(Position, o->Position);
	}

	return pCha;
}

void CharacterTeleport(int key)
{
	CHARACTER* pCharacter;
	for (int i = 0; i < 5; i++)
	{
		pCharacter = gmCharacters->GetCharacter(i);

		OBJECT* pObject = &pCharacter->Object;

		if (pObject->Live)
		{
			if (key == i)
			{
				pObject->Position[0] = 13629.f;
				pObject->Position[1] = 12339.f;
				pObject->Angle[2] = 90.f;
			}
			else
			{
				pObject->Position[0] = 8046.f;
				pObject->Position[1] = 19400.f;
				pObject->Angle[2] = 90.f;
			}
		}
	}
}

void RenderInterfaceEdge()
{
	float Width, Height;
	float WindowX, WindowY;

	EnableAlphaTest();
	glColor3f(1.f, 1.f, 1.f);
	//interface edge
	WindowX = 320.0; WindowY = 0.0; Width = 256.0; Height = 86.0;
	RenderNoBitmap(BITMAP_LOG_IN + 8, WindowX, WindowY, Width, Height, 0.f, 0.f, Width / 256.f, Height / 128.f);
	WindowX = 64.0; WindowY = 0.0; Width = 256.0; Height = 86.0;
	RenderNoBitmap(BITMAP_LOG_IN + 8, WindowX, WindowY, Width, Height, Width / 256.f, 0.f, -Width / 256.f, Height / 128.f);

	WindowX = 576.0; WindowY = 0.0; Width = 64.0; Height = 256.0;
	RenderNoBitmap(BITMAP_LOG_IN + 9, WindowX, WindowY, Width, Height, 0.f, 0.f, Width / 64.f, Height / 256.f);
	WindowX = 0.0; WindowY = 0.0; Width = 64.0; Height = 256;
	RenderNoBitmap(BITMAP_LOG_IN + 9, WindowX, WindowY, Width, Height, Width / 64.f, 0.f, -Width / 64.f, Height / 256.f);

	WindowX = 576.0; WindowY = 256.0; Width = 64.0; Height = 224.0;
	RenderNoBitmap(BITMAP_LOG_IN + 10, WindowX, WindowY, Width, Height, 0.f, 0.f, Width / 64.f, Height / 256.f);
	WindowX = 0.0; WindowY = 256.0; Width = 64.0; Height = 224.0;
	RenderNoBitmap(BITMAP_LOG_IN + 10, WindowX, WindowY, Width, Height, Width / 64.f, 0.f, -Width / 64.f, Height / 256.f);

	WindowX = 320.0; WindowY = 448.0; Width = 256.0; Height = 32.0;
	RenderNoBitmap(BITMAP_LOG_IN + 14, WindowX, WindowY, Width, Height, 0.f, 0.f, Width / 256.f, Height / 32.f);
	WindowX = 64.0; WindowY = 448.0; Width = 256.0; Height = 32.0;
	RenderNoBitmap(BITMAP_LOG_IN + 14, WindowX, WindowY, Width, Height, Width / 256.f, 0.f, -Width / 256.f, Height / 32.f);
}

void CreateCharacterScene()
{
	g_pNewUIMng->ResetActiveUIObj();

	EnableMainRender = true;
	MouseOnWindow = false;
	ErrorMessage = NULL;

	if (gmProtect->SceneCharacter == 1)
	{
		vec3_t Angle, Position;

		World = -1;
		CurrentCameraPosition[0] = -80.0;
		CurrentCameraPosition[1] = 0.0;
		CurrentCameraPosition[2] = -10.0;
		CurrentCameraWalkType = 0;
		CurrentCameraNumber = 5;
		CurrentCameraCount = 0;
		CurrentCameraAngle[0] = 200.0;
		CurrentCameraAngle[1] = -800.0;
		CurrentCameraAngle[2] = 300.0;

		OpenCharacterSceneData();

		Vector(0.0, 0.0, 0.0, Angle);
		Vector(0.0, 0.0, 0.0, Position);
		CreateObject(MODEL_CARD, Position, Angle, 1.1f);
	}
	else
	{
		if (gmProtect->SceneCharacter == 2)
		{
			World = WD_54CHARACTERSCENE;
		}
		else if (gmProtect->SceneCharacter == 3)
		{
			World = WD_78NEW_CHARACTER_SCENE;
		}
		else if (gmProtect->SceneCharacter == 4)
		{
			World = WD_93NEW_CHARACTER_SCENE;
		}
		else
		{
			World = WD_74NEW_CHARACTER_SCENE;
		}

		gMapManager->LoadWorld(World);

		OpenCharacterSceneData();
	}

	CreateCharacterPointer(&CharacterView, MODEL_FACE + 1, 0, 0);
	CharacterView.Class = 1;
	CharacterView.Object.Kind = 0;

	SelectedHero = -1;
	CUIMng::Instance().CreateCharacterScene();

	ClearInventory();
	CharacterAttribute->SkillNumber = 0;

	for (int i = 0; i < MAX_MAGIC; i++)
		CharacterAttribute->Skill[i] = 0;

	for (int i = EQUIPMENT_WEAPON_RIGHT; i < EQUIPMENT_HELPER; i++)
		CharacterMachine->Equipment[i].Level = 0;

	g_pNewUISystem->HideAll();

	g_iKeyPadEnable = 0;
	GuildInputEnable = false;
	TabInputEnable = false;
	GoldInputEnable = false;
	InputEnable = true;
	ClearInput();
	InputIndex = 0;
	InputTextWidth = 90;
	InputNumber = 1;

	for (int i = 0; i < MAX_WHISPER; i++)
	{
		g_pChatListBox->AddText("", "", SEASON3B::TYPE_WHISPER_MESSAGE);
	}

	HIMC hIMC = ImmGetContext(gwinhandle->GethWnd());
	DWORD Conversion, Sentence;

	Conversion = IME_CMODE_NATIVE;
	Sentence = IME_SMODE_NONE;

	g_bIMEBlock = FALSE;
	RestoreIMEStatus();
	ImmSetConversionStatus(hIMC, Conversion, Sentence);
	ImmGetConversionStatus(hIMC, &g_dwBKConv, &g_dwBKSent);
	SaveIMEStatus();
	ImmReleaseContext(gwinhandle->GethWnd(), hIMC);
	g_bIMEBlock = TRUE;

	g_ErrorReport.Write("> Character scene init success.\r\n");
}

void NewMoveCharacterScene()
{
	if (CurrentProtocolState >= RECEIVE_CHARACTERS_LIST)
	{
		if (!InitCharacterScene)
		{
			InitCharacterScene = true;
			CreateCharacterScene();
		}

		if (gmProtect->SceneCharacter == 1)
		{
			MoveObjects();
			gGoboidManager->MoveBugs();

			MoveCharactersClient();
			MoveCharacterClient(&CharacterView);
			MoveEffects();
			MoveJoints();
			MoveParticles();
		}
		else
		{
			InitTerrainLight();
			MoveObjects();
			gGoboidManager->MoveBugs();

			MoveCharactersClient();
			MoveCharacterClient(&CharacterView);

			MoveEffects();
			MoveJoints();
			MoveParticles();
			MoveBoids();
			ThePetProcess().UpdatePets();
		}

		MoveCamera();

		CInput& rInput = CInput::Instance();
		CUIMng& rUIMng = CUIMng::Instance();

		if (rInput.IsKeyDown(VK_RETURN))
		{
			if (!rUIMng.IsInterfaceCharacter() && SelectedHero > -1 && SelectedHero < 5)
			{
				PlayBuffer(SOUND_CLICK01);
				if (SelectedCharacter >= 0)
					SelectedHero = SelectedCharacter;
				::StartGame();
			}
		}
		else if (rInput.IsKeyDown(VK_ESCAPE))
		{
			if (!rUIMng.IsInterfaceCharacter() && rUIMng.IsSysMenuWinShow())
			{
				::PlayBuffer(SOUND_CLICK01);
				rUIMng.ShowWin(&rUIMng.m_SysMenuWin);
			}
		}

		if (rUIMng.IsCursorOnUI())
		{
			return;
		}

		if (rInput.IsLBtnDbl() && rUIMng.m_CharSelMainWin.IsShow())
		{
			if (SelectedCharacter < 0 || SelectedCharacter > 4)
			{
				return;
			}

			SelectedHero = SelectedCharacter;
			::StartGame();
		}
		else if (rInput.IsLBtnDn())
		{
			if (SelectedCharacter < 0 || SelectedCharacter > 4)
				SelectedHero = -1;
			else
				SelectedHero = SelectedCharacter;

			rUIMng.m_CharSelMainWin.UpdateDisplay();
		}
	}
}

bool NewRenderCharacterScene(HDC hDC)
{
	if (!InitCharacterScene)
	{
		return false;
	}
	if (CurrentProtocolState < RECEIVE_CHARACTERS_LIST)
	{
		return false;
	}

	FogEnable = false;

	if (gmProtect->SceneCharacter == 1)
	{
		glClearColor(0.f, 0.f, 0.f, 1.f);
		BeginOpengl(0, 0, 640, 480);
		CreateScreenVector(MouseX, MouseY, MouseTarget, true);

		CHARACTER* pCharacter = NULL;

		for (int i = 0; i < 5; i++)
		{
			pCharacter = gmCharacters->GetCharacter(i);
			Vector(0.0f, 0.0f, 0.0f, pCharacter->Object.Light);
		}

		pCharacter = gmCharacters->GetCharacter(SelectedHero);

		if (pCharacter)
		{
			OBJECT* pObject = &pCharacter->Object;

			if (pObject->Live)
			{
				Vector(1.0f, 1.0f, 1.0f, pObject->Light);
			}
		}

		RenderObjects();
		RenderCharactersClient();

		if (!CUIMng::Instance().IsCursorOnUI())
			SelectObjects();

		gGoboidManager->RenderBugs();
		RenderBlurs();
		RenderJoints();
		RenderEffects();
		CheckSprites();

		pCharacter = gmCharacters->GetCharacter(SelectedHero);

		if (pCharacter)
		{
			OBJECT* pObject = &pCharacter->Object;

			if (pObject->Live)
			{
				float Rotation = (int)WorldTime % 3600 / (float)10.f;

				EnableAlphaBlend();
				RenderCircle(BITMAP_MAGIC + 2, pObject->Position, 50.0, 70.0, 200.0, Rotation, 0.0, 0.0);
				RenderCircle(BITMAP_MAGIC + 2, pObject->Position, 50.0, 70.0, 200.0, Rotation, 0.0, 0.0);
				DisableAlphaBlend();
				g_csMapServer.SetHeroID(pCharacter->ID);
			}
		}

		BeginSprite();
		RenderSprites();
		RenderParticles();
		EndSprite();

		BeginBitmap();
		RenderInterfaceEdge();
		RenderInfomation();
		EndBitmap();
		EndOpengl();
	}
	else
	{
		vec3_t pos;

		if (gmProtect->SceneCharacter != 0)
		{
			gCameraManager->GetPosition(pos);
		}
		else
		{
			Vector(9758.0f, 18913.0f, 675.0f, pos);
		}

		MoveMainCamera();

		int Width, Height;

		glColor3f(1.f, 1.f, 1.f);
		if (gmProtect->SceneCharacter == 2 || gmProtect->SceneCharacter == 3)
		{
			BeginBitmap();
			Width = GetNoWideX(320.0);
			Height = GetNoWideY(320.0);
			SEASON3B::RenderImageF(BITMAP_LOG_IN + 9, 0, GetNoWideY(25.0), Width, Height, 0.f, 25.0, 511.75, 487.0);
			SEASON3B::RenderImageF(BITMAP_LOG_IN + 10, Width, GetNoWideY(25.0), Width, Height, 0.25, 25.0, 512.0, 487.0);
			EndBitmap();
		}

		Height = GetWindowsY;
		Width = GetScreenWidth();

		glClearColor(0.f, 0.f, 0.f, 1.f);
		BeginOpengl(0, 25, 640, 430);

		CreateFrustrum((float)Width / (float)GetWindowsX, pos);

		CreateScreenVector(MouseX, MouseY, MouseTarget);

		OBJECT* pObj = NULL;
		CHARACTER* pCharacter = NULL;

		for (int i = 0; i < 5; i++)
		{
			pCharacter = gmCharacters->GetCharacter(i);

			if (pCharacter != NULL)
			{
				Vector(0.0f, 0.0f, 0.0f, pCharacter->Object.Light);

				if (gmProtect->SceneCharacter == 4)
				{
					pObj = &pCharacter->Object;

					if (pObj->Live)
					{
						if (SelectedCharacter == i && SelectedHero == i)
						{
							if (pObj->Position[0] == 13629.f && pObj->Position[1] == 12339.f)
							{
								pObj->Angle[2] -= timefac(3);
							}
						}
						else
						{
							if (pObj->Position[0] == 13629.f && pObj->Position[1] == 12339.f && pObj->Angle[2] != 90.f)
							{
								pObj->Angle[2] = 90.f;
							}
						}
					}
				}
			}
		}

		pCharacter = gmCharacters->GetCharacter(SelectedHero);

		if (pCharacter)
		{
			OBJECT* o = &pCharacter->Object;

			if (o->Live)
			{
				EnableAlphaBlend();
				Vector(1.0f, 1.0f, 1.0f, o->Light);
				AddTerrainLight(o->Position[0], o->Position[1], o->Light, 1, PrimaryTerrainLight);
				DisableAlphaBlend();
			}
		}

		CHARACTER* pCha = NULL;

		for (int i = 0; i < 5; ++i)
		{
			pCha = gmCharacters->GetCharacter(i);

			if (pCha)
			{
				pObj = &pCha->Object;

				if (gmProtect->SceneCharacter == 3)
				{
					if (pCha->Helper.Type == MODEL_HELPER + 3)
						pObj->Position[2] = 55.0f;
					else
						pObj->Position[2] = 30.0f;
				}
				else
				{
					if (pCha->Helper.Type == MODEL_HELPER + 3)
						pObj->Position[2] = 194.5f;
					else
						pObj->Position[2] = 169.5f;
				}
			}
		}

		RenderTerrain(false);
		RenderObjects();
		RenderCharactersClient();

		if (!CUIMng::Instance().IsCursorOnUI())
			SelectObjects();

		gGoboidManager->RenderBugs();
		RenderBlurs();
		RenderJoints();
		RenderEffects();
		if (g_pOption->GetRenderPets()) ThePetProcess().RenderPets();
		RenderBoids();
		RenderObjects_AfterCharacter();
		CheckSprites();

		pCharacter = gmCharacters->GetCharacter(SelectedHero);

		if (pCharacter)
		{
			OBJECT* o = &pCharacter->Object;

			if (o->Live)
			{
				vec3_t vLight;

				Vector(1.0f, 1.0f, 1.f, vLight);
				float fLumi = sinf(WorldTime * 0.0015f) * 0.3f + 0.5f;
				Vector(fLumi * vLight[0], fLumi * vLight[1], fLumi * vLight[2], vLight);

				EnableAlphaBlend();
				if (gmProtect->SceneCharacter == 3)
				{
					RenderTerrainAlphaBitmap(BITMAP_GM_AURORA, o->Position[0], o->Position[1], 1.5f, 1.5f, vLight, WorldTime * 0.01f);
					RenderTerrainAlphaBitmap(BITMAP_GM_AURORA, o->Position[0], o->Position[1], 1.f, 1.f, vLight, -WorldTime * 0.01f);
				}
				else
				{
					RenderTerrainAlphaBitmap(BITMAP_GM_AURORA, o->Position[0], o->Position[1], 1.8f, 1.8f, vLight, WorldTime * 0.01f);
					RenderTerrainAlphaBitmap(BITMAP_GM_AURORA, o->Position[0], o->Position[1], 1.2f, 1.2f, vLight, -WorldTime * 0.01f);
				}
				DisableAlphaBlend();

				float Rotation = (int)WorldTime % 3600 / (float)10.f;
				Vector(0.15f, 0.15f, 0.15f, o->Light);
				CreateParticleSync(BITMAP_EFFECT, o->Position, o->Angle, o->Light, 4);
				CreateParticleSync(BITMAP_EFFECT, o->Position, o->Angle, o->Light, 5);
				g_csMapServer.SetHeroID(pCharacter->ID);
			}
		}

		BeginSprite();
		RenderSprites();
		RenderParticles();
		RenderPoints();
		EndSprite();

		BeginBitmap();
		RenderInfomation();
#ifdef ENABLE_EDIT
		RenderDebugWindow();
#endif //ENABLE_EDIT
		EndBitmap();
		EndOpengl();
	}

	return true;
}

void CreateLogInScene()
{
	EnableMainRender = true;

	if (gmProtect->SceneLogin == 1)
	{
		vec3_t Angle, Position;

		World = -1;

		OpenLogoSceneData();

		Vector(0.0, 0.0, 0.0, Angle);
		Vector(0.0, -520.0, 0.0, Position);
		CreateObject(MODEL_LOGO, Position, Angle, 1.0);

		Vector(0.0, 0.0, 180.0, Angle);
		Vector(-700.0, 700.0, 0.0, Position);

		CreateObject(MODEL_SHIP, Position, Angle, 1.0);
		CreateObject(MODEL_WAVEBYSHIP, Position, Angle, 1.0);

		Vector(400.0, 400.0, 0.0, Position);
		CreateObject(MODEL_SHIP, Position, Angle, 1.0);
		CreateObject(MODEL_WAVEBYSHIP, Position, Angle, 1.0);

		Vector(-200.0, -400.0, 0.0, Position);
		CreateObject(MODEL_SHIP, Position, Angle, 1.0);
		CreateObject(MODEL_WAVEBYSHIP, Position, Angle, 1.0);

		Vector(0.0, 0.0, 0.0, Angle);
		Vector(-110.0, 1600.0, 50.0, Position);
		CreateObject(MODEL_LOGOSUN, Position, Angle, 1.0);

		Vector(0.0, -600.0, 500.0, Position);
		CreateObject(MODEL_MUGAME, Position, Angle, 1.0);


		CHARACTER* pCharacter;
		Vector(0.0, 0.0, 180.0, Angle);

		Vector(-200.0, -750.0, 250.0, Position);
		CreateHeroLogIn(1, Dark_Knight, 0, 0.6, Angle, Position);

		Vector(-240.0, -770.0, 250.0, Position);
		CreateHeroLogIn(2, Dark_Wizard, 0, 0.6, Angle, Position);

		Vector(400.0, 50.0, 250.0, Position);
		pCharacter = CreateHeroLogIn(3, Fairy_Elf, 0, 0.6, Angle, Position);

		if (pCharacter != NULL)
		{
			gGoboidManager->CreateBug(MODEL_HELPER, Position, &pCharacter->Object);
		}
		CurrentCameraCount = -1;
		Vector(0.0, 0.0, 0.0, CameraAngle);
		Vector(0.0, 0.0, 0.0, CameraPosition);
	}
	else
	{
		if (gmProtect->SceneLogin == 2)
		{
			World = WD_55LOGINSCENE;
		}
		else if (gmProtect->SceneLogin == 3)
		{
			World = WD_77NEW_LOGIN_SCENE;
		}
		else if (gmProtect->SceneLogin == 4)
		{
			World = WD_94NEW_LOGIN_SCENE;
		}
		else
		{
			World = WD_73NEW_LOGIN_SCENE;
		}

		gMapManager->LoadWorld(World);

		OpenLogoSceneData();

		// World55 / Select Server: um unico DL montado, animado (criado por
		// codigo). A decoracao "fantasma" do mapa (Type 91/92/93) ja e
		// filtrada no OpenObjectsEnc, entao so sobra este aqui.
		if (World == WD_55LOGINSCENE)
		{
			ClearCharacters();

			vec3_t Angle, Position;
			Vector(0.0f, 0.0f, 188.0f, Angle);
			Vector(10603.0f, 14112.0f, 580.0f, Position);

			CHARACTER* pDarkLord = CreateHeroLogIn(2, Dark_Lord, 0, 1.6f, Angle, Position);
			if (pDarkLord != NULL)
			{
				OBJECT* pOwner = &pDarkLord->Object;

				// Remove qualquer cavalo/helper/pet que tenha sido criado antes.
				gGoboidManager->DeleteBug(pOwner);
				giPetManager::DeletePet(pDarkLord);

				// CreateHero ja seta Helper.Type/Weapon[1] = -1 para o World55,
				// mas reforcamos aqui para garantir que nada recrie por cima.
				pDarkLord->Helper.Type = -1;
				pDarkLord->Weapon[1].Type = -1;

				// Um unico Dark Horse. CreateBug tambem marca o owner como Horse.
				gGoboidManager->CreateBug(MODEL_DARK_HORSE, pOwner->Position, pOwner);
				SetAction(pOwner, PLAYER_STOP_RIDE_HORSE, 1);
			}
		}
	}

	CUIMng::Instance().CreateLoginScene();

	CurrentProtocolState = REQUEST_JOIN_SERVER;

	if (g_pReconnectUI->ReconnectCreateConnection(szServerIpAddress, g_ServerPort))
	{
		SendRequestServerHWID();
	}

	EnableSocket = true;
	GuildInputEnable = false;
	TabInputEnable = false;
	GoldInputEnable = false;
	InputEnable = true;
	ClearInput();

	if (g_iChatInputType == 0)
	{
		strcpy(InputText[0], m_ID[5]);
		InputLength[0] = strlen(InputText[0]);
		InputTextMax[0] = MAX_ID_SIZE;

		if (InputLength[0] == 0)
			InputIndex = 0;
		else
			InputIndex = 1;
	}
	InputNumber = 2;
	InputTextHide[1] = 1;

	if (gmProtect->SceneLogin != 1)
	{
		if (gmProtect->SceneLogin != 4)
		{
			CCameraMove::GetInstancePtr()->PlayCameraWalk(Hero->Object.Position, 1000);
			CCameraMove::GetInstancePtr()->SetTourMode(TRUE, FALSE, (gmProtect->SceneLogin != 3));
		}
		MoveMainCamera();
	}

	g_fMULogoAlpha = 0.0;

	PlayMp3(g_lpszMp3[MUSIC_LOGIN_THEME]);

	g_ErrorReport.Write("> Login Scene init success.\r\n");
}

void NewMoveLogInScene()
{
	if (!InitLogIn)
	{
		InitLogIn = true;
		CreateLogInScene();
	}

	if (gmProtect->SceneLogin == 1)
	{
		if (!CUIMng::Instance().m_CreditWin.IsShow())
		{
			MoveObjects();
			gGoboidManager->MoveBugs();

			MoveCharactersClient();
			MoveParticles();
			MoveCamera();

			if (CameraWalkCut == 1 && !CurrentCameraCount)
				PlayBuffer(SOUND_TITLE01);
		}
	}
	else
	{
		if (!CUIMng::Instance().m_CreditWin.IsShow())
		{
			InitTerrainLight();
			MoveObjects();
			gGoboidManager->MoveBugs();

			MoveLeaves();
			MoveCharactersClient();
			MoveEffects();
			MoveJoints();
			MoveParticles();
			MoveBoids();
			ThePetProcess().UpdatePets();
			MoveCamera();
		}
	}

	if (CInput::Instance().IsKeyDown(VK_ESCAPE))
	{
		CUIMng* rUIMng = &CUIMng::Instance();

		if (rUIMng->IsInterfaceLogin())
		{
			PlayBuffer(SOUND_CLICK01);
			rUIMng->ShowWin(&(rUIMng->m_SysMenuWin));
		}
	}

	if (RECEIVE_LOG_IN_SUCCESS == CurrentProtocolState)
	{
		g_ErrorReport.Write("> Request Character list\r\n");
		CCameraMove::GetInstancePtr()->SetTourMode(FALSE);
		SceneFlag = CHARACTER_SCENE;
		SendRequestCharactersList(g_pMultiLanguage->GetLanguage());
		ReleaseLogoSceneData();
		ClearCharacters();
	}
	g_ConsoleDebug->UpdateMainScene();
}

bool NewRenderLogInScene(HDC hDC)
{
	if (!InitLogIn)
		return false;

	FogEnable = false;

	if (gmProtect->SceneLogin == 1)
	{
		glClearColor(0.f, 0.f, 0.f, 1.f);
		BeginOpengl(0, 80, 640, 320);

		if (!CUIMng::Instance().m_CreditWin.IsShow())
		{
			RenderCharactersClient();
			gGoboidManager->RenderBugs();
			RenderObjects();
		}
		CheckSprites();
		BeginSprite();
		RenderSprites();
		RenderParticles();
		EndSprite();
		BeginBitmap();

		//g_fMULogoAlpha += 0.02f;
		//if (g_fMULogoAlpha > 10.0f)
		//	g_fMULogoAlpha = 10.0f;
		//EnableAlphaBlend();
		//glColor4f(g_fMULogoAlpha - 0.3f, g_fMULogoAlpha - 0.3f, g_fMULogoAlpha - 0.3f, g_fMULogoAlpha - 0.3f);
		//RenderBitmap(BITMAP_LOG_IN + 17, AddMiddleX() + 320.0f - 128.0f * 0.8f, 25.0f, 256.0f * 0.8f, 128.0f * 0.8f);
		//EnableAlphaTest();
		//glColor4f(g_fMULogoAlpha, g_fMULogoAlpha, g_fMULogoAlpha, g_fMULogoAlpha);
		//RenderBitmap(BITMAP_LOG_IN + 16, AddMiddleX() + 320.0f - 128.0f * 0.8f, 25.0f, 256.0f * 0.8f, 128.0f * 0.8f);
	}
	else
	{
		vec3_t pos;
		float Width, Height;

		if (CCameraMove::GetInstancePtr()->IsCameraMove())
		{
			VectorCopy(CameraPosition, pos);
		}
		else
		{
			if (gmProtect->SceneLogin != 0)
			{
				gCameraManager->GetPosition(pos);
			}
		}

		MoveMainCamera();
		glColor3f(1.f, 1.f, 1.f);

		if (gmProtect->SceneLogin == 2 || gmProtect->SceneLogin == 3)
		{
			BeginBitmap();
			Width = (320.0);
			Height = (325.0);
			RenderNoBitmap(BITMAP_LOG_IN + 9, 0, 25.0, Width, Height, 0.0, 0.0, 511.75f / 512.f, 512.f / 512.f);
			RenderNoBitmap(BITMAP_LOG_IN + 10, Width, 25.0, Width, Height, 0.0, 0.0, 511.75f / 512.f, 512.f / 512.f);
			EndBitmap();
		}

		Width = ((float)GetScreenWidth() / GetWindowsX);

		glClearColor(0.f, 0.f, 0.f, 1.f);
		BeginOpengl(0, 25, 640, 430);
		CreateFrustrum(Width, pos);

		if (!CUIMng::Instance().m_CreditWin.IsShow())
		{
			if (CCameraMove::GetInstancePtr()->IsCameraMove())
			{
				CameraViewFar = CCameraMove::GetInstancePtr()->GetCurrentCameraDistanceLevel() * 330.f;
			}
			if (gmProtect->SceneLogin == 3)
				BeginOpengl();

			RenderTerrain(false);
			CameraViewFar = 7000.f;
			RenderCharactersClient();
			gGoboidManager->RenderBugs();
			RenderObjects();
			RenderJoints();
			RenderEffects();
			CheckSprites();
			RenderLeaves();
			RenderBoids();
			RenderObjects_AfterCharacter();
			if (g_pOption->GetRenderPets()) ThePetProcess().RenderPets();

			if (gmProtect->SceneLogin == 3)
				EndOpengl();
		}

		BeginSprite();
		RenderSprites();
		RenderParticles();
		EndSprite();
		BeginBitmap();

		if (CCameraMove::GetInstancePtr()->IsTourMode())
		{
			if (gmProtect->SceneLogin == 3)
			{
				EnableAlphaBlend4();
				glColor4f(0.7f, 0.7f, 0.7f, 1.0f);
				float fScale = (sinf(WorldTime * 0.0005f) + 1.f) * 0.00011f;

				RenderNoBitmap(BITMAP_CHROME + 3, 0.0f, 0.0f, 640.0f, 480.0f, 800.f * fScale, 600.f * fScale, (800.f) / 1024.f - 800.f * fScale * 2, (600.f) / 1024.f - 600.f * fScale * 2);
				float fAngle = WorldTime * 0.00018f;
				float fLumi = 1.0f - (sinf(WorldTime * 0.0015f) + 1.f) * 0.25f;
				glColor4f(fLumi * 0.3f, fLumi * 0.3f, fLumi * 0.7f, fLumi);
				fScale = (sinf(WorldTime * 0.0015f) + 1.f) * 0.00021f;
				RenderNoBitmapLocalRotate(BITMAP_CHROME + 4, 320.0f, 240.0f, 1150.0f, 1150.0f, fAngle, fScale * 512.f, fScale * 512.f, (512.f) / 512.f - fScale * 2 * 512.f, (512.f) / 512.f - fScale * 2 * 512.f);

				EnableAlphaTest();
				glColor4f(0.0f, 0.0f, 0.0f, 1.0f);
				RenderNoColor(0, 0, 640, 25);
				RenderNoColor(0, 480 - 25, 640, 25);

				glColor4f(0.0f, 0.0f, 0.0f, 0.2f);
				RenderNoColor(0, 25, 640, 430);
			}

			g_fMULogoAlpha += 0.02f;
			if (g_fMULogoAlpha > 10.0f)
				g_fMULogoAlpha = 10.0f;

			EnableAlphaBlend();
			glColor4f(g_fMULogoAlpha - 0.3f, g_fMULogoAlpha - 0.3f, g_fMULogoAlpha - 0.3f, g_fMULogoAlpha - 0.3f);
			RenderBitmap(BITMAP_LOG_IN + 17, AddMiddleX() + 320.0f - 128.0f * 0.8f, 25.0f, 256.0f * 0.8f, 128.0f * 0.8f);

			EnableAlphaTest();
			glColor4f(g_fMULogoAlpha, g_fMULogoAlpha, g_fMULogoAlpha, g_fMULogoAlpha);
			RenderBitmap(BITMAP_LOG_IN + 16, AddMiddleX() + 320.0f - 128.0f * 0.8f, 25.0f, 256.0f * 0.8f, 128.0f * 0.8f);
		}
	}

	SIZE Size;
	char Text[100];

	g_pRenderText->SetFont(g_hFont);

	InputTextWidth = 256;
	glColor3f(0.8f, 0.7f, 0.6f);
	g_pRenderText->SetTextColor(255, 255, 255, 255);
	g_pRenderText->SetBgColor(0, 0, 0, 128);

	strcpy(Text, GlobalText[454]);
	g_pMultiLanguage->_GetTextExtentPoint32(g_pRenderText->GetFontDC(), Text, lstrlen(Text), &Size);
	g_pRenderText->RenderText(AddMiddleX() + 335 - Size.cx * gwinhandle->GetScreenX() / WindowWidth, gwinhandle->GetScreenY() - Size.cy * gwinhandle->GetScreenX() / WindowWidth - 1, Text);

	strcpy(Text, GlobalText[455]);

	g_pMultiLanguage->_GetTextExtentPoint32(g_pRenderText->GetFontDC(), Text, lstrlen(Text), &Size);
	g_pRenderText->RenderText(AddMiddleX() + 335, gwinhandle->GetScreenY() - Size.cy * gwinhandle->GetScreenX() / WindowWidth - 1, Text);

	sprintf(Text, GlobalText[456], m_ExeVersion);

	g_pMultiLanguage->_GetTextExtentPoint32(g_pRenderText->GetFontDC(), Text, lstrlen(Text), &Size);
	g_pRenderText->RenderText(0, gwinhandle->GetScreenY() - Size.cy * gwinhandle->GetScreenX() / WindowWidth - 1, Text);

	RenderInfomation();

#ifdef ENABLE_EDIT
	RenderDebugWindow();
#endif //ENABLE_EDIT

	EndBitmap();

	EndOpengl();

	return true;
}

float CameraDistanceTarget = 1000.f;
float CameraDistance = CameraDistanceTarget;

bool update_camera_in_real_time()
{
	if (SceneFlag == MAIN_SCENE)
	{
		float Matrix[3][4];
		vec3_t Position, TransformPosition;

		Vector(0.f, -CameraDistance, 0.f, Position);//-750
		AngleMatrix(CameraAngle, Matrix);
		VectorIRotate(Position, Matrix, TransformPosition);

		g_pCatapultWindow->GetCameraPos(Position);

		VectorAdd(Position, TransformPosition, CameraPosition);
	}

	return false;
}

void LoadingScene(HDC hDC)
{
	g_ConsoleDebug->Write(MCD_NORMAL, "LoadingScene_Start");

	CUIMng& rUIMng = CUIMng::Instance();
	if (!InitLoading)
	{
		LoadingWorld = 9999999;

		InitLoading = true;

		LoadBitmap("Interface\\LSBg01.JPG", BITMAP_TITLE, GL_LINEAR);
		LoadBitmap("Interface\\LSBg02.JPG", BITMAP_TITLE + 1, GL_LINEAR);
		LoadBitmap("Interface\\LSBg03.JPG", BITMAP_TITLE + 2, GL_LINEAR);
		LoadBitmap("Interface\\LSBg04.JPG", BITMAP_TITLE + 3, GL_LINEAR);

		::StopMp3(g_lpszMp3[MUSIC_LOGIN_THEME]);

		rUIMng.m_pLoadingScene = new CLoadingScene;
		rUIMng.m_pLoadingScene->Create();
	}

	FogEnable = false;
	BeginOpengl();
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	BeginBitmap();

	rUIMng.m_pLoadingScene->Render();

	EndBitmap();
	EndOpengl();
	glFlush();
	SwapBuffers(hDC);

	SAFE_DELETE(rUIMng.m_pLoadingScene);

	SceneFlag = MAIN_SCENE;
	for (int i = 0; i < 4; ++i)
		DeleteBitmap(BITMAP_TITLE + i);

	ClearInput();

	g_ConsoleDebug->Write(MCD_NORMAL, "LoadingScene_End");
}

bool MoveMainCamera()
{
	bool bLockCamera = false;

	CAMERA_INFO* CurrentCam = CameraFactorPtr->CurrentCam();

	int iWorld = World;

	if ((iWorld == WD_73NEW_LOGIN_SCENE || iWorld == WD_77NEW_LOGIN_SCENE) && CCameraMove::GetInstancePtr()->IsTourMode())
	{
		if (iWorld == WD_77NEW_LOGIN_SCENE)
			CameraFOV = 61.0;
		else
			CameraFOV = 65.0;
	}
	else if (SceneFlag == MAIN_SCENE)
	{
		CameraFOV = CurrentCam->zoom;
	}
	else
	{
		if (gCameraManager->Enable)
			CameraFOV = gCameraManager->GetFov();
		else
			CameraFOV = 35.0;
	}


#ifdef ENABLE_EDIT2
	{
		bool EditMove = false;
		if (!g_pUIManager->IsInputEnable())
		{
			if (HIBYTE(GetAsyncKeyState(VK_INSERT)) == 128)
				CameraAngle[2] += 15;
			if (HIBYTE(GetAsyncKeyState(VK_DELETE)) == 128)
				CameraAngle[2] -= 15;

			vec3_t p1, p2;
			Vector(0.f, 0.f, 0.f, p1);
			FLOAT Velocity = sqrtf(TERRAIN_SCALE * TERRAIN_SCALE) * 1.25f;

			if (HIBYTE(GetAsyncKeyState(VK_LEFT)) == 128)// || (MouseX<=0 && MouseY>=100))
			{
				Vector(-Velocity, -Velocity, 0.f, p1);
				EditMove = true;
			}
			if (HIBYTE(GetAsyncKeyState(VK_RIGHT)) == 128)// || (MouseX>=639 && MouseY>=100))
			{
				Vector(Velocity, Velocity, 0.f, p1);
				EditMove = true;
			}
			if (HIBYTE(GetAsyncKeyState(VK_UP)) == 128)// || (MouseY<=0 && MouseX>=100 && MouseX<540))
			{
				Vector(-Velocity, Velocity, 0.f, p1);
				EditMove = true;
			}
			if (HIBYTE(GetAsyncKeyState(VK_DOWN)) == 128)// || (MouseY>=479))
			{
				Vector(Velocity, -Velocity, 0.f, p1);
				EditMove = true;
			}

			glPushMatrix();
			glLoadIdentity();
			glRotatef(-CameraAngle[2], 0.f, 0.f, 1.f);
			float Matrix[3][4];
			GetOpenGLMatrix(Matrix);
			glPopMatrix();
			VectorRotate(p1, Matrix, p2);
			VectorAdd(Hero->Object.Position, p2, Hero->Object.Position);
		}

		if (gMapManager->InChaosCastle() == false || !Hero->Object.m_bActionStart)
		{
			if (World == WD_39KANTURU_3RD && Hero->Object.m_bActionStart)
			{
			}
			else
				if (World == -1 || Hero->Helper.Type != MODEL_HELPER + 3 || Hero->SafeZone)
				{
					Hero->Object.Position[2] = RequestTerrainHeight(Hero->Object.Position[0], Hero->Object.Position[1]);
				}
				else
				{
					if (World == WD_8TARKAN || World == WD_10HEAVEN)
						Hero->Object.Position[2] = RequestTerrainHeight(Hero->Object.Position[0], Hero->Object.Position[1]) + 90.f;
					else
						Hero->Object.Position[2] = RequestTerrainHeight(Hero->Object.Position[0], Hero->Object.Position[1]) + 30.f;
				}
		}

		if (EditMove)
		{
			BYTE PathX[1];
			BYTE PathY[1];
			PathX[0] = (BYTE)(Hero->Object.Position[0] / TERRAIN_SCALE);
			PathY[0] = (BYTE)(Hero->Object.Position[1] / TERRAIN_SCALE);
			SendCharacterMove(Hero->Key, Hero->Object.Angle[2], 1, PathX, PathY, PathX[0], PathY[0]);
			Hero->Path.PathNum = 0;
		}
	}
#endif //ENABLE_EDIT2

	CameraAngle[0] = 0.f;
	CameraAngle[1] = 0.f;

	if (CameraTopViewEnable)
	{
		CameraViewFar = 3200.f;
		CameraPosition[0] = Hero->Object.Position[0];
		CameraPosition[1] = Hero->Object.Position[1];
		CameraPosition[2] = CameraViewFar;
	}
	else
	{
		int iIndex = TERRAIN_INDEX((Hero->PositionX), (Hero->PositionY));
		vec3_t Position, TransformPosition;
		float Matrix[3][4];

		if (battleCastle::InBattleCastle2(Hero->Object.Position))
		{
			CameraViewFar = 3000.f;
		}
		else if (gMapManager->InBattleCastle() && SceneFlag == MAIN_SCENE)
		{
			CameraViewFar = 2500.f;
		}
		else if (iWorld == WD_51HOME_6TH_CHAR)
		{
			CameraViewFar = 3220.000;
		}
		else if (gMapManager->IsPKField() || IsDoppelGanger2())
		{
			CameraViewFar = 3700.0f;
		}
		else
		{
			if (g_shCameraLevel != 0)
			{
				switch (g_shCameraLevel)
				{
				case 1:
					CameraViewFar = 2500.000;
					break;
				case 2:
					CameraViewFar = 2600.000;
					break;
				case 3:
					CameraViewFar = 2950.000;
					break;
				case 5:
				case 4:
					CameraViewFar = 4200.000;
					break;
				}
			}
			else
			{
				if (SceneFlag == LOG_IN_SCENE)
				{
					if (CCameraMove::GetInstancePtr()->IsTourMode() == false)
					{
						if (gmProtect->SceneLogin != 0)
						{
							CameraViewFar = gCameraManager->CalcViewFar(2400.000);
						}
					}
				}
				else if (SceneFlag == CHARACTER_SCENE)
				{
					if (gmProtect->SceneCharacter != 0)
					{
						CameraViewFar = gCameraManager->CalcViewFar(2400.000);
					}
					else
					{
						CameraViewFar = 3500.f;
					}
				}
				else if (g_Direction.m_CKanturu.IsMayaScene())
				{
					CameraViewFar = 2300.0f;
				}
				else
				{
					CameraViewFar = CurrentCam->camWidthFar;
				}
			}
		}

		Vector(0.f, -CameraDistance, 0.f, Position);//-750
		AngleMatrix(CameraAngle, Matrix);
		VectorIRotate(Position, Matrix, TransformPosition);

		if (SceneFlag == MAIN_SCENE)
		{
			g_pCatapultWindow->GetCameraPos(Position);
		}
		else if (CCameraMove::GetInstancePtr()->IsTourMode())
		{
			CCameraMove::GetInstancePtr()->UpdateTourWayPoint();
			CCameraMove::GetInstancePtr()->GetCurrentCameraPos(Position);
			CameraViewFar = 390.f * CCameraMove::GetInstancePtr()->GetCurrentCameraDistanceLevel();
		}

		if (g_Direction.IsDirection() && !g_Direction.m_bDownHero)
		{
			Hero->Object.Position[2] = 300.0f;
			g_shCameraLevel = g_Direction.GetCameraPosition(Position);
		}
		else if (gMapManager->IsPKField() || IsDoppelGanger2())
		{
			g_shCameraLevel = 5;
		}
		else if (IsDoppelGanger1())
		{
			g_shCameraLevel = 5;
		}
		else
		{
			g_shCameraLevel = 0;
		}

		if (gmProtect->SceneLogin != 3 && gmProtect->SceneLogin != 4)
		{
			if (CCameraMove::GetInstancePtr()->IsTourMode())
			{
				vec3_t temp = { 0.0f, 0.0f, -100.0f };
				VectorAdd(TransformPosition, temp, TransformPosition);
			}
		}

		VectorAdd(Position, TransformPosition, CameraPosition);

		if (gMapManager->InBattleCastle() == true)
		{
			CameraPosition[2] = 255.f;//700
		}
		else if (CCameraMove::GetInstancePtr()->IsTourMode() == false)
		{
			CameraPosition[2] = Hero->Object.Position[2];//700
		}

		if ((TerrainWall[iIndex] & TW_HEIGHT) == TW_HEIGHT)
		{
			CameraPosition[2] = g_fSpecialHeight = 1200.f + 1;
		}

		if (SceneFlag == MAIN_SCENE)
		{
			CameraPosition[2] += CameraDistance - CurrentCam->Rot.Z;
		}
		else
		{
			CameraPosition[2] += CameraDistance - 150.0;
		}

		if (CCameraMove::GetInstancePtr()->IsTourMode())
		{
			if (gmProtect->SceneLogin != 3)
			{
				CCameraMove::GetInstancePtr()->SetAngleFrustum(-112.5f);
				CameraAngle[0] = CCameraMove::GetInstancePtr()->GetAngleFrustum();
			}
			else
			{
				CameraAngle[0] = -78.5;
			}

			CameraAngle[1] = 0.0f;
			CameraAngle[2] = CCameraMove::GetInstancePtr()->GetCameraAngle();
		}
		else if (SceneFlag == LOG_IN_SCENE)
		{
			if (gmProtect->SceneLogin != 0)
			{
				gCameraManager->GetAngle(CameraAngle);

				gCameraManager->GetPosition(CameraPosition);
			}
		}
		else if (SceneFlag == CHARACTER_SCENE)
		{
			if (gmProtect->SceneCharacter != 0)
			{
				gCameraManager->GetAngle(CameraAngle);
				gCameraManager->GetPosition(CameraPosition);
			}
			else
			{
				CameraAngle[0] = -84.5f;
				CameraAngle[1] = 0.0f;
				CameraAngle[2] = -75.0f;
				CameraPosition[0] = 9758.93f;
				CameraPosition[1] = 18913.11f;
				CameraPosition[2] = 675.5f;
			}
		}
		else
		{
			CameraAngle[0] = CurrentCam->Rot.Y;
		}

		CameraAngle[0] += EarthQuake;

		if ((TerrainWall[iIndex] & TW_CAMERA_UP) == TW_CAMERA_UP)
		{
			if (g_fCameraCustomDistance <= CUSTOM_CAMERA_DISTANCE1)
			{
				g_fCameraCustomDistance += timefac(10);
			}
		}
		else
		{
			if (g_fCameraCustomDistance > 0)
			{
				g_fCameraCustomDistance -= timefac(10);
			}
		}

		if (g_fCameraCustomDistance > 0)
		{
			vec3_t angle = { 0.f, 0.f, -45.f };
			Vector(0.f, g_fCameraCustomDistance, 0.f, Position);
			AngleMatrix(angle, Matrix);
			VectorIRotate(Position, Matrix, TransformPosition);
			interpolatesum(CameraPosition, TransformPosition);
		}
		else if (g_fCameraCustomDistance < 0)
		{
			vec3_t angle = { 0.f, 0.f, -45.f };
			Vector(0.f, g_fCameraCustomDistance, 0.f, Position);
			AngleMatrix(angle, Matrix);
			VectorIRotate(Position, Matrix, TransformPosition);
			interpolatesum(CameraPosition, TransformPosition);
		}
	}

	if (iWorld == WD_5UNKNOWN)
	{
		CameraAngle[0] += sinf(WorldTime * 0.0005f) * 2.f;
		CameraAngle[1] += sinf(WorldTime * 0.0008f) * 2.5f;
	}
	else if (CCameraMove::GetInstancePtr()->IsTourMode())
	{
		CameraDistanceTarget = 1100.f * CCameraMove::GetInstancePtr()->GetCurrentCameraDistanceLevel() * 0.1f;
		CameraDistance = CameraDistanceTarget;
	}
	else
	{
		if (gMapManager->InBattleCastle())
		{
			CameraDistanceTarget = 1100.f;
			CameraDistance = CameraDistanceTarget;
		}
		else
		{
			switch (g_shCameraLevel)
			{
			case 0:
				CameraDistanceTarget = 1000.f;
				CameraDistance += (CameraDistanceTarget - CameraDistance) / 3;
				break;
			case 1:
				CameraDistanceTarget = 1100.f;
				CameraDistance += (CameraDistanceTarget - CameraDistance) / 3;
				break;
			case 2:
				CameraDistanceTarget = 1200.f;
				CameraDistance += (CameraDistanceTarget - CameraDistance) / 3;
				break;
			case 3:
				CameraDistanceTarget = 1300.f;
				CameraDistance += (CameraDistanceTarget - CameraDistance) / 3;
				break;
			case 4:
				CameraDistanceTarget = 1400.f;
				CameraDistance += (CameraDistanceTarget - CameraDistance) / 3;
				break;
			case 5:
				CameraDistanceTarget = g_Direction.m_fCameraViewFar;
				CameraDistance += (CameraDistanceTarget - CameraDistance) / 3;
				break;
			}
		}
	}
	return bLockCamera;
}

namespace
{
    struct LegacyUpdateTiming
    {
        double ObjectsMs, CharactersMs, PetsMs, EffectsMs, JointsMs, ParticlesMs;
    };
    LegacyUpdateTiming g_LegacyUpdateTiming = {};

    template <typename UpdateFunction>
    void MeasureLegacyUpdate(double& elapsedMs, UpdateFunction update)
    {
        const auto start = std::chrono::steady_clock::now();
        update();
        elapsedMs += std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - start).count();
    }
}

#include "ChatTranslationClient.h"

void MoveMainScene()
{
    ChatTranslationClient::Tick();
	if (!InitMainScene)
	{
		g_pMainFrame->ResetSkillHotKey();

		CHARACTER* pCharacter = gmCharacters->GetCharacter(SelectedHero);

		InitMainScene = true;

		if (pCharacter)
		{
			g_ConsoleDebug->Write(MCD_NORMAL, "Join the game with the following character: %s", pCharacter->ID);

			g_ErrorReport.Write("> Character selected <%d> \"%s\"\r\n", SelectedHero + 1, pCharacter->ID);

			g_ConsoleDebug->Write(MCD_SEND, "SendRequestJoinMapServer");

			SendRequestJoinMapServer(pCharacter->ID);
		}

		CUIMng::Instance().CreateMainScene();

		CameraAngle[2] = -45.f;

		ClearInput();
		InputEnable = false;
		TabInputEnable = false;
		InputTextWidth = 256;
		InputTextMax[0] = 42;
		InputTextMax[1] = 10;
		InputNumber = 2;
		for (int i = 0; i < MAX_WHISPER; i++)
		{
			g_pChatListBox->AddText("", "", SEASON3B::TYPE_WHISPER_MESSAGE);
		}

		g_GuildNotice[0][0] = '\0';
		g_GuildNotice[1][0] = '\0';

		g_pPartyManager->Create();

		g_pChatListBox->ClearAll();

		g_pSlideHelpMgr->Init();

		g_pUIMapName->Init();

		g_GuildCache.Reset();

		g_PortalMgr.Reset();

		ClearAllObjectBlurs();

		SetFocus(gwinhandle->GethWnd());

		//CameraFactorPtr->recover_backup_cam();

		g_ErrorReport.Write("> Main Scene init success. ");
		g_ErrorReport.WriteCurrentTime();

		g_ConsoleDebug->Write(MCD_NORMAL, "MainScene Init Success");
	}

	if (CurrentProtocolState == RECEIVE_JOIN_MAP_SERVER)
	{
		EnableMainRender = true;
	}
	if (EnableMainRender == false)
	{
		return;
	}

	EarthQuake *= timepow(0.2f);

	InitTerrainLight();
	//return;

	if (gmAIController->IsRunning())
	{
		gmAIController->WhatToDoNext();
	}

	CheckInventory = NULL;
	CheckSkill = -1;
	MouseOnWindow = false;

	if (!CameraTopViewEnable && LoadingWorld < 30)
	{
		if (CheckMouseIn((int)PositionX_The_Mid(640), (int)(GetWindowsY - 51), 640, 51))
			MouseOnWindow = true;

		g_pPartyManager->Update();

		g_pNewUISystem->Update();

		if (MouseLButton == true && false == g_pNewUISystem->CheckMouseUse() && g_dwMouseUseUIID == 0 && g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_CHATINPUTBOX) == false)
		{
			g_pWindowMgr->SetWindowsEnable(FALSE);

			g_pFriendMenu->HideMenu();

			g_dwKeyFocusUIID = 0;

			if (GetFocus() != gwinhandle->GethWnd())
			{
				SaveIMEStatus();
				SetFocus(gwinhandle->GethWnd());
			}
		}
		MoveInterface();
		MoveTournamentInterface();
		if (ErrorMessage != MESSAGE_LOG_OUT)
			g_pUIManager->UpdateInput();
	}

	if (ErrorMessage != NULL)
		MouseOnWindow = true;

	MeasureLegacyUpdate(g_LegacyUpdateTiming.ObjectsMs, []() { MoveObjects(); });
	if (!CameraTopViewEnable)
		MoveItems();

	if ((World == WD_0LORENCIA && HeroTile != 4) ||
		(World == WD_2DEVIAS && HeroTile != 3 && HeroTile < 10)
		|| World == WD_3NORIA
		|| World == WD_7ATLANSE
		|| gMapManager->InDevilSquare() == true
		|| World == WD_10HEAVEN
		|| gMapManager->InChaosCastle() == true
		|| gMapManager->InBattleCastle() == true
		|| M31HuntingGround::IsInHuntingGround() == true
		|| M33Aida::IsInAida() == true
		|| M34CryWolf1st::IsCyrWolf1st() == true
		|| World == WD_42CHANGEUP3RD_2ND
		|| IsIceCity()
		|| IsSantaTown()
		|| gMapManager->IsPKField()
		|| IsDoppelGanger2()
		|| gMapManager->IsEmpireGuardian1()
		|| gMapManager->IsEmpireGuardian2()
		|| gMapManager->IsEmpireGuardian3()
		|| gMapManager->IsEmpireGuardian4()
		|| IsUnitedMarketPlace()
		)
	{
		MoveLeaves();
	}
	MoveBoids();
	MoveFishs();

	gmHeadChat->MoveChat();

	UpdatePersonalShopTitleImp();
	MeasureLegacyUpdate(g_LegacyUpdateTiming.CharactersMs, []() {
		MoveHero();
		MoveCharactersClient();
	});
	MoveMainCamera();
	MeasureLegacyUpdate(g_LegacyUpdateTiming.PetsMs, []() {
		ThePetProcess().UpdatePets();
		gGoboidManager->MoveBugs();
	});


	MovePoints();
	MovePlanes();
	MeasureLegacyUpdate(g_LegacyUpdateTiming.EffectsMs, []() { MoveEffects(); });
	MeasureLegacyUpdate(g_LegacyUpdateTiming.JointsMs, []() { MoveJoints(); });
	MeasureLegacyUpdate(g_LegacyUpdateTiming.ParticlesMs, []() { MoveParticles(); });
	MovePointers();

	g_Direction.CheckDirection();

#ifdef ENABLE_EDIT
	EditObjects();
#endif //ENABLE_EDIT

	g_GameCensorship->Update();

	g_ConsoleDebug->UpdateMainScene();

	//HORAS ONLINE:

	static DWORD dwPlayTimeStart = 0;
	static int iLastSentHour = 0;

	if (dwPlayTimeStart == 0)
	{
		dwPlayTimeStart = timeGetTime();
	}

	DWORD dwCurrentTime = timeGetTime();
	DWORD dwPlayedMs = dwCurrentTime - dwPlayTimeStart;

	int iCurrentHour = (int)(dwPlayedMs / 3600000);

	if (iCurrentHour > iLastSentHour && iCurrentHour > 0)
	{
		char szMessage[128];
		sprintf(szMessage, "Voc� est� jogando a %d horas. Por favor, tome um descanso.", iCurrentHour);
		g_pChatListBox->AddText("", szMessage, SEASON3B::TYPE_UNION_MESSAGE);

		iLastSentHour = iCurrentHour;
	}
}

namespace
{
	struct LegacyStageTiming
	{
		double TerrainObjectsMs;
		double TerrainOnlyMs;
		double ObjectsOnlyMs;
		double CharactersMs;
		double WorldExtrasMs;
		double EffectsParticlesMs;
		double JointsMs;
		double EffectsMs;
		double BlursMs;
		double SpritesParticlesMs;
		double AfterEffectsMs;
		double WaterMs;
		double WeatherMs;
		double InterfaceMs;
		double NewUIMs;
	};

	LegacyStageTiming g_LegacyStageTiming = {};

	struct WindowProfileBaseline
	{
		bool Ready;
		int Samples;
		double WorkMs;
		double UIMs;
		double TerrainMs;
		double CharactersMs;
		double EffectsMs;
		double UpdateMs;
		double PresentMs;
	};

	void UpdateWindowProfileAverage(double& average, double value, int samples)
	{
		if (samples == 0)
			average = value;
		else
			average += (value - average) * 0.025;
	}

	DWORD BuildWindowProfileSignature(const SEASON3B::SUIFrameProfileSnapshot& profile)
	{
		DWORD keys[SEASON3B::UI_PROFILE_MAX_VISIBLE_WINDOWS] = {};
		const int count = (profile.VisibleWindowCount < SEASON3B::UI_PROFILE_MAX_VISIBLE_WINDOWS)
			? profile.VisibleWindowCount : SEASON3B::UI_PROFILE_MAX_VISIBLE_WINDOWS;
		for (int i = 0; i < count; ++i)
			keys[i] = profile.VisibleWindowKeys[i];
		std::sort(keys, keys + count);

		DWORD signature = 2166136261u;
		for (int i = 0; i < count; ++i)
		{
			signature ^= keys[i];
			signature *= 16777619u;
		}
		return signature ^ static_cast<DWORD>(count);
	}

	void BuildWindowProfileNames(const SEASON3B::SUIFrameProfileSnapshot& profile,
		char* output, size_t outputSize)
	{
		if (outputSize == 0)
			return;
		output[0] = '\0';
		const int count = (profile.VisibleWindowCount < SEASON3B::UI_PROFILE_MAX_VISIBLE_WINDOWS)
			? profile.VisibleWindowCount : SEASON3B::UI_PROFILE_MAX_VISIBLE_WINDOWS;
		for (int i = 0; i < count; ++i)
		{
			const DWORD key = profile.VisibleWindowKeys[i];
			char entry[80];
			_snprintf_s(entry, sizeof(entry), _TRUNCATE, "%s%s(%lu)",
				(i == 0) ? "" : "+", SEASON3B::GetNewUIInterfaceProfileName(key), key);
			strncat_s(output, outputSize, entry, _TRUNCATE);
		}
		if (count == 0)
			strcpy_s(output, outputSize, "None");
	}
}

namespace
{
    // Timestamp pairs avoid nesting GL_TIME_ELAPSED queries used by other
    // profilers. Read only finished samples; never wait for the GPU here.
    struct ArenaTerrainGpuSample
    {
        bool pending;
        int world;
        int stage;
        DWORD tick;
        float zoom;
        float fps;
        double cpuMs;
    };
    GLuint g_ArenaTerrainGpuQueries[32] = {};
    ArenaTerrainGpuSample g_ArenaTerrainGpuSamples[16] = {};
    HGLRC g_ArenaTerrainGpuContext = NULL;
    DWORD g_ArenaTerrainGpuLastSample[8] = {};
    const char* MAP_GPU_STAGE_NAMES[8] = { "Terrain", "Objects", "Characters", "Effects", "WorldExtras", "Water", "Weather", "Interface" };

    int BeginArenaTerrainGpuSample(int stage)
    {
        if (SceneFlag != MAIN_SCENE || stage < 0 || stage >= 8 ||
            !(GLEW_VERSION_3_3 || GLEW_ARB_timer_query) ||
            !glQueryCounter || !glGetQueryObjectui64v) return -1;
        const HGLRC context = wglGetCurrentContext();
        if (!context) return -1;
        if (context != g_ArenaTerrainGpuContext)
        {
            // A destroyed context releases its own query objects.
            memset(g_ArenaTerrainGpuQueries, 0, sizeof(g_ArenaTerrainGpuQueries));
            memset(g_ArenaTerrainGpuSamples, 0, sizeof(g_ArenaTerrainGpuSamples));
            g_ArenaTerrainGpuContext = context;
            glGenQueries(32, g_ArenaTerrainGpuQueries);
        }
        for (int i = 0; i < 16; ++i)
        {
            ArenaTerrainGpuSample& sample = g_ArenaTerrainGpuSamples[i];
            if (!sample.pending) continue;
            GLint available = GL_FALSE;
            glGetQueryObjectiv(g_ArenaTerrainGpuQueries[i * 2 + 1], GL_QUERY_RESULT_AVAILABLE, &available);
            if (!available) continue;
            GLuint64 begin = 0, end = 0;
            glGetQueryObjectui64v(g_ArenaTerrainGpuQueries[i * 2], GL_QUERY_RESULT, &begin);
            glGetQueryObjectui64v(g_ArenaTerrainGpuQueries[i * 2 + 1], GL_QUERY_RESULT, &end);
            FILE* profile = fopen("MapRenderGPU.log", "ab");
            if (profile)
            {
                fprintf(profile, "Time=%lu World=%d Stage=%s Zoom=%.1f CPUms=%.3f GPUms=%.3f FPS=%.1f\r\n",
                    sample.tick, sample.world, MAP_GPU_STAGE_NAMES[sample.stage], sample.zoom, sample.cpuMs,
                    end >= begin ? double(end - begin) / 1000000.0 : -1.0, sample.fps);
                fclose(profile);
            }
            sample.pending = false;
        }
        const DWORD now = GetTickCount();
        if (now - g_ArenaTerrainGpuLastSample[stage] < 2000) return -1;
        for (int i = 0; i < 16; ++i)
        {
            if (g_ArenaTerrainGpuSamples[i].pending || !g_ArenaTerrainGpuQueries[i * 2] ||
                !g_ArenaTerrainGpuQueries[i * 2 + 1]) continue;
            g_ArenaTerrainGpuLastSample[stage] = now;
            ArenaTerrainGpuSample& sample = g_ArenaTerrainGpuSamples[i];
            sample.tick = now;
            sample.world = gMapManager->currentMap;
            sample.stage = stage;
            sample.zoom = CameraFactorPtr->CurrentCam()->zoom;
            sample.fps = RealRenderFPS;
            glQueryCounter(g_ArenaTerrainGpuQueries[i * 2], GL_TIMESTAMP);
            return i;
        }
        return -1;
    }

    void EndArenaTerrainGpuSample(int index, double cpuMs)
    {
        if (index < 0) return;
        glQueryCounter(g_ArenaTerrainGpuQueries[index * 2 + 1], GL_TIMESTAMP);
        g_ArenaTerrainGpuSamples[index].cpuMs = cpuMs;
        g_ArenaTerrainGpuSamples[index].pending = true;
    }
}

bool RenderMainScene()
{
	if (EnableMainRender == false)
	{
		return false;
	}

	if ((LoadingWorld) > 30)
	{
		return false;
	}

	FogEnable = (g_pOption == NULL || g_pOption->IsFogEnabled());

	vec3_t pos;

	if (MoveMainCamera() == true)
	{
		VectorCopy(Hero->Object.StartPosition, pos);
	}
	else
	{
		g_pCatapultWindow->GetCameraPos(pos);

		if (g_Direction.IsDirection() && g_Direction.m_bDownHero == false)
		{
			g_Direction.GetCameraPosition(pos);
		}
	}

	int Width, Height;

	BYTE byWaterMap = 0;
	const bool isWaterTerrain = IsWaterTerrain();

	if (CameraTopViewEnable == false)
	{
#if MAIN_UPDATE > 303
		if (WindowWidth > 800 || (GMProtect->LookAndFeelType() == 3 || GMProtect->LookAndFeelType() == 4))
		{
			Height = GetWindowsY;
		}
		else
		{
			Height = GetWindowsY - 48;
		}
#else
		Height = GetWindowsY;
#endif // MAIN_UPDATE == 303
	}
	else
	{
		Height = GetWindowsY;
	}

#if MAIN_UPDATE > 303
	if (gmProtect->ScreenType == 0 && (GMProtect->LookAndFeelType() != 3 && GMProtect->LookAndFeelType() != 4))
	{
		Width = GetScreenWidth();
		Height = GetWindowsY - 48;
	}
	else
	{
		if (WindowWidth > 800 || (GMProtect->LookAndFeelType() == 3 || GMProtect->LookAndFeelType() == 4))
		{
			Width = GetWindowsX;
		}
		else
		{
			Width = GetScreenWidth();
		}
	}
#else
	Width = GetWindowsX;
#endif // MAIN_UPDATE == 303

	if (World == WD_0LORENCIA)
	{
		glClearColor(10 / 256.f, 20 / 256.f, 14 / 256.f, 1.f);
	}
	else if (World == WD_2DEVIAS)
	{
		glClearColor(0.f / 256.f, 0.f / 256.f, 10.f / 256.f, 1.f);
	}
	else if (World == WD_10HEAVEN)
	{
		glClearColor(3.f / 256.f, 25.f / 256.f, 44.f / 256.f, 1.f);
	}
	else if (gMapManager->InChaosCastle() == true)
	{
		glClearColor(0 / 256.f, 0 / 256.f, 0 / 256.f, 1.f);
	}
	else if (World >= WD_45CURSEDTEMPLE_LV1 && World <= WD_45CURSEDTEMPLE_LV6)
	{
		glClearColor(9.f / 256.f, 8.f / 256.f, 33.f / 256.f, 1.f);
	}
	else if (gMapManager->InHellas() == true)
	{
		byWaterMap = 1;
		glClearColor(0.f / 256.f, 0.f / 256.f, 0.f / 256.f, 1.f);
	}
	else
	{
		glClearColor(0 / 256.f, 0 / 256.f, 0 / 256.f, 1.f);
	}

	UpdateServerAtmosphere();
	BeginOpengl(0, 0, Width, Height, true);

	CreateFrustrum((float)Width / (float)GetScreenWidth(), pos);

	// Cascaded shadow maps belong to the Modern backend. Legacy already uses
	// its original projected shadows, so avoid rebuilding modern camera/shadow
	// matrices every frame when that backend is not active.
	if (g_RenderBackend.GetModernBackend() != nullptr)
	{
		float sunX, sunY, sunZ;
		GetServerVisualSunDirection(sunX, sunY, sunZ);
		glm::vec3 lightDir = glm::normalize(glm::vec3(sunX, sunY, sunZ));
		glm::vec3 cameraPos = glm::vec3(CameraPosition[0], CameraPosition[1], CameraPosition[2]);
		
		// Build view matrix from CameraAngle
		glm::mat4 cameraView = glm::mat4(1.0f);
		cameraView = glm::rotate(cameraView, glm::radians(-CameraAngle[0]), glm::vec3(1.0f, 0.0f, 0.0f));
		cameraView = glm::rotate(cameraView, glm::radians(-CameraAngle[2]), glm::vec3(0.0f, 0.0f, 1.0f));
		cameraView = glm::translate(cameraView, -cameraPos);
		
		// Build projection matrix
		float aspect = (float)Width / (float)Height;
		glm::mat4 cameraProj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, CameraViewFar);
		
		g_ShadowRenderer.RenderShadowMaps(lightDir, cameraPos, cameraView, cameraProj);
	}
	// ShadowRenderer binds framebuffer 0. Capture only after that pass, so the
	// terrain, objects and sky all reach the scene framebuffer.
	if (g_PostProcessPipeline.IsInitialized() && g_PostProcessPipeline.IsEnabled())
	{
		// Width/Height are logical MU coordinates; the FBO must match the
		// physical drawable size used by glViewport2.
		g_PostProcessPipeline.Resize(WindowWidth, WindowHeight);
		g_PostProcessPipeline.BeginFrame();
	}
	RenderServerVisualSky();
	bool horizonFogWasEnabled = false;
	const bool horizonFogActive = BeginServerHorizonFog(horizonFogWasEnabled);

	if (gMapManager->InBattleCastle())
	{
		if (battleCastle::InBattleCastle2(Hero->Object.Position))
		{
			vec3_t Color = { 0.f, 0.f, 0.f };
			battleCastle::StartFog(Color);
		}
		else
		{
			glDisable(GL_FOG);
		}
	}

	CreateScreenVector(MouseX, MouseY, MouseTarget);

	if (gwinhandle->CheckPerformance())
	{
		g_LegacyStageTiming = {};
		g_TwistWeaponRenderMs = 0.0;
		g_TwistWeaponDrawCount = 0;
		const auto renderTerrainTimed = []() {
            const int gpuSample = BeginArenaTerrainGpuSample(0);
			const auto start = std::chrono::steady_clock::now();
			RenderTerrain(false);
            const double cpuMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
			g_LegacyStageTiming.TerrainOnlyMs += cpuMs;
            EndArenaTerrainGpuSample(gpuSample, cpuMs);
		};
		const auto renderObjectsTimed = []() {
            const int gpuSample = BeginArenaTerrainGpuSample(1);
			const auto start = std::chrono::steady_clock::now();
			RenderObjects();
            const double cpuMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
			g_LegacyStageTiming.ObjectsOnlyMs += cpuMs;
            EndArenaTerrainGpuSample(gpuSample, cpuMs);
		};
		auto legacyStageStart = std::chrono::steady_clock::now();
		if (!isWaterTerrain)
		{
			if (World == WD_39KANTURU_3RD)
			{
				if (!g_Direction.m_CKanturu.IsMayaScene())
					renderTerrainTimed();
			}
			else if (World != WD_10HEAVEN && World != -1)
			{
				if (gMapManager->IsPKField() || IsDoppelGanger2())
				{
					renderObjectsTimed();
				}
				renderTerrainTimed();
			}
		}

		if (!gMapManager->IsPKField() && !IsDoppelGanger2())
			renderObjectsTimed();

		RenderEffectShadows();
		RenderBoids();
		auto legacyStageEnd = std::chrono::steady_clock::now();
		g_LegacyStageTiming.TerrainObjectsMs =
			std::chrono::duration<double, std::milli>(legacyStageEnd - legacyStageStart).count();

		legacyStageStart = legacyStageEnd;
		BeginCharacterGlowTiming();
        const int charactersGpuSample = BeginArenaTerrainGpuSample(2);
		RenderCharactersClient();
		legacyStageEnd = std::chrono::steady_clock::now();
		g_LegacyStageTiming.CharactersMs =
			std::chrono::duration<double, std::milli>(legacyStageEnd - legacyStageStart).count();
        EndArenaTerrainGpuSample(charactersGpuSample, g_LegacyStageTiming.CharactersMs);
		EndCharacterGlowTiming();
		legacyStageStart = legacyStageEnd;

		if (EditFlag != EDIT_NONE)
			RenderTerrain(true);

        const int extrasGpuSample = BeginArenaTerrainGpuSample(4);
		if (!CameraTopViewEnable)
			RenderItems();

		RenderFishs();
		gGoboidManager->RenderBugs();
		RenderLeaves();

		if (!gMapManager->InChaosCastle())
			if (g_pOption->GetRenderPets()) ThePetProcess().RenderPets();

		RenderBoids(true);
		RenderObjects_AfterCharacter();
        RenderLorenciaWaterReflections();
		legacyStageEnd = std::chrono::steady_clock::now();
		g_LegacyStageTiming.WorldExtrasMs =
			std::chrono::duration<double, std::milli>(legacyStageEnd - legacyStageStart).count();
        EndArenaTerrainGpuSample(extrasGpuSample, g_LegacyStageTiming.WorldExtrasMs);
		legacyStageStart = legacyStageEnd;

        const int effectsGpuSample = BeginArenaTerrainGpuSample(3);
		RenderJoints(byWaterMap);
		auto effectStageEnd = std::chrono::steady_clock::now();
		g_LegacyStageTiming.JointsMs =
			std::chrono::duration<double, std::milli>(effectStageEnd - legacyStageStart).count();
		auto effectStageStart = effectStageEnd;

		RenderEffects();
		effectStageEnd = std::chrono::steady_clock::now();
		g_LegacyStageTiming.EffectsMs =
			std::chrono::duration<double, std::milli>(effectStageEnd - effectStageStart).count();
		effectStageStart = effectStageEnd;

		RenderBlurs();
		effectStageEnd = std::chrono::steady_clock::now();
		g_LegacyStageTiming.BlursMs =
			std::chrono::duration<double, std::milli>(effectStageEnd - effectStageStart).count();
		effectStageStart = effectStageEnd;
		CheckSprites();
		BeginSprite();

		if ((World == WD_2DEVIAS && HeroTile != 3 && HeroTile < 10)
			|| IsIceCity()
			|| IsSantaTown()
			|| gMapManager->IsPKField()
			|| IsDoppelGanger2()
			|| gMapManager->IsEmpireGuardian1()
			|| gMapManager->IsEmpireGuardian2()
			|| gMapManager->IsEmpireGuardian3()
			|| gMapManager->IsEmpireGuardian4()
			|| IsUnitedMarketPlace()
			)
		{
			RenderLeaves();
		}

		RenderSprites();
		RenderParticles();

		if (!isWaterTerrain)
		{
			RenderPoints(byWaterMap);
		}

		EndSprite();
		effectStageEnd = std::chrono::steady_clock::now();
		g_LegacyStageTiming.SpritesParticlesMs =
			std::chrono::duration<double, std::milli>(effectStageEnd - effectStageStart).count();
		effectStageStart = effectStageEnd;

		RenderAfterEffects();
		effectStageEnd = std::chrono::steady_clock::now();
		g_LegacyStageTiming.AfterEffectsMs =
			std::chrono::duration<double, std::milli>(effectStageEnd - effectStageStart).count();
		legacyStageEnd = std::chrono::steady_clock::now();
		g_LegacyStageTiming.EffectsParticlesMs =
			std::chrono::duration<double, std::milli>(legacyStageEnd - legacyStageStart).count();
        EndArenaTerrainGpuSample(effectsGpuSample, g_LegacyStageTiming.EffectsParticlesMs);
		legacyStageStart = legacyStageEnd;

        const int waterGpuSample = isWaterTerrain ? BeginArenaTerrainGpuSample(5) : -1;
		if (isWaterTerrain)
		{
			byWaterMap = 2;

			EndOpengl();
			BeginOpengl(0, 0, Width, Height, true, !g_PostProcessPipeline.IsCapturing());
			RenderWaterTerrain();
			RenderJoints(byWaterMap);
			RenderEffects(true);
			RenderBlurs();
			CheckSprites();
			BeginSprite();

			if (World == WD_2DEVIAS && HeroTile != 3 && HeroTile < 10)
				RenderLeaves();

			RenderSprites(byWaterMap);
			RenderParticles(byWaterMap);
			RenderPoints(byWaterMap);

			EndSprite();
			EndOpengl();
			BeginOpengl(0, 0, Width, Height, true, !g_PostProcessPipeline.IsCapturing());
		}
		legacyStageEnd = std::chrono::steady_clock::now();
		g_LegacyStageTiming.WaterMs =
			std::chrono::duration<double, std::milli>(legacyStageEnd - legacyStageStart).count();
        EndArenaTerrainGpuSample(waterGpuSample, g_LegacyStageTiming.WaterMs);

		if (gMapManager->InBattleCastle())
		{
			if (battleCastle::InBattleCastle2(Hero->Object.Position))
			{
				battleCastle::EndFog();
			}
		}
	}

    const int weatherGpuSample = BeginArenaTerrainGpuSample(6);
	auto weatherStageStart = std::chrono::steady_clock::now();
	UpdateAndRenderSurfaceTracks();
	RenderServerSnowCover();
	RenderServerWeatherPrecipitation();
	RenderServerSandstorm();
	RenderServerSnow();
	RenderServerFog();
	RenderServerVisualTimeTint();
	g_LegacyStageTiming.WeatherMs = std::chrono::duration<double, std::milli>(
		std::chrono::steady_clock::now() - weatherStageStart).count();
    EndArenaTerrainGpuSample(weatherGpuSample, g_LegacyStageTiming.WeatherMs);
	RenderMuHelperRanges();
	if (horizonFogActive)
	{
		glPopAttrib();
		if (horizonFogWasEnabled) glEnable(GL_FOG); else glDisable(GL_FOG);
	}

    const int interfaceGpuSample = BeginArenaTerrainGpuSample(7);
	auto interfaceStageStart = std::chrono::steady_clock::now();
	SelectObjects();
	if (g_PostProcessPipeline.IsCapturing())
		g_PostProcessPipeline.EndFrame(g_RenderBackend.GetModernBackend());
	BeginBitmap();
	RenderObjectDescription();

	if (gwinhandle->CheckPerformance())
	{
		if (CameraTopViewEnable == false)
		{
			RenderInterface(true);
		}
		RenderTournamentInterface();
		EndBitmap();

		g_pPartyManager->Render();
		auto newUIStageStart = std::chrono::steady_clock::now();
		g_pNewUISystem->Render();
		g_LegacyStageTiming.NewUIMs = std::chrono::duration<double, std::milli>(
			std::chrono::steady_clock::now() - newUIStageStart).count();

		BeginBitmap();
		// ADICIONE A LINHA AQUI :
		//CGMProtect::Instance()->DrawCharacterResets();

		RenderInfomation();
		RenderServerClock();

#ifdef ENABLE_EDIT
		RenderDebugWindow();
#endif //ENABLE_EDIT

		EndBitmap();
		BeginBitmap();

		RenderCursor();

		EndBitmap();
		EndOpengl();
	}
	g_LegacyStageTiming.InterfaceMs = std::chrono::duration<double, std::milli>(
		std::chrono::steady_clock::now() - interfaceStageStart).count();
    EndArenaTerrainGpuSample(interfaceGpuSample, g_LegacyStageTiming.InterfaceMs);

	return true;
}

void MoveClientManager()
{
	if (SceneFlag == LOG_IN_SCENE && (InitLogIn && !CUIMng::Instance().m_CreditWin.IsShow()))
	{
		InitTerrainLight();
		MoveObjects();
		MoveEffects();
		MoveJoints();
		MoveParticles();
	}
	else if (SceneFlag == CHARACTER_SCENE && (CurrentProtocolState >= RECEIVE_CHARACTERS_LIST && InitCharacterScene))
	{
		InitTerrainLight();
		MoveObjects();
		MoveEffects();
		MoveJoints();
		MoveParticles();
	}
	else if (SceneFlag == MAIN_SCENE && (CurrentProtocolState == RECEIVE_JOIN_MAP_SERVER && EnableMainRender && InitMainScene))
	{
		InitTerrainLight();
		MoveObjects();
		MoveEffects();
		MoveJoints();
		MoveParticles();
	}
}

void MainScene(HDC hDC)
{
	// Pace the complete client frame.  Starting the timer immediately before
	// rendering excluded input, animation/effect updates and scene preparation
	// from the 16.67 ms budget, so even an empty scene settled below 60 FPS.
	auto thread_tick = gsteady_clock->GetthreadTime();
	g_LegacyUpdateTiming = {};
	double frameUpdateMs = 0.0;
	double bitmapManageMs = 0.0;
	double frameSetupMs = 0.0;
	double sceneRenderMs = 0.0;
	double physicsRenderMs = 0.0;
	double presentMs = 0.0;
	const auto frameUpdateStart = std::chrono::steady_clock::now();

	const uintmax_t fixedDeltaTime = gsteady_clock->Getframe_per_second();

	static uintmax_t accumulatedTime = fixedDeltaTime;

	gsteady_clock->LoadInformationFps();

	// Normal client update: one logic step per rendered frame.
	{
		if (accumulatedTime >= fixedDeltaTime)
			accumulatedTime -= fixedDeltaTime;
	{
		g_pNewKeyInput->ScanAsyncKeyState();

		if (LOG_IN_SCENE == SceneFlag || CHARACTER_SCENE == SceneFlag)
		{
			double dDeltaTick = g_pTimer->GetTimeElapsed();
			dDeltaTick = MIN(dDeltaTick, 200.0);
			g_pTimer->ResetTimer();

			CInput::Instance().Update();
			CUIMng::Instance().Update(dDeltaTick);

			if (Hero != NULL)
			{
				SIXEMULATOR();
			}
		}

		g_dwMouseUseUIID = 0;

		switch (SceneFlag)
		{
		case LOG_IN_SCENE:
			NewMoveLogInScene();
			break;
		case CHARACTER_SCENE:
			NewMoveCharacterScene();
			break;
		case MAIN_SCENE:
			MoveMainScene();
			break;
		}

		g_PhysicsManager.Move(0.005f);

		MoveNotices();

		if (PressKey(VK_SNAPSHOT))
		{
			if (GrabEnable)
				GrabEnable = false;
			else
				GrabEnable = true;
		}

		if (ChatTime > 0)
			ChatTime -= timefac(1.f);

		if (MacroTime > 0)
			MacroTime--;

		if (checkNormalizer)
		{
			WaterTextureNumber++;
			WaterTextureNumber %= 32;
		}

		MoveSceneFrame++;
		frame_scene_desplace++;
	}
	}
	frameUpdateMs = std::chrono::duration<double, std::milli>(
		std::chrono::steady_clock::now() - frameUpdateStart).count();

	if (Destroy)
	{
		return;
	}

	const auto bitmapManageStart = std::chrono::steady_clock::now();
	Bitmaps.Manage();
	bitmapManageMs = std::chrono::duration<double, std::milli>(
		std::chrono::steady_clock::now() - bitmapManageStart).count();
	const auto frameSetupStart = std::chrono::steady_clock::now();

	Set3DSoundPosition();

	SYSTEMTIME st;
	GetLocalTime(&st);
	sprintf(GrabFileName, GMProtect->GetScreenPath(), st.wMonth, st.wDay, st.wHour, st.wMinute, GrabScreen);
	char Text[256];
	sprintf(Text, GlobalText[459], GrabFileName);
	char lpszTemp[64];
	wsprintf(lpszTemp, " [%s / %s]", g_ServerListManager->GetSelectServerName(), Hero->ID);
	strcat(Text, lpszTemp);
	int iCaptureMode = 1;

	if (HIBYTE(GetAsyncKeyState(VK_SHIFT)))
	{
		iCaptureMode = 1 - iCaptureMode;
	}
	if (GrabEnable && iCaptureMode == 1)
	{
		g_pChatListBox->AddText("", Text, SEASON3B::TYPE_SYSTEM_MESSAGE);
	}

	if (World == WD_10HEAVEN)
	{
		glClearColor(3.f / 256.f, 25.f / 256.f, 44.f / 256.f, 1.f);
	}
	else if (World == WD_73NEW_LOGIN_SCENE || World == WD_74NEW_CHARACTER_SCENE)
	{
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	}
	else if (gMapManager->InHellas())
	{
		glClearColor(30.f / 256.f, 40.f / 256.f, 40.f / 256.f, 1.f);
	}
	else if (gMapManager->InChaosCastle() == true)
	{
		glClearColor(0.f, 0.f, 0.f, 1.f);
	}
	else if (gMapManager->InBattleCastle() && battleCastle::InBattleCastle2(Hero->Object.Position))
	{
		glClearColor(0.f, 0.f, 0.f, 1.f);
	}
	else if (World >= WD_45CURSEDTEMPLE_LV1 && World <= WD_45CURSEDTEMPLE_LV6)
	{
		glClearColor(9.f / 256.f, 8.f / 256.f, 33.f / 256.f, 1.f);
	}
	else if (World == WD_94NEW_LOGIN_SCENE)
	{
		glClearColor(0.f, 0.f, 0.f, 1.f);
	}
	else if (World == WD_51HOME_6TH_CHAR)
	{
		glClearColor(178.f / 256.f, 178.f / 256.f, 178.f / 256.f, 1.f);
	}
	else if (World == WD_65DOPPLEGANGER1)
	{
		glClearColor(148.f / 256.f, 179.f / 256.f, 223.f / 256.f, 1.f);
	}
	else
	{
		if (gmProtect->SceneLogin == 3 && SceneFlag == LOG_IN_SCENE)
		{
			glClearColor(178.f / 256.f, 178.f / 256.f, 178.f / 256.f, 1.f);
		}
		else if (gmProtect->SceneCharacter == 3 && SceneFlag == CHARACTER_SCENE)
		{
			glClearColor(0.f, 0.f, 0.f, 1.f);
		}
		else
		{
			glClearColor(0 / 256.f, 0 / 256.f, 0 / 256.f, 1.f);
		}
	}

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	frameSetupMs = std::chrono::duration<double, std::milli>(
		std::chrono::steady_clock::now() - frameSetupStart).count();


	bool Success = false;
	RenderStats* modernStats = g_RenderBackend.GetModernBackend() != nullptr
		? g_RenderBackend.GetModernBackend()->GetRenderStats() : nullptr;
	if (modernStats != nullptr && modernStats->IsEnabled())
		modernStats->BeginFrame();

	const auto sceneRenderStart = std::chrono::steady_clock::now();
	switch (SceneFlag)
	{
	case LOG_IN_SCENE:
		Success = NewRenderLogInScene(hDC);
		break;
	case CHARACTER_SCENE:
		Success = NewRenderCharacterScene(hDC);
		break;
	case MAIN_SCENE:
		Success = RenderMainScene();
		break;
	}
	sceneRenderMs = std::chrono::duration<double, std::milli>(
		std::chrono::steady_clock::now() - sceneRenderStart).count();

	const auto physicsRenderStart = std::chrono::steady_clock::now();
	g_PhysicsManager.Render();
	physicsRenderMs = std::chrono::duration<double, std::milli>(
		std::chrono::steady_clock::now() - physicsRenderStart).count();

	if (GrabEnable)
	{
		SaveScreen();
	}

	if (GrabEnable && iCaptureMode == 0)
	{
		g_pChatListBox->AddText("", Text, SEASON3B::TYPE_SYSTEM_MESSAGE);
	}
	GrabEnable = false;


	if (Success)
	{
		const auto presentStart = std::chrono::steady_clock::now();
		// SwapBuffers already flushes the OpenGL command stream. An explicit
		// glFlush here is redundant and can introduce an extra driver boundary
		// before presentation without changing the rendered image.
		SwapBuffers(hDC);
		presentMs = std::chrono::duration<double, std::milli>(
			std::chrono::steady_clock::now() - presentStart).count();
	}

	if (modernStats != nullptr && modernStats->IsEnabled())
	{
		modernStats->EndFrame();
	}

	// Sample before synchronous diagnostics so logging cannot inflate WorkMs.
	const double measuredWorkMs = std::chrono::duration<double, std::milli>(
		std::chrono::steady_clock::now() - thread_tick).count();
	UpdateRealRenderFPSCounter();

	// Window-focused profiler.  It compares each frame with a rolling baseline
	// collected only while no normal window is open, making it possible to tell
	// whether the drop comes from the window itself or the world behind it.
	{
		static const bool windowProfileEnabled =
			GetPrivateProfileIntA("Renderer", "Stats", 0, ".\\config.ini") != 0;
		static WindowProfileBaseline baseline = {};
		static DWORD previousSignature = 0;
		static int previousWindowCount = 0;
		static DWORD nextWindowSample = 0;

		if (windowProfileEnabled && SceneFlag == MAIN_SCENE && g_pNewUIMng != nullptr)
		{
			const SEASON3B::SUIFrameProfileSnapshot& uiProfile =
				g_pNewUIMng->GetFrameProfileSnapshot();
			const DWORD now = GetTickCount();
			const DWORD signature = BuildWindowProfileSignature(uiProfile);
			const bool stateChanged = signature != previousSignature ||
				uiProfile.VisibleWindowCount != previousWindowCount;

			if (uiProfile.VisibleWindowCount == 0 && measuredWorkMs < 80.0)
			{
				UpdateWindowProfileAverage(baseline.WorkMs, measuredWorkMs, baseline.Samples);
				UpdateWindowProfileAverage(baseline.UIMs, uiProfile.TotalMs, baseline.Samples);
				UpdateWindowProfileAverage(baseline.TerrainMs,
					g_LegacyStageTiming.TerrainObjectsMs, baseline.Samples);
				UpdateWindowProfileAverage(baseline.CharactersMs,
					g_LegacyStageTiming.CharactersMs, baseline.Samples);
				UpdateWindowProfileAverage(baseline.EffectsMs,
					g_LegacyStageTiming.EffectsParticlesMs, baseline.Samples);
				UpdateWindowProfileAverage(baseline.UpdateMs, frameUpdateMs, baseline.Samples);
				UpdateWindowProfileAverage(baseline.PresentMs, presentMs, baseline.Samples);
				if (baseline.Samples < 1000000)
					++baseline.Samples;
				baseline.Ready = baseline.Samples >= 30;
			}

			const bool periodicSample = nextWindowSample == 0 ||
				static_cast<LONG>(now - nextWindowSample) >= 0;
			const bool shouldLog =
				(uiProfile.VisibleWindowCount > 0 && (stateChanged || periodicSample)) ||
				(uiProfile.VisibleWindowCount == 0 && previousWindowCount > 0);

			if (shouldLog)
			{
				nextWindowSample = now + 500;
				char windowNames[768];
				BuildWindowProfileNames(uiProfile, windowNames, sizeof(windowNames));

				const double uiDelta = baseline.Ready ? uiProfile.TotalMs - baseline.UIMs : uiProfile.WindowMs;
				const double terrainDelta = baseline.Ready
					? g_LegacyStageTiming.TerrainObjectsMs - baseline.TerrainMs
					: g_LegacyStageTiming.TerrainObjectsMs;
				const double characterDelta = baseline.Ready
					? g_LegacyStageTiming.CharactersMs - baseline.CharactersMs
					: g_LegacyStageTiming.CharactersMs;
				const double effectsDelta = baseline.Ready
					? g_LegacyStageTiming.EffectsParticlesMs - baseline.EffectsMs
					: g_LegacyStageTiming.EffectsParticlesMs;
				const double updateDelta = baseline.Ready ? frameUpdateMs - baseline.UpdateMs : frameUpdateMs;
				const double presentDelta = baseline.Ready ? presentMs - baseline.PresentMs : presentMs;

				const char* cause = "WindowUI";
				double causeDelta = uiDelta;
				if (terrainDelta > causeDelta) { cause = "TerrainObjects"; causeDelta = terrainDelta; }
				if (characterDelta > causeDelta) { cause = "Characters"; causeDelta = characterDelta; }
				if (effectsDelta > causeDelta) { cause = "EffectsParticles"; causeDelta = effectsDelta; }
				if (updateDelta > causeDelta) { cause = "Update"; causeDelta = updateDelta; }
				if (presentDelta > causeDelta) { cause = "PresentDriver"; causeDelta = presentDelta; }

				const char* eventName = uiProfile.VisibleWindowCount == 0 ? "CLOSE" :
					(previousWindowCount == 0 ? "OPEN" : (stateChanged ? "CHANGE" : "SAMPLE"));
				const double workDelta = baseline.Ready ? measuredWorkMs - baseline.WorkMs : 0.0;
				const double budgetOverMs = measuredWorkMs > 16.6667 ? measuredWorkMs - 16.6667 : 0.0;
				const DWORD topKey = uiProfile.TopWindowKeys[0];
				const char* topName = (topKey == 0xFFFFFFFF)
					? "None" : SEASON3B::GetNewUIInterfaceProfileName(topKey);

				char line[2048];
				const int length = _snprintf_s(line, sizeof(line), _TRUNCATE,
					"Time=%lu Event=%s World=%d Windows=%s WindowCount=%d FPS=%.1f WorkMs=%.2f BaselineWorkMs=%.2f WorkDeltaMs=%.2f BudgetOverMs=%.2f UI=%.2f UIDelta=%.2f WindowDirectMs=%.2f TopWindow=%s(%lu):%.2f Top2=%s(%lu):%.2f Top3=%s(%lu):%.2f TopUI=%s(%lu):%.2f TopUI2=%s(%lu):%.2f TopUI3=%s(%lu):%.2f Terrain=%.2f TerrainDelta=%.2f Characters=%.2f CharactersDelta=%.2f EffectsParticles=%.2f EffectsDelta=%.2f Update=%.2f UpdateDelta=%.2f Present=%.2f PresentDelta=%.2f Cause=%s CauseDelta=%.2f BaselineReady=%d\r\n",
					now, eventName, World, windowNames, uiProfile.VisibleWindowCount,
					RealRenderFPS, measuredWorkMs, baseline.WorkMs, workDelta, budgetOverMs,
					uiProfile.TotalMs, uiDelta, uiProfile.WindowMs,
					topName, topKey, uiProfile.TopWindowMs[0],
					SEASON3B::GetNewUIInterfaceProfileName(uiProfile.TopWindowKeys[1]),
					uiProfile.TopWindowKeys[1], uiProfile.TopWindowMs[1],
					SEASON3B::GetNewUIInterfaceProfileName(uiProfile.TopWindowKeys[2]),
					uiProfile.TopWindowKeys[2], uiProfile.TopWindowMs[2],
					SEASON3B::GetNewUIInterfaceProfileName(uiProfile.TopKeys[0]),
					uiProfile.TopKeys[0], uiProfile.TopMs[0],
					SEASON3B::GetNewUIInterfaceProfileName(uiProfile.TopKeys[1]),
					uiProfile.TopKeys[1], uiProfile.TopMs[1],
					SEASON3B::GetNewUIInterfaceProfileName(uiProfile.TopKeys[2]),
					uiProfile.TopKeys[2], uiProfile.TopMs[2],
					g_LegacyStageTiming.TerrainObjectsMs, terrainDelta,
					g_LegacyStageTiming.CharactersMs, characterDelta,
					g_LegacyStageTiming.EffectsParticlesMs, effectsDelta,
					frameUpdateMs, updateDelta, presentMs, presentDelta,
					cause, causeDelta, baseline.Ready ? 1 : 0);

				HANDLE file = CreateFileA(".\\WindowFPSProfile.log", FILE_APPEND_DATA,
					FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
					FILE_ATTRIBUTE_NORMAL, nullptr);
				if (file != INVALID_HANDLE_VALUE)
				{
					DWORD written = 0;
					WriteFile(file, line, static_cast<DWORD>((std::max)(length, 0)), &written, nullptr);
					CloseHandle(file);
				}
			}

			previousSignature = signature;
			previousWindowCount = uiProfile.VisibleWindowCount;
		}
	}

	// Capture short stalls (such as opening/closing UI windows) that disappear
	// between the regular five-second workload samples.
	{
		static const bool spikeTelemetryEnabled =
			GetPrivateProfileIntA("Renderer", "Stats", 0, ".\\config.ini") != 0 &&
			GetPrivateProfileIntA("Renderer", "UISpikeLog", 0, ".\\config.ini") != 0;
		static DWORD lastSpikeLog = 0;
		const DWORD spikeNow = GetTickCount();
		const double spikeWorkMs = measuredWorkMs;
		if (spikeTelemetryEnabled && SceneFlag == MAIN_SCENE && spikeWorkMs >= 33.0 &&
			(spikeNow - lastSpikeLog) >= 250)
		{
			lastSpikeLog = spikeNow;
			char spikeLine[640];
			const int spikeLength = _snprintf_s(spikeLine, sizeof(spikeLine), _TRUNCATE,
				"Time=%lu World=%d WorkMs=%.2f UpdateMs=%.2f BitmapMs=%.2f SetupMs=%.2f SceneRenderMs=%.2f PhysicsMs=%.2f PresentMs=%.2f TerrainObjectsMs=%.2f CharactersMs=%.2f EffectsParticlesMs=%.2f WeatherMs=%.2f InterfaceMs=%.2f NewUIMs=%.2f\r\n",
				spikeNow, World, spikeWorkMs, frameUpdateMs, bitmapManageMs,
				frameSetupMs, sceneRenderMs, physicsRenderMs, presentMs,
				g_LegacyStageTiming.TerrainObjectsMs,
				g_LegacyStageTiming.CharactersMs, g_LegacyStageTiming.EffectsParticlesMs,
				g_LegacyStageTiming.WeatherMs,
				g_LegacyStageTiming.InterfaceMs, g_LegacyStageTiming.NewUIMs);
			HANDLE spikeFile = CreateFileA(".\\RendererUISpike.log", FILE_APPEND_DATA,
				FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
				FILE_ATTRIBUTE_NORMAL, nullptr);
			if (spikeFile != INVALID_HANDLE_VALUE)
			{
				DWORD written = 0;
				WriteFile(spikeFile, spikeLine,
					static_cast<DWORD>((std::max)(spikeLength, 0)), &written, nullptr);
				CloseHandle(spikeFile);
			}
		}
	}

	// Low-frequency workload snapshot.  FPS alone cannot distinguish an empty
	// map from a crowded/effect-heavy frame, so record the active legacy pools
	// and the real work time before the frame limiter waits.  This runs only
	// once every five seconds, plus slow frames at most once per second.
	{
		static const int workloadTelemetryEnabled =
			GetPrivateProfileIntA("Renderer", "Stats", 0, ".\\config.ini");
		static DWORD nextWorkloadSample = 0;
		static DWORD nextSpikeSample = 0;
		const DWORD now = GetTickCount();
		const bool periodicSample = nextWorkloadSample == 0 ||
			static_cast<LONG>(now - nextWorkloadSample) >= 0;
		const bool spikeSample = measuredWorkMs > 18.0 &&
			(nextSpikeSample == 0 || static_cast<LONG>(now - nextSpikeSample) >= 0);
		if (workloadTelemetryEnabled != 0 && (periodicSample || spikeSample))
		{
			if (periodicSample) nextWorkloadSample = now + 5000;
			nextSpikeSample = now + 1000;
			int liveCharacters = 0;
			int liveEffects = 0;
			int liveJoints = 0;
			int liveParticles = 0;

			for (int i = 0; i < MAX_CHARACTERS_CLIENT; ++i)
			{
				CHARACTER* character = gmCharacters->GetCharacter(i);
				if (character != nullptr && character->Object.Live)
					++liveCharacters;
			}
			for (int i = 0; i < MAX_EFFECTS; ++i)
				if (Effects[i].Live) ++liveEffects;
			for (int i = 0; i < MAX_JOINTS; ++i)
				if (Joints[i].Live) ++liveJoints;
			for (int i = 0; i < MAX_PARTICLES; ++i)
				if (Particles[i].Live) ++liveParticles;

			const double workMs = measuredWorkMs;
			char line[1792];
			const int lineLength = _snprintf_s(line, sizeof(line), _TRUNCATE,
				"Time=%lu Scene=%d World=%d Camera3D=%d Zoom=%.2f AL_Effect=%d AL_Shadow=%d AL_Pets=%d AL_Glow=%d AL_Equipment=%d AL_Terrain=%d AL_Objects=%d AL_Particles=%d AL_RenderLevel=%d AL_ViewDistance=%d AL_TextureQuality=%d WorkMs=%.2f UpdateMs=%.2f BitmapMs=%.2f SetupMs=%.2f SceneRenderMs=%.2f PhysicsMs=%.2f PresentMs=%.2f Characters=%d Effects=%d Joints=%d Particles=%d TerrainObjectsMs=%.2f CharactersMs=%.2f WorldExtrasMs=%.2f EffectsParticlesMs=%.2f JointsMs=%.2f EffectsMs=%.2f BlursMs=%.2f SpritesParticlesMs=%.2f AfterEffectsMs=%.2f WaterMs=%.2f WeatherMs=%.2f InterfaceMs=%.2f NewUIMs=%.2f TerrainOnlyMs=%.2f ObjectsOnlyMs=%.2f TwistWeaponMs=%.2f TwistWeaponDraws=%d UpdateObjectsMs=%.2f UpdateCharactersMs=%.2f UpdatePetsMs=%.2f UpdateEffectsMs=%.2f UpdateJointsMs=%.2f UpdateParticlesMs=%.2f Spike=%d\r\n",
				now, SceneFlag, World, CameraFactorPtr->IsEnable() ? 1 : 0,
				CameraFactorPtr->CurrentCam()->zoom,
				g_pOption->GetRenderEffect() ? 1 : 0,
				g_pOption->GetRenderShadows() ? 1 : 0,
				g_pOption->GetRenderPets() ? 1 : 0,
				g_pOption->GetRenderGlow() ? 1 : 0,
				g_pOption->GetRenderEquipment() ? 1 : 0,
				g_pOption->GetRenderTerrain() ? 1 : 0,
				g_pOption->GetRenderObjects() ? 1 : 0,
				g_pOption->GetRenderParticles() ? 1 : 0,
				g_pOption->GetRenderLevel(),
				g_pOption->GetViewDistanceLevel(),
				g_pOption->GetTextureQuality(),
				workMs, frameUpdateMs,
				bitmapManageMs, frameSetupMs, sceneRenderMs, physicsRenderMs, presentMs,
				liveCharacters, liveEffects, liveJoints, liveParticles,
				g_LegacyStageTiming.TerrainObjectsMs, g_LegacyStageTiming.CharactersMs,
				g_LegacyStageTiming.WorldExtrasMs, g_LegacyStageTiming.EffectsParticlesMs,
				g_LegacyStageTiming.JointsMs, g_LegacyStageTiming.EffectsMs,
				g_LegacyStageTiming.BlursMs, g_LegacyStageTiming.SpritesParticlesMs,
				g_LegacyStageTiming.AfterEffectsMs,
				g_LegacyStageTiming.WaterMs, g_LegacyStageTiming.WeatherMs,
				g_LegacyStageTiming.InterfaceMs,
				g_LegacyStageTiming.NewUIMs,
				g_LegacyStageTiming.TerrainOnlyMs, g_LegacyStageTiming.ObjectsOnlyMs,
				g_TwistWeaponRenderMs, g_TwistWeaponDrawCount,
				g_LegacyUpdateTiming.ObjectsMs, g_LegacyUpdateTiming.CharactersMs,
				g_LegacyUpdateTiming.PetsMs, g_LegacyUpdateTiming.EffectsMs,
				g_LegacyUpdateTiming.JointsMs, g_LegacyUpdateTiming.ParticlesMs,
				spikeSample ? 1 : 0);
			HANDLE file = CreateFileA(".\\RendererWorkload.log", FILE_APPEND_DATA,
				FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
				FILE_ATTRIBUTE_NORMAL, nullptr);
			if (file != INVALID_HANDLE_VALUE)
			{
				DWORD written = 0;
				WriteFile(file, line, static_cast<DWORD>((std::max)(lineLength, 0)), &written, nullptr);
				CloseHandle(file);
			}
		}
	}

	uintmax_t DifTimer = gsteady_clock->thread_sleep(thread_tick);

	accumulatedTime += DifTimer;

	if (EnableSocket && SceneFlag == MAIN_SCENE)
	{
		if (SocketClient.GetSocket() == INVALID_SOCKET)
		{
			if (g_pReconnectUI->GetReconnectStatus() != RECONNECT_STATUS_RECONNECT)
			{
				static BOOL s_bClosed = FALSE;

				if (!s_bClosed)
				{
					s_bClosed = TRUE;
					g_ErrorReport.Write("> Connection closed. ");
					g_ErrorReport.WriteCurrentTime();
					SEASON3B::CreateMessageBox(MSGBOX_LAYOUT_CLASS(SEASON3B::CServerLostMsgBoxLayout));
				}
			}
		}
	}

	if (SceneFlag == MAIN_SCENE)
	{
		switch (World)
		{
		case WD_0LORENCIA:
			if (HeroTile == 4)
			{
				StopBuffer(SOUND_WIND01, true);
				StopBuffer(SOUND_RAIN01, true);
			}
			else
			{
				PlayBuffer(SOUND_WIND01, NULL, true);
				if (RainCurrent > 0)
					PlayBuffer(SOUND_RAIN01, NULL, true);
			}
			break;
		case WD_1DUNGEON:
			PlayBuffer(SOUND_DUNGEON01, NULL, true);
			break;
		case WD_2DEVIAS:
			if (HeroTile == 3 || HeroTile >= 10)
				StopBuffer(SOUND_WIND01, true);
			else
				PlayBuffer(SOUND_WIND01, NULL, true);
			break;
		case WD_3NORIA:
			PlayBuffer(SOUND_WIND01, NULL, true);
			if (rand() % 512 == 0)
				PlayBuffer(SOUND_FOREST01);
			break;
		case WD_4LOSTTOWER:
			PlayBuffer(SOUND_TOWER01, NULL, true);
			break;
		case WD_5UNKNOWN:
			//PlayBuffer(SOUND_BOSS01,NULL,true);
			break;
		case WD_7ATLANSE:
			PlayBuffer(SOUND_WATER01, NULL, true);
			break;
		case WD_8TARKAN:
			PlayBuffer(SOUND_DESERT01, NULL, true);
			break;
		case WD_10HEAVEN:
			PlayBuffer(SOUND_HEAVEN01, NULL, true);
			if ((rand() % 100) == 0)
			{
				//PlayBuffer(SOUND_HEAVEN01);
			}
			else if ((rand() % 10) == 0)
			{
				//PlayBuffer(SOUND_THUNDERS02);
			}
			break;
		case WD_58ICECITY_BOSS:
			PlayBuffer(SOUND_WIND01, NULL, true);
			break;
		case WD_79UNITEDMARKETPLACE:
		{
			PlayBuffer(SOUND_WIND01, NULL, true);
			PlayBuffer(SOUND_RAIN01, NULL, true);
		}
		break;
		case WD_80KARUTAN1:
			PlayBuffer(SOUND_KARUTAN_DESERT_ENV, NULL, true);
			break;
		case WD_81KARUTAN2:
			if (HeroTile == 12)
			{
				StopBuffer(SOUND_KARUTAN_DESERT_ENV, true);
				PlayBuffer(SOUND_KARUTAN_KARDAMAHAL_ENV, NULL, true);
			}
			else
			{
				StopBuffer(SOUND_KARUTAN_KARDAMAHAL_ENV, true);
				PlayBuffer(SOUND_KARUTAN_DESERT_ENV, NULL, true);
			}
			break;
		}
		if (World != WD_0LORENCIA && World != WD_2DEVIAS && World != WD_3NORIA && World != WD_58ICECITY_BOSS && World != WD_79UNITEDMARKETPLACE)
		{
			StopBuffer(SOUND_WIND01, true);
		}
		if (World != WD_0LORENCIA && gMapManager->InDevilSquare() == false && World != WD_79UNITEDMARKETPLACE)
		{
			StopBuffer(SOUND_RAIN01, true);
		}
		if (World != WD_1DUNGEON)
		{
			StopBuffer(SOUND_DUNGEON01, true);
		}
		if (World != WD_3NORIA)
		{
			StopBuffer(SOUND_FOREST01, true);
		}
		if (World != WD_4LOSTTOWER)
		{
			StopBuffer(SOUND_TOWER01, true);
		}
		if (World != WD_7ATLANSE)
		{
			StopBuffer(SOUND_WATER01, true);
		}
		if (World != WD_8TARKAN)
		{
			StopBuffer(SOUND_DESERT01, true);
		}
		if (World != WD_10HEAVEN)
		{
			StopBuffer(SOUND_HEAVEN01, true);
		}
		if (World != WD_51HOME_6TH_CHAR)
		{
			StopBuffer(SOUND_ELBELAND_VILLAGEPROTECTION01, true);
			StopBuffer(SOUND_ELBELAND_WATERFALLSMALL01, true);
			StopBuffer(SOUND_ELBELAND_WATERWAY01, true);
			StopBuffer(SOUND_ELBELAND_ENTERDEVIAS01, true);
			StopBuffer(SOUND_ELBELAND_WATERSMALL01, true);
			StopBuffer(SOUND_ELBELAND_RAVINE01, true);
			StopBuffer(SOUND_ELBELAND_ENTERATLANCE01, true);
		}

		if (!IsKarutanMap())
			StopBuffer(SOUND_KARUTAN_DESERT_ENV, true);
		if (World != WD_80KARUTAN1)
			StopBuffer(SOUND_KARUTAN_INSECT_ENV, true);
		if (World != WD_81KARUTAN2)
			StopBuffer(SOUND_KARUTAN_KARDAMAHAL_ENV, true);

		if (World == WD_0LORENCIA)
		{
			if (Hero->SafeZone)
			{
				if (HeroTile == 4)
					PlayMp3(g_lpszMp3[MUSIC_PUB]);
				else
					PlayMp3(g_lpszMp3[MUSIC_MAIN_THEME]);
			}
		}
		else
		{
			StopMp3(g_lpszMp3[MUSIC_PUB]);
			StopMp3(g_lpszMp3[MUSIC_MAIN_THEME]);
		}
		if (World == WD_2DEVIAS)
		{
			if (Hero->SafeZone)
			{
				if ((Hero->PositionX) >= 205 && (Hero->PositionX) <= 214 && (Hero->PositionY) >= 13 && (Hero->PositionY) <= 31)
				{
					PlayMp3(g_lpszMp3[MUSIC_CHURCH]);
				}
				else
				{
					PlayMp3(g_lpszMp3[MUSIC_DEVIAS]);
				}
			}
		}
		else
		{
			StopMp3(g_lpszMp3[MUSIC_CHURCH]);
			StopMp3(g_lpszMp3[MUSIC_DEVIAS]);
		}
		if (World == WD_3NORIA)
		{
			if (Hero->SafeZone)
			{
				PlayMp3(g_lpszMp3[MUSIC_NORIA]);
			}
		}
		else
		{
			StopMp3(g_lpszMp3[MUSIC_NORIA]);
		}
		if (World == WD_1DUNGEON || World == WD_5UNKNOWN)
		{
			PlayMp3(g_lpszMp3[MUSIC_DUNGEON]);
		}
		else
		{
			StopMp3(g_lpszMp3[MUSIC_DUNGEON]);
		}

		if (World == WD_7ATLANSE)
		{
			PlayMp3(g_lpszMp3[MUSIC_ATLANS]);
		}
		else
		{
			StopMp3(g_lpszMp3[MUSIC_ATLANS]);
		}
		if (World == WD_10HEAVEN)
		{
			PlayMp3(g_lpszMp3[MUSIC_ICARUS]);
		}
		else
		{
			StopMp3(g_lpszMp3[MUSIC_ICARUS]);
		}
		if (World == WD_8TARKAN)
		{
			PlayMp3(g_lpszMp3[MUSIC_TARKAN]);
		}
		else
		{
			StopMp3(g_lpszMp3[MUSIC_TARKAN]);
		}
		if (World == WD_4LOSTTOWER)
		{
			PlayMp3(g_lpszMp3[MUSIC_LOSTTOWER_A]);
		}
		else
		{
			StopMp3(g_lpszMp3[MUSIC_LOSTTOWER_A]);
		}

		if (gMapManager->InHellas())
		{
			PlayMp3(g_lpszMp3[MUSIC_KALIMA]);
		}
		else
		{
			StopMp3(g_lpszMp3[MUSIC_KALIMA]);
		}

		if (World == WD_31HUNTING_GROUND)
		{
			PlayMp3(g_lpszMp3[MUSIC_BC_HUNTINGGROUND]);
		}
		else
		{
			StopMp3(g_lpszMp3[MUSIC_BC_HUNTINGGROUND]);
		}

		if (World == WD_33AIDA)
		{
			PlayMp3(g_lpszMp3[MUSIC_BC_ADIA]);
		}
		else
		{
			StopMp3(g_lpszMp3[MUSIC_BC_ADIA]);
		}

		M34CryWolf1st::ChangeBackGroundMusic(World);
		M39Kanturu3rd::ChangeBackGroundMusic(World);

		if (World == WD_37KANTURU_1ST)
			PlayMp3(g_lpszMp3[MUSIC_KANTURU_1ST]);
		else
			StopMp3(g_lpszMp3[MUSIC_KANTURU_1ST]);

		M38Kanturu2nd::PlayBGM();
		SEASON3A::CGM3rdChangeUp::Instance().PlayBGM();
		if (gMapManager->IsCursedTemple())
		{
			g_CursedTemple->PlayBGM();
		}
		if (World == WD_51HOME_6TH_CHAR)
		{
			PlayMp3(g_lpszMp3[MUSIC_ELBELAND]);
		}
		else
		{
			StopMp3(g_lpszMp3[MUSIC_ELBELAND]);
		}

		if (World == WD_56MAP_SWAMP_OF_QUIET)
		{
			PlayMp3(g_lpszMp3[MUSIC_SWAMP_OF_QUIET]);
		}
		else
		{
			StopMp3(g_lpszMp3[MUSIC_SWAMP_OF_QUIET]);
		}

		g_Raklion.PlayBGM();
		g_SantaTown.PlayBGM();
		g_PKField.PlayBGM();
		g_DoppelGanger1.PlayBGM();
		g_EmpireGuardian1.PlayBGM();
		g_EmpireGuardian2.PlayBGM();
		g_EmpireGuardian3.PlayBGM();
		g_EmpireGuardian4.PlayBGM();
		g_UnitedMarketPlace.PlayBGM();
		g_Karutan1.PlayBGM();

		if (World > WD_81KARUTAN2)
		{
			gMapManager->MapIndexPlayMP3();
		}
	}

	TimeRemain = accumulatedTime;
}

void Scene(HDC hDC)
{
	g_Luminosity = sinf(WorldTime * 0.004f) * 0.15f + 0.6f;

	switch (SceneFlag)
	{
#ifdef MOVIE_DIRECTSHOW
	case MOVIE_SCENE:
		MovieScene(hDC);
		break;
#endif // MOVIE_DIRECTSHOW
	case WEBZEN_SCENE:
		WebzenScene(hDC);
		break;
	case LOADING_SCENE:
		LoadingScene(hDC);
		break;
	case LOG_IN_SCENE:
	case CHARACTER_SCENE:
	case MAIN_SCENE:
		MainScene(hDC);
		break;
	}
}

bool GetTimeCheck(int DelayTime)
{
	int PresentTime = timeGetTime();

	if (g_bTimeCheck)
	{
		g_iBackupTime = PresentTime;
		g_bTimeCheck = false;
	}

	if (g_iBackupTime + DelayTime <= PresentTime)
	{
		g_bTimeCheck = true;
		return true;
	}
	return false;
}
