#include "stdafx.h"
#include "PostProcess.h"
#include "Renderer/Modern/ModernRenderBackend.h"
#include "Renderer/Modern/GLResources.h"
#include "Renderer/Modern/ShaderProgram.h"
#include "ZzzScene.h"
#include <cmath>
#include <string>

static const char* g_FullscreenVS = R"(
#version 330 compatibility
layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inTexCoord;
out vec2 vTexCoord;
void main() { vTexCoord = inTexCoord; gl_Position = vec4(inPosition, 0.0, 1.0); }
)";

static const char* g_BrightPassFS = R"(
#version 330 compatibility
uniform sampler2D uSceneTexture;
uniform float uThreshold;
in vec2 vTexCoord;
out vec4 outColor;
void main()
{
    vec3 c = texture(uSceneTexture, vTexCoord).rgb;
    float luma = dot(c, vec3(0.2126, 0.7152, 0.0722));
    float knee = max(luma - uThreshold, 0.0) / max(luma, 0.0001);
    outColor = vec4(c * knee, 1.0);
}
)";

static const char* g_BlurFS = R"(
#version 330 compatibility
uniform sampler2D uTexture;
uniform vec2 uTexelSize;
uniform int uDirection;
uniform float uRadius;
in vec2 vTexCoord;
out vec4 outColor;
void main()
{
    const float w0 = 0.2270270270;
    const float w1 = 0.1945945946;
    const float w2 = 0.1216216216;
    const float w3 = 0.0540540541;
    const float w4 = 0.0162162162;
    vec2 axis = (uDirection == 0 ? vec2(1.0, 0.0) : vec2(0.0, 1.0)) * uTexelSize * uRadius;
    vec3 c = texture(uTexture, vTexCoord).rgb * w0;
    c += texture(uTexture, vTexCoord + axis * 1.0).rgb * w1;
    c += texture(uTexture, vTexCoord - axis * 1.0).rgb * w1;
    c += texture(uTexture, vTexCoord + axis * 2.0).rgb * w2;
    c += texture(uTexture, vTexCoord - axis * 2.0).rgb * w2;
    c += texture(uTexture, vTexCoord + axis * 3.0).rgb * w3;
    c += texture(uTexture, vTexCoord - axis * 3.0).rgb * w3;
    c += texture(uTexture, vTexCoord + axis * 4.0).rgb * w4;
    c += texture(uTexture, vTexCoord - axis * 4.0).rgb * w4;
    outColor = vec4(c, 1.0);
}
)";

static const char* g_CompositeFS = R"(
#version 330 compatibility
uniform sampler2D uSceneTexture;
uniform sampler2D uBloomTexture;
uniform vec2 uTexelSize;
uniform float uExposure;
uniform float uContrast;
uniform float uSaturation;
uniform float uSharpen;
uniform float uBloomIntensity;
uniform int uTonemapMode;
uniform float uGamma;
uniform bool uVignetteEnabled;
uniform float uVignetteIntensity;
uniform vec3 uWorldTint;
uniform float uWorldBrightness;
uniform float uWorldStrength;
in vec2 vTexCoord;
out vec4 outColor;

vec3 AcesTonemap(vec3 x)
{
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main()
{
    vec3 scene = texture(uSceneTexture, vTexCoord).rgb;

    if (uSharpen > 0.001)
    {
        vec3 n = texture(uSceneTexture, vTexCoord + vec2(0.0, uTexelSize.y)).rgb;
        vec3 s = texture(uSceneTexture, vTexCoord - vec2(0.0, uTexelSize.y)).rgb;
        vec3 e = texture(uSceneTexture, vTexCoord + vec2(uTexelSize.x, 0.0)).rgb;
        vec3 w = texture(uSceneTexture, vTexCoord - vec2(uTexelSize.x, 0.0)).rgb;
        vec3 blur = (n + s + e + w) * 0.25;
        scene += (scene - blur) * uSharpen;
    }

    vec3 litScene = scene * uWorldTint * uWorldBrightness;
    scene = mix(scene, litScene, clamp(uWorldStrength, 0.0, 1.0));

    float luma = dot(scene, vec3(0.2126, 0.7152, 0.0722));
    scene = mix(vec3(luma), scene, uSaturation);
    scene = (scene - vec3(0.5)) * uContrast + vec3(0.5);
    scene *= uExposure;

    vec3 bloom = texture(uBloomTexture, vTexCoord).rgb;
    vec3 color = scene + bloom * uBloomIntensity;

    if (uTonemapMode == 1) color = color / (color + vec3(1.0));
    else if (uTonemapMode == 2) color = AcesTonemap(color);

    if (abs(uGamma - 1.0) > 0.001)
        color = pow(max(color, vec3(0.0)), vec3(1.0 / max(uGamma, 0.1)));

    if (uVignetteEnabled)
    {
        vec2 p = vTexCoord - vec2(0.5);
        float v = smoothstep(0.42, 0.82, length(p));
        color *= 1.0 - v * uVignetteIntensity;
    }

    outColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}
)";

