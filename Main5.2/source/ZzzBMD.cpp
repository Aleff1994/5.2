///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ZzzOpenglUtil.h"
#include "ZzzInfomation.h"
#include "ZzzBMD.h"
#include "CrowdMeshBudget.h"
#include "NaturalSurfaceLight.h"
#include "MeshSubmissionPolicy.h"
#include "IndexedMeshTopology.h"
#include "ShadowFlatGroundPatch.h"
#include "ZzzObject.h"
#include "ZzzCharacter.h"
#include "zzzlodterrain.h"
#include "ZzzTexture.h"
#include "ZzzAI.h"
#include "SMD.h"
#include "zzzEffect.h"
#include "MapManager.h"
#include "./Utilities/Log/muConsoleDebug.h"
//#include "FillPolygon.h"
#include "GMBattleCastle.h"
#include "UIMng.h"
#include "CameraMove.h"
#include "PhysicsManager.h"
#include "MuCrypto/MuCrypto.h"
#include "Renderer/Modern/ModelRenderer.h"
#include "Renderer/RenderBackend.h"
#include "NewUISystem.h"
#include "NewUIOptionWindow.h"
#include "ZzzScene.h"
#include <algorithm>
#include <map>

// Compatibilidade com macro max/min do Windows (tipos mistos)
#undef max
#undef min
template <typename T, typename U>
inline auto bmd_max(T a, U b) -> decltype(a > b ? a : b) { return (a > b ? a : b); }
template <typename T, typename U>
inline auto bmd_min(T a, U b) -> decltype(a < b ? a : b) { return (a < b ? a : b); }
#define max bmd_max
#define min bmd_min

extern float MouseX;
extern float MouseY;
extern bool MouseLButton;
extern float RealRenderFPS;
extern int g_VisibleCharacterCountForShadowLod;

// RenderItem3D usa os mesmos BMDs do mundo para inventario e tooltip.
// Esta guarda evita neve nas miniaturas da interface sem afetar os modelos 3D
// equipados no personagem ou os itens dropados no mapa.
bool g_BmdSnowSuppressUI = false;

// Pegasus-style snow material for opaque world meshes. Read settings at most
// once per second; RenderMesh is a hot path and must not access the INI per mesh.
namespace
{
	struct BmdSnowSettings
	{
		bool enabled;
		bool nativeOnly;
		bool exposedOnly;
		bool opaqueOnly;
		float opacity;
		float slopeStart;
		float slopeFull;
		float color[3];
	};
	BmdSnowSettings g_BmdSnow = {true, true, true, true, 0.72f, 0.18f, 0.72f, {0.90f, 0.95f, 1.00f}};
	DWORD g_BmdSnowLastRead = 0;

	void UpdateBmdSnowSettings()
	{
		const DWORD now = GetTickCount();
		if (g_BmdSnowLastRead != 0 && now - g_BmdSnowLastRead < 1000) return;
		g_BmdSnowLastRead = now;
		g_BmdSnow.enabled = GetPrivateProfileIntA("SnowMaterial", "Enabled", 1, ".\\config.ini") != 0;
		g_BmdSnow.nativeOnly = GetPrivateProfileIntA("SnowMaterial", "SnowNativeMapsOnly", 1, ".\\config.ini") != 0;
		g_BmdSnow.exposedOnly = GetPrivateProfileIntA("SnowMaterial", "ExposedOnly", 1, ".\\config.ini") != 0;
		g_BmdSnow.opaqueOnly = GetPrivateProfileIntA("SnowMaterial", "OpaqueOnly", 1, ".\\config.ini") != 0;
		char text[80];
		GetPrivateProfileStringA("SnowMaterial", "MaxOpacity", "0.72", text, sizeof(text), ".\\config.ini");
		g_BmdSnow.opacity = fminf(0.95f, fmaxf(0.f, (float)atof(text)));
		GetPrivateProfileStringA("SnowMaterial", "SlopeStart", "0.18", text, sizeof(text), ".\\config.ini");
		g_BmdSnow.slopeStart = fminf(0.90f, fmaxf(-0.25f, (float)atof(text)));
		GetPrivateProfileStringA("SnowMaterial", "SlopeFull", "0.72", text, sizeof(text), ".\\config.ini");
		g_BmdSnow.slopeFull = fminf(1.f, fmaxf(g_BmdSnow.slopeStart + 0.05f, (float)atof(text)));
		GetPrivateProfileStringA("SnowMaterial", "Color", "0.90,0.95,1.00", text, sizeof(text), ".\\config.ini");
		float r = g_BmdSnow.color[0], g = g_BmdSnow.color[1], b = g_BmdSnow.color[2];
		if (sscanf_s(text, "%f,%f,%f", &r, &g, &b) == 3)
		{
			g_BmdSnow.color[0] = fminf(1.5f, fmaxf(0.f, r));
			g_BmdSnow.color[1] = fminf(1.5f, fmaxf(0.f, g));
			g_BmdSnow.color[2] = fminf(1.5f, fmaxf(0.f, b));
		}
	}

	float BmdSnowCoverAt(const float* origin)
	{
		UpdateBmdSnowSettings();
		if (!g_BmdSnow.enabled || g_BmdSnowSuppressUI || !Hero) return 0.f;
		// Evita um passe extra de neve em modelos muito distantes da camera.
		const float dx = origin[0] - Hero->Object.Position[0];
		const float dy = origin[1] - Hero->Object.Position[1];
		if (dx * dx + dy * dy > 1200.f * 1200.f) return 0.f;
		if (g_BmdSnow.nativeOnly && World != WD_2DEVIAS && World != WD_57ICECITY &&
			World != WD_58ICECITY_BOSS && World != WD_62SANTA_TOWN) return 0.f;
		const float cover = GetServerSnowCoverVisual();
		if (cover <= 0.025f) return 0.f;
		if (g_BmdSnow.exposedOnly && World == WD_2DEVIAS)
		{
			const int tx = (int)(origin[0] / TERRAIN_SCALE);
			const int ty = (int)(origin[1] / TERRAIN_SCALE);
			if (tx < 0 || ty < 0 || tx >= TERRAIN_SIZE || ty >= TERRAIN_SIZE) return 0.f;
			const BYTE layer = TerrainMappingLayer1[ty * TERRAIN_SIZE + tx];
			if (layer == 3 || layer >= 10) return 0.f;
		}
		// O quadrado fazia a neve leve (inclusive a base de Devias) quase
		// desaparecer dos modelos. Mantem o acumulo linear e deixa a opacidade
		// maxima controlada por MaxOpacity, sem pintar o item inteiro de branco.
		return fminf(1.f, cover);
	}
}


vec4_t BoneQuaternion[MAX_BONES];
short  BoundingVertices[MAX_BONES];
vec3_t BoundingMin[MAX_BONES];
vec3_t BoundingMax[MAX_BONES];

float  BoneTransform[MAX_BONES][3][4];

vec3_t VertexTransform[MAX_MESH][MAX_VERTICES];
vec3_t NormalTransform[MAX_MESH][MAX_VERTICES];
float  IntensityTransform[MAX_MESH][MAX_VERTICES];
vec3_t LightTransform[MAX_MESH][MAX_VERTICES];

vec3_t RenderArrayVertices[MAX_VERTICES * 3];
vec4_t RenderArrayColors[MAX_VERTICES * 3];
vec2_t RenderArrayTexCoords[MAX_VERTICES * 3];

namespace
{
	int g_LegacyMeshArrayBatchDepth = 0;
	bool g_LegacyMeshBatchColorArrayEnabled = false;
	bool g_KundunMeshPassProfileActive = false;
	int g_KundunTextureCalls = 0;
	int g_KundunChromeCalls = 0;
	int g_KundunOtherCalls = 0;
	int g_KundunProfileVertices = 0;
	double g_KundunTextureMs = 0.0;
	double g_KundunChromeMs = 0.0;
	double g_KundunOtherMs = 0.0;
	double g_KundunTexturePrepareMs = 0.0;
	double g_KundunChromePrepareMs = 0.0;
	double g_KundunOtherPrepareMs = 0.0;
	int g_ProfileSnowCalls = 0;
	double g_ProfileSnowMs = 0.0;
 double g_ProfileBoneMs = 0.0, g_ProfileSkinMs = 0.0;
 int g_ProfileBoneCalls = 0, g_ProfileSkinCalls = 0;
 double g_ProfileShadowMs = 0.0;
 int g_ProfileShadowCalls = 0;
 double g_ProfileShadowDrawMs = 0.0;
 int g_ProfileShadowDrawBatches = 0;
 // Only sampled frames read the clock. RAII includes early returns.
 class CharacterStageTimer
 {
     bool active;
     double& elapsed;
     int& calls;
     std::chrono::steady_clock::time_point start;
 public:
     CharacterStageTimer(double& ms, int& count)
         : active(g_KundunMeshPassProfileActive), elapsed(ms), calls(count)
     {
         if (active) start = std::chrono::steady_clock::now();
     }
     ~CharacterStageTimer()
     {
         if (!active) return;
         elapsed += std::chrono::duration<double, std::milli>(
             std::chrono::steady_clock::now() - start).count();
         ++calls;
     }
 };

	unsigned int g_ExpandedPositionGeneration = 1;
}

namespace
{
    bool BindBaseVertexStream(int count,const vec3_t* positions,const vec2_t* uv,const vec4_t* colors,bool lit,bool glowStream=false);

    // Streaming is limited to indexed, uniformly colored chrome passes.
    // GL owns buffer storage until the current context is destroyed.
    bool BindGlowVertexStream(int count, const vec3_t* positions, const vec2_t* uv)
    {
        static const bool enabled = GetPrivateProfileIntA(
            "Renderer", "GpuGlowVertices", 1, ".\\config.ini") != 0;
        if (!enabled || count <= 0 || count > MAX_VERTICES * 3 ||
            !glGenBuffers || !glBindBuffer || !glBufferData ||
            !glGetBufferParameteriv || !glDeleteBuffers) return false;
        static HGLRC owner = NULL;
        static GLuint buffer = 0;
        static bool failed = false;
        static bool mappedFailed = false;
        static const bool mappedEnabled = GetPrivateProfileIntA(
            "Renderer", "GpuGlowMapped", 0, ".\\config.ini") != 0;
        // Append chrome and base vertices to the same stream instead of allocating
        // a new store for every chrome pass. Keep each toggle independent.
        if(!mappedEnabled)return BindBaseVertexStream(count,positions,uv,NULL,false,true);
        const HGLRC context = wglGetCurrentContext();
        if (!context) return false;
        if (owner != context)
        {
            owner = context;
            buffer = 0;
            failed = false;
            mappedFailed = false;
        }
        if (failed) return false;
        if (!buffer) glGenBuffers(1, &buffer);
        if (!buffer) { failed = true; return false; }
        struct StreamVertex { float x, y, z, u, v; };
        static_assert(sizeof(StreamVertex) == 5 * sizeof(float), "Unexpected glow vertex layout");
        const GLint bytes = count * sizeof(StreamVertex);
        bool mappedReady = false;
        if (mappedEnabled && !mappedFailed && glMapBufferRange && glUnmapBuffer)
        {
            glBindBuffer(GL_ARRAY_BUFFER, buffer);
            glBufferData(GL_ARRAY_BUFFER, bytes, NULL, GL_STREAM_DRAW);
            StreamVertex* destination = static_cast<StreamVertex*>(glMapBufferRange(
                GL_ARRAY_BUFFER, 0, bytes, GL_MAP_WRITE_BIT));
            if (destination)
            {
                for (int n = 0; n < count; ++n)
                {
                    destination[n].x = positions[n][0];
                    destination[n].y = positions[n][1];
                    destination[n].z = positions[n][2];
                    destination[n].u = uv[n][0];
                    destination[n].v = uv[n][1];
                }
                mappedReady = glUnmapBuffer(GL_ARRAY_BUFFER) == GL_TRUE;
            }
            if (!mappedReady) mappedFailed = true;
            // Fall back to the proven upload path if mapping/unmapping fails.
        }
        if (!mappedReady)
        {
            static StreamVertex staging[MAX_VERTICES * 3];
            for (int n = 0; n < count; ++n)
            {
                staging[n].x = positions[n][0];
                staging[n].y = positions[n][1];
                staging[n].z = positions[n][2];
                staging[n].u = uv[n][0];
                staging[n].v = uv[n][1];
            }
            glBindBuffer(GL_ARRAY_BUFFER, buffer);
            // Re-specification lets the driver retain storage still used by prior draws.
            glBufferData(GL_ARRAY_BUFFER, bytes, staging, GL_STREAM_DRAW);
            GLint allocated = 0;
            glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &allocated);
            if (allocated != bytes)
            {
                glBindBuffer(GL_ARRAY_BUFFER, 0);
                glDeleteBuffers(1, &buffer);
                buffer = 0;
                failed = true;
                return false;
            }
        }
        glVertexPointer(3, GL_FLOAT, sizeof(StreamVertex), NULL);
        glTexCoordPointer(2, GL_FLOAT, sizeof(StreamVertex),
            reinterpret_cast<const void*>(3 * sizeof(float)));
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        return true;
    }

    // Index topology is immutable; upload once per mesh/context.
    bool BindGlowIndexBuffer(Mesh_t* mesh)
    {
        static const bool enabled = GetPrivateProfileIntA(
            "Renderer", "GpuGlowIndices", 1, ".\\config.ini") != 0;
        if (!enabled || !glGenBuffers || !glBindBuffer || !glBufferData ||
            !glGetBufferParameteriv || !glDeleteBuffers) return false;
        HGLRC context = wglGetCurrentContext();
        if (!context) return false;
        if (mesh->GlowIndexContext != context)
        {
            // Never delete a name belonging to another GL context.
            mesh->GlowIndexBuffer = 0;
            mesh->GlowIndexContext = context;
        }
        if (!mesh->GlowIndexBuffer)
        {
            glGenBuffers(1, &mesh->GlowIndexBuffer);
            if (!mesh->GlowIndexBuffer) return false;
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->GlowIndexBuffer);
            const GLint bytes = mesh->ExpandedVertexCount * sizeof(unsigned int);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, bytes, mesh->GlowIndices, GL_STATIC_DRAW);
            GLint allocated = 0;
            glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &allocated);
            if (allocated != bytes)
            {
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
                glDeleteBuffers(1, &mesh->GlowIndexBuffer);
                mesh->GlowIndexBuffer = 0;
                return false;
            }
        }
        else glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->GlowIndexBuffer);
        return true;
    }

    // Append animated mesh vertices to a driver-owned stream. Orphan on wrap,
    // so CPU writes never overwrite vertices referenced by earlier draws.
    bool BindBaseVertexStream(int count,const vec3_t* positions,const vec2_t* uv,const vec4_t* colors,bool lit,bool glowStream)
    {
        static const bool enabled=GetPrivateProfileIntA("Renderer","GpuCrowdBase",1,".\\config.ini")!=0;
        if((!enabled && !glowStream) || count<=0 || count>MAX_VERTICES*3 || !glGenBuffers || !glBindBuffer ||
            !glBufferData || !glBufferSubData || !glGetBufferParameteriv || !glDeleteBuffers)return false;
        static HGLRC owner=NULL;
        static GLuint buffer=0;
        static int cursor=0;
        static bool failed=false;
        const HGLRC context=wglGetCurrentContext();
        if(!context)return false;
        if(owner!=context){owner=context;buffer=0;cursor=0;failed=false;}
        if(failed)return false;
        const int capacity=4*1024*1024;
        const int stride=(lit?9:5)*sizeof(float);
        const int bytes=count*stride;
        if(bytes>capacity)return false;
        bool allocate=!buffer;
        if(!buffer){glGenBuffers(1,&buffer);if(!buffer){failed=true;return false;}}
        glBindBuffer(GL_ARRAY_BUFFER,buffer);
        if(allocate || cursor>capacity-bytes){
            glBufferData(GL_ARRAY_BUFFER,capacity,NULL,GL_STREAM_DRAW);
            GLint allocated=0;glGetBufferParameteriv(GL_ARRAY_BUFFER,GL_BUFFER_SIZE,&allocated);
            if(allocated!=capacity){glBindBuffer(GL_ARRAY_BUFFER,0);glDeleteBuffers(1,&buffer);buffer=0;failed=true;return false;}
            cursor=0;
        }
        static float staging[MAX_VERTICES*3*9];
        for(int n=0;n<count;++n){
            float* v=staging+n*(lit?9:5);
            v[0]=positions[n][0];v[1]=positions[n][1];v[2]=positions[n][2];
            v[3]=uv[n][0];v[4]=uv[n][1];
            if(lit){v[5]=colors[n][0];v[6]=colors[n][1];v[7]=colors[n][2];v[8]=colors[n][3];}
        }
        glBufferSubData(GL_ARRAY_BUFFER,cursor,bytes,staging);
        glVertexPointer(3,GL_FLOAT,stride,reinterpret_cast<const void*>(static_cast<size_t>(cursor)));
        glTexCoordPointer(2,GL_FLOAT,stride,reinterpret_cast<const void*>(static_cast<size_t>(cursor+3*sizeof(float))));
        if(lit)glColorPointer(4,GL_FLOAT,stride,reinterpret_cast<const void*>(static_cast<size_t>(cursor+5*sizeof(float))));
        cursor+=bytes;
        glBindBuffer(GL_ARRAY_BUFFER,0);
        return true;
    }

    bool PreferSmallMeshClientArrays(int vertices,int indices)
    {
        static const bool enabled=GetPrivateProfileIntA("Renderer","CrowdSmallMeshClientArrays",1,".\\config.ini")!=0;
        static const bool intel=[](){const char* vendor=reinterpret_cast<const char*>(glGetString(GL_VENDOR));return vendor && strstr(vendor,"Intel")!=NULL;}();
        return MeshSubmissionPolicy::PreferClientArrays(intel,vertices,indices,enabled);
    }

    struct BaseTopologyCache { bool built=false; bool useful=false; IndexedMeshTopology::Topology topology; GLuint indexBuffer=0; HGLRC context=NULL; bool gpuFailed=false; };
    std::map<Mesh_t*,BaseTopologyCache> g_BaseTopology;
    BaseTopologyCache* PrepareIndexedBase(Mesh_t* mesh)
    {
        BaseTopologyCache& cache=g_BaseTopology[mesh];
        if(!cache.built){
            cache.built=true;
            cache.useful=IndexedMeshTopology::Build(mesh->Triangles,mesh->NumTriangles,
                mesh->NumVertices,mesh->NumNormals,mesh->NumTexCoords,cache.topology);
        }
        return cache.useful?&cache:NULL;
    }

    // The topology does not animate. Keep indices in driver-owned storage;
    // mixed GPU vertices/client indices otherwise require a new driver upload per mesh.
    bool BindBaseIndexBuffer(BaseTopologyCache& cache)
    {
        static const bool enabled=GetPrivateProfileIntA("Renderer","GpuCrowdIndices",1,".\\config.ini")!=0;
        if(!enabled || cache.topology.indices.empty() || !glGenBuffers || !glBindBuffer ||
           !glBufferData || !glGetBufferParameteriv || !glDeleteBuffers)return false;
        const HGLRC context=wglGetCurrentContext();if(!context)return false;
        if(cache.context!=context){cache.context=context;cache.indexBuffer=0;cache.gpuFailed=false;}
        if(cache.gpuFailed)return false;
        if(!cache.indexBuffer){
            glGenBuffers(1,&cache.indexBuffer);
            if(!cache.indexBuffer){cache.gpuFailed=true;return false;}
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,cache.indexBuffer);
            const GLint bytes=static_cast<GLint>(cache.topology.indices.size()*sizeof(unsigned int));
            glBufferData(GL_ELEMENT_ARRAY_BUFFER,bytes,cache.topology.indices.data(),GL_STATIC_DRAW);
            GLint actual=0;glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER,GL_BUFFER_SIZE,&actual);
            if(actual!=bytes){
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);glDeleteBuffers(1,&cache.indexBuffer);
                cache.indexBuffer=0;cache.gpuFailed=true;return false;
            }
        }else glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,cache.indexBuffer);
        return true;
    }

    bool PrepareIndexedGlow(Mesh_t* mesh)
    {
        if (mesh->GlowVertexCount != 0) return mesh->GlowVertexCount > 0;
        if (mesh->ExpandedVertexCount <= 0 || mesh->ExpandedVertexCount > MAX_VERTICES * 3 ||
            mesh->ExpandedVertexIndices == NULL || mesh->ExpandedNormalIndices == NULL)
            return false;
        for (int triangle = 0; triangle < mesh->NumTriangles; ++triangle)
        {
            if (mesh->Triangles[triangle].Polygon != 3)
            {
                mesh->GlowVertexCount = -1;
                return false;
            }
        }
        std::map<std::pair<int, int>, unsigned int> unique;
        std::vector<unsigned int> indices(mesh->ExpandedVertexCount);
        std::vector<int> corners;
        corners.reserve(mesh->ExpandedVertexCount);
        for (int corner = 0; corner < mesh->ExpandedVertexCount; ++corner)
        {
            const int vertex = mesh->ExpandedVertexIndices[corner];
            const int normal = mesh->ExpandedNormalIndices[corner];
            if (vertex < 0 || vertex >= mesh->NumVertices || normal < 0 || normal >= mesh->NumNormals)
            {
                mesh->GlowVertexCount = -1;
                return false;
            }
            const std::pair<int, int> key(vertex, normal);
            auto inserted = unique.emplace(key, static_cast<unsigned int>(corners.size()));
            if (inserted.second) corners.push_back(corner);
            indices[corner] = inserted.first->second;
        }
        if (corners.size() == indices.size())
        {
            mesh->GlowVertexCount = -1;
            return false;
        }
        mesh->GlowIndices = new unsigned int[indices.size()];
        mesh->GlowSourceCorners = new int[corners.size()];
        std::copy(indices.begin(), indices.end(), mesh->GlowIndices);
        std::copy(corners.begin(), corners.end(), mesh->GlowSourceCorners);
        mesh->GlowVertexCount = static_cast<int>(corners.size());
        return true;
    }
}

void GetCharacterStageProfile(double* boneMs, double* skinMs, int* boneCalls, int* skinCalls)
{
 *boneMs = g_ProfileBoneMs;
 *skinMs = g_ProfileSkinMs;
 *boneCalls = g_ProfileBoneCalls;
 *skinCalls = g_ProfileSkinCalls;
}

void GetCharacterShadowProfile(double* shadowMs, int* shadowCalls, double* drawMs, int* drawBatches)
{
 *shadowMs = g_ProfileShadowMs;
 *shadowCalls = g_ProfileShadowCalls;
 *drawMs = g_ProfileShadowDrawMs;
 *drawBatches = g_ProfileShadowDrawBatches;
}

void BeginKundunMeshPassProfile()
{
	g_KundunMeshPassProfileActive = true;
	g_KundunTextureCalls = g_KundunChromeCalls = g_KundunOtherCalls = 0;
	g_KundunProfileVertices = 0;
	g_KundunTextureMs = g_KundunChromeMs = g_KundunOtherMs = 0.0;
	g_KundunTexturePrepareMs = g_KundunChromePrepareMs = g_KundunOtherPrepareMs = 0.0;
	g_ProfileSnowCalls = 0;
	g_ProfileSnowMs = 0.0;
 g_ProfileBoneMs = g_ProfileSkinMs = 0.0;
 g_ProfileBoneCalls = g_ProfileSkinCalls = 0;
 g_ProfileShadowMs = 0.0;
 g_ProfileShadowCalls = 0;
 g_ProfileShadowDrawMs = 0.0;
 g_ProfileShadowDrawBatches = 0;
}

void EndKundunMeshPassProfile(int* textureCalls, int* chromeCalls,
	int* otherCalls, int* vertices, double* textureMs, double* chromeMs,
	double* otherMs, double* texturePrepareMs, double* chromePrepareMs,
	double* otherPrepareMs, int* snowCalls, double* snowMs)
{
	g_KundunMeshPassProfileActive = false;
	*textureCalls = g_KundunTextureCalls;
	*chromeCalls = g_KundunChromeCalls;
	*otherCalls = g_KundunOtherCalls;
	*vertices = g_KundunProfileVertices;
	*textureMs = g_KundunTextureMs;
	*chromeMs = g_KundunChromeMs;
	*otherMs = g_KundunOtherMs;
	*texturePrepareMs = g_KundunTexturePrepareMs;
	*chromePrepareMs = g_KundunChromePrepareMs;
	*otherPrepareMs = g_KundunOtherPrepareMs;
	if (snowCalls) *snowCalls = g_ProfileSnowCalls;
	if (snowMs) *snowMs = g_ProfileSnowMs;
}

// Foreign client-array draws (sprites, shadows and grass) restore these
// enables before returning to a mesh batch. Mesh pointers are set per draw.
void RestoreLegacyMeshArrayBatchState()
{
	if (g_LegacyMeshArrayBatchDepth == 0) return;
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	if (g_LegacyMeshBatchColorArrayEnabled) glEnableClientState(GL_COLOR_ARRAY);
	else glDisableClientState(GL_COLOR_ARRAY);
}

void BeginLegacyMeshArrayBatch()
{
	if (g_LegacyMeshArrayBatchDepth++ == 0)
	{
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glDisableClientState(GL_COLOR_ARRAY);
		g_LegacyMeshBatchColorArrayEnabled = false;
	}
}

void EndLegacyMeshArrayBatch()
{
	if (g_LegacyMeshArrayBatchDepth <= 0)
	{
		g_LegacyMeshArrayBatchDepth = 0;
		return;
	}
	if (--g_LegacyMeshArrayBatchDepth == 0)
	{
		if (g_LegacyMeshBatchColorArrayEnabled)
		{
			glDisableClientState(GL_COLOR_ARRAY);
			g_LegacyMeshBatchColorArrayEnabled = false;
		}
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glDisableClientState(GL_VERTEX_ARRAY);
	}
}

unsigned char ShadowBuffer[256 * 256];
int           ShadowBufferWidth = 256;
int           ShadowBufferHeight = 256;


vec3_t	g_vright;		// needs to be set to viewer's right in order for chrome to work
int		g_smodels_total = 1;				// cookie
float	g_chrome[MAX_VERTICES][2];	// texture coords for surface normals
int		g_chromeage[MAX_BONES];	// last time chrome vectors were updated
vec3_t	g_chromeup[MAX_BONES];		// chrome vector "up" in bone reference frames
vec3_t	g_chromeright[MAX_BONES];	// chrome vector "right" in bone reference frames

extern int EditFlag;
extern int WaterTextureNumber;

bool HighLight = true;
float BoneScale = 1.f;
bool  StopMotion = false;
float ParentMatrix[3][4];

static vec3_t LightVector = { 0.f, -0.1f, -0.8f };
static vec3_t LightVector2 = { 0.f, -0.5f, -0.8f };

void BMD::Animation(float(*BoneMatrix)[3][4], float AnimationFrame, float PriorFrame, unsigned short PriorAction, vec3_t Angle, vec3_t HeadAngle, bool Parent, bool Translate)
{
 CharacterStageTimer stageTimer(g_ProfileBoneMs, g_ProfileBoneCalls);
	if (NumActions <= 0)
		return;

	if (PriorAction >= NumActions)
		PriorAction = 0;
	if (CurrentAction >= NumActions)
		CurrentAction = 0;

	VectorCopy(Angle, BodyAngle);

	CurrentAnimation = AnimationFrame;
	CurrentAnimationFrame = (int)AnimationFrame;
	float s1 = (CurrentAnimation - CurrentAnimationFrame);
	float s2 = 1.f - s1;

	int PriorAnimationFrame = (int)PriorFrame;
	if (NumActions > 0)
	{
		if (PriorAnimationFrame < 0)
			PriorAnimationFrame = 0;
		if (CurrentAnimationFrame < 0)
			CurrentAnimationFrame = 0;
		if (PriorAnimationFrame >= Actions[PriorAction].NumAnimationKeys)
			PriorAnimationFrame = 0;
		if (CurrentAnimationFrame >= Actions[CurrentAction].NumAnimationKeys)
			CurrentAnimationFrame = 0;
	}

	// bones
	for (int i = 0; i < NumBones; i++)
	{
		Bone_t* b = &Bones[i];
		if (b->Dummy)
		{
			continue;
		}
		BoneMatrix_t* bm1 = &b->BoneMatrixes[PriorAction];
		BoneMatrix_t* bm2 = &b->BoneMatrixes[CurrentAction];
		vec4_t q1, q2;

		if (i == BoneHead)
		{
			vec3_t Angle1, Angle2;
			VectorCopy(bm1->Rotation[PriorAnimationFrame], Angle1);
			VectorCopy(bm2->Rotation[CurrentAnimationFrame], Angle2);

			float HeadAngleX = HeadAngle[0] / (180.f / Q_PI);
			float HeadAngleY = HeadAngle[1] / (180.f / Q_PI);
			Angle1[0] -= HeadAngleX;
			Angle2[0] -= HeadAngleX;
			Angle1[2] -= HeadAngleY;
			Angle2[2] -= HeadAngleY;
			AngleQuaternion(Angle1, q1);
			AngleQuaternion(Angle2, q2);
		}
		else
		{
			QuaternionCopy(bm1->Quaternion[PriorAnimationFrame], q1);
			QuaternionCopy(bm2->Quaternion[CurrentAnimationFrame], q2);
		}
		if (!QuaternionCompare(q1, q2))
		{
			QuaternionSlerp(q1, q2, s1, BoneQuaternion[i]);
		}
		else
		{
			QuaternionCopy(q1, BoneQuaternion[i]);
		}

		float Matrix[3][4];
		QuaternionMatrix(BoneQuaternion[i], Matrix);
		float* Position1 = bm1->Position[PriorAnimationFrame];
		float* Position2 = bm2->Position[CurrentAnimationFrame];

		if (i == 0 && (Actions[PriorAction].LockPositions || Actions[CurrentAction].LockPositions))
		{
			Matrix[0][3] = bm2->Position[0][0];
			Matrix[1][3] = bm2->Position[0][1];
			Matrix[2][3] = Position1[2] * s2 + Position2[2] * s1 + BodyHeight;
		}
		else
		{
			Matrix[0][3] = Position1[0] * s2 + Position2[0] * s1;
			Matrix[1][3] = Position1[1] * s2 + Position2[1] * s1;
			Matrix[2][3] = Position1[2] * s2 + Position2[2] * s1;
		}

		if (b->Parent == -1)
		{
			if (!Parent)
			{
				AngleMatrix(BodyAngle, ParentMatrix);

				if (Translate)
				{
					for (int y = 0; y < 3; ++y)
					{
						for (int x = 0; x < 3; ++x)
						{
							ParentMatrix[y][x] *= BodyScale;
						}
					}

					ParentMatrix[0][3] = BodyOrigin[0];
					ParentMatrix[1][3] = BodyOrigin[1];
					ParentMatrix[2][3] = BodyOrigin[2];
				}
			}
			R_ConcatTransforms(ParentMatrix, Matrix, BoneMatrix[i]);
		}
		else
		{
			R_ConcatTransforms(BoneMatrix[b->Parent], Matrix, BoneMatrix[i]);
		}
	}
}

bool BMD::PlayAnimation(float* AnimationFrame, float* PriorAnimationFrame, unsigned short* PriorAction, float Speed, vec3_t Origin, vec3_t Angle)
{
	bool Loop = true;

	if (AnimationFrame == NULL || PriorAnimationFrame == NULL || PriorAction == NULL || (NumActions > 0 && CurrentAction >= NumActions))
	{
		return Loop;
	}

	if (NumActions == 0 || Actions[CurrentAction].NumAnimationKeys <= 1)
	{
		return Loop;
	}

	int Temp = (int)*AnimationFrame;

	*AnimationFrame += timefac(Speed);

	if (Temp != (int)*AnimationFrame)
	{
		*PriorAction = CurrentAction;
		*PriorAnimationFrame = (float)Temp;
	}
	if (*AnimationFrame <= 0.f)
	{
		*AnimationFrame += ((float)Actions[CurrentAction].NumAnimationKeys - 1.f);
	}

	if (Actions[CurrentAction].Loop)
	{
		if (*AnimationFrame >= (float)Actions[CurrentAction].NumAnimationKeys)
		{
			*AnimationFrame = (float)Actions[CurrentAction].NumAnimationKeys - 0.01f;
			Loop = false;
		}
	}
	else
	{
		int Key;
		if (Actions[CurrentAction].LockPositions)
			Key = Actions[CurrentAction].NumAnimationKeys - 1;
		else
			Key = Actions[CurrentAction].NumAnimationKeys;

		float fTemp;

		if (SceneFlag == 4)
		{
			fTemp = *AnimationFrame + 2;
		}
		else if (gMapManager->currentMap == WD_39KANTURU_3RD && CurrentAction == MONSTER01_APEAR)
		{
			fTemp = *AnimationFrame + 1;
		}
		else
		{
			fTemp = *AnimationFrame;
		}

		if (fTemp >= (int)Key)
		{
			int Frame = (int)*AnimationFrame;
			*AnimationFrame = (float)(Frame % (Key)) + (*AnimationFrame - (float)Frame);
			Loop = false;
		}
	}
	CurrentAnimation = *AnimationFrame;
	CurrentAnimationFrame = (int)CurrentAnimation;
	return Loop;
}

