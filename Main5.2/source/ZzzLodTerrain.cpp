///////////////////////////////////////////////////////////////////////////////
// Terrain °ü·Ã ÇÔ¼ö
///////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include <gl\gl.h>
#include <gl\glu.h>
#include <math.h>
#include <chrono>
#include "ZzzOpenglUtil.h"
#include "ZzzBMD.h"
#include "ZzzLodTerrain.h"
#include "zzzpath.h"
#include "ZzzTexture.h"
#include "ZzzInfomation.h"
#include "ZzzObject.h"
#include "ZzzCharacter.h"
#include "ZzzInterface.h"
#include "ZzzEffect.h"
#include "./Utilities/Log/ErrorReport.h"
#include "CSChaosCastle.h"
#include "GMBattleCastle.h"
#include "CMVP1stDirection.h"
#include "CDirection.h"
#include "MapManager.h"
#include "./Utilities/Log/muConsoleDebug.h"
#include "w_MapHeaders.h"
#include "NewUISystem.h"
#include "GMKarutan1.h"
#include "CameraManager.h"
#include "ZzzScene.h"

#ifdef PJH_NEW_SERVER_SELECT_MAP
#include "CameraMove.h"
#endif //PJH_NEW_SERVER_SELECT_MAP
#include "CGMProtect.h"
#include "MuCrypto/MuCrypto.h"
#include "LorenciaWaterMaterial.h"
#include "TarkanLavaMaterial.h"
#include "LorenciaWaterSSR.h"
#include "LorenciaRainPuddles.h"

extern float RealRenderFPS;

//-------------------------------------------------------------------------------------------------------------

int  TerrainFlag;
bool ActiveTerrain = false;
bool TerrainGrassEnable = true;
bool DetailLowEnable = false;

char            LodBuffer[64 * 64];
vec3_t          TerrainNormal[TERRAIN_SIZE * TERRAIN_SIZE];
vec3_t          PrimaryTerrainLight[TERRAIN_SIZE * TERRAIN_SIZE];
vec3_t          BackTerrainLight[TERRAIN_SIZE * TERRAIN_SIZE];
vec3_t          TerrainLight[TERRAIN_SIZE * TERRAIN_SIZE];
float           PrimaryTerrainHeight[TERRAIN_SIZE * TERRAIN_SIZE];
float           BackTerrainHeight[TERRAIN_SIZE * TERRAIN_SIZE];
unsigned char   TerrainMappingLayer1[TERRAIN_SIZE * TERRAIN_SIZE];
unsigned char   TerrainMappingLayer2[TERRAIN_SIZE * TERRAIN_SIZE];
float           TerrainMappingAlpha[TERRAIN_SIZE * TERRAIN_SIZE];
float           TerrainGrassTexture[TERRAIN_SIZE * TERRAIN_SIZE];
float           TerrainGrassWind[TERRAIN_SIZE * TERRAIN_SIZE];
float			g_fTerrainGrassWind1[TERRAIN_SIZE * TERRAIN_SIZE];

WORD            TerrainWall[TERRAIN_SIZE * TERRAIN_SIZE];

float           SelectXF;
float           SelectYF;
float           WaterMove;
int             CurrentLayer;

float           g_fSpecialHeight = 1200.f;

namespace
{
	// Map classification is constant throughout a frame. Cache it once before
	// the terrain pass instead of repeating the same manager/function chain for
	// every visible tile and grass face.
	int g_TerrainMapIndex = -1;
	bool g_TerrainCursedTemple = false;
	bool g_TerrainEmpireGuardian = false;
	bool g_TerrainKarutan = false;
	bool g_TerrainPKField = false;
	bool g_TerrainDoppel2 = false;
	bool g_TerrainDoppel3 = false;
	bool g_TerrainBloodCastle = false;
	bool g_TerrainChaosCastle = false;

	// The normal and grass passes use the same camera/frustum inside one
	// RenderTerrain() call. Keep the exact row-major tile result from the normal
	// pass so the grass pass does not repeat both levels of frustum testing.
	// No extra tile is rejected: an overflow falls back to the original scan.
	struct TerrainVisibleTile
	{
		short x;
		short y;
	};

	TerrainVisibleTile g_TerrainVisibleTiles[TERRAIN_SIZE * TERRAIN_SIZE];
	int g_TerrainVisibleTileCount = 0;
	bool g_TerrainCollectVisibleTiles = false;
	bool g_TerrainVisibleTileOverflow = false;

	// Keep the fixed-function client arrays enabled for the entire grass pass.
	// The old code enabled/disabled all three arrays once per tile, causing
	// thousands of redundant OpenGL state changes on maps with grass.
	bool g_TerrainGrassClientArraysActive = false;
	bool g_TerrainRenderEnabledCached = true;
}



bool  SelectFlag;

int    TerrainIndex1;
int    TerrainIndex2;
int    TerrainIndex3;
int    TerrainIndex4;
int    Index0;
int    Index1;
int    Index2;
int    Index3;
int    Index01;
int    Index12;
int    Index23;
int    Index30;
int    Index02;
vec3_t TerrainVertex[4];
vec3_t TerrainVertex01;
vec3_t TerrainVertex12;
vec3_t TerrainVertex23;
vec3_t TerrainVertex30;
vec3_t TerrainVertex02;
float  TerrainTextureCoord[4][2];
float  TerrainTextureCoord01[2];
float  TerrainTextureCoord12[2];
float  TerrainTextureCoord23[2];
float  TerrainTextureCoord30[2];
float  TerrainTextureCoord02[2];
float  TerrainMappingAlpha01;
float  TerrainMappingAlpha12;
float  TerrainMappingAlpha23;
float  TerrainMappingAlpha30;
float  TerrainMappingAlpha02;

#ifdef DYNAMIC_FRUSTRUM
FrustrumMap_t	g_FrustrumMap;
#endif //DYNAMIC_FRUSTRUM

const float g_fMinHeight = -500.f;
const float g_fMaxHeight = 1000.f;

extern  short   g_shCameraLevel;
extern  float CameraDistanceTarget;
extern  float CameraDistance;

static  float   g_fFrustumRange = -40.f;


inline int TERRAIN_INDEX(int x, int y)
{
	return (y)*TERRAIN_SIZE + (x);
}

inline int TERRAIN_INDEX_REPEAT(int x, int y)
{
	return (y & TERRAIN_SIZE_MASK) * TERRAIN_SIZE + (x & TERRAIN_SIZE_MASK);
}

inline WORD TERRAIN_ATTRIBUTE(float x, float y)
{
	int xf = (int)(x / TERRAIN_SCALE);
	int yf = (int)(y / TERRAIN_SCALE);
	return TerrainWall[(yf)*TERRAIN_SIZE + (xf)];
}

void InitTerrainMappingLayer()
{
	for (int i = 0; i < TERRAIN_SIZE * TERRAIN_SIZE; i++)
	{
		TerrainMappingLayer1[i] = 0;
		TerrainMappingLayer2[i] = 255;
		TerrainMappingAlpha[i] = 0.f;
		TerrainGrassTexture[i] = (float)((rand() % 4) / 4.f);
	}
}

void ExitProgram()
{
	gwinhandle->SendWindowMessage(GlobalText[11], false);
	gwinhandle->SendNowMessage(WM_DESTROY, 0, 0);
}

int OpenTerrainAttribute(char* FileName)
{
	FILE* fp = fopen(FileName, "rb");

	if (fp == NULL)
	{
		return (-1);
	}

	fseek(fp, 0, SEEK_END);

	unsigned int FileSize = ftell(fp);

	fseek(fp, 0, SEEK_SET);

	BYTE Enc[4];

	fread(&Enc, 4u, 1u, fp);

	std::vector<BYTE> FileBuffer;

	if (Enc[0] != 'A' || Enc[1] != 'T' || Enc[2] != 'T' || Enc[3] != 1)
	{
		BYTE* EncData = new BYTE[FileSize];

		fseek(fp, 0, SEEK_SET);

		fread(EncData, 1u, FileSize, fp);

		fclose(fp);

		FileBuffer = std::vector<BYTE>(FileSize, 0);

		MapFileDecrypt(FileBuffer.data(), EncData, FileSize);

		SAFE_DELETE_ARRAY(EncData);
	}
	else
	{
		FileSize -= 4;

		FileBuffer = std::vector<BYTE>(FileSize, 0);

		fread(FileBuffer.data(), 1, FileSize, fp);

		fclose(fp);

		MuFileDecrypt2(FileBuffer);
	}

	BYTE* blockchain = FileBuffer.data();
	FileSize = FileBuffer.size();

	if (FileSize != 131076 && FileSize != 65540)
	{
		return (-1);
	}

	bool extAtt = false;

	if (FileSize == 131076)
	{
		extAtt = true;
	}

	BYTE Version, Width, Height;

	BuxConvert(blockchain, FileSize);

	Version = blockchain[0];
	int iMap = blockchain[1];
	Width = blockchain[2];
	Height = blockchain[3];

	bool Error = false;

	if (Version || Width != 255 || Height != 255)
	{
		Error = true;
	}

	if (extAtt)
	{
		memcpy(TerrainWall, &blockchain[4], TERRAIN_SIZE * TERRAIN_SIZE * sizeof(WORD));
	}
	else
	{
		BYTE* TWall = new BYTE[TERRAIN_SIZE * TERRAIN_SIZE];
		memcpy(TWall, &blockchain[4], TERRAIN_SIZE * TERRAIN_SIZE);

		for (int i = 0; i < TERRAIN_SIZE * TERRAIN_SIZE; ++i)
		{
			TerrainWall[i] = TWall[i];
		}
		SAFE_DELETE_ARRAY(TWall);
	}

	switch (gMapManager->currentMap)
	{
	case WD_0LORENCIA:
		if (TerrainWall[123 * 256 + 135] != 5)
			Error = true;
		break;
	case WD_1DUNGEON:
		if (TerrainWall[120 * 256 + 227] != 4)
			Error = true;
		break;
	case WD_2DEVIAS:
		if (TerrainWall[55 * 256 + 208] != 5)
			Error = true;
		break;
	case WD_3NORIA:
		if (TerrainWall[119 * 256 + 186] != 5)
			Error = true;
		break;
	case WD_4LOSTTOWER:
		if (TerrainWall[75 * 256 + 193] != 5)
			Error = true;
		break;
	}

	for (int i = 0; i < TERRAIN_SIZE * TERRAIN_SIZE; i++)
	{
		TerrainWall[i] = TerrainWall[i] & 0xFF;
		if ((BYTE)TerrainWall[i] >= 128)
			Error = true;
	}

	if (Error)
	{
		ExitProgram();
		return (-1);
	}

	return iMap;
}

bool SaveTerrainAttribute(char* FileName, int iMap)
{
	FILE* fp = fopen(FileName, "wb");

	BYTE Version = 0;
	BYTE Width = 255;
	BYTE Height = 255;
	fwrite(&Version, 1, 1, fp);
	fwrite(&iMap, 1, 1, fp);
	fwrite(&Width, 1, 1, fp);
	fwrite(&Height, 1, 1, fp);
	fwrite(TerrainWall, TERRAIN_SIZE * TERRAIN_SIZE, 1, fp);

	fclose(fp);
	return true;
}

void AddTerrainAttribute(int x, int y, BYTE att)
{
	int iIndex = (x + (y * TERRAIN_SIZE));

	TerrainWall[iIndex] |= att;
}


void SubTerrainAttribute(int x, int y, BYTE att)
{
	int iIndex = (x + (y * TERRAIN_SIZE));

	TerrainWall[iIndex] ^= (TerrainWall[iIndex] & att);
}

void AddTerrainAttributeRange(int x, int y, int dx, int dy, BYTE att, BYTE Add)
{
	for (int j = 0; j < dy; ++j)
	{
		for (int i = 0; i < dx; ++i)
		{
			if (Add)
			{
				AddTerrainAttribute(x + i, y + j, att);
			}
			else
			{
				SubTerrainAttribute(x + i, y + j, att);
			}
		}
	}
}

void SetTerrainWaterState(std::list<int>& terrainIndex, int state)
{
	if (state == 0)
	{
		for (int i = 0; i < TERRAIN_SIZE * TERRAIN_SIZE; ++i)
		{
			if ((TerrainWall[i] & TW_WATER) == TW_WATER)
			{
				TerrainWall[i] = 0;
				terrainIndex.push_back(i);
			}
		}
	}
	else
	{
		for (std::list<int>::iterator iter = terrainIndex.begin(); iter != terrainIndex.end(); )
		{
			std::list<int>::iterator curiter = iter;
			++iter;
			int index = *curiter;
			TerrainWall[index] = TW_WATER;
		}
	}
}

int SaveCameraAngle(char* FileName, BYTE* pbySrc, int iSize)
{
	if (pbySrc != NULL && iSize != 0)
	{
		FILE* fp = fopen(FileName, "wb");

		if (fp != NULL)
		{
			fwrite(pbySrc, iSize, 1, fp);
			fclose(fp);
			return (1);
		}
	}

	return (0);
}

int Open_Camera_Angle_Position(char* WorldName)
{
	char FileName[64];

	sprintf(FileName, "Data\\%s\\Camera_Angle_Position.bmd", WorldName);


	FILE* fp = fopen(FileName, "rb");

	if (fp == NULL)
	{
		return (0);
	}

	fseek(fp, 0, SEEK_END);

	unsigned int FileSize = ftell(fp);

	fseek(fp, 0, SEEK_SET);

	BYTE Enc[4];

	fread(&Enc, 4u, 1u, fp);

	std::vector<BYTE> FileBuffer;

	if (Enc[0] == 'M' && Enc[1] == 'A' && Enc[2] == 'P' && Enc[3] == 1)
	{
		FileSize -= 4;

		FileBuffer = std::vector<BYTE>(FileSize, 0);

		fread(FileBuffer.data(), 1, FileSize, fp);

		fclose(fp);

		MuFileDecrypt2(FileBuffer);
	}
	else
	{
		BYTE* EncData = new BYTE[FileSize];

		fseek(fp, 0, SEEK_SET);

		fread(EncData, 1u, FileSize, fp);

		fclose(fp);

		FileBuffer = std::vector<BYTE>(FileSize, 0);

		MapFileDecrypt(FileBuffer.data(), EncData, FileSize);

		SAFE_DELETE_ARRAY(EncData);
	}

	sprintf(FileName, "Data\\%s\\CAP.ini", WorldName);

	return SaveCameraAngle(FileName, FileBuffer.data(), FileBuffer.size());
}

int OpenTerrainMapping(char* FileName)	//
{
	InitTerrainMappingLayer();


	FILE* fp = fopen(FileName, "rb");

	if (fp == NULL)
	{
		return (-1);
	}

	fseek(fp, 0, SEEK_END);

	unsigned int FileSize = ftell(fp);

	fseek(fp, 0, SEEK_SET);

	BYTE Enc[4];

	fread(&Enc, 4u, 1u, fp);

	std::vector<BYTE> FileBuffer;

	if (Enc[0] != 'M' || Enc[1] != 'A' || Enc[2] != 'P' || Enc[3] != 1)
	{
		BYTE* EncData = new BYTE[FileSize];

		fseek(fp, 0, SEEK_SET);

		fread(EncData, 1u, FileSize, fp);

		fclose(fp);

		FileBuffer = std::vector<BYTE>(FileSize, 0);

		MapFileDecrypt(FileBuffer.data(), EncData, FileSize);

		SAFE_DELETE_ARRAY(EncData);
	}
	else
	{
		FileSize -= 4;

		FileBuffer = std::vector<BYTE>(FileSize, 0);

		fread(FileBuffer.data(), 1, FileSize, fp);

		fclose(fp);

		MuFileDecrypt2(FileBuffer);
	}


	int DataPtr = 0;
	BYTE Version = FileBuffer[0];
	int iMapNumber = FileBuffer[1];

	DataPtr = 2;

	memcpy(TerrainMappingLayer1, &FileBuffer[DataPtr], 256 * 256);
	DataPtr += (TERRAIN_SIZE * TERRAIN_SIZE);
	memcpy(TerrainMappingLayer2, &FileBuffer[DataPtr], 256 * 256);
	DataPtr += (TERRAIN_SIZE * TERRAIN_SIZE);

	for (int i = 0; i < TERRAIN_SIZE * TERRAIN_SIZE; i++)
	{
		TerrainMappingAlpha[i] = ((float)FileBuffer[DataPtr++]) / 255.f;
	}

	TerrainGrassEnable = true;

	if (gMapManager->InChaosCastle() == true)
	{
		TerrainGrassEnable = false;
	}
	if (gMapManager->InBattleCastle())
	{
		TerrainGrassEnable = false;
	}

	return (iMapNumber);
}

bool SaveTerrainMapping(char* FileName, int iMapNumber)	//
{
	FILE* fp = fopen(FileName, "wb");

	BYTE Version = 0;
	fwrite(&Version, 1, 1, fp);
	fwrite(&iMapNumber, 1, 1, fp);	//
	fwrite(TerrainMappingLayer1, TERRAIN_SIZE * TERRAIN_SIZE, 1, fp);
	fwrite(TerrainMappingLayer2, TERRAIN_SIZE * TERRAIN_SIZE, 1, fp);
	for (int i = 0; i < TERRAIN_SIZE * TERRAIN_SIZE; i++)
	{
		unsigned char Alpha = (unsigned char)(TerrainMappingAlpha[i] * 255.f);
		fwrite(&Alpha, 1, 1, fp);
	}
	/*
#ifndef BATTLE_CASTLE
	for(i=0;i<MAX_GROUNDS;i++)
	{
		GROUND *o = &Grounds[i];
		if(o->Live)
		{
			fwrite(&o->Type ,2,1,fp);
			fwrite(&o->x    ,1,1,fp);
			fwrite(&o->y    ,1,1,fp);
			fwrite(&o->Angle,1,1,fp);
		}
	}
#endif// BATTLE_CASTLE
*/
	fclose(fp);

	{
		fp = fopen(FileName, "rb");
		if (fp == NULL)
		{
			return (false);
		}
		fseek(fp, 0, SEEK_END);
		int EncBytes = ftell(fp);	//
		fseek(fp, 0, SEEK_SET);
		unsigned char* EncData = new unsigned char[EncBytes];	//
		fread(EncData, 1, EncBytes, fp);//
		fclose(fp);

		int DataBytes = MapFileEncrypt(NULL, EncData, EncBytes);	//
		unsigned char* Data = new unsigned char[DataBytes];		//
		MapFileEncrypt(Data, EncData, EncBytes);	//
		delete[] EncData;		//

		fp = fopen(FileName, "wb");
		fwrite(Data, DataBytes, 1, fp);
		fclose(fp);
		delete[] Data;
	}
	return true;
}