static const char* g_FXAAFS = R"(
#version 330 compatibility
uniform sampler2D uTexture;
uniform vec2 uTexelSize;
in vec2 vTexCoord;
out vec4 outColor;
void main()
{
    vec3 rgbNW = texture(uTexture, vTexCoord + vec2(-1.0,-1.0) * uTexelSize).rgb;
    vec3 rgbNE = texture(uTexture, vTexCoord + vec2( 1.0,-1.0) * uTexelSize).rgb;
    vec3 rgbSW = texture(uTexture, vTexCoord + vec2(-1.0, 1.0) * uTexelSize).rgb;
    vec3 rgbSE = texture(uTexture, vTexCoord + vec2( 1.0, 1.0) * uTexelSize).rgb;
    vec3 rgbM  = texture(uTexture, vTexCoord).rgb;
    vec3 lumaV = vec3(0.299,0.587,0.114);
    float lNW=dot(rgbNW,lumaV), lNE=dot(rgbNE,lumaV), lSW=dot(rgbSW,lumaV), lSE=dot(rgbSE,lumaV), lM=dot(rgbM,lumaV);
    float lMin=min(lM,min(min(lNW,lNE),min(lSW,lSE)));
    float lMax=max(lM,max(max(lNW,lNE),max(lSW,lSE)));
    vec2 dir=vec2(-((lNW+lNE)-(lSW+lSE)), ((lNW+lSW)-(lNE+lSE)));
    float reduce=max((lNW+lNE+lSW+lSE)*0.03125, 1.0/128.0);
    float rcp=1.0/(min(abs(dir.x),abs(dir.y))+reduce);
    dir=clamp(dir*rcp,vec2(-8.0),vec2(8.0))*uTexelSize;
    vec3 a=.5*(texture(uTexture,vTexCoord+dir*(1.0/3.0-.5)).rgb+texture(uTexture,vTexCoord+dir*(2.0/3.0-.5)).rgb);
    vec3 b=a*.5+.25*(texture(uTexture,vTexCoord+dir*-.5).rgb+texture(uTexture,vTexCoord+dir*.5).rgb);
    float lb=dot(b,lumaV);
    outColor=vec4((lb<lMin||lb>lMax)?a:b,1.0);
}
)";

static const char* g_CopyFS = R"(
#version 330 compatibility
uniform sampler2D uTexture;
in vec2 vTexCoord;
out vec4 outColor;
void main() { outColor = texture(uTexture, vTexCoord); }
)";

namespace
{
    float ReadFloatSetting(const char* section, const char* name, float def, float lo, float hi)
    {
        char fallback[32] = {}, text[64] = {};
        sprintf_s(fallback, "%.3f", def);
        GetPrivateProfileStringA(section, name, fallback, text, sizeof(text), ".\\config.ini");
        float value = (float)atof(text);
        if (!std::isfinite(value)) value = def;
        if (value < lo) value = lo;
        if (value > hi) value = hi;
        return value;
    }

    void SetSampler(ShaderProgram* shader, const char* name, int unit)
    {
        if (!shader) return;
        GLint loc = shader->GetUniformLocation(name);
        if (loc >= 0) glUniform1i(loc, unit);
    }
}

PostProcessPipeline::PostProcessPipeline() {}
PostProcessPipeline::~PostProcessPipeline() { Shutdown(); }

