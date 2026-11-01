#include "stdafx.h"
#include "ShadowRenderer.h"
#include "ModernFrustum.h"
#include "TerrainRenderer.h"
#include "ModelRenderer.h"
#include "ZzzOpenglUtil.h"
#include <algorithm>
#include <cmath>
#include <glm/gtc/type_ptr.hpp>

static const char* g_ShadowVertexShader = R"(
#version 330 compatibility
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec4 inBoneWeights;
layout(location = 4) in ivec4 inBoneIndices;

uniform mat4 uViewProjMatrix[4];
uniform mat4 uModelMatrix;

layout(std140, binding = 0) uniform BoneBuffer
{
    mat4 uBones[128];
};

out int vCascadeIndex;
out vec4 vShadowCoord[4];

void main()
{
    mat4 boneTransform = uBones[inBoneIndices[0]] * inBoneWeights[0] +
                         uBones[inBoneIndices[1]] * inBoneWeights[1] +
                         uBones[inBoneIndices[2]] * inBoneWeights[2] +
                         uBones[inBoneIndices[3]] * inBoneWeights[3];

    vec4 worldPos = uModelMatrix * boneTransform * vec4(inPosition, 1.0);

    for (int i = 0; i < 4; ++i)
    {
        vShadowCoord[i] = uViewProjMatrix[i] * worldPos;
    }
    vCascadeIndex = 0;

    gl_Position = uViewProjMatrix[0] * worldPos;
}
)";

static const char* g_TerrainShadowVertexShader = R"(
#version 330 compatibility
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

uniform mat4 uViewProjMatrix[4];
uniform mat4 uModelMatrix;

out int vCascadeIndex;
out vec4 vShadowCoord[4];

void main()
{
    vec4 worldPos = uModelMatrix * vec4(inPosition, 1.0);

    for (int i = 0; i < 4; ++i)
    {
        vShadowCoord[i] = uViewProjMatrix[i] * worldPos;
    }
    vCascadeIndex = 0;

    gl_Position = uViewProjMatrix[0] * vec4(inPosition, 1.0);
}
)";

static const char* g_SkinnedShadowVertexShader = R"(
#version 330 compatibility
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec4 inBoneWeights;
layout(location = 4) in ivec4 inBoneIndices;

uniform mat4 uViewProjMatrix[4];
uniform mat4 uModelMatrix;

layout(std140, binding = 0) uniform BoneBuffer
{
    mat4 uBones[128];
};

out int vCascadeIndex;
out vec4 vShadowCoord[4];

void main()
{
    mat4 boneTransform = uBones[inBoneIndices[0]] * inBoneWeights[0] +
                         uBones[inBoneIndices[1]] * inBoneWeights[1] +
                         uBones[inBoneIndices[2]] * inBoneWeights[2] +
                         uBones[inBoneIndices[3]] * inBoneWeights[3];

    vec4 worldPos = uModelMatrix * boneTransform * vec4(inPosition, 1.0);

    for (int i = 0; i < 4; ++i)
    {
        vShadowCoord[i] = uViewProjMatrix[i] * worldPos;
    }
    vCascadeIndex = 0;

    gl_Position = uViewProjMatrix[0] * worldPos;
}
)";

static const char* g_ShadowFragmentShader = R"(
#version 330 compatibility
in int vCascadeIndex;
in vec4 vShadowCoord[4];

out float outDepth;

void main()
{
    // Depth is written automatically via gl_FragDepth or early-z
    // This fragment shader is intentionally minimal for depth-only rendering
}
)";