/*
#ifndef BATTLE_CASTLE
	void CreateGround(int Type,int x,int y,float Angle)
	{
		for(int i=0;i<MAX_GROUNDS;i++)
		{
			GROUND *o = &Grounds[i];
			if(!o->Live)
			{
				o->Live  = true;
				o->Type  = Type;
				o->x     = x;
				o->y     = y;
				o->Angle = (unsigned char)(Angle/360.f*255.f);
				return;
			}
		}
	}

	void DeleteGround(int x,int y)
	{
		for(int i=0;i<MAX_GROUNDS;i++)
		{
			GROUND *o = &Grounds[i];
			if(o->Live)
			{
				if(o->x==x && o->y==y) o->Live = false;
			}
		}
	}

	void RenderGrounds()
	{
		for(int i=0;i<MAX_GROUNDS;i++)
		{
			GROUND *o = &Grounds[i];
			if(o->Live)
			{
				float Angle = (float)o->Angle/255.f*360.f;
				RenderTerrainBitmap(BITMAP_CURSOR+6,o->x,o->y,Angle);
			}
		}
	}
#endif// BATTLE_CASTLE
*/
void CreateTerrainNormal()
{
	for (int y = 0; y < TERRAIN_SIZE; y++)
	{
		for (int x = 0; x < TERRAIN_SIZE; x++)
		{
			int Index = TERRAIN_INDEX(x, y);
			vec3_t v1, v2, v3, v4;
			Vector((x)*TERRAIN_SCALE, (y)*TERRAIN_SCALE, BackTerrainHeight[TERRAIN_INDEX_REPEAT(x, y)], v4);
			Vector((x + 1) * TERRAIN_SCALE, (y)*TERRAIN_SCALE, BackTerrainHeight[TERRAIN_INDEX_REPEAT(x + 1, y)], v1);
			Vector((x + 1) * TERRAIN_SCALE, (y + 1) * TERRAIN_SCALE, BackTerrainHeight[TERRAIN_INDEX_REPEAT(x + 1, y + 1)], v2);
			Vector((x)*TERRAIN_SCALE, (y + 1) * TERRAIN_SCALE, BackTerrainHeight[TERRAIN_INDEX_REPEAT(x, y + 1)], v3);
			FaceNormalize(v1, v2, v3, TerrainNormal[Index]);
		}
	}
}

void CreateTerrainNormal_Part(int xi, int yi)
{
	if (xi > TERRAIN_SIZE - 4) xi = TERRAIN_SIZE - 4;
	else if (xi < 4)         xi = 4;

	if (yi > TERRAIN_SIZE - 4) yi = TERRAIN_SIZE - 4;
	else if (yi < 4)         yi = 4;

	for (int y = yi - 4; y < yi + 4; y++)
	{
		for (int x = xi - 4; x < xi + 4; x++)
		{
			int Index = TERRAIN_INDEX(x, y);
			vec3_t v1, v2, v3, v4;
			Vector((x)*TERRAIN_SCALE, (y)*TERRAIN_SCALE, BackTerrainHeight[TERRAIN_INDEX_REPEAT(x, y)], v4);
			Vector((x + 1) * TERRAIN_SCALE, (y)*TERRAIN_SCALE, BackTerrainHeight[TERRAIN_INDEX_REPEAT(x + 1, y)], v1);
			Vector((x + 1) * TERRAIN_SCALE, (y + 1) * TERRAIN_SCALE, BackTerrainHeight[TERRAIN_INDEX_REPEAT(x + 1, y + 1)], v2);
			Vector((x)*TERRAIN_SCALE, (y + 1) * TERRAIN_SCALE, BackTerrainHeight[TERRAIN_INDEX_REPEAT(x, y + 1)], v3);
			FaceNormalize(v1, v2, v3, TerrainNormal[Index]);
		}
	}
}

void CreateTerrainLight()
{
	vec3_t Light;
	if (gMapManager->InBattleCastle())
	{
		Vector(0.5f, -1.f, 1.f, Light);
	}
	else
	{
		Vector(0.5f, -0.5f, 0.5f, Light);
	}
	for (int y = 0; y < TERRAIN_SIZE; y++)
	{
		for (int x = 0; x < TERRAIN_SIZE; x++)
		{
			int Index = TERRAIN_INDEX(x, y);
			float Luminosity = DotProduct(TerrainNormal[Index], Light) + 0.5f;
			if (Luminosity < 0.f) Luminosity = 0.f;
			else if (Luminosity > 1.f) Luminosity = 1.f;
			for (int i = 0; i < 3; i++)
				BackTerrainLight[Index][i] = TerrainLight[Index][i] * Luminosity;
		}
	}
}

void CreateTerrainLight_Part(int xi, int yi)
{
	if (xi > TERRAIN_SIZE - 4) xi = TERRAIN_SIZE - 4;
	else if (xi < 4)         xi = 4;

	if (yi > TERRAIN_SIZE - 4) yi = TERRAIN_SIZE - 4;
	else if (yi < 4)         yi = 4;

	vec3_t Light;
	Vector(0.5f, -0.5f, 0.5f, Light);
	for (int y = yi - 4; y < yi + 4; y++)
	{
		for (int x = xi - 4; x < xi + 4; x++)
		{
			int Index = TERRAIN_INDEX(x, y);
			float Luminosity = DotProduct(TerrainNormal[Index], Light) + 0.5f;
			if (Luminosity < 0.f) Luminosity = 0.f;
			else if (Luminosity > 1.f) Luminosity = 1.f;
			for (int i = 0; i < 3; i++)
				BackTerrainLight[Index][i] = TerrainLight[Index][i] * Luminosity;
		}
	}
}

void pull_extension(std::string& fileName, const char* ext)
{
	size_t dotPosition = fileName.find_last_of('.');
	if (dotPosition != std::string::npos)
	{
		fileName.erase(dotPosition + 1);  // Elimina la extensión existente
		fileName += ext;  // Agrega la nueva extensión
	}
}

BOOL ExistOZJ(std::string filename)
{
	FILE* fp;
	filename = "Data\\" + filename;

	pull_extension(filename, "OZJ");
	if ((fp = fopen(filename.c_str(), "rb")) != NULL)
	{
		fclose(fp);
		return TRUE;
	}
	else
	{
		return !TRUE;
	}
}

bool OpenBMPBuffer(std::string filename, float* BufferFloat)
{
	FILE* fp;
	filename = "Data\\" + filename;

	pull_extension(filename, "OZB");

	if ((fp = fopen(filename.c_str(), "rb")) != NULL)
	{
		fseek(fp, 0, SEEK_END);
		int iBytes = ftell(fp);
		fseek(fp, 4, SEEK_SET);

		BITMAPFILEHEADER header;
		BITMAPINFOHEADER bmiHeader;
		fread(&header, 1, sizeof(BITMAPFILEHEADER), fp);
		fread(&bmiHeader, 1, sizeof(BITMAPINFOHEADER), fp);

		BYTE* pbyData = new BYTE[iBytes];
		fread(pbyData, 1, iBytes, fp);
		fclose(fp);

		int Index = 0;
		for (int i = 0; i < TERRAIN_SIZE * TERRAIN_SIZE; ++i)
		{
			if ((Index + 2) < iBytes)
			{
				BufferFloat[Index + 0] = (float)pbyData[Index + 2] / 255.f;
				BufferFloat[Index + 1] = (float)pbyData[Index + 1] / 255.f;
				BufferFloat[Index + 2] = (float)pbyData[Index + 0] / 255.f;
			}
			Index += 3;
		}

		delete[] pbyData;

		return true;
	}
	else
	{
		char Text[256];
		sprintf(Text, "%s - File not exist.", filename.c_str());
		MessageBox(gwinhandle->GethWnd(), Text, NULL, MB_OK);
		SendMessage(gwinhandle->GethWnd(), WM_DESTROY, 0, 0);
		return false;
	}
}

void OpenTerrainLight(char* FileName)
{
	if (ExistOZJ(FileName))
	{
		OpenJpegBuffer(FileName, &TerrainLight[0][0]);
	}
	else
	{
		OpenBMPBuffer(FileName, &TerrainLight[0][0]);
	}

	CreateTerrainNormal();
	CreateTerrainLight();
}

void SaveTerrainLight(char* FileName)
{
	unsigned char* Buffer = new unsigned char[TERRAIN_SIZE * TERRAIN_SIZE * 3];
	for (int i = 0; i < TERRAIN_SIZE * TERRAIN_SIZE; i++)
	{
		for (int j = 0; j < 3; j++)
		{
			float Light = TerrainLight[i][j] * 255.f;
			if (Light < 0.f)
				Light = 0.f;
			else if (Light > 255.f)
				Light = 255.f;
			Buffer[i * 3 + j] = (unsigned char)(Light);
		}
	}
	WriteJpeg(FileName, TERRAIN_SIZE, TERRAIN_SIZE, Buffer, 100);
	SAFE_DELETE_ARRAY(Buffer);
}

void CreateTerrain(char* FileName, bool bNew)
{
	ActiveTerrain = true;

	if (bNew)
	{
		OpenTerrainHeightNew(FileName);
	}
	else
	{
		OpenTerrainHeight(FileName);
	}

	CreateSun();
}

unsigned char BMPHeader[1080];

bool IsTerrainHeightExtMap(int iWorld)
{
	if (iWorld == WD_42CHANGEUP3RD_2ND
		|| gMapManager->IsPKField()
		|| iWorld == WD_66DOPPLEGANGER2
		|| IsUrukMountain(iWorld)
		|| IsNars(iWorld)
		|| IsFerea(iWorld)
		|| IsSwampOfDarkness(iWorld)
		|| IsKuberaMine()
		|| IsAtlansAbbyss()
		|| IsScorchedCanyon()
		|| IsIcarusRed()
		|| IsTempleOfArnil(iWorld)
		|| IsOldKethotum(iWorld)
		|| IsBlazeKethotum(iWorld)
		|| IsKanturuUnderGround(iWorld)
		)
	{
		return true;
	}

	if (iWorld > WD_81KARUTAN2)
	{
		CUSTOM_MAPE_INFO* world = gMapManager->FindMap(iWorld);

		if (world != NULL && world->TerrainExt)
		{
			return true;
		}
	}

	return false;
}

bool OpenTerrainHeight(char* filename)
{
	char FileName[256];

	char NewFileName[256];

	memset(NewFileName, 0, sizeof(NewFileName));
	for (int i = 0; i < (int)strlen(filename); i++)
	{
		NewFileName[i] = filename[i];
		NewFileName[i + 1] = NULL;
		if (filename[i] == '.')
			break;
	}
	NewFileName[255] = 0;

	strcpy(FileName, "Data\\");
	strcat(FileName, NewFileName);
	strcat(FileName, "OZB");

	FILE* fp = fopen(FileName, "rb");

	if (fp == NULL)
	{
		char Text[256];
		sprintf(Text, "%s file not found.", FileName);
		g_ErrorReport.Write(Text);
		g_ErrorReport.Write("\r\n");
		MessageBox(gwinhandle->GethWnd(), Text, NULL, MB_OK);
		SendMessage(gwinhandle->GethWnd(), WM_DESTROY, 0, 0);
		return false;
	}
	fseek(fp, 4, SEEK_SET);
	int Index = 1080;
	int Size = 256 * 256 + Index;
	unsigned char* Buffer = new unsigned char[Size];
	fread(Buffer, 1, Size, fp);
	fclose(fp);

	memcpy(BMPHeader, Buffer, Index);

	for (int i = 0; i < 256; i++)
	{
		unsigned char* src = &Buffer[Index + i * 256];
		float* dst = &BackTerrainHeight[i * 256];
		for (int j = 0; j < 256; j++)
		{
			if (gMapManager->currentMap == WD_55LOGINSCENE)
				*dst = (float)(*src) * 3.0f;
			else
				*dst = (float)(*src) * 1.5f;

			src++;
			dst++;
		}
	}
	SAFE_DELETE_ARRAY(Buffer);
	return true;
}

void SaveTerrainHeight(char* name)
{
	unsigned char* Buffer = new unsigned char[256 * 256];
	for (int i = 0; i < 256; i++)
	{
		float* src = &BackTerrainHeight[i * 256];
		unsigned char* dst = &Buffer[(255 - i) * 256];
		for (int j = 0; j < 256; j++)
		{
			float Height;

			if (gMapManager->currentMap == WD_55LOGINSCENE)
				Height = *src / 3.0f;
			else
				Height = *src / 1.5f;

			if (Height < 0.f)
				Height = 0.f;
			else if (Height > 255.f)
				Height = 255.f;

			*dst = (unsigned char)(Height);
			src++;
			dst++;
		}
	}

	FILE* fp = fopen(name, "wb");
	fwrite(BMPHeader, 1080, 1, fp);

	for (int i = 0; i < 256; i++)
		fwrite(Buffer + (255 - i) * 256, 256, 1, fp);

	SAFE_DELETE_ARRAY(Buffer);
	fclose(fp);
}

bool OpenTerrainHeightNew(const char* strFilename)
{
	char FileName[256];
	char NewFileName[256] = { '\0', };

	for (int i = 0; i < (int)strlen(strFilename); i++)
	{
		NewFileName[i] = strFilename[i];
		NewFileName[i + 1] = '\0';
		if (strFilename[i] == '.')
			break;
	}

	strcpy(FileName, "Data\\");
	strcat(FileName, NewFileName);
	strcat(FileName, "OZB");

	FILE* fp = fopen(FileName, "rb");
	fseek(fp, 0, SEEK_END);
	int iBytes = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	BYTE* pbyData = new BYTE[iBytes];
	fread(pbyData, 1, iBytes, fp);
	fclose(fp);

	DWORD dwCurPos = 0;
	dwCurPos += 4;

	BITMAPFILEHEADER header;
	memcpy(&header, &pbyData[dwCurPos], sizeof(BITMAPFILEHEADER));
	dwCurPos += sizeof(BITMAPFILEHEADER);

	BITMAPINFOHEADER bmiHeader;
	memcpy(&bmiHeader, &pbyData[dwCurPos], sizeof(BITMAPINFOHEADER));
	dwCurPos += sizeof(BITMAPINFOHEADER);

	for (int i = 0; i < TERRAIN_SIZE * TERRAIN_SIZE; ++i)
	{
		BYTE* pbysrc = &pbyData[dwCurPos + i * 3];
		DWORD dwHeight = 0;
		BYTE* pbyHeight = (BYTE*)&dwHeight;

		pbyHeight[0] = pbysrc[2];
		pbyHeight[1] = pbysrc[1];
		pbyHeight[2] = pbysrc[0];

		BackTerrainHeight[i] = (float)dwHeight;
		BackTerrainHeight[i] += g_fMinHeight;
	}

	delete[] pbyData;

	return true;
}

extern int SceneFlag;

float RequestTerrainHeight(float xf, float yf)
{
	if ((gmProtect->SceneLogin == 1 && SceneFlag == LOG_IN_SCENE) || (gmProtect->SceneCharacter == 1 && SceneFlag == CHARACTER_SCENE))
	{
		return 0.f;
	}

	if (SceneFlag == SERVER_LIST_SCENE || SceneFlag == WEBZEN_SCENE || SceneFlag == LOADING_SCENE)
		return 0.f;

	if (xf < 0.f || yf < 0.f)
		return 0.f;

	xf = xf / TERRAIN_SCALE;
	yf = yf / TERRAIN_SCALE;

	unsigned int Index = TERRAIN_INDEX(xf, yf);

	if (Index >= TERRAIN_SIZE * TERRAIN_SIZE)
		return g_fSpecialHeight;

	if ((TerrainWall[Index] & TW_HEIGHT) == TW_HEIGHT)
	{
		return g_fSpecialHeight;
	}

	int xi = (int)xf;
	int yi = (int)yf;
	float xd = xf - (float)xi;
	float yd = yf - (float)yi;
	unsigned int Index1 = TERRAIN_INDEX_REPEAT(xi, yi);
	unsigned int Index2 = TERRAIN_INDEX_REPEAT(xi, yi + 1);
	unsigned int Index3 = TERRAIN_INDEX_REPEAT(xi + 1, yi);
	unsigned int Index4 = TERRAIN_INDEX_REPEAT(xi + 1, yi + 1);

	if (Index1 >= TERRAIN_SIZE * TERRAIN_SIZE || Index2 >= TERRAIN_SIZE * TERRAIN_SIZE ||
		Index3 >= TERRAIN_SIZE * TERRAIN_SIZE || Index4 >= TERRAIN_SIZE * TERRAIN_SIZE)
		return g_fSpecialHeight;

	float left = BackTerrainHeight[Index1] + (BackTerrainHeight[Index2] - BackTerrainHeight[Index1]) * yd;
	float right = BackTerrainHeight[Index3] + (BackTerrainHeight[Index4] - BackTerrainHeight[Index3]) * yd;

	return left + (right - left) * xd;
}

void RequestTerrainNormal(float xf, float yf, vec3_t Normal)
{
	xf = xf / TERRAIN_SCALE;
	yf = yf / TERRAIN_SCALE;
	int xi = (int)xf;
	int yi = (int)yf;
	VectorCopy(TerrainNormal[TERRAIN_INDEX_REPEAT(xi, yi)], Normal);
}

void AddTerrainHeight(float xf, float yf, float Height, int Range, float* Buffer)
{
	float rf = (float)Range;

	xf = xf / TERRAIN_SCALE;
	yf = yf / TERRAIN_SCALE;
	int   xi = (int)xf;
	int   yi = (int)yf;
	int   syi = yi - Range;
	int   eyi = yi + Range;
	float syf = (float)(syi);
	for (; syi <= eyi; syi++, syf += 1.f)
	{
		int   sxi = xi - Range;
		int   exi = xi + Range;
		float sxf = (float)(sxi);
		for (; sxi <= exi; sxi++, sxf += 1.f)
		{
			float xd = xf - sxf;
			float yd = yf - syf;
			float lf = (rf - sqrtf(xd * xd + yd * yd)) / rf;
			if (lf > 0.f)
			{
				Buffer[TERRAIN_INDEX_REPEAT(sxi, syi)] += Height * lf;
			}
		}
	}
}

void SetTerrainLight(float xf, float yf, vec3_t Light, int Range, vec3_t* Buffer)
{
	float rf = (float)Range;

	xf = (xf / TERRAIN_SCALE);
	yf = (yf / TERRAIN_SCALE);
	int   xi = (int)xf;
	int   yi = (int)yf;
	int   syi = yi - Range;
	int   eyi = yi + Range;
	float syf = (float)(syi);
	for (; syi <= eyi; syi++, syf += 1.f)
	{
		int   sxi = xi - Range;
		int   exi = xi + Range;
		float sxf = (float)(sxi);
		for (; sxi <= exi; sxi++, sxf += 1.f)
		{
			float xd = xf - sxf;
			float yd = yf - syf;
			float lf = (rf - sqrtf(xd * xd + yd * yd)) / rf;
			if (lf > 0.f)
			{
				float* b = &Buffer[TERRAIN_INDEX_REPEAT(sxi, syi)][0];
				for (int i = 0; i < 3; i++)
				{
					b[i] += Light[i] * lf;
				}
			}
		}
	}
}

void AddTerrainLight(float xf, float yf, vec3_t Light, int Range, vec3_t* Buffer)
{
	float rf = (float)Range;

	xf = (xf / TERRAIN_SCALE);
	yf = (yf / TERRAIN_SCALE);
	int   xi = (int)xf;
	int   yi = (int)yf;
	int   syi = yi - Range;
	int   eyi = yi + Range;
	float syf = (float)(syi);

	for (; syi <= eyi; syi++, syf += 1.f)
	{
		int   sxi = xi - Range;
		int   exi = xi + Range;
		float sxf = (float)(sxi);
		for (; sxi <= exi; sxi++, sxf += 1.f)
		{
			float xd = xf - sxf;
			float yd = yf - syf;
			float lf = (rf - sqrtf(xd * xd + yd * yd)) / rf;
			if (lf > 0.f)
			{
				float* b = &Buffer[TERRAIN_INDEX_REPEAT(sxi, syi)][0];
				for (int i = 0; i < 3; i++)
				{
					b[i] += Light[i] * lf;
					if (b[i] < 0.f)
						b[i] = 0.f;
				}
			}
		}
	}
}