void BMD::Transform(float(*BoneMatrix)[3][4], vec3_t BoundingBoxMin, vec3_t BoundingBoxMax, OBB_t* OBB, bool Translate, float _Scale)
{
 CharacterStageTimer stageTimer(g_ProfileSkinMs, g_ProfileSkinCalls);
	// VertexTransform is a shared legacy scratch buffer. A new transform means
	// every per-mesh expanded-position cache belongs to the previous pose.
	if (++g_ExpandedPositionGeneration == 0)
		g_ExpandedPositionGeneration = 1;

	vec3_t LightPosition;

	if (LightEnable)
	{
		vec3_t Position;

		float Matrix[3][4];
		if (HighLight)
		{
			Vector(1.3f, 0.f, 2.f, Position);
		}
		else if (gMapManager->InBattleCastle())
		{
			Vector(0.5f, -1.f, 1.f, Position);
			Vector(0.f, 0.f, -45.f, ShadowAngle);
		}
		else
		{
			Vector(0.f, -1.5f, 0.f, Position);
		}

		AngleMatrix(ShadowAngle, Matrix);
		VectorIRotate(Position, Matrix, LightPosition);
	}
	vec3_t BoundingMin;
	vec3_t BoundingMax;
	Vector(0.0, 0.0, 0.0, BoundingMin);
	Vector(0.0, 0.0, 0.0, BoundingMax);

#ifdef _DEBUG
#else
	if (EditFlag == 2)
#endif
	{
		Vector(999999.f, 999999.f, 999999.f, BoundingMin);
		Vector(-999999.f, -999999.f, -999999.f, BoundingMax);
	}
	for (int i = 0; i < NumMeshs; i++)
	{
		Mesh_t* m = &Meshs[i];
		for (int j = 0; j < m->NumVertices; j++)
		{
			Vertex_t* v = &m->Vertices[j];
			float* vp = VertexTransform[i][j];

			if (BoneScale == 1.f)
			{
#ifdef PBG_ADD_NEWCHAR_MONK_ITEM
				if (_Scale)
				{
					vec3_t Position;
					VectorCopy(v->Position, Position);
					VectorScale(Position, _Scale, Position);
					VectorTransform(Position, BoneMatrix[v->Node], vp);
				}
				else
#endif //PBG_ADD_NEWCHAR_MONK_ITEM
					VectorTransform(v->Position, BoneMatrix[v->Node], vp);
				if (Translate)
					VectorScale(vp, BodyScale, vp);
			}
			else
			{
				VectorRotate(v->Position, BoneMatrix[v->Node], vp);
				vp[0] = vp[0] * BoneScale + BoneMatrix[v->Node][0][3];
				vp[1] = vp[1] * BoneScale + BoneMatrix[v->Node][1][3];
				vp[2] = vp[2] * BoneScale + BoneMatrix[v->Node][2][3];
				if (Translate)
					VectorScale(vp, BodyScale, vp);
			}
#ifdef _DEBUG
#else
			if (EditFlag == 2)
#endif
			{
				for (int k = 0; k < 3; k++)
				{
					if (vp[k] < BoundingMin[k]) BoundingMin[k] = vp[k];
					if (vp[k] > BoundingMax[k]) BoundingMax[k] = vp[k];
				}
			}
			if (Translate)
				VectorAdd(vp, BodyOrigin, vp);
		}

		for (int j = 0; j < m->NumNormals; j++)
		{
			Normal_t* sn = &m->Normals[j];
			float* tn = NormalTransform[i][j];
			VectorRotate(sn->Normal, BoneMatrix[sn->Node], tn);
			if (LightEnable)
			{
				float Luminosity;
				Luminosity = DotProduct(tn, LightPosition) * 0.8f + 0.4f;

				if (Luminosity < 0.2f) Luminosity = 0.2f;
				static const bool naturalLight=GetPrivateProfileIntA("Renderer","NaturalLighting",1,".\\config.ini")!=0;
                if(naturalLight && SceneFlag==MAIN_SCENE && !g_BmdSnowSuppressUI)
                    Luminosity=NaturalSurfaceLight::Blend(Luminosity,DotProduct(tn,LightPosition),tn[2]);
                IntensityTransform[i][j] = Luminosity;
			}
		}
	}
	if (EditFlag == 2)
	{
		VectorCopy(BoundingMin, OBB->StartPos);
		OBB->XAxis[0] = (BoundingMax[0] - BoundingMin[0]);
		OBB->YAxis[1] = (BoundingMax[1] - BoundingMin[1]);
		OBB->ZAxis[2] = (BoundingMax[2] - BoundingMin[2]);
	}
	else
	{
		VectorCopy(BoundingBoxMin, OBB->StartPos);
		OBB->XAxis[0] = (BoundingBoxMax[0] - BoundingBoxMin[0]);
		OBB->YAxis[1] = (BoundingBoxMax[1] - BoundingBoxMin[1]);
		OBB->ZAxis[2] = (BoundingBoxMax[2] - BoundingBoxMin[2]);
	}
	fTransformedSize = max(max(BoundingMax[0] - BoundingMin[0], BoundingMax[1] - BoundingMin[1]), BoundingMax[2] - BoundingMin[2]);

	//fTransformedSize *= 0.3f;
	VectorAdd(OBB->StartPos, BodyOrigin, OBB->StartPos);
	OBB->XAxis[1] = 0.f;
	OBB->XAxis[2] = 0.f;
	OBB->YAxis[0] = 0.f;
	OBB->YAxis[2] = 0.f;
	OBB->ZAxis[0] = 0.f;
	OBB->ZAxis[1] = 0.f;
}

void BMD::TransformByObjectBone(vec3_t vResultPosition, OBJECT* pObject, int iBoneNumber, vec3_t vRelativePosition)
{
	if (iBoneNumber >= 0 && iBoneNumber < NumBones && pObject)
	{
		vec3_t vOutTemp;
		float(*TransformMatrix)[4];

		TransformMatrix = pObject->GetBoneTransform(iBoneNumber);

		if (TransformMatrix == NULL)
		{
			TransformMatrix = BoneTransform[iBoneNumber];
		}

		if (vRelativePosition)
		{
			VectorTransform(vRelativePosition, TransformMatrix, vOutTemp);
		}
		else
		{
			vOutTemp[0] = TransformMatrix[0][3];
			vOutTemp[1] = TransformMatrix[1][3];
			vOutTemp[2] = TransformMatrix[2][3];
		}

		VectorScale(vOutTemp, BodyScale, vOutTemp);
		VectorAdd(vOutTemp, pObject->Position, vResultPosition);
	}
	else
	{
		assert(!"Empty Bone");
	}
}

void BMD::TransformByBoneMatrix(vec3_t vResultPosition, float(*BoneMatrix)[4], vec3_t vWorldPosition, vec3_t vRelativePosition)
{
	if (BoneMatrix == NULL)
	{
		assert(!"Empty Matrix");
		return;
	}

	vec3_t vTemp;
	if (vRelativePosition == NULL)
	{
		vTemp[0] = BoneMatrix[0][3];
		vTemp[1] = BoneMatrix[1][3];
		vTemp[2] = BoneMatrix[2][3];
	}
	else
	{
		VectorTransform(vRelativePosition, BoneMatrix, vTemp);
	}
	if (vWorldPosition != NULL)
	{
		VectorScale(vTemp, BodyScale, vTemp);
		VectorAdd(vTemp, vWorldPosition, vResultPosition);
	}
	else
	{
		VectorScale(vTemp, BodyScale, vResultPosition);
	}
}

void BMD::TransformPosition(float(*Matrix)[4], vec3_t Position, vec3_t WorldPosition, bool Translate)
{
	if (Translate)
	{
		vec3_t p;
		VectorTransform(Position, Matrix, p);
		VectorScale(p, BodyScale, p);
		VectorAdd(p, BodyOrigin, WorldPosition);
	}
	else
	{
		VectorTransform(Position, Matrix, WorldPosition);
	}
}

void BMD::RotationPosition(float(*Matrix)[4], vec3_t Position, vec3_t WorldPosition)
{
	vec3_t p;
	VectorRotate(Position, Matrix, p);
	VectorScale(p, BodyScale, WorldPosition);

	for (int i = 0; i < 3; i++)
	{
		for (int j = 0; j < 4; j++)
		{
			ParentMatrix[i][j] = Matrix[i][j];
		}
	}
}


void BMD::AnimationTransformWithAttachHighModel_usingGlobalTM(OBJECT* oHighHierarchyModel, BMD* bmdHighHierarchyModel, int iBoneNumberHighHierarchyModel, vec3_t& vOutPosHighHiearachyModelBone, vec3_t* arrOutSetfAllBonePositions, bool bApplyTMtoVertices)
{
	if (NumBones < 1) return;
	if (NumBones > MAX_BONES) return;

	vec34_t arrBonesTMLocal[MAX_BONES]; // Bounded scratch; no per-call heap allocation.

	vec34_t		tmBoneHierarchicalObject;

	vec3_t		Temp, v3Position;
	OBB_t		OBB;

	Vector(0.0f, 0.0f, 0.0f, Temp);

	memset(arrBonesTMLocal, 0, sizeof(vec34_t) * NumBones);
	memset(tmBoneHierarchicalObject, 0, sizeof(vec34_t));

	memcpy(tmBoneHierarchicalObject, oHighHierarchyModel->BoneTransform[iBoneNumberHighHierarchyModel], sizeof(vec34_t));
	BodyScale = oHighHierarchyModel->Scale;

	tmBoneHierarchicalObject[0][3] = tmBoneHierarchicalObject[0][3] * BodyScale;
	tmBoneHierarchicalObject[1][3] = tmBoneHierarchicalObject[1][3] * BodyScale;
	tmBoneHierarchicalObject[2][3] = tmBoneHierarchicalObject[2][3] * BodyScale;

	if (NULL != vOutPosHighHiearachyModelBone)
	{
		Vector(tmBoneHierarchicalObject[0][3], tmBoneHierarchicalObject[1][3], tmBoneHierarchicalObject[2][3],
			vOutPosHighHiearachyModelBone);
	}

	VectorCopy(oHighHierarchyModel->Position, v3Position);

	Animation(arrBonesTMLocal, 0, 0, 0, Temp, Temp, false, false);

	for (int i_ = 0; i_ < NumBones; ++i_)
	{
		R_ConcatTransforms(tmBoneHierarchicalObject, arrBonesTMLocal[i_], BoneTransform[i_]);
		BoneTransform[i_][0][3] = BoneTransform[i_][0][3] + v3Position[0];
		BoneTransform[i_][1][3] = BoneTransform[i_][1][3] + v3Position[1];
		BoneTransform[i_][2][3] = BoneTransform[i_][2][3] + v3Position[2];

		Vector(BoneTransform[i_][0][3],
			BoneTransform[i_][1][3],
			BoneTransform[i_][2][3],
			arrOutSetfAllBonePositions[i_]);
	}

	if (true == bApplyTMtoVertices)
	{
		Transform(BoneTransform, Temp, Temp, &OBB, false);
	}

}

void BMD::AnimationTransformWithAttachHighModel(OBJECT* oHighHierarchyModel, BMD* bmdHighHierarchyModel, int iBoneNumberHighHierarchyModel, vec3_t& vOutPosHighHiearachyModelBone, vec3_t* arrOutSetfAllBonePositions)
{
	if (NumBones < 1)
		return;
	if (NumBones > MAX_BONES)
		return;

	vec34_t arrBonesTMLocal[MAX_BONES];
	vec34_t arrBonesTMLocalResult[MAX_BONES];
	vec34_t	tmBoneHierarchicalObject;
	vec3_t	Temp, v3Position;

	Vector(0.0f, 0.0f, 0.0f, Temp);


	memset(arrBonesTMLocalResult, 0, sizeof(vec34_t) * NumBones);
	memset(arrBonesTMLocal, 0, sizeof(vec34_t) * NumBones);

	memset(tmBoneHierarchicalObject, 0, sizeof(vec34_t));

	memcpy(tmBoneHierarchicalObject, oHighHierarchyModel->BoneTransform[iBoneNumberHighHierarchyModel], sizeof(vec34_t));

	BodyScale = oHighHierarchyModel->Scale;

	tmBoneHierarchicalObject[0][3] = tmBoneHierarchicalObject[0][3] * BodyScale;
	tmBoneHierarchicalObject[1][3] = tmBoneHierarchicalObject[1][3] * BodyScale;
	tmBoneHierarchicalObject[2][3] = tmBoneHierarchicalObject[2][3] * BodyScale;

	if (NULL != vOutPosHighHiearachyModelBone)
	{
		Vector(tmBoneHierarchicalObject[0][3], tmBoneHierarchicalObject[1][3], tmBoneHierarchicalObject[2][3],
			vOutPosHighHiearachyModelBone);
	}

	VectorCopy(oHighHierarchyModel->Position, v3Position);

	Animation(arrBonesTMLocal, 0, 0, 0, Temp, Temp, false, false);
	for (int i_ = 0; i_ < NumBones; ++i_)
	{
		R_ConcatTransforms(tmBoneHierarchicalObject, arrBonesTMLocal[i_], arrBonesTMLocalResult[i_]);
		arrBonesTMLocalResult[i_][0][3] = arrBonesTMLocalResult[i_][0][3] + v3Position[0];
		arrBonesTMLocalResult[i_][1][3] = arrBonesTMLocalResult[i_][1][3] + v3Position[1];
		arrBonesTMLocalResult[i_][2][3] = arrBonesTMLocalResult[i_][2][3] + v3Position[2];

		Vector(arrBonesTMLocalResult[i_][0][3], arrBonesTMLocalResult[i_][1][3], arrBonesTMLocalResult[i_][2][3], arrOutSetfAllBonePositions[i_]);
	}

}

void BMD::AnimationTransformOnlySelf(vec3_t* arrOutSetfAllBonePositions, const OBJECT* oSelf)
{
	if (NumBones < 1)
		return;

	if (NumBones > MAX_BONES)
		return;

	vec34_t arrBonesTMLocal[MAX_BONES];

	vec3_t		Temp;

	Vector(0.0f, 0.0f, 0.0f, Temp);

	memset(arrBonesTMLocal, 0, sizeof(vec34_t) * NumBones);

	Animation(arrBonesTMLocal, oSelf->AnimationFrame, oSelf->PriorAnimationFrame, oSelf->PriorAction, (const_cast<OBJECT*>(oSelf))->Angle, Temp, false, true);

	for (int i_ = 0; i_ < NumBones; ++i_)
	{
		Vector(arrBonesTMLocal[i_][0][3], arrBonesTMLocal[i_][1][3], arrBonesTMLocal[i_][2][3], arrOutSetfAllBonePositions[i_]);
	}
}

void BMD::AnimationTransformOnlySelf(vec3_t* arrOutSetfAllBonePositions,
	const vec3_t& v3Angle,
	const vec3_t& v3Position,
	const float& fScale,
	OBJECT* oRefAnimation,
	const float fFrameArea,
	const float fWeight)
{
	if (NumBones < 1)
		return;

	if (NumBones > MAX_BONES)
		return;

	vec34_t arrBonesTMLocal[MAX_BONES];
	vec3_t v3RootAngle, v3RootPosition;
	float fRootScale;
	vec3_t Temp;

	fRootScale = const_cast<float&>(fScale);

	v3RootAngle[0] = v3Angle[0];
	v3RootAngle[1] = v3Angle[1];
	v3RootAngle[2] = v3Angle[2];

	v3RootPosition[0] = v3Position[0];
	v3RootPosition[1] = v3Position[1];
	v3RootPosition[2] = v3Position[2];

	Vector(0.0f, 0.0f, 0.0f, Temp);

	memset(arrBonesTMLocal, 0, sizeof(vec34_t) * NumBones);

	if (NULL == oRefAnimation)
	{
		Animation(arrBonesTMLocal, 0, 0, 0, v3RootAngle, Temp, false, true);
	}
	else
	{
		float fAnimationFrame = oRefAnimation->AnimationFrame;
		float fPiriorAnimationFrame = oRefAnimation->PriorAnimationFrame;

		unsigned short	iPiriorAction = oRefAnimation->PriorAction;

		if (fWeight >= 0.0f && fFrameArea > 0.0f)
		{
			float fAnimationFrameStart = fAnimationFrame - fFrameArea;
			float fAnimationFrameEnd = fAnimationFrame;
			LInterpolationF(fAnimationFrame, fAnimationFrameStart, fAnimationFrameEnd, fWeight);
		}

		Animation(arrBonesTMLocal, fAnimationFrame, fPiriorAnimationFrame, iPiriorAction, v3RootAngle, Temp, false, true);
	}

	vec3_t	v3RelatePos;
	Vector(1.0f, 1.0f, 1.0f, v3RelatePos);
	for (int i_ = 0; i_ < NumBones; ++i_)
	{
		Vector(arrBonesTMLocal[i_][0][3],
			arrBonesTMLocal[i_][1][3],
			arrBonesTMLocal[i_][2][3],
			arrOutSetfAllBonePositions[i_]);
	}

}

void BMD::Chrome(float* pchrome, int bone, vec3_t normal)
{
	Vector(0.f, 0.f, 1.f, g_vright);

	float n;

	//if (g_chromeage[bone] != g_smodels_total)
	{
		// calculate vectors from the viewer to the bone. This roughly adjusts for position
		vec3_t chromeupvec;		// g_chrome t vector in world reference frame
		vec3_t chromerightvec;	// g_chrome s vector in world reference frame
		vec3_t tmp;				// vector pointing at bone in world reference frame
		VectorScale(BodyOrigin, -1, tmp);
		//tmp[0] += BoneMatrix[bone][0][3];
		//tmp[1] += BoneMatrix[bone][1][3];
		//tmp[2] += BoneMatrix[bone][2][3];
		//tmp[0] += LinkBoneMatrix[0][3];
		//tmp[1] += LinkBoneMatrix[1][3];
		//tmp[2] += LinkBoneMatrix[2][3];
		VectorNormalize(tmp);
		CrossProduct(tmp, g_vright, chromeupvec);
		VectorNormalize(chromeupvec);
		CrossProduct(tmp, chromeupvec, chromerightvec);
		VectorNormalize(chromerightvec);

		//VectorIRotate( chromeupvec, BoneMatrix[bone], g_chromeup[bone] );
		//VectorIRotate( chromerightvec, BoneMatrix[bone], g_chromeright[bone] );
		//VectorIRotate( chromeupvec, LinkBoneMatrix, g_chromeup[bone] );
		//VectorIRotate( chromerightvec, LinkBoneMatrix, g_chromeright[bone] );

		g_chromeage[bone] = g_smodels_total;
	}

	// calc s coord
	n = DotProduct(normal, g_chromeright[bone]);
	pchrome[0] = (n + 1.f); // FIX: make this a float

	// calc t coord
	n = DotProduct(normal, g_chromeup[bone]);
	pchrome[1] = (n + 1.f); // FIX: make this a float
}

void BMD::Lighting(float* pLight, Light_t* lp, vec3_t Position, vec3_t Normal)
{
	vec3_t Light;
	VectorSubtract(lp->Position, Position, Light);
	float Length = sqrtf(Light[0] * Light[0] + Light[1] * Light[1] + Light[2] * Light[2]);

	float LightCos = (DotProduct(Normal, Light) / Length) * 0.8f + 0.3f;

	if (Length > lp->Range)
		LightCos -= (Length - lp->Range) * 0.01f;

	if (LightCos < 0.f)
		LightCos = 0.f;

	pLight[0] += LightCos * lp->Color[0];
	pLight[1] += LightCos * lp->Color[1];
	pLight[2] += LightCos * lp->Color[2];
}

///////////////////////////////////////////////////////////////////////////////
// light map
///////////////////////////////////////////////////////////////////////////////

#define AXIS_X  0
#define AXIS_Y  1
#define AXIS_Z  2

float SubPixel = 16.f;

void SmoothBitmap(int Width, int Height, unsigned char* Buffer)
{
	int RowStride = Width * 3;
	for (int i = 1; i < Height - 1; i++)
	{
		for (int j = 1; j < Width - 1; j++)
		{
			int Index = (i * Width + j) * 3;
			for (int k = 0; k < 3; k++)
			{
				Buffer[Index] = (Buffer[Index - RowStride - 3] + Buffer[Index - RowStride] + Buffer[Index - RowStride + 3] +
					Buffer[Index - 3] + Buffer[Index + 3] +
					Buffer[Index + RowStride - 3] + Buffer[Index + RowStride] + Buffer[Index + RowStride + 3]) / 8;
				Index++;
			}
		}
	}
}

bool BMD::CollisionDetectLineToMesh(vec3_t Position, vec3_t Target, bool Collision, int Mesh, int Triangle)
{
	int i, j;
	for (i = 0; i < NumMeshs; i++)
	{
		Mesh_t* m = &Meshs[i];

		for (j = 0; j < m->NumTriangles; j++)
		{
			if (i == Mesh && j == Triangle)
				continue;

			Triangle_t* tp = &m->Triangles[j];
			float* vp1 = VertexTransform[i][tp->VertexIndex[0]];
			float* vp2 = VertexTransform[i][tp->VertexIndex[1]];
			float* vp3 = VertexTransform[i][tp->VertexIndex[2]];
			float* vp4 = VertexTransform[i][tp->VertexIndex[3]];

			vec3_t Normal;
			FaceNormalize(vp1, vp2, vp3, Normal);
			bool success = CollisionDetectLineToFace(Position, Target, tp->Polygon, vp1, vp2, vp3, vp4, Normal, Collision);
			if (success == true) return true;
		}
	}
	return false;
}

void BMD::CreateLightMapSurface(Light_t* lp, Mesh_t* m, int i, int j, int MapWidth, int MapHeight, int MapWidthMax, int MapHeightMax, vec3_t BoundingMin, vec3_t BoundingMax, int Axis)
{
	int k, l;
	Triangle_t* tp = &m->Triangles[j];
	float* np = NormalTransform[i][tp->NormalIndex[0]];
	float* vp = VertexTransform[i][tp->VertexIndex[0]];
	float d = -DotProduct(vp, np);

	Bitmap_t* lmp = &LightMaps[NumLightMaps];
	if (lmp->Buffer == NULL)
	{
		lmp->Width = MapWidthMax;
		lmp->Height = MapHeightMax;
		int BufferBytes = lmp->Width * lmp->Height * 3;
		lmp->Buffer = new unsigned char[BufferBytes];
		memset(lmp->Buffer, 0, BufferBytes);
	}

	for (k = 0; k < MapHeight; k++)
	{
		for (l = 0; l < MapWidth; l++)
		{
			vec3_t p;
			Vector(0.f, 0.f, 0.f, p);
			switch (Axis)
			{
			case AXIS_Z:
				p[0] = BoundingMin[0] + l * SubPixel;
				p[1] = BoundingMin[1] + k * SubPixel;
				if (p[0] >= BoundingMax[0]) p[0] = BoundingMax[0];
				if (p[1] >= BoundingMax[1]) p[1] = BoundingMax[1];
				p[2] = (np[0] * p[0] + np[1] * p[1] + d) / -np[2];
				break;
			case AXIS_Y:
				p[0] = BoundingMin[0] + (float)l * SubPixel;
				p[2] = BoundingMin[2] + (float)k * SubPixel;
				if (p[0] >= BoundingMax[0]) p[0] = BoundingMax[0];
				if (p[2] >= BoundingMax[2]) p[2] = BoundingMax[2];
				p[1] = (np[0] * p[0] + np[2] * p[2] + d) / -np[1];
				break;
			case AXIS_X:
				p[2] = BoundingMin[2] + l * SubPixel;
				p[1] = BoundingMin[1] + k * SubPixel;
				if (p[2] >= BoundingMax[2]) p[2] = BoundingMax[2];
				if (p[1] >= BoundingMax[1]) p[1] = BoundingMax[1];
				p[0] = (np[2] * p[2] + np[1] * p[1] + d) / -np[0];
				break;
			}
			vec3_t Direction;
			VectorSubtract(p, lp->Position, Direction);
			VectorNormalize(Direction);
			VectorSubtract(p, Direction, p);
			bool success = CollisionDetectLineToMesh(lp->Position, p, true, i, j);
			/*if(success == true)
			{
				DisableTexture();
				glBegin(GL_LINES);
				glColor3fv(lp->Color);
				glVertex3fv(p);
				glVertex3fv(lp->Position);
				glEnd();
			}*/
			if (success == false)
			{
				unsigned char* Bitmap = &lmp->Buffer[(k * MapWidthMax + l) * 3];
				vec3_t Light;
				Vector(0.f, 0.f, 0.f, Light);
				Lighting(Light, lp, p, np);
				for (int c = 0; c < 3; c++)
				{
					int Color;
					Color = Bitmap[c];
					Color += (unsigned char)(Light[c] * 255.f);
					if (Color > 255) Color = 255;
					Bitmap[c] = Color;
				}
			}
		}
	}
}

void BMD::CreateLightMaps()
{
}

void BMD::BindLightMaps()
{
	if (LightMapEnable == true)
		return;

	for (int i = 0; i < NumLightMaps; i++)
	{
		Bitmap_t* lmp = &LightMaps[i];
		if (lmp->Buffer != NULL)
		{
			SmoothBitmap(lmp->Width, lmp->Height, lmp->Buffer);
			SmoothBitmap(lmp->Width, lmp->Height, lmp->Buffer);

			glBindTexture(GL_TEXTURE_2D, i + IndexLightMap);
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, lmp->Width, lmp->Height, 0, GL_RGB, GL_UNSIGNED_BYTE, lmp->Buffer);
		}
	}
	LightMapEnable = true;
}

void BMD::ReleaseLightMaps()
{
	if (LightMapEnable == false)
		return;

	for (int i = 0; i < NumLightMaps; i++)
	{
		Bitmap_t* lmp = &LightMaps[i];
		if (lmp->Buffer != NULL)
		{
			delete lmp->Buffer;
			lmp->Buffer = NULL;
		}
	}
	LightMapEnable = false;
}

void BMD::BeginRender(float Alpha)
{
	glPushMatrix();
}

void BMD::EndRender()
{
	glPopMatrix();
}

bool BMD::runtime_linkaction(PART_t* f, OBJECT* pObject)
{
	return false;
	if (unknown != 0 && commandLua != NULL)
	{
		if (commandLua->RenderLinkAction(f, pObject))
		{
			return true;
		}
	}
	return false;
}

bool BMD::runtime_render(OBJECT* pObject, float Alpha, int RenderType, BMD* oHighHierarchyModel)
{
	return false;
	if (unknown != 0 && commandLua != NULL)
	{
		if (commandLua->RenderPartObjectBody(this, pObject, Alpha, RenderType))
		{
			return true;
		}
	}

	return false;
}

bool BMD::runtime_render_effect(OBJECT* pObject, float Alpha, int RenderType, int Texture, BMD* oHighHierarchyModel)
{
	return false;
	if (unknown != 0 && commandLua != NULL)
	{
		if (commandLua->RenderEffectObjectBody(this, pObject, Alpha, RenderType, Texture))
		{
			return true;
		}
	}
	return false;
}

void BMD::RenderMeshEffect(int i, int iType, int iSubType, vec3_t Angle, VOID* obj)
{
	if (i >= NumMeshs || i < 0)
		return;

	Mesh_t* m = &Meshs[i];
	if (m->NumTriangles <= 0)
		return;

	vec3_t angle, Light;
	int iEffectCount = 0;

	Vector(0.f, 0.f, 0.f, angle);
	Vector(1.f, 1.f, 1.f, Light);

	for (int j = 0; j < m->NumTriangles; j++)
	{
		Triangle_t* tp = &m->Triangles[j];
		for (int k = 0; k < tp->Polygon; k++)
		{
			int vi = tp->VertexIndex[k];

			switch (iType)
			{
			case MODEL_STONE_COFFIN:
				if (iSubType == 0)
				{
					if ((rand() % 2) == 0)
					{
						CreateEffectSync(MODEL_STONE_COFFIN + 1, VertexTransform[i][vi], angle, Light);
					}
					if ((rand() % 10) == 0)
					{
						CreateEffectSync(MODEL_STONE_COFFIN, VertexTransform[i][vi], angle, Light);
					}
				}
				else if (iSubType == 1)
				{
					CreateEffectSync(MODEL_STONE_COFFIN + 1, VertexTransform[i][vi], angle, Light, 2);
				}
				else if (iSubType == 2)
				{
					CreateEffectSync(MODEL_STONE_COFFIN + 1, VertexTransform[i][vi], angle, Light, 3);
				}
				else if (iSubType == 3)
				{
					CreateEffectSync(MODEL_STONE_COFFIN + rand() % 2, VertexTransform[i][vi], angle, Light, 4);
				}
				break;
			case MODEL_GATE:
				if (iSubType == 1)
				{
					Vector(0.2f, 0.2f, 0.2f, Light);
					if ((rand() % 5) == 0)
					{
						CreateEffectSync(MODEL_GATE + 1, VertexTransform[i][vi], angle, Light, 2);
					}
					if ((rand() % 10) == 0)
					{
						CreateEffectSync(MODEL_GATE, VertexTransform[i][vi], angle, Light, 2);
					}
				}
				else if (iSubType == 0)
				{
					Vector(0.2f, 0.2f, 0.2f, Light);
					if ((rand() % 12) == 0)
					{
						CreateEffectSync(MODEL_GATE + 1, VertexTransform[i][vi], angle, Light);
					}
					if ((rand() % 50) == 0)
					{
						CreateEffectSync(MODEL_GATE, VertexTransform[i][vi], angle, Light);
					}
				}
				break;
			case MODEL_BIG_STONE_PART1:
				if ((rand() % 3) == 0)
				{
					CreateEffectSync(MODEL_BIG_STONE_PART1 + rand() % 2, VertexTransform[i][vi], angle, Light, 1);
				}
				break;

			case MODEL_BIG_STONE_PART2:
				if ((rand() % 3) == 0)
				{
					CreateEffectSync(MODEL_BIG_STONE_PART1 + rand() % 2, VertexTransform[i][vi], angle, Light);
				}
				break;

			case MODEL_WALL_PART1:
				if ((rand() % 3) == 0)
				{
					CreateEffectSync(MODEL_WALL_PART1 + rand() % 2, VertexTransform[i][vi], angle, Light);
				}
				break;

			case MODEL_GATE_PART1:
				Vector(0.2f, 0.2f, 0.2f, Light);
				if ((rand() % 12) == 0)
				{
					CreateEffectSync(MODEL_GATE_PART1 + 1, VertexTransform[i][vi], angle, Light);
				}
				if ((rand() % 40) == 0)
				{
					CreateEffectSync(MODEL_GATE_PART1, VertexTransform[i][vi], angle, Light);
				}
				if ((rand() % 40) == 0)
				{
					CreateEffectSync(MODEL_GATE_PART1 + 2, VertexTransform[i][vi], angle, Light);
				}
				break;
			case MODEL_GOLEM_STONE:
				if ((rand() % 45) == 0 && iEffectCount < 20)
				{
					if (iSubType == 0)
					{
						CreateEffectSync(MODEL_GOLEM_STONE, VertexTransform[i][vi], angle, Light);
					}
					else if (iSubType == 1)
					{
						CreateEffectSync(MODEL_BIG_STONE_PART1, VertexTransform[i][vi], angle, Light, 2);
						CreateEffectSync(MODEL_BIG_STONE_PART2, VertexTransform[i][vi], angle, Light, 2);
					}
					iEffectCount++;
				}
				break;
			case MODEL_SKIN_SHELL:
				if ((rand() % 8) == 0)
				{
					CreateEffectSync(MODEL_SKIN_SHELL, VertexTransform[i][vi], angle, Light, iSubType);
				}
				break;
			case BITMAP_LIGHT:
				Vector(0.08f, 0.08f, 0.08f, Light);
				if (iSubType == 0)
				{
					CreateSprite(BITMAP_LIGHT, VertexTransform[i][vi], BodyScale, Light, NULL);
				}
				else if (iSubType == 1)
				{
					Vector(1.f, 0.8f, 0.2f, Light);
					if ((j % 22) == 0)
					{
						OBJECT* o = (OBJECT*)obj;
						angle[0] = -(float)(rand() % 90);
						angle[1] = 0.f;
						angle[2] = Angle[2] + (float)(rand() % 120 - 60);
						CreateJointSync(BITMAP_JOINT_SPIRIT, VertexTransform[i][vi], o->Position, angle, 13, o, 20.f, 0, 0);
					}
				}
				break;
			case BITMAP_BUBBLE:
				Vector(1.f, 1.f, 1.f, Light);
				if ((rand() % 30) == 0)
				{
					CreateParticleSync(BITMAP_BUBBLE, VertexTransform[i][vi], angle, Light, 2);
				}
				break;
			}
		}
	}
}