bool PostProcessPipeline::Initialize(ModernRenderBackend* backend, int width, int height)
{
    Shutdown();
    m_Backend = backend;
    m_Width = width > 0 ? width : 1;
    m_Height = height > 0 ? height : 1;
    ReloadSettings(true);
    if (!m_Enabled) return false;
    if (!CreateShaders()) return false;
    if (!CreateFramebuffers()) return false;
    if (!CreateQuad()) return false;
    m_Initialized = true;
    return true;
}

void PostProcessPipeline::Shutdown()
{
    m_Capturing = false;
    m_Initialized = false;
    DestroyFramebuffers();
    DestroyShaders();
    if (m_QuadVAO.GetId()) m_QuadVAO.Reset();
    if (m_QuadVBO.GetId()) m_QuadVBO.Reset();
    m_Backend = nullptr;
}

void PostProcessPipeline::ReloadSettings(bool force)
{
    DWORD now = GetTickCount();
    if (!force && m_LastConfigRead != 0 && now - m_LastConfigRead < 1000) return;
    m_LastConfigRead = now;

    m_Enabled = GetPrivateProfileIntA("GraphicsEngine", "Enabled", 1, ".\\config.ini") != 0;
    m_WorldLightingEnabled = GetPrivateProfileIntA("WorldLighting", "Enabled", 1, ".\\config.ini") != 0;
    m_BloomEnabled = GetPrivateProfileIntA("PostProcess", "BloomEnabled", 1, ".\\config.ini") != 0;
    m_FXAAEnabled = GetPrivateProfileIntA("PostProcess", "FXAA", 1, ".\\config.ini") != 0;
    m_VignetteEnabled = GetPrivateProfileIntA("PostProcess", "Vignette", 0, ".\\config.ini") != 0;
    m_TonemapMode = GetPrivateProfileIntA("PostProcess", "Tonemap", 0, ".\\config.ini");
    if (m_TonemapMode < 0) m_TonemapMode = 0;
    if (m_TonemapMode > 2) m_TonemapMode = 2;

    m_Exposure = ReadFloatSetting("PostProcess", "Exposure", 1.03f, 0.50f, 2.00f);
    m_Contrast = ReadFloatSetting("PostProcess", "Contrast", 1.08f, 0.50f, 1.80f);
    m_Saturation = ReadFloatSetting("PostProcess", "Saturation", 1.06f, 0.00f, 2.00f);
    m_Gamma = ReadFloatSetting("PostProcess", "Gamma", 1.00f, 0.50f, 2.50f);
    m_Sharpen = ReadFloatSetting("PostProcess", "Sharpen", 0.15f, 0.00f, 1.00f);
    m_BloomThreshold = ReadFloatSetting("PostProcess", "BloomThreshold", 0.80f, 0.05f, 2.50f);
    m_BloomIntensity = ReadFloatSetting("PostProcess", "BloomIntensity", 0.24f, 0.00f, 2.00f);
    m_BloomRadius = ReadFloatSetting("PostProcess", "BloomRadius", 1.20f, 0.25f, 4.00f);
    m_VignetteIntensity = ReadFloatSetting("PostProcess", "VignetteIntensity", 0.12f, 0.00f, 0.80f);
    m_WorldStrength = ReadFloatSetting("WorldLighting", "Strength", 0.70f, 0.00f, 1.00f);
    m_NightBrightness = ReadFloatSetting("WorldLighting", "NightBrightness", 0.72f, 0.40f, 1.00f);
}

bool PostProcessPipeline::CreateShaders()
{
    std::string error;
    m_BrightPassShader = m_ShaderManager.Create("engine_bright", g_FullscreenVS, g_BrightPassFS, error);
    if (!m_BrightPassShader) return false;
    m_BlurShader = m_ShaderManager.Create("engine_blur", g_FullscreenVS, g_BlurFS, error);
    if (!m_BlurShader) return false;
    m_CompositeShader = m_ShaderManager.Create("engine_composite", g_FullscreenVS, g_CompositeFS, error);
    if (!m_CompositeShader) return false;
    m_FXAAShader = m_ShaderManager.Create("engine_fxaa", g_FullscreenVS, g_FXAAFS, error);
    if (!m_FXAAShader) return false;
    m_CopyShader = m_ShaderManager.Create("engine_copy", g_FullscreenVS, g_CopyFS, error);
    return m_CopyShader != nullptr;
}

void PostProcessPipeline::DestroyShaders()
{
    m_BrightPassShader = m_BlurShader = m_CompositeShader = m_FXAAShader = m_CopyShader = nullptr;
    m_ShaderManager.Clear();
}