void AddTerrainLightClip(float xf, float yf, vec3_t Light, int Range, vec3_t* Buffer)
{
	float rf = (float)Range;

	xf = (xf / TERRAIN_SCALE);
	yf = (yf / TERRAIN_SCALE);
	int   xi = (int)xf;
	int   yi = (int)yf;
	int   syi = yi - Range;
	int   eyi = yi + Range;
	float syf = (float)(syi);
	for (; syi <= eyi; syi++, syf += 1.f)
	{
		int   sxi = xi - Range;
		int   exi = xi + Range;
		float sxf = (float)(sxi);
		for (; sxi <= exi; sxi++, sxf += 1.f)
		{
			float xd = xf - sxf;
			float yd = yf - syf;
			float lf = (rf - sqrtf(xd * xd + yd * yd)) / rf;
			if (lf > 0.f)
			{
				float* b = &Buffer[TERRAIN_INDEX_REPEAT(sxi, syi)][0];
				for (int i = 0; i < 3; i++)
				{
					b[i] += Light[i] * lf;
					if (b[i] < 0.f) b[i] = 0.f;
					else if (b[i] > 1.f) b[i] = 1.f;
				}
			}
		}
	}
}

void RequestTerrainLight(float xf, float yf, vec3_t Light)
{
	if ((gmProtect->SceneLogin == 1 && SceneFlag == LOG_IN_SCENE) || (gmProtect->SceneCharacter == 1 && SceneFlag == CHARACTER_SCENE))
	{
		Vector(0.f, 0.f, 0.f, Light);
		return;
	}

	if (SceneFlag == SERVER_LIST_SCENE
		|| SceneFlag == WEBZEN_SCENE
		|| SceneFlag == LOADING_SCENE
		|| ActiveTerrain == false)
	{
		Vector(0.f, 0.f, 0.f, Light);
		return;
	}

	xf = xf / TERRAIN_SCALE;
	yf = yf / TERRAIN_SCALE;
	int xi = (int)xf;
	int yi = (int)yf;
	if (xi<0 || yi<0 || xi>TERRAIN_SIZE_MASK - 1 || yi>TERRAIN_SIZE_MASK - 1)
	{
		Vector(0.f, 0.f, 0.f, Light);
		return;
	}
	int Index1 = ((xi)+(yi)*TERRAIN_SIZE);
	int Index2 = ((xi + 1) + (yi)*TERRAIN_SIZE);
	int Index3 = ((xi + 1) + (yi + 1) * TERRAIN_SIZE);
	int Index4 = ((xi)+(yi + 1) * TERRAIN_SIZE);
	float xd = xf - (float)xi;
	float yd = yf - (float)yi;
	for (int i = 0; i < 3; i++)
	{
		float left = PrimaryTerrainLight[Index1][i] + (PrimaryTerrainLight[Index4][i] - PrimaryTerrainLight[Index1][i]) * yd;
		float right = PrimaryTerrainLight[Index2][i] + (PrimaryTerrainLight[Index3][i] - PrimaryTerrainLight[Index2][i]) * yd;
		Light[i] = (left + (right - left) * xd);
	}
}

void CreateLodBuffer()
{
	for (int y = 0; y < TERRAIN_SIZE; y += 4)
	{
		for (int x = 0; x < TERRAIN_SIZE; x += 4)
		{
			vec3_t NormalMin, NormalMax;
			NormalMin[0] = 1.f;
			NormalMin[1] = 1.f;
			NormalMin[2] = 1.f;
			NormalMax[0] = -1.f;
			NormalMax[1] = -1.f;
			NormalMax[2] = -1.f;
			for (int i = 0; i < 4; i++)
			{
				for (int j = 0; j < 4; j++)
				{
					vec3_t v1, v2, v3, v4;
					Vector((x + j) * TERRAIN_SCALE, (y + i) * TERRAIN_SCALE, BackTerrainHeight[TERRAIN_INDEX_REPEAT(x + j, y + i)], v1);
					Vector((x + j + 1) * TERRAIN_SCALE, (y + i) * TERRAIN_SCALE, BackTerrainHeight[TERRAIN_INDEX_REPEAT(x + j + 1, y + i)], v2);
					Vector((x + j + 1) * TERRAIN_SCALE, (y + i + 1) * TERRAIN_SCALE, BackTerrainHeight[TERRAIN_INDEX_REPEAT(x + j + 1, y + i + 1)], v3);
					Vector((x + j) * TERRAIN_SCALE, (y + i + 1) * TERRAIN_SCALE, BackTerrainHeight[TERRAIN_INDEX_REPEAT(x + j, y + i + 1)], v4);

					int Index = TERRAIN_INDEX(x + j, y + i);
					FaceNormalize(v1, v2, v3, TerrainNormal[Index]);

					vec3_t* Normal = &TerrainNormal[Index];
					if (NormalMin[0] > (*Normal)[0]) NormalMin[0] = (*Normal)[0];
					if (NormalMax[0] < (*Normal)[0]) NormalMax[0] = (*Normal)[0];
					if (NormalMin[1] > (*Normal)[1]) NormalMin[1] = (*Normal)[1];
					if (NormalMax[1] < (*Normal)[1]) NormalMax[1] = (*Normal)[1];
					if (NormalMin[2] > (*Normal)[2]) NormalMin[2] = (*Normal)[2];
					if (NormalMax[2] < (*Normal)[2]) NormalMax[2] = (*Normal)[2];
				}
			}
			float Delta = maxf(maxf(absf(NormalMax[0] - NormalMin[0]), absf(NormalMax[1] - NormalMin[1])), absf(NormalMax[2] - NormalMin[2]));
			if (DetailLowEnable == true)
			{
				LodBuffer[y / 4 * 64 + x / 4] = 4;
			}
			else
			{
				if (Delta >= 1.f) LodBuffer[y / 4 * 64 + x / 4] = 1;
				else if (Delta >= 0.5f) LodBuffer[y / 4 * 64 + x / 4] = 2;
				else                   LodBuffer[y / 4 * 64 + x / 4] = 4;
				LodBuffer[y / 4 * 64 + x / 4] = 1;
			}
		}
	}
}

void InterpolationHeight(int lod, int x, int y, int xd, float* TerrainHeight)
{
	if (lod >= 4)
	{
		TerrainHeight[TERRAIN_INDEX_REPEAT(x + xd, y + 2)] = (
			TerrainHeight[TERRAIN_INDEX_REPEAT(x + xd, y)] +
			TerrainHeight[TERRAIN_INDEX_REPEAT(x + xd, y + 4)]) * 0.5f;
	}
	if (lod >= 2)
	{
		TerrainHeight[TERRAIN_INDEX_REPEAT(x + xd, y + 1)] = (
			TerrainHeight[TERRAIN_INDEX_REPEAT(x + xd, y)] +
			TerrainHeight[TERRAIN_INDEX_REPEAT(x + xd, y + 2)]) * 0.5f;
		TerrainHeight[TERRAIN_INDEX_REPEAT(x + xd, y + 3)] = (
			TerrainHeight[TERRAIN_INDEX_REPEAT(x + xd, y + 2)] +
			TerrainHeight[TERRAIN_INDEX_REPEAT(x + xd, y + 4)]) * 0.5f;
	}
}

void InterpolationWidth(int lod, int x, int y, int yd, float* TerrainHeight)
{
	if (lod >= 4)
	{
		TerrainHeight[TERRAIN_INDEX_REPEAT(x + 2, y + yd)] = (
			TerrainHeight[TERRAIN_INDEX_REPEAT(x, y + yd)] +
			TerrainHeight[TERRAIN_INDEX_REPEAT(x + 4, y + yd)]) * 0.5f;
	}
	if (lod >= 2)
	{
		TerrainHeight[TERRAIN_INDEX_REPEAT(x + 1, y + yd)] = (
			TerrainHeight[TERRAIN_INDEX_REPEAT(x, y + yd)] +
			TerrainHeight[TERRAIN_INDEX_REPEAT(x + 2, y + yd)]) * 0.5f;
		TerrainHeight[TERRAIN_INDEX_REPEAT(x + 3, y + yd)] = (
			TerrainHeight[TERRAIN_INDEX_REPEAT(x + 2, y + yd)] +
			TerrainHeight[TERRAIN_INDEX_REPEAT(x + 4, y + yd)]) * 0.5f;
	}
}

void InterpolationCross(int lod, int x, int y)
{
	BackTerrainHeight[TERRAIN_INDEX_REPEAT(x, y)] = (
		BackTerrainHeight[TERRAIN_INDEX_REPEAT(x - lod, y)] +
		BackTerrainHeight[TERRAIN_INDEX_REPEAT(x + lod, y)] +
		BackTerrainHeight[TERRAIN_INDEX_REPEAT(x, y - lod)] +
		BackTerrainHeight[TERRAIN_INDEX_REPEAT(x, y + lod)]) * 0.25f;
}

void PrefixTerrainHeightEdge(int x, int y, int lod, float* TerrainHeight)
{
	if (lod >= LodBuffer[((y) & 63) * 64 + ((x - 1) & 63)])
		InterpolationHeight(lod, x * 4, y * 4, 0, TerrainHeight);

	if (lod >= LodBuffer[((y) & 63) * 64 + ((x + 1) & 63)])
		InterpolationHeight(lod, x * 4, y * 4, 4, TerrainHeight);

	if (lod >= LodBuffer[((y - 1) & 63) * 64 + ((x) & 63)])
		InterpolationWidth(lod, x * 4, y * 4, 0, TerrainHeight);

	if (lod >= LodBuffer[((y + 1) & 63) * 64 + ((x) & 63)])
		InterpolationWidth(lod, x * 4, y * 4, 4, TerrainHeight);
}

void PrefixTerrainHeight()
{
	for (int y = 0; y < 64; y++)
	{
		for (int x = 0; x < 64; x++)
		{
			int lod = LodBuffer[((y) & 63) * 64 + ((x) & 63)];
			PrefixTerrainHeightEdge(x, y, lod, BackTerrainHeight);
			if (lod >= 2)
			{
				if (lod >= 4)
				{
					InterpolationHeight(lod, x * 4, y * 4, 2, BackTerrainHeight);
					InterpolationWidth(lod, x * 4, y * 4, 2, BackTerrainHeight);
				}
				if (lod >= 2)
				{
					InterpolationHeight(lod, x * 4, y * 4, 1, BackTerrainHeight);
					InterpolationWidth(lod, x * 4, y * 4, 1, BackTerrainHeight);
					InterpolationHeight(lod, x * 4, y * 4, 3, BackTerrainHeight);
					InterpolationWidth(lod, x * 4, y * 4, 3, BackTerrainHeight);
				}
			}
		}
	}
}

inline void Interpolation(int mx, int my)
{
	Index01 = (my * 2) * 512 + (mx * 2 + 1);
	Index12 = (my * 2 + 1) * 512 + (mx * 2 + 2);
	Index23 = (my * 2 + 2) * 512 + (mx * 2 + 1);
	Index30 = (my * 2 + 1) * 512 + (mx * 2);
	Index02 = (my * 2 + 1) * 512 + (mx * 2 + 1);

	for (int i = 0; i < 3; i++)
	{
		TerrainVertex01[i] = (TerrainVertex[0][i] + TerrainVertex[1][i]) * 0.5f;
		TerrainVertex12[i] = (TerrainVertex[1][i] + TerrainVertex[2][i]) * 0.5f;
		TerrainVertex23[i] = (TerrainVertex[2][i] + TerrainVertex[3][i]) * 0.5f;
		TerrainVertex30[i] = (TerrainVertex[3][i] + TerrainVertex[0][i]) * 0.5f;
		TerrainVertex02[i] = (TerrainVertex[0][i] + TerrainVertex[2][i]) * 0.5f;
	}

	for (int i = 0; i < 2; i++)
	{
		TerrainTextureCoord01[i] = (TerrainTextureCoord[0][i] + TerrainTextureCoord[1][i]) * 0.5f;
		TerrainTextureCoord12[i] = (TerrainTextureCoord[1][i] + TerrainTextureCoord[2][i]) * 0.5f;
		TerrainTextureCoord23[i] = (TerrainTextureCoord[2][i] + TerrainTextureCoord[3][i]) * 0.5f;
		TerrainTextureCoord30[i] = (TerrainTextureCoord[3][i] + TerrainTextureCoord[0][i]) * 0.5f;
		TerrainTextureCoord02[i] = (TerrainTextureCoord[0][i] + TerrainTextureCoord[2][i]) * 0.5f;
	}
}

namespace
{
	bool g_TerrainWorldLighting = false;
	bool g_LorenciaLookEnabled = false;
	bool g_TerrainWorldWetness = false;
	bool g_TerrainSceneNightOnly = true;
	float g_TerrainWorldStrength = 0.35f;
	float g_TerrainNightBrightness = 0.78f;
	float g_TerrainNightFactor = 1.0f;
	float g_TerrainWorldRain = 0.0f;
	float g_TerrainWorldStorm = 0.0f;
	bool g_TerrainWaterDetail = true;
	bool g_LorenciaWaterTrial = false;
	bool g_LorenciaWaterActive = false;
	bool g_LorenciaWaterMaterial = false;
	bool g_LorenciaWaterReflections = false;
	bool g_LorenciaRainPuddles = false;
	bool g_TerrainWaterAtmosphere = true;
	float g_TerrainWaterStrength = 0.35f;
	float g_TerrainWaterHour = 12.0f;
	vec3_t g_TerrainWorldSun = {0.0f, 0.0f, 1.0f};
	vec3_t g_TerrainWorldColorCache[TERRAIN_SIZE * TERRAIN_SIZE];
	unsigned short g_TerrainWorldColorStamp[TERRAIN_SIZE * TERRAIN_SIZE] = {};
	unsigned short g_TerrainWorldColorGeneration = 1;

	float g_TerrainWetAmount = 0.f;
	float g_TerrainWaterSky[3] = {};
	float g_TerrainWaterMix = 0.f;
	float g_TerrainWaterRainDark = 1.f;
	float g_TerrainNightScale[3] = {1.f, 1.f, 1.f};

	void UpdateTerrainWorldVisual()
	{
		static DWORD lastRead = 0;
		const DWORD now = GetTickCount();
		if (lastRead == 0 || now - lastRead >= 1000)
		{
			g_LorenciaLookEnabled = GetPrivateProfileIntA("VisualTrial", "LorenciaLook", 0, ".\\config.ini") != 0;
			g_TerrainWorldLighting = GetPrivateProfileIntA("WorldLighting", "TerrainGeometry", 1, ".\\config.ini") != 0;
			g_TerrainWorldWetness = GetPrivateProfileIntA("WorldLighting", "TerrainWetness", 1, ".\\config.ini") != 0;
			g_TerrainSceneNightOnly = GetPrivateProfileIntA("WorldLighting", "SceneNightOnly", 1, ".\\config.ini") != 0;
			int nightBrightness = GetPrivateProfileIntA("WorldLighting", "LegacyNightBrightness", 78, ".\\config.ini");
			if (nightBrightness < 60) nightBrightness = 60;
			if (nightBrightness > 100) nightBrightness = 100;
			g_TerrainNightBrightness = nightBrightness * 0.01f;
			g_TerrainWaterDetail = GetPrivateProfileIntA("WaterVisual", "WaveDetail", 1, ".\\config.ini") != 0;
			g_LorenciaWaterTrial = GetPrivateProfileIntA("WaterVisual", "LorenciaTrial", 0, ".\\config.ini") != 0;
			g_LorenciaWaterMaterial = GetPrivateProfileIntA("WaterVisual", "LorenciaMaterial", 0, ".\\config.ini") != 0;
			g_LorenciaWaterReflections = GetPrivateProfileIntA("WaterVisual", "LorenciaReflections", 0, ".\\config.ini") != 0;
			g_LorenciaRainPuddles = GetPrivateProfileIntA("WaterVisual", "LorenciaRainPuddlesV2", 0, ".\\config.ini") != 0;
			g_TerrainWaterAtmosphere = GetPrivateProfileIntA("WaterVisual", "Atmosphere", 1, ".\\config.ini") != 0;
			int waterStrength = GetPrivateProfileIntA("WaterVisual", "WaveStrength", 35, ".\\config.ini");
			if (waterStrength < 0) waterStrength = 0;
			if (waterStrength > 100) waterStrength = 100;
			g_TerrainWaterStrength = waterStrength * 0.01f;
			int strength = GetPrivateProfileIntA("WorldLighting", "TerrainStrength", 35, ".\\config.ini");
			if (strength < 0) strength = 0;
			if (strength > 100) strength = 100;
			g_TerrainWorldStrength = strength * 0.01f;
			lastRead = now;
		}
		float dust, snow, fog;
		GetServerAtmosphereVisual(g_TerrainWaterHour, g_TerrainWorldRain, g_TerrainWorldStorm, dust, snow, fog);
		const float daylight = (cosf((g_TerrainWaterHour - 12.0f) * 3.14159265f / 12.0f) + 1.0f) * 0.5f;
		g_LorenciaWaterActive = g_LorenciaWaterTrial && SceneFlag == MAIN_SCENE && gMapManager->currentMap == WD_0LORENCIA;
		const int world = gMapManager->currentMap;
		const bool outdoor = world == WD_0LORENCIA || world == WD_2DEVIAS ||
			world == WD_3NORIA || world == WD_8TARKAN || world == WD_33AIDA ||
			world == WD_34CRYWOLF_1ST || world == WD_35CRYWOLF_2ND;
		g_TerrainNightFactor = (g_TerrainSceneNightOnly && outdoor) ?
			(g_TerrainNightBrightness + (1.0f - g_TerrainNightBrightness) * daylight) : 1.0f;
		GetServerVisualSunDirection(g_TerrainWorldSun[0], g_TerrainWorldSun[1], g_TerrainWorldSun[2]);
		// The shadow vector points from the sun toward the ground.
		g_TerrainWorldSun[0] = -g_TerrainWorldSun[0];
		g_TerrainWorldSun[1] = -g_TerrainWorldSun[1];
		g_TerrainWorldSun[2] = -g_TerrainWorldSun[2];
		// These values are identical for every terrain vertex in this pass.
		g_TerrainWetAmount = fminf(1.0f, g_TerrainWorldRain * 0.65f + g_TerrainWorldStorm * 0.35f);
		const float waterDaylight = fmaxf(0.0f, 1.0f - fabsf(g_TerrainWaterHour - 12.0f) / 12.0f);
		g_TerrainWaterSky[0] = 0.08f + 0.42f * waterDaylight;
		g_TerrainWaterSky[1] = 0.14f + 0.52f * waterDaylight;
		g_TerrainWaterSky[2] = 0.27f + 0.60f * waterDaylight;
		g_TerrainWaterMix = 0.10f + g_TerrainWorldRain * 0.05f;
		g_TerrainWaterRainDark = 1.0f - g_TerrainWorldRain * 0.07f;
		const float night = 1.0f - g_TerrainNightFactor;
		g_TerrainNightScale[0] = g_TerrainNightFactor * (1.0f - night * 0.15f);
		g_TerrainNightScale[1] = g_TerrainNightFactor * (1.0f - night * 0.07f);
		g_TerrainNightScale[2] = g_TerrainNightFactor;
		// Fold a restrained earth-tone grade into the existing lighting multiply.
		// No extra geometry, textures, render passes, or per-vertex branch.
		if (g_LorenciaLookEnabled && world == WD_0LORENCIA && g_TerrainSceneNightOnly)
		{
			g_TerrainNightScale[0] *= 0.96f;
			g_TerrainNightScale[1] *= 0.93f;
			g_TerrainNightScale[2] *= 0.90f;
		}
	}

	// Adjacent water tiles share wave coordinates. Keep the exact wave formula,
	// but evaluate each row/column only once for the current time and strength.
	struct TerrainWaveSample
	{
		bool valid;
		float coordinate, time, strength, first, second;
	};
	TerrainWaveSample g_TerrainWaveRows[TERRAIN_SIZE] = {};
	TerrainWaveSample g_TerrainWaveColumns[TERRAIN_SIZE] = {};