void BMD::RenderMesh(int i, int RenderFlag, float Alpha, int BlendMesh, float BlendMeshLight, float BlendMeshTexCoordU, float BlendMeshTexCoordV, int MeshTexture)
{
	if (i >= 0 && i < NumMeshs)
	{
		Mesh_t* m = &Meshs[i];

		if (m->NumTriangles)
		{
			int Texture = IndexTexture[m->Texture];
            if(Texture!=BITMAP_HIDE && CrowdMeshBudget::Skip(this,i,RenderFlag,Alpha,m->NumTriangles))return;

			if (RenderFlag != RENDER_TEXTURE && (Texture == BITMAP_SKIN
				|| Texture == BITMAP_HQSKIN
				|| Texture == BITMAP_HQSKIN2
				|| Texture == BITMAP_HQSKIN3
				|| Texture == BITMAP_HAIR
				|| Texture == BITMAP_HQHAIR)
				)
			{
				return;
			}

			//if ((Texture == BITMAP_SKIN
			//	|| Texture == BITMAP_HQSKIN
			//	|| Texture == BITMAP_HQSKIN2
			//	|| Texture == BITMAP_HQSKIN3
			//	|| Texture == BITMAP_HAIR
			//	|| Texture == BITMAP_HQHAIR)
			//	)
			//{
			//	g_ConsoleDebug->Write(5, "[BITMAP: %d][SKIN: %d]", Texture, Skin);
			//}

			if (Texture != BITMAP_HIDE)
			{
				switch (Texture)
				{
				case BITMAP_SKIN:
					if (HideSkin == true)
						return;
					Texture = Skin + BITMAP_SKIN;
					break;
				case BITMAP_HQSKIN:
					if (HideSkin == true)
						return;
					Texture = Skin + BITMAP_HQSKIN;
					break;
				case BITMAP_HQSKIN2:
					if (HideSkin == true)
						return;
					Texture = Skin + BITMAP_HQSKIN2;
					break;
				case BITMAP_HQSKIN3:
					if (HideSkin == true)
						return;
					Texture = Skin + BITMAP_HQSKIN3;
					break;
				case BITMAP_WATER:
					if (HideSkin == true)
						return;
					Texture = (int)WaterTextureNumber + BITMAP_WATER;
					break;
				case BITMAP_HAIR:
					if (HideSkin == true)
						return;
					Texture = Skin + BITMAP_HAIR;
					break;
				case BITMAP_HQHAIR:
					if (HideSkin == true)
						return;
					Texture = Skin + BITMAP_HQHAIR;
					break;
				}

				if (MeshTexture != -1)
					Texture = MeshTexture;

				if (Texture != -1)
				{
					if (LightTexture[m->Texture] != 0 && (HideSkin || RenderFlag != RENDER_TEXTURE))
					{
						return;
					}

					BITMAP_t* pBitmap = Bitmaps.GetTexture(Texture);

					bool EnableWave = false;
					int streamMesh = StreamMesh;

					if (m->m_csTScript && m->m_csTScript->getStreamMesh())
						streamMesh = i;

					if ((i == BlendMesh || i == streamMesh) && (BlendMeshTexCoordU != 0.0 || BlendMeshTexCoordV != 0.0))
						EnableWave = true;

					bool EnableLight = LightEnable;

					if (i == StreamMesh)
					{
						glColor3fv(BodyLight);
						EnableLight = false;
					}

					int renderFlags = RenderFlag;

					if (RenderFlag & RENDER_COLOR)
					{
						renderFlags = RENDER_COLOR;
						if (RenderFlag & RENDER_BRIGHT)
						{
							EnableAlphaBlend();
						}
						else if (RenderFlag & RENDER_DARK)
						{
							EnableAlphaBlendMinus();
						}
						else
						{
							DisableAlphaBlend();
						}
						if (RenderFlag & RENDER_NODEPTH)
							DisableDepthTest();
						DisableTexture(false);

						if (Alpha < 0.99)
						{
							EnableAlphaTest(true);
							glColor4f(BodyLight[0], BodyLight[1], BodyLight[2], Alpha);
						}
						else
						{
							Alpha = 1.f;
							glColor3fv(BodyLight);
						}
					}
					else if (RenderFlag & RENDER_CHROME
						|| RenderFlag & RENDER_METAL
						|| RenderFlag & RENDER_CHROME2
						|| RenderFlag & RENDER_CHROME3
						|| RenderFlag & RENDER_CHROME4
						|| RenderFlag & RENDER_CHROME5
						|| RenderFlag & RENDER_OIL
						|| RenderFlag & RENDER_CHROME6
						|| RenderFlag & RENDER_CHROME7
						|| RenderFlag & RENDER_CHROME8)
					{
						if ((m->m_csTScript && m->m_csTScript->getNoneBlendMesh()) || m->NoneBlendMesh)
							return;

						renderFlags = RENDER_CHROME;
						if (RenderFlag & RENDER_CHROME4)
							renderFlags = RENDER_CHROME4;
						if (RenderFlag & RENDER_CHROME8)
							renderFlags = RENDER_CHROME8;
						if (RenderFlag & RENDER_OIL)
							renderFlags = RENDER_OIL;


						// Most character chrome passes do not consume Wave2 or the
						// trigonometric light vector. Compute each value only for the
						// exact legacy modes that use it; formulas and output stay equal.
						// Glow/chrome terms depend only on WorldTime. Crowded scenes call
						// RenderMesh many times with the exact same time value, so cache the
						// modulo/trigonometric work once while preserving identical output.
						static float s_ChromeWorldTime = -1.0f;
						static float s_ChromeWave = 0.0f;
						static float s_ChromeWave2 = 0.0f;
						static vec3_t s_ChromeLight = { 0.0f, 0.0f, 1.0f };
						if (s_ChromeWorldTime != WorldTime)
						{
							s_ChromeWorldTime = WorldTime;
							s_ChromeWave = (int)WorldTime % 10000 * 0.0001f;
							s_ChromeWave2 = ((int)WorldTime % 5000) * 0.00024f - 0.4f;
							s_ChromeLight[0] = (float)(cos(WorldTime * 0.001f));
							s_ChromeLight[1] = (float)(sin(WorldTime * 0.002f));
							s_ChromeLight[2] = 1.0f;
						}

						float Wave = s_ChromeWave;
						float Wave2 = s_ChromeWave2;
						vec3_t L;
						VectorCopy(s_ChromeLight, L);

						const bool plainChrome = (RenderFlag & RENDER_CHROME) != 0 &&
							(RenderFlag & (RENDER_CHROME2 | RENDER_CHROME3 | RENDER_CHROME4 |
								RENDER_CHROME5 | RENDER_CHROME6 | RENDER_CHROME7 |
								RENDER_CHROME8 | RENDER_OIL)) == 0;
						const int chromeModeMask = RENDER_CHROME2 | RENDER_CHROME3 |
							RENDER_CHROME4 | RENDER_CHROME5 | RENDER_CHROME6 |
							RENDER_CHROME7 | RENDER_CHROME8 | RENDER_OIL;
						const int selectedChromeMode = RenderFlag & chromeModeMask;
						if (plainChrome)
						{
							// Common boss glow: the material mode is invariant for the whole
							// mesh, so avoid retesting every other chrome mode per normal.
							for (int j = 0; j < m->NumNormals && j < MAX_VERTICES; ++j)
							{
								const float* normal = NormalTransform[i][j];
								g_chrome[j][0] = normal[2] * 0.5f + Wave;
								g_chrome[j][1] = normal[1] * 0.5f + Wave * 2.f;
							}
						}
						else if (selectedChromeMode == RENDER_CHROME2)
						{
							const float waveU = Wave2 * 2.0f;
							const float waveV = Wave2 * 3.0f;
							for (int j = 0; j < m->NumNormals && j < MAX_VERTICES; ++j)
							{
								const float* normal = NormalTransform[i][j];
								g_chrome[j][0] = (normal[2] + normal[0]) * 0.8f + waveU;
								g_chrome[j][1] = (normal[1] + normal[0]) + waveV;
							}
						}
						else if (selectedChromeMode == RENDER_CHROME6)
						{
							const float waveOffset = Wave2 * 2.0f;
							for (int j = 0; j < m->NumNormals && j < MAX_VERTICES; ++j)
							{
								const float* normal = NormalTransform[i][j];
								const float chromeCoord = (normal[2] + normal[0]) * 0.8f + waveOffset;
								g_chrome[j][0] = chromeCoord;
								g_chrome[j][1] = chromeCoord;
							}
						}
						else if (selectedChromeMode == RENDER_CHROME3)
						{
							for (int j = 0; j < m->NumNormals && j < MAX_VERTICES; ++j)
							{
								const float* normal = NormalTransform[i][j];
								const float lightDot = DotProduct(normal, LightVector);
								g_chrome[j][0] = lightDot;
								g_chrome[j][1] = 1.0f - lightDot;
							}
						}
						else if (selectedChromeMode == RENDER_CHROME4)
						{
							const float waveV = Wave * 3.0f;
							const float lightU = L[1] * 3.0f;
							for (int j = 0; j < m->NumNormals && j < MAX_VERTICES; ++j)
							{
								const float* normal = NormalTransform[i][j];
								const float lightDot = DotProduct(normal, L);
								g_chrome[j][0] = lightDot;
								g_chrome[j][1] = 1.0f - lightDot;
								g_chrome[j][1] -= (normal[2] * 0.5f + waveV);
								g_chrome[j][0] += (normal[1] * 0.5f + lightU);
							}
						}
						else if (selectedChromeMode == RENDER_CHROME5)
						{
							const float lightU = L[1] * 5.0f;
							for (int j = 0; j < m->NumNormals && j < MAX_VERTICES; ++j)
							{
								const float* normal = NormalTransform[i][j];
								const float lightDot = DotProduct(normal, L);
								g_chrome[j][0] = lightDot;
								g_chrome[j][1] = 1.0f - lightDot;
								g_chrome[j][1] -= (normal[2] * 2.5f + Wave);
								g_chrome[j][0] += (normal[1] * 3.0f + lightU);
							}
						}
						else if (selectedChromeMode == RENDER_CHROME7)
						{
							const float timeOffset = WorldTime * 0.00006f;
							for (int j = 0; j < m->NumNormals && j < MAX_VERTICES; ++j)
							{
								const float* normal = NormalTransform[i][j];
								const float chromeCoord = (normal[2] + normal[0]) * 0.8f + timeOffset;
								g_chrome[j][0] = chromeCoord;
								g_chrome[j][1] = chromeCoord;
							}
						}
						else if (selectedChromeMode == RENDER_CHROME8 ||
							selectedChromeMode == RENDER_OIL)
						{
							for (int j = 0; j < m->NumNormals && j < MAX_VERTICES; ++j)
							{
								const float* normal = NormalTransform[i][j];
								g_chrome[j][0] = normal[0];
								g_chrome[j][1] = normal[1];
							}
						}
						else for (int j = 0; j < m->NumNormals && j < MAX_VERTICES; ++j)
						{
							float* Normal = NormalTransform[i][j];

							if (RenderFlag & RENDER_CHROME2)
							{
								g_chrome[j][0] = (Normal[2] + Normal[0]) * 0.8f + Wave2 * 2.f;
								g_chrome[j][1] = (Normal[1] + Normal[0]) * 1.0f + Wave2 * 3.f;
							}
							else if (RenderFlag & RENDER_CHROME3)
							{
								g_chrome[j][0] = DotProduct(Normal, LightVector);
								g_chrome[j][1] = 1.f - DotProduct(Normal, LightVector);
							}
							else if (RenderFlag & RENDER_CHROME4)
							{
								g_chrome[j][0] = DotProduct(Normal, L);
								g_chrome[j][1] = 1.f - DotProduct(Normal, L);
								g_chrome[j][1] -= ((Normal[2] * 0.5f + Wave * 3.f));
								g_chrome[j][0] += ((Normal[1] * 0.5f + L[1] * 3.f));
							}
							else if (RenderFlag & RENDER_CHROME5)
							{
								g_chrome[j][0] = DotProduct(Normal, L);
								g_chrome[j][1] = 1.f - DotProduct(Normal, L);
								g_chrome[j][1] -= ((Normal[2] * 2.5f + Wave * 1.f));
								g_chrome[j][0] += ((Normal[1] * 3.f + L[1] * 5.f));
							}
							else if (RenderFlag & RENDER_CHROME6)
							{
								g_chrome[j][0] = (Normal[2] + Normal[0]) * 0.8f + Wave2 * 2.f;
								g_chrome[j][1] = (Normal[2] + Normal[0]) * 0.8f + Wave2 * 2.f;
							}
							else if (RenderFlag & RENDER_CHROME7)
							{
								g_chrome[j][0] = (Normal[2] + Normal[0]) * 0.8f + WorldTime * 0.00006f;
								g_chrome[j][1] = (Normal[2] + Normal[0]) * 0.8f + WorldTime * 0.00006f;
							}
							else if (RenderFlag & RENDER_CHROME8)
							{
								g_chrome[j][0] = Normal[0];
								g_chrome[j][1] = Normal[1];
							}
							else if (RenderFlag & RENDER_OIL)
							{
								g_chrome[j][0] = Normal[0];
								g_chrome[j][1] = Normal[1];
							}
							else if (RenderFlag & RENDER_CHROME)
							{
								g_chrome[j][0] = Normal[2] * 0.5f + Wave;
								g_chrome[j][1] = Normal[1] * 0.5f + Wave * 2.f;
							}
							else
							{
								g_chrome[j][0] = Normal[2] * 0.5f + 0.2f;
								g_chrome[j][1] = Normal[1] * 0.5f + 0.5f;
							}
						}

						if (RenderFlag & RENDER_CHROME3 || RenderFlag & RENDER_CHROME4 || RenderFlag & RENDER_CHROME5 || RenderFlag & RENDER_CHROME7 || RenderFlag & RENDER_CHROME8)
						{
							if (Alpha < 0.99f)
							{
								BodyLight[0] *= (Alpha);
								BodyLight[1] *= (Alpha);
								BodyLight[2] *= (Alpha);
							}
							EnableAlphaBlend();
						}
						else if (RenderFlag & RENDER_BRIGHT)
						{
							if (Alpha < 0.99f)
							{
								BodyLight[0] *= (Alpha);
								BodyLight[1] *= (Alpha);
								BodyLight[2] *= (Alpha);
							}
							EnableAlphaBlend();
						}
						else if (RenderFlag & RENDER_DARK)
						{
							EnableAlphaBlendMinus();
						}
						else if (RenderFlag & RENDER_LIGHTMAP)
						{
							EnableLightMap();
						}
						else if (Alpha < 0.99f)
						{
							EnableAlphaTest();
						}
						else
						{
							DisableAlphaBlend();
						}

						if (RenderFlag & RENDER_NODEPTH)
						{
							DisableDepthTest();
						}

						if (RenderFlag & RENDER_CHROME2 && MeshTexture == -1)
						{
							BindTexture(BITMAP_CHROME2);
						}
						else if (RenderFlag & RENDER_CHROME3 && MeshTexture == -1)
						{
							BindTexture(BITMAP_CHROME2);
						}
						else if (RenderFlag & RENDER_CHROME4 && MeshTexture == -1)
						{
							BindTexture(BITMAP_CHROME2);
						}
						else if (RenderFlag & RENDER_CHROME6 && MeshTexture == -1)
						{
							BindTexture(BITMAP_CHROME6);
						}
						else if (RenderFlag & RENDER_CHROME && MeshTexture == -1)
						{
							BindTexture(BITMAP_CHROME);
						}
						else if (RenderFlag & RENDER_METAL && MeshTexture == -1)
						{
							BindTexture(BITMAP_SHINY);
						}
						else
						{
							BindTexture(Texture);
						}
					}
					else if (BlendMesh > -2 && m->Texture != BlendMesh)
					{
						if (RenderFlag & 2)
						{
							renderFlags = RENDER_TEXTURE;
							BindTexture(Texture);

							if (RenderFlag & RENDER_BRIGHT)
							{
								EnableAlphaBlend();
							}
							else if (RenderFlag & RENDER_DARK)
							{
								EnableAlphaBlendMinus();
							}
							else if (Alpha > 0.99f && pBitmap->Components != 4)
							{
								DisableAlphaBlend();
							}
							else
							{
								EnableAlphaTest();
							}

							if (RenderFlag & RENDER_NODEPTH)
								DisableDepthTest();
						}
						else if (RenderFlag & RENDER_BRIGHT)
						{
							if (pBitmap->Components == 4 || m->Texture == BlendMesh)
								return;

							renderFlags = RENDER_BRIGHT;

							EnableAlphaBlend();
							DisableTexture();
							DisableDepthMask();

							if (RenderFlag & RENDER_NODEPTH)
								DisableDepthTest();
						}
						else
						{
							renderFlags = RENDER_TEXTURE;
						}
					}
					else
					{
						renderFlags = RENDER_TEXTURE;
						BindTexture(Texture);

						if (RenderFlag & RENDER_DARK)
							EnableAlphaBlendMinus();
						else
							EnableAlphaBlend();

						if (RenderFlag & RENDER_NODEPTH)
							DisableDepthTest();

						glColor3f(BodyLight[0] * BlendMeshLight, BodyLight[1] * BlendMeshLight, BodyLight[2] * BlendMeshLight);
						EnableLight = false;
					}

					if (RenderFlag & RENDER_DOPPELGANGER && pBitmap->Components != 4)
					{
						EnableCullFace();
						EnableDepthMask();
					}

					if (m->ExpandedVertexIndices == NULL && m->NumTriangles > 0)
					{
						int expandedCount = 0;
						for (int triangleIndex = 0; triangleIndex < m->NumTriangles; ++triangleIndex)
							expandedCount += m->Triangles[triangleIndex].Polygon;
						if (expandedCount > 0)
						{
							m->ExpandedVertexIndices = new short[expandedCount];
							m->ExpandedNormalIndices = new short[expandedCount];
							m->ExpandedTexCoords = new vec2_t[expandedCount];
							int expandedIndex = 0;
							for (int triangleIndex = 0; triangleIndex < m->NumTriangles; ++triangleIndex)
							{
								const Triangle_t* triangle = &m->Triangles[triangleIndex];
								for (int corner = 0; corner < triangle->Polygon; ++corner)
								{
									m->ExpandedVertexIndices[expandedIndex] = triangle->VertexIndex[corner];
									m->ExpandedNormalIndices[expandedIndex] = triangle->NormalIndex[corner];
									const TexCoord_t* uv = &m->TexCoords[triangle->TexCoordIndex[corner]];
									m->ExpandedTexCoords[expandedIndex][0] = uv->TexCoordU;
									m->ExpandedTexCoords[expandedIndex][1] = uv->TexCoordV;
									++expandedIndex;
								}
							}
							m->ExpandedVertexCount = expandedIndex;

							// Shadows only need vertices referenced by the immutable triangle
							// topology. Build this compact list once with the expanded cache.
							bool referencedVertices[MAX_VERTICES] = {};
							for (int index = 0; index < expandedIndex; ++index)
							{
								const int vertexIndex = m->ExpandedVertexIndices[index];
								if (vertexIndex >= 0 && vertexIndex < MAX_VERTICES)
									referencedVertices[vertexIndex] = true;
							}
							for (int vertexIndex = 0; vertexIndex < m->NumVertices && vertexIndex < MAX_VERTICES; ++vertexIndex)
							{
								if (referencedVertices[vertexIndex])
									++m->ShadowVertexCount;
							}
							if (m->ShadowVertexCount > 0)
							{
								m->ShadowVertexIndices = new short[m->ShadowVertexCount];
								int shadowIndex = 0;
								for (int vertexIndex = 0; vertexIndex < m->NumVertices && vertexIndex < MAX_VERTICES; ++vertexIndex)
								{
									if (referencedVertices[vertexIndex])
										m->ShadowVertexIndices[shadowIndex++] = static_cast<short>(vertexIndex);
								}
							}
						}
					}

					// Chrome/color passes use BodyLight or generated chrome coordinates and
					// never consume LightTransform. The legacy path nevertheless rebuilt
					// per-normal lighting for every extra pass. Defer it until the final
					// render mode is known and only compute it for lit texture passes.
					if (EnableLight && renderFlags == RENDER_TEXTURE)
					{
						for (int normal = 0; normal < m->NumNormals; ++normal)
						{
							VectorScale(BodyLight, IntensityTransform[i][normal], LightTransform[i][normal]);
						}
					}

					auto vertices = RenderArrayVertices;
					auto colors = RenderArrayColors;
					const bool useCachedBaseTexCoords =
						renderFlags == RENDER_TEXTURE && !EnableWave &&
						m->ExpandedTexCoords != NULL;
					vec2_t* textCoords = useCachedBaseTexCoords
						? m->ExpandedTexCoords : RenderArrayTexCoords;
					bool useIndexedGlow = false;
#ifndef SHADER_VERSION_TEST
					static const bool indexedGlowEnabled =
						GetPrivateProfileIntA("Renderer", "IndexedGlow", 1, ".\\config.ini") != 0;
					useIndexedGlow = indexedGlowEnabled && !(RenderFlag & RENDER_WAVE) &&
						(renderFlags == RENDER_CHROME || renderFlags == RENDER_CHROME4 || renderFlags == RENDER_CHROME8) &&
						PrepareIndexedGlow(m);
#endif
					// Indexed glow reads unique transformed vertices directly. Expanding all
					// triangle corners first only to collapse them again wastes CPU copies.
					// Other passes keep their existing per-transform position cache.
                    // Preserve normal/UV seams and triangle order with exact indexing.
                    static const bool indexedBaseEnabled=GetPrivateProfileIntA("Renderer","IndexedCrowdBase",1,".\\config.ini")!=0;
                    BaseTopologyCache* indexedBase=NULL;
#ifndef SHADER_VERSION_TEST
                    if(indexedBaseEnabled && (CrowdMeshBudget::Current() || g_BmdSnowSuppressUI || SceneFlag==MAIN_SCENE) && m->ExpandedVertexCount>0 &&
                        m->ExpandedVertexCount<=MAX_VERTICES*3 && renderFlags==RENDER_TEXTURE &&
                        !(RenderFlag&(RENDER_WAVE|RENDER_WAVE_EXT|RENDER_SHADOWMAP|RENDER_NODEPTH)) &&
                        !EnableWave && BmdSnowCoverAt(BodyOrigin)<=0.025f)
                        indexedBase=PrepareIndexedBase(m);
#endif
                    const bool useExpandedPositionCache = !(RenderFlag & RENDER_WAVE) && !useIndexedGlow && !indexedBase;
					if (useExpandedPositionCache && m->ExpandedVertexCount > 0)
					{
						if (m->ExpandedPositions == NULL)
							m->ExpandedPositions = new vec3_t[m->ExpandedVertexCount];
						if (m->ExpandedPositionGeneration != g_ExpandedPositionGeneration)
						{
							for (int expandedIndex = 0; expandedIndex < m->ExpandedVertexCount; ++expandedIndex)
							{
								const int sourceVertexIndex = m->ExpandedVertexIndices[expandedIndex];
								VectorCopy(VertexTransform[i][sourceVertexIndex], m->ExpandedPositions[expandedIndex]);
							}
							m->ExpandedPositionGeneration = g_ExpandedPositionGeneration;
						}
						vertices = m->ExpandedPositions;
					}

					int target_vertex_index = -1;

					const bool uniformChromeColor = renderFlags == RENDER_CHROME
						|| renderFlags == RENDER_CHROME4
						|| renderFlags == RENDER_CHROME8
						|| renderFlags == RENDER_OIL;
					bool enableColor = EnableLight && renderFlags == RENDER_TEXTURE;
					if (uniformChromeColor)
					{
						glColor4f(BodyLight[0], BodyLight[1], BodyLight[2], Alpha);
					}

					const auto profiledPrepareStart = g_KundunMeshPassProfileActive
						? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point();
					// The plain chrome pass only needs transformed positions and the
					// precomputed chrome UV. Keep this hot path out of the generic
					// per-vertex texture/oil/wave decision tree. Color remains the
					// uniform glColor set above, preserving the original glow exactly.
                    if(indexedBase)
                    {
                        vertices=RenderArrayVertices;textCoords=RenderArrayTexCoords;
                        const auto& topology=indexedBase->topology;
                        for(size_t n=0;n<topology.corners.size();++n){
                            const int corner=topology.corners[n];
                            const int v=m->ExpandedVertexIndices[corner];
                            const int normal=m->ExpandedNormalIndices[corner];
                            VectorCopy(VertexTransform[i][v],vertices[n]);
                            textCoords[n][0]=m->ExpandedTexCoords[corner][0];
                            textCoords[n][1]=m->ExpandedTexCoords[corner][1];
                            if(enableColor){colors[n][0]=LightTransform[i][normal][0];colors[n][1]=LightTransform[i][normal][1];colors[n][2]=LightTransform[i][normal][2];colors[n][3]=Alpha;}
                        }
                        target_vertex_index=static_cast<int>(topology.corners.size())-1;
                    }
                    else if (useIndexedGlow)
					{
						vertices = RenderArrayVertices;
						textCoords = RenderArrayTexCoords;
						for (int vertex = 0; vertex < m->GlowVertexCount; ++vertex)
						{
							const int corner = m->GlowSourceCorners[vertex];
							const int normal = m->ExpandedNormalIndices[corner];
							const int sourceVertex = m->ExpandedVertexIndices[corner];
							VectorCopy(VertexTransform[i][sourceVertex], vertices[vertex]);
							if (renderFlags == RENDER_CHROME)
							{
								textCoords[vertex][0] = g_chrome[normal][0];
								textCoords[vertex][1] = g_chrome[normal][1];
							}
							else
							{
								textCoords[vertex][0] = g_chrome[normal][0] + BlendMeshTexCoordU;
								textCoords[vertex][1] = g_chrome[normal][1] + BlendMeshTexCoordV;
							}
						}
						target_vertex_index = m->ExpandedVertexCount - 1;
					}
					else if (renderFlags == RENDER_TEXTURE && m->ExpandedTexCoords != NULL &&
						!EnableLight && !(RenderFlag & RENDER_WAVE))
					{
						// Scrolling textures only need UV offsets; keep geometry cached.
						if (EnableWave)
						{
							for (int expandedIndex = 0; expandedIndex < m->ExpandedVertexCount; ++expandedIndex)
							{
								textCoords[expandedIndex][0] = m->ExpandedTexCoords[expandedIndex][0] + BlendMeshTexCoordU;
								textCoords[expandedIndex][1] = m->ExpandedTexCoords[expandedIndex][1] + BlendMeshTexCoordV;
							}
						}
						// Static base UVs are already referenced directly through textCoords.
						// This common pass therefore needs only the animated positions.
						target_vertex_index = m->ExpandedVertexCount - 1;
					}
					else if (renderFlags == RENDER_TEXTURE && m->ExpandedTexCoords != NULL &&
						EnableLight && !(RenderFlag & RENDER_WAVE))
					{
						// Lit base UVs and animated positions are already cached. Write
						// each final light color once instead of filling a provisional
						// BodyLight color, rereading unused UVs, entering the generic mode
						// switch and then overwriting that color.
						for (int expandedIndex = 0; expandedIndex < m->ExpandedVertexCount; ++expandedIndex)
						{
							const int normalIndex = m->ExpandedNormalIndices[expandedIndex];
							if (EnableWave)
							{
								textCoords[expandedIndex][0] = m->ExpandedTexCoords[expandedIndex][0] + BlendMeshTexCoordU;
								textCoords[expandedIndex][1] = m->ExpandedTexCoords[expandedIndex][1] + BlendMeshTexCoordV;
							}
							colors[expandedIndex][3] = Alpha;
							colors[expandedIndex][0] = LightTransform[i][normalIndex][0];
							colors[expandedIndex][1] = LightTransform[i][normalIndex][1];
							colors[expandedIndex][2] = LightTransform[i][normalIndex][2];
						}
						target_vertex_index = m->ExpandedVertexCount - 1;
					}
					else if (renderFlags == RENDER_CHROME && !(RenderFlag & RENDER_WAVE))
					{
						for (int expandedIndex = 0; expandedIndex < m->ExpandedVertexCount; ++expandedIndex)
						{
							const int normalIndex = m->ExpandedNormalIndices[expandedIndex];
							textCoords[expandedIndex][0] = g_chrome[normalIndex][0];
							textCoords[expandedIndex][1] = g_chrome[normalIndex][1];
						}
						target_vertex_index = m->ExpandedVertexCount - 1;
					}
					else if ((renderFlags == RENDER_CHROME4 || renderFlags == RENDER_CHROME8) &&
						!(RenderFlag & RENDER_WAVE))
					{
						// Same UV arithmetic as the generic path. Positions are cached
						// and color is uniform; avoid unused base-UV reads and per-vertex
						// mode/color/wave decisions for these glow passes.
						for (int expandedIndex = 0; expandedIndex < m->ExpandedVertexCount; ++expandedIndex)
						{
							const int normalIndex = m->ExpandedNormalIndices[expandedIndex];
							textCoords[expandedIndex][0] = g_chrome[normalIndex][0] + BlendMeshTexCoordU;
							textCoords[expandedIndex][1] = g_chrome[normalIndex][1] + BlendMeshTexCoordV;
						}
						target_vertex_index = m->ExpandedVertexCount - 1;
					}
					else for (int expandedIndex = 0; expandedIndex < m->ExpandedVertexCount; ++expandedIndex)
					{
							int source_vertex_index = m->ExpandedVertexIndices[expandedIndex];

							target_vertex_index++;

							if (!useExpandedPositionCache)
								VectorCopy(VertexTransform[i][source_vertex_index], vertices[target_vertex_index]);

							// Fase 3: evita preencher o array de cores quando o OpenGL nao
							// vai utiliza-lo neste passe. Em cenas com muitos personagens,
							// isso remove milhares de escritas de memoria por frame.
							if (enableColor)
							{
								colors[target_vertex_index][3] = Alpha;
								colors[target_vertex_index][0] = BodyLight[0];
								colors[target_vertex_index][1] = BodyLight[1];
								colors[target_vertex_index][2] = BodyLight[2];
							}

							int normalIndex = m->ExpandedNormalIndices[expandedIndex];

							const float baseTexCoordU = m->ExpandedTexCoords[expandedIndex][0];
							const float baseTexCoordV = m->ExpandedTexCoords[expandedIndex][1];
							if (!useCachedBaseTexCoords)
							{
								textCoords[target_vertex_index][0] = baseTexCoordU;
								textCoords[target_vertex_index][1] = baseTexCoordV;
							}

							switch (renderFlags)
							{
							case RENDER_TEXTURE:
								if (EnableWave)
								{
									textCoords[target_vertex_index][0] += BlendMeshTexCoordU;
									textCoords[target_vertex_index][1] += BlendMeshTexCoordV;
								}
								if (EnableLight)
								{
									colors[target_vertex_index][3] = Alpha;
									colors[target_vertex_index][0] = LightTransform[i][normalIndex][0];
									colors[target_vertex_index][1] = LightTransform[i][normalIndex][1];
									colors[target_vertex_index][2] = LightTransform[i][normalIndex][2];
								}
								break;
							case RENDER_OIL:
								textCoords[target_vertex_index][0] = (g_chrome[normalIndex][0] * baseTexCoordU) + BlendMeshTexCoordU;
								textCoords[target_vertex_index][1] = (g_chrome[normalIndex][1] * baseTexCoordV) + BlendMeshTexCoordV;
								break;
							case RENDER_CHROME:
								textCoords[target_vertex_index][0] = g_chrome[normalIndex][0];
								textCoords[target_vertex_index][1] = g_chrome[normalIndex][1];
								break;
							case RENDER_CHROME4:
							case RENDER_CHROME8:
								textCoords[target_vertex_index][0] = g_chrome[normalIndex][0] + BlendMeshTexCoordU;
								textCoords[target_vertex_index][1] = g_chrome[normalIndex][1] + BlendMeshTexCoordV;
								break;
							}

							// O bloco antigo de RENDER_SHADOWMAP calculava uma posicao em
							// uma variavel local e nunca gravava o resultado no vertice.
							// Portanto era trabalho morto executado para cada vertice.
							if (RenderFlag & RENDER_WAVE)
							{
								float time_sin = sinf((float)((int)WorldTime + source_vertex_index * 931) * 0.007f) * 28.0f;
								float* normal = NormalTransform[i][normalIndex];

								for (int iCoord = 0; iCoord < 3; ++iCoord)
								{
									vertices[target_vertex_index][iCoord] += normal[iCoord] * time_sin;
								}
							}
					}

					if (g_KundunMeshPassProfileActive)
					{
						const double prepareMs = std::chrono::duration<double, std::milli>(
							std::chrono::steady_clock::now() - profiledPrepareStart).count();
						if (renderFlags == RENDER_TEXTURE)
							g_KundunTexturePrepareMs += prepareMs;
						else if (renderFlags == RENDER_CHROME || renderFlags == RENDER_CHROME4 ||
							renderFlags == RENDER_CHROME8 || renderFlags == RENDER_OIL)
							g_KundunChromePrepareMs += prepareMs;
						else
							g_KundunOtherPrepareMs += prepareMs;
					}

					if (target_vertex_index != -1)
					{
						const auto profiledDrawStart = g_KundunMeshPassProfileActive
							? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point();
#ifdef SHADER_VERSION_TEST
						this->RenderVertexBuffer(i, m, target_vertex_index + 1, vertices, textCoords, colors);
#else
						if (g_LegacyMeshArrayBatchDepth == 0) glEnableClientState(GL_VERTEX_ARRAY);
						if (g_LegacyMeshArrayBatchDepth != 0)
						{
							if (enableColor != g_LegacyMeshBatchColorArrayEnabled)
							{
								if (enableColor) glEnableClientState(GL_COLOR_ARRAY);
								else glDisableClientState(GL_COLOR_ARRAY);
								g_LegacyMeshBatchColorArrayEnabled = enableColor;
							}
						}
						else if (enableColor)
						{
							glEnableClientState(GL_COLOR_ARRAY);
						}
						if (g_LegacyMeshArrayBatchDepth == 0) glEnableClientState(GL_TEXTURE_COORD_ARRAY);

						glVertexPointer(3, GL_FLOAT, 0, vertices);
						if (enableColor) glColorPointer(4, GL_FLOAT, 0, colors);
						glTexCoordPointer(2, GL_FLOAT, 0, textCoords);
                        if(indexedBase)
                        {
                            const bool clientSmall=PreferSmallMeshClientArrays(static_cast<int>(indexedBase->topology.corners.size()),static_cast<int>(indexedBase->topology.indices.size()));
                            const bool streamed=!clientSmall && BindBaseVertexStream(static_cast<int>(indexedBase->topology.corners.size()),
                                vertices,textCoords,colors,enableColor);
                            const bool gpuIndices=!clientSmall && BindBaseIndexBuffer(*indexedBase);
                            if(clientSmall)++CrowdMeshBudget::Stats().clientSmallMeshDraws;
                            if(!gpuIndices && glBindBuffer)glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
                            glDrawElements(GL_TRIANGLES,static_cast<GLsizei>(indexedBase->topology.indices.size()),
                                GL_UNSIGNED_INT,gpuIndices?NULL:indexedBase->topology.indices.data());
                            if(gpuIndices){glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);++CrowdMeshBudget::Stats().gpuBaseIndexDraws;}
                            else ++CrowdMeshBudget::Stats().clientBaseIndexDraws;
                            if(streamed){
                                // Other legacy effects still pass CPU pointers.
                                glVertexPointer(3,GL_FLOAT,0,vertices);
                                glTexCoordPointer(2,GL_FLOAT,0,textCoords);
                                if(enableColor)glColorPointer(4,GL_FLOAT,0,colors);
                            }
                        }
                        else if (useIndexedGlow)
						{
                            const bool clientSmall=PreferSmallMeshClientArrays(m->GlowVertexCount,m->ExpandedVertexCount);
                            const bool gpuVertices = !clientSmall && BindGlowVertexStream(m->GlowVertexCount, vertices, textCoords);
                            const bool gpuIndices = !clientSmall && BindGlowIndexBuffer(m);
                            if(clientSmall)++CrowdMeshBudget::Stats().clientSmallMeshDraws;
                            if (!gpuIndices && glBindBuffer) glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
                            glDrawElements(GL_TRIANGLES, m->ExpandedVertexCount, GL_UNSIGNED_INT,
                                gpuIndices ? NULL : m->GlowIndices);
                            // Subsequent legacy draws pass CPU index pointers.
                            if (gpuIndices) glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
                            if (gpuVertices)
                            {
                                // Restore client pointers before any later legacy draw.
                                glVertexPointer(3, GL_FLOAT, 0, vertices);
                                glTexCoordPointer(2, GL_FLOAT, 0, textCoords);
                            }
						}
						else
							glDrawArrays(GL_TRIANGLES, 0, m->NumTriangles * 3);

						// The Pegasus snow pass follows transformed normals, so only
						// upward-facing opaque surfaces accumulate snow. This includes
						// dropped items and scenery without recoloring their base texture.
						if (!indexedBase && renderFlags == RENDER_TEXTURE && Alpha >= 0.75f &&
							m->ExpandedNormalIndices != NULL &&
							(!g_BmdSnow.opaqueOnly || pBitmap->Components != 4))
						{
							const float cover = BmdSnowCoverAt(BodyOrigin);
							if (cover > 0.f)
							{
								const auto snowStart = g_KundunMeshPassProfileActive
									? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point();
								static float snowOpacity[MAX_VERTICES];
								const auto opacityForNormal = [&](int normalIndex) -> float {
									const float normalZ = NormalTransform[i][normalIndex][2];
									float slope = (normalZ - g_BmdSnow.slopeStart) /
										(g_BmdSnow.slopeFull - g_BmdSnow.slopeStart);
									slope = fminf(1.f, fmaxf(0.f, slope));
									slope = slope * slope * (3.f - 2.f * slope);
									return fminf(0.92f, slope * cover * g_BmdSnow.opacity);
								};
								const bool cacheSnowNormals = m->NumNormals <= MAX_VERTICES;
								if (cacheSnowNormals)
									for (int normal = 0; normal < m->NumNormals; ++normal)
										snowOpacity[normal] = opacityForNormal(normal);
								for (int vertex = 0; vertex <= target_vertex_index; ++vertex)
								{
									const int normalIndex = m->ExpandedNormalIndices[vertex];
									colors[vertex][0] = g_BmdSnow.color[0];
									colors[vertex][1] = g_BmdSnow.color[1];
									colors[vertex][2] = g_BmdSnow.color[2];
									colors[vertex][3] = cacheSnowNormals ? snowOpacity[normalIndex] : opacityForNormal(normalIndex);
								}
								glPushAttrib(GL_CURRENT_BIT | GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_POLYGON_BIT);
								glDisable(GL_TEXTURE_2D);
								glDisable(GL_LIGHTING);
								glDisable(GL_ALPHA_TEST);
								glEnable(GL_BLEND);
								glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
								glDepthMask(GL_FALSE);
								glEnable(GL_POLYGON_OFFSET_FILL);
								glPolygonOffset(-1.f, -1.f);
								if (!enableColor) glEnableClientState(GL_COLOR_ARRAY);
								glColorPointer(4, GL_FLOAT, 0, colors);
								glDrawArrays(GL_TRIANGLES, 0, target_vertex_index + 1);
								if (!enableColor) glDisableClientState(GL_COLOR_ARRAY);
								glPopAttrib();
								if (g_KundunMeshPassProfileActive)
								{
									++g_ProfileSnowCalls;
									g_ProfileSnowMs += std::chrono::duration<double, std::milli>(
										std::chrono::steady_clock::now() - snowStart).count();
								}
							}
						}

						if (g_LegacyMeshArrayBatchDepth == 0) glDisableClientState(GL_TEXTURE_COORD_ARRAY);
						if (g_LegacyMeshArrayBatchDepth == 0 && enableColor) glDisableClientState(GL_COLOR_ARRAY);
						if (g_LegacyMeshArrayBatchDepth == 0) glDisableClientState(GL_VERTEX_ARRAY);
#endif // SHADER_VERSION_TEST
						if (g_KundunMeshPassProfileActive)
						{
							const double elapsedMs = std::chrono::duration<double, std::milli>(
								std::chrono::steady_clock::now() - profiledDrawStart).count();
							g_KundunProfileVertices += useIndexedGlow ? m->GlowVertexCount : target_vertex_index + 1;
							if (renderFlags == RENDER_TEXTURE)
							{
								++g_KundunTextureCalls;
								g_KundunTextureMs += elapsedMs;
							}
							else if (renderFlags == RENDER_CHROME || renderFlags == RENDER_CHROME4 ||
								renderFlags == RENDER_CHROME8 || renderFlags == RENDER_OIL)
							{
								++g_KundunChromeCalls;
								g_KundunChromeMs += elapsedMs;
							}
							else
							{
								++g_KundunOtherCalls;
								g_KundunOtherMs += elapsedMs;
							}
						}
					}
				}
			}
		}
	}
}

