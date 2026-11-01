//OPENCODE - Profiling system: Real-time GPU/CPU metrics for optimization
#pragma once

#include <string>
#include <array>
#include <chrono>
#include <GL/glew.h>

class RenderStats
{
public:
    enum Category
    {
        CAT_TERRAIN = 0,
        CAT_CHARACTERS,
        CAT_OBJECTS,
        CAT_EFFECTS,
        CAT_PARTICLES,
        CAT_UI,
        CAT_SHADOWS,
        CAT_COUNT
    };

    struct FrameStats
    {
        // Timing
        double cpuFrameTimeMs = 0.0;
        double gpuFrameTimeMs = 0.0;
        double renderThreadTimeMs = 0.0;
        
        // Draw calls
        uint32_t drawCalls = 0;
        uint32_t drawCallsInstanced = 0;
        uint32_t stateChanges = 0;
        uint32_t textureBinds = 0;
        uint32_t shaderBinds = 0;
        uint32_t vboBinds = 0;
        
        // Geometry
        uint32_t triangles = 0;
        uint32_t vertices = 0;
        uint32_t instances = 0;
        
        // Per-category
        uint32_t drawCallsPerCat[CAT_COUNT] = {0};
        uint32_t trianglesPerCat[CAT_COUNT] = {0};
        uint32_t objectsRenderedPerCat[CAT_COUNT] = {0};
        uint32_t objectsCulledPerCat[CAT_COUNT] = {0};
        
        // Memory
        uint64_t vboMemoryBytes = 0;
        uint64_t textureMemoryBytes = 0;
        
        // FPS
        float fps = 0.0f;
        float frameTimeMs = 0.0f;
    };

    struct QueryData
    {
        GLuint queryId = 0;
        bool inUse = false;
        std::chrono::steady_clock::time_point startTime;
        Category category = CAT_TERRAIN;
    };

    static constexpr int MAX_QUERIES = 64;
    static constexpr int HISTORY_SIZE = 120;

    RenderStats();
    ~RenderStats();

    void Initialize();
    void Shutdown();

    // Frame lifecycle
    void BeginFrame();
    void EndFrame();
    
    // Draw call tracking
    void BeginDrawCall(Category cat, uint32_t triangleCount, uint32_t vertexCount, uint32_t instanceCount = 1);
    void EndDrawCall();
    
    // State change tracking
    void RecordTextureBind();
    void RecordShaderBind();
    void RecordVboBind();
    void RecordStateChange();
    
    // Culling tracking
    void RecordObjectRendered(Category cat);
    void RecordObjectCulled(Category cat);
    
    // Memory tracking
    void RecordVboAllocation(uint64_t bytes);
    void RecordTextureAllocation(uint64_t bytes);

    // Query-based GPU timing (optional, for detailed profiling)
    GLuint BeginGpuQuery(Category cat);
    void EndGpuQuery(GLuint queryId);
    
    // Access
    const FrameStats& GetCurrentFrame() const { return m_CurrentFrame; }
    const FrameStats& GetPreviousFrame() const { return m_PreviousFrame; }
    const std::array<FrameStats, HISTORY_SIZE>& GetHistory() const { return m_History; }
    int GetHistoryCount() const { return m_HistoryCount; }
    
    // Formatted output
    std::string GetFormattedStats() const;
    std::string GetFormattedCategoryStats() const;
	void WritePeriodicLog(const char* path, unsigned int intervalMilliseconds = 5000);

    // Config
    void SetEnabled(bool enabled) { m_Enabled = enabled; }
    bool IsEnabled() const { return m_Enabled; }

    // OPENCODE: Legacy API compatibility (for ZzzCharacter.cpp etc.)
    void AddDrawCalls(unsigned int count = 1) { if (m_Enabled) m_CurrentFrame.drawCalls += count; }
    void AddTextureChanges(unsigned int count = 1) { if (m_Enabled) m_CurrentFrame.textureBinds += count; }
    void AddProgramChanges(unsigned int count = 1) { if (m_Enabled) m_CurrentFrame.shaderBinds += count; }
    void AddRenderedObjects(unsigned int count = 1) { if (m_Enabled) m_CurrentFrame.objectsRenderedPerCat[CAT_OBJECTS] += count; }
    void AddCulledObjects(unsigned int count = 1) { if (m_Enabled) m_CurrentFrame.objectsCulledPerCat[CAT_OBJECTS] += count; }
    const FrameStats& GetLastFrame() const { return m_PreviousFrame; }

private:
    FrameStats m_CurrentFrame;
    FrameStats m_PreviousFrame;
    std::array<FrameStats, HISTORY_SIZE> m_History;
    int m_HistoryIndex = 0;
    int m_HistoryCount = 0;
    
    std::array<QueryData, MAX_QUERIES> m_Queries;
    int m_ActiveQueries = 0;
    
    std::chrono::steady_clock::time_point m_FrameStartTime;
    std::chrono::steady_clock::time_point m_DrawCallStartTime;
    Category m_CurrentCategory = CAT_TERRAIN;
    bool m_InDrawCall = false;
    bool m_Enabled = true;
    
    uint64_t m_TotalVboMemory = 0;
    uint64_t m_TotalTextureMemory = 0;
	unsigned long long m_LastLogTick = 0;

    int AllocateQuery();
    void ReleaseQuery(int index);
    void UpdateHistory();
};

extern RenderStats g_RenderStats;