	inline void GetTerrainWave(float coordinate, bool row, float time, float strength,
		float& first, float& second)
	{
		TerrainWaveSample& sample = (row ? g_TerrainWaveRows : g_TerrainWaveColumns)
			[static_cast<int>(coordinate) & TERRAIN_SIZE_MASK];
		if (!sample.valid || sample.coordinate != coordinate || sample.time != time || sample.strength != strength)
		{
			sample.coordinate = coordinate;
			sample.time = time;
			sample.strength = strength;
			if (row)
			{
				sample.first = sinf(coordinate * 0.41f + time * 0.67f) * strength;
				sample.second = sinf(coordinate * 0.41f + time * 0.67f + 0.35f) * strength;
			}
			else
			{
				sample.first = cosf(coordinate * 0.33f - time * 0.51f) * strength;
				sample.second = cosf(coordinate * 0.33f - time * 0.51f + 0.30f) * strength;
			}
			sample.valid = true;
		}
		first = sample.first;
		second = sample.second;
	}

	inline void GetTerrainWorldColor(int index, vec3_t color)
	{
		if (g_TerrainWorldColorStamp[index] == g_TerrainWorldColorGeneration)
		{
			VectorCopy(g_TerrainWorldColorCache[index], color);
			return;
		}
		VectorCopy(PrimaryTerrainLight[index], color);
		if (g_TerrainWorldLighting)
		{
			float ndl = DotProduct(TerrainNormal[index], g_TerrainWorldSun);
			if (ndl < 0.0f) ndl = 0.0f;
			if (ndl > 1.0f) ndl = 1.0f;
			const float directional = 0.88f + ndl * 0.24f;
			const float multiplier = 1.0f + (directional - 1.0f) * g_TerrainWorldStrength;
			for (int c = 0; c < 3; ++c) color[c] *= multiplier;
		}
		if (g_TerrainWorldWetness && TerrainMappingLayer1[index] != 5 && TerrainMappingLayer2[index] != 5)
		{
			const float wet = g_TerrainWetAmount;
			const float up = fmaxf(0.0f, TerrainNormal[index][2]);
			const float sheen = wet * up * up * 0.035f;
			color[0] = color[0] * (1.0f - wet * 0.08f) + sheen * 0.72f;
			color[1] = color[1] * (1.0f - wet * 0.08f) + sheen * 0.82f;
			color[2] = color[2] * (1.0f - wet * 0.06f) + sheen;
		}
		if ((g_TerrainWaterAtmosphere || g_LorenciaWaterActive) &&
			(TerrainMappingLayer1[index] == 5 || TerrainMappingLayer2[index] == 5))
		{
			for (int c = 0; c < 3; ++c)
				color[c] = (color[c] * (1.0f - g_TerrainWaterMix) + g_TerrainWaterSky[c] * g_TerrainWaterMix) * g_TerrainWaterRainDark;
		}
		if (g_TerrainSceneNightOnly)
		{
			for (int c = 0; c < 3; ++c)
				color[c] *= g_TerrainNightScale[c];
		}
		for (int c = 0; c < 3; ++c)
		{
			if (color[c] < 0.0f) color[c] = 0.0f;
			if (color[c] > 1.5f) color[c] = 1.5f;
		}
		VectorCopy(color, g_TerrainWorldColorCache[index]);
		g_TerrainWorldColorStamp[index] = g_TerrainWorldColorGeneration;
	}
}

// Terrain tiles occupy separate cells. Group base tiles by texture,
// then render their alpha overlays, retaining order within each texture.
// Geometry, UVs and skill lighting are captured anew every frame.
namespace
{
    struct ArenaTerrainVertex
    {
        float position[3];
        float color[4];
        float uv[2];
    };
    struct ArenaTerrainFace
    {
        int texture;
        bool alpha;
        ArenaTerrainVertex vertices[6];
    };
    const int ARENA_TERRAIN_BATCH_CAPACITY = 1536;
    const int ARENA_TERRAIN_FACE_CAPACITY = 4096;
    ArenaTerrainVertex g_ArenaTerrainVertices[ARENA_TERRAIN_BATCH_CAPACITY];
    ArenaTerrainFace g_ArenaTerrainFaces[ARENA_TERRAIN_FACE_CAPACITY];
    int g_ArenaTerrainTextureFaces[2][256] = {};
    // Stable per-texture chains: visit each queued face once, keeping draw order.
    int g_ArenaTerrainFirstFace[2][256] = {};
    int g_ArenaTerrainLastFace[2][256] = {};
    int g_ArenaTerrainNextFace[ARENA_TERRAIN_FACE_CAPACITY];
    int g_ArenaTerrainFaceCount = 0;
    bool g_ArenaTerrainBatchActive = false;
    int g_ArenaTerrainTiles = 0;
    int g_ArenaTerrainAlphaTiles = 0;
    int g_ArenaTerrainDrawCalls = 0;

    void FlushArenaTerrainBatch()
    {
        if (g_ArenaTerrainFaceCount == 0) return;
        glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_COLOR_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glVertexPointer(3, GL_FLOAT, sizeof(ArenaTerrainVertex), g_ArenaTerrainVertices[0].position);
        glColorPointer(4, GL_FLOAT, sizeof(ArenaTerrainVertex), g_ArenaTerrainVertices[0].color);
        glTexCoordPointer(2, GL_FLOAT, sizeof(ArenaTerrainVertex), g_ArenaTerrainVertices[0].uv);
        for (int pass = 0; pass < 2; ++pass)
        {
            if (pass == 0) DisableAlphaBlend();
            else EnableAlphaTest();
            glAlphaFunc(GL_GREATER, 0.0f);
            for (int texture = 0; texture < 256; ++texture)
            {
                if (g_ArenaTerrainTextureFaces[pass][texture] == 0) continue;
                BindTexture(BITMAP_MAPTILE + texture);
                TarkanLavaMaterial::Scope lavaMaterial(TarkanLavaMaterial::ForMap(gMapManager->currentMap) &&
                    texture == 5, WorldTime * .001f, RealRenderFPS);
                LorenciaWaterMaterial::Scope waterMaterial(g_LorenciaWaterActive && g_LorenciaWaterMaterial && texture == 5,
                    WorldTime * 0.001f, CameraPosition, g_TerrainWorldSun, g_TerrainWaterSky, RealRenderFPS);
                int vertexCount = 0;
                for (int i = g_ArenaTerrainFirstFace[pass][texture]; i >= 0;
                    i = g_ArenaTerrainNextFace[i])
                {
                    const ArenaTerrainFace& face = g_ArenaTerrainFaces[i];
                    if (vertexCount + 6 > ARENA_TERRAIN_BATCH_CAPACITY)
                    {
                        glDrawArrays(GL_TRIANGLES, 0, vertexCount);
                        ++g_ArenaTerrainDrawCalls;
                        vertexCount = 0;
                    }
                    memcpy(g_ArenaTerrainVertices + vertexCount, face.vertices, sizeof(face.vertices));
                    vertexCount += 6;
                }
                if (vertexCount > 0)
                {
                    glDrawArrays(GL_TRIANGLES, 0, vertexCount);
                    ++g_ArenaTerrainDrawCalls;
                }
            }
        }
        glPopClientAttrib();
        const ArenaTerrainVertex& last = g_ArenaTerrainFaces[g_ArenaTerrainFaceCount - 1].vertices[5];
        glColor4fv(last.color);
        glTexCoord2fv(last.uv);
        glAlphaFunc(GL_GREATER, 0.2500000);
        g_ArenaTerrainFaceCount = 0;
        memset(g_ArenaTerrainTextureFaces, 0, sizeof(g_ArenaTerrainTextureFaces));
    }

    void QueueArenaTerrainTile(int texture, bool alpha)
    {
        if (g_ArenaTerrainFaceCount == ARENA_TERRAIN_FACE_CAPACITY)
            FlushArenaTerrainBatch();
        const int terrainIndices[4] = { TerrainIndex1, TerrainIndex2, TerrainIndex3, TerrainIndex4 };
        ArenaTerrainVertex corners[4];
        for (int i = 0; i < 4; ++i)
        {
            VectorCopy(TerrainVertex[i], corners[i].position);
            GetTerrainWorldColor(terrainIndices[i], corners[i].color);
            corners[i].color[3] = alpha ? TerrainMappingAlpha[terrainIndices[i]] : 1.0f;
            corners[i].uv[0] = TerrainTextureCoord[i][0];
            corners[i].uv[1] = TerrainTextureCoord[i][1];
        }
        ArenaTerrainFace& face = g_ArenaTerrainFaces[g_ArenaTerrainFaceCount++];
        face.texture = texture;
        face.alpha = alpha;
        const int order[6] = { 0, 1, 2, 0, 2, 3 };
        for (int i = 0; i < 6; ++i) face.vertices[i] = corners[order[i]];
        const int pass = alpha ? 1 : 0;
        const int faceIndex = g_ArenaTerrainFaceCount - 1;
        g_ArenaTerrainNextFace[faceIndex] = -1;
        if (g_ArenaTerrainTextureFaces[pass][texture] == 0)
            g_ArenaTerrainFirstFace[pass][texture] = faceIndex;
        else
            g_ArenaTerrainNextFace[g_ArenaTerrainLastFace[pass][texture]] = faceIndex;
        g_ArenaTerrainLastFace[pass][texture] = faceIndex;
        ++g_ArenaTerrainTextureFaces[pass][texture];
        if (alpha) ++g_ArenaTerrainAlphaTiles;
        else ++g_ArenaTerrainTiles;
    }
    bool CanBatchTerrainBaseTexture(int texture)
    {
        if (texture < 0 || texture >= 256 || texture == 14) return false;
        // Preserve every specialized blend, skipped tile and auxiliary layer.
        if (g_TerrainMapIndex == WD_39KANTURU_3RD && (texture == 3 || texture == 100)) return false;
        if (g_TerrainCursedTemple && texture == 4) return false;
        if (g_TerrainMapIndex == WD_51HOME_6TH_CHAR && texture == 2) return false;
        if (g_TerrainEmpireGuardian && texture == 10) return false;
        if (g_TerrainKarutan && texture == 12) return false;
        if ((g_TerrainMapIndex == WD_91GMACHERON1 || g_TerrainMapIndex == WD_92GMACHERON2) && texture == 9) return false;
        if (g_TerrainMapIndex == WD_113NIXIESLAKE && (TerrainWall[TerrainIndex1] & TW_ATT5)) return false;
        return true;
    }
}

inline void Vertex0()
{
	glTexCoord2f(TerrainTextureCoord[0][0], TerrainTextureCoord[0][1]);
	vec3_t color; GetTerrainWorldColor(TerrainIndex1, color); glColor3fv(color);
	glVertex3fv(TerrainVertex[0]);
}

inline void Vertex1()
{
	glTexCoord2f(TerrainTextureCoord[1][0], TerrainTextureCoord[1][1]);
	vec3_t color; GetTerrainWorldColor(TerrainIndex2, color); glColor3fv(color);
	glVertex3fv(TerrainVertex[1]);
}

inline void Vertex2()
{
	glTexCoord2f(TerrainTextureCoord[2][0], TerrainTextureCoord[2][1]);
	vec3_t color; GetTerrainWorldColor(TerrainIndex3, color); glColor3fv(color);
	glVertex3fv(TerrainVertex[2]);
}

inline void Vertex3()
{
	glTexCoord2f(TerrainTextureCoord[3][0], TerrainTextureCoord[3][1]);
	vec3_t color; GetTerrainWorldColor(TerrainIndex4, color); glColor3fv(color);
	glVertex3fv(TerrainVertex[3]);
}

inline void Vertex01()
{
	glTexCoord2f(TerrainTextureCoord01[0], TerrainTextureCoord01[1]);
	vec3_t color; GetTerrainWorldColor(Index01, color); glColor3fv(color);
	glVertex3fv(TerrainVertex01);
}

inline void Vertex12()
{
	glTexCoord2f(TerrainTextureCoord12[0], TerrainTextureCoord12[1]);
	vec3_t color; GetTerrainWorldColor(Index12, color); glColor3fv(color);
	glVertex3fv(TerrainVertex12);
}

inline void Vertex23()
{
	glTexCoord2f(TerrainTextureCoord23[0], TerrainTextureCoord23[1]);
	vec3_t color; GetTerrainWorldColor(Index23, color); glColor3fv(color);
	glVertex3fv(TerrainVertex23);
}

inline void Vertex30()
{
	glTexCoord2f(TerrainTextureCoord30[0], TerrainTextureCoord30[1]);
	vec3_t color; GetTerrainWorldColor(Index30, color); glColor3fv(color);
	glVertex3fv(TerrainVertex30);
}

inline void Vertex02()
{
	glTexCoord2f(TerrainTextureCoord02[0], TerrainTextureCoord02[1]);
	vec3_t color; GetTerrainWorldColor(Index02, color); glColor3fv(color);
	glVertex3fv(TerrainVertex02);
}

inline void VertexAlpha0()
{
	glTexCoord2f(TerrainTextureCoord[0][0], TerrainTextureCoord[0][1]);
	vec3_t color; GetTerrainWorldColor(TerrainIndex1, color);
	glColor4f(color[0], color[1], color[2], TerrainMappingAlpha[TerrainIndex1]);
	glVertex3fv(TerrainVertex[0]);
}

inline void VertexAlpha1()
{
	glTexCoord2f(TerrainTextureCoord[1][0], TerrainTextureCoord[1][1]);
	vec3_t color; GetTerrainWorldColor(TerrainIndex2, color);
	glColor4f(color[0], color[1], color[2], TerrainMappingAlpha[TerrainIndex2]);
	glVertex3fv(TerrainVertex[1]);
}

inline void VertexAlpha2()
{
	glTexCoord2f(TerrainTextureCoord[2][0], TerrainTextureCoord[2][1]);
	vec3_t color; GetTerrainWorldColor(TerrainIndex3, color);
	glColor4f(color[0], color[1], color[2], TerrainMappingAlpha[TerrainIndex3]);
	glVertex3fv(TerrainVertex[2]);
}

inline void VertexAlpha3()
{
	glTexCoord2f(TerrainTextureCoord[3][0], TerrainTextureCoord[3][1]);
	vec3_t color; GetTerrainWorldColor(TerrainIndex4, color);
	glColor4f(color[0], color[1], color[2], TerrainMappingAlpha[TerrainIndex4]);
	glVertex3fv(TerrainVertex[3]);
}

inline void VertexAlpha01()
{
	glTexCoord2f(TerrainTextureCoord01[0], TerrainTextureCoord01[1]);
	vec3_t color; GetTerrainWorldColor(Index01, color);
	glColor4f(color[0], color[1], color[2], TerrainMappingAlpha01);
	glVertex3fv(TerrainVertex01);
}

inline void VertexAlpha12()
{
	glTexCoord2f(TerrainTextureCoord12[0], TerrainTextureCoord12[1]);
	vec3_t color; GetTerrainWorldColor(Index12, color);
	glColor4f(color[0], color[1], color[2], TerrainMappingAlpha12);
	glVertex3fv(TerrainVertex12);
}

inline void VertexAlpha23()
{
	glTexCoord2f(TerrainTextureCoord23[0], TerrainTextureCoord23[1]);
	vec3_t color; GetTerrainWorldColor(Index23, color);
	glColor4f(color[0], color[1], color[2], TerrainMappingAlpha23);
	glVertex3fv(TerrainVertex23);
}

inline void VertexAlpha30()
{
	glTexCoord2f(TerrainTextureCoord30[0], TerrainTextureCoord30[1]);
	vec3_t color; GetTerrainWorldColor(Index30, color);
	glColor4f(color[0], color[1], color[2], TerrainMappingAlpha30);
	glVertex3fv(TerrainVertex30);
}

inline void VertexAlpha02()
{
	glTexCoord2f(TerrainTextureCoord02[0], TerrainTextureCoord02[1]);
	vec3_t color; GetTerrainWorldColor(Index02, color);
	glColor4f(color[0], color[1], color[2], TerrainMappingAlpha02);
	glVertex3fv(TerrainVertex02);
}

inline void VertexBlend0()
{
	glTexCoord2f(TerrainTextureCoord[0][0], TerrainTextureCoord[0][1]);
	float Light = TerrainMappingAlpha[TerrainIndex1];
	glColor3f(Light, Light, Light);
	glVertex3fv(TerrainVertex[0]);
}

inline void VertexBlend1()
{
	glTexCoord2f(TerrainTextureCoord[1][0], TerrainTextureCoord[1][1]);
	float Light = TerrainMappingAlpha[TerrainIndex2];
	glColor3f(Light, Light, Light);
	glVertex3fv(TerrainVertex[1]);
}

inline void VertexBlend2()
{
	glTexCoord2f(TerrainTextureCoord[2][0], TerrainTextureCoord[2][1]);
	float Light = TerrainMappingAlpha[TerrainIndex3];
	glColor3f(Light, Light, Light);
	glVertex3fv(TerrainVertex[2]);
}

inline void VertexBlend3()
{
	glTexCoord2f(TerrainTextureCoord[3][0], TerrainTextureCoord[3][1]);
	float Light = TerrainMappingAlpha[TerrainIndex4];
	glColor3f(Light, Light, Light);
	glVertex3fv(TerrainVertex[3]);
}

void Vertex__alpha0(float Alpha, bool Normal)
{
	glTexCoord2f(TerrainTextureCoord[0][0], TerrainTextureCoord[0][1]);
	glColor4f(PrimaryTerrainLight[TerrainIndex1][0], PrimaryTerrainLight[TerrainIndex1][1], PrimaryTerrainLight[TerrainIndex1][2], Alpha);
	glVertex3fv(TerrainVertex[0]);
	if (Normal) glNormal3fv(TerrainNormal[TerrainIndex1]);
}

void Vertex__alpha1(float Alpha, bool Normal)
{
	glTexCoord2f(TerrainTextureCoord[1][0], TerrainTextureCoord[1][1]);
	glColor4f(PrimaryTerrainLight[TerrainIndex2][0], PrimaryTerrainLight[TerrainIndex2][1], PrimaryTerrainLight[TerrainIndex2][2], Alpha);
	glVertex3fv(TerrainVertex[1]);
	if (Normal) glNormal3fv(TerrainNormal[TerrainIndex2]);
}

void Vertex__alpha2(float Alpha, bool Normal)
{
	glTexCoord2f(TerrainTextureCoord[2][0], TerrainTextureCoord[2][1]);
	glColor4f(PrimaryTerrainLight[TerrainIndex3][0], PrimaryTerrainLight[TerrainIndex3][1], PrimaryTerrainLight[TerrainIndex3][2], Alpha);
	glVertex3fv(TerrainVertex[2]);
	if (Normal) glNormal3fv(TerrainNormal[TerrainIndex3]);
}

void Vertex__alpha3(float Alpha, bool Normal)
{
	glTexCoord2f(TerrainTextureCoord[3][0], TerrainTextureCoord[3][1]);
	glColor4f(PrimaryTerrainLight[TerrainIndex4][0], PrimaryTerrainLight[TerrainIndex4][1], PrimaryTerrainLight[TerrainIndex4][2], Alpha);
	glVertex3fv(TerrainVertex[3]);

	if (Normal) glNormal3fv(TerrainNormal[TerrainIndex4]);
}

namespace
{
    void CaptureLorenciaWaterFace(int texture, bool alpha)
    {
        if (!g_LorenciaWaterActive || !g_LorenciaWaterMaterial || !g_LorenciaWaterReflections ||
            !g_TerrainRenderEnabledCached || texture != 5) return;
        const int indices[4] = {TerrainIndex1, TerrainIndex2, TerrainIndex3, TerrainIndex4};
        float coverage[4];
        for (int i = 0; i < 4; ++i)
        {
            const int index = indices[i];
            coverage[i] = alpha ? TerrainMappingAlpha[index] : 1.f;
            // Do not paint reflections over shore textures sharing the water mesh.
            if (!alpha && TerrainMappingLayer2[TerrainIndex1] != 5 && TerrainMappingLayer2[TerrainIndex1] != 255)
                coverage[i] *= 1.f - TerrainMappingAlpha[index];
        }
        LorenciaWaterSSR::Add(TerrainVertex, coverage);
    }
}