void BMD::RenderBody(int Flag, float Alpha, int BlendMesh, float BlendMeshLight, float BlendMeshTexCoordU, float BlendMeshTexCoordV, int HiddenMesh, int Texture)
{
	if (g_RenderBackend.GetActiveType() == RENDER_BACKEND_TYPE_MODERN)
	{
		g_ModelRenderer.Render(this, Flag, Alpha, HiddenMesh, BlendMesh, BlendMeshLight, Texture);
		this->BeginRender(Alpha);
		this->EndRender();
		return;
	}

	if (NumMeshs)
	{
		int iBlendMesh = BlendMesh;

		// Legacy RenderMesh uses transformed arrays without changing matrices.
		// Preserve matrix protection for the alternate shader implementation.
#ifdef SHADER_VERSION_TEST
		this->BeginRender(Alpha);
#endif

		if (!LightEnable)
		{
			if (Alpha < 0.99)
				glColor4f(BodyLight[0], BodyLight[1], BodyLight[2], Alpha);
			else
				glColor3fv(BodyLight);
		}

#ifndef SHADER_VERSION_TEST
		// RenderBody submits every mesh with the same legacy client-array setup.
		// Keep those arrays enabled for the complete body and let RenderMesh only
		// switch GL_COLOR_ARRAY when a pass actually needs per-vertex lighting.
		// This changes no vertex, texture coordinate, material or draw call.
		BeginLegacyMeshArrayBatch();
#endif

		for (int i = 0; i < NumMeshs; i++)
		{
			Mesh_t* m = &Meshs[i];

			iBlendMesh = BlendMesh;

			if (m->m_csTScript)
			{
				if (!m->m_csTScript->getHiddenMesh() && i != HiddenMesh)
				{
					if (m->m_csTScript->getBright())
						iBlendMesh = i;

					this->RenderMesh(i, Flag, Alpha, iBlendMesh, BlendMeshLight, BlendMeshTexCoordU, BlendMeshTexCoordV, Texture);

					BYTE shadowType = m->m_csTScript->getShadowMesh();

					if (shadowType == SHADOW_RENDER_COLOR)
					{
						DisableAlphaBlend();
						if (Alpha < 0.99)
							glColor4f(0.f, 0.f, 0.f, Alpha);
						else
							glColor3f(0.f, 0.f, 0.f);

						this->RenderMesh(i, RENDER_COLOR | RENDER_SHADOWMAP, Alpha, iBlendMesh, BlendMeshLight, BlendMeshTexCoordU, BlendMeshTexCoordV);
						glColor3f(1.f, 1.f, 1.f);
					}
					else if (shadowType == SHADOW_RENDER_TEXTURE)
					{
						DisableAlphaBlend();
						if (Alpha < 0.99)
							glColor4f(0.f, 0.f, 0.f, Alpha);
						else
							glColor3f(0.f, 0.f, 0.f);

						this->RenderMesh(i, RENDER_TEXTURE | RENDER_SHADOWMAP, Alpha, iBlendMesh, BlendMeshLight, BlendMeshTexCoordU, BlendMeshTexCoordV);
						glColor3f(1.f, 1.f, 1.f);
					}
				}
			}
			else if (i != HiddenMesh)
			{
				this->RenderMesh(i, Flag, Alpha, BlendMesh, BlendMeshLight, BlendMeshTexCoordU, BlendMeshTexCoordV, Texture);
			}
		}
#ifndef SHADER_VERSION_TEST
		EndLegacyMeshArrayBatch();
#endif
#ifdef SHADER_VERSION_TEST
		this->EndRender();
#endif
	}
}

void BMD::RenderMeshTranslate(int i, int RenderFlag, float Alpha, int BlendMesh, float BlendMeshLight, float BlendMeshTexCoordU, float BlendMeshTexCoordV, int MeshTexture)
{
	if (i >= NumMeshs || i < 0)
		return;

	Mesh_t* m = &Meshs[i];
	if (m->NumTriangles == 0)
		return;
	float Wave = (int)WorldTime % 10000 * 0.0001f;

	int Texture = IndexTexture[m->Texture];
	if (Texture == BITMAP_HIDE)
		return;
	else if (Texture == BITMAP_SKIN)
	{
		if (HideSkin) return;
		Texture = BITMAP_SKIN + Skin;
	}
	else if (Texture == BITMAP_WATER)
	{
		Texture = BITMAP_WATER + (int)WaterTextureNumber;
	}
	if (MeshTexture != -1)
		Texture = MeshTexture;

	BITMAP_t* pBitmap = Bitmaps.GetTexture(Texture);

	bool EnableWave = false;
	int streamMesh = StreamMesh;
	if (m->m_csTScript != NULL)
	{
		if (m->m_csTScript->getStreamMesh())
		{
			streamMesh = i;
		}
	}
	if ((i == BlendMesh || i == streamMesh) && (BlendMeshTexCoordU != 0.f || BlendMeshTexCoordV != 0.f))
		EnableWave = true;

	bool EnableLight = LightEnable;
	if (i == StreamMesh)
	{
		//vec3_t Light;
		//Vector(1.f,1.f,1.f,Light);
		glColor3fv(BodyLight);
		EnableLight = false;
	}
	else if (EnableLight)
	{
		for (int j = 0; j < m->NumNormals; j++)
		{
			VectorScale(BodyLight, IntensityTransform[i][j], LightTransform[i][j]);
		}
	}

	int Render = RenderFlag;
	if ((RenderFlag & RENDER_COLOR) == RENDER_COLOR)
	{
		Render = RENDER_COLOR;
		if ((RenderFlag & RENDER_BRIGHT) == RENDER_BRIGHT)
			EnableAlphaBlend();
		else if ((RenderFlag & RENDER_DARK) == RENDER_DARK)
			EnableAlphaBlendMinus();
		else
			DisableAlphaBlend();
		DisableTexture();
		glColor3fv(BodyLight);
	}
	else if ((RenderFlag & RENDER_CHROME) == RENDER_CHROME
		|| (RenderFlag & RENDER_METAL) == RENDER_METAL
		|| (RenderFlag & RENDER_CHROME2) == RENDER_CHROME2
		|| (RenderFlag & RENDER_CHROME6) == RENDER_CHROME6
		)
	{
		if (m->m_csTScript != NULL)
		{
			if (m->m_csTScript->getNoneBlendMesh()) return;
		}
		if (m->NoneBlendMesh)
			return;
		Render = RENDER_CHROME;

		float Wave2 = (int)WorldTime % 5000 * 0.00024f - 0.4f;

		for (int j = 0; j < m->NumNormals; j++)
		{
			//			Normal_t *np = &m->Normals[j];
			if (j >= MAX_VERTICES) break;
			float* Normal = NormalTransform[i][j];

			if ((RenderFlag & RENDER_CHROME2) == RENDER_CHROME2)
			{
				g_chrome[j][0] = (Normal[2] + Normal[0]) * 0.8f + Wave2 * 2.f;
				g_chrome[j][1] = (Normal[1] + Normal[0]) * 1.0f + Wave2 * 3.f;
			}
			else if ((RenderFlag & RENDER_CHROME) == RENDER_CHROME)
			{
				g_chrome[j][0] = Normal[2] * 0.5f + Wave;
				g_chrome[j][1] = Normal[1] * 0.5f + Wave * 2.f;
			}
			else if ((RenderFlag & RENDER_CHROME6) == RENDER_CHROME6)
			{
				g_chrome[j][0] = (Normal[2] + Normal[0]) * 0.8f + Wave2 * 2.f;
				g_chrome[j][1] = (Normal[1] + Normal[0]) * 1.0f + Wave2 * 3.f;
			}
			else
			{
				g_chrome[j][0] = Normal[2] * 0.5f + 0.2f;
				g_chrome[j][1] = Normal[1] * 0.5f + 0.5f;
			}
		}

		if ((RenderFlag & RENDER_BRIGHT) == RENDER_BRIGHT)
			EnableAlphaBlend();
		else if ((RenderFlag & RENDER_DARK) == RENDER_DARK)
			EnableAlphaBlendMinus();
		else if ((RenderFlag & RENDER_LIGHTMAP) == RENDER_LIGHTMAP)
			EnableLightMap();
		else
			DisableAlphaBlend();

		if ((RenderFlag & RENDER_CHROME2) == RENDER_CHROME2 && MeshTexture == -1)
		{
			BindTexture(BITMAP_CHROME2);
		}
		else if ((RenderFlag & RENDER_CHROME) == RENDER_CHROME && MeshTexture == -1)
			BindTexture(BITMAP_CHROME);
		else if ((RenderFlag & RENDER_METAL) == RENDER_METAL && MeshTexture == -1)
			BindTexture(BITMAP_SHINY);
		else
			BindTexture(Texture);
	}
	else if (BlendMesh <= -2 || m->Texture == BlendMesh)
	{
		Render = RENDER_TEXTURE;
		BindTexture(Texture);
		if ((RenderFlag & RENDER_DARK) == RENDER_DARK)
			EnableAlphaBlendMinus();
		else
			EnableAlphaBlend();
		glColor3f(BodyLight[0] * BlendMeshLight, BodyLight[1] * BlendMeshLight, BodyLight[2] * BlendMeshLight);
		//glColor3f(BlendMeshLight,BlendMeshLight,BlendMeshLight);
		EnableLight = false;
	}
	else if ((RenderFlag & RENDER_TEXTURE) == RENDER_TEXTURE)
	{
		Render = RENDER_TEXTURE;
		BindTexture(Texture);
		if ((RenderFlag & RENDER_BRIGHT) == RENDER_BRIGHT)
		{
			EnableAlphaBlend();
		}
		else if ((RenderFlag & RENDER_DARK) == RENDER_DARK)
		{
			EnableAlphaBlendMinus();
		}
		else if (Alpha < 0.99f || pBitmap->Components == 4)
		{
			EnableAlphaTest();
		}
		else
		{
			DisableAlphaBlend();
		}
	}
	else if ((RenderFlag & RENDER_BRIGHT) == RENDER_BRIGHT)
	{
		if (pBitmap->Components == 4 || m->Texture == BlendMesh)
		{
			return;
		}
		Render = RENDER_BRIGHT;
		EnableAlphaBlend();
		DisableTexture();
		DisableDepthMask();
	}
	else
	{
		Render = RENDER_TEXTURE;
	}

	int target_vertex_index = -1;
	auto vertices = RenderArrayVertices;
	auto colors = RenderArrayColors;
	auto textCoords = RenderArrayTexCoords;

	for (int j = 0; j < m->NumTriangles; j++)
	{
		Triangle_t* tp = &m->Triangles[j];
		for (int k = 0; k < tp->Polygon; k++)
		{
			++target_vertex_index;
			int vi = tp->VertexIndex[k];
			colors[target_vertex_index][0] = BodyLight[0];
			colors[target_vertex_index][1] = BodyLight[1];
			colors[target_vertex_index][2] = BodyLight[2];
			colors[target_vertex_index][3] = Alpha;
			switch (Render)
			{
			case RENDER_TEXTURE:
			{
				TexCoord_t* texp = &m->TexCoords[tp->TexCoordIndex[k]];
				if (EnableWave)
				{
					textCoords[target_vertex_index][0] = texp->TexCoordU + BlendMeshTexCoordU;
					textCoords[target_vertex_index][1] = texp->TexCoordV + BlendMeshTexCoordV;
				}
				else
				{
					textCoords[target_vertex_index][0] = texp->TexCoordU;
					textCoords[target_vertex_index][1] = texp->TexCoordV;
				}
				if (EnableLight)
				{
					int ni = tp->NormalIndex[k];
					float* Light = LightTransform[i][ni];
					colors[target_vertex_index][0] = Light[0];
					colors[target_vertex_index][1] = Light[1];
					colors[target_vertex_index][2] = Light[2];
				}
				break;
			}
			case RENDER_CHROME:
			{
				int ni = tp->NormalIndex[k];
				textCoords[target_vertex_index][0] = g_chrome[ni][0];
				textCoords[target_vertex_index][1] = g_chrome[ni][1];
				break;
			}
			}
			VectorAdd(VertexTransform[i][vi], BodyOrigin, vertices[target_vertex_index]);
		}
	}

	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, vertices);
	if (Render == RENDER_TEXTURE || Render == RENDER_CHROME)
	{
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glTexCoordPointer(2, GL_FLOAT, 0, textCoords);
	}
	glEnableClientState(GL_COLOR_ARRAY);
	glColorPointer(4, GL_FLOAT, 0, colors);
	glDrawArrays(GL_TRIANGLES, 0, target_vertex_index + 1);
	glDisableClientState(GL_COLOR_ARRAY);
	if (Render == RENDER_TEXTURE || Render == RENDER_CHROME)
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);
}

void BMD::RenderBodyTranslate(int Flag, float Alpha, int BlendMesh, float BlendMeshLight, float BlendMeshTexCoordU, float BlendMeshTexCoordV, int HiddenMesh, int Texture)
{
	if (NumMeshs == 0) return;

	BeginRender(Alpha);
	if (!LightEnable)
	{
		if (Alpha >= 0.99f)
			glColor3fv(BodyLight);
		else
			glColor4f(BodyLight[0], BodyLight[1], BodyLight[2], Alpha);
	}
	for (int i = 0; i < NumMeshs; i++)
	{
		if (i != HiddenMesh)
		{
			RenderMeshTranslate(i, Flag, Alpha, BlendMesh, BlendMeshLight, BlendMeshTexCoordU, BlendMeshTexCoordV, Texture);
		}
	}
	EndRender();
}

// -----------------------------------------------------------------------------
// Stage 2 - softer projected shadows without re-projecting the geometry.
//
// The expensive part of the legacy shadow is projecting every animated vertex
// against the terrain (RequestTerrainHeight).  We keep that calculation once
// and reuse the already projected triangle array for a few tiny offset draws.
// This creates a cheap penumbra while the adaptive sample count falls back to a
// single pass automatically when FPS is under pressure.
// -----------------------------------------------------------------------------
struct LegacyShadowVisualSettings
{
	float Opacity;
	int SoftQuality;
	float Softness;
	float GroundOffset;
	bool Adaptive;
	float LowFps;
	float CriticalFps;
	bool DirectionalProjection;
	float DirectionAngle;
	float LengthScale;
	bool StencilDedup;
};

static const LegacyShadowVisualSettings& GetLegacyShadowVisualSettings()
{
	static const LegacyShadowVisualSettings settings = []()
	{
		LegacyShadowVisualSettings value = {};

		int opacityPercent = GetPrivateProfileIntA("Renderer", "ShadowOpacity", 22, ".\\config.ini");
		if (opacityPercent < 6) opacityPercent = 6;
		if (opacityPercent > 55) opacityPercent = 55;
		value.Opacity = opacityPercent * 0.01f;

		value.SoftQuality = GetPrivateProfileIntA("Renderer", "ShadowSoftQuality", 1, ".\\config.ini");
		if (value.SoftQuality < 0) value.SoftQuality = 0;
		if (value.SoftQuality > 2) value.SoftQuality = 2;

		value.Softness = (float)GetPrivateProfileIntA("Renderer", "ShadowSoftness", 4, ".\\config.ini");
		if (value.Softness < 0.0f) value.Softness = 0.0f;
		if (value.Softness > 12.0f) value.Softness = 12.0f;

		value.GroundOffset = (float)GetPrivateProfileIntA("Renderer", "ShadowGroundOffset", 2, ".\\config.ini");
		if (value.GroundOffset < 1.0f) value.GroundOffset = 1.0f;
		if (value.GroundOffset > 6.0f) value.GroundOffset = 6.0f;

		value.Adaptive = GetPrivateProfileIntA("Renderer", "ShadowAdaptive", 1, ".\\config.ini") != 0;
		value.LowFps = (float)GetPrivateProfileIntA("Renderer", "ShadowLowFPS", 57, ".\\config.ini");
		value.CriticalFps = (float)GetPrivateProfileIntA("Renderer", "ShadowCriticalFPS", 32, ".\\config.ini");

		value.DirectionalProjection =
			GetPrivateProfileIntA("Renderer", "ShadowDirectional", 1, ".\\config.ini") != 0;

		int angle = GetPrivateProfileIntA("Renderer", "ShadowDirectionAngle", 225, ".\\config.ini");
		while (angle < 0) angle += 360;
		while (angle >= 360) angle -= 360;
		value.DirectionAngle = (float)angle;

		int lengthPercent = GetPrivateProfileIntA("Renderer", "ShadowLength", 42, ".\\config.ini");
		if (lengthPercent < 10) lengthPercent = 10;
		if (lengthPercent > 85) lengthPercent = 85;
		value.LengthScale = lengthPercent * 0.01f;

		value.StencilDedup =
			GetPrivateProfileIntA("Renderer", "ShadowStencilDedup", 1, ".\\config.ini") != 0;
		return value;
	}();
	return settings;
}

namespace
{
	bool g_LegacyShadowStencilActive = false;
	bool g_LegacyShadowCharacterGroup = false;
	bool g_LegacyShadowHasReferenceGround = false;
	float g_LegacyShadowReferenceGroundZ = 0.0f;
	unsigned int g_LegacyShadowStencilRef = 0;

	bool LegacyShadowHasStencilBuffer()
	{
		static int stencilBits = -1;
		if (stencilBits < 0)
		{
			GLint bits = 0;
			glGetIntegerv(GL_STENCIL_BITS, &bits);
			stencilBits = (int)bits;
		}
		return stencilBits > 0;
	}

	bool BeginLegacyShadowStencilGroup()
	{
		if (!GetLegacyShadowVisualSettings().StencilDedup || !LegacyShadowHasStencilBuffer())
			return false;

		if (g_LegacyShadowStencilRef == 0 || g_LegacyShadowStencilRef >= 254)
		{
			glClearStencil(0);
			glStencilMask(0xFF);
			glClear(GL_STENCIL_BUFFER_BIT);
			g_LegacyShadowStencilRef = 1;
		}
		else
		{
			++g_LegacyShadowStencilRef;
		}

		glEnable(GL_STENCIL_TEST);
		glStencilMask(0xFF);
		glStencilFunc(GL_NOTEQUAL, (GLint)g_LegacyShadowStencilRef, 0xFF);
		glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
		g_LegacyShadowStencilActive = true;
		return true;
	}

	void EndLegacyShadowStencilGroup()
	{
		if (!g_LegacyShadowStencilActive)
			return;

		glStencilMask(0xFF);
		glDisable(GL_STENCIL_TEST);
		g_LegacyShadowStencilActive = false;
	}

	float GetLegacyShadowProjectionReferenceZ(const vec3_t fallbackOrigin)
	{
		if (g_LegacyShadowCharacterGroup && g_LegacyShadowHasReferenceGround)
			return g_LegacyShadowReferenceGroundZ;
		return fallbackOrigin[2];
	}
}

// Player body + weapons + wing share the same stencil reference so overlapping
// pieces do not accumulate into a solid black blob.
void BeginLegacyCharacterShadowGroup(float worldX, float worldY)
{
	g_LegacyShadowCharacterGroup = true;
	g_LegacyShadowReferenceGroundZ = RequestTerrainHeight(worldX, worldY);
	g_LegacyShadowHasReferenceGround = true;
	BeginLegacyShadowStencilGroup();
}

void EndLegacyCharacterShadowGroup()
{
	EndLegacyShadowStencilGroup();
	g_LegacyShadowHasReferenceGround = false;
	g_LegacyShadowCharacterGroup = false;
}

static int GetLegacyShadowSampleCount()
{
	const LegacyShadowVisualSettings& settings = GetLegacyShadowVisualSettings();
	int samples = settings.SoftQuality <= 0 ? 1 : (settings.SoftQuality == 1 ? 3 : 5);

	// A soft projected shadow repeats the same geometry for every sample. Keep
	// the shadow itself in crowded scenes, but use a single clean projection so
	// 50-character battles do not multiply the shadow cost by three or five.
	static const int crowdSoftLimit = []()
	{
		const int value = GetPrivateProfileIntA(
			"Renderer", "ShadowCrowdSoftLimit", 12, ".\\config.ini");
		return value < 2 ? 2 : (value > 24 ? 24 : value);
	}();
	if (g_VisibleCharacterCountForShadowLod >= 24 ||
		(settings.Adaptive && g_VisibleCharacterCountForShadowLod >= crowdSoftLimit))
		return 1;

	if (settings.Adaptive && RealRenderFPS > 0.0f)
	{
		if (RealRenderFPS < settings.CriticalFps)
			return 1;
		if (RealRenderFPS < settings.LowFps)
			return samples >= 5 ? 3 : 1;
	}
	return samples;
}

static float GetLegacyShadowOpacity()
{
	float opacity = GetLegacyShadowVisualSettings().Opacity;
	float dynamicAngle = 0.0f, dynamicLength = 0.0f, dynamicOpacity = 1.0f;
	if (GetServerVisualSunShadow(dynamicAngle, dynamicLength, dynamicOpacity))
		opacity *= dynamicOpacity;
	if (gMapManager != NULL && gMapManager->currentMap == 7 && opacity > 0.17f)
		opacity = 0.17f;
	return opacity;
}

static void DrawProjectedShadowTriangles(const vec3_t* vertices, const int vertexCount)
{
	if (vertices == NULL || vertexCount <= 0)
		return;

	CharacterStageTimer drawTimer(g_ProfileShadowDrawMs, g_ProfileShadowDrawBatches);
	const LegacyShadowVisualSettings& settings = GetLegacyShadowVisualSettings();
	const int samples = GetLegacyShadowSampleCount();
	const float opacity = GetLegacyShadowOpacity();
	const float s = settings.Softness;

	glDisableClientState(GL_COLOR_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, vertices);

	if (samples <= 1 || s <= 0.0f)
	{
		glColor4f(0.0f, 0.0f, 0.0f, opacity);
		glDrawArrays(GL_TRIANGLES, 0, vertexCount);
	}
	else if (samples == 3)
	{
		const float offsets[3][2] =
		{
			{ 0.0f, 0.0f },
			{ -s, s * 0.55f },
			{ s, -s * 0.55f }
		};
		const float weights[3] = { 1.0f, 0.42f, 0.42f };

		for (int i = 0; i < 3; ++i)
		{
			glColor4f(0.0f, 0.0f, 0.0f, opacity * weights[i]);
			glPushMatrix();
			glTranslatef(offsets[i][0], offsets[i][1], 0.0f);
			glDrawArrays(GL_TRIANGLES, 0, vertexCount);
			glPopMatrix();
		}
	}
	else
	{
		const float offsets[5][2] =
		{
			{ 0.0f, 0.0f },
			{ -s, 0.0f },
			{ s, 0.0f },
			{ 0.0f, -s },
			{ 0.0f, s }
		};
		const float weights[5] = { 1.0f, 0.28f, 0.28f, 0.28f, 0.28f };

		for (int i = 0; i < 5; ++i)
		{
			glColor4f(0.0f, 0.0f, 0.0f, opacity * weights[i]);
			glPushMatrix();
			glTranslatef(offsets[i][0], offsets[i][1], 0.0f);
			glDrawArrays(GL_TRIANGLES, 0, vertexCount);
			glPopMatrix();
		}
	}

	glDisableClientState(GL_VERTEX_ARRAY);
	RestoreLegacyMeshArrayBatchState();
}

// One immutable projection per body/cloth pass. All vertices see the same
// sun state, reference ground and direction; terrain is still sampled per vertex.
struct LegacyShadowProjection
{
    ShadowFlatGroundPatch flatGround;
    bool directional;
    float referenceGroundZ;
    float lengthScale;
    float directionX;
    float directionY;
    float groundOffset;
};

static LegacyShadowProjection MakeLegacyShadowProjection(const vec3_t origin)
{
    const LegacyShadowVisualSettings& settings = GetLegacyShadowVisualSettings();
    LegacyShadowProjection projection = {};
    projection.directional = settings.DirectionalProjection;
    projection.groundOffset = settings.GroundOffset;
    static const bool flatGround=GetPrivateProfileIntA("Renderer","ShadowFlatGround",1,".\\config.ini")!=0;
    if(flatGround && SceneFlag==MAIN_SCENE && g_VisibleCharacterCountForShadowLod>=4)
        projection.flatGround.Build(BackTerrainHeight,TerrainWall,TERRAIN_SIZE,TERRAIN_SCALE,origin[0],origin[1],TW_HEIGHT);
    if (projection.directional)
    {
        projection.referenceGroundZ = GetLegacyShadowProjectionReferenceZ(origin);
        float directionAngle = settings.DirectionAngle;
        projection.lengthScale = settings.LengthScale;
        float opacityScale = 1.0f;
        GetServerVisualSunShadow(directionAngle, projection.lengthScale, opacityScale);
        const float radians = directionAngle * (3.14159265358979323846f / 180.0f);
        projection.directionX = cosf(radians);
        projection.directionY = sinf(radians);
    }
    return projection;
}

static float SampleShadowGround(float x,float y,const LegacyShadowProjection& projection)
{
    if(projection.flatGround.Contains(x,y)){++CrowdMeshBudget::Stats().flatShadowSamples;return projection.flatGround.height;}
    ++CrowdMeshBudget::Stats().fullShadowSamples;return RequestTerrainHeight(x,y);
}

__forceinline void CalcShadowPosition(vec3_t* position, const vec3_t origin, const float sx, const float sy, const LegacyShadowProjection& projection)
{
	if (projection.directional)
	{
		// Orthographic sun-style projection. The fallback still uses Renderer
		// ShadowDirectionAngle/ShadowLength, while TimeOfDay can override both
		// with the server-synchronised sun position.
		const float referenceGroundZ = projection.referenceGroundZ;
		float height = (*position)[2] - referenceGroundZ;
		if (height < 0.0f) height = 0.0f;
		if (height > 450.0f) height = 450.0f; // protects against malformed/extreme models

		const float castDistance = height * projection.lengthScale;
		const float projectedX = (*position)[0] + projection.directionX * castDistance;
		const float projectedY = (*position)[1] + projection.directionY * castDistance;

		(*position)[0] = projectedX;
		(*position)[1] = projectedY;
		(*position)[2] = SampleShadowGround(projectedX, projectedY, projection) + projection.groundOffset;
		return;
	}

	// Optional compatibility fallback to the original Season 6 projection.
	const float relativeX = (*position)[0] - origin[0];
	const float relativeZ = (*position)[2] - origin[2];
	const float denominator = relativeZ - sy;
	const float safeDenominator = (fabsf(denominator) < 1.0f)
		? (denominator < 0.0f ? -1.0f : 1.0f) : denominator;
	const float projectedX = relativeX +
		relativeZ * (relativeX + sx) / safeDenominator + origin[0];

	(*position)[0] = projectedX;
	(*position)[2] = SampleShadowGround(projectedX, (*position)[1], projection) + projection.groundOffset;
}

__forceinline void GetClothShadowPosition(vec3_t* target, CPhysicsCloth* pCloth, const int index, const vec3_t origin, const float sx, const float sy, const LegacyShadowProjection& projection)
{
	pCloth->GetPosition(index, target);
	CalcShadowPosition(target, origin, sx, sy, projection);
}

void BMD::AddClothesShadowTriangles(void* pClothes, const int clothesCount, const float sx, const float sy) const
{
	const LegacyShadowProjection projection = MakeLegacyShadowProjection(BodyOrigin);
	auto vertices = RenderArrayVertices;
	int target_vertex_index = -1;
	// Render-thread scratch: project shared cloth vertices once per shadow pass.
	// Rebuilt for each cloth, so animation and terrain heights remain current.
	static vec3_t projectedClothVertices[MAX_VERTICES];
	const int triangleBufferCapacity = MAX_VERTICES * 3;

	for (int i = 0; i < clothesCount; i++)
	{
		auto* const pCloth = &static_cast<CPhysicsCloth*>(pClothes)[i];
		auto const columns = pCloth->GetVerticalCount();
		auto const rows = pCloth->GetHorizontalCount();
		if (columns < 2 || rows < 2)
			continue;
		const bool useProjectedCache = columns <= MAX_VERTICES / rows;
		if (useProjectedCache)
		{
			const int vertexCount = columns * rows;
			for (int vertex = 0; vertex < vertexCount; ++vertex)
				GetClothShadowPosition(&projectedClothVertices[vertex], pCloth,
					vertex, BodyOrigin, sx, sy, projection);
		}

		for (int col = 0; col < columns - 1; ++col)
		{
			for (int row = 0; row < rows - 1; ++row)
			{
				// first we take each point for an square from which we derive
				// a A-Triangle and the V-Triangle.
				int a = rows * col + row;
				int b = rows * (col + 1) + row;
				int c = rows * col + row + 1;
				int d = rows * (col + 1) + row + 1;

				vec3_t posA, posB, posC, posD;

				if (useProjectedCache)
				{
					VectorCopy(projectedClothVertices[a], posA);
					VectorCopy(projectedClothVertices[b], posB);
					VectorCopy(projectedClothVertices[c], posC);
					VectorCopy(projectedClothVertices[d], posD);
				}
				else
				{
					GetClothShadowPosition(&posA, pCloth, a, BodyOrigin, sx, sy, projection);
					GetClothShadowPosition(&posB, pCloth, b, BodyOrigin, sx, sy, projection);
					GetClothShadowPosition(&posC, pCloth, c, BodyOrigin, sx, sy, projection);
					GetClothShadowPosition(&posD, pCloth, d, BodyOrigin, sx, sy, projection);
				}

				// Flush whole triangles before the fixed staging buffer fills.
				if (target_vertex_index + 7 > triangleBufferCapacity)
				{
					DrawProjectedShadowTriangles(vertices, target_vertex_index + 1);
					target_vertex_index = -1;
				}

				// A-Triangle:
				target_vertex_index++;
				VectorCopy(posA, vertices[target_vertex_index]);
				target_vertex_index++;
				VectorCopy(posB, vertices[target_vertex_index]);
				target_vertex_index++;
				VectorCopy(posC, vertices[target_vertex_index]);

				// V-Triangle:
				target_vertex_index++;
				VectorCopy(posD, vertices[target_vertex_index]);
				target_vertex_index++;
				VectorCopy(posB, vertices[target_vertex_index]);
				target_vertex_index++;
				VectorCopy(posC, vertices[target_vertex_index]);
			}
		}
	}

	if (target_vertex_index < 0)
	{
		return;
	}
	DrawProjectedShadowTriangles(vertices, target_vertex_index + 1);
}