ShadowRenderer::ShadowRenderer()
    : m_LightDir(glm::normalize(glm::vec3(-0.5f, -1.0f, -0.3f)))
    , m_ShadowDistance(300.0f)
    , m_ActiveCascades(4)
    , m_AutoSplit(true)
    , m_StabilizationEnabled(true)
    , m_CullingEnabled(true)
    , m_ShadowMapArray(0)
    , m_ShadowFBO(0)
    , m_ShadowMapResolution(2048)
    , m_ShadowProgram(0)
    , m_TerrainShadowProgram(0)
    , m_SkinnedShadowProgram(0)
    , m_Frustum(nullptr)
    , m_TerrainRenderer(nullptr)
    , m_ModelRenderer(nullptr)
{
    for (int i = 0; i < MAX_CASCADES; ++i)
    {
        m_CascadeConfigs[i].splitDepth = 0.0f;
        m_CascadeConfigs[i].splitLambda = 0.92f;
        m_CascadeConfigs[i].resolution = 2048;
        m_CascadeConfigs[i].pcfSamples = 5;
        m_CascadeConfigs[i].bias = 0.0005f;
        m_CascadeConfigs[i].normalBias = 0.002f;
        m_Cascades[i].resolution = 2048;
        m_Cascades[i].pcfSamples = 5;
        m_Cascades[i].bias = 0.0005f;
        m_Cascades[i].normalBias = 0.002f;
    }

    m_CascadeConfigs[0].splitDepth = 10.0f;
    m_CascadeConfigs[1].splitDepth = 50.0f;
    m_CascadeConfigs[2].splitDepth = 150.0f;
    m_CascadeConfigs[3].splitDepth = 300.0f;
}

ShadowRenderer::~ShadowRenderer()
{
    Shutdown();
}

bool ShadowRenderer::Initialize(ModernFrustum* frustum, TerrainRenderer* terrainRenderer, ModelRenderer* modelRenderer)
{
    m_Frustum = frustum;
    m_TerrainRenderer = terrainRenderer;
    m_ModelRenderer = modelRenderer;

    if (!CreateShadowResources())
        return false;

    if (!CreateShaders())
        return false;

    for (int i = 0; i < m_ActiveCascades; ++i)
    {
        m_Cascades[i].resolution = m_CascadeConfigs[i].resolution;
        m_Cascades[i].pcfSamples = m_CascadeConfigs[i].pcfSamples;
        m_Cascades[i].bias = m_CascadeConfigs[i].bias;
        m_Cascades[i].normalBias = m_CascadeConfigs[i].normalBias;
    }

    return true;
}

void ShadowRenderer::Shutdown()
{
    DestroyResources();
    DestroyShaders();
}

bool ShadowRenderer::CreateShadowResources()
{
    glGenTextures(1, &m_ShadowMapArray);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_ShadowMapArray);
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_DEPTH_COMPONENT32F, m_ShadowMapResolution, m_ShadowMapResolution, 
MAX_CASCADES);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, borderColor);

    glGenFramebuffers(1, &m_ShadowFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_ShadowFBO);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, m_ShadowMapArray, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    m_ShadowMapResolution = 2048;

    return true;
}

void ShadowRenderer::DestroyResources()
{
    if (m_ShadowMapArray != 0)
    {
        glDeleteTextures(1, &m_ShadowMapArray);
        m_ShadowMapArray = 0;
    }

    if (m_ShadowFBO != 0)
    {
        glDeleteFramebuffers(1, &m_ShadowFBO);
        m_ShadowFBO = 0;
    }
}

bool ShadowRenderer::CreateShaders()
{
    std::string error;

    m_ShadowProgram = CreateShaderProgram(g_ShadowVertexShader, g_ShadowFragmentShader, error);
    if (!m_ShadowProgram)
        return false;

    m_TerrainShadowProgram = CreateShaderProgram(g_TerrainShadowVertexShader, g_ShadowFragmentShader, error);
    if (!m_TerrainShadowProgram)
        return false;

    m_SkinnedShadowProgram = CreateShaderProgram(g_SkinnedShadowVertexShader, g_ShadowFragmentShader, error);
    if (!m_SkinnedShadowProgram)
        return false;

    return true;
}

void ShadowRenderer::DestroyShaders()
{
    if (m_ShadowProgram != 0)
    {
        glDeleteProgram(m_ShadowProgram);
        m_ShadowProgram = 0;
    }
    if (m_TerrainShadowProgram != 0)
    {
        glDeleteProgram(m_TerrainShadowProgram);
        m_TerrainShadowProgram = 0;
    }
    if (m_SkinnedShadowProgram != 0)
    {
        glDeleteProgram(m_SkinnedShadowProgram);
        m_SkinnedShadowProgram = 0;
    }
}

void ShadowRenderer::SetCascadeConfig(int cascadeIndex, const CascadeConfig& config)
{
    if (cascadeIndex >= 0 && cascadeIndex < MAX_CASCADES)
    {
        m_CascadeConfigs[cascadeIndex] = config;
    }
}