bool IsLorenciaRainPuddle(float x, float y)
{
    return SceneFlag == MAIN_SCENE && gMapManager->currentMap == WD_0LORENCIA && LorenciaRainPuddles::Contains(x, y);
}

void AddLorenciaRainRipple(float x, float y)
{
    if (IsLorenciaRainPuddle(x, y)) LorenciaRainPuddles::AddRipple(x, y, false);
}

void RenderLorenciaWaterReflections()
{
    if (SceneFlag == MAIN_SCENE && gMapManager->currentMap == WD_0LORENCIA &&
        g_LorenciaWaterActive && g_LorenciaWaterMaterial && g_LorenciaWaterReflections)
    {
        LorenciaRainPuddles::Queue();
        LorenciaWaterSSR::Render(WorldTime * 0.001f, CameraPosition, RealRenderFPS, g_TerrainWaterSky);
    }
}

void RenderFace(int Texture, int mx, int my)
{
    CaptureLorenciaWaterFace(Texture, false);
    if (g_ArenaTerrainBatchActive && CanBatchTerrainBaseTexture(Texture))
    {
        QueueArenaTerrainTile(Texture, false);
        return;
    }
    FlushArenaTerrainBatch();
	if (!g_TerrainRenderEnabledCached)
	{
		return;
	}

	int pMapIndex = g_TerrainMapIndex;

	if (pMapIndex == WD_39KANTURU_3RD)
	{
		if (Texture == 3)
			EnableAlphaTest(TRUE);
		else if (Texture == 100)
			return;
		else
			DisableAlphaBlend();
	}
	else if (g_TerrainCursedTemple)
	{
		if (Texture == 4)
			EnableAlphaTest(TRUE);
		else
			DisableAlphaBlend();
	}
	else if (pMapIndex == WD_51HOME_6TH_CHAR)
	{
		if (Texture == 2)
			EnableAlphaTest(TRUE);
		else
			DisableAlphaBlend();
	}
	else if (g_TerrainEmpireGuardian)
	{
		if (Texture == 10)
			EnableAlphaTest(TRUE);
		else
			DisableAlphaBlend();
	}
	else if (g_TerrainKarutan)
	{
		if (Texture == 12)
			EnableAlphaTest(TRUE);
		else
			DisableAlphaBlend();
	}
	else if (pMapIndex == WD_91GMACHERON1 || pMapIndex == WD_92GMACHERON2)
	{
		if (Texture == 9)
			EnableAlphaTest(TRUE);
		else
			DisableAlphaBlend();
	}
	else if (pMapIndex == WD_113NIXIESLAKE)
	{
		if (Texture == 14)
			EnableAlphaTest(TRUE);
		else
			DisableAlphaBlend();

		if ((TerrainWall[TerrainIndex1] & TW_ATT5) != 0)
		{
			BindTexture(BITMAP_map_texture08);
			glBegin(GL_TRIANGLE_FAN);
			Vertex__alpha0(0.4000000, true);
			Vertex__alpha1(0.4000000, true);
			Vertex__alpha2(0.4000000, true);
			Vertex__alpha3(0.4000000, true);
			glEnd();

			EnableAlphaTest(true);
			BindTexture(BITMAP_MAPTILE + Texture);
			glBegin(GL_TRIANGLE_FAN);
			Vertex__alpha0((1.000000 - 0.4000000), false);
			Vertex__alpha1((1.000000 - 0.4000000), false);
			Vertex__alpha2((1.000000 - 0.4000000), false);
			Vertex__alpha3((1.000000 - 0.4000000), false);
			glEnd();
			return;
		}
	}
	else
	{
		if (Texture == 14)
			EnableAlphaTest(TRUE);
		else
			DisableAlphaBlend();
	}

	BindTexture(BITMAP_MAPTILE + Texture);
	TarkanLavaMaterial::Scope lavaMaterial(TarkanLavaMaterial::ForMap(gMapManager->currentMap) &&
            Texture == 5, WorldTime * .001f, RealRenderFPS);
	LorenciaWaterMaterial::Scope waterMaterial(g_LorenciaWaterActive && g_LorenciaWaterMaterial && Texture == 5,
		WorldTime * 0.001f, CameraPosition, g_TerrainWorldSun, g_TerrainWaterSky, RealRenderFPS);
	glBegin(GL_TRIANGLE_FAN);
	Vertex0();
	Vertex1();
	Vertex2();
	Vertex3();
	glEnd();
}

void RenderFace_After(int Texture, int mx, int my)
{
	if (g_TerrainRenderEnabledCached)
	{
		if (Texture == 100 || Texture == 101)
		{
			if (Texture == 100)
				EnableAlphaTest();
			else if (Texture == 101)
				EnableAlphaBlend();

			BindTexture(BITMAP_MAPTILE + Texture);

			glBegin(GL_TRIANGLE_FAN);
			Vertex0();
			Vertex1();
			Vertex2();
			Vertex3();
			glEnd();
		}
	}
}

void RenderFaceAlpha(int Texture, int mx, int my)
{
    CaptureLorenciaWaterFace(Texture, true);
    if (g_ArenaTerrainBatchActive && Texture >= 0 && Texture < 256 && Texture != 14)
    {
        QueueArenaTerrainTile(Texture, true);
        return;
    }
    FlushArenaTerrainBatch();
	if (g_TerrainRenderEnabledCached)
	{
		EnableAlphaTest();
		BindTexture(BITMAP_MAPTILE + Texture);
		TarkanLavaMaterial::Scope lavaMaterial(TarkanLavaMaterial::ForMap(gMapManager->currentMap) &&
            Texture == 5, WorldTime * .001f, RealRenderFPS);
	LorenciaWaterMaterial::Scope waterMaterial(g_LorenciaWaterActive && g_LorenciaWaterMaterial && Texture == 5,
			WorldTime * 0.001f, CameraPosition, g_TerrainWorldSun, g_TerrainWaterSky, RealRenderFPS);

		glBegin(GL_TRIANGLE_FAN);
		VertexAlpha0();
		VertexAlpha1();
		VertexAlpha2();
		VertexAlpha3();
		glEnd();
	}

}

void RenderFaceBlend(int Texture, int mx, int my)
{
    FlushArenaTerrainBatch();
	if (g_TerrainRenderEnabledCached)
	{
		EnableAlphaBlend();
		BindTexture(BITMAP_MAPTILE + Texture);

		glBegin(GL_TRIANGLE_FAN);
		VertexBlend0();
		VertexBlend1();
		VertexBlend2();
		VertexBlend3();
		glEnd();
	}
}

void FaceTexture(int Texture, float xf, float yf, bool Water, bool Scale)
{
	vec3_t Light, Pos;
	Vector(0.30f, 0.40f, 0.20f, Light);
	BITMAP_t* b = &Bitmaps[BITMAP_MAPTILE + Texture];
	float Width, Height;

	if (Scale)
	{
		Width = 16.f / b->Width;
		Height = 16.f / b->Height;
	}
	else
	{
		Width = 64.f / b->Width;
		Height = 64.f / b->Height;
	}

	if (TarkanLavaMaterial::ForMap(gMapManager->currentMap) && Texture == 5)
    {
        // Four terrain cells per material tile; resolution changes must not enlarge the pattern.
        Width = Height = .25f;
        const float drift = fmodf(WorldTime * .000003f, 1.f);
        const float u = xf * Width + drift, v = yf * Height - drift;
        TEXCOORD(TerrainTextureCoord[0],u,v);
        TEXCOORD(TerrainTextureCoord[1],u+Width,v);
        TEXCOORD(TerrainTextureCoord[2],u+Width,v+Height);
        TEXCOORD(TerrainTextureCoord[3],u,v+Height);
        return;
    }
    float suf = xf * Width;
	float svf = yf * Height;
	if (!Water)
	{
		TEXCOORD(TerrainTextureCoord[0], suf, svf);
		TEXCOORD(TerrainTextureCoord[1], suf + Width, svf);
		TEXCOORD(TerrainTextureCoord[2], suf + Width, svf + Height);
		TEXCOORD(TerrainTextureCoord[3], suf, svf + Height);
	}
	else
	{
		float Water1 = 0.f;
		float Water2 = 0.f;
		float Water3 = 0.f;
		float Water4 = 0.f;

		if (gMapManager->currentMap == WD_34CRYWOLF_1ST && Texture == 5)
		{
			if (rand() % 50 == 0)
			{
				float sx = xf * TERRAIN_SCALE + (float)((rand() % 100 + 1) * 1.0f);
				float sy = yf * TERRAIN_SCALE + (float)((rand() % 100 + 1) * 1.0f);
				Vector(sx, sy, Hero->Object.Position[2] + 10.f, Pos);
				CreateParticleSync(BITMAP_SPOT_WATER, Pos, Hero->Object.Angle, Light, 0);
			}
		}

		if (gMapManager->currentMap == WD_30BATTLECASTLE && Texture == 5)
			suf -= WaterMove;
		else
			suf += WaterMove;

		if (Scale)
		{
			Water3 = TerrainGrassWind[TerrainIndex1] * 0.008f;
			Water4 = TerrainGrassWind[TerrainIndex2] * 0.008f;
		}
		else
		{
			Water3 = TerrainGrassWind[TerrainIndex1] * 0.002f;
			Water4 = TerrainGrassWind[TerrainIndex2] * 0.002f;
		}
		if (g_TerrainWaterDetail && !g_LorenciaWaterActive)
		{
			// Two low-amplitude wave trains break up straight scrolling tiles.
			// Keep the same geometry and texture; no extra draw call per tile.
			const float time = WorldTime * 0.001f;
			const float strength = g_TerrainWaterStrength * 0.012f;
			float wave1, wave2;
			GetTerrainWave(yf, true, time, strength, wave1, wave2);
			Water1 += wave1;
			Water2 += wave2;
			GetTerrainWave(xf, false, time, strength, wave1, wave2);
			Water3 += wave1;
			Water4 += wave2;
		}

		// Pilot: world-space UV displacement, identical on shared tile edges.
		// Reuses the terrain draw and texture; no additional render pass.
		if (g_LorenciaWaterActive)
		{
			const float time = WorldTime * 0.001f;
			const float strength = g_TerrainWaterStrength * 0.018f;
			for (int corner = 0; corner < 4; ++corner)
			{
				const float x = xf + ((corner == 1 || corner == 2) ? 1.f : 0.f);
				const float y = yf + ((corner >= 2) ? 1.f : 0.f);
				float u, unused, v;
				GetTerrainWave(y, true, time, strength, u, unused);
				GetTerrainWave(x, false, time, strength, v, unused);
				TEXCOORD(TerrainTextureCoord[corner], x * Width + (suf - xf * Width) + u, y * Height + v);
			}
			return;
		}
		TEXCOORD(TerrainTextureCoord[0], suf + Water1, svf + Water3);
		TEXCOORD(TerrainTextureCoord[1], suf + Width + Water2, svf + Water4);
		TEXCOORD(TerrainTextureCoord[2], suf + Width + Water2, svf + Height + Water4);
		TEXCOORD(TerrainTextureCoord[3], suf + Water1, svf + Height + Water3);
	}
}

int WaterTextureNumber = 0;

void RenderTerrainFace(float xf, float yf, int xi, int yi, float lodf)
{
	// Grass alpha-cutout: use the texture alpha exactly like foliage cutout,
	// without alpha-blending the whole quad. This keeps the transparent parts
	// invisible and avoids bright/colored rectangles over the terrain.
	if (TerrainFlag == TERRAIN_MAP_GRASS)
	{
		static const bool grassAlphaFix = GetPrivateProfileIntA("Renderer", "GrassAlphaFix", 1, ".\config.ini") != 0;
		static const float grassAlphaRef = []()
		{
			int value = GetPrivateProfileIntA("Renderer", "GrassAlphaRef", 38, ".\config.ini");
			if (value < 10) value = 10;
			if (value > 75) value = 75;
			return value * 0.01f;
		}();

		if (!g_TerrainGrassClientArraysActive)
			glAlphaFunc(GL_GREATER, grassAlphaFix ? grassAlphaRef : 0.0f);
	}
	else
	{
		glAlphaFunc(GL_GREATER, 0.0000000);
	}

	if (g_TerrainChaosCastle)
		RenderTerrainVisual(xi, yi);

	if (TerrainFlag != TERRAIN_MAP_GRASS)
	{
		int Texture;
		bool Alpha;
		bool Water = false;
		if (TerrainMappingAlpha[TerrainIndex1] >= 1.f && TerrainMappingAlpha[TerrainIndex2] >= 1.f && TerrainMappingAlpha[TerrainIndex3] >= 1.f && TerrainMappingAlpha[TerrainIndex4] >= 1.f)
		{
			Texture = TerrainMappingLayer2[TerrainIndex1];
			Alpha = false;
		}
		else
		{
			Texture = TerrainMappingLayer1[TerrainIndex1];
			Alpha = true;
			if (Texture == 5)
			{
				Water = true;
			}
			if (Texture == 11 && (g_TerrainPKField || g_TerrainDoppel2))
				Water = true;
		}
        if (g_LorenciaWaterActive && Texture == 5) Water = true;
        FaceTexture(Texture, xf, yf, Water, false);
		RenderFace(Texture, xi, yi);

		if (TerrainMappingAlpha[TerrainIndex1] > 0.f
			|| TerrainMappingAlpha[TerrainIndex2] > 0.f
			|| TerrainMappingAlpha[TerrainIndex3] > 0.f
			|| TerrainMappingAlpha[TerrainIndex4] > 0.f)
		{
			if ((gMapManager->currentMap == WD_7ATLANSE || g_TerrainDoppel3) && TerrainMappingLayer2[TerrainIndex1] == 5)
			{
				Texture = BITMAP_WATER - BITMAP_MAPTILE + (int)WaterTextureNumber;
				FaceTexture(Texture, xf, yf, false, true);
				RenderFaceBlend(Texture, xi, yi);
			}
			else if (Alpha)
			{
				Texture = TerrainMappingLayer2[TerrainIndex1];
				if (Texture != 5)
					Water = false;
				else if (g_LorenciaWaterActive)
					Water = true;
				if (Texture != 255)
				{
					FaceTexture(Texture, xf, yf, Water, false);
					RenderFaceAlpha(Texture, xi, yi);
				}
			}
		}
	}
	else
	{
		if (TerrainMappingAlpha[TerrainIndex1] > 0.f || TerrainMappingAlpha[TerrainIndex2] > 0.f || TerrainMappingAlpha[TerrainIndex3] > 0.f || TerrainMappingAlpha[TerrainIndex4] > 0.f)
		{
			return;
		}
		if (CurrentLayer == 0 && !g_TerrainBloodCastle)
		{
			int Texture = BITMAP_MAPGRASS + TerrainMappingLayer1[TerrainIndex1];

			BITMAP_t* pBitmap = Bitmaps.FindTexture(Texture);
			if (pBitmap)
			{
				float Height = pBitmap->Height * 2.f;
				BindTexture(Texture);

				if (g_TerrainPKField || g_TerrainDoppel2)
					EnableAlphaBlend();

				// Grass V3: keep the same one-quad render cost, but make the tuft shape
				// less rectangular and less repetitive. The changes below only add a few
				// cheap multiplications per tuft, so they preserve FPS.
				const float seedL = TerrainGrassTexture[TerrainIndex1];
				const float seedR = TerrainGrassTexture[TerrainIndex2];
				const float seedMid = (seedL + seedR) * 0.5f;

				float Width = (52.f + seedMid * 10.f) / 256.f;
				float su = xf * Width;
				su += TerrainGrassTexture[yi & TERRAIN_SIZE_MASK];
				TEXCOORD(TerrainTextureCoord[0], su, 0.f);
				TEXCOORD(TerrainTextureCoord[1], su + Width, 0.f);
				TEXCOORD(TerrainTextureCoord[2], su + Width, 1.f);
				TEXCOORD(TerrainTextureCoord[3], su, 1.f);

				VectorCopy(TerrainVertex[0], TerrainVertex[3]);
				VectorCopy(TerrainVertex[2], TerrainVertex[1]);

				const float heightL = Height * (0.92f + seedL * 0.32f);
				const float heightR = Height * (0.92f + seedR * 0.32f);
				const float topLeanL = -40.f - seedL * 14.f;
				const float topLeanR = -40.f - seedR * 14.f;
				const float tipInsetL = 4.f + seedL * 5.f;
				const float tipInsetR = 4.f + seedR * 5.f;

				TerrainVertex[0][2] += heightL;
				TerrainVertex[1][2] += heightR;
				TerrainVertex[0][0] += topLeanL + tipInsetL;
				TerrainVertex[1][0] += topLeanR - tipInsetR;

				if (g_TerrainKarutan)
				{
					TerrainVertex[0][1] += g_fTerrainGrassWind1[TerrainIndex1];
					TerrainVertex[1][1] += g_fTerrainGrassWind1[TerrainIndex2];
				}
				else
				{
					TerrainVertex[0][1] += TerrainGrassWind[TerrainIndex1];
					TerrainVertex[1][1] += TerrainGrassWind[TerrainIndex2];
				}

				vec4_t colors[4];

				int terrain_index[] = { TerrainIndex1 , TerrainIndex2, TerrainIndex3, TerrainIndex4, };

				for (int i = 0; i < 4; i++)
				{
					colors[i][3] = 1.0f;

					// Darker base and slightly brighter tips give more volume without shaders.
					float lightScale = (i < 2) ? 1.08f : 0.82f;
					vec3_t worldColor;
					GetTerrainWorldColor(terrain_index[i], worldColor);
					for (int c = 0; c < 3; ++c)
					{
						float v = worldColor[c] * lightScale;
						if (v > 1.0f) v = 1.0f;
						colors[i][c] = v;
					}
				}

				if (!g_TerrainGrassClientArraysActive)
				{
					glEnableClientState(GL_VERTEX_ARRAY);
					glEnableClientState(GL_COLOR_ARRAY);
					glEnableClientState(GL_TEXTURE_COORD_ARRAY);
				}

				glVertexPointer(3, GL_FLOAT, 0, TerrainVertex);
				glColorPointer(4, GL_FLOAT, 0, colors);
				glTexCoordPointer(2, GL_FLOAT, 0, TerrainTextureCoord);

				glDrawArrays(GL_QUADS, 0, 4);

				if (!g_TerrainGrassClientArraysActive)
				{
					glDisableClientState(GL_TEXTURE_COORD_ARRAY);
					glDisableClientState(GL_COLOR_ARRAY);
					glDisableClientState(GL_VERTEX_ARRAY);
					RestoreLegacyMeshArrayBatchState();
				}

				//glBegin(GL_QUADS);
				//glTexCoord2f(TerrainTextureCoord[0][0], TerrainTextureCoord[0][1]);
				//glColor3fv(PrimaryTerrainLight[TerrainIndex1]);
				//glVertex3fv(TerrainVertex[0]);

				//glTexCoord2f(TerrainTextureCoord[1][0], TerrainTextureCoord[1][1]);
				//glColor3fv(PrimaryTerrainLight[TerrainIndex2]);
				//glVertex3fv(TerrainVertex[1]);

				//glTexCoord2f(TerrainTextureCoord[2][0], TerrainTextureCoord[2][1]);
				//glColor3fv(PrimaryTerrainLight[TerrainIndex3]);
				//glVertex3fv(TerrainVertex[2]);

				//glTexCoord2f(TerrainTextureCoord[3][0], TerrainTextureCoord[3][1]);
				//glColor3fv(PrimaryTerrainLight[TerrainIndex4]);
				//glVertex3fv(TerrainVertex[3]);
				//glEnd();

				if (g_TerrainPKField || g_TerrainDoppel2)
					DisableAlphaBlend();
			}
		}
	}

	if (!g_TerrainGrassClientArraysActive)
		glAlphaFunc(GL_GREATER, 0.2500000);
}