void BMD::AddMeshShadowTriangles(const int blendMesh, const int hiddenMesh, const int startMesh, const int endMesh, const float sx, const float sy, const int maxMeshes) const
{
	const LegacyShadowProjection projection = MakeLegacyShadowProjection(BodyOrigin);
	auto vertices = RenderArrayVertices;
	int target_vertex_index = -1;
	int renderedMeshes = 0;

	for (int i = startMesh; i < endMesh; i++)
	{
		if (i == hiddenMesh)
		{
			continue;
		}

		const Mesh_t* mesh = &Meshs[i];
		if (mesh->NumTriangles <= 0 || mesh->Texture == blendMesh)
		{
			continue;
		}
		if (maxMeshes > 0 && renderedMeshes >= maxMeshes)
		{
			break;
		}
		++renderedMeshes;

		// A vertex is commonly referenced by several triangles. The legacy path
		// projected it onto the terrain (including RequestTerrainHeight) once for
		// every triangle corner. Project each unique transformed vertex once and
		// reuse the exact result while expanding the triangle list. This preserves
		// the original shadow geometry and removes the dominant crowd-side cost.
		vec3_t projectedVertices[MAX_VERTICES];
		const int projectedVertexCount = mesh->NumVertices < MAX_VERTICES
			? mesh->NumVertices : MAX_VERTICES;
		if (mesh->ShadowVertexIndices != NULL && mesh->ShadowVertexCount > 0)
		{
			for (int shadowIndex = 0; shadowIndex < mesh->ShadowVertexCount; ++shadowIndex)
			{
				const int vertex = mesh->ShadowVertexIndices[shadowIndex];
				VectorCopy(VertexTransform[i][vertex], projectedVertices[vertex]);
				CalcShadowPosition(&projectedVertices[vertex], BodyOrigin, sx, sy, projection);
			}
		}
		else
		{
			for (int vertex = 0; vertex < projectedVertexCount; ++vertex)
			{
				VectorCopy(VertexTransform[i][vertex], projectedVertices[vertex]);
				CalcShadowPosition(&projectedVertices[vertex], BodyOrigin, sx, sy, projection);
			}
		}

		// The body pass has already flattened the immutable triangle topology.
		// Reuse that order for the shadow instead of reopening every triangle and
		// polygon corner on every character/frame. The produced vertex sequence is
		// identical to the legacy nested loop.
		if (mesh->ExpandedVertexIndices != NULL && mesh->ExpandedVertexCount > 0)
		{
			for (int expandedIndex = 0; expandedIndex < mesh->ExpandedVertexCount; ++expandedIndex)
			{
				const int source_vertex_index = mesh->ExpandedVertexIndices[expandedIndex];
				target_vertex_index++;

				if (source_vertex_index >= 0 && source_vertex_index < projectedVertexCount)
				{
					VectorCopy(projectedVertices[source_vertex_index], vertices[target_vertex_index]);
				}
				else
				{
					VectorCopy(VertexTransform[i][source_vertex_index], vertices[target_vertex_index]);
					CalcShadowPosition(&vertices[target_vertex_index], BodyOrigin, sx, sy, projection);
				}
			}
		}
		else
		{
			for (int j = 0; j < mesh->NumTriangles; j++)
			{
				const auto* tp = &mesh->Triangles[j];
				for (int k = 0; k < tp->Polygon; k++)
				{
					const int source_vertex_index = tp->VertexIndex[k];
					target_vertex_index++;
					if (source_vertex_index >= 0 && source_vertex_index < projectedVertexCount)
						VectorCopy(projectedVertices[source_vertex_index], vertices[target_vertex_index]);
					else
					{
						VectorCopy(VertexTransform[i][source_vertex_index], vertices[target_vertex_index]);
						CalcShadowPosition(&vertices[target_vertex_index], BodyOrigin, sx, sy, projection);
					}
				}
			}
		}
	}

	if (target_vertex_index < 0)
	{
		return;
	}
	DrawProjectedShadowTriangles(vertices, target_vertex_index + 1);
}


void BMD::RenderBodyShadow(int BlendMesh, int HiddenMesh, int StartMeshNumber, int EndMeshNumber, void* pClothes, int ClothesCount, int MaxMeshes)
{
	if (g_pOption != NULL && g_pOption->GetRenderShadows() == false)
		return;

	if (NumMeshs == 0)
		return;

 CharacterStageTimer shadowTimer(g_ProfileShadowMs, g_ProfileShadowCalls);

	// Standalone monster/object shadows get their own stencil reference. Player
	// body/equipment shadows arrive inside one shared character group instead.
	const bool ownsStencilGroup = !g_LegacyShadowStencilActive &&
		BeginLegacyShadowStencilGroup();

	// Use regular alpha blending instead of the old opaque black pass.  This
	// makes the shadow read as a shadow rather than a black decal while the
	// helper keeps the legacy OpenGL state tracker synchronized.
	EnableAlphaBlend3();
	DisableTexture();
	DisableDepthMask();
	BeginRender(1.f);

	int startMesh = 0;
	int endMesh = NumMeshs;

	if (StartMeshNumber != -1)
		startMesh = StartMeshNumber;

	if (EndMeshNumber != -1)
		endMesh = EndMeshNumber;

	float sx = 2000.f;
	float sy = 4000.f;

	if (gMapManager->InBattleCastle())
	{
		sx = 2500.f;
		sy = 4000.f;
	}

	if (ClothesCount == 0)
	{
		AddMeshShadowTriangles(BlendMesh, HiddenMesh, startMesh, endMesh, sx, sy, MaxMeshes);
	}
	else
	{
		AddClothesShadowTriangles(pClothes, ClothesCount, sx, sy);
	}

	EndRender();
	EnableDepthMask();

	if (ownsStencilGroup)
	{
		EndLegacyShadowStencilGroup();
	}
	else if (!g_LegacyShadowCharacterGroup)
	{
		// Defensive cleanup when stencil de-duplication is disabled/unavailable.
		glDisable(GL_STENCIL_TEST);
	}

	// Return to the normal opaque render state.  RenderPartObjectEffect exits
	// immediately after a shadow pass, so restoring here prevents the new alpha
	// mode from leaking into the next object.
	DisableAlphaBlend();
}

void BMD::RenderObjectBoundingBox()
{
	DisableTexture();
	glPushMatrix();
	glTranslatef(BodyOrigin[0], BodyOrigin[1], BodyOrigin[2]);
	glScalef(BodyScale, BodyScale, BodyScale);
	for (int i = 0; i < NumBones; i++)
	{
		Bone_t* b = &Bones[i];
		if (b->BoundingBox)
		{
			vec3_t BoundingVertices[8];
			for (int j = 0; j < 8; j++)
			{
				VectorTransform(b->BoundingVertices[j], BoneTransform[i], BoundingVertices[j]);
			}

			glBegin(GL_QUADS);
			//glBegin(GL_LINES);
			glColor3f(0.2f, 0.2f, 0.2f);
			glTexCoord2f(1.0F, 1.0F);
			glVertex3fv(BoundingVertices[7]);
			glTexCoord2f(1.0F, 0.0F);
			glVertex3fv(BoundingVertices[6]);
			glTexCoord2f(0.0F, 0.0F);
			glVertex3fv(BoundingVertices[4]);
			glTexCoord2f(0.0F, 1.0F);
			glVertex3fv(BoundingVertices[5]);

			glColor3f(0.2f, 0.2f, 0.2f);
			glTexCoord2f(0.0F, 1.0F);
			glVertex3fv(BoundingVertices[0]);
			glTexCoord2f(1.0F, 1.0F);
			glVertex3fv(BoundingVertices[2]);
			glTexCoord2f(1.0F, 0.0F);
			glVertex3fv(BoundingVertices[3]);
			glTexCoord2f(0.0F, 0.0F);
			glVertex3fv(BoundingVertices[1]);

			glColor3f(0.6f, 0.6f, 0.6f);
			glTexCoord2f(1.0F, 1.0F);
			glVertex3fv(BoundingVertices[7]);
			glTexCoord2f(1.0F, 0.0F);
			glVertex3fv(BoundingVertices[3]);
			glTexCoord2f(0.0F, 0.0F);
			glVertex3fv(BoundingVertices[2]);
			glTexCoord2f(0.0F, 1.0F);
			glVertex3fv(BoundingVertices[6]);

			glColor3f(0.6f, 0.6f, 0.6f);
			glTexCoord2f(0.0F, 1.0F);
			glVertex3fv(BoundingVertices[0]);
			glTexCoord2f(1.0F, 1.0F);
			glVertex3fv(BoundingVertices[1]);
			glTexCoord2f(1.0F, 0.0F);
			glVertex3fv(BoundingVertices[5]);
			glTexCoord2f(0.0F, 0.0F);
			glVertex3fv(BoundingVertices[4]);

			glColor3f(0.4f, 0.4f, 0.4f);
			glTexCoord2f(1.0F, 1.0F);
			glVertex3fv(BoundingVertices[7]);
			glTexCoord2f(1.0F, 0.0F);
			glVertex3fv(BoundingVertices[5]);
			glTexCoord2f(0.0F, 0.0F);
			glVertex3fv(BoundingVertices[1]);
			glTexCoord2f(0.0F, 1.0F);
			glVertex3fv(BoundingVertices[3]);

			glColor3f(0.4f, 0.4f, 0.4f);
			glTexCoord2f(0.0F, 1.0F);
			glVertex3fv(BoundingVertices[0]);
			glTexCoord2f(1.0F, 1.0F);
			glVertex3fv(BoundingVertices[4]);
			glTexCoord2f(1.0F, 0.0F);
			glVertex3fv(BoundingVertices[6]);
			glTexCoord2f(0.0F, 0.0F);
			glVertex3fv(BoundingVertices[2]);
			glEnd();
		}
	}
	glPopMatrix();
	DisableAlphaBlend();
}

void BMD::RenderBone(float(*BoneMatrix)[3][4])
{
	DisableTexture();
	glDepthFunc(GL_ALWAYS);
	glColor3f(0.8f, 0.8f, 0.2f);
	for (int i = 0; i < NumBones; i++)
	{
		Bone_t* b = &Bones[i];
		if (!b->Dummy)
		{
			BoneMatrix_t* bm = &b->BoneMatrixes[CurrentAction];
			int Parent = b->Parent;
			if (Parent > 0)
			{
				float Scale = 1.f;
				float dx = bm->Position[CurrentAnimationFrame][0];
				float dy = bm->Position[CurrentAnimationFrame][1];
				float dz = bm->Position[CurrentAnimationFrame][2];
				Scale = sqrtf(dx * dx + dy * dy + dz * dz) * 0.05f;
				vec3_t Position[3];
				Vector(0.f, 0.f, -Scale, Position[0]);
				Vector(0.f, 0.f, Scale, Position[1]);
				Vector(0.f, 0.f, 0.f, Position[2]);
				vec3_t BoneVertices[3];
				VectorTransform(Position[0], BoneMatrix[Parent], BoneVertices[0]);
				VectorTransform(Position[1], BoneMatrix[Parent], BoneVertices[1]);
				VectorTransform(Position[2], BoneMatrix[i], BoneVertices[2]);
				for (int j = 0; j < 3; j++)
				{
					VectorMA(BodyOrigin, BodyScale, BoneVertices[j], BoneVertices[j]);
				}
				glBegin(GL_LINES);
				glVertex3fv(BoneVertices[0]);
				glVertex3fv(BoneVertices[1]);
				glEnd();
				glBegin(GL_LINES);
				glVertex3fv(BoneVertices[1]);
				glVertex3fv(BoneVertices[2]);
				glEnd();
				glBegin(GL_LINES);
				glVertex3fv(BoneVertices[2]);
				glVertex3fv(BoneVertices[0]);
				glEnd();
			}
		}
	}
	glDepthFunc(GL_LEQUAL);
}

void BlurShadow()
{
	for (int i = 1; i < ShadowBufferHeight - 1; i++)
	{
		unsigned char* ptr = &ShadowBuffer[i * ShadowBufferWidth];
		for (int j = 1; j < ShadowBufferWidth - 1; j++)
		{
			ptr[j] = (ptr[j - ShadowBufferWidth] + ptr[j + ShadowBufferWidth] +
				ptr[j - 1] + ptr[j + 1]) >> 2;
		}
	}
}

void BMD::Release()
{
	for (int i = 0; i < NumBones; i++)
	{
		Bone_t* b = &Bones[i];

		if (!b->Dummy)
		{
			for (int j = 0; j < NumActions; j++)
			{
				BoneMatrix_t* bm = &b->BoneMatrixes[j];

				if (bm)
				{
					SAFE_DELETE_ARRAY(bm->Position);
					SAFE_DELETE_ARRAY(bm->Rotation);
					SAFE_DELETE_ARRAY(bm->Quaternion);
				}
			}
			SAFE_DELETE_ARRAY(b->BoneMatrixes);
		}
	}

	for (int i = 0; i < NumActions; i++)
	{
		Action_t* a = &Actions[i];
		if (a->LockPositions)
		{
			SAFE_DELETE_ARRAY(a->Positions);
		}
	}

	if (Meshs)
	{
		for (int i = 0; i < NumMeshs; i++)
		{
			Mesh_t* m = &Meshs[i];

			SAFE_DELETE_ARRAY(m->Vertices);
			SAFE_DELETE_ARRAY(m->Normals);
			SAFE_DELETE_ARRAY(m->TexCoords);
			SAFE_DELETE_ARRAY(m->Triangles);
            auto baseCache=g_BaseTopology.find(m);
            if(baseCache!=g_BaseTopology.end()){
                if(baseCache->second.indexBuffer && baseCache->second.context==wglGetCurrentContext() && glDeleteBuffers)
                    glDeleteBuffers(1,&baseCache->second.indexBuffer);
                g_BaseTopology.erase(baseCache);
            }
			SAFE_DELETE_ARRAY(m->ExpandedVertexIndices);
			SAFE_DELETE_ARRAY(m->ExpandedNormalIndices);
			SAFE_DELETE_ARRAY(m->ExpandedTexCoords);
			SAFE_DELETE_ARRAY(m->ShadowVertexIndices);
			SAFE_DELETE_ARRAY(m->ExpandedPositions);
            if (m->GlowIndexBuffer && m->GlowIndexContext == wglGetCurrentContext() && glDeleteBuffers)
                glDeleteBuffers(1, &m->GlowIndexBuffer);
            m->GlowIndexBuffer = 0;
            m->GlowIndexContext = NULL;
			SAFE_DELETE_ARRAY(m->GlowIndices);
			SAFE_DELETE_ARRAY(m->GlowSourceCorners);
			m->GlowVertexCount = 0;

			if (m->m_csTScript)
			{
				delete m->m_csTScript;
				m->m_csTScript = NULL;
			}
			if (IndexTexture[m->Texture] != BITMAP_SKIN && IndexTexture[m->Texture] != BITMAP_HQSKIN && IndexTexture[m->Texture] != BITMAP_HQSKIN2 && IndexTexture[m->Texture] != BITMAP_HQSKIN3)
			{
				DeleteBitmap(IndexTexture[m->Texture]);

			}
		}
	}

	SAFE_DELETE_ARRAY(Meshs);
	SAFE_DELETE_ARRAY(Bones);
	SAFE_DELETE_ARRAY(Actions);
	SAFE_DELETE_ARRAY(Textures);
	SAFE_DELETE_ARRAY(IndexTexture);

	NumBones = 0;
	NumActions = 0;
	NumMeshs = 0;

#ifdef LDS_FIX_SETNULLALLOCVALUE_WHEN_BMDRELEASE
	m_bCompletedAlloc = false;
#endif // LDS_FIX_SETNULLALLOCVALUE_WHEN_BMDRELEASE
}

void BMD::FindNearTriangle(void)
{
	for (int iMesh = 0; iMesh < NumMeshs; iMesh++)
	{
		Mesh_t* m = &Meshs[iMesh];

		Triangle_t* pTriangle = m->Triangles;
		int iNumTriangles = m->NumTriangles;
		for (int iTri = 0; iTri < iNumTriangles; ++iTri)
		{
			for (int i = 0; i < 3; ++i)
			{
				pTriangle[iTri].EdgeTriangleIndex[i] = -1;
			}
		}
		for (int iTri = 0; iTri < iNumTriangles; ++iTri)
		{
			FindTriangleForEdge(iMesh, iTri, 0);
			FindTriangleForEdge(iMesh, iTri, 1);
			FindTriangleForEdge(iMesh, iTri, 2);
		}
	}
}

void BMD::FindTriangleForEdge(int iMesh, int iTri1, int iIndex11)
{
	if (iMesh >= NumMeshs || iMesh < 0) return;

	Mesh_t* m = &Meshs[iMesh];
	Triangle_t* pTriangle = m->Triangles;

	Triangle_t* pTri1 = &pTriangle[iTri1];
	if (pTri1->EdgeTriangleIndex[iIndex11] != -1)
	{
		return;
	}

	int iNumTriangles = m->NumTriangles;
	for (int iTri2 = 0; iTri2 < iNumTriangles; ++iTri2)
	{
		if (iTri1 == iTri2)
		{
			continue;
		}

		Triangle_t* pTri2 = &pTriangle[iTri2];
		int iIndex12 = (iIndex11 + 1) % 3;
		for (int iIndex21 = 0; iIndex21 < 3; ++iIndex21)
		{
			int iIndex22 = (iIndex21 + 1) % 3;
			if (pTri2->EdgeTriangleIndex[iIndex21] == -1 &&
				pTri1->VertexIndex[iIndex11] == pTri2->VertexIndex[iIndex22] &&
				pTri1->VertexIndex[iIndex12] == pTri2->VertexIndex[iIndex21])
			{
				pTri1->EdgeTriangleIndex[iIndex11] = iTri2;
				pTri2->EdgeTriangleIndex[iIndex21] = iTri1;
				return;
			}
		}
	}
}
//#endif //USE_SHADOWVOLUME

int BMD::FindBoneName(char* name)
{
	int iNum = -1;
	if (Bones)
	{
		for (int i = 0; i < NumBones; i++)
		{
			if (Strnicmp(Bones[i].Name, name))
			{
				return i;
			}
		}
	}
	return iNum;
}

bool BMD::GetBonePosition(OBJECT* pObject, char* name, OUT vec3_t Position)
{
	bool success = false;
	vec3_t LocalPos, Relative;
	int Bone = this->FindBoneName(name);

	Vector(0, 0, 0, Relative);

	if (Bone != -1)
	{
		this->TransformPosition(pObject->BoneTransform[Bone], Relative, LocalPos, false);
		VectorScale(LocalPos, pObject->Scale, LocalPos);
		VectorAdd(LocalPos, pObject->Position, Position);
		success = true;
	}
	return success;
}

bool BMD::Open(char* DirName, char* ModelFileName)
{
	char ModelName[64];
	strcpy(ModelName, DirName);
	strcat(ModelName, ModelFileName);
	FILE* fp = fopen(ModelName, "rb");
	if (fp == NULL)
	{
		return false;
	}
	fseek(fp, 0, SEEK_END);
	int DataBytes = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	unsigned char* Data = new unsigned char[DataBytes];
	fread(Data, 1, DataBytes, fp);
	fclose(fp);

	int Size;
	int DataPtr = 3;
	Version = *((char*)(Data + DataPtr)); DataPtr += 1;
	memcpy(Name, Data + DataPtr, 32); DataPtr += 32;
	NumMeshs = *((short*)(Data + DataPtr)); DataPtr += 2;
	NumBones = *((short*)(Data + DataPtr)); DataPtr += 2;
	NumActions = *((short*)(Data + DataPtr)); DataPtr += 2;

	Meshs = new Mesh_t[max<int>(1, NumMeshs)];
	Bones = new Bone_t[max<int>(1, NumBones)];
	Actions = new Action_t[max<int>(1, NumActions)];
	Textures = new Texture_t[max<int>(1, NumMeshs)];
	IndexTexture = new GLuint[max<int>(1, NumMeshs)];
	LightTexture = new GLuint[max<int>(1, NumMeshs)];

	int i;
	for (i = 0; i < NumMeshs; i++)
	{
		Mesh_t* m = &Meshs[i];
		m->NumVertices = *((short*)(Data + DataPtr)); DataPtr += 2;
		m->NumNormals = *((short*)(Data + DataPtr)); DataPtr += 2;
		m->NumTexCoords = *((short*)(Data + DataPtr)); DataPtr += 2;
		m->NumTriangles = *((short*)(Data + DataPtr)); DataPtr += 2;
		m->Texture = *((short*)(Data + DataPtr)); DataPtr += 2;
		m->NoneBlendMesh = false;
		//m->NumCommandBytes  = *((int   *)(Data+DataPtr));DataPtr+=4;
		m->Vertices = new Vertex_t[m->NumVertices];
		m->Normals = new Normal_t[m->NumNormals];
		m->TexCoords = new TexCoord_t[m->NumTexCoords];
		m->Triangles = new Triangle_t[m->NumTriangles];
		//m->Commands  = new unsigned char [m->NumCommandBytes];
		Size = m->NumVertices * sizeof(Vertex_t);
		memcpy(m->Vertices, Data + DataPtr, Size); DataPtr += Size;
		Size = m->NumNormals * sizeof(Normal_t);
		memcpy(m->Normals, Data + DataPtr, Size); DataPtr += Size;
		Size = m->NumTexCoords * sizeof(TexCoord_t);
		memcpy(m->TexCoords, Data + DataPtr, Size); DataPtr += Size;
		//Size = m->NumTriangles * sizeof(Triangle_t);
		//memcpy(m->Triangles,Data+DataPtr,Size);DataPtr+=Size;
		Size = sizeof(Triangle_t);
		int Size2 = sizeof(Triangle_t2);
		for (int j = 0; j < m->NumTriangles; j++)
		{
			memcpy(&m->Triangles[j], Data + DataPtr, Size); DataPtr += Size2;
		}
		//memcpy(m->Commands ,Data+DataPtr,m->NumCommandBytes);DataPtr+=m->NumCommandBytes;
		memcpy(Textures[i].FileName, Data + DataPtr, 32); DataPtr += 32;

		TextureScriptParsing TSParsing;

		if (TSParsing.parsingTScript(Textures[i].FileName))
		{
			m->m_csTScript = new TextureScript;
			m->m_csTScript->setScript((TextureScript&)TSParsing);
		}
		else
		{
			m->m_csTScript = NULL;
		}
	}
	//#ifdef USE_SHADOWVOLUME
		/*for(i=0;i<NumMeshs;i++)
		{
			Mesh_t *m = &Meshs[i];
			EdgeTriangleIndex(m->Triangles,m->NumTriangles);
		}*/
	FindNearTriangle();
	//#endif
	for (i = 0; i < NumActions; i++)
	{
		Action_t* a = &Actions[i];
		a->Loop = false;
		a->NumAnimationKeys = *((short*)(Data + DataPtr)); DataPtr += 2;
		a->LockPositions = *((bool*)(Data + DataPtr)); DataPtr += 1;
		if (a->LockPositions)
		{
			a->Positions = new vec3_t[a->NumAnimationKeys];
			Size = a->NumAnimationKeys * sizeof(vec3_t);
			memcpy(a->Positions, Data + DataPtr, Size); DataPtr += Size;
		}
	}
	for (i = 0; i < NumBones; i++)
	{
		Bone_t* b = &Bones[i];
		b->Dummy = *((char*)(Data + DataPtr)); DataPtr += 1;
		if (!b->Dummy)
		{
			memcpy(b->Name, Data + DataPtr, 32); DataPtr += 32;
			b->Parent = *((short*)(Data + DataPtr)); DataPtr += 2;
			b->BoneMatrixes = new BoneMatrix_t[NumActions];
			for (int j = 0; j < NumActions; j++)
			{
				BoneMatrix_t* bm = &b->BoneMatrixes[j];
				Size = Actions[j].NumAnimationKeys * sizeof(vec3_t);
				int NumAnimationKeys = Actions[j].NumAnimationKeys;
				bm->Position = new vec3_t[NumAnimationKeys];
				bm->Rotation = new vec3_t[NumAnimationKeys];
				bm->Quaternion = new vec4_t[NumAnimationKeys];
				memcpy(bm->Position, Data + DataPtr, Size); DataPtr += Size;
				memcpy(bm->Rotation, Data + DataPtr, Size); DataPtr += Size;
				for (int k = 0; k < NumAnimationKeys; k++)
				{
					AngleQuaternion(bm->Rotation[k], bm->Quaternion[k]);
				}
			}
		}
	}
	delete[] Data;
	Init(false);
	return true;
}

bool BMD::Save(char* DirName, char* ModelFileName)
{
	char ModelName[64];
	strcpy(ModelName, DirName);
	strcat(ModelName, ModelFileName);
	FILE* fp = fopen(ModelName, "wb");
	if (fp == NULL) return false;
	putc('B', fp);
	putc('M', fp);
	putc('D', fp);
	fwrite(&Version, 1, 1, fp);
	fwrite(Name, 32, 1, fp);
	fwrite(&NumMeshs, 2, 1, fp);
	fwrite(&NumBones, 2, 1, fp);
	fwrite(&NumActions, 2, 1, fp);
	int i;
	for (i = 0; i < NumMeshs; i++)
	{
		Mesh_t* m = &Meshs[i];
		fwrite(&m->NumVertices, 2, 1, fp);
		fwrite(&m->NumNormals, 2, 1, fp);
		fwrite(&m->NumTexCoords, 2, 1, fp);
		fwrite(&m->NumTriangles, 2, 1, fp);
		fwrite(&m->Texture, 2, 1, fp);
		//fwrite(&m->NumCommandBytes ,4,1,fp);
		fwrite(m->Vertices, m->NumVertices * sizeof(Vertex_t), 1, fp);
		fwrite(m->Normals, m->NumNormals * sizeof(Normal_t), 1, fp);
		fwrite(m->TexCoords, m->NumTexCoords * sizeof(TexCoord_t), 1, fp);
		//fwrite(m->Triangles,m->NumTriangles*sizeof(Triangle_t),1,fp);
		for (int j = 0; j < m->NumTriangles; j++)
		{
			fwrite(&m->Triangles[j], sizeof(Triangle_t2), 1, fp);
		}
		//fwrite(m->Commands ,m->NumCommandBytes                ,1,fp);
		fwrite(Textures[i].FileName, 32, 1, fp);
	}
	for (i = 0; i < NumActions; i++)
	{
		Action_t* a = &Actions[i];
		fwrite(&a->NumAnimationKeys, 2, 1, fp);
		fwrite(&a->LockPositions, 1, 1, fp);
		if (a->LockPositions)
		{
			fwrite(a->Positions, a->NumAnimationKeys * sizeof(vec3_t), 1, fp);
		}
	}
	for (i = 0; i < NumBones; i++)
	{
		Bone_t* b = &Bones[i];
		fwrite(&b->Dummy, 1, 1, fp);
		if (!b->Dummy)
		{
			fwrite(b->Name, 32, 1, fp);
			fwrite(&b->Parent, 2, 1, fp);
			for (int j = 0; j < NumActions; j++)
			{
				BoneMatrix_t* bm = &b->BoneMatrixes[j];
				fwrite(bm->Position, Actions[j].NumAnimationKeys * sizeof(vec3_t), 1, fp);
				fwrite(bm->Rotation, Actions[j].NumAnimationKeys * sizeof(vec3_t), 1, fp);
			}
		}
	}
	fclose(fp);
	return true;
}

void BMD::decripted_unique_lua(BYTE* DataBytes, int pos, int DataSize)
{
	std::vector<unsigned char> binaryLua(DataSize, 0);
	memcpy(binaryLua.data(), DataBytes, DataSize);

	unknown = DataSize;
	commandLua = new CGMLuaBase;

	if (commandLua->runtime_link(binaryLua) == false)
	{
		unknown = 0;
		delete commandLua;
		commandLua = NULL;
	}
}

bool BMD::Open2(char* DirName, char* ModelFileName, bool bReAlloc)
{
	if (true == m_bCompletedAlloc)
	{
		if (true == bReAlloc)
			Release();
		else
			return true;
	}

	char ModelName[MAX_PATH];
	strcpy(ModelName, DirName);
	strcat(ModelName, ModelFileName);

	FILE* fp = fopen(ModelName, "rb");

	if (fp == NULL)
	{
		m_bCompletedAlloc = false;
		return false;
	}
	strcpy(FileName, ModelFileName);

	fseek(fp, 0, SEEK_END);
	int fileSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	unsigned char* Data = new unsigned char[fileSize];
	//std::vector<BYTE> PakBuffer(fileSize, 0);

	fread(Data, 1, fileSize, fp);
	fclose(fp);

	//BYTE* Data = PakBuffer.data();

	int Size;
	int DataPtr = 3;

	Version = *((char*)(Data + DataPtr));
	DataPtr += 1;

	if (Version == 12 || Version == 0xE)
	{
		if (Data[0] == 'L' && Data[1] == 'U' && Data[2] == 'A')
		{
			int pos = DataPtr;
			long lSize = *((long*)(Data + pos));
			pos += 4;
			pos += lSize;

			long DataLen = *((long*)(Data + pos));
			pos += 4;

			if (DataLen > 0)
			{
				decripted_unique_lua((Data + pos), 0, DataLen);
			}
		}
	}

	if (Version == 12)
	{
		long lSize = *((long*)(Data + DataPtr)); DataPtr += sizeof(long);
		long lDecSize = MapFileDecrypt(NULL, Data + DataPtr, lSize);
		BYTE* pbyDec = new BYTE[lDecSize];
		MapFileDecrypt(pbyDec, Data + DataPtr, lSize);
		delete[] Data;
		Data = pbyDec;
		DataPtr = 0;
	}
	else if (Version == 0xE)
	{
		long lSize = *((long*)(Data + DataPtr));
		DataPtr += 4;
		long lDecSize = MuFileDecrypt(NULL, &Data[DataPtr], lSize);
		BYTE* pbyDec = new BYTE[lDecSize];
		MuFileDecrypt(pbyDec, &Data[DataPtr], lSize);
		delete[] Data;
		Data = pbyDec;
		DataPtr = 0;
	}

	memcpy(Name, Data + DataPtr, 32); DataPtr += 32;

	NumMeshs = *((short*)(Data + DataPtr)); DataPtr += 2;
	NumBones = *((short*)(Data + DataPtr)); DataPtr += 2;
	assert(NumBones <= MAX_BONES && "Bones 200");
	NumActions = *((short*)(Data + DataPtr)); DataPtr += 2;

	Meshs = new Mesh_t[max<int>(1, NumMeshs)];
	Bones = new Bone_t[max<int>(1, NumBones)];
	Actions = new Action_t[max<int>(1, NumActions)];
	Textures = new Texture_t[max<int>(1, NumMeshs)];
	IndexTexture = new GLuint[max<int>(1, NumMeshs)];
	LightTexture = new GLuint[max<int>(1, NumMeshs)];

	int i;

	for (i = 0; i < NumMeshs; i++)
	{
		Mesh_t* m = &Meshs[i];
		m->NumVertices = *((short*)(Data + DataPtr));
		DataPtr += 2;

		m->NumNormals = *((short*)(Data + DataPtr));
		DataPtr += 2;

		m->NumTexCoords = *((short*)(Data + DataPtr));
		DataPtr += 2;

		m->NumTriangles = *((short*)(Data + DataPtr));
		DataPtr += 2;

		m->Texture = *((short*)(Data + DataPtr));
		DataPtr += 2;

		m->NoneBlendMesh = false;
		m->Vertices = new Vertex_t[m->NumVertices];
		m->Normals = new Normal_t[m->NumNormals];
		m->TexCoords = new TexCoord_t[m->NumTexCoords];
		m->Triangles = new Triangle_t[m->NumTriangles];

		Size = m->NumVertices * sizeof(Vertex_t);
		memcpy(m->Vertices, Data + DataPtr, Size);
		DataPtr += Size;

		Size = m->NumNormals * sizeof(Normal_t);
		memcpy(m->Normals, Data + DataPtr, Size);
		DataPtr += Size;

		Size = m->NumTexCoords * sizeof(TexCoord_t);
		memcpy(m->TexCoords, Data + DataPtr, Size);
		DataPtr += Size;

		Size = sizeof(Triangle_t);

		for (int j = 0; j < m->NumTriangles; j++)
		{
			memcpy(&m->Triangles[j], Data + DataPtr, Size);
			DataPtr += sizeof(Triangle_t2);
		}

		memcpy(Textures[i].FileName, Data + DataPtr, 32);
		DataPtr += 32;

		TextureScriptParsing TSParsing;

		if (TSParsing.parsingTScript(Textures[i].FileName))
		{
			m->m_csTScript = new TextureScript;
			m->m_csTScript->setScript((TextureScript&)TSParsing);
		}
		else
		{
			m->m_csTScript = NULL;
		}

#ifdef SHADER_VERSION_TEST
		this->CreateVertexBuffer(i, *m);
#endif // SHADER_VERSION_TEST
	}

	for (i = 0; i < NumActions; i++)
	{
		Action_t* a = &Actions[i];
		a->Loop = false;
		a->NumAnimationKeys = *((short*)(Data + DataPtr));
		DataPtr += 2;

		a->LockPositions = *((bool*)(Data + DataPtr));
		DataPtr += 1;

		if (a->LockPositions)
		{
			a->Positions = new vec3_t[a->NumAnimationKeys];
			Size = a->NumAnimationKeys * sizeof(vec3_t);
			memcpy(a->Positions, Data + DataPtr, Size); DataPtr += Size;
		}
		else
		{
			a->Positions = NULL;
		}
	}

	for (i = 0; i < NumBones; i++)
	{
		Bone_t* b = &Bones[i];
		b->Dummy = *((char*)(Data + DataPtr));
		DataPtr += 1;

		if (!b->Dummy)
		{
			memcpy(b->Name, Data + DataPtr, 32);
			DataPtr += 32;

			b->Parent = *((short*)(Data + DataPtr));
			DataPtr += 2;

			b->BoneMatrixes = new BoneMatrix_t[NumActions];

			for (int j = 0; j < NumActions; j++)
			{
				BoneMatrix_t* bm = &b->BoneMatrixes[j];
				Size = Actions[j].NumAnimationKeys * sizeof(vec3_t);
				int NumAnimationKeys = Actions[j].NumAnimationKeys;
				bm->Position = new vec3_t[NumAnimationKeys];
				bm->Rotation = new vec3_t[NumAnimationKeys];
				bm->Quaternion = new vec4_t[NumAnimationKeys];

				memcpy(bm->Position, Data + DataPtr, Size);
				DataPtr += Size;

				memcpy(bm->Rotation, Data + DataPtr, Size);
				DataPtr += Size;

				for (int k = 0; k < NumAnimationKeys; k++)
				{
					AngleQuaternion(bm->Rotation[k], bm->Quaternion[k]);
				}
			}
		}
	}

	delete[] Data;
	Init(false);

	m_bCompletedAlloc = true;
	return true;
}

