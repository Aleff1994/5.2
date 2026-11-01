#include "stdafx.h"
#include "ModernRenderBackend.h"
#include "BufferManager.h"
#include "ShaderManager.h"
#include "TextureManager.h"
#include "ShaderProgram.h"
#include "GLResources.h"
#include "ZzzOpenglUtil.h"
#include "ShadowRenderer.h"
#include "ModernFrustum.h"
#include "TerrainRenderer.h"
#include "ModelRenderer.h"
#include "RenderStats.h"

static const char* g_SkinnedVertexShader = R"(
#version 330 compatibility
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec4 inBoneWeights;
layout(location = 4) in ivec4 inBoneIndices;

uniform mat4 uModelViewProjection;
uniform mat4 uModelView;
uniform mat3 uNormalMatrix;
uniform mat4 uShadowVP[4];

layout(std140) uniform BoneBuffer
{
	mat4 uBones[128];
};

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vTexCoord;
out vec4 vShadowCoord[4];

void main()
{
	mat4 boneTransform = uBones[inBoneIndices[0]] * inBoneWeights[0] +
						 uBones[inBoneIndices[1]] * inBoneWeights[1] +
						 uBones[inBoneIndices[2]] * inBoneWeights[2] +
						 uBones[inBoneIndices[3]] * inBoneWeights[3];

	vec4 worldPos = uModelView * boneTransform * vec4(inPosition, 1.0);
	vWorldPos = worldPos.xyz;
	vNormal = normalize(uNormalMatrix * (boneTransform * vec4(inNormal, 0.0)).xyz);
	vTexCoord = inTexCoord;

	for (int i = 0; i < 4; ++i)
	{
		vShadowCoord[i] = uShadowVP[i] * vec4(vWorldPos, 1.0);
	}

	gl_Position = uModelViewProjection * worldPos;
}
)";

static const char* g_SkinnedFragmentShader = R"(
#version 330 compatibility
uniform sampler2D uDiffuseMap;
uniform sampler2DArrayShadow uShadowMap;
uniform vec4 uDiffuseColor;
uniform bool uHasTexture;
uniform vec3 uLightDir;
uniform vec3 uLightColor;
uniform vec3 uAmbientColor;
uniform mat4 uShadowVP[4];
uniform float uShadowBias[4];
uniform float uSplitDepth[4];
uniform int uActiveCascades;

in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vTexCoord;
in vec4 vShadowCoord[4];

out vec4 outColor;

float CalcShadowFactor(vec4 shadowCoord)
{
	if (shadowCoord.w == 0.0) return 1.0;
	vec3 projCoord = shadowCoord.xyz / shadowCoord.w;
	projCoord.z += uShadowBias[0];
	return texture(uShadowMap, vec4(projCoord, 0));
}

float GetShadow(vec3 worldPos)
{
	float shadow = 1.0;
	for (int i = 0; i < uActiveCascades; ++i)
	{
		if (length(worldPos) < uSplitDepth[i])
		{
			vec4 shadowCoord = uShadowVP[i] * vec4(worldPos, 1.0);
			shadow = CalcShadowFactor(shadowCoord);
			break;
		}
	}
	return shadow;
}

void main()
{
	vec3 normal = normalize(vNormal);
	vec3 lightDir = normalize(uLightDir);
	float diff = max(dot(normal, lightDir), 0.0);
	vec3 diffuse = uLightColor * diff;
	vec3 ambient = uAmbientColor;

	vec3 color = uDiffuseColor.rgb * (diffuse + ambient);

	if (uHasTexture)
	{
		color *= texture(uDiffuseMap, vTexCoord).rgb;
	}

	// Shadow
	float shadow = GetShadow(vWorldPos);
	color *= shadow;

	outColor = vec4(color, uDiffuseColor.a);
}
)";

static const char* g_TerrainVertexShader = R"(
#version 330 compatibility
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec4 inColor;

uniform mat4 uModelViewProjection;
uniform mat4 uModelView;
uniform mat3 uNormalMatrix;
uniform mat4 uShadowVP[4];
uniform float uWaterMove;
uniform bool uIsWater;

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vTexCoord;
out vec4 vColor;
out vec4 vShadowCoord[4];