void RenderTerrainFace_After(float xf, float yf, int xi, int yi, float lodf)
{
	if (TerrainFlag != TERRAIN_MAP_GRASS)
	{
		int Texture;
		int Water = 0;
		if (TerrainMappingAlpha[TerrainIndex1] >= 1.f && TerrainMappingAlpha[TerrainIndex2] >= 1.f && TerrainMappingAlpha[TerrainIndex3] >= 1.f && TerrainMappingAlpha[TerrainIndex4] >= 1.f)
		{
			Texture = TerrainMappingLayer2[TerrainIndex1];
		}
		else
		{
			Texture = TerrainMappingLayer1[TerrainIndex1];
			if (TerrainMappingLayer1[TerrainIndex1] == 5)
				Water = 1;
		}

		FaceTexture(Texture, xf, yf, Water, false);
		RenderFace_After(Texture, xi, yi);
	}
}

extern PATH* path;

extern int SelectWall;

bool RenderTerrainTile(float xf, float yf, int xi, int yi, float lodf, int lodi, bool Flag)
{
	TerrainIndex1 = TERRAIN_INDEX(xi, yi);

	if ((TerrainWall[TerrainIndex1] & TW_NOGROUND) == TW_NOGROUND && !Flag)
		return false;

	TerrainIndex2 = TERRAIN_INDEX(xi + lodi, yi);
	TerrainIndex3 = TERRAIN_INDEX(xi + lodi, yi + lodi);
	TerrainIndex4 = TERRAIN_INDEX(xi, yi + lodi);

	float sx = xf * TERRAIN_SCALE;
	float sy = yf * TERRAIN_SCALE;

	Vector(sx, sy, BackTerrainHeight[TerrainIndex1], TerrainVertex[0]);
	Vector(sx + TERRAIN_SCALE, sy, BackTerrainHeight[TerrainIndex2], TerrainVertex[1]);
	Vector(sx + TERRAIN_SCALE, sy + TERRAIN_SCALE, BackTerrainHeight[TerrainIndex3], TerrainVertex[2]);
	Vector(sx, sy + TERRAIN_SCALE, BackTerrainHeight[TerrainIndex4], TerrainVertex[3]);

	if ((TerrainWall[TerrainIndex1] & TW_HEIGHT) == TW_HEIGHT)
		TerrainVertex[0][2] = g_fSpecialHeight;
	if ((TerrainWall[TerrainIndex2] & TW_HEIGHT) == TW_HEIGHT)
		TerrainVertex[1][2] = g_fSpecialHeight;
	if ((TerrainWall[TerrainIndex3] & TW_HEIGHT) == TW_HEIGHT)
		TerrainVertex[2][2] = g_fSpecialHeight;
	if ((TerrainWall[TerrainIndex4] & TW_HEIGHT) == TW_HEIGHT)
		TerrainVertex[3][2] = g_fSpecialHeight;

	if (!Flag)
	{
		RenderTerrainFace(xf, yf, xi, yi, lodf);
#ifdef SHOW_PATH_INFO
#ifdef CSK_DEBUG_MAP_PATHFINDING
		if (g_bShowPath == true)
#endif // CSK_DEBUG_MAP_PATHFINDING
		{
			if (2 <= path->GetClosedStatus(TerrainIndex1))
			{
				EnableAlphaTest();
				DisableTexture();
				glBegin(GL_TRIANGLE_FAN);
				if (4 <= path->GetClosedStatus(TerrainIndex1))
				{
					glColor4f(0.3f, 0.3f, 1.0f, 0.5f);
				}
				else
				{
					glColor4f(1.0f, 1.0f, 1.0f, 0.3f);
				}
				for (int i = 0; i < 4; i++)
				{
					glVertex3fv(TerrainVertex[i]);
				}
				glEnd();
				DisableAlphaBlend();
			}
		}
#endif // SHOW_PATH_INFO
	}
	else
	{
#ifdef _DEBUG
		if (EditFlag != EDIT_LIGHT)
		{
			DisableTexture();
			glColor3f(0.5f, 0.5f, 0.5f);
			glBegin(GL_LINE_STRIP);
			for (int i = 0; i < 4; i++)
			{
				glVertex3fv(TerrainVertex[i]);
			}
			glEnd();
			DisableAlphaBlend();
		}
#endif// _DEBUG

		vec3_t Normal;
		FaceNormalize(TerrainVertex[0], TerrainVertex[1], TerrainVertex[2], Normal);
		bool Success = CollisionDetectLineToFace(MousePosition, MouseTarget, 3, TerrainVertex[0], TerrainVertex[1], TerrainVertex[2], TerrainVertex[3], Normal);
		if (Success == false)
		{
			FaceNormalize(TerrainVertex[0], TerrainVertex[2], TerrainVertex[3], Normal);
			Success = CollisionDetectLineToFace(MousePosition, MouseTarget, 3, TerrainVertex[0], TerrainVertex[2], TerrainVertex[3], TerrainVertex[1], Normal);
		}
		if (Success == true)
		{
			SelectFlag = true;
			SelectXF = xf;
			SelectYF = yf;
		}
#ifdef CSK_DEBUG_MAP_ATTRIBUTE
		if (EditFlag == EDIT_WALL &&
			((SelectWall == 0 && (TerrainWall[TerrainIndex1] & TW_NOMOVE) == TW_NOMOVE)
				|| (SelectWall == 2 && (TerrainWall[TerrainIndex1] & TW_SAFEZONE) == TW_SAFEZONE)
				|| (SelectWall == 6 && (TerrainWall[TerrainIndex1] & TW_CAMERA_UP) == TW_CAMERA_UP)
				|| (SelectWall == 7 && (TerrainWall[TerrainIndex1] & TW_NOATTACKZONE) == TW_NOATTACKZONE)
				|| (SelectWall == 8 && (TerrainWall[TerrainIndex1] & TW_ATT1) == TW_ATT1)
				|| (SelectWall == 9 && (TerrainWall[TerrainIndex1] & TW_ATT2) == TW_ATT2)
				|| (SelectWall == 10 && (TerrainWall[TerrainIndex1] & TW_ATT3) == TW_ATT3)
				|| (SelectWall == 11 && (TerrainWall[TerrainIndex1] & TW_ATT4) == TW_ATT4)
				|| (SelectWall == 12 && (TerrainWall[TerrainIndex1] & TW_ATT5) == TW_ATT5)
				|| (SelectWall == 13 && (TerrainWall[TerrainIndex1] & TW_ATT6) == TW_ATT6)
				|| (SelectWall == 14 && (TerrainWall[TerrainIndex1] & TW_ATT7) == TW_ATT7)
				))
		{
			DisableDepthTest();
			EnableAlphaTest();
			DisableTexture();

			glBegin(GL_TRIANGLE_FAN);
			glColor4f(1.f, 0.5f, 0.5f, 0.3f);
			for (int i = 0; i < 4; i++)
			{
				glVertex3fv(TerrainVertex[i]);
			}
			glEnd();

			DisableAlphaBlend();
		}
#endif // CSK_DEBUG_MAP_ATTRIBUTE

		return Success;
	}
	return false;
}

void RenderTerrainTile_After(float xf, float yf, int xi, int yi, float lodf, int lodi, bool Flag)
{
	TerrainIndex1 = TERRAIN_INDEX(xi, yi);
	TerrainIndex2 = TERRAIN_INDEX(xi + lodi, yi);
	TerrainIndex3 = TERRAIN_INDEX(xi + lodi, yi + lodi);
	TerrainIndex4 = TERRAIN_INDEX(xi, yi + lodi);

	float sx = xf * TERRAIN_SCALE;
	float sy = yf * TERRAIN_SCALE;

	Vector(sx, sy, BackTerrainHeight[TerrainIndex1], TerrainVertex[0]);
	Vector(sx + TERRAIN_SCALE, sy, BackTerrainHeight[TerrainIndex2], TerrainVertex[1]);
	Vector(sx + TERRAIN_SCALE, sy + TERRAIN_SCALE, BackTerrainHeight[TerrainIndex3], TerrainVertex[2]);
	Vector(sx, sy + TERRAIN_SCALE, BackTerrainHeight[TerrainIndex4], TerrainVertex[3]);

	if ((TerrainWall[TerrainIndex1] & TW_HEIGHT) == TW_HEIGHT)
		TerrainVertex[0][2] = g_fSpecialHeight;
	if ((TerrainWall[TerrainIndex2] & TW_HEIGHT) == TW_HEIGHT)
		TerrainVertex[1][2] = g_fSpecialHeight;
	if ((TerrainWall[TerrainIndex3] & TW_HEIGHT) == TW_HEIGHT)
		TerrainVertex[2][2] = g_fSpecialHeight;
	if ((TerrainWall[TerrainIndex4] & TW_HEIGHT) == TW_HEIGHT)
		TerrainVertex[3][2] = g_fSpecialHeight;

	if (!Flag)
	{
		if ((TerrainWall[TerrainIndex1] & TW_NOGROUND) != TW_NOGROUND)
			RenderTerrainFace_After(xf, yf, xi, yi, lodf);
	}
}

void RenderTerrainBitmapTile(float xf, float yf, float lodf, int lodi, vec3_t c[4], bool LightEnable, float Alpha, float Height = 0.f)
{
	int xi = (int)xf;
	int yi = (int)yf;
	if (xi < 0 || yi < 0 || xi >= TERRAIN_SIZE_MASK || yi >= TERRAIN_SIZE_MASK)
		return;

	float TileScale = TERRAIN_SCALE * lodf;
	float sx = xf * TERRAIN_SCALE;
	float sy = yf * TERRAIN_SCALE;
	TerrainIndex1 = TERRAIN_INDEX(xi, yi);
	TerrainIndex2 = TERRAIN_INDEX(xi + lodi, yi);
	TerrainIndex3 = TERRAIN_INDEX(xi + lodi, yi + lodi);
	TerrainIndex4 = TERRAIN_INDEX(xi, yi + lodi);
	Vector(sx, sy, BackTerrainHeight[TerrainIndex1] + Height, TerrainVertex[0]);
	Vector(sx + TileScale, sy, BackTerrainHeight[TerrainIndex2] + Height, TerrainVertex[1]);
	Vector(sx + TileScale, sy + TileScale, BackTerrainHeight[TerrainIndex3] + Height, TerrainVertex[2]);
	Vector(sx, sy + TileScale, BackTerrainHeight[TerrainIndex4] + Height, TerrainVertex[3]);

	vec3_t Light[4];
	if (LightEnable)
	{
		VectorCopy(PrimaryTerrainLight[TerrainIndex1], Light[0]);
		VectorCopy(PrimaryTerrainLight[TerrainIndex2], Light[1]);
		VectorCopy(PrimaryTerrainLight[TerrainIndex3], Light[2]);
		VectorCopy(PrimaryTerrainLight[TerrainIndex4], Light[3]);
	}

	glBegin(GL_TRIANGLE_FAN);
	for (int i = 0; i < 4; i++)
	{
		if (LightEnable)
		{
			if (Alpha == 1.f)
				glColor3fv(Light[i]);
			else
				glColor4f(Light[i][0], Light[i][1], Light[i][2], Alpha);
		}
		glTexCoord2f(c[i][0], c[i][1]);
		glVertex3fv(TerrainVertex[i]);
	}
	glEnd();
}

void RenderTerrainBitmap(int Texture, int mxi, int myi, float Rotation)
{
	glColor3f(1.f, 1.f, 1.f);

	vec3_t Angle;
	Vector(0.f, 0.f, Rotation, Angle);
	float Matrix[3][4];
	AngleMatrix(Angle, Matrix);

	BindTexture(Texture);
	BITMAP_t* b = &Bitmaps[Texture];
	float TexScaleU = 64.f / b->Width;
	float TexScaleV = 64.f / b->Height;
	for (float y = 0.f; y < b->Height / 64.f; y += 1.f)
	{
		for (float x = 0.f; x < b->Width / 64.f; x += 1.f)
		{
			vec3_t p1[4], p2[4];
			Vector((x)*TexScaleU, (y)*TexScaleV, 0.f, p1[0]);
			Vector((x + 1.f) * TexScaleU, (y)*TexScaleV, 0.f, p1[1]);
			Vector((x + 1.f) * TexScaleU, (y + 1.f) * TexScaleV, 0.f, p1[2]);
			Vector((x)*TexScaleU, (y + 1.f) * TexScaleV, 0.f, p1[3]);
			for (int i = 0; i < 4; i++)
			{
				p1[i][0] -= 0.5f;
				p1[i][1] -= 0.5f;
				VectorRotate(p1[i], Matrix, p2[i]);
				p2[i][0] += 0.5f;
				p2[i][1] += 0.5f;
			}
			RenderTerrainBitmapTile((float)mxi + x, (float)myi + y, 1.f, 1, p2, true, 1.f);
		}
	}
}

void RenderTerrainAlphaBitmap(int Texture, float xf, float yf, float SizeX, float SizeY, vec3_t Light, float Rotation, float Alpha, float Height)
{
	if (Alpha == 1.f)
		glColor3fv(Light);
	else
		glColor4f(Light[0], Light[1], Light[2], Alpha);

	vec3_t Angle;
	Vector(0.f, 0.f, Rotation, Angle);
	float Matrix[3][4];
	AngleMatrix(Angle, Matrix);

	BindTexture(Texture);
	float mxf = (xf / TERRAIN_SCALE);
	float myf = (yf / TERRAIN_SCALE);
	int   mxi = (int)(mxf);
	int   myi = (int)(myf);

	float Size;
	if (SizeX >= SizeY)
		Size = SizeX;
	else
		Size = SizeY;
	float TexU = (((float)mxi - mxf) + 0.5f * Size);
	float TexV = (((float)myi - myf) + 0.5f * Size);
	float TexScaleU = 1.f / Size;
	float TexScaleV = 1.f / Size;
	Size = (float)((int)Size + 1);
	float Aspect = SizeX / SizeY;
	for (float y = -Size; y <= Size; y += 1.f)
	{
		for (float x = -Size; x <= Size; x += 1.f)
		{
			vec3_t p1[4], p2[4];
			Vector((TexU + x) * TexScaleU, (TexV + y) * TexScaleV, 0.f, p1[0]);
			Vector((TexU + x + 1.f) * TexScaleU, (TexV + y) * TexScaleV, 0.f, p1[1]);
			Vector((TexU + x + 1.f) * TexScaleU, (TexV + y + 1.f) * TexScaleV, 0.f, p1[2]);
			Vector((TexU + x) * TexScaleU, (TexV + y + 1.f) * TexScaleV, 0.f, p1[3]);
			for (int i = 0; i < 4; i++)
			{
				p1[i][0] -= 0.5f;
				p1[i][1] -= 0.5f;
				VectorRotate(p1[i], Matrix, p2[i]);
				p2[i][0] *= Aspect;
				p2[i][0] += 0.5f;
				p2[i][1] += 0.5f;
				//if((p2[i][0]>=0.f && p2[i][0]<=1.f) || (p2[i][1]>=0.f && p2[i][1]<=1.f)) Clip = true;
			}
			RenderTerrainBitmapTile((float)mxi + x, (float)myi + y, 1.f, 1, p2, false, Alpha, Height);
		}
	}
}

vec3_t  FrustrumVertex[5];
vec3_t  FrustrumFaceNormal[5];
float   FrustrumFaceD[5];
int     FrustrumBoundMinX = 0;
int     FrustrumBoundMinY = 0;
int     FrustrumBoundMaxX = TERRAIN_SIZE_MASK;
int     FrustrumBoundMaxY = TERRAIN_SIZE_MASK;

float FrustrumX[4];
float FrustrumY[4];

extern int GetScreenWidth();