void ShadowRenderer::SetLightDirection(const glm::vec3& dir)
{
    m_LightDir = glm::normalize(dir);
}

void ShadowRenderer::SetShadowDistance(float distance)
{
    m_ShadowDistance = std::max(10.0f, distance);
}

void ShadowRenderer::SetResolution(int resolution)
{
    m_ShadowMapResolution = std::clamp(resolution, 512, 4096);
}

void ShadowRenderer::RenderShadowMaps(const glm::vec3& lightDir, const glm::vec3& cameraPos, const glm::mat4& 
cameraView, const glm::mat4& cameraProj)
{
    if (!m_Frustum) return;

    m_LightDir = glm::normalize(lightDir);

    CalculateCascadeSplits(cameraPos, cameraView, cameraProj);

    for (int i = 0; i < m_ActiveCascades; ++i)
    {
        CalculateCascadeMatrices(i, cameraPos, cameraView);
        UpdateCascadeFrustum(i);
    }

    if (m_StabilizationEnabled)
    {
        StabilizeCascadeMatrices();
    }

    glBindFramebuffer(GL_FRAMEBUFFER, m_ShadowFBO);
    glViewport(0, 0, m_ShadowMapResolution, m_ShadowMapResolution);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);

    for (int i = 0; i < m_ActiveCascades; ++i)
    {
        RenderCascade(i);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glCullFace(GL_BACK);
}

void ShadowRenderer::CalculateCascadeSplits(const glm::vec3& cameraPos, const glm::mat4& cameraView, const glm::mat4& 
cameraProj)
{
    if (!m_AutoSplit)
    {
        for (int i = 0; i < m_ActiveCascades; ++i)
        {
            m_Cascades[i].splitDepth = m_CascadeConfigs[i].splitDepth;
        }
        return;
    }

    float nearPlane = 0.1f;
    float farPlane = m_ShadowDistance;

    for (int i = 0; i < m_ActiveCascades; ++i)
    {
        float lambda = m_CascadeConfigs[i].splitLambda;
        float uniformSplit = nearPlane + (farPlane - nearPlane) * (float)(i + 1) / m_ActiveCascades;
        float logSplit = nearPlane * pow(farPlane / nearPlane, (float)(i + 1) / m_ActiveCascades);
        m_Cascades[i].splitDepth = glm::mix(uniformSplit, logSplit, m_CascadeConfigs[i].splitLambda);
    }
}

void ShadowRenderer::CalculateCascadeMatrices(int cascadeIndex, const glm::vec3& cameraPos, const glm::mat4& 
cameraView)
{
    CascadeData& cascade = m_Cascades[cascadeIndex];

    float nearPlane = (cascadeIndex == 0) ? 0.1f : m_Cascades[cascadeIndex - 1].splitDepth;
    float farPlane = m_Cascades[cascadeIndex].splitDepth;

    cascade.splitDepth = (cascadeIndex == m_ActiveCascades - 1) ? m_ShadowDistance : 
m_CascadeConfigs[cascadeIndex].splitDepth;
    cascade.resolution = m_CascadeConfigs[cascadeIndex].resolution;
    cascade.pcfSamples = m_CascadeConfigs[cascadeIndex].pcfSamples;
    cascade.bias = m_CascadeConfigs[cascadeIndex].bias;
    cascade.normalBias = m_CascadeConfigs[cascadeIndex].normalBias;

    float aspect = 1.0f;
    float fov = 90.0f;

    cascade.projMatrix = glm::perspective(glm::radians(fov), aspect, nearPlane, farPlane);

    glm::vec3 lightPos = cameraPos - m_LightDir * 50.0f;
    cascade.viewMatrix = glm::lookAt(lightPos, lightPos + m_LightDir, glm::vec3(0.0f, 1.0f, 0.0f));
    cascade.viewProjMatrix = cascade.projMatrix * cascade.viewMatrix;

    if (m_StabilizationEnabled)
    {
        StabilizeCascadeMatrices();
    }
}

void ShadowRenderer::UpdateCascadeFrustum(int cascadeIndex)
{
    if (m_Frustum)
    {
        m_CascadeFrustums[cascadeIndex].UpdateFromOpenGL();
    }
}