void main()
{
	vec4 worldPos = uModelView * vec4(inPosition, 1.0);
	vWorldPos = worldPos.xyz;
	vNormal = normalize(uNormalMatrix * inNormal);
	vTexCoord = inTexCoord;
	vColor = inColor;
	
	if (uIsWater)
	{
		vTexCoord.y += uWaterMove;
	}
	
	for (int i = 0; i < 4; ++i)
	{
		vShadowCoord[i] = uShadowVP[i] * worldPos;
	}
	
	gl_Position = uModelViewProjection * vec4(inPosition, 1.0);
}
)";

static const char* g_TerrainFragmentShader = R"(
#version 330 compatibility
uniform sampler2D uDiffuseMap;
uniform sampler2D uLayer2Map;
uniform sampler2DArrayShadow uShadowMap;
uniform bool uHasLayer2;
uniform float uLayer2Alpha;
uniform bool uIsWater;
uniform float uWaterMove;
uniform vec3 uLightDir;
uniform vec3 uLightColor;
uniform vec3 uAmbientColor;
uniform vec3 uFogColor;
uniform float uFogStart;
uniform float uFogEnd;
uniform mat4 uShadowVP[4];
uniform float uShadowBias[4];
uniform float uSplitDepth[4];
uniform int uActiveCascades;

in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vTexCoord;
in vec4 vColor;
in vec4 vShadowCoord[4];

out vec4 outColor;

float CalcShadowFactor(vec4 shadowCoord)
{
	if (shadowCoord.w == 0.0) return 1.0;
	vec3 projCoord = shadowCoord.xyz / shadowCoord.w;
	projCoord.z += uShadowBias[0];
	return texture(uShadowMap, vec4(projCoord, 0));
}

float GetShadow(vec3 worldPos)
{
	float shadow = 1.0;
	for (int i = 0; i < uActiveCascades; ++i)
	{
		if (length(worldPos) < uSplitDepth[i])
		{
			vec4 shadowCoord = uShadowVP[i] * vec4(worldPos, 1.0);
			shadow = CalcShadowFactor(shadowCoord);
			break;
		}
	}
	return shadow;
}

void main()
{
	vec3 normal = normalize(vNormal);
	vec3 lightDir = normalize(uLightDir);
	float diff = max(dot(normal, lightDir), 0.0);
	vec3 diffuse = uLightColor * diff;
	vec3 ambient = uAmbientColor;
	
	vec3 baseColor = vColor.rgb * (diffuse + ambient);
	
	vec4 texColor = texture(uDiffuseMap, vTexCoord);
	
	if (uHasLayer2)
	{
		vec4 layer2Color = texture(uLayer2Map, vTexCoord);
		baseColor = mix(baseColor, layer2Color.rgb * (diffuse + ambient), uLayer2Alpha);
	}
	
	baseColor *= texColor.rgb;
	
	float fogFactor = 1.0;
	if (uFogEnd > uFogStart)
	{
		float fogDist = length(vWorldPos);
		fogFactor = clamp((uFogEnd - fogDist) / (uFogEnd - uFogStart), 0.0, 1.0);
		baseColor = mix(uFogColor, baseColor, fogFactor);
	}
	
	// Shadow
	float shadow = GetShadow(vWorldPos);
	baseColor *= shadow;
	
	if (uIsWater)
	{
		// Duas famílias de ondas em direções diferentes quebram o aspecto de
		// textura rolando. As derivadas analíticas formam uma normal dinâmica
		// sem normal-map adicional e sem nova leitura de textura.
		float t = uWaterMove * 6.2831853;
		vec2 waterUV = vTexCoord * 38.0;
		float phaseA = waterUV.x * 1.17 + waterUV.y * 0.63 + t * 1.35;
		float phaseB = waterUV.x * -0.54 + waterUV.y * 1.31 - t * 0.91;
		float waveA = sin(phaseA);
		float waveB = sin(phaseB);
		float microWave = sin((waterUV.x + waterUV.y) * 2.45 + t * 1.82);
		vec2 slope = vec2(
			cos(phaseA) * 1.17 - cos(phaseB) * 0.54,
			cos(phaseA) * 0.63 + cos(phaseB) * 1.31) * 0.085;
		slope += vec2(cos((waterUV.x + waterUV.y) * 2.45 + t * 1.82)) * 0.018;
		normal = normalize(normal + vec3(slope.x, slope.y, 0.0));
		float ripple = waveA * 0.50 + waveB * 0.35 + microWave * 0.15;
		vec3 viewDir = normalize(-vWorldPos);
		float fresnel = pow(1.0 - clamp(abs(dot(normal, viewDir)), 0.0, 1.0), 4.0);
		vec3 halfDir = normalize(lightDir + viewDir);
		float specular = pow(max(dot(normal, halfDir), 0.0), 72.0);
		float crest = smoothstep(0.68, 1.0, ripple);
		vec3 deepWater = vec3(0.018, 0.115, 0.205);
		vec3 shallowWater = vec3(0.055, 0.315, 0.425);
		vec3 waterColor = mix(deepWater, shallowWater, 0.46 + ripple * 0.12);
		baseColor = mix(baseColor, waterColor, 0.36 + fresnel * 0.34);
		baseColor += vec3(0.24, 0.44, 0.52) * crest * 0.16;
		baseColor += vec3(0.72, 0.86, 0.92) * specular * (0.22 + fresnel * 0.55);
	}
	
	outColor = vec4(baseColor, texColor.a * vColor.a);
}
)";