void CreateFrustrum2D(vec3_t Position)
{
	float Width = 0.0f;
	float WidthFar = 0.0f;
	float WidthNear = 0.0f;
	float CameraViewFar = 0.0f;
	float CameraViewNear = 0.0f;
	float CameraViewTarget = 0.0f;

#if MAIN_UPDATE > 303
	if (gmProtect->ScreenType == 0 && (GMProtect->LookAndFeelType() != 3 && GMProtect->LookAndFeelType() != 4))
	{
		Width = (float)GetScreenWidth() / GetWindowsX;
	}
	else
	{
		if (WindowWidth > 800 || (GMProtect->LookAndFeelType() == 3 || GMProtect->LookAndFeelType() == 4))
			Width = 1.0;
		else
			Width = (float)GetScreenWidth() / GetWindowsX;
	}
#else
	Width = 1.0;
#endif // MAIN_UPDATE > 303

	CAMERA_INFO* CurrentCam = CameraFactorPtr->CurrentCam();

	int iWorld = gMapManager->currentMap;

	if (iWorld == WD_6STADIUM && (FindText(Hero->ID, "webzen") || FindText(Hero->ID, "webzen2")))
	{
		CameraViewFar = CurrentCam->camviewFar[0];
		CameraViewNear = CameraViewFar * 0.05f;
		CameraViewTarget = CameraViewFar * 0.47f;
		WidthFar = CurrentCam->WidthFar[0] * Width;
		WidthNear = CurrentCam->WidthNear[0] * Width;
	}
	else
	{
		if (gMapManager->InBattleCastle() && SceneFlag == MAIN_SCENE)
		{
			if (battleCastle::InBattleCastle2(Hero->Object.Position) && (Hero->Object.Position[0] < 17100.f || Hero->Object.Position[0]> 18300.f))
			{
				CameraViewFar = CurrentCam->camviewFar[1];// * 0.1f;
				CameraViewNear = CameraViewFar * 0.19f;//0.22
				CameraViewTarget = CameraViewFar * 0.47f;//0.47
				WidthFar = CurrentCam->WidthFar[1] * Width; // 1140.f
				WidthNear = CurrentCam->WidthNear[1] * Width; // 540.f
			}
			else
			{
				CameraViewFar = CurrentCam->camviewFar[2];// * 0.1f;
				CameraViewNear = CameraViewFar * 0.19f;//0.22
				CameraViewTarget = CameraViewFar * 0.47f;//0.47
				WidthFar = CurrentCam->WidthFar[2] * Width; // 1140.f
				WidthNear = CurrentCam->WidthNear[2] * Width; // 540.f
			}
		}
		else if (iWorld == WD_62SANTA_TOWN)
		{
			CameraViewFar = CurrentCam->camviewFar[3];
			CameraViewNear = CameraViewFar * 0.19f;
			CameraViewTarget = CameraViewFar * 0.47f;
			WidthFar = CurrentCam->WidthFar[3] * Width;
			WidthNear = CurrentCam->WidthNear[3] * Width;
		}
		else if (gMapManager->IsPKField() || IsDoppelGanger2())
		{
			CameraViewFar = CurrentCam->camviewFar[5];
			CameraViewNear = CameraViewFar * 0.0166666666666667;
			CameraViewTarget = CameraViewFar * 0.2515151515151515;
			WidthFar = Width * CurrentCam->WidthFar[5];
			WidthNear = Width * CurrentCam->WidthNear[5];
		}
		else
		{
			static int CameraLevel;

			if ((int)CameraDistanceTarget >= (int)CameraDistance)
				CameraLevel = g_shCameraLevel;

			if (CameraLevel != 0)
			{
				switch (CameraLevel)
				{
				case 1:
					Width += 0.1f;
					CameraViewFar = 2700.f;
					CameraViewNear = CameraViewFar * 0.19f;
					CameraViewTarget = CameraViewFar * 0.47f;
					WidthFar = 1200.f * Width;
					WidthNear = 540.f * Width;
					break;
				case 2:
					Width += 0.1f;
					CameraViewFar = 3000.f;
					CameraViewNear = CameraViewFar * 0.19f;
					CameraViewTarget = CameraViewFar * 0.47f;
					WidthFar = 1300.f * Width;
					WidthNear = 540.f * Width;
					break;
				case 3:
					Width += 0.1f;
					CameraViewFar = 3300.f;
					CameraViewNear = CameraViewFar * 0.19f;
					CameraViewTarget = CameraViewFar * 0.47f;
					WidthFar = 1500.f * Width;
					WidthNear = 580.f * Width;
					break;
				case 4:
				case 5:
					Width += 0.1f;
					CameraViewFar = 3400.f;
					CameraViewNear = CameraViewFar * 0.19f;
					CameraViewTarget = CameraViewFar * 0.47f;
					WidthFar = 1600.f * Width;
					WidthNear = 660.f * Width;
					break;
				}
			}
			else
			{
				if (SceneFlag == LOG_IN_SCENE)
				{
					if (iWorld == WD_55LOGINSCENE)
					{
						Width *= 17.0 * 6.0;
					}
				}
				else if (SceneFlag == CHARACTER_SCENE)
				{
					if (iWorld == WD_54CHARACTERSCENE || iWorld == WD_74NEW_CHARACTER_SCENE)
					{
						Width *= 10.0 * 0.575995f;
					}
					else
					{
						Width *= 9.1f * 0.404998f;
					}
				}
				else if (g_Direction.m_CKanturu.IsMayaScene())
				{
					Width *= 10.0f * 0.115f;
				}
				else
				{
#if MAIN_UPDATE > 303
					if (WindowWidth > 800 || (GMProtect->LookAndFeelType() == 3 || GMProtect->LookAndFeelType() == 4))
						Width = 1.0;
					else
						Width = (float)GetScreenWidth() / GetWindowsX;
#else
					Width = 1.0;
#endif // MAIN_UPDATE > 303
				}

				if (SceneFlag == LOG_IN_SCENE)
				{
					if (iWorld == WD_78NEW_CHARACTER_SCENE)
					{
						CameraViewFar = 44880.f;
						CameraViewNear = 20400.f;
						CameraViewTarget = 19584.f;
						WidthFar = 6300.f * Width;
						WidthNear = 690.f * Width;
					}
					else
					{
						if (gmProtect->SceneLogin != 0)
						{
							CameraViewFar = gCameraManager->CalcViewFar(2400.0);
							Width = gCameraManager->CalcWidthCam();

							CameraViewNear = CameraViewFar * 0.19f;
							CameraViewTarget = CameraViewFar * 0.47f;
							WidthFar = 1190.0 * Width;
							WidthNear = 540.0 * Width;
						}
						else
						{
							CameraViewFar = 530400.0;
							CameraViewNear = 20400.0;
							CameraViewTarget = 20400.0;
							WidthFar = 5000.f * Width;
							WidthNear = 300.f * Width;
						}
					}
				}
				else if (SceneFlag == CHARACTER_SCENE)
				{
					if (gmProtect->SceneCharacter != 0)
					{
						CameraViewFar = gCameraManager->CalcViewFar(2400.0);
						Width = gCameraManager->CalcWidthCam();
					}
					else
					{
						CameraViewFar = 7370.9639;
					}

					CameraViewNear = CameraViewFar * 0.19f;
					CameraViewTarget = CameraViewFar * 0.47f;
					WidthFar = 1190.0 * Width;
					WidthNear = 540.0 * Width;
				}
				else
				{
					if (iWorld == WD_39KANTURU_3RD)
						CameraViewFar = 2300.0;
					else
						CameraViewFar = CurrentCam->camviewFar[4];

					CameraViewNear = CameraViewFar * 0.19f;//0.22
					CameraViewTarget = CameraViewFar * 0.47f;//0.47
					WidthFar = CurrentCam->WidthFar[4] * Width; // 1140.f
					WidthNear = CurrentCam->WidthNear[4] * Width; // 540.f
				}
			}
		}
	}

	vec3_t Angle, p[4];
	float Matrix[3][4];

	Vector(-WidthFar, CameraViewFar - CameraViewTarget, 0.f, p[0]);
	Vector(WidthFar, CameraViewFar - CameraViewTarget, 0.f, p[1]);
	Vector(WidthNear, CameraViewNear - CameraViewTarget, 0.f, p[2]);
	Vector(-WidthNear, CameraViewNear - CameraViewTarget, 0.f, p[3]);

	if (iWorld == WD_6STADIUM && (FindText(Hero->ID, "webzen") || FindText(Hero->ID, "webzen2")))
	{
		Vector(0.f, 0.f, -CameraAngle[2], Angle);
	}
	else if (iWorld == WD_73NEW_LOGIN_SCENE || iWorld == WD_77NEW_LOGIN_SCENE)
	{
		VectorScale(CameraAngle, -1.0f, Angle);
		if (iWorld == WD_73NEW_LOGIN_SCENE)
		{
			CCameraMove::GetInstancePtr()->SetFrustumAngle(89.5f);
			vec3_t _Temp = { CCameraMove::GetInstancePtr()->GetFrustumAngle(), 0.0f, 0.0f };
			VectorAdd(Angle, _Temp, Angle);
		}
	}
	else if (iWorld == WD_55LOGINSCENE)
	{
		Vector(0.f, 0.f, CameraAngle[2], Angle);
	}
	else
	{
		if (SceneFlag == MAIN_SCENE)
		{
			Vector(0.f, 0.f, CurrentCam->camAngle, Angle);
		}
		else
		{
			Vector(0.f, 0.f, 45.0, Angle);
		}
	}

	AngleMatrix(Angle, Matrix);
	vec3_t Frustrum[4];
	for (int i = 0; i < 4; i++)
	{
		VectorRotate(p[i], Matrix, Frustrum[i]);
		VectorAdd(Frustrum[i], Position, Frustrum[i]);
		FrustrumX[i] = Frustrum[i][0] * 0.01f;
		FrustrumY[i] = Frustrum[i][1] * 0.01f;
	}
}

bool TestFrustrum2D(float x, float y, float Range)
{
	if (!((gmProtect->SceneLogin == 1 && SceneFlag == LOG_IN_SCENE) || (gmProtect->SceneCharacter == 1 && SceneFlag == CHARACTER_SCENE)))
	{
		if (SceneFlag == SERVER_LIST_SCENE || SceneFlag == WEBZEN_SCENE || SceneFlag == LOADING_SCENE)
			return true;

		int j = 3;
		for (int i = 0; i < 4; j = i, i++)
		{
			float d = (FrustrumX[i] - x) * (FrustrumY[j] - y) - (FrustrumX[j] - x) * (FrustrumY[i] - y);

			Range -= 70;
			if (d <= Range)
			{
				return false;
			}
		}
	}
	return true;
}

void CreateFrustrum(float Aspect, vec3_t position)
{
	float Distance = CameraViewFar * 0.9f;
	if (g_pOption != NULL)
		Distance *= g_pOption->GetViewDistanceScale();
	float Width = tanf(CameraFOV * 0.5f * Q_PI / 180.f) * Distance * Aspect + 100.f;
	float Height = Width * 3.f / 4.f;

	vec3_t Temp[5];
	Vector(0.f, 0.f, 0.f, Temp[0]);
	Vector(-Width, Height, -Distance, Temp[1]);
	Vector(Width, Height, -Distance, Temp[2]);
	Vector(Width, -Height, -Distance, Temp[3]);
	Vector(-Width, -Height, -Distance, Temp[4]);

	float FrustrumMinX = (float)TERRAIN_SIZE * TERRAIN_SCALE;
	float FrustrumMinY = (float)TERRAIN_SIZE * TERRAIN_SCALE;
	float FrustrumMaxX = 0.f;
	float FrustrumMaxY = 0.f;
	float Matrix[3][4];

	GetOpenGLMatrix(Matrix);

	for (int i = 0; i < 5; i++)
	{
		vec3_t t;
		VectorIRotate(Temp[i], Matrix, t);
		VectorAdd(t, CameraPosition, FrustrumVertex[i]);

		if (FrustrumMinX > FrustrumVertex[i][0])
			FrustrumMinX = FrustrumVertex[i][0];

		if (FrustrumMinY > FrustrumVertex[i][1])
			FrustrumMinY = FrustrumVertex[i][1];

		if (FrustrumMaxX < FrustrumVertex[i][0])
			FrustrumMaxX = FrustrumVertex[i][0];

		if (FrustrumMaxY < FrustrumVertex[i][1])
			FrustrumMaxY = FrustrumVertex[i][1];
	}

	int tileWidth = 4;

	FrustrumBoundMinX = (int)(FrustrumMinX / TERRAIN_SCALE) / tileWidth * tileWidth - tileWidth;
	FrustrumBoundMinY = (int)(FrustrumMinY / TERRAIN_SCALE) / tileWidth * tileWidth - tileWidth;
	FrustrumBoundMaxX = (int)(FrustrumMaxX / TERRAIN_SCALE) / tileWidth * tileWidth + tileWidth;
	FrustrumBoundMaxY = (int)(FrustrumMaxY / TERRAIN_SCALE) / tileWidth * tileWidth + tileWidth;

	if (FrustrumBoundMinX < 0)
		FrustrumBoundMinX = 0;

	if (FrustrumBoundMinY < 0)
		FrustrumBoundMinY = 0;

	if (FrustrumBoundMaxX < 0)
		FrustrumBoundMaxX = 0;

	if (FrustrumBoundMaxY < 0)
		FrustrumBoundMaxY = 0;

	if (FrustrumBoundMinX > TERRAIN_SIZE_MASK - tileWidth)
		FrustrumBoundMinX = TERRAIN_SIZE_MASK - tileWidth;

	if (FrustrumBoundMinY > TERRAIN_SIZE_MASK - tileWidth)
		FrustrumBoundMinY = TERRAIN_SIZE_MASK - tileWidth;

	if (FrustrumBoundMaxX > TERRAIN_SIZE_MASK - tileWidth)
		FrustrumBoundMaxX = TERRAIN_SIZE_MASK - tileWidth;

	if (FrustrumBoundMaxY > TERRAIN_SIZE_MASK - tileWidth)
		FrustrumBoundMaxY = TERRAIN_SIZE_MASK - tileWidth;

	FaceNormalize(FrustrumVertex[0], FrustrumVertex[1], FrustrumVertex[2], FrustrumFaceNormal[0]);
	FaceNormalize(FrustrumVertex[0], FrustrumVertex[2], FrustrumVertex[3], FrustrumFaceNormal[1]);
	FaceNormalize(FrustrumVertex[0], FrustrumVertex[3], FrustrumVertex[4], FrustrumFaceNormal[2]);
	FaceNormalize(FrustrumVertex[0], FrustrumVertex[4], FrustrumVertex[1], FrustrumFaceNormal[3]);
	FaceNormalize(FrustrumVertex[3], FrustrumVertex[2], FrustrumVertex[1], FrustrumFaceNormal[4]);

	FrustrumFaceD[0] = -DotProduct(FrustrumVertex[0], FrustrumFaceNormal[0]);
	FrustrumFaceD[1] = -DotProduct(FrustrumVertex[0], FrustrumFaceNormal[1]);
	FrustrumFaceD[2] = -DotProduct(FrustrumVertex[0], FrustrumFaceNormal[2]);
	FrustrumFaceD[3] = -DotProduct(FrustrumVertex[0], FrustrumFaceNormal[3]);
	FrustrumFaceD[4] = -DotProduct(FrustrumVertex[1], FrustrumFaceNormal[4]);

	CreateFrustrum2D(position);
}

bool TestFrustrum(vec3_t Position, float Range)
{
	for (int i = 0; i < 5; i++)
	{
		float Value;
		Value = FrustrumFaceD[i] + DotProduct(Position, FrustrumFaceNormal[i]);
		if (Value < -Range)
			return false;
	}
	return true;
}

#ifdef DYNAMIC_FRUSTRUM

void CFrustrum::Make(vec3_t vEye, float fFov, float fAspect, float fDist)
{
	float Width = tanf(fFov * 0.5f * 3.141592f / 180.f) * fDist * fAspect + 100.f;
	float Height = Width * 3.f / 4.f;
	vec3_t Temp[5];
	vec3_t FrustrumVertex[5];
	Vector(0.f, 0.f, 0.f, Temp[0]);
	Vector(-Width, Height, -fDist, Temp[1]);
	Vector(Width, Height, -fDist, Temp[2]);
	Vector(Width, -Height, -fDist, Temp[3]);
	Vector(-Width, -Height, -fDist, Temp[4]);

	float Matrix[3][4];
	GetOpenGLMatrix(Matrix);
	for (int i = 0; i < 5; i++)
	{
		vec3_t t;
		VectorIRotate(Temp[i], Matrix, t);
		VectorAdd(t, vEye, FrustrumVertex[i]);
	}

	FaceNormalize(FrustrumVertex[0], FrustrumVertex[1], FrustrumVertex[2], m_FrustrumNorm[0]);
	FaceNormalize(FrustrumVertex[0], FrustrumVertex[2], FrustrumVertex[3], m_FrustrumNorm[1]);
	FaceNormalize(FrustrumVertex[0], FrustrumVertex[3], FrustrumVertex[4], m_FrustrumNorm[2]);
	FaceNormalize(FrustrumVertex[0], FrustrumVertex[4], FrustrumVertex[1], m_FrustrumNorm[3]);
	FaceNormalize(FrustrumVertex[3], FrustrumVertex[2], FrustrumVertex[1], m_FrustrumNorm[4]);
	m_FrustrumD[0] = -DotProduct(FrustrumVertex[0], m_FrustrumNorm[0]);
	m_FrustrumD[1] = -DotProduct(FrustrumVertex[0], m_FrustrumNorm[1]);
	m_FrustrumD[2] = -DotProduct(FrustrumVertex[0], m_FrustrumNorm[2]);
	m_FrustrumD[3] = -DotProduct(FrustrumVertex[0], m_FrustrumNorm[3]);
	m_FrustrumD[4] = -DotProduct(FrustrumVertex[1], m_FrustrumNorm[4]);
}

void CFrustrum::Create(vec3_t vEye, float fFov, float fAspect, float fDist)
{
	Make(vEye, fFov, fAspect, fDist);

	SetEye(vEye);
	SetFOV(fFov);
	SetAspect(fAspect);
	SetDist(fDist);
}

bool CFrustrum::Test(vec3_t vPos, float fRange)
{
	for (int i = 0; i < 5; ++i)
	{
		float fValue;
		fValue = m_FrustrumD[i] + DotProduct(vPos, m_FrustrumNorm[i]);
		if (fValue < -fRange) return false;
	}
	return true;
}

void CFrustrum::Reset()
{
	Make(m_vEye, m_fFov, m_fAspect, m_fDist);
}

void ResetAllFrustrum()
{
	FrustrumMap_t::iterator iter = g_FrustrumMap.begin();
	for (; iter != g_FrustrumMap.end(); ++iter)
	{
		CFrustrum* pData = iter->second;
		if (!pData) continue;
		pData->SetEye(CameraPosition);
		pData->Reset();
	}
}

CFrustrum* FindFrustrum(unsigned int iID)
{
	FrustrumMap_t::iterator iter = g_FrustrumMap.find(iID);
	if (iter != g_FrustrumMap.end()) return (CFrustrum*)iter->second;
	return NULL;
}

void DeleteAllFrustrum()
{
	FrustrumMap_t::iterator iter = g_FrustrumMap.begin();
	for (; iter != g_FrustrumMap.end(); ++iter)
	{
		CFrustrum* pData = iter->second;
		SAFE_DELETE(pData);
	}
	g_FrustrumMap.clear();
}

#endif// DYNAMIC_FRUSTRUM


/*bool TestFrustrum(vec3_t Position,float Range)
{
	int j = 3;
	for(int i=0;i<4;j=i,i++)
	{
		float d = (Frustrum[i][0]-Position[0]) * (Frustrum[j][1]-Position[1]) -
				  (Frustrum[j][0]-Position[0]) * (Frustrum[i][1]-Position[1]);
		if(d < 0.f) return false;
	}
	return true;
}*/

extern int RainCurrent;
extern int EnableEvent;

void InitTerrainLight()
{
	// The visible terrain rectangle is contiguous and never wraps here because
	// the frustum bounds are clamped before this function is called. Copy one
	// row at a time instead of issuing VectorCopy for every tile.
	const int minX = FrustrumBoundMinX;
	const int maxX = (std::min)(FrustrumBoundMaxX + 3, TERRAIN_SIZE_MASK);
	const int minY = FrustrumBoundMinY;
	const int maxY = (std::min)(FrustrumBoundMaxY + 3, TERRAIN_SIZE_MASK);
	const int rowCount = maxX - minX + 1;

	if (rowCount > 0)
	{
		for (int yi = minY; yi <= maxY; ++yi)
		{
			const int index = TERRAIN_INDEX(minX, yi);
			memcpy(&PrimaryTerrainLight[index], &BackTerrainLight[index],
				static_cast<size_t>(rowCount) * sizeof(vec3_t));
		}
	}

	float WindScale = 10.f;
	float WindSpeed;

	if (EnableEvent == 0)
	{
		WindSpeed = (int)WorldTime % (360000 * 2) * (0.002f);
	}
	else
	{
		WindSpeed = (int)WorldTime % 36000 * (0.01f);
	}

	// This update can precede RenderTerrain after a map change.
	// Query the current map once; the render-pass cache may still be stale.
	const bool karutan = IsKarutanMap();
	const bool iceCity = (gMapManager->currentMap == WD_57ICECITY || gMapManager->currentMap == WD_58ICECITY_BOSS);
	if (iceCity)
		WindScale = 60.f;

	const float WindScale1 = karutan ? 15.f : 0.f;
	const float WindSpeed1 = karutan ? ((int)WorldTime % 36000 * 0.008f) : 0.f;

	// TerrainGrassWind only depends on X and WorldTime. The legacy loop called
	// sinf() once for every X/Y tile even though every row receives the same
	// value. Compute it once per X, then fan it out to all visible rows.
	float windByX[TERRAIN_SIZE];
	float windKarutanByX[TERRAIN_SIZE];

	for (int xi = minX; xi <= maxX; ++xi)
	{
		const float xf = static_cast<float>(xi);
		const float phaseScale = (gMapManager->currentMap == WD_8TARKAN || karutan || iceCity) ? 50.f : 5.f;
		windByX[xi] = sinf(WindSpeed + xf * phaseScale) * WindScale;
		if (karutan)
			windKarutanByX[xi] = sinf(WindSpeed1 + xf * 50.f) * WindScale1;
	}

	for (int yi = minY; yi <= maxY; ++yi)
	{
		for (int xi = minX; xi <= maxX; ++xi)
		{
			const int index = TERRAIN_INDEX(xi, yi);
			TerrainGrassWind[index] = windByX[xi];
			if (karutan)
				g_fTerrainGrassWind1[index] = windKarutanByX[xi];
		}
	}
}