bool BMD::OpenPack(unsigned char* Data, long DataBytes, bool bReAlloc)
{
	if (true == m_bCompletedAlloc)
	{
		if (true == bReAlloc)
		{
			Release();
		}
		else
		{
			return true;
		}
	}

	if (Data == NULL || DataBytes == 0)
	{
		m_bCompletedAlloc = false;
		return false;
	}

	int Size;
	int DataPtr = 3;
	Version = *((char*)(Data + DataPtr));
	DataPtr += 1;

	if (Version == 12)
	{
		long lSize = *((long*)(Data + DataPtr)); DataPtr += sizeof(long);
		long lDecSize = MapFileDecrypt(NULL, Data + DataPtr, lSize);
		BYTE* pbyDec = new BYTE[lDecSize];
		MapFileDecrypt(pbyDec, Data + DataPtr, lSize);
		delete[] Data;
		Data = pbyDec;
		DataPtr = 0;
	}
	else if (Version == 14)
	{
		long lSize = *((long*)(Data + DataPtr));
		DataPtr += 4;
		long lDecSize = MuFileDecrypt(NULL, &Data[DataPtr], lSize);
		BYTE* pbyDec = new BYTE[lDecSize];
		MuFileDecrypt(pbyDec, &Data[DataPtr], lSize);
		delete[] Data;
		Data = pbyDec;
		DataPtr = 0;
	}
	else if (Version == 16)
	{
		//-- aqui va el script del render
	}
	else if (Version == 18)
	{
		//-- aqui va el script del render
	}

	memcpy(Name, Data + DataPtr, 32); DataPtr += 32;

	NumMeshs = *((short*)(Data + DataPtr)); DataPtr += 2;
	NumBones = *((short*)(Data + DataPtr)); DataPtr += 2;
	assert(NumBones <= MAX_BONES && "Bones 200");
	NumActions = *((short*)(Data + DataPtr)); DataPtr += 2;

	Meshs = new Mesh_t[max<int>(1, NumMeshs)];
	Bones = new Bone_t[max<int>(1, NumBones)];
	Actions = new Action_t[max<int>(1, NumActions)];
	Textures = new Texture_t[max<int>(1, NumMeshs)];
	IndexTexture = new GLuint[max<int>(1, NumMeshs)];
	LightTexture = new GLuint[max<int>(1, NumMeshs)];

	int i;

	for (i = 0; i < NumMeshs; i++)
	{
		Mesh_t* m = &Meshs[i];
		m->NumVertices = *((short*)(Data + DataPtr)); DataPtr += 2;
		m->NumNormals = *((short*)(Data + DataPtr)); DataPtr += 2;
		m->NumTexCoords = *((short*)(Data + DataPtr)); DataPtr += 2;
		m->NumTriangles = *((short*)(Data + DataPtr)); DataPtr += 2;
		m->Texture = *((short*)(Data + DataPtr)); DataPtr += 2;
		m->NoneBlendMesh = false;
		//m->NumCommandBytes  = *((int   *)(Data+DataPtr));DataPtr+=4;
		m->Vertices = new Vertex_t[m->NumVertices];
		m->Normals = new Normal_t[m->NumNormals];
		m->TexCoords = new TexCoord_t[m->NumTexCoords];
		m->Triangles = new Triangle_t[m->NumTriangles];
		//m->Commands  = new unsigned char [m->NumCommandBytes];
		Size = m->NumVertices * sizeof(Vertex_t);
		memcpy(m->Vertices, Data + DataPtr, Size); DataPtr += Size;
		Size = m->NumNormals * sizeof(Normal_t);
		memcpy(m->Normals, Data + DataPtr, Size); DataPtr += Size;
		Size = m->NumTexCoords * sizeof(TexCoord_t);
		memcpy(m->TexCoords, Data + DataPtr, Size); DataPtr += Size;
		//Size = m->NumTriangles * sizeof(Triangle_t);
		//memcpy(m->Triangles,Data+DataPtr,Size);DataPtr+=Size;
		Size = sizeof(Triangle_t);
		int Size2 = sizeof(Triangle_t2);

		for (int j = 0; j < m->NumTriangles; j++)
		{
			memcpy(&m->Triangles[j], Data + DataPtr, Size); DataPtr += Size2;
		}
		//memcpy(m->Commands ,Data+DataPtr,m->NumCommandBytes);DataPtr+=m->NumCommandBytes;
		memcpy(Textures[i].FileName, Data + DataPtr, 32); DataPtr += 32;

		TextureScriptParsing TSParsing;

		if (TSParsing.parsingTScript(Textures[i].FileName))
		{
			m->m_csTScript = new TextureScript;
			m->m_csTScript->setScript((TextureScript&)TSParsing);
		}
		else
		{
			m->m_csTScript = NULL;
		}
	}

	for (i = 0; i < NumActions; i++)
	{
		Action_t* a = &Actions[i];
		a->Loop = false;
		a->NumAnimationKeys = *((short*)(Data + DataPtr)); DataPtr += 2;
		a->LockPositions = *((bool*)(Data + DataPtr)); DataPtr += 1;
		if (a->LockPositions)
		{
			a->Positions = new vec3_t[a->NumAnimationKeys];
			Size = a->NumAnimationKeys * sizeof(vec3_t);
			memcpy(a->Positions, Data + DataPtr, Size); DataPtr += Size;
		}
		else
		{
			a->Positions = NULL;
		}
	}

	for (i = 0; i < NumBones; i++)
	{
		Bone_t* b = &Bones[i];
		b->Dummy = *((char*)(Data + DataPtr)); DataPtr += 1;
		if (!b->Dummy)
		{
			memcpy(b->Name, Data + DataPtr, 32); DataPtr += 32;
			b->Parent = *((short*)(Data + DataPtr)); DataPtr += 2;
			b->BoneMatrixes = new BoneMatrix_t[NumActions];
			for (int j = 0; j < NumActions; j++)
			{
				BoneMatrix_t* bm = &b->BoneMatrixes[j];
				Size = Actions[j].NumAnimationKeys * sizeof(vec3_t);
				int NumAnimationKeys = Actions[j].NumAnimationKeys;
				bm->Position = new vec3_t[NumAnimationKeys];
				bm->Rotation = new vec3_t[NumAnimationKeys];
				bm->Quaternion = new vec4_t[NumAnimationKeys];
				memcpy(bm->Position, Data + DataPtr, Size); DataPtr += Size;
				memcpy(bm->Rotation, Data + DataPtr, Size); DataPtr += Size;
				for (int k = 0; k < NumAnimationKeys; k++)
				{
					AngleQuaternion(bm->Rotation[k], bm->Quaternion[k]);
				}
			}
		}
	}

	Init(false);
	m_bCompletedAlloc = true;
	return true;
}

bool BMD::Save2(char* DirName, char* ModelFileName)
{
	char ModelName[64];
	strcpy(ModelName, DirName);
	strcat(ModelName, ModelFileName);
	FILE* fp = fopen(ModelName, "wb");
	if (fp == NULL)
		return false;
	putc('B', fp);
	putc('M', fp);
	putc('D', fp);
	Version = 12;
	fwrite(&Version, 1, 1, fp);

	BYTE* pbyBuffer = new BYTE[1024 * 1024];
	BYTE* pbyCur = pbyBuffer;
	memcpy(pbyCur, Name, 32); pbyCur += 32;
	memcpy(pbyCur, &NumMeshs, 2); pbyCur += 2;
	memcpy(pbyCur, &NumBones, 2); pbyCur += 2;
	memcpy(pbyCur, &NumActions, 2); pbyCur += 2;

	int i;
	for (i = 0; i < NumMeshs; i++)
	{
		Mesh_t* m = &Meshs[i];
		memcpy(pbyCur, &m->NumVertices, 2); pbyCur += 2;
		memcpy(pbyCur, &m->NumNormals, 2); pbyCur += 2;
		memcpy(pbyCur, &m->NumTexCoords, 2); pbyCur += 2;
		memcpy(pbyCur, &m->NumTriangles, 2); pbyCur += 2;
		memcpy(pbyCur, &m->Texture, 2); pbyCur += 2;
		//fwrite(&m->NumCommandBytes ,4,1,fp);
		memcpy(pbyCur, m->Vertices, m->NumVertices * sizeof(Vertex_t)); pbyCur += m->NumVertices * sizeof(Vertex_t);
		memcpy(pbyCur, m->Normals, m->NumNormals * sizeof(Normal_t)); pbyCur += m->NumNormals * sizeof(Normal_t);
		memcpy(pbyCur, m->TexCoords, m->NumTexCoords * sizeof(TexCoord_t)); pbyCur += m->NumTexCoords * sizeof(TexCoord_t);
		//fwrite(m->Triangles,m->NumTriangles*sizeof(Triangle_t),1,fp);
		for (int j = 0; j < m->NumTriangles; j++)
		{
			memcpy(pbyCur, &m->Triangles[j], sizeof(Triangle_t2)); pbyCur += sizeof(Triangle_t2);
		}
		//fwrite(m->Commands ,m->NumCommandBytes                ,1,fp);
		memcpy(pbyCur, Textures[i].FileName, 32); pbyCur += 32;
	}
	for (i = 0; i < NumActions; i++)
	{
		Action_t* a = &Actions[i];
		memcpy(pbyCur, &a->NumAnimationKeys, 2); pbyCur += 2;
		memcpy(pbyCur, &a->LockPositions, 1); pbyCur += 1;
		if (a->LockPositions)
		{
			memcpy(pbyCur, a->Positions, a->NumAnimationKeys * sizeof(vec3_t)); pbyCur += a->NumAnimationKeys * sizeof(vec3_t);
		}
	}
	for (i = 0; i < NumBones; i++)
	{
		Bone_t* b = &Bones[i];
		memcpy(pbyCur, &b->Dummy, 1); pbyCur += 1;
		if (!b->Dummy)
		{
			memcpy(pbyCur, b->Name, 32); pbyCur += 32;
			memcpy(pbyCur, &b->Parent, 2); pbyCur += 2;
			for (int j = 0; j < NumActions; j++)
			{
				BoneMatrix_t* bm = &b->BoneMatrixes[j];
				memcpy(pbyCur, bm->Position, Actions[j].NumAnimationKeys * sizeof(vec3_t));
				pbyCur += Actions[j].NumAnimationKeys * sizeof(vec3_t);
				memcpy(pbyCur, bm->Rotation, Actions[j].NumAnimationKeys * sizeof(vec3_t));
				pbyCur += Actions[j].NumAnimationKeys * sizeof(vec3_t);
			}
		}
	}
	long lSize = (long)(pbyCur - pbyBuffer);
	long lEncSize = MapFileEncrypt(NULL, pbyBuffer, lSize);
	BYTE* pbyEnc = new BYTE[lEncSize];
	MapFileEncrypt(pbyEnc, pbyBuffer, lSize);
	fwrite(&lEncSize, sizeof(long), 1, fp);
	fwrite(pbyEnc, lEncSize, 1, fp);
	fclose(fp);
	delete[] pbyBuffer;
	delete[] pbyEnc;
	return true;
}

void BMD::Init(bool Dummy)
{
	if (Dummy)
	{
		int i;
		for (i = 0; i < NumBones; i++)
		{
			Bone_t* b = &Bones[i];
			if (b->Name[0] == 'D' && b->Name[1] == 'u')
				b->Dummy = true;
			else
				b->Dummy = false;
		}
	}
	renderCount = 0;
	BoneHead = -1;
	StreamMesh = -1;
	CreateBoundingBox();
}

void BMD::CreateBoundingBox()
{
	for (int i = 0; i < NumBones; i++)
	{
		for (int j = 0; j < 3; j++)
		{
			BoundingMin[i][j] = 9999.0;
			BoundingMax[i][j] = -9999.0;
		}
		BoundingVertices[i] = 0;
	}

	for (int i = 0; i < NumMeshs; i++)
	{
		Mesh_t* m = &Meshs[i];
		for (int j = 0; j < m->NumVertices; j++)
		{
			Vertex_t* v = &m->Vertices[j];
			for (int k = 0; k < 3; k++)
			{
				if (v->Position[k] < BoundingMin[v->Node][k])
					BoundingMin[v->Node][k] = v->Position[k];

				if (v->Position[k] > BoundingMax[v->Node][k])
					BoundingMax[v->Node][k] = v->Position[k];
			}
			BoundingVertices[v->Node]++;
		}
	}
	for (int i = 0; i < NumBones; i++)
	{
		Bone_t* b = &Bones[i];
		if (BoundingVertices[i])
			b->BoundingBox = true;
		else
			b->BoundingBox = false;
		Vector(BoundingMax[i][0], BoundingMax[i][1], BoundingMax[i][2], b->BoundingVertices[0]);
		Vector(BoundingMax[i][0], BoundingMax[i][1], BoundingMin[i][2], b->BoundingVertices[1]);
		Vector(BoundingMax[i][0], BoundingMin[i][1], BoundingMax[i][2], b->BoundingVertices[2]);
		Vector(BoundingMax[i][0], BoundingMin[i][1], BoundingMin[i][2], b->BoundingVertices[3]);
		Vector(BoundingMin[i][0], BoundingMax[i][1], BoundingMax[i][2], b->BoundingVertices[4]);
		Vector(BoundingMin[i][0], BoundingMax[i][1], BoundingMin[i][2], b->BoundingVertices[5]);
		Vector(BoundingMin[i][0], BoundingMin[i][1], BoundingMax[i][2], b->BoundingVertices[6]);
		Vector(BoundingMin[i][0], BoundingMin[i][1], BoundingMin[i][2], b->BoundingVertices[7]);
	}
}

#include "CShaderGL.h"

void createPerspectiveMatrix(float* matrix, float fov, float aspect, float _near, float _far)
{
	float tanHalfFOV = tan(fov * 0.5f);
	float range = _near - _far;

	matrix[0] = 1.0f / (aspect * tanHalfFOV);
	matrix[1] = 0.0f;
	matrix[2] = 0.0f;
	matrix[3] = 0.0f;

	matrix[4] = 0.0f;
	matrix[5] = 1.0f / tanHalfFOV;
	matrix[6] = 0.0f;
	matrix[7] = 0.0f;

	matrix[8] = 0.0f;
	matrix[9] = 0.0f;
	matrix[10] = (_far + _near) / range;
	matrix[11] = -1.0f;

	matrix[12] = 0.0f;
	matrix[13] = 0.0f;
	matrix[14] = (2.0f * _far * _near) / range;
	matrix[15] = 0.0f;
}

void multiplyMatrices(float* result, const float* mat1, const float* mat2)
{
	// Multiplicación de matrices 4x4
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			result[i * 4 + j] = 0.0f;
			for (int k = 0; k < 4; k++) {
				result[i * 4 + j] += mat1[i * 4 + k] * mat2[k * 4 + j];
			}
		}
	}
}

void rotateMatrix(float* matrix, float angle, float x, float y, float z)
{
	// Calculamos la matriz de rotación usando OpenGL
	float rad = angle * (Q_PI / 180.f);
	float cosA = cosf(rad);
	float sinA = sinf(rad);

	// Matriz de rotación 3D en torno al eje (x, y, z)
	float rot[16] = {
		cosA + (1 - cosA) * x * x,        (1 - cosA) * x * y - sinA * z, (1 - cosA) * x * z + sinA * y, 0.0f,
		(1 - cosA) * y * x + sinA * z,    cosA + (1 - cosA) * y * y,      (1 - cosA) * y * z - sinA * x, 0.0f,
		(1 - cosA) * z * x - sinA * y,    (1 - cosA) * z * y + sinA * x,  cosA + (1 - cosA) * z * z,      0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	};

	// Multiplicamos la matriz actual por la matriz de rotación
	multiplyMatrices(matrix, matrix, rot);
}

void translateMatrix(float* matrix, float tx, float ty, float tz)
{
	// Matriz de traslación
	float translation[16] = {
		1.0f, 0.0f, 0.0f, tx,
		0.0f, 1.0f, 0.0f, ty,
		0.0f, 0.0f, 1.0f, tz,
		0.0f, 0.0f, 0.0f, 1.0f
	};

	// Multiplicamos la matriz actual por la matriz de traslación
	multiplyMatrices(matrix, matrix, translation);
}

void createViewMatrix(float* matrix, float* cameraPosition, float* cameraAngles, bool cameraTopViewEnable)
{
	// Inicializamos la matriz a la identidad
	float identity[16] = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	};

	// Primero, copiamos la matriz de identidad al arreglo final
	memcpy(matrix, identity, sizeof(float) * 16);

	// Realizamos las rotaciones de la cámara según los ángulos
	// Rotación en el eje Y (alrededor de Y)
	rotateMatrix(matrix, cameraAngles[1], 0.f, 1.f, 0.f);

	// Si la vista no es superior, aplicamos la rotación en el eje X
	if (!cameraTopViewEnable) {
		rotateMatrix(matrix, cameraAngles[0], 1.f, 0.f, 0.f);
	}

	// Rotación en el eje Z (alrededor de Z)
	rotateMatrix(matrix, cameraAngles[2], 0.f, 0.f, 1.f);

	// Aplicamos la traslación para posicionar la cámara (en el espacio mundial)
	translateMatrix(matrix, -cameraPosition[0], -cameraPosition[1], -cameraPosition[2]);
}


void BMD::CreateVertexBuffer(int i, Mesh_t& mesh)
{
#ifdef SHADER_VERSION_TEST
	GLuint shader_id = gShaderGL->GetShaderId();

	if (shader_id != 0)
	{
		auto vertices = RenderArrayVertices;
		auto colors = RenderArrayColors;
		auto textCoords = RenderArrayTexCoords;
		std::vector<unsigned short> indices;

		int target_vertex_index = -1;

		for (int j = 0; j < mesh.NumTriangles; j++)
		{
			Triangle_t* triangle = &mesh.Triangles[j];

			for (int k = 0; k < triangle->Polygon; k++)
			{
				int source_vertex_index = triangle->VertexIndex[k];

				target_vertex_index++;

				VectorCopy(VertexTransform[i][source_vertex_index], vertices[target_vertex_index]);

				TexCoord_t* textcoord = &mesh.TexCoords[triangle->TexCoordIndex[k]];
				textCoords[target_vertex_index][0] = 0.0;
				textCoords[target_vertex_index][1] = 0.0;
				colors[target_vertex_index][0] = 1.f;
				colors[target_vertex_index][1] = 1.f;
				colors[target_vertex_index][2] = 1.f;
				colors[target_vertex_index][3] = 1.f;
				indices.push_back(target_vertex_index);
			}
		}


		glGenVertexArrays(1, &mesh.VAO);
		glGenBuffers(1, &mesh.VBO_Vertices);
		glGenBuffers(1, &mesh.VBO_TexCoords);
		glGenBuffers(1, &mesh.VBO_Colors);
		glGenBuffers(1, &mesh.EBO);

		glBindVertexArray(mesh.VAO);
		// Vertices
		glBindBuffer(GL_ARRAY_BUFFER, mesh.VBO_Vertices);
		glBufferData(GL_ARRAY_BUFFER, indices.size() * sizeof(vec3_t), vertices, GL_STATIC_DRAW);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vec3_t), (void*)0);
		glEnableVertexAttribArray(0);
		// Coordenadas de Textura
		glBindBuffer(GL_ARRAY_BUFFER, mesh.VBO_TexCoords);
		glBufferData(GL_ARRAY_BUFFER, indices.size() * sizeof(vec2_t), textCoords, GL_STATIC_DRAW);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vec2_t), (void*)0);
		glEnableVertexAttribArray(1);
		// Color de la textura
		glBindBuffer(GL_ARRAY_BUFFER, mesh.VBO_Colors);
		glBufferData(GL_ARRAY_BUFFER, indices.size() * sizeof(vec4_t), colors, GL_STATIC_DRAW);
		glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(vec4_t), (void*)0);
		glEnableVertexAttribArray(2);
		// Indices de los Triangulos
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.EBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned short), indices.data(), GL_STATIC_DRAW);
		glBindVertexArray(0); // Desvinculamos el VAO
	}
#endif // SHADER_VERSION_TEST
}

void BMD::RenderVertexBuffer(int i, Mesh_t* m, int vertex_index, vec3_t* vertices, vec2_t* textCoords, vec4_t* colors)
{
#ifdef SHADER_VERSION_TEST
	GLuint shader_id = gShaderGL->GetShaderId();

	if (shader_id != 0)
	{
		gShaderGL->run_projection();

		glBindBuffer(GL_ARRAY_BUFFER, m->VBO_Vertices);
		glBufferSubData(GL_ARRAY_BUFFER, 0, vertex_index * sizeof(vec3_t), vertices);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		//--
		glBindBuffer(GL_ARRAY_BUFFER, m->VBO_TexCoords);
		glBufferSubData(GL_ARRAY_BUFFER, 0, vertex_index * sizeof(vec2_t), textCoords);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		//--
		if (colors != NULL)
		{
			glBindBuffer(GL_ARRAY_BUFFER, m->VBO_Colors);
			glBufferSubData(GL_ARRAY_BUFFER, 0, vertex_index * sizeof(vec4_t), colors);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
		}

		glUseProgram(shader_id);
		glBindVertexArray(m->VAO);
		glDrawElements(GL_TRIANGLES, vertex_index, GL_UNSIGNED_SHORT, 0);
		glBindVertexArray(0);
		glUseProgram(0);
	}
	else
	{
		glEnableClientState(GL_VERTEX_ARRAY);
		if (colors != NULL) glEnableClientState(GL_COLOR_ARRAY);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);

		glVertexPointer(3, GL_FLOAT, 0, vertices);
		if (colors != NULL) glColorPointer(4, GL_FLOAT, 0, colors);
		glTexCoordPointer(2, GL_FLOAT, 0, textCoords);
		glDrawArrays(GL_TRIANGLES, 0, m->NumTriangles * 3);

		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		if (colors != NULL) glDisableClientState(GL_COLOR_ARRAY);
		glDisableClientState(GL_VERTEX_ARRAY);
	}
#endif // SHADER_VERSION_TEST
}


BMD::~BMD()
{
	Release();
}

void BMD::InterpolationTrans(float(*Mat1)[4], float(*TransMat2)[4], float _Scale)
{
	TransMat2[0][3] = TransMat2[0][3] - (TransMat2[0][3] - Mat1[0][3]) * (1 - _Scale);
	TransMat2[1][3] = TransMat2[1][3] - (TransMat2[1][3] - Mat1[1][3]) * (1 - _Scale);
	TransMat2[2][3] = TransMat2[2][3] - (TransMat2[2][3] - Mat1[2][3]) * (1 - _Scale);
}

Action_t* BMD::GetAction(int i)
{
	if (i >= 0 && i < NumActions)
	{
		return &Actions[i];
	}
	return nullptr;
}

Mesh_t* BMD::GetMesh(int i)
{
	if (i >= 0 && i < NumMeshs)
	{
		return &Meshs[i];
	}
	return nullptr;
}

bool BMD::IsTextureHide(int index)
{
	if (Strnicmp(Textures[index].FileName, "blur02_mono")
		|| Strnicmp(Textures[index].FileName, "ground_wind2")
		|| Strnicmp(Textures[index].FileName, "line_of_big_R")
		|| Strnicmp(Textures[index].FileName, "fire_15frame")
		|| Strnicmp(Textures[index].FileName, "fire_15fmono")
		|| Strnicmp(Textures[index].FileName, "fire_15fmono_blue")
		|| Strnicmp(Textures[index].FileName, "damage01mono")
		|| Strnicmp(Textures[index].FileName, "gra2")
		|| Strnicmp(Textures[index].FileName, "gra3")
		|| Strnicmp(Textures[index].FileName, "lightmarks")
		|| Strnicmp(Textures[index].FileName, "s195")
		|| Strnicmp(Textures[index].FileName, "fire_sq")
		)
	{
		return true;
	}
	return false;
}

#ifdef EFFECT_KERNEL_VERTEX
void BMD::runtime_render_level(OBJECT* pObj, int GroupId, float r, float g, float b)
{
	vec3_t body_light;
	VectorCopy(BodyLight, body_light);

	this->SetBodyLight(1.f, 1.f, 1.f);
	RenderMesh(GroupId, RENDER_TEXTURE | RENDER_BRIGHT | RENDER_CHROME2, pObj->Alpha, GroupId, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);

	this->SetBodyLight((r / 255.0f), (g / 255.0f), (b / 255.0f));
	RenderMesh(GroupId, RENDER_BRIGHT | RENDER_CHROME, pObj->Alpha, GroupId, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_CHROME);
	RenderMesh(GroupId, RENDER_BRIGHT | RENDER_METAL, pObj->Alpha, GroupId, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_SHINY);

	VectorCopy(body_light, BodyLight);
}

