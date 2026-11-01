#pragma once

#include <vector>
#include <array>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "ModernFrustum.h"

class TerrainRenderer;
class ModelRenderer;

class ShadowRenderer
{
public:
    struct CascadeConfig
    {
        float splitDepth;      // Far plane of this cascade
        float splitLambda;     // Lambda for PSSM (0 = uniform, 1 = logarithmic)
        int resolution;        // Shadow map resolution
        int pcfSamples;        // PCF kernel size (0 = no PCF, 3 = 3x3, 5 = 5x5)
        float bias;            // Depth bias
        float normalBias;      // Normal-based bias
    };

    struct CascadeData
    {
        glm::mat4 viewProjMatrix;
        glm::mat4 viewMatrix;
        glm::mat4 projMatrix;
        glm::vec3 lightDir;
        float splitDepth;
        int resolution;
        int pcfSamples;
        float bias;
        float normalBias;
    };

    static constexpr int MAX_CASCADES = 4;
    static constexpr int DEFAULT_RESOLUTION = 2048;
    static constexpr int MAX_RESOLUTION = 4096;

    ShadowRenderer();
    ~ShadowRenderer();

    bool Initialize(ModernFrustum* frustum, TerrainRenderer* terrainRenderer, ModelRenderer* modelRenderer);
    void Shutdown();

    // Main render function
    void RenderShadowMaps(const glm::vec3& lightDir, const glm::vec3& cameraPos, const glm::mat4& cameraView, const glm::mat4& cameraProj);

    // Configuration
    void SetCascadeConfig(int cascadeIndex, const CascadeConfig& config);
    void SetLightDirection(const glm::vec3& dir);
    void SetShadowDistance(float distance);
    void SetAutoSplitEnabled(bool enabled) { m_AutoSplit = enabled; }
    void SetStabilizationEnabled(bool enabled) { m_StabilizationEnabled = enabled; }
    void SetResolution(int resolution);

    // Getters
    const std::array<CascadeData, MAX_CASCADES>& GetCascadeData() const { return m_Cascades; }
    GLuint GetShadowMapTexture() const { return m_ShadowMapArray; }
    int GetActiveCascades() const { return m_ActiveCascades; }

    // Culling
    void SetCullingEnabled(bool enabled) { m_CullingEnabled = enabled; }

private:
    // Light
    glm::vec3 m_LightDir = glm::normalize(glm::vec3(-0.5f, -1.0f, -0.3f));
    float m_ShadowDistance = 300.0f;
    int m_ActiveCascades = 4;

    // Configuration
    std::array<CascadeConfig, MAX_CASCADES> m_CascadeConfigs;
    bool m_AutoSplit = true;
    bool m_StabilizationEnabled = true;
    bool m_CullingEnabled = true;

    // Cascade data
    std::array<CascadeData, MAX_CASCADES> m_Cascades;
    std::array<glm::mat4, MAX_CASCADES> m_PrevViewProjMatrices; // For temporal stabilization

    // Resources
    GLuint m_ShadowMapArray = 0;
    GLuint m_ShadowFBO = 0;
    GLuint m_DepthTexture = 0;
    int m_ShadowMapResolution = 2048;

    // Shaders
    GLuint m_ShadowProgram = 0;
    GLuint m_TerrainShadowProgram = 0;
    GLuint m_SkinnedShadowProgram = 0;

    // Dependencies
    ModernFrustum* m_Frustum = nullptr;
    TerrainRenderer* m_TerrainRenderer = nullptr;
    ModelRenderer* m_ModelRenderer = nullptr;

    // Frustum for culling
    std::array<ModernFrustum, 4> m_CascadeFrustums;

    // Methods
    bool CreateShadowResources();
    void DestroyResources();
    bool CreateShaders();
    void DestroyShaders();

    void CalculateCascadeSplits(const glm::vec3& cameraPos, const glm::mat4& cameraView, const glm::mat4& cameraProj);
    void CalculateCascadeMatrices(int cascadeIndex, const glm::vec3& cameraPos, const glm::mat4& cameraView);
    void UpdateCascadeFrustum(int cascadeIndex);

    void RenderCascade(int cascadeIndex);
    void RenderTerrainShadows(int cascadeIndex);
    void RenderModelShadows(int cascadeIndex);

    void BindShadowMapArray();
    void SetShaderUniforms(int cascadeIndex);

    // Stabilization
    void StabilizeCascadeMatrices();

    // PCF
    void ConfigurePCF(int cascadeIndex);

    // Shader creation helper
    GLuint CreateShaderProgram(const char* vertexSource, const char* fragmentSource, std::string& error);
};

extern ShadowRenderer g_ShadowRenderer;