static const char* g_StaticVertexShader = R"(
#version 330 compatibility
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

uniform mat4 uModelViewProjection;
uniform mat4 uModelView;
uniform mat3 uNormalMatrix;
uniform mat4 uShadowVP[4];

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vTexCoord;
out vec4 vShadowCoord[4];

void main()
{
	vec4 worldPos = uModelView * vec4(inPosition, 1.0);
	vWorldPos = worldPos.xyz;
	vNormal = normalize(uNormalMatrix * inNormal);
	vTexCoord = inTexCoord;
	
	for (int i = 0; i < 4; ++i)
	{
		vShadowCoord[i] = uShadowVP[i] * worldPos;
	}
	
	gl_Position = uModelViewProjection * vec4(inPosition, 1.0);
}
)";

static const char* g_StaticFragmentShader = R"(
#version 330 compatibility
uniform sampler2D uDiffuseMap;
uniform sampler2DArrayShadow uShadowMap;
uniform vec4 uDiffuseColor;
uniform bool uHasTexture;
uniform vec3 uLightDir;
uniform vec3 uLightColor;
uniform vec3 uAmbientColor;
uniform mat4 uShadowVP[4];
uniform float uShadowBias[4];
uniform float uSplitDepth[4];
uniform int uActiveCascades;

in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vTexCoord;
in vec4 vShadowCoord[4];

out vec4 outColor;

float CalcShadowFactor(vec4 shadowCoord)
{
	if (shadowCoord.w == 0.0) return 1.0;
	vec3 projCoord = shadowCoord.xyz / shadowCoord.w;
	projCoord.z += uShadowBias[0];
	return texture(uShadowMap, vec4(projCoord, 0));
}

float GetShadow(vec3 worldPos)
{
	float shadow = 1.0;
	for (int i = 0; i < uActiveCascades; ++i)
	{
		if (length(worldPos) < uSplitDepth[i])
		{
			vec4 shadowCoord = uShadowVP[i] * vec4(worldPos, 1.0);
			shadow = CalcShadowFactor(shadowCoord);
			break;
		}
	}
	return shadow;
}

void main()
{
	vec3 normal = normalize(vNormal);
	vec3 lightDir = normalize(uLightDir);
	float diff = max(dot(normal, lightDir), 0.0);
	vec3 diffuse = uLightColor * diff;
	vec3 ambient = uAmbientColor;

	vec3 color = uDiffuseColor.rgb * (diffuse + ambient);

	if (uHasTexture)
	{
		color *= texture(uDiffuseMap, vTexCoord).rgb;
	}

	// Shadow
	float shadow = GetShadow(vWorldPos);
	color *= shadow;

	outColor = vec4(color, uDiffuseColor.a);
}
)";