void InitTerrainShadow()
{
	/*int xi,yi;
	yi = FrustrumBoundMinY*2;
	for(;yi<=FrustrumBoundMaxY*2;yi+=2)
	{
		xi = FrustrumBoundMinX*2;
		for(;xi<=FrustrumBoundMaxX*2;xi+=2)
		{
			int Index1 = (yi  )*512+(xi  );
			int Index2 = (yi  )*512+(xi+1);
			int Index3 = (yi+1)*512+(xi+1);
			int Index4 = (yi+1)*512+(xi  );
			if(TerrainShadow[Index1] >= 1.f)
			{
				Vector(0.f,0.f,0.f,PrimaryTerrainLight[Index1]);
				Vector(0.f,0.f,0.f,PrimaryTerrainLight[Index2]);
				Vector(0.f,0.f,0.f,PrimaryTerrainLight[Index3]);
				Vector(0.f,0.f,0.f,PrimaryTerrainLight[Index4]);
			}
		}
	}*/
}

void Ray(int sx1, int sy1, int sx2, int sy2)
{
	/*int ShadowIndex = (sy1*TERRAIN_SIZE*2+sx1);
	if(TerrainShadow[ShadowIndex]==1.f) return;

	int nx1,ny1,d1,d2,len1,len2;
	int px1 = sx2-sx1;
	int py1 = sy2-sy1;
	if(px1 < 0  ) {px1 = -px1;nx1 =-1             ;} else nx1 = 1;
	if(py1 < 0  ) {py1 = -py1;ny1 =-TERRAIN_SIZE*2;} else ny1 = TERRAIN_SIZE*2;
	if(px1 > py1) {len1 = px1;len2 = py1;d1 = ny1;d2 = nx1;}
	else          {len1 = py1;len2 = px1;d1 = nx1;d2 = ny1;}

	int error = 0,count = 0;
	float Shadow = 0.f;
	do{
		TerrainShadow[ShadowIndex] = Shadow;
		int x = ShadowIndex%(TERRAIN_SIZE*2)/2;
		int y = ShadowIndex/(TERRAIN_SIZE*2)/2;
		if(TerrainWall[TERRAIN_INDEX(x,y)] >= 5)
			Shadow = 1.f;
		error += len2;
		if(error > len1/2)
		{
			ShadowIndex += d1;
			error -= len1;
		}
		ShadowIndex += d2;
	} while(++count <= len1);*/
}

void InitTerrainRay(int HeroX, int HeroY)
{
	/*TerrainShadow[HeroY*512+HeroX] = 0.f;

	int xi,yi;
	yi = FrustrumBoundMinY*2;
	for(;yi<=FrustrumBoundMaxY*2;yi++)
	{
		xi = FrustrumBoundMinX*2;
		for(;xi<=FrustrumBoundMaxX*2;xi++)
		{
			TerrainShadow[(yi*TERRAIN_SIZE*2+xi)] = -1.f;
		}
	}
	yi = FrustrumBoundMinY*2;
	for(;yi<=FrustrumBoundMaxY*2;yi++)
	{
		xi = FrustrumBoundMinX*2;
		for(;xi<=FrustrumBoundMaxX*2;xi++)
		{
			Ray(HeroX,HeroY,xi,yi);
		}
	}*/
}

void RenderTerrainBlock(float xf, float yf, int xi, int yi, bool EditFlag)
{
	//int x = ((xi/4)&63);
	//int y = ((yi/4)&63);
	//int lodi = LodBuffer[y*64+x];
	int lodi = 1;
	float lodf = (float)lodi;
	for (int i = 0; i < 4; i += lodi)
	{
		float temp = xf;
		for (int j = 0; j < 4; j += lodi)
		{
			if (TestFrustrum2D(xf + 0.5f, yf + 0.5f, 0.f) || CameraTopViewEnable)
			{
				if (g_TerrainCollectVisibleTiles)
				{
					if (g_TerrainVisibleTileCount < TERRAIN_SIZE * TERRAIN_SIZE)
					{
						TerrainVisibleTile& tile = g_TerrainVisibleTiles[g_TerrainVisibleTileCount++];
						tile.x = static_cast<short>(xi + j);
						tile.y = static_cast<short>(yi + i);
					}
					else
					{
						g_TerrainVisibleTileOverflow = true;
					}
				}
				RenderTerrainTile(xf, yf, xi + j, yi + i, lodf, lodi, EditFlag);
			}
			xf += lodf;
		}
		xf = temp;
		yf += lodf;
	}
}

void RenderTerrainVisibleTiles(bool EditFlag)
{
	int grassStride = 1;
	if (RealRenderFPS > 0.0f && RealRenderFPS < 40.0f)
		grassStride = 4;
	else if (RealRenderFPS > 0.0f && RealRenderFPS < 55.0f)
		grassStride = 2;

	for (int i = 0; i < g_TerrainVisibleTileCount; ++i)
	{
		const int xi = g_TerrainVisibleTiles[i].x;
		const int yi = g_TerrainVisibleTiles[i].y;

		// This function is the second (grass) pass. Keep every nearby tile and
		// thin only distant grass cards while the renderer is over budget. The
		// terrain itself is still rendered in full by the first pass.
		if (grassStride > 1 && Hero != NULL &&
			((xi + yi * 3) & (grassStride - 1)) != 0)
		{
			const float worldX = static_cast<float>(xi) * TERRAIN_SCALE;
			const float worldY = static_cast<float>(yi) * TERRAIN_SCALE;
			const float dx = worldX - Hero->Object.Position[0];
			const float dy = worldY - Hero->Object.Position[1];
			const float fullGrassRadius = 1200.0f;
			if ((dx * dx + dy * dy) > (fullGrassRadius * fullGrassRadius))
				continue;
		}

		RenderTerrainTile(static_cast<float>(xi), static_cast<float>(yi), xi, yi, 1.f, 1, EditFlag);
	}
}

void RenderTerrainFrustrum(bool EditFlag)
{
	int     xi;
	int     yi = FrustrumBoundMinY;
	float   xf;
	float   yf = (float)yi;

	for (; yi <= FrustrumBoundMaxY; yi += 4, yf += 4.f)
	{
		xi = FrustrumBoundMinX;
		xf = (float)xi;
		for (; xi <= FrustrumBoundMaxX; xi += 4, xf += 4.f)
		{
			if (TestFrustrum2D(xf + 2.f, yf + 2.f, g_fFrustumRange) || CameraTopViewEnable)
			{
				if (gMapManager->currentMap == WD_73NEW_LOGIN_SCENE || gMapManager->currentMap == WD_77NEW_LOGIN_SCENE)
				{
					const float fDistance_x = CameraPosition[0] - xf / 0.01f;
					const float fDistance_y = CameraPosition[1] - yf / 0.01f;
					const float fDistanceSq = fDistance_x * fDistance_x + fDistance_y * fDistance_y;

					// Avoid sqrtf() for every candidate terrain block.
					if (fDistanceSq > (5200.f * 5200.f))
						continue;
				}
				RenderTerrainBlock(xf, yf, xi, yi, EditFlag);
			}
		}
	}
}

void RenderTerrainBlock_After(float xf, float yf, int xi, int yi, bool EditFlag)
{
	int lodi = 1;
	float lodf = (float)lodi;
	for (int i = 0; i < 4; i += lodi)
	{
		float temp = xf;
		for (int j = 0; j < 4; j += lodi)
		{
			if (TestFrustrum2D(xf + 0.5f, yf + 0.5f, 0.f) || CameraTopViewEnable)
			{
				RenderTerrainTile_After(xf, yf, xi + j, yi + i, lodf, lodi, EditFlag);
			}
			xf += lodf;
		}
		xf = temp;
		yf += lodf;
	}
}

void RenderTerrainFrustrum_After(bool EditFlag)
{
	int   xi, yi;
	float xf, yf;
	yi = FrustrumBoundMinY;
	yf = (float)yi;
	for (; yi <= FrustrumBoundMaxY; yi += 4, yf += 4.f)
	{
		xi = FrustrumBoundMinX;
		xf = (float)xi;
		for (; xi <= FrustrumBoundMaxX; xi += 4, xf += 4.f)
		{
			if (TestFrustrum2D(xf + 2.f, yf + 2.f, -80.f) || CameraTopViewEnable)
			{
				RenderTerrainBlock_After(xf, yf, xi, yi, EditFlag);
			}
		}
	}
}

extern int SelectMapping;
extern void RenderCharactersClient();

void RenderTerrain(bool EditFlag)
{
    if (EditFlag) LorenciaWaterSSR::Reset(false);
	if (!EditFlag)
	{
		UpdateTerrainWorldVisual();
        LorenciaWaterSSR::Reset(g_LorenciaWaterActive && g_LorenciaWaterMaterial && g_LorenciaWaterReflections);
        LorenciaRainPuddles::Update(g_LorenciaWaterActive && g_LorenciaWaterMaterial && g_LorenciaWaterReflections && g_LorenciaRainPuddles, g_TerrainWorldRain);
		if (++g_TerrainWorldColorGeneration == 0)
		{
			memset(g_TerrainWorldColorStamp, 0, sizeof(g_TerrainWorldColorStamp));
			g_TerrainWorldColorGeneration = 1;
		}
	}
	g_TerrainRenderEnabledCached = g_pOption->GetRenderTerrain();
	g_TerrainMapIndex = gMapManager->FindMapIndex();
	g_TerrainCursedTemple = gMapManager->IsCursedTemple();
	g_TerrainEmpireGuardian = gMapManager->IsEmpireGuardian();
	g_TerrainKarutan = IsKarutanMap();
	g_TerrainPKField = gMapManager->IsPKField();
	g_TerrainDoppel2 = IsDoppelGanger2();
	g_TerrainDoppel3 = IsDoppelGanger3();
	g_TerrainBloodCastle = gMapManager->InBloodCastle();
	g_TerrainChaosCastle = gMapManager->InChaosCastle();

	if (!EditFlag)
	{
		if (gMapManager->currentMap == WD_8TARKAN)
		{
			WaterMove = (float)((int)(WorldTime) % 40000) * 0.000025f;
		}
		else if (gMapManager->currentMap == WD_42CHANGEUP3RD_2ND)
		{
			int iWorldTime = (int)WorldTime;
			int iRemainder = iWorldTime % 50000;
			WaterMove = (float)iRemainder * 0.00002f;
		}
		else
		{
			// Login-water style uses a more detailed texture, so the stock 20s UV
			// cycle looks unnaturally fast. Slow only that style; do not touch
			// gameplay FPS or water animation on other maps.
			static const bool waterLoginStyle = GetPrivateProfileIntA("Renderer", "WaterLoginStyle", 1, ".\\config.ini") != 0;
			static const bool waterLoginAllMaps = GetPrivateProfileIntA("Renderer", "WaterLoginStyleAllMaps", 0, ".\\config.ini") != 0;
			const bool useLoginWater = waterLoginStyle && SceneFlag == MAIN_SCENE &&
				(gMapManager->currentMap == WD_0LORENCIA ||
				 (waterLoginAllMaps && gMapManager->currentMap != WD_7ATLANSE && !gMapManager->InHellas()));

			if (useLoginWater)
			{
				static const int speedPercent = []()
				{
					int value = GetPrivateProfileIntA("Renderer", "WaterLoginSpeed", 35, ".\\config.ini");
					if (value < 10) value = 10;
					if (value > 100) value = 100;
					return value;
				}();

				// Stock water = one UV cycle every 20 seconds. 35% = ~57 seconds.
				const int cycleMs = (20000 * 100) / speedPercent;
				WaterMove = (float)((int)(WorldTime) % cycleMs) / (float)cycleMs;
			}
			else
			{
				WaterMove = (float)((int)(WorldTime) % 20000) * 0.00005f;
			}
		}
	}

	if (!EditFlag)
	{
		DisableAlphaBlend();
	}
	else
	{
		SelectFlag = false;
		InitCollisionDetectLineToFace();
	}

	TerrainFlag = TERRAIN_MAP_NORMAL;
	g_TerrainVisibleTileCount = 0;
	g_TerrainVisibleTileOverflow = false;
    g_TerrainCollectVisibleTiles = !EditFlag;
    static const bool arenaBatchEnabled = GetPrivateProfileIntA("Renderer", "ArenaTerrainBatch", 1, ".\\config.ini") != 0;
    static const bool terrainBatchEnabled = GetPrivateProfileIntA("Renderer", "TerrainBatch", 1, ".\\config.ini") != 0;
    const bool mapBatchEnabled = gMapManager->currentMap == WD_6STADIUM ? arenaBatchEnabled : terrainBatchEnabled;
    // Chaos Castle inserts visual draws between terrain cells; retain its ordering.
    g_ArenaTerrainBatchActive = mapBatchEnabled && !EditFlag && SceneFlag == MAIN_SCENE &&
        g_TerrainRenderEnabledCached && !g_TerrainChaosCastle;
#ifdef SHOW_PATH_INFO
    g_ArenaTerrainBatchActive = false;
#endif
    g_ArenaTerrainTiles = 0;
    g_ArenaTerrainAlphaTiles = 0;
    g_ArenaTerrainDrawCalls = 0;
    const auto terrainBaseStart = std::chrono::steady_clock::now();
    RenderTerrainFrustrum(EditFlag);
    FlushArenaTerrainBatch();
    const double terrainBaseMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - terrainBaseStart).count();
    double terrainGrassMs = 0.0;
    const bool arenaBatchUsed = g_ArenaTerrainBatchActive;
    g_ArenaTerrainBatchActive = false;
    g_TerrainCollectVisibleTiles = false;

	//  
	if (EditFlag && SelectFlag)
	{
		RenderTerrainTile(SelectXF, SelectYF, (int)SelectXF, (int)SelectYF, 1.f, 1, EditFlag);
	}
	if (!EditFlag)
	{
		EnableAlphaTest();
		if (TerrainGrassEnable && gMapManager->currentMap != WD_7ATLANSE && !g_TerrainDoppel3)
		{
			TerrainFlag = TERRAIN_MAP_GRASS;

            const auto terrainGrassStart = std::chrono::steady_clock::now();
            static const bool grassAlphaFix = GetPrivateProfileIntA("Renderer", "GrassAlphaFix", 1, ".\\config.ini") != 0;
            static const float grassAlphaRef = []()
            {
                int value = GetPrivateProfileIntA("Renderer", "GrassAlphaRef", 38, ".\\config.ini");
                if (value < 10) value = 10;
                if (value > 75) value = 75;
                return value * 0.01f;
            }();
            glAlphaFunc(GL_GREATER, grassAlphaFix ? grassAlphaRef : 0.0f);
			// One setup/teardown for the full grass pass instead of per tile.
			glEnableClientState(GL_VERTEX_ARRAY);
			glEnableClientState(GL_COLOR_ARRAY);
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			g_TerrainGrassClientArraysActive = true;

			if (g_TerrainVisibleTileOverflow)
				RenderTerrainFrustrum(EditFlag);
			else
				RenderTerrainVisibleTiles(EditFlag);

            terrainGrassMs = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - terrainGrassStart).count();
			g_TerrainGrassClientArraysActive = false;
            glAlphaFunc(GL_GREATER, 0.2500000);
			glDisableClientState(GL_TEXTURE_COORD_ARRAY);
			glDisableClientState(GL_COLOR_ARRAY);
			glDisableClientState(GL_VERTEX_ARRAY);
			RestoreLegacyMeshArrayBatchState();
		}
		DisableDepthTest();
		EnableCullFace();
		RenderPointers();
		EnableDepthTest();
	}
    if (!EditFlag && SceneFlag == MAIN_SCENE)
    {
        static DWORD lastArenaBatchLog = 0;
        const DWORD now = GetTickCount();
        if (now - lastArenaBatchLog >= 5000)
        {
            lastArenaBatchLog = now;
            FILE* profile = fopen("MapTerrainBatchProfile.log", "ab");
            if (profile)
            {
                fprintf(profile, "Time=%lu World=%d Batched=%d OpaqueTiles=%d AlphaTiles=%d DrawCalls=%d BaseMs=%.3f GrassMs=%.3f VisibleTiles=%d FPS=%.1f\r\n",
                    now, gMapManager->currentMap, arenaBatchUsed ? 1 : 0, g_ArenaTerrainTiles, g_ArenaTerrainAlphaTiles, g_ArenaTerrainDrawCalls, terrainBaseMs, terrainGrassMs, g_TerrainVisibleTileCount, RealRenderFPS);
                fclose(profile);
            }
        }
    }
}

void RenderTerrain_After(bool EditFlag)
{
	if (gMapManager->currentMap != WD_39KANTURU_3RD)
		return;

	TerrainFlag = TERRAIN_MAP_NORMAL;
	RenderTerrainFrustrum_After(EditFlag);
}

OBJECT Sun;

void CreateSun()
{
	//Sun.Type = BITMAP_LIGHT;
	Sun.Scale = 8.f;
	Sun.AnimationFrame = 1.f;
}

void RenderSun()
{
	EnableAlphaBlend();
	vec3_t Angle;
	float Matrix[3][4];
	Angle[0] = 0.f;
	Angle[1] = 0.f;
	Angle[2] = CameraAngle[2];
	AngleIMatrix(Angle, Matrix);
	vec3_t p, Position;
	Vector(-900.f, CameraViewFar * 0.9f, 0.f, p);
	VectorRotate(p, Matrix, Position);
	VectorAdd(CameraPosition, Position, Sun.Position);
	Sun.Position[2] = 550.f;
	Sun.Visible = TestDepthBuffer(Sun.Position);
	BeginSprite();
	//RenderSprite(&Sun);
	EndSprite();
	DisableAlphaBlend();
}

void RenderSky()
{
	vec3_t Angle;
	float Matrix[3][4];
	Angle[0] = 0.f;
	Angle[1] = 0.f;
	Angle[2] = CameraAngle[2];
	AngleIMatrix(Angle, Matrix);
	float Aspect = (float)(WindowWidth) / (float)(WindowWidth);
	float Width = 1780.f * Aspect;

	BeginSprite();
	vec3_t p, Position;
	float Num = 20.f;
	vec3_t LightTable[21];

	for (int i = 0; i <= Num; i++)
	{
		Vector(((float)i - Num * 0.5f) * (Width / Num), CameraViewFar * 0.99f, 0.f, p);
		VectorRotate(p, Matrix, Position);
		VectorAdd(CameraPosition, Position, Position);
		RequestTerrainLight(Position[0], Position[1], LightTable[i]);
	}

	for (int i = 1; i <= (int)Num; i++)
	{
		if (LightTable[i][0] <= 0.f)
		{
			Vector(0.f, 0.f, 0.f, LightTable[i - 1]);
		}
	}

	for (int i = (int)Num - 1; i >= 0; i--)
	{
		if (LightTable[i][0] <= 0.f)
		{
			Vector(0.f, 0.f, 0.f, LightTable[i + 1]);
		}
	}
	for (float x = 0.f; x < Num; x += 1.f)
	{
		float UV[4][2];
		TEXCOORD(UV[0], (x) * (1.f / Num), 1.f);
		TEXCOORD(UV[1], (x + 1.f) * (1.f / Num), 1.f);
		TEXCOORD(UV[2], (x + 1.f) * (1.f / Num), 0.f);
		TEXCOORD(UV[3], (x) * (1.f / Num), 0.f);

		vec3_t Light[4];
		VectorCopy(LightTable[(int)x], Light[0]);
		VectorCopy(LightTable[(int)x + 1], Light[1]);
		//VectorCopy(LightTable[(int)x+1],Light[2]);
		//VectorCopy(LightTable[(int)x  ],Light[3]);
		Vector(1.f, 1.f, 1.f, Light[2]);
		Vector(1.f, 1.f, 1.f, Light[3]);

		Vector((x - Num * 0.5f + 0.5f) * (Width / Num), CameraViewFar * 0.9f, 0.f, p);
		VectorRotate(p, Matrix, Position);
		VectorAdd(CameraPosition, Position, Position);
		Position[2] = 400.f;
		//RenderSpriteUV(BITMAP_SKY,Position,Width/Num,Height,UV,Light);
	}
	EndSprite();
}