bool PostProcessPipeline::CreateFramebuffers()
{
    if (m_Width < 1 || m_Height < 1) return false;

    glGenFramebuffers(1, &m_SceneFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_SceneFBO);
    glGenTextures(1, &m_SceneColorTexture);
    glBindTexture(GL_TEXTURE_2D, m_SceneColorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_Width, m_Height, 0, GL_RGBA, GL_HALF_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_SceneColorTexture, 0);

    glGenTextures(1, &m_SceneDepthTexture);
    glBindTexture(GL_TEXTURE_2D, m_SceneDepthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, m_Width, m_Height, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, m_SceneDepthTexture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) { glBindFramebuffer(GL_FRAMEBUFFER, 0); return false; }

    glGenFramebuffers(1, &m_BrightFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_BrightFBO);
    glGenTextures(1, &m_BrightTexture);
    glBindTexture(GL_TEXTURE_2D, m_BrightTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_Width, m_Height, 0, GL_RGBA, GL_HALF_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_BrightTexture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) { glBindFramebuffer(GL_FRAMEBUFFER, 0); return false; }

    const int bloomW = m_Width > 1 ? m_Width / 2 : 1;
    const int bloomH = m_Height > 1 ? m_Height / 2 : 1;
    for (int i = 0; i < BLOOM_LEVELS; ++i)
    {
        m_BloomTextureWidth[i] = bloomW;
        m_BloomTextureHeight[i] = bloomH;
        glGenFramebuffers(1, &m_BloomFBO[i]);
        glBindFramebuffer(GL_FRAMEBUFFER, m_BloomFBO[i]);
        glGenTextures(1, &m_BloomTextures[i]);
        glBindTexture(GL_TEXTURE_2D, m_BloomTextures[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, bloomW, bloomH, 0, GL_RGBA, GL_HALF_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_BloomTextures[i], 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) { glBindFramebuffer(GL_FRAMEBUFFER, 0); return false; }
    }

    glGenFramebuffers(1, &m_CompositeFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_CompositeFBO);
    glGenTextures(1, &m_CompositeTexture);
    glBindTexture(GL_TEXTURE_2D, m_CompositeTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_Width, m_Height, 0, GL_RGBA, GL_HALF_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_CompositeTexture, 0);
    const bool ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return ok;
}

void PostProcessPipeline::DestroyFramebuffers()
{
    if (m_SceneFBO) glDeleteFramebuffers(1, &m_SceneFBO);
    if (m_SceneColorTexture) glDeleteTextures(1, &m_SceneColorTexture);
    if (m_SceneDepthTexture) glDeleteTextures(1, &m_SceneDepthTexture);
    if (m_BrightFBO) glDeleteFramebuffers(1, &m_BrightFBO);
    if (m_BrightTexture) glDeleteTextures(1, &m_BrightTexture);
    for (int i = 0; i < BLOOM_LEVELS; ++i)
    {
        if (m_BloomFBO[i]) glDeleteFramebuffers(1, &m_BloomFBO[i]);
        if (m_BloomTextures[i]) glDeleteTextures(1, &m_BloomTextures[i]);
        m_BloomFBO[i] = m_BloomTextures[i] = 0;
    }
    if (m_CompositeFBO) glDeleteFramebuffers(1, &m_CompositeFBO);
    if (m_CompositeTexture) glDeleteTextures(1, &m_CompositeTexture);
    m_SceneFBO = m_SceneColorTexture = m_SceneDepthTexture = 0;
    m_BrightFBO = m_BrightTexture = 0;
    m_CompositeFBO = m_CompositeTexture = 0;
}

bool PostProcessPipeline::CreateQuad()
{
    const float v[] = {
        -1.f,-1.f, 0.f,0.f,
         1.f,-1.f, 1.f,0.f,
        -1.f, 1.f, 0.f,1.f,
         1.f, 1.f, 1.f,1.f
    };
    if (!m_QuadVAO.Create()) return false;
    m_QuadVAO.Bind();
    if (!m_QuadVBO.Create()) return false;
    m_QuadVBO.Bind();
    m_QuadVBO.Upload(sizeof(v), v, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return true;
}

void PostProcessPipeline::Resize(int width, int height)
{
    if (!m_Initialized || width < 1 || height < 1) return;
    if (width == m_Width && height == m_Height) return;
    m_Width = width; m_Height = height;
    DestroyFramebuffers();
    if (!CreateFramebuffers())
    {
        m_Enabled = false;
        m_Initialized = false;
    }
}

void PostProcessPipeline::BeginFrame()
{
    if (!m_Initialized) return;
    ReloadSettings();
    if (!m_Enabled) return;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &m_PreviousDrawFBO);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &m_PreviousReadFBO);
    glGetIntegerv(GL_VIEWPORT, m_PreviousViewport);
    glBindFramebuffer(GL_FRAMEBUFFER, m_SceneFBO);
    glViewport(0, 0, m_Width, m_Height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    m_Capturing = true;
}

void PostProcessPipeline::ConfigureFullscreenState()
{
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_FOG);
    glDisable(GL_LIGHTING);
    glDisable(GL_STENCIL_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}

void PostProcessPipeline::EndFrame(ModernRenderBackend* backend)
{
    if (!m_Capturing) return;
    m_Capturing = false;
    ReloadSettings();

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    ConfigureFullscreenState();
    m_QuadVAO.Bind();

    if (m_BloomEnabled && m_BloomIntensity > 0.001f)
    {
        BrightPass(backend);
        BlurBloom(backend);
    }
    Composite(backend);
    if (m_FXAAEnabled) ApplyFXAA(backend); else CopyToScreen(backend);

    glBindVertexArray(0);
    glUseProgram(0);
    glPopAttrib();
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_PreviousDrawFBO);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_PreviousReadFBO);
    glViewport(m_PreviousViewport[0], m_PreviousViewport[1], m_PreviousViewport[2], m_PreviousViewport[3]);
    glActiveTexture(GL_TEXTURE0);
}