bool ModernRenderBackend::Initialize()
{
	m_InitializationError.clear();
	ShaderManager& shaders = m_ShaderManager;
	BufferManager& buffers = m_BufferManager;
	TextureManager& textures = m_TextureManager;

	std::string error;

	m_Shaders[SHADER_SKINNED_MESH] = shaders.Create("skinned_mesh", g_SkinnedVertexShader, g_SkinnedFragmentShader, error);
	if (!m_Shaders[SHADER_SKINNED_MESH])
	{
		m_InitializationError = "skinned_mesh shader: " + error;
		return false;
	}
	const GLuint boneBlockIndex = glGetUniformBlockIndex(m_Shaders[SHADER_SKINNED_MESH]->GetId(), "BoneBuffer");
	if (boneBlockIndex == GL_INVALID_INDEX)
	{
		m_InitializationError = "skinned_mesh shader: BoneBuffer block not found";
		return false;
	}
	glUniformBlockBinding(m_Shaders[SHADER_SKINNED_MESH]->GetId(), boneBlockIndex, 0);

	m_Shaders[SHADER_STATIC_MESH] = shaders.Create("static_mesh", g_StaticVertexShader, g_StaticFragmentShader, error);
	if (!m_Shaders[SHADER_STATIC_MESH])
	{
		m_InitializationError = "static_mesh shader: " + error;
		return false;
	}

	m_Shaders[SHADER_TERRAIN] = shaders.Create("terrain", g_TerrainVertexShader, g_TerrainFragmentShader, error);
	if (!m_Shaders[SHADER_TERRAIN])
	{
		m_InitializationError = "terrain shader: " + error;
		return false;
	}

	m_BoneBuffer = buffers.Create("bone_matrices", GL_UNIFORM_BUFFER);
	if (!m_BoneBuffer)
	{
		m_InitializationError = "bone_matrices uniform buffer creation failed";
		return false;
	}

	m_BoneBuffer->Bind();
	m_BoneBuffer->Upload(MODERN_MAX_BONES * sizeof(float[16]), nullptr, GL_DYNAMIC_DRAW);
	m_BoneBuffer->BindBase(0);;

	// OPENCODE: Initialize profiling stats
	m_RenderStats.Initialize();

	return true;
}

bool ModernRenderBackend::InitializeShadowRenderer(ModernFrustum* frustum, TerrainRenderer* terrainRenderer, ModelRenderer* modelRenderer)
{
	if (m_ShadowRenderer)
		return true; // Already initialized

	m_ShadowRenderer = new ShadowRenderer();
	if (!m_ShadowRenderer->Initialize(frustum, terrainRenderer, modelRenderer))
	{
		delete m_ShadowRenderer;
		m_ShadowRenderer = nullptr;
		return false;
	}
	return true;
}

void ModernRenderBackend::Shutdown()
{
	if (m_ShadowRenderer)
	{
		m_ShadowRenderer->Shutdown();
		delete m_ShadowRenderer;
		m_ShadowRenderer = nullptr;
	}

	// OPENCODE: Shutdown profiling stats
	m_RenderStats.Shutdown();

	m_BufferManager.Clear();
	m_ShaderManager.Clear();
	m_TextureManager.Clear();

	for (int i = 0; i < SHADER_COUNT; ++i)
	{
		m_Shaders[i] = nullptr;
	}
}

bool ModernRenderBackend::ValidateInfrastructure(std::string& error) const
{
	static const char* vertexSource =
		"#version 330 compatibility\n"
		"layout(location = 0) in vec3 inPosition;\n"
		"uniform mat4 uMvp;\n"
		"void main() { gl_Position = uMvp * vec4(inPosition, 1.0); }\n";
	static const char* fragmentSource =
		"#version 330 compatibility\n"
		"out vec4 outColor;\n"
		"void main() { outColor = vec4(1.0); }\n";

	GLint previousProgram = 0;
	GLint previousVertexArray = 0;
	GLint previousArrayBuffer = 0;
	GLint previousActiveTexture = 0;
	GLint previousTexture2D = 0;
	glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
	glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVertexArray);
	glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previousArrayBuffer);
	glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActiveTexture);
	glActiveTexture(GL_TEXTURE0);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture2D);

	bool valid = false;
	{
		ShaderManager shaders;
		BufferManager buffers;
		TextureManager textures;
		GLVertexArray vertexArray;
		ShaderProgram* program = shaders.Create("stage3_validation", vertexSource, fragmentSource, error);
		GLBuffer* vertexBuffer = buffers.Create("stage3_triangle", GL_ARRAY_BUFFER);
		GLTexture2D* texture = textures.Create("stage3_white");
		if (program != NULL && vertexBuffer != NULL && texture != NULL && vertexArray.Create())
		{
			const float vertices[9] = { 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f };
			const unsigned char pixel[4] = { 255, 255, 255, 255 };
			vertexArray.Bind();
			vertexBuffer->Bind();
			vertexBuffer->Upload(sizeof(vertices), vertices, GL_STATIC_DRAW);
			texture->Bind();
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
			program->Bind();
			const GLint firstLocation = program->GetUniformLocation("uMvp");
			const GLint cachedLocation = program->GetUniformLocation("uMvp");
			valid = firstLocation >= 0 && firstLocation == cachedLocation &&
				shaders.GetCount() == 1 && buffers.GetCount() == 1 && textures.GetCount() == 1;
			if (!valid) error = "resource or uniform cache validation failed";
		}
		else if (error.empty())
		{
			error = "failed to create one or more OpenGL validation resources";
		}
	}

	glUseProgram((GLuint)previousProgram);
	glBindVertexArray((GLuint)previousVertexArray);
	glBindBuffer(GL_ARRAY_BUFFER, (GLuint)previousArrayBuffer);
	glBindTexture(GL_TEXTURE_2D, (GLuint)previousTexture2D);
	glActiveTexture((GLenum)previousActiveTexture);
	return valid;
}