bool BMD::runtime_make_render(int EffectIndex, OBJECT* pObj)
{
	float fLuminosity;
	float blend_mesh_light;
	float blend_mesh_tex_coord_u;
	float blend_mesh_tex_coord_v;
	vec3_t body_light;

	VectorCopy(BodyLight, body_light);

	switch (EffectIndex)
	{
	case Blessed_Divine_Staff:
	{
		glColor3fv(BodyLight);
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(3, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(3, 576, pObj->Alpha, 3, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_black_gold_crom05);
		blend_mesh_tex_coord_u = ((int)(1000 - timeGetTime()) % 1001) / (float)1000;
		BodyLight[0] = 1.f; BodyLight[1] = 0.9f; BodyLight[2] = 0.7f;
		RenderMesh(4, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 4, pObj->BlendMeshLight, blend_mesh_tex_coord_u, pObj->BlendMeshTexCoordV, -1);
		BodyLight[0] = 0.f; BodyLight[1] = 0.1f; BodyLight[2] = 1.f;
		RenderMesh(5, RENDER_TEXTURE | RENDER_BRIGHT, 1.f, 5, 1.f, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
	}
	break;
	case Blessed_Divine_Sword:
	{
		glColor3fv(BodyLight);
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);

		this->SetBodyLight(0.0, 0.0, 0.1);
		RenderMesh(3, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		VectorCopy(body_light, BodyLight);

		fLuminosity = Render22(-4.f, 0.f) * 0.4f + 0.6f;
		RenderMesh(0, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 0, fLuminosity, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_absolute02_swordrender);
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, pObj->BlendMesh, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_absolute02_swordrender);

		this->SetBodyLight(0.7, 0.8, 1.0);
		RenderMesh(3, RENDER_TEXTURE | RENDER_CHROME | RENDER_BRIGHT, pObj->Alpha, 3, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);

		this->SetBodyLight(0.7, 0.9, 1.0);
		blend_mesh_tex_coord_u = (float)(((1000 - timeGetTime()) % 1001) / (float)1000);
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, pObj->BlendMeshLight, blend_mesh_tex_coord_u, pObj->BlendMeshTexCoordV, -1);
	}
	break;
	case Blessed_Divine_CrossBow:
	{
		glColor3fv(BodyLight);
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(4, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(0, RENDER_TEXTURE | RENDER_CHROME | RENDER_BRIGHT, pObj->Alpha, 0, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_black_gold_crom05);
		float blessedcb03 = (((int)((timeGetTime()) * 0.08f)) % 16 / 4) * 0.25f;
		RenderMesh(5, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 5, pObj->BlendMeshLight, blessedcb03, pObj->BlendMeshTexCoordV, -1);
		float blessedcb01 = (((int)((1000 - timeGetTime())) % 1001)) / (float)1000;
		BodyLight[0] = 1.f; BodyLight[1] = 0.9f; BodyLight[2] = 0.5f;
		RenderMesh(3, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 3, pObj->BlendMeshLight, blessedcb01, pObj->BlendMeshTexCoordV, -1);
	}
	break;
	case Blessed_Divine_Scepter:
	{
		glColor3fv(BodyLight);
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		BodyLight[0] = 0.5f; BodyLight[1] = 0.5f; BodyLight[2] = 1.f;
		RenderMesh(2, RENDER_TEXTURE | RENDER_CHROME | RENDER_BRIGHT, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_black_gold_crom03);
		blend_mesh_tex_coord_u = (int)(1000 - timeGetTime()) % 1001 / (float)1000;
		RenderMesh(3, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 3, pObj->BlendMeshLight, blend_mesh_tex_coord_u, pObj->BlendMeshTexCoordV, -1);
	}
	break;
	case Blessed_Divine_Stick:
	{
		glColor3fv(BodyLight);
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(4, 576, pObj->Alpha, 4, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_black_gold_crom05);
		RenderMesh(5, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		BodyLight[0] = 1.f; BodyLight[1] = 0.9f; BodyLight[2] = 0.5f;
		blend_mesh_tex_coord_u = (int)(1000 - timeGetTime()) % 1001 / (float)1000;
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, pObj->BlendMeshLight, blend_mesh_tex_coord_u, pObj->BlendMeshTexCoordV, -1);
		BodyLight[0] = 0.f; BodyLight[1] = 0.1f; BodyLight[2] = 1.f;
		RenderMesh(3, RENDER_TEXTURE | RENDER_BRIGHT, 1.0, 3, 1.0, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_gra);
	}
	break;
	case Blessed_Divine_Claw:
	{
		glColor3fv(BodyLight);
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, 1.f, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		BodyLight[0] = 1.f; BodyLight[1] = 0.9f; BodyLight[2] = 0.5f;
		blend_mesh_tex_coord_u = (int)(1000 - timeGetTime()) % 1001 / (float)1000;
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, 1.f, blend_mesh_tex_coord_u, pObj->BlendMeshTexCoordV, -1);
	}
	break;
	case Blessed_Divine_Lancer:
	{
		glColor3fv(BodyLight);
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		fLuminosity = Render22(-1.2f, 0.f) + 0.3f;
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, fLuminosity, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		fLuminosity = (int)(rand() % 15 / 37.5) + 0.5f;

		BodyLight[0] = fLuminosity * 0.8f;
		BodyLight[1] = fLuminosity * 0.6f;
		BodyLight[2] = fLuminosity * 0.4f;

		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		fLuminosity = 1.f - Render22(4.f, 0.f);

		BodyLight[0] = 1.f;
		BodyLight[1] = 1.f;
		BodyLight[2] = 1.f;

		RenderMesh(3, RENDER_TEXTURE, pObj->Alpha, 3, 1.f, fLuminosity, pObj->BlendMeshTexCoordV, -1);
	}
	break;
	case Blessed_Divine_Mace:
	{
		glColor3fv(BodyLight);
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		blend_mesh_tex_coord_u = 0.65f - Render22(4.f, 0.f);

		BodyLight[0] = 0.7f;
		BodyLight[1] = 0.9f;
		BodyLight[2] = 1.f;

		RenderMesh(3, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 3, pObj->BlendMeshLight, blend_mesh_tex_coord_u, pObj->BlendMeshTexCoordV, -1);

		fLuminosity = (int)(rand() % 20 / 32.5) + 0.4f;

		BodyLight[0] = 0.2f;
		BodyLight[1] = 0.5f;
		BodyLight[2] = 1.f;
		RenderMesh(4, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 4, fLuminosity, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_gra3);
	}
	break;
	case Blessed_Divine_ShortSword:
	{
		glColor3fv(BodyLight);
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		fLuminosity = (Render22(-4.f, 0.f) * 0.8f + 0.2f);
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, fLuminosity, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);

		blend_mesh_tex_coord_u = ((int)timeGetTime() % 2000) * 0.0005f;
		blend_mesh_tex_coord_v = ((int)timeGetTime() % 2000) * 0.0005f;
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, fLuminosity, blend_mesh_tex_coord_u, blend_mesh_tex_coord_v, BITMAP_black_gold_crom05);

		fLuminosity = Render22(-8.f, 0.f);
		RenderMesh(3, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 3, fLuminosity, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);

		int Texture = timeGetTime() % 3 + BITMAP_LIGHTNING_MEGA1;
		RenderMesh(4, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 4, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, Texture);

		fLuminosity = (int)(rand() % 20 / 32.5) + 0.4f;
		BodyLight[0] = fLuminosity * 1.f;
		BodyLight[1] = fLuminosity * 0.8f;
		BodyLight[2] = fLuminosity * 0.4f;
		RenderMesh(6, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 6, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);

		BodyLight[0] = 1.f;
		BodyLight[1] = 0.6f;
		BodyLight[2] = 0.05f;
		blend_mesh_tex_coord_u = ((int)timeGetTime() % 800) * 0.00125f;
		RenderMesh(5, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 5, pObj->BlendMeshLight, -blend_mesh_tex_coord_u, pObj->BlendMeshTexCoordV, BITMAP_ground_wind2);
	}
	break;
	case Heavenly_Wings:
	{
		glColor3fv(BodyLight);
		blend_mesh_light = Render22(-2.f, 0.f) * 0.7f + 0.2f;

		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, 1, blend_mesh_light, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_magicianwing4da_render);
		runtime_render_level(pObj, 0, 182, 131, 69);
	}
	break;
	case Avatar_Wings:
	{
		glColor3fv(BodyLight);
		float blend_mesh_light = Render22(-1.75f, 0.f);

		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(3, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		int Avatar1 = (((int)((timeGetTime()) * 0.75f)) % 600 / 40);
		blend_mesh_tex_coord_u = (float)(Avatar1 % 4) * 0.25f;
		blend_mesh_tex_coord_v = (float)(Avatar1 / 4) * 0.25f;
		RenderMesh(4, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 4, pObj->BlendMeshLight, blend_mesh_tex_coord_u, blend_mesh_tex_coord_v, -1);
		RenderMesh(2, RENDER_TEXTURE, pObj->Alpha, 2, blend_mesh_light, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_dknightwing4db_render);
		runtime_render_level(pObj, 0, 212, 200, 170);
	}
	break;
	case Orb_Wings:
	{
		glColor3fv(BodyLight);
		float elfwing4da = Render22(-4.f, 0.f) * 0.5f + 0.5f;
		float elfwing4da_render01 = Render22(-4.f, 120.f);
		float elfwing4da_render02 = Render22(-4.f, 240.f);

		RenderMesh(2, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, elfwing4da, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, elfwing4da_render01, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_elfwing4da_render01);
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, elfwing4da_render02, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_elfwing4da_render02);
		RenderMesh(0, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 0, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(3, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 3, elfwing4da, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		runtime_render_level(pObj, 2, 255, 166, 124);
	}
	break;
	case Fall_Wings:
	{
		glColor3fv(BodyLight);
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(3, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(4, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		int fall1 = (int)timeGetTime() % 600 / 40;
		blend_mesh_tex_coord_u = (float)(fall1 % 4) * 0.25f;
		blend_mesh_tex_coord_v = (float)(fall1 / 4) * 0.25f;
		BodyLight[0] = 0.50f;
		BodyLight[1] = 0.50f;
		BodyLight[2] = 0.50f;
		RenderMesh(0, RENDER_TEXTURE | RENDER_BRIGHT, 1.f, 0, pObj->BlendMeshLight, blend_mesh_tex_coord_u, blend_mesh_tex_coord_v, -1);
		runtime_render_level(pObj, 1, 204, 190, 151);
		runtime_render_level(pObj, 2, 204, 190, 151);
		runtime_render_level(pObj, 3, 204, 190, 151);
	}
	break;
	case null01:
		RenderBody(2, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1, -1);
		break;
	case The_Wings:
	{
		glColor3fv(BodyLight);
		float alicewing4db_render = Render22(-4.f, 0.f) * 0.1f + 0.3f;
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, RENDER_TEXTURE, pObj->Alpha, 2, alicewing4db_render, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_alicewing4db_render);
		runtime_render_level(pObj, 0, 255, 20, 20);
	}
	break;
	case null02:
		RenderBody(2, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1, -1);
		break;
	case Holyangel_Sword:
	{
		glColor3fv(BodyLight);
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float v401 = ((int)timeGetTime() % 2000) * 0.0005f;
		float v403 = -v401;
		RenderMesh(3, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 3, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, v403, BITMAP_line_of_big_R);
	}
	break;
	case Holyangel_MagicSword:
	{
		glColor3fv(BodyLight);
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float v641 = Render22(-4.f, 0.f) + 0.3f;
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, v641, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float v401 = ((int)timeGetTime() % 2000) * 0.0005f;
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, v401, BITMAP_line_of_big_R);
	}
	break;
	case Holyangel_Scepter:
	{
		glColor3fv(BodyLight);
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		BodyLight[0] = 0.1f;
		BodyLight[1] = 0.2f;
		BodyLight[2] = 1.0f;
		float v401 = ((int)timeGetTime() % 2000) * 0.0005f;
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, 1.0, 1, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, v401, BITMAP_line_of_big_R);
		float HolyangelSc01 = (float)(rand() % 15) / 37.5f;
		float HolyangelSc02 = HolyangelSc01 + 0.5f;
		BodyLight[0] = 0.25f;
		BodyLight[1] = 0.7f;
		BodyLight[2] = 1.f;
		RenderMesh(3, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 3, HolyangelSc02, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_gra2);
	}
	break;
	case Holyangel_Staff:
	{
		glColor3fv(BodyLight);
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(3, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 3, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		int v655 = (int)timeGetTime() % 600 / 40;
		blend_mesh_tex_coord_u = (float)(v655 % 4) * 0.25f;
		blend_mesh_tex_coord_v = (float)(v655 / 4) * 0.25f;
		BodyLight[0] = 0.2f;
		BodyLight[1] = 0.45f;
		BodyLight[2] = 1.f;
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, pObj->BlendMeshLight, blend_mesh_tex_coord_u, blend_mesh_tex_coord_v, -1);
		float v645 = Render22(-6.f, 0.f) + 0.3f;
		RenderMesh(4, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 4, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(4, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 4, v645, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
	}
	break;
	case Holyangel_Stick:
	{
		glColor3fv(BodyLight);
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		int v655 = (int)timeGetTime() % 600 / 40;
		blend_mesh_tex_coord_u = (float)(v655 % 4) * 0.25f;
		blend_mesh_tex_coord_v = (float)(v655 / 4) * 0.25f;
		BodyLight[0] = 0.3f;
		BodyLight[1] = 0.6f;
		BodyLight[2] = 1.f;
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, pObj->BlendMeshLight, blend_mesh_tex_coord_u, blend_mesh_tex_coord_v, -1);
	}
	break;
	case Holyangel_Claw:
	{
		glColor3fv(BodyLight);
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(0, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float v638 = Render22(-4.f, 0.f) + 0.3f;
		BodyLight[0] = 0.15f;
		BodyLight[1] = 0.15f;
		BodyLight[2] = 1.f;
		RenderMesh(0, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 0, v638, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float v551 = (int)(1000 - timeGetTime()) % 1001 / (float)1000;
		BodyLight[0] = 0.6f;
		BodyLight[1] = 0.6f;
		BodyLight[2] = 1.f;
		RenderMesh(0, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 0, 1.f, v551, pObj->BlendMeshTexCoordV, BITMAP_black_gold_crom05);
		BodyLight[0] = 0.5f;
		BodyLight[1] = 0.5f;
		BodyLight[2] = 1.f;
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, 1.f, pObj->BlendMeshTexCoordU, v551, BITMAP_line_of_big_R);
	}
	break;
	case Awakening_Sword:
	{
		glColor3fv(BodyLight);
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float v525 = Render22(-4.f, 0.f);
		RenderMesh(0, RENDER_TEXTURE, 1.f, 0, v525, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_soulknightswordB_render);
		RenderMesh(1, RENDER_TEXTURE, 1.f, 1, v525, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_soulknightswordA_render);
		BodyLight[0] = 1.f;
		BodyLight[1] = 0.9f;
		BodyLight[2] = 0.4f;
		RenderMesh(3, RENDER_TEXTURE | RENDER_BRIGHT, 1.f, 3, 1.f, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_gra3);
	}
	break;
	case Awakening_MagicSword:
	{
		glColor3fv(BodyLight);
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float v525 = Render22(-4.f, 0.f);
		float MagicSword = ((int)timeGetTime() % 2000) * 0.0005f;
		RenderMesh(0, RENDER_TEXTURE, 1.f, 0, v525, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_soulmagicswordA_render);
		BodyLight[0] = 0.5f;
		BodyLight[1] = 0.5f;
		BodyLight[2] = 0.5f;
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, MagicSword, BITMAP_flame_chrome);
	}
	break;
	case Awakening_Bow:
	{
		glColor3fv(BodyLight);
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(3, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(4, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(5, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float v525 = Render22(-4.f, 0.f);
		RenderMesh(0, RENDER_TEXTURE, 1.0, 0, v525, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_hs_soularcherbowA01_render);
		BodyLight[0] = 0.5f;
		BodyLight[1] = 0.5f;
		BodyLight[2] = 0.5f;
		float v401 = ((int)timeGetTime() % 2000) * 0.0005f;
		int v655 = (int)timeGetTime() % 600 / 40;
		float v657 = (float)(v655 % 4) * 0.25f;
		float v659 = (float)(v655 / 4) * 0.25f;
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, v401, BITMAP_flame_chrome);
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, pObj->BlendMeshLight, v657, v659, -1);
	}
	break;
	case Awakening_Staff:
	{
		glColor3fv(BodyLight);
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float v525 = Render22(-4.f, 0.f);
		RenderMesh(1, RENDER_TEXTURE, 1.f, 1, v525, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_sd_SoulWizardStaff01_render);
		int v655 = (int)timeGetTime() % 600 / 40;
		float v657 = (float)(v655 % 4) * 0.25f;
		float v659 = (float)(v655 / 4) * 0.25f;
		BodyLight[0] = 1.f;
		BodyLight[1] = 1.f;
		BodyLight[2] = 1.f;
		RenderMesh(3, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 3, pObj->BlendMeshLight, v657, v659, -1);
	}
	break;
	case Awakening_Stick:
	{
		glColor3fv(BodyLight);
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float v525 = Render22(-4.f, 0.f);
		RenderMesh(0, RENDER_TEXTURE, 1.f, 0, v525, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_sd_soulsummonerstickA02_render);
		BodyLight[0] = 0.5f;
		BodyLight[1] = 0.5f;
		BodyLight[2] = 0.5f;
		float v401 = ((int)timeGetTime() % 2000) * 0.0005f;
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, v401, BITMAP_flame_chrome);
	}
	break;
	case Awakening_Claw:
	{
		glColor3fv(BodyLight);
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float v525 = Render22(-4.f, 0.f);
		RenderMesh(0, RENDER_TEXTURE, 1.f, 0, v525, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_soulfighterclawA_render);
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		BodyLight[0] = 1.f;
		BodyLight[1] = 1.f;
		BodyLight[2] = 1.f;
		int v655 = (int)timeGetTime() % 600 / 40;
		float v657 = (float)(v655 % 4) * 0.25f;
		float v659 = (float)(v655 / 4) * 0.25f;
		RenderMesh(2, RENDER_TEXTURE, pObj->Alpha, 2, 1.f, v657, v659, -1);
	}
	break;
	case Blue_Eye_Sword:
	{
		glColor3fv(BodyLight);
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		BodyLight[0] = 0.55f;
		BodyLight[1] = 0.7f;
		BodyLight[2] = 1.f;
		int Cal = (int)timeGetTime() % 1125 / 75;
		blend_mesh_tex_coord_u = (float)(Cal % 4) * 0.25f;
		blend_mesh_tex_coord_v = (float)(Cal / 4) * 0.25f;
		RenderMesh(3, RENDER_TEXTURE | RENDER_BRIGHT, 1.f, 3, pObj->BlendMeshLight, blend_mesh_tex_coord_u, blend_mesh_tex_coord_v, -1);
		fLuminosity = Render22(-6.f, 0.f) * 0.8f + 0.2f;

		if (fLuminosity > 0.8f)
			fLuminosity = 0.8f;
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, 1, fLuminosity, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_hs_blueeyeknightA02_render);
		float v639 = ((int)timeGetTime() % 2000) * 0.0005f;
		float v640 = ((int)timeGetTime() % 2000) * 0.0005f;
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, 1, pObj->BlendMeshLight, v640, v639, BITMAP_black_gold_crom04_mono);
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, fLuminosity, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_hs_blueeyeknightA03r);
	}
	break;
	case Blue_Eye_MagicSword:
	{
		glColor3fv(BodyLight);
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		fLuminosity = Render22(-6.f, 0.f) * 0.8f + 0.2f;
		if (fLuminosity > 1.f)
			fLuminosity = 1.f;
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, 1, fLuminosity, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_blueeyemagicsword03r);
		fLuminosity = Render22(-6.f, 0.f) * 0.9f + 0.2f;
		if (fLuminosity > 0.8f)
			fLuminosity = 0.8f;
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, fLuminosity, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_blueeyemagicsword02r);
		float v670 = pObj->BlendMeshTexCoordV;
		float v671 = ((int)timeGetTime() % 10000) * 0.0003f;
		RenderMesh(2, RENDER_TEXTURE, pObj->Alpha, 2, 0.7f, v671, v670, BITMAP_black_gold_crom04_mono);
		int v834 = 1200;
		float v833 = (((int)((v834 - timeGetTime())) % 1201)) / (float)v834;
		RenderMesh(3, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 3, 0.3f, v833, pObj->BlendMeshTexCoordV, BITMAP_alpha_line2);
	}
	break;
	case Blue_Eye_Scepter:
	{
		glColor3fv(BodyLight);
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float v631 = pObj->BlendMeshTexCoordV;
		float v632 = pObj->BlendMeshTexCoordU;
		float v633 = Render22(-4.f, 0.f);
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, v633, v632, v631, BITMAP_blueeyesceptre_render);
		float v634 = ((int)timeGetTime() % 2000)
			* 0.0005f;
		float v635 = pObj->BlendMeshTexCoordU;
		float v636 = Render22(-4.f, 0.f) * 0.7f
			+ 0.15f;
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, 1, v636, v635, v634, BITMAP_black_gold_crom04);
	}
	break;
	case Blue_Eye_Bow:
	{
		glColor3fv(BodyLight);
		int v854 = (int)timeGetTime() % 900 / 60;
		float v855 = (float)(v854 % 4) * 0.25f;
		float v857 = (float)(v854 / 4) * 0.25f;
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float v643 = pObj->BlendMeshTexCoordV;
		float v644 = pObj->BlendMeshTexCoordU;
		float v645 = Render22(-4.f, 0.f);
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, v645, v644, v643, BITMAP_Blue_eyebow02r);
		float v646 = ((int)timeGetTime() % 2000)
			* 0.0005f;
		float v647 = pObj->BlendMeshTexCoordU;
		float v648 = Render22(-4.f, 0.f) * 0.8500000238418579f + 0.15f;
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, 1, v648, v647, v646, BITMAP_black_gold_crom04);
		BodyLight[0] = 0.1f;
		BodyLight[1] = 0.65f;
		BodyLight[2] = 0.5f;
		float v650 = v857;
		float v651 = v855;
		float v652 = Render22(-4.f, 0.f) * 0.7f
			+ 0.3f;
		RenderMesh(2, RENDER_TEXTURE, pObj->Alpha, 2, v652, v651, v650, -1);
		RenderMesh(3, RENDER_TEXTURE, pObj->Alpha, 3, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(4, RENDER_TEXTURE, pObj->Alpha, 4, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(5, RENDER_TEXTURE, pObj->Alpha, 5, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
	}
	break;
	case Blue_Eye_Staff:
	{
		glColor3fv(BodyLight);
		int v849 = (int)timeGetTime() % 750 / 50;
		float v850 = (float)(v849 % 4) * 0.25f;
		float v852 = (float)(v849 / 4) * 0.25f;
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float v653 = pObj->BlendMeshTexCoordV;
		float v654 = pObj->BlendMeshTexCoordU;
		float v655 = Render22(-4.f, 0.f);
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, v655, v654, v653, BITMAP_blue_eye_ws02_render);
		float v656 = ((int)timeGetTime() % 2000)
			* 0.0005f;
		float v657 = pObj->BlendMeshTexCoordU;
		float v658 = Render22(-4.f, 0.f) * 0.85f + 0.15f;
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, 1, v658, v657, v656, BITMAP_black_gold_crom04);
		BodyLight[0] = 0.1f;
		BodyLight[1] = 0.3f;
		BodyLight[2] = 1.f;
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, pObj->BlendMeshLight, v850, v852, -1);
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, pObj->BlendMeshLight, v850, v852, -1);
		int v851 = 15;
		signed int v659 = timeGetTime();
		v849 = v659 % (36 * v851) / v851;
		float v660 = (float)v849 * 0.03f;
		v852 = minf(1.0, v660);
		BodyLight[0] = 1.f;
		BodyLight[1] = 0.6f;
		BodyLight[2] = 0.2f;
		float v661 = 1.f - v852;
		RenderMesh(3, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 3, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, v661, -1);
	}
	break;
	case Blue_Eye_Stick:
	{
		glColor3fv(BodyLight);
		int v859 = (int)timeGetTime() % 900 / 60;
		float v860 = (float)(v859 % 4) * 0.25f;
		float v862 = (float)(v859 / 4) * 0.25f;
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float v637 = pObj->BlendMeshTexCoordV;
		float v638 = pObj->BlendMeshTexCoordU;
		float v639 = Render22(-4.f, 0.f);
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, v639, v638, v637, BITMAP_bsummoners02_render);
		float v640 = ((int)timeGetTime() % 2000) * 0.0005f;
		float v641 = pObj->BlendMeshTexCoordU;
		float v642 = Render22(-4.f, 0.f) * 0.85f + 0.15f;
		RenderMesh(2, RENDER_TEXTURE, pObj->Alpha, 2, v642, v641, v640, BITMAP_black_gold_crom04);
		BodyLight[0] = 0.3f;
		BodyLight[1] = 1.f;
		BodyLight[2] = 0.6f;
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, pObj->BlendMeshLight, v860, v862, -1);
	}
	break;
	case Blue_Eye_Claw:
	{

		glColor3fv(BodyLight);

		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float v827 = Render22(-8.f, 0.f);
		if (v827 > 1.f)
			v827 = 1.f;
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, 0, v827, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_Blueeyeclaw_render);
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, 1, v827, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_Blueeyeclaw_render);
		v827 = Render22(-4.f, 0.f) * 0.7f + 0.3f;
		if (v827 > 1.f)
			v827 = 1.f;
		int v826 = (int)timeGetTime() % 750 / 50;
		float v829 = (float)(v826 % 4) * 0.25f;
		float v831 = (float)(v826 / 4) * 0.25f;
		BodyLight[0] = 0.15f;
		BodyLight[1] = 0.3f;
		BodyLight[2] = 1.f;
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, v827, v829, v831, BITMAP_fire_15frame_mono);
	}
	break;
	case Darkangel_Bow:
	{
		glColor3fv(BodyLight);
		float DarkAngelBow = ((int)timeGetTime() % 2000) * 0.0005f;
		RenderMesh(0, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 0, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, DarkAngelBow, BITMAP_black_gold_crom05);
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
	}
	break;
	case Darkangel_Lance:
	{
		glColor3fv(BodyLight);
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(3, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(3, 576, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_black_gold_crom05);
		float v803 = ((int)(timeGetTime() * 0.1f) % 200) * 0.01f - 1.f;
		RenderMesh(4, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 4, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, v803, -1);
	}
	break;
	case Darkangel_Mace:
	{
		glColor3fv(BodyLight);
		float v808 = Render22(-4.f, 0.f);
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(0, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 0, v808, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_mace_mastr02s_render);
		float v398 = ((int)timeGetTime() % 2000) * 0.0005f;
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, v398, BITMAP_black_gold_crom05);
		int v807 = (int)timeGetTime() % 1200 / 80;
		float v809 = (float)(v807 % 4) * 0.25f;
		BodyLight[0] = 1.f;
		BodyLight[1] = 0.6f;
		BodyLight[2] = 0.34f;
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, 1, pObj->BlendMeshLight, v809, pObj->BlendMeshTexCoordV, -1);
	}
	break;
	case Darkangel_Staff:
	{
		glColor3fv(BodyLight);
		RenderMesh(0, RENDER_TEXTURE, 1.f, -1, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float v525 = Render22(-4.f, 0.f);
		RenderMesh(0, RENDER_TEXTURE, 1.0, 0, v525, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_Darkangel_StaffA_render);
		float v486 = BodyLight[0] * 0.3f;
		BodyLight[0] = v486;
		float v487 = BodyLight[1] * 0.3f;
		BodyLight[1] = v487;
		float v488 = BodyLight[2] * 0.3f;
		BodyLight[2] = v488;
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, 1.f, 1, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, 576, 1.f, 1, 1.f, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_black_gold_crom05);
	}
	break;
	case Darkangel_MagicSword:
	{
		glColor3fv(BodyLight);
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		BodyLight[0] = 0.5f;
		BodyLight[1] = 0.5f;
		BodyLight[2] = 0.5f;
		float v401 = ((int)timeGetTime() % 2000) * 0.0005f;
		RenderMesh(0, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 0, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, v401, BITMAP_black_gold_crom05);
		BodyLight[0] = 1.f;
		BodyLight[1] = 0.7f;
		BodyLight[2] = 1.f;
		float v551 = (float)(1000 - (int)timeGetTime() % 1001) / (float)1000;
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, pObj->BlendMeshLight, v551, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, pObj->BlendMeshLight, v551, pObj->BlendMeshTexCoordV, -1);
	}
	break;
	case Darkangel_ShortSword:
	{
		glColor3fv(BodyLight);
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float v1011 = ((int)timeGetTime() % 2000) * 0.0005f;
		BodyLight[0] = 1.f;
		BodyLight[1] = 0.3f;
		BodyLight[2] = 1.f;
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, v1011, -1);
	}
	break;
	case Darkangel_Sword:
	{
		glColor3fv(BodyLight);
		RenderMesh(0, RENDER_TEXTURE, 1.f, -1, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, 1.f, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, 576, 1.f, 1, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_black_gold_crom05);
		BodyLight[0] = 1.0f;
		BodyLight[1] = 0.3f;
		BodyLight[2] = 1.0f;
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, 1.0, 2, 1.0, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_gra2);
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, 1.0, 2, 1.0, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_gra2);
	}
	break;
	case Holyangel_Mace:
	{
		glColor3fv(BodyLight);
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		BodyLight[0] = 0.5;
		BodyLight[1] = 0.5;
		BodyLight[2] = 0.5;
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, 68, pObj->Alpha, 2, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float v507 = ((int)timeGetTime() % 2000) * 0.0005f;
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, v507, -1);
		BodyLight[0] = 0.0;
		BodyLight[1] = 0.5;
		BodyLight[2] = 1.0;
		RenderMesh(3, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 3, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float v598 = Render22(-4.f, 0.f) * 0.7f + 0.3f;
		RenderMesh(4, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 4, v598, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
	}
	break;
	case Holyangel_Lance:
	{
		glColor3fv(BodyLight);
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		BodyLight[0] = 0.5f;
		BodyLight[1] = 0.8f;
		BodyLight[2] = 1.f;
		float v777 = Render22(-4.f, 0.f) + 0.3f;
		RenderMesh(3, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 3, v777, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
	}
	break;
	case Holyangel_ShortSword:
	{
		glColor3fv(BodyLight);
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float v401 = ((int)timeGetTime() % 2000) * 0.0005f;
		float v403 = -v401;
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, v403, BITMAP_line_of_big_R);
		BodyLight[0] = 0.2f;
		BodyLight[1] = 0.6f;
		BodyLight[2] = 1.f;
		RenderMesh(3, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 3, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
	}
	break;
	case Awakening_Lance:
	{
		glColor3fv(BodyLight);
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float v503 = (float)(rand() % 20) / 52.5f;
		float v601 = v503 + 0.4f;
		int v1148 = 47;
		float v44 = v601 * 0.4f;
		float v45 = v601 * 0.6f;
		float v46 = v601 * 0.8f;
		BodyLight[0] = v46;
		BodyLight[1] = v45;
		BodyLight[2] = v44;
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		BodyLight[0] = 1.f;
		BodyLight[1] = 0.8f;
		BodyLight[2] = 0.8f;
		BodyLight[0] = v46;
		BodyLight[1] = v45;
		BodyLight[2] = v44;
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float v600 = Render22(-5.f, 0.f) * 0.8f + 0.2f;
		if (v600 > 1.f)
			float v600 = 1.f;
		BodyLight[0] = 1.f;
		BodyLight[1] = 1.f;
		BodyLight[2] = 1.f;
		RenderMesh(0, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 0, v600, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_sd_soullance01_render);
		RenderMesh(0, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 0, v600, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_sd_soullance01_render);
	}
	break;
	case Awakening_Mace:
	{
		glColor3fv(BodyLight);
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float v595 = Render22(-4.0, 0.0);
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, v595, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float v508 = (float)(rand() % 20) / 52.5f;
		float v597 = v508 + 0.4f;
		float v596 = ((int)timeGetTime() % 4000) * 0.00025f;
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, v597, v596, pObj->BlendMeshTexCoordV, -1);
	}
	break;
	case Awakening_ShortSword:
	{
		glColor3fv(BodyLight);
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		int v655 = (int)timeGetTime() % 600 / 40;
		float v657 = (float)(v655 % 4) * 0.25f;
		float v659 = (float)(v655 / 4) * 0.25f;
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, pObj->BlendMeshLight, v657, v659, -1);
		BodyLight[0] = 0.3f;
		BodyLight[1] = 0.3f;
		BodyLight[2] = 0.3f;
		float v1010 = ((int)timeGetTime() % 2000) * 0.0005f;
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, v1010, BITMAP_flame_chrome);
	}
	break;
	case Blue_Eye_ShortSword:
	{

		glColor3fv(BodyLight);

		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float v1008 = (Render22(-4.f, 0.f) * 0.7f
			+ 0.3f)
			* pObj->Alpha;
		RenderMesh(3, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 3, v1008, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		BodyLight[0] = 0.3f;
		BodyLight[1] = 0.3f;
		BodyLight[2] = 0.3f;
		RenderMesh(4, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 4, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float v1009 = ((int)timeGetTime() % 2000)
			* 0.0005f;
		float v728 = -v1009;
		RenderMesh(4, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 4, pObj->BlendMeshLight, v728, pObj->BlendMeshTexCoordV, BITMAP_black_gold_crom05);
		BodyLight[0] = 0.55f;
		BodyLight[1] = 0.7f;
		BodyLight[2] = 1.f;
		int v655 = (int)timeGetTime() % 600 / 40;
		float v657 = (float)(v655 % 4) * 0.25f;
		float v659 = (float)(v655 / 4) * 0.25f;
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, pObj->BlendMeshLight, v657, v659, -1);
	}
	break;
	case Blue_Eye_Lance:
	{

		glColor3fv(BodyLight);

		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float v976 = Render22(-4.f, 0.f) * 0.7f + 0.3f;
		if (v976 > 1.f)
			v976 = 1.f;
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, 1, v976, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_hs_blueeyelanceA02r);
		float v770 = pObj->BlendMeshTexCoordV,
			v771 = ((int)timeGetTime() % 2000)
			* 0.0005f;
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, 1, v976, v771, v770, BITMAP_hs_blueeyelanceA02r);
		int v979 = 50;
		int v772 = (int)timeGetTime();
		int v975 = v772 % (15 * v979) / v979;
		float v978 = (float)(v975 % 4) * 0.25f;
		float v980 = (float)(v975 / 4) * 0.25f;
		BodyLight[0] = 0.15f;
		BodyLight[1] = 0.4f;
		BodyLight[2] = 1.f;
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, v976, v978, v980, -1);
		v976 = Render22(-4.0, 0.0);
		if (v976 > 1.0)
			v976 = 1.0;
		RenderMesh(3, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 3, v976, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
	}
	break;
	case Blue_Eye_Mace:
	{

		glColor3fv(BodyLight);

		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float v974 = Render22(-5.f, 0.f);
		if (v974 > 1.f)
			v974 = 1.f;
		float v775 = pObj->BlendMeshTexCoordV,
			v776 = ((int)timeGetTime() % 2000)
			* 0.0005f;
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, v974, v776, v775, BITMAP_MMsSs2_r);
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, 1, v974, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_mace5master02_render);
	}
	break;
	case Wings_of_Disillusionment:
	{

		glColor3fv(BodyLight);

		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float v1137 = Render22(-2.f, 0.f) * 0.4f + 0.6f;
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, v1137, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		v1137 = Render22(-4.0, 0.0);
		RenderMesh(2, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, v1137, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_runewing3db_render);
		RenderMesh(3, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(3, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 3, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
	}
	break;
	case Wings_of_Fate:
	{

		glColor3fv(BodyLight);

		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float v1121 = Render22(-4.f, 0.f) * 0.5f + 0.5f;
		float v1122 = Render22(-4.f, 120.f);
		float v1119 = Render22(-4.f, 240.f);
		RenderMesh(0, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 0, v1121, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(0, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 0, v1122, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_runewing4dc_star01);
		RenderMesh(0, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 0, v1119, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_runewing4dc_star02);
		float v1120 = Render22(-4.f, 0.f) * 0.6f;
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, v1120, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_runewing4db_render);
		runtime_render_level(pObj, 1, 188, 79, 88);
	}
	break;
	case Wings_of_Silence:
	{

		glColor3fv(BodyLight);

		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(4, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 4, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		BodyLight[0] = 0.3f;
		BodyLight[1] = 0.3f;
		BodyLight[2] = 0.3f;
		float v731 = ((int)timeGetTime() % 2000) * 0.0005f;
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, v731, BITMAP_black_gold_crom05);
		BodyLight[0] = 1.f;
		BodyLight[1] = 1.f;
		BodyLight[2] = 1.f;
		float v1002 = (Render22(-4.f, 0.f) * 0.8f + 0.2f) * pObj->Alpha;
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, v1002, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_wings_van_render);
		v1002 = (Render22(-4.f, 0.f) * 0.5f + 0.5f) * pObj->Alpha;
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, v1002, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(3, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 3, v1002, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		int v1004 = 60;
		int v732 = (int)timeGetTime();
		int v1001 = v732 % (13 * v1004) / v1004;
		float v1003 = (float)(v1001 % 4) * 0.25f;
		float v1005 = (float)(v1001 / 4) * 0.25f;
		BodyLight[0] = 0.25f;
		BodyLight[1] = 0.15f;
		BodyLight[2] = 0.23f;
		RenderMesh(5, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 5, pObj->BlendMeshLight, v1003, v1005, -1);
	}
	break;
	case Wings_of_Condemnation:
	{
		glColor3fv(BodyLight);
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(3, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float v997 = Render22(-4.f, 0.f) * pObj->Alpha;
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, v997, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_benzia_wing4_render);
		v997 = (Render22(-2.f, 0.f) * 0.7f + 0.3f) * pObj->Alpha;
		RenderMesh(0, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 0, v997, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_benzia_wing4a_render);
		v997 = Render22(-2.f, 180.f) * pObj->Alpha;
		BodyLight[0] = 0.8f;
		BodyLight[1] = 0.8f;
		BodyLight[2] = 0.8f;
		RenderMesh(0, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 0, v997, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_benzia_wing4b_render);
		RenderMesh(0, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 0, v997, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_benzia_wing4b_render);
		int v999 = 80;
		int v735 = (int)timeGetTime();
		int v996 = v735 % (15 * v999) / v999;
		float v998 = (float)(v996 % 4) * 0.25f;
		float v1000 = (float)(v996 / 4) * 0.25f;
		BodyLight[0] = 0.4f;
		BodyLight[1] = 0.4f;
		BodyLight[2] = 0.4f;
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, pObj->BlendMeshLight, v998, v1000, -1);
		runtime_render_level(pObj, 1, 135, 147, 144);
	}
	break;
	case Wings_of_Power_old:
	{
		glColor3fv(BodyLight);
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(5, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float v1503 = Render22(-6.f, 0.f) + 0.3f;
		if (v1503 > 1.f)
			v1503 = 1.f;
		RenderMesh(0, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 66, v1503, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_season_wing_period_render);
		BodyLight[0] = 1.f;
		BodyLight[1] = 0.8f;
		BodyLight[2] = 0.65f;
		RenderMesh(0, 68, pObj->Alpha, 0, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_CHROME);
		BodyLight[0] = 0.4f;
		BodyLight[1] = 0.45f;
		BodyLight[2] = 1.f;
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		BodyLight[0] = 0.4f;
		BodyLight[1] = 0.76f;
		BodyLight[2] = 0.95f;
		int v1505 = 40;
		signed int v49 = timeGetTime();
		int v1502 = v49 % (15 * v1505) / v1505;
		float v1504 = (float)(v1502 % 4) * 0.25f;
		float v1506 = (float)(v1502 / 4) * 0.25f;
		RenderMesh(3, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 3, pObj->BlendMeshLight, v1504, v1506, -1);
		BodyLight[0] = 0.25f;
		BodyLight[1] = 0.35f;
		BodyLight[2] = 1.f;
		int wofpeffect1 = (int)(timeGetTime() * 1.25f) % 600 / 40;
		float wofpeffect2 = (float)(wofpeffect1 % 4) * 0.25f;
		float wofpeffect3 = (float)(wofpeffect1 / 4) * 0.25f;
		RenderMesh(4, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 4, pObj->BlendMeshLight, wofpeffect2, wofpeffect3, BITMAP_firek_sq_mono);
	}
	break;
	case Control_Cloak:
	{
		if (BodyLight[0] != 1.f || BodyLight[1] != 1.f || BodyLight[2] != 1.f)
		{
			BodyLight[0] = 1.f;
			BodyLight[1] = 1.f;
			BodyLight[2] = 1.f;

			glColor3fv(BodyLight);

			RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
			RenderMesh(3, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
			RenderMesh(4, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
			RenderMesh(5, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 5, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
			BodyLight[0] = 1.f;
			BodyLight[1] = 1.f;
			BodyLight[2] = 1.f;
			RenderMesh(3, RENDER_TEXTURE | RENDER_CHROME | RENDER_BRIGHT, pObj->Alpha, 3, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
			runtime_render_level(pObj, 4, 61, 112, 255);
		}
		else
		{
			BodyLight[0] = 1.f;
			BodyLight[1] = 1.f;
			BodyLight[2] = 1.f;
			glColor3fv(BodyLight);
			RenderBody(2, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1, -1);
		}

		glColor3fv(BodyLight);

		BodyLight[0] = 1.f;
		BodyLight[1] = 1.f;
		BodyLight[2] = 1.f;
		RenderMesh(3, RENDER_TEXTURE | RENDER_CHROME | RENDER_BRIGHT, pObj->Alpha, 3, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		runtime_render_level(pObj, 4, 61, 112, 255);
	}
	break;
	case Referre_Cloak:
	{
		if (BodyLight[0] != 1.f || BodyLight[1] != 1.f || BodyLight[2] != 1.f)
		{
			BodyLight[0] = 1.f;
			BodyLight[1] = 1.f;
			BodyLight[2] = 1.f;
			glColor3fv(BodyLight);
			RenderMesh(6, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		}
		else
		{
			BodyLight[0] = 1.f;
			BodyLight[1] = 1.f;
			BodyLight[2] = 1.f;
			glColor3fv(BodyLight);
			RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		}

		glColor3fv(BodyLight);
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(3, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 3, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		runtime_render_level(pObj, 1, 181, 137, 139);
	}
	break;
	case Wings_of_Power:
	{
		if (BodyLight[0] != 1.f || BodyLight[1] != 1.f || BodyLight[2] != 1.f)
		{
			BodyLight[0] = 1.f;
			BodyLight[1] = 1.f;
			BodyLight[2] = 1.f;
			glColor3fv(BodyLight);
			RenderMesh(6, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		}
		else
		{
			BodyLight[0] = 1.f;
			BodyLight[1] = 1.f;
			BodyLight[2] = 1.f;
			glColor3fv(BodyLight);
			RenderMesh(5, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		}

		glColor3fv(BodyLight);
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float v1503 = Render22(-6.f, 0.f) + 0.3f;
		if (v1503 > 1.0)
			v1503 = 1.0;
		RenderMesh(0, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 66, v1503, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_season_wing_period_render);
		BodyLight[0] = 1.f;
		BodyLight[1] = 0.8f;
		BodyLight[2] = 0.65f;
		RenderMesh(0, 68, pObj->Alpha, 0, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_CHROME);
		BodyLight[0] = 0.4f;
		BodyLight[1] = 0.45f;
		BodyLight[2] = 1.f;
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		BodyLight[0] = 0.4f;
		BodyLight[1] = 0.76f;
		BodyLight[2] = 0.95f;
		int v1505 = 40;
		signed int v49 = timeGetTime();
		int v1502 = v49 % (15 * v1505) / v1505;
		float v1504 = (float)(v1502 % 4) * 0.25f;
		float v1506 = (float)(v1502 / 4) * 0.25f;
		RenderMesh(3, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 3, pObj->BlendMeshLight, v1504, v1506, -1);
		BodyLight[0] = 0.25f;
		BodyLight[1] = 0.35f;
		BodyLight[2] = 1.f;
		int wofpeffect1 = (int)(timeGetTime() * 1.25f) % 600 / 40;
		float wofpeffect2 = (float)(wofpeffect1 % 4) * 0.25f;
		float wofpeffect3 = (float)(wofpeffect1 / 4) * 0.25f;
		RenderMesh(4, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 4, pObj->BlendMeshLight, wofpeffect2, wofpeffect3, -1);
	}
	break;
	case Bloodangel_Shield:
	{
		glColor3fv(BodyLight);
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float Parpadeo1 = (float)(rand() % 20) / 32.5f;
		float Parpadeo2 = Parpadeo1 + 0.4f;
		BodyLight[0] = 1.f;
		BodyLight[1] = 0.75f;
		BodyLight[2] = 0.4f;
		RenderMesh(3, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 3, Parpadeo2, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_gra3);
		BodyLight[0] = 1.f;
		BodyLight[1] = 0.15f;
		BodyLight[2] = 0.1f;
		RenderMesh(4, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 4, Parpadeo2, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_gra2);
	}
	break;
	case Darkangel_Shield:
	{
		glColor3fv(BodyLight);
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, 576, 1.0, 2, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_black_gold_crom05);
		float Parpadeo1 = (float)(rand() % 20) / 32.5f;
		float Parpadeo2 = Parpadeo1 + 0.4f;
		BodyLight[0] = 0.3f;
		BodyLight[1] = 0.3f;
		BodyLight[2] = 1.f;
		RenderMesh(3, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 3, Parpadeo2, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_gra3);
		RenderMesh(3, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 3, Parpadeo2, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_gra3);
		BodyLight[0] = 0.2f;
		BodyLight[1] = 0.75f;
		BodyLight[2] = 1.f;
		RenderMesh(4, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 4, Parpadeo2, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_gra2);
	}
	break;
	case Holyangel_Shield:
	{
		glColor3fv(BodyLight);
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(3, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 3, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		BodyLight[0] = 1.f;
		BodyLight[1] = 0.85f;
		BodyLight[2] = 0.45f;
		float v401 = ((int)timeGetTime() % 2000) * 0.0005f;
		float v776 = -v401;
		RenderMesh(5, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 5, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, v776, BITMAP_line_of_big_R);
		float HolyangelSc01 = (float)(rand() % 15) / 37.5f;
		float HolyangelSc02 = HolyangelSc01 + 0.5f;
		BodyLight[0] = 0.25f;
		BodyLight[1] = 0.55f;
		BodyLight[2] = 1.5f;
		RenderMesh(4, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 4, HolyangelSc02, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_gra2);
	}
	break;
	case Awakening_Shield:
	{
		glColor3fv(BodyLight);
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(3, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float Parpadeo1 = (float)(rand() % 20) / 32.5f;
		float Parpadeo2 = Parpadeo1 + 0.4f;
		RenderMesh(4, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 4, Parpadeo2, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		BodyLight[0] = 1.f;
		BodyLight[1] = 0.25f;
		BodyLight[2] = 0.1f;
		RenderMesh(3, 6, pObj->Alpha, 0, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_black_gold_crom04_mono);
		BodyLight[0] = 1.f;
		BodyLight[1] = 1.f;
		BodyLight[2] = 1.f;
		float v1010 = ((int)timeGetTime() % 2000) * 0.0005f;
		RenderMesh(3, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 3, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, v1010, BITMAP_flame_chrome);
		float v777 = Render22(-4.f, 0.f) * 0.8f + 0.2f;
		BodyLight[0] = 1.f;
		BodyLight[1] = 0.08f;
		BodyLight[2] = 0.05f;
		RenderMesh(5, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 5, v777, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_gra2);
	}
	break;
	case Blueeye_Shield:
	{
		glColor3fv(BodyLight);
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(3, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		BodyLight[0] = 0.6f;
		BodyLight[1] = 0.85f;
		BodyLight[2] = 1.f;
		float v778 = (float)(timeGetTime() % 2000) * 0.00050f;
		float v779 = (float)(timeGetTime() % 2000) * 0.0005f;
		RenderMesh(3, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 3, pObj->BlendMeshLight, v779, v778, BITMAP_CHROME);//Chrome01.jpg
		BodyLight[0] = 0.3f;
		BodyLight[1] = 0.75f;
		BodyLight[2] = 1.7f;
		float Parpadeo1 = (float)(rand() % 20) / 32.5f;
		float Parpadeo2 = Parpadeo1 + 0.4f;
		RenderMesh(4, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 4, Parpadeo2, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_gra2);
	}
	break;
	//Correccion 19/10/2020
	case Silverheart_Shield:
	{
		glColor3fv(BodyLight);
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(3, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(4, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		BodyLight[0] = 0.55f / 2;
		BodyLight[1] = 0.25f / 2;
		BodyLight[2] = 1.f / 2;
		float RenderWizardBE = Render22(-4.f, 0.f) * 0.7f + 0.3f;
		if (RenderWizardBE > 1.f)
			RenderWizardBE = 1.f;
		RenderMesh(5, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 5, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_gra2);
		RenderMesh(5, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 5, RenderWizardBE, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_gra2);
	}
	break;
	case Mayas_Wings_MuOrigin:
	{
		glColor3fv(BodyLight);
		RenderMesh(0, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		int v658 = 40;
		int v655 = (int)timeGetTime() % 600 / 40;
		float v657 = (float)(v655 % 4) * 0.25f;
		float v659 = (float)(v655 / 4) * 0.25f;
		BodyLight[0] = 0.75f;
		BodyLight[1] = 0.75f;
		BodyLight[2] = 0.75f;
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, pObj->BlendMeshLight, v657, v659, -1);
		BodyLight[0] = 0.35f;
		BodyLight[1] = 0.35f;
		BodyLight[2] = 1.00f;
		RenderMesh(3, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 3, pObj->BlendMeshLight, v657, v659, -1);
		BodyLight[0] = 0.35f;
		BodyLight[1] = 0.35f;
		BodyLight[2] = 1.00f;
		RenderMesh(3, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 3, pObj->BlendMeshLight, v657, v659, -1);
		float v551 = (1000 - timeGetTime() % 1001) / 1000.0f;
		BodyLight[0] = 0.5f;
		BodyLight[1] = 0.5f;
		BodyLight[2] = 0.5f;
		RenderMesh(4, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 4, pObj->BlendMeshLight, v551, pObj->BlendMeshTexCoordV, BITMAP_0120);
		BodyLight[0] = 0.5f;
		BodyLight[1] = 0.5f;
		BodyLight[2] = 0.5f;
		RenderMesh(5, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 5, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		BodyLight[0] = 0.15f;
		BodyLight[1] = 0.15f;
		BodyLight[2] = 0.25f;
		RenderMesh(5, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 5, pObj->BlendMeshLight, v551, pObj->BlendMeshTexCoordV, BITMAP_0143);
		BodyLight[0] = 0.87f;
		BodyLight[1] = 0.41f;
		BodyLight[2] = 1.00f;
		RenderMesh(4, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 4, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		BodyLight[0] = 1.f;
		BodyLight[1] = 1.f;
		BodyLight[2] = 1.f;
		RenderMesh(1, RENDER_TEXTURE | RENDER_CHROME | RENDER_BRIGHT, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
	}
	break;
	case Trigis_Claw:
	{
		glColor3fv(BodyLight);
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float tigris = ((int)(timeGetTime() * 0.05f) % 8 / 2) * -0.25f;
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, 1.f, tigris, pObj->BlendMeshTexCoordV, -1);
	}
	break;
	case Wing_of_Conqueror:
	{
		glColor3fv(BodyLight);
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float v1209 = Render22(-2.f, 0.3f);
		if (v1209 < 0.4f)
			v1209 = 0.4f;
		RenderMesh(0, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 0, v1209, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_ChaWing_renderbright);
	}
	break;
	case Absolute_Claw:
	{
		glColor3fv(BodyLight);
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, -1, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, -1, 1.f, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, 1.f, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, RENDER_TEXTURE, pObj->Alpha, 2, 1.f, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float v51 = timeGetTime() * 0.0005f;
		BodyLight[0] = 0.5f;
		BodyLight[1] = 0.5f;
		BodyLight[2] = 0.5f;
		RenderMesh(1, 18, pObj->Alpha, 1, 1.0, pObj->BlendMeshTexCoordU, v51, BITMAP_CHROME);//Chrome01.jpg
		float v52 = (float)(rand() % 15) / 21.5f;
		float v1179 = v52 + 0.4f;
		RenderMesh(3, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 3, v1179, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
	}
	break;
	case Absolute_Mace:
	{
		glColor3fv(BodyLight);
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		BodyLight[0] = 1.0f;
		BodyLight[1] = 0.5f;
		BodyLight[2] = 0.15f;
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		//30/05/2020
	}
	break;
	case CollectorWings_Purple_MU2:
	{
		glColor3fv(BodyLight);
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		int fall1 = (int)timeGetTime() % 600 / 40;
		float fall2 = (float)(fall1 % 4) * 0.25f;
		float fall3 = (float)(fall1 / 4) * 0.25f;
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, 1.f, 1, pObj->BlendMeshLight, fall2, fall3, -1);
		BodyLight[0] = 0.20f;
		BodyLight[1] = 0.15f;
		BodyLight[2] = 0.25f;
		RenderMesh(0, RENDER_TEXTURE | RENDER_CHROME | RENDER_BRIGHT, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float Parpadeo1 = (float)(rand() % 125) / 32.5f;
		float Parpadeo2 = Parpadeo1 + 0.4f;
		RenderMesh(0, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 0, Parpadeo2, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_KCWWingP_Render);
	}
	break;
	case CollectorWings_Red_MU2:
	{

		glColor3fv(BodyLight);

		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		int fall1 = (int)timeGetTime() % 600 / 40;
		float fall2 = (float)(fall1 % 4) * 0.25f;
		float fall3 = (float)(fall1 / 4) * 0.25f;
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, 1.f, 1, pObj->BlendMeshLight, fall2, fall3, -1);
		BodyLight[0] = 0.20f;
		BodyLight[1] = 0.15f;
		BodyLight[2] = 0.25f;
		RenderMesh(0, RENDER_TEXTURE | RENDER_CHROME | RENDER_BRIGHT, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float Parpadeo1 = (float)(rand() % 125) / 32.5f;
		float Parpadeo2 = Parpadeo1 + 0.4f;
		RenderMesh(0, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 0, Parpadeo2, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_KCWWingR_Render);
	}
	break;
	case Kit_Of_War_2_0_MuAway:
	{

		glColor3fv(BodyLight);

		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float v551 = ((int)(4000 - timeGetTime() * 0.75f) % 4001) * 0.0002500000118743628f;
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, 1, pObj->BlendMeshLight, -v551, v551, -1);
	}
	case Russell_Light_Lance:
	{

		glColor3fv(BodyLight);

		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float v698 = ((int)timeGetTime() % 15) / 37.5f;
		float v800 = v698 + 0.5f;
		BodyLight[0] = v800 * 1.f;
		BodyLight[1] = v800 * 0.2f;
		BodyLight[2] = v800 * 0.6f;
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
	}
	break;
	case Constant_Shield:
	{

		glColor3fv(BodyLight);

		float v796 = Render22(-4.f, 0.f) * 0.7f + 0.3f;
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, v796, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		BodyLight[0] = 1.0f;
		BodyLight[1] = 0.0f;
		BodyLight[2] = 0.2f;
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, v796, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_gra);
	}
	break;
	case Bloodangel_Sword:
	{

		glColor3fv(BodyLight);

		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float v810 = Render22(-4.f, 0.f);
		RenderMesh(0, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 0, v810, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_BoodAnelsword_render);
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, v810, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		BodyLight[0] = 0.2f;
		BodyLight[1] = 0.2f;
		BodyLight[2] = 1.0f;
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		BodyLight[0] = 1.0f;
		BodyLight[1] = 0.8f;
		BodyLight[2] = 0.2f;
		RenderMesh(1, 68, pObj->Alpha, 1, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_CHROME);//Chrome01.jpg
	}
	break;
	case Bloodangel_MagicSword:
	{

		glColor3fv(BodyLight);

		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float v810 = Render22(-4.f, 0.f);
		RenderMesh(0, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 0, v810, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_BAGsword_render);
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, v810, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		BodyLight[0] = 0.2f;
		BodyLight[1] = 0.2f;
		BodyLight[2] = 1.0f;
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		BodyLight[0] = 1.0f;
		BodyLight[1] = 0.8f;
		BodyLight[2] = 0.2f;
		RenderMesh(1, 68, pObj->Alpha, 1, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_CHROME);//Chrome01.jpg
	}
	break;
	case Bloodangel_Claw:
	{

		glColor3fv(BodyLight);

		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float v810 = Render22(-4.f, 0.f);
		RenderMesh(0, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 0, v810, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_bloodclaw2_render);
	}
	break;
	case Bloodangel_Scepter:
	{

		glColor3fv(BodyLight);

		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float v810 = Render22(-4.f, 0.f);
		RenderMesh(0, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 0, v810, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_booldangel_sapterr);
	}
	break;
	case Bloodangel_Bow:
	{

		glColor3fv(BodyLight);

		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float v810 = Render22(-4.f, 0.f);
		RenderMesh(0, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 0, v810, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_blood_bow_render);
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float v407 = ((int)(timeGetTime() * 0.05f) % 16 / 4) * 0.25f;
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, v407, -1);
	}
	break;
	case Bloodangel_Staff:
	{

		glColor3fv(BodyLight);

		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float v810 = Render22(-4.f, 0.f);
		RenderMesh(0, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 0, v810, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_bloodangelstaff01_render);
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, v810, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_bloodangelstaff02);
	}
	break;
	case Bloodangel_Stick:
	{

		glColor3fv(BodyLight);

		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float v810 = Render22(-4.f, 0.f);
		RenderMesh(0, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 0, v810, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_bloodangelstick01_render);
		float v407 = ((int)(timeGetTime() * 0.05f) % 16 / 4) * 0.25f;
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, v407, -1);
	}
	break;
	//19/10/2020
	case Silver_Heart_Sword:
	{

		glColor3fv(BodyLight);

		float RenderWizardBE = Render22(-4.f, 0.f) * 0.7f + 0.1f;
		if (RenderWizardBE > 1.f)
			RenderWizardBE = 1.f;
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, RenderWizardBE, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, 68, pObj->Alpha, 2, RenderWizardBE, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_black_gold_crom02_mono);
		RenderMesh(3, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 3, RenderWizardBE, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(3, 68, pObj->Alpha, 3, RenderWizardBE, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_black_gold_crom02_mono);
		BodyLight[0] = 0.35f;
		BodyLight[1] = 0.3f;
		BodyLight[2] = 0.5f;
		RenderMesh(4, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 4, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_gra3);
		RenderMesh(4, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 4, RenderWizardBE, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_gra3);
	}
	break;
	case Silver_Heart_MagicSword:
	{

		glColor3fv(BodyLight);

		float RenderWizardBE = Render22(-4.f, 0.f) * 0.7f + 0.1f;
		if (RenderWizardBE > 1.f)
			RenderWizardBE = 1.f;
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, RenderWizardBE, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, 68, pObj->Alpha, 2, RenderWizardBE, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_black_gold_crom02_mono);
		RenderMesh(3, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 3, RenderWizardBE, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(3, 68, pObj->Alpha, 3, RenderWizardBE, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_black_gold_crom02_mono);
		BodyLight[0] = 0.25f;
		BodyLight[1] = 0.05f;
		BodyLight[2] = 0.525f;
		RenderMesh(4, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 4, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_gra3);
		RenderMesh(4, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 4, RenderWizardBE, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_gra3);
	}
	break;
	case Silver_Heart_Claws:
	{

		glColor3fv(BodyLight);

		float RenderWizardBE = Render22(-4.f, 0.f) * 0.7f + 0.3f;
		if (RenderWizardBE > 1.f)
			RenderWizardBE = 1.f;
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(4, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, RenderWizardBE, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, 68, pObj->Alpha, 1, RenderWizardBE, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_black_gold_crom02);
		RenderMesh(3, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 3, RenderWizardBE, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(3, 68, pObj->Alpha, 3, RenderWizardBE, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_black_gold_crom02);
		BodyLight[0] = 0.5f;
		BodyLight[1] = 0.07f;
		BodyLight[2] = 0.3f;
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_gra3);
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, RenderWizardBE, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_gra3);
	}
	break;
	case Silver_Heart_ShortSword:
	{

		glColor3fv(BodyLight);

		float RenderWizardBE = Render22(-4.f, 0.f) * 0.7f + 0.3f;
		if (RenderWizardBE > 1.f)
			RenderWizardBE = 1.f;
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(3, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, RenderWizardBE, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, 68, pObj->Alpha, 1, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_black_gold_crom02);
		RenderMesh(4, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 4, RenderWizardBE, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(4, 68, pObj->Alpha, 4, RenderWizardBE, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_black_gold_crom02);
		BodyLight[0] = 0.15f;
		BodyLight[1] = 0.f;
		BodyLight[2] = 0.5f;
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_gra3);
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, RenderWizardBE, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_gra3);
	}
	break;
	case Silver_Heart_Scepter:
	{

		glColor3fv(BodyLight);

		float RenderWizardBE = Render22(-4.f, 0.f) * 0.7f + 0.3f;
		if (RenderWizardBE > 1.f)
			RenderWizardBE = 1.f;
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, RenderWizardBE, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, 68, pObj->Alpha, 1, RenderWizardBE, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_black_gold_crom02);
		RenderMesh(3, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 3, RenderWizardBE, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(3, 68, pObj->Alpha, 3, RenderWizardBE, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_black_gold_crom02);
		BodyLight[0] = 0.5f;
		BodyLight[1] = 0.07f;
		BodyLight[2] = 0.3f;
		RenderMesh(4, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 4, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_gra3);
		RenderMesh(4, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 4, RenderWizardBE, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_gra3);
	}
	break;
	case Silver_Heart_Mace:
	{

		glColor3fv(BodyLight);

		float RenderWizardBE = Render22(-4.f, 0.f) * 0.7f + 0.3f;
		if (RenderWizardBE > 1.f)
			RenderWizardBE = 1.f;
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, RENDER_TEXTURE, pObj->Alpha, 2, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, RenderWizardBE, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, 68, pObj->Alpha, 1, RenderWizardBE, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_black_gold_crom02);
		RenderMesh(3, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 3, RenderWizardBE, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(3, 68, pObj->Alpha, 3, RenderWizardBE, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_black_gold_crom02);
		BodyLight[0] = 0.5f;
		BodyLight[1] = 0.07f;
		BodyLight[2] = 0.3f;
		RenderMesh(4, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 4, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_gra3);
		RenderMesh(4, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 4, RenderWizardBE, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_gra3);
	}
	break;
	case Silver_Heart_Lance://3.30
	{

		glColor3fv(BodyLight);

		float RenderWizardBE = Render22(-4.0f, 0.0f) * 0.7f + 0.300000011920929f;
		if (RenderWizardBE > 1.0f)
			RenderWizardBE = 1.0f;
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, RenderWizardBE, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, 68, pObj->Alpha, 2, RenderWizardBE, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_black_gold_crom02);
		RenderMesh(3, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 3, RenderWizardBE, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(3, 68, pObj->Alpha, 3, RenderWizardBE, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_black_gold_crom02);
		BodyLight[0] = 0.5f;
		BodyLight[1] = 0.07f;
		BodyLight[2] = 0.3f;
		RenderMesh(4, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 4, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(4, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 4, RenderWizardBE, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
	}
	break;
	case Silver_Heart_Bow:
	{

		glColor3fv(BodyLight);

		float RenderWizardBE = Render22(-4.0f, 0.0f) * 0.7f + 0.300000011920929f;
		if (RenderWizardBE > 1.0f)
			RenderWizardBE = 1.0f;
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(3, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(5, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 5, 0.5, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(5, 68, pObj->Alpha, 5, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_black_gold_crom02);
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, RenderWizardBE, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, 68, pObj->Alpha, 2, RenderWizardBE, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_black_gold_crom02);
		RenderMesh(4, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 4, RenderWizardBE, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(4, 68, pObj->Alpha, 4, RenderWizardBE, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_black_gold_crom02);
		//if (SafeZone(Hero) == 0)
		//{
		BodyLight[0] = 0.6f;
		BodyLight[1] = 0.4f;
		BodyLight[2] = 0.95f;
		RenderMesh(6, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 6, RenderWizardBE, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		//}
		BodyLight[0] = 0.12f;
		BodyLight[1] = 0.07f;
		BodyLight[2] = 0.37f;
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_gra3);
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, RenderWizardBE, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_gra3);
	}
	break;
	case Silver_Heart_Staff:
	{

		glColor3fv(BodyLight);

		float RenderWizardBE = Render22(-4.f, 0.0f) * 0.7f + 0.300000011920929f;
		if (RenderWizardBE > 1.0f)
			RenderWizardBE = 1.0f;
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(4, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, RenderWizardBE, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, 68, pObj->Alpha, 1, RenderWizardBE, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_black_gold_crom02);
		RenderMesh(5, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 5, RenderWizardBE, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(5, 68, pObj->Alpha, 5, RenderWizardBE, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_black_gold_crom02);
		float v1438 = (int)(timeGetTime() % 2000) * 0.0005000000237487257f;
		float v934 = -v1438;
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, pObj->BlendMeshLight, 0.0f, v934, -1);
		BodyLight[0] = 0.12f;
		BodyLight[1] = 0.07f;
		BodyLight[2] = 0.37f;
		RenderMesh(3, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 3, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_gra3);
		RenderMesh(3, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 3, RenderWizardBE, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_gra3);
	}
	break;
	case Silver_Heart_Stick:
	{

		glColor3fv(BodyLight);

		float RenderWizardBE = Render22(-4.0f, 0.0f) * 0.7f + 0.3f;
		if (RenderWizardBE > 1.0f)
			RenderWizardBE = 1.0f;
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(3, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, RenderWizardBE, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, 68, pObj->Alpha, 2, RenderWizardBE, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_black_gold_crom02);
		RenderMesh(4, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 4, RenderWizardBE, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(4, 68, pObj->Alpha, 4, RenderWizardBE, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_black_gold_crom02);
		int v1431 = timeGetTime() % (15 * 40) / 40;
		float v1432 = (float)(v1431 % 4) * 0.25f;
		float v1435 = (float)(v1431 / 4) * 0.25f;
		BodyLight[0] = 0.75f;
		BodyLight[1] = 0.15f;
		BodyLight[2] = 1.0f;
		RenderMesh(5, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 5, pObj->BlendMeshLight, v1432, v1435, -1);
		BodyLight[0] = 0.12f;
		BodyLight[1] = 0.07f;
		BodyLight[2] = 0.37f;
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_gra3);
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, RenderWizardBE, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_gra3);
	}
	break;
	//20/10/2020
	case Cyclone_Sword:
	{

		glColor3fv(BodyLight);

		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		BodyLight[0] = 1.0;
		BodyLight[1] = 1.0;
		BodyLight[2] = 1.0;
		RenderMesh(1, 68, pObj->Alpha, 1, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_CHROME);//Chrome01.jpg
	}
	break;
	case Blast_Break:
	{

		glColor3fv(BodyLight);

		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		BodyLight[0] = 1.0;
		BodyLight[1] = 1.0;
		BodyLight[2] = 1.0;
		float v443 = timeGetTime() * 0.002f;
		float v1817 = (sinf(v443) + 1.0f) * 0.3f + 0.3f;
		float v444 = pObj->BlendMeshLight * v1817;
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, -2, v444, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, RENDER_TEXTURE, pObj->Alpha, -2, v444, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float v220 = (float)((int)timeGetTime() % 10000) * 0.0003f;
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, v220, BITMAP_CHROME);//Chrome01.jpg
	}
	break;
	case Horn_of_Steal:
	{
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float v224 = timeGetTime() * 0.002f;
		float v258 = (sinf(v224) + 1.f) * 0.2f + 0.4f;
		float v225 = pObj->BlendMeshLight * v258;
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, -2, v225, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
	}
	break;
	case Magmus_Peer:
	{
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float v224 = timeGetTime() * 0.002f;
		float v258 = (sinf(v224) + 1.f) * 0.3f + 0.3f;
		float v225 = pObj->BlendMeshLight * v258;
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, -2, v225, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float v220 = (float)((signed int)timeGetTime() % 10000) * 0.0003f;
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, v220, BITMAP_flame_chrome);
	}
	break;
	case Angelic_Bow:
	{
		float v215 = timeGetTime() * 0.003f;
		float v262 = sinf(v215) * 0.25f;
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		BodyLight[0] = v262 + 0.7f;
		BodyLight[1] = v262 + 0.7f;
		BodyLight[2] = v262 + 0.7f;
		RenderMesh(0, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, -2, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_AngelicBow_overlay);
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
	}
	break;
	case Miracle_Staff:
	{
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float v224 = timeGetTime() * 0.002f;
		float v258 = (sinf(v224) + 1.f) * 0.3f + 0.5f;
		float v225 = pObj->BlendMeshLight * v258;
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, -2, v225, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, RENDER_TEXTURE, pObj->Alpha, -2, v225, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
	}
	break;
	case Lazy_Wind_Shield:
	{

		glColor3fv(BodyLight);

		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float Parpadeo1 = (float)(rand() % 20) / 32.5f;
		float Parpadeo2 = Parpadeo1 + 0.8f;
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, Parpadeo2, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
	}
	break;
	case Light_Lord_Shield:
	case Dark_Devil_Shield:
	{

		glColor3fv(BodyLight);

		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float Parpadeo1 = (float)(rand() % 20) / 32.5f;
		float Parpadeo2 = Parpadeo1 + 0.1f;
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, Parpadeo2, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, Parpadeo2, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
	}
	break;
	case Magic_Knight_Shield:
	{

		glColor3fv(BodyLight);

		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float v224 = timeGetTime() * 0.002f;
		float v258 = (sinf(v224) + 1.f) * 0.3f + 0.5f;
		float v225 = pObj->BlendMeshLight * v258;
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, -2, v225, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		BodyLight[0] = 1.f;
		BodyLight[1] = 1.f;
		BodyLight[2] = 1.f;
		RenderMesh(2, 68, pObj->Alpha, 2, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_CHROME);//Chrome01.jpg
		RenderMesh(2, 68, pObj->Alpha, 2, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_CHROME);//Chrome01.jpg
	}
	break;
	case Wings_of_Hit:
	{

		glColor3fv(BodyLight);

		float RenderWizardBE = Render22(-4.f, 0.f) * 0.7f + 0.3f;
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, RenderWizardBE, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_sd_wing3C_render);
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, RenderWizardBE, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_sd_wing3B_render);
	}
	break;
	case Kit_StylizedMoon_Kapocha:
	{

		glColor3fv(BodyLight);

		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(3, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, 68, pObj->Alpha, 1, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_K_StylizedMoon_E02);
		RenderMesh(2, 68, pObj->Alpha, 2, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_K_StylizedMoon_E02);
		RenderMesh(1, 68, pObj->Alpha, 1, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_K_StylizedMoon_E02);
		int BlessedSword01 = 1000;
		float BlessedSword02 = (((int)((BlessedSword01 - timeGetTime())) % 1001)) / (float)BlessedSword01;
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, 1, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, -BlessedSword02, -1);
		float v979 = Render22(-4.f, 120.f);
		float v977 = Render22(-4.f, 240.f);
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, v979, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_K_StylizedMoon_SwordB02_Render01);
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, v977, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_K_StylizedMoon_SwordB02_Render02);
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, v979, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_K_StylizedMoon_SwordB02_Render01);
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, v977, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_K_StylizedMoon_SwordB02_Render02);
	}
	break;
	//Agregado 20/12/2020
	case Bloody_wings:
	{

		glColor3fv(BodyLight);

		float RenderWizardBE = Render22(-4.f, 0.f) * 0.7f + 0.3f;
		RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(3, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, RenderWizardBE, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_sd_wing4D_render);
		RenderMesh(3, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 3, RenderWizardBE, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_sd_wing4C_Red_render);
		runtime_render_level(pObj, 0, 124, 55, 32);
		BodyLight[0] = (float)(100 / 255.0f);
		BodyLight[1] = (float)(100 / 255.0f);
		BodyLight[2] = (float)(100 / 255.0f);
		RenderMesh(1, 68, pObj->Alpha, 1, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_CHROME);//Chrome01.jpg
	}
	break;
	//Agregado 21/12/2020
	case Cloak_of_Isolation:
	{
		if (BodyLight[0] != 1.f || BodyLight[1] != 1.f || BodyLight[2] != 1.f)
		{
			BodyLight[0] = 1.f;
			BodyLight[1] = 1.f;
			BodyLight[2] = 1.f;

			glColor3fv(BodyLight);

			RenderMesh(0, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
			RenderMesh(2, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
			RenderMesh(1, RENDER_TEXTURE, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
			float v2165 = Render22(-5.f, 0.f);
			RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, v2165, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_growlancerwing4db_render);
			v2165 = Render22(-3.f, 0.f) * 0.5f;
			RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, v2165, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
			float Parpadeo1 = (float)(rand() % 20) / 23.5f;
			float Parpadeo2 = Parpadeo1 + 0.5f;
			RenderMesh(3, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 3, Parpadeo2, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_gra3);
			runtime_render_level(pObj, 0, 195, 127, 125);
		}
		else
		{
			BodyLight[0] = 1.f;
			BodyLight[1] = 1.f;
			BodyLight[2] = 1.f;
			glColor3fv(BodyLight);
			RenderBody(2, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1, -1);
		}
		float v2165 = Render22(-5.f, 0.f);
		RenderMesh(1, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 1, v2165, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_growlancerwing4db_render);
		v2165 = Render22(-3.f, 0.f) * 0.5f;
		RenderMesh(2, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 2, v2165, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		float Parpadeo1 = (float)(rand() % 20) / 23.5f;
		float Parpadeo2 = Parpadeo1 + 0.5f;
		RenderMesh(3, RENDER_TEXTURE | RENDER_BRIGHT, pObj->Alpha, 3, Parpadeo2, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, BITMAP_gra3);
		runtime_render_level(pObj, 0, 195, 127, 125);
	}
	break;
	default:
		this->RenderBody(2, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, pObj->HiddenMesh, -1);
		break;
	}
	return false;
}

bool BMD::runtime_make_effect(int EffectIndex, OBJECT* pObj, float Alpha, int RenderType, int Texture)
{
	switch (EffectIndex)
	{
	case Blessed_Divine_Staff:
	case Blessed_Divine_Mace:
	case Awakening_Sword:
	case Darkangel_Lance:
	case Awakening_Staff:
	case Bloodangel_Shield:
	case Darkangel_Shield:
	case Awakening_Shield:
	case Blueeye_Shield:
	case Blue_Eye_Sword:
		this->RenderMesh(0, RenderType, Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, Texture);
		this->RenderMesh(1, RenderType, Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, Texture);
		this->RenderMesh(2, RenderType, Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, Texture);
		break;
	case Blessed_Divine_Sword:
	case Blessed_Divine_Scepter:
		//case Blessed_Divine_Claw:
	case Blessed_Divine_Lancer:
	case Holyangel_Sword:
	case Awakening_Bow:
	case Holyangel_ShortSword:
	case Silver_Heart_Sword:
	case Silver_Heart_MagicSword:
	case Silver_Heart_Lance:
		//case Awakening_Claw:
	case Awakening_MagicSword:
	case Holyangel_Lance:
		//case Trigis_Claw:
		this->RenderMesh(0, RenderType, Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, Texture);
		this->RenderMesh(1, RenderType, Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, Texture);
		break;
	case Holyangel_Scepter:
		//case Absolute_Claw:
	case Absolute_Mace:
	case Bloodangel_Sword:
	case Bloodangel_MagicSword:
	case Silver_Heart_Scepter:
	case Silver_Heart_Mace:
		this->RenderMesh(0, RenderType, Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, Texture);
		this->RenderMesh(2, RenderType, Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, Texture);
		break;
	case Silver_Heart_ShortSword:
	case Silver_Heart_Bow:
	case Silver_Heart_Stick:
		this->RenderMesh(0, RenderType, Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, Texture);
		this->RenderMesh(3, RenderType, Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, Texture);
		break;
		//case Silver_Heart_Claws:
	case Silver_Heart_Staff:
		this->RenderMesh(0, RenderType, Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, Texture);
		this->RenderMesh(4, RenderType, Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, Texture);
		break;
	case Blessed_Divine_CrossBow:
		this->RenderMesh(0, RenderType, Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, Texture);
		this->RenderMesh(1, RenderType, Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, Texture);
		this->RenderMesh(2, RenderType, Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, Texture);
		this->RenderMesh(4, RenderType, Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, Texture);
		break;
	case Blessed_Divine_Stick:
		this->RenderMesh(0, RenderType, Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, Texture);
		this->RenderMesh(1, RenderType, Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, Texture);
		this->RenderMesh(5, RenderType, Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, Texture);
		break;
	case Blessed_Divine_ShortSword:
	case Holyangel_Staff:
		this->RenderMesh(0, RenderType, Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, Texture);
		this->RenderMesh(2, RenderType, Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, Texture);
		this->RenderMesh(3, RenderType, Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, Texture);
		break;
		//case Holyangel_Claw:
	case Darkangel_Bow:
	case Darkangel_MagicSword:
		this->RenderMesh(1, RenderType, Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, Texture);
		break;
	case Wings_of_Condemnation:
		this->RenderMesh(3, RenderType, Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, Texture);
		break;
	case Holyangel_Shield:
		this->RenderMesh(0, RenderType, Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, Texture);
		this->RenderMesh(1, RenderType, Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, Texture);
		this->RenderMesh(2, RenderType, Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, Texture);
		this->RenderMesh(3, RenderType, Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, Texture);
		break;
	case Silverheart_Shield:
		this->RenderMesh(0, RenderType, Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, Texture);
		this->RenderMesh(1, RenderType, Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, Texture);
		this->RenderMesh(3, RenderType, Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, Texture);
		break;
	case Blessed_Divine_Claw:
	case Awakening_Claw:
	case Trigis_Claw:
		this->RenderMesh(0, RenderType, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		this->RenderMesh(1, RenderType, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		break;
	case Absolute_Claw:
		this->RenderMesh(0, RenderType, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		this->RenderMesh(2, RenderType, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		break;
	case Silver_Heart_Claws:
		this->RenderMesh(0, RenderType, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		this->RenderMesh(4, RenderType, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		break;
	case Holyangel_Claw:
		this->RenderMesh(1, RenderType, pObj->Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, -1);
		break;
	default:
		this->RenderMesh(0, RenderType, Alpha, pObj->BlendMesh, pObj->BlendMeshLight, pObj->BlendMeshTexCoordU, pObj->BlendMeshTexCoordV, Texture);
		break;
	}
	return false;
}

#endif // EFFECT_KERNEL_VERTEX
