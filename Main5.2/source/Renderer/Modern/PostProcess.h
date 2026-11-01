#pragma once

#include "Renderer/Modern/ModernRenderBackend.h"
#include "Renderer/Modern/GLResources.h"
#include "Renderer/Modern/ShaderProgram.h"
#include "Renderer/Modern/ShaderManager.h"

class PostProcessPipeline
{
public:
    PostProcessPipeline();
    ~PostProcessPipeline();

    // The enhanced engine can run over the stable Legacy geometry path.
    // backend may be nullptr; OpenGL 3.3 compatibility is the real requirement.
    bool Initialize(ModernRenderBackend* backend, int width, int height);
    void Shutdown();
    void Resize(int width, int height);

    void BeginFrame();
    void EndFrame(ModernRenderBackend* backend);
    void RenderToScreen(ModernRenderBackend* backend);

    bool IsInitialized() const { return m_Initialized; }
    bool IsEnabled() const { return m_Initialized && m_Enabled; }
    bool IsCapturing() const { return m_Capturing; }

    // Settings
    void SetExposure(float exposure) { m_Exposure = exposure; }
    void SetBloomThreshold(float threshold) { m_BloomThreshold = threshold; }
    void SetBloomIntensity(float intensity) { m_BloomIntensity = intensity; }
    void SetBloomRadius(float radius) { m_BloomRadius = radius; }
    void SetTonemapMode(int mode) { m_TonemapMode = mode; }
    void SetGamma(float gamma) { m_Gamma = gamma; }
    void SetFXAAEnabled(bool enabled) { m_FXAAEnabled = enabled; }
    void SetVignetteEnabled(bool enabled) { m_VignetteEnabled = enabled; }
    void SetVignetteIntensity(float intensity) { m_VignetteIntensity = intensity; }
    void SetColorGradingEnabled(bool enabled) { m_ColorGradingEnabled = enabled; }
    void SetColorGradingLUT(GLuint lutTexture) { m_ColorGradingLUT = lutTexture; }

    GLuint GetSceneTexture() const { return m_SceneColorTexture; }
    GLuint GetBloomTexture() const { return m_BloomTextures[1]; }

private:
    ModernRenderBackend* m_Backend = nullptr;
    ShaderManager m_ShaderManager;
    ShaderProgram* m_BrightPassShader = nullptr;
    ShaderProgram* m_BlurShader = nullptr;
    ShaderProgram* m_CompositeShader = nullptr;
    ShaderProgram* m_FXAAShader = nullptr;
    ShaderProgram* m_CopyShader = nullptr;

    GLuint m_SceneFBO = 0;
    GLuint m_SceneColorTexture = 0;
    GLuint m_SceneDepthTexture = 0;

    GLuint m_BrightFBO = 0;
    GLuint m_BrightTexture = 0;

    static constexpr int BLOOM_LEVELS = 2;
    GLuint m_BloomFBO[BLOOM_LEVELS] = { 0 };
    GLuint m_BloomTextures[BLOOM_LEVELS] = { 0 };
    int m_BloomTextureWidth[BLOOM_LEVELS] = { 0 };
    int m_BloomTextureHeight[BLOOM_LEVELS] = { 0 };

    GLuint m_CompositeFBO = 0;
    GLuint m_CompositeTexture = 0;

    GLVertexArray m_QuadVAO;
    GLBuffer m_QuadVBO;

    bool m_Initialized = false;
    bool m_Enabled = true;
    bool m_Capturing = false;
    bool m_BloomEnabled = true;
    bool m_WorldLightingEnabled = true;
    DWORD m_LastConfigRead = 0;

    float m_Exposure = 1.03f;
    float m_Contrast = 1.08f;
    float m_Saturation = 1.06f;
    float m_Sharpen = 0.15f;
    float m_BloomThreshold = 0.80f;
    float m_BloomIntensity = 0.24f;
    float m_BloomRadius = 1.20f;
    int m_TonemapMode = 0;
    float m_Gamma = 1.0f;
    bool m_FXAAEnabled = true;
    bool m_VignetteEnabled = false;
    float m_VignetteIntensity = 0.12f;
    bool m_ColorGradingEnabled = false;
    GLuint m_ColorGradingLUT = 0;
    float m_WorldStrength = 0.70f;
    float m_NightBrightness = 0.72f;

    int m_Width = 0;
    int m_Height = 0;
    GLint m_PreviousDrawFBO = 0;
    GLint m_PreviousReadFBO = 0;
    GLint m_PreviousViewport[4] = { 0, 0, 0, 0 };

    bool CreateFramebuffers();
    bool CreateShaders();
    bool CreateQuad();
    void DestroyFramebuffers();
    void DestroyShaders();
    void ReloadSettings(bool force = false);
    void ConfigureFullscreenState();

    void BrightPass(ModernRenderBackend* backend);
    void BlurBloom(ModernRenderBackend* backend);
    void Composite(ModernRenderBackend* backend);
    void ApplyFXAA(ModernRenderBackend* backend);
    void CopyToScreen(ModernRenderBackend* backend);
};

extern PostProcessPipeline g_PostProcessPipeline;