ShaderProgram* ModernRenderBackend::GetShader(ShaderType type) const
{
	if (type >= 0 && type < SHADER_COUNT)
		return m_Shaders[type];
	return nullptr;
}

void ModernRenderBackend::UpdateBoneMatrices(const float* matrices, int count)
{
	if (!m_BoneBuffer || count <= 0 || count > MODERN_MAX_BONES)
		return;

	m_BoneBuffer->Bind();
	m_BoneBuffer->UploadSubData(0, count * sizeof(float[16]), matrices);
}

void ModernRenderBackend::BindBoneBuffer() const
{
	if (m_BoneBuffer)
		m_BoneBuffer->BindBase(0);
}

GLBuffer* ModernRenderBackend::CreateBoneBuffer()
{
	GLBuffer* boneBuffer = m_BufferManager.Create("bone_matrices", GL_UNIFORM_BUFFER);
	if (boneBuffer)
	{
		boneBuffer->Bind();
		boneBuffer->Upload(MODERN_MAX_BONES * sizeof(float[16]), nullptr, GL_DYNAMIC_DRAW);
		boneBuffer->BindBase(0);
	}
	return boneBuffer;
}

void ModernRenderBackend::SetShadowRenderer(ShadowRenderer* shadowRenderer)
{
	m_ShadowRenderer = shadowRenderer;
}

ShadowRenderer* ModernRenderBackend::GetShadowRenderer() const
{
	return m_ShadowRenderer;
}

void ModernRenderBackend::BindShadowMaps() const
{
	if (m_ShadowRenderer)
	{
		GLuint shadowMap = m_ShadowRenderer->GetShadowMapTexture();
		if (shadowMap != 0)
		{
			glActiveTexture(GL_TEXTURE7);
			glBindTexture(GL_TEXTURE_2D_ARRAY, shadowMap);
		}
	}
}

void ModernRenderBackend::SetShadowUniforms(ShaderProgram* shader) const
{
	if (!m_ShadowRenderer || !shader)
		return;

	const auto& cascades = m_ShadowRenderer->GetCascadeData();
	int activeCascades = m_ShadowRenderer->GetActiveCascades();

	shader->Bind();

	// Set shadow VP matrices
	for (int i = 0; i < 4; ++i)
	{
		std::string uniformName = "uShadowVP[" + std::to_string(i) + "]";
		GLint loc = shader->GetUniformLocation(uniformName.c_str());
		if (loc >= 0)
		{
			glUniformMatrix4fv(loc, 1, GL_FALSE, &cascades[i].viewProjMatrix[0][0]);
		}
	}

	// Set split depths
	for (int i = 0; i < 4; ++i)
	{
		std::string uniformName = "uSplitDepth[" + std::to_string(i) + "]";
		GLint loc = shader->GetUniformLocation(uniformName.c_str());
		if (loc >= 0)
		{
			glUniform1f(loc, cascades[i].splitDepth);
		}
	}

	// Set shadow biases
	for (int i = 0; i < 4; ++i)
	{
		std::string uniformName = "uShadowBias[" + std::to_string(i) + "]";
		GLint loc = shader->GetUniformLocation(uniformName.c_str());
		if (loc >= 0)
		{
			glUniform1f(loc, cascades[i].bias);
		}
	}

	// Set active cascades
	GLint loc = shader->GetUniformLocation("uActiveCascades");
	if (loc >= 0)
	{
		glUniform1i(loc, activeCascades);
	}

	// Bind shadow map array
	BindShadowMaps();
	GLint shadowLoc = shader->GetUniformLocation("uShadowMap");
	if (shadowLoc >= 0)
	{
		glUniform1i(shadowLoc, 7); // Texture unit 7
	}
}

RenderStats* ModernRenderBackend::GetRenderStats()
{
	return &m_RenderStats;
}
