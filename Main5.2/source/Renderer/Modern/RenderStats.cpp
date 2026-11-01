//OPENCODE - Profiling system: Real-time GPU/CPU metrics for optimization
#include "stdafx.h"
#include "RenderStats.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <fstream>
#include <windows.h>

RenderStats g_RenderStats;

RenderStats::RenderStats()
{
    for (auto& q : m_Queries)
    {
        q.queryId = 0;
        q.inUse = false;
    }
}

RenderStats::~RenderStats()
{
    Shutdown();
}

void RenderStats::Initialize()
{
    if (GLEW_ARB_timer_query || GLEW_EXT_timer_query)
    {
        glGenQueries(MAX_QUERIES, &m_Queries[0].queryId);
    }
    m_Enabled = true;
}

void RenderStats::Shutdown()
{
    if (m_Queries[0].queryId != 0)
    {
        glDeleteQueries(MAX_QUERIES, &m_Queries[0].queryId);
        for (auto& q : m_Queries)
        {
            q.queryId = 0;
            q.inUse = false;
        }
    }
}

void RenderStats::BeginFrame()
{
    if (!m_Enabled) return;
    
    m_FrameStartTime = std::chrono::steady_clock::now();
    m_CurrentFrame = FrameStats();
    m_CurrentFrame.vboMemoryBytes = m_TotalVboMemory;
    m_CurrentFrame.textureMemoryBytes = m_TotalTextureMemory;
}

void RenderStats::EndFrame()
{
    if (!m_Enabled) return;
    
    auto now = std::chrono::steady_clock::now();
    m_CurrentFrame.cpuFrameTimeMs = std::chrono::duration<double, std::milli>(now - m_FrameStartTime).count();
    m_CurrentFrame.frameTimeMs = static_cast<float>(m_CurrentFrame.cpuFrameTimeMs);
    m_CurrentFrame.fps = (m_CurrentFrame.frameTimeMs > 0.0f) ? (1000.0f / m_CurrentFrame.frameTimeMs) : 0.0f;
    
    // Collect GPU query results
    for (auto& q : m_Queries)
    {
        if (q.inUse && q.queryId != 0)
        {
            GLuint available = 0;
            glGetQueryObjectuiv(q.queryId, GL_QUERY_RESULT_AVAILABLE, &available);
            if (available)
            {
                GLuint64 elapsed = 0;
                glGetQueryObjectui64v(q.queryId, GL_QUERY_RESULT, &elapsed);
                m_CurrentFrame.gpuFrameTimeMs += static_cast<double>(elapsed) / 1000000.0;
                q.inUse = false;
            }
        }
    }
    
    UpdateHistory();
    m_PreviousFrame = m_CurrentFrame;
}

void RenderStats::WritePeriodicLog(const char* path, unsigned int intervalMilliseconds)
{
    if (!m_Enabled || path == nullptr || path[0] == '\0') return;
    const unsigned long long now = GetTickCount64();
    if (m_LastLogTick != 0 && now - m_LastLogTick < intervalMilliseconds) return;
    m_LastLogTick = now;

    char resolvedPath[MAX_PATH] = {};
    const char* outputPath = path;
    if (path[0] != '\\' && !(path[0] != '\0' && path[1] == ':'))
    {
        const DWORD length = GetModuleFileNameA(nullptr, resolvedPath, MAX_PATH);
        if (length > 0 && length < MAX_PATH)
        {
            char* separator = strrchr(resolvedPath, '\\');
            if (separator != nullptr)
            {
                *(separator + 1) = '\0';
                const char* filename = path;
                while (filename[0] == '.' && (filename[1] == '\\' || filename[1] == '/'))
                    filename += 2;
                strcat_s(resolvedPath, filename);
                outputPath = resolvedPath;
            }
        }
    }

    std::ofstream log(outputPath, std::ios::out | std::ios::app);
    if (!log.is_open()) return;
    log << GetFormattedStats() << " | " << GetFormattedCategoryStats() << std::endl;
}

void RenderStats::UpdateHistory()
{
    m_History[m_HistoryIndex] = m_CurrentFrame;
    m_HistoryIndex = (m_HistoryIndex + 1) % HISTORY_SIZE;
    if (m_HistoryCount < HISTORY_SIZE) m_HistoryCount++;
}

void RenderStats::BeginDrawCall(Category cat, uint32_t triangleCount, uint32_t vertexCount, uint32_t instanceCount)
{
    if (!m_Enabled || m_InDrawCall) return;
    
    m_InDrawCall = true;
    m_CurrentCategory = cat;
    m_DrawCallStartTime = std::chrono::steady_clock::now();
    
    m_CurrentFrame.drawCalls++;
    m_CurrentFrame.triangles += triangleCount * instanceCount;
    m_CurrentFrame.vertices += vertexCount * instanceCount;
    m_CurrentFrame.instances += instanceCount;
    m_CurrentFrame.drawCallsPerCat[cat]++;
    m_CurrentFrame.trianglesPerCat[cat] += triangleCount * instanceCount;
    
    if (instanceCount > 1)
    {
        m_CurrentFrame.drawCallsInstanced++;
    }
}