void PostProcessPipeline::RenderToScreen(ModernRenderBackend*) {}

void PostProcessPipeline::BrightPass(ModernRenderBackend*)
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_BrightFBO);
    glViewport(0, 0, m_Width, m_Height);
    glClear(GL_COLOR_BUFFER_BIT);
    m_BrightPassShader->Bind();
    SetSampler(m_BrightPassShader, "uSceneTexture", 0);
    GLint loc = m_BrightPassShader->GetUniformLocation("uThreshold");
    if (loc >= 0) glUniform1f(loc, m_BloomThreshold);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_SceneColorTexture);
    m_QuadVAO.Bind();
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void PostProcessPipeline::BlurBloom(ModernRenderBackend*)
{
    m_BlurShader->Bind();
    SetSampler(m_BlurShader, "uTexture", 0);
    GLint dirLoc = m_BlurShader->GetUniformLocation("uDirection");
    GLint sizeLoc = m_BlurShader->GetUniformLocation("uTexelSize");
    GLint radiusLoc = m_BlurShader->GetUniformLocation("uRadius");
    if (radiusLoc >= 0) glUniform1f(radiusLoc, m_BloomRadius);

    glBindFramebuffer(GL_FRAMEBUFFER, m_BloomFBO[0]);
    glViewport(0, 0, m_BloomTextureWidth[0], m_BloomTextureHeight[0]);
    glClear(GL_COLOR_BUFFER_BIT);
    if (dirLoc >= 0) glUniform1i(dirLoc, 0);
    if (sizeLoc >= 0) glUniform2f(sizeLoc, 1.0f / m_Width, 1.0f / m_Height);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_BrightTexture);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glBindFramebuffer(GL_FRAMEBUFFER, m_BloomFBO[1]);
    glViewport(0, 0, m_BloomTextureWidth[1], m_BloomTextureHeight[1]);
    glClear(GL_COLOR_BUFFER_BIT);
    if (dirLoc >= 0) glUniform1i(dirLoc, 1);
    if (sizeLoc >= 0) glUniform2f(sizeLoc, 1.0f / m_BloomTextureWidth[0], 1.0f / m_BloomTextureHeight[0]);
    glBindTexture(GL_TEXTURE_2D, m_BloomTextures[0]);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void PostProcessPipeline::Composite(ModernRenderBackend*)
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_CompositeFBO);
    glViewport(0, 0, m_Width, m_Height);
    glClear(GL_COLOR_BUFFER_BIT);
    m_CompositeShader->Bind();
    SetSampler(m_CompositeShader, "uSceneTexture", 0);
    SetSampler(m_CompositeShader, "uBloomTexture", 1);

    float hour=12.f, rain=0.f, storm=0.f, dust=0.f, snow=0.f, fog=0.f;
    GetServerAtmosphereVisual(hour, rain, storm, dust, snow, fog);
    const float pi = 3.14159265358979323846f;
    float daylight = (cosf((hour - 12.0f) * pi / 12.0f) + 1.0f) * 0.5f;
    float night = 1.0f - daylight;
    float dawn = expf(-((hour - 6.0f) * (hour - 6.0f)) / 7.0f);
    float dusk = expf(-((hour - 18.0f) * (hour - 18.0f)) / 7.0f);
    float warm = fminf(1.0f, dawn + dusk);

    float worldTint[3] = {
        1.0f + warm * 0.08f - night * 0.10f,
        1.0f - warm * 0.02f - night * 0.05f,
        1.0f - warm * 0.10f + night * 0.10f
    };
    const float badWeather = fminf(1.0f, rain * 0.22f + storm * 0.42f + dust * 0.30f + snow * 0.12f + fog * 0.30f);
    worldTint[0] *= 1.0f - rain * 0.04f - storm * 0.08f + dust * 0.07f;
    worldTint[1] *= 1.0f + snow * 0.03f - dust * 0.04f;
    worldTint[2] *= 1.0f + rain * 0.05f + snow * 0.08f - dust * 0.12f;
    float brightness = (m_NightBrightness + (1.0f - m_NightBrightness) * daylight) * (1.0f - badWeather * 0.18f);
    if (!m_WorldLightingEnabled) { worldTint[0]=worldTint[1]=worldTint[2]=1.0f; brightness=1.0f; }

    auto f1=[&](const char* n,float v){ GLint l=m_CompositeShader->GetUniformLocation(n); if(l>=0) glUniform1f(l,v); };
    auto i1=[&](const char* n,int v){ GLint l=m_CompositeShader->GetUniformLocation(n); if(l>=0) glUniform1i(l,v); };
    f1("uExposure", m_Exposure); f1("uContrast", m_Contrast); f1("uSaturation", m_Saturation);
    f1("uSharpen", m_Sharpen); f1("uBloomIntensity", m_BloomEnabled ? m_BloomIntensity : 0.0f);
    i1("uTonemapMode", m_TonemapMode); f1("uGamma", m_Gamma);
    i1("uVignetteEnabled", m_VignetteEnabled ? 1 : 0); f1("uVignetteIntensity", m_VignetteIntensity);
    f1("uWorldBrightness", brightness); f1("uWorldStrength", m_WorldLightingEnabled ? m_WorldStrength : 0.0f);
    GLint tintLoc=m_CompositeShader->GetUniformLocation("uWorldTint"); if(tintLoc>=0) glUniform3f(tintLoc,worldTint[0],worldTint[1],worldTint[2]);
    GLint texelLoc=m_CompositeShader->GetUniformLocation("uTexelSize"); if(texelLoc>=0) glUniform2f(texelLoc,1.0f/m_Width,1.0f/m_Height);

    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, m_SceneColorTexture);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, m_BloomEnabled ? m_BloomTextures[1] : m_BrightTexture);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void PostProcessPipeline::ApplyFXAA(ModernRenderBackend*)
{
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_PreviousDrawFBO);
    glViewport(m_PreviousViewport[0], m_PreviousViewport[1], m_PreviousViewport[2], m_PreviousViewport[3]);
    m_FXAAShader->Bind();
    SetSampler(m_FXAAShader, "uTexture", 0);
    GLint loc=m_FXAAShader->GetUniformLocation("uTexelSize"); if(loc>=0) glUniform2f(loc,1.0f/m_Width,1.0f/m_Height);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, m_CompositeTexture);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void PostProcessPipeline::CopyToScreen(ModernRenderBackend*)
{
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_PreviousDrawFBO);
    glViewport(m_PreviousViewport[0], m_PreviousViewport[1], m_PreviousViewport[2], m_PreviousViewport[3]);
    m_CopyShader->Bind();
    SetSampler(m_CopyShader, "uTexture", 0);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, m_CompositeTexture);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

PostProcessPipeline g_PostProcessPipeline;
