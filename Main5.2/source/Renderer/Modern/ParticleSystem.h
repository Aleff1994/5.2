#pragma once

#include "Renderer/Modern/ModernRenderBackend.h"
#include "Renderer/Modern/GLResources.h"
#include "Renderer/Modern/ShaderProgram.h"
#include <vector>
#include <array>

#define MAX_GPU_PARTICLES 65536
#define PARTICLE_EMITTERS 256

struct ParticleData
{
	vec3_t position;
	vec3_t velocity;
	vec4_t color;
	vec2_t size;        // x = startSize, y = endSize
	float life;         // remaining life (0 = dead)
	float maxLife;      // total life
	float rotation;     // current rotation
	float rotationSpeed;
	uint32_t textureIndex;
	uint32_t flags;     // blending mode, billboard type, etc.
	float padding[2];   // alignment
};

struct ParticleEmitter
{
	vec3_t position;
	vec3_t emissionDirection;
	vec3_t emissionSpread;
	vec4_t colorStart;
	vec4_t colorEnd;
	float sizeStart;
	float sizeEnd;
	float lifeMin;
	float lifeMax;
	float emissionRate;    // particles per second
	float emissionTimer;
	uint32_t textureIndex;
	uint32_t flags;
	bool active;
};

class GPUParticleSystem
{
public:
	GPUParticleSystem();
	~GPUParticleSystem();

	bool Initialize(ModernRenderBackend* backend);
	void Shutdown();

	void Update(float deltaTime);
	void Render(ModernRenderBackend* backend, const vec3_t& cameraPos, const float* viewProjMatrix);

	int AddEmitter(const ParticleEmitter& emitter);
	void RemoveEmitter(int emitterId);
	void UpdateEmitter(int emitterId, const ParticleEmitter& emitter);

	void SetWind(const vec3_t& wind) { m_Wind[0] = wind[0]; m_Wind[1] = wind[1]; m_Wind[2] = wind[2]; }
	void SetGravity(float gravity) { m_Gravity = gravity; }

private:
	ModernRenderBackend* m_Backend = nullptr;
	ShaderProgram* m_UpdateShader = nullptr;
	ShaderProgram* m_RenderShader = nullptr;

	GLBuffer m_ParticleBuffer[2];      // ping-pong buffers
	GLBuffer m_EmitterBuffer;
	GLVertexArray m_ParticleVAO[2];
	int m_CurrentBuffer = 0;
	int m_AliveCount = 0;

	std::array<ParticleEmitter, PARTICLE_EMITTERS> m_Emitters;
	int m_ActiveEmitters = 0;

	vec3_t m_Wind = { 0, 0, 0 };
	float m_Gravity = -9.8f;
	float m_Time = 0.0f;

	bool CreateShaders();
	bool CreateBuffers();
	void UpdateEmitters(float deltaTime);
	void SimulateParticles(float deltaTime);
	void RenderParticles(const vec3_t& cameraPos, const float* viewProjMatrix);
};

extern GPUParticleSystem g_GPUParticleSystem;