void RenderStats::EndDrawCall()
{
    if (!m_Enabled || !m_InDrawCall) return;
    
    auto now = std::chrono::steady_clock::now();
    double elapsedMs = std::chrono::duration<double, std::milli>(now - m_DrawCallStartTime).count();
    m_CurrentFrame.renderThreadTimeMs += elapsedMs;
    m_InDrawCall = false;
}

void RenderStats::RecordTextureBind()
{
    if (!m_Enabled) return;
    m_CurrentFrame.textureBinds++;
}

void RenderStats::RecordShaderBind()
{
    if (!m_Enabled) return;
    m_CurrentFrame.shaderBinds++;
}

void RenderStats::RecordVboBind()
{
    if (!m_Enabled) return;
    m_CurrentFrame.vboBinds++;
}

void RenderStats::RecordStateChange()
{
    if (!m_Enabled) return;
    m_CurrentFrame.stateChanges++;
}

void RenderStats::RecordObjectRendered(Category cat)
{
    if (!m_Enabled) return;
    m_CurrentFrame.objectsRenderedPerCat[cat]++;
}

void RenderStats::RecordObjectCulled(Category cat)
{
    if (!m_Enabled) return;
    m_CurrentFrame.objectsCulledPerCat[cat]++;
}

void RenderStats::RecordVboAllocation(uint64_t bytes)
{
    if (!m_Enabled) return;
    m_TotalVboMemory += bytes;
}

void RenderStats::RecordTextureAllocation(uint64_t bytes)
{
    if (!m_Enabled) return;
    m_TotalTextureMemory += bytes;
}

int RenderStats::AllocateQuery()
{
    for (int i = 0; i < MAX_QUERIES; ++i)
    {
        if (!m_Queries[i].inUse && m_Queries[i].queryId != 0)
        {
            m_Queries[i].inUse = true;
            m_Queries[i].startTime = std::chrono::steady_clock::now();
            return i;
        }
    }
    return -1;
}

void RenderStats::ReleaseQuery(int index)
{
    if (index >= 0 && index < MAX_QUERIES)
    {
        m_Queries[index].inUse = false;
    }
}

GLuint RenderStats::BeginGpuQuery(Category cat)
{
    if (!m_Enabled) return 0;
    
    int idx = AllocateQuery();
    if (idx < 0) return 0;
    
    m_Queries[idx].category = cat;
    glBeginQuery(GL_TIME_ELAPSED, m_Queries[idx].queryId);
    return m_Queries[idx].queryId;
}

void RenderStats::EndGpuQuery(GLuint queryId)
{
    if (!m_Enabled || queryId == 0) return;
    glEndQuery(GL_TIME_ELAPSED);
}

std::string RenderStats::GetFormattedStats() const
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    
    oss << "=== RENDER STATS ===\n";
    oss << "FPS: " << m_PreviousFrame.fps << " | Frame: " << m_PreviousFrame.frameTimeMs << "ms\n";
    oss << "CPU: " << m_PreviousFrame.cpuFrameTimeMs << "ms | GPU: " << m_PreviousFrame.gpuFrameTimeMs << "ms | RenderThread: " << m_PreviousFrame.renderThreadTimeMs << "ms\n";
    oss << "DrawCalls: " << m_PreviousFrame.drawCalls << " (Instanced: " << m_PreviousFrame.drawCallsInstanced << ")\n";
    oss << "Triangles: " << m_PreviousFrame.triangles << " | Vertices: " << m_PreviousFrame.vertices << " | Instances: " << m_PreviousFrame.instances << "\n";
    oss << "StateChanges: " << m_PreviousFrame.stateChanges << " | TexBinds: " << m_PreviousFrame.textureBinds << " | ShaderBinds: " << m_PreviousFrame.shaderBinds << " | VBOBinds: " << m_PreviousFrame.vboBinds << "\n";
    oss << "VBO Mem: " << (m_PreviousFrame.vboMemoryBytes / 1024 / 1024) << "MB | Tex Mem: " << (m_PreviousFrame.textureMemoryBytes / 1024 / 1024) << "MB\n";
    
    return oss.str();
}

std::string RenderStats::GetFormattedCategoryStats() const
{
    static const char* catNames[] = {"TERRAIN", "CHARACTERS", "OBJECTS", "EFFECTS", "PARTICLES", "UI", "SHADOWS"};
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1);
    
    oss << "\n=== PER CATEGORY ===\n";
    for (int i = 0; i < CAT_COUNT; ++i)
    {
        uint32_t rendered = m_PreviousFrame.objectsRenderedPerCat[i];
        uint32_t culled = m_PreviousFrame.objectsCulledPerCat[i];
        uint32_t total = rendered + culled;
        float cullPct = total > 0 ? (float)culled / total * 100.0f : 0.0f;
        
        oss << catNames[i] << ": DrawCalls=" << m_PreviousFrame.drawCallsPerCat[i]
            << " Tris=" << m_PreviousFrame.trianglesPerCat[i]
            << " Rendered=" << rendered << " Culled=" << culled << " (" << cullPct << "%)\n";
    }
    
    return oss.str();
}