void ShadowRenderer::StabilizeCascadeMatrices()
{
    if (!m_StabilizationEnabled) return;

    for (int i = 0; i < m_ActiveCascades; ++i)
    {
        if (i < (int)m_PrevViewProjMatrices.size())
        {
            m_Cascades[i].viewProjMatrix = m_PrevViewProjMatrices[i];
        }
        m_PrevViewProjMatrices[i] = m_Cascades[i].viewProjMatrix;
    }
}

void ShadowRenderer::ConfigurePCF(int cascadeIndex)
{
    const CascadeData& cascade = m_Cascades[cascadeIndex];
    int samples = cascade.pcfSamples;

    if (cascade.pcfSamples > 0)
    {
        glEnable(GL_TEXTURE_2D_ARRAY);
        glBindTexture(GL_TEXTURE_2D_ARRAY, m_ShadowMapArray);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
}

void ShadowRenderer::RenderCascade(int cascadeIndex)
{
    const CascadeData& cascade = m_Cascades[cascadeIndex];

    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, m_ShadowMapArray, 0, cascadeIndex);
    glClear(GL_DEPTH_BUFFER_BIT);

    glViewport(0, 0, cascade.resolution, cascade.resolution);

    ConfigurePCF(cascadeIndex);

    if (m_TerrainRenderer)
    {
        RenderTerrainShadows(cascadeIndex);
    }

    if (m_ModelRenderer)
    {
        RenderModelShadows(cascadeIndex);
    }
}

void ShadowRenderer::RenderTerrainShadows(int cascadeIndex)
{
    if (m_TerrainRenderer && m_TerrainShadowProgram)
    {
        GLuint program = m_TerrainShadowProgram;
        glUseProgram(program);

        const CascadeData& cascade = m_Cascades[cascadeIndex];
        glUniformMatrix4fv(glGetUniformLocation(program, "uViewProjMatrix"), 1, GL_FALSE, 
&cascade.viewProjMatrix[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(program, "uModelMatrix"), 1, GL_FALSE, 
glm::value_ptr(glm::mat4(1.0f)));

        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, m_ShadowMapArray, 0, cascadeIndex);
        glClear(GL_DEPTH_BUFFER_BIT);

        // Terrain rendering would call m_TerrainRenderer->RenderShadows(cascadeIndex);
        // This is a placeholder - actual implementation depends on TerrainRenderer interface
    }
}

void ShadowRenderer::RenderModelShadows(int cascadeIndex)
{
    if (m_ModelRenderer && m_SkinnedShadowProgram)
    {
        GLuint program = m_SkinnedShadowProgram;
        glUseProgram(program);

        const CascadeData& cascade = m_Cascades[cascadeIndex];
        glUniformMatrix4fv(glGetUniformLocation(program, "uViewProjMatrix"), 1, GL_FALSE, 
&cascade.viewProjMatrix[0][0]);

        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, m_ShadowMapArray, 0, cascadeIndex);
        glClear(GL_DEPTH_BUFFER_BIT);

        // Model rendering would call m_ModelRenderer->RenderShadows(cascadeIndex);
    }
}

GLuint ShadowRenderer::CreateShaderProgram(const char* vertexSource, const char* fragmentSource, std::string& error)
{
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vertexSource, nullptr);
    glCompileShader(vs);

    GLint success;
    glGetShaderiv(vs, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char infoLog[512];
        glGetShaderInfoLog(vs, 512, nullptr, infoLog);
        error = "Vertex shader compile error: " + std::string(infoLog);
        glDeleteShader(vs);
        return 0;
    }

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fragmentSource, nullptr);
    glCompileShader(fs);

    glGetShaderiv(fs, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char infoLog[512];
        glGetShaderInfoLog(fs, 512, nullptr, infoLog);
        error = "Fragment shader compile error: " + std::string(infoLog);
        glDeleteShader(vs);
        glDeleteShader(fs);
        return 0;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success)
    {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        error = "Program link error: " + std::string(infoLog);
        glDeleteShader(vs);
        glDeleteShader(fs);
        glDeleteProgram(program);
        return 0;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    return program;
}

ShadowRenderer g_ShadowRenderer;