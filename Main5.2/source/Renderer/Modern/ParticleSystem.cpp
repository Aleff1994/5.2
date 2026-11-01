#include "stdafx.h"
#include "ParticleSystem.h"
#include "Renderer/Modern/ModernRenderBackend.h"
#include "Renderer/Modern/GLResources.h"
#include "Renderer/Modern/ShaderProgram.h"
#include "Renderer/Modern/BufferManager.h"
#include "Renderer/Modern/TextureManager.h"
#include <cmath>
#include <algorithm>

static const char* g_ParticleUpdateVS = R"(
#version 330 compatibility
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inVelocity;
layout(location = 2) in vec4 inColor;
layout(location = 3) in vec2 inSize;
layout(location = 4) in float inLife;
layout(location = 5) in float inMaxLife;
layout(location = 6) in float inRotation;
layout(location = 7) in float inRotationSpeed;
layout(location = 8) in uint inTextureIndex;
layout(location = 9) in uint inFlags;

out vec3 outPosition;
out vec3 outVelocity;
out vec4 outColor;
out vec2 outSize;
out float outLife;
out float outMaxLife;
out float outRotation;
out float outRotationSpeed;
out uint outTextureIndex;
out uint outFlags;

uniform float uDeltaTime;
uniform float uTime;
uniform vec3 uWind;
uniform float uGravity;
uniform vec3 uEmitterPosition;
uniform vec3 uEmitterDirection;
uniform vec3 uEmitterSpread;
uniform vec4 uColorStart;
uniform vec4 uColorEnd;
uniform float uSizeStart;
uniform float uSizeEnd;
uniform float uLifeMin;
uniform float uLifeMax;
uniform float uEmissionRate;
uniform uint uEmitterTextureIndex;
uniform uint uEmitterFlags;
uniform int uEmitterID;

void main()
{
	float age = uTime - inLife;
	
	if (age < 0.0 || inLife <= 0.0)
	{
		// Check if we should spawn new particle from emitter
		if (uEmitterID >= 0 && age < uDeltaTime)
		{
			// Spawn new particle
			float rand1 = fract(sin(uTime * 12.9898 + inPosition.x * 78.233) * 43758.5453);
			float rand2 = fract(sin(uTime * 12.9898 + inPosition.y * 78.233) * 43758.5453);
			float rand3 = fract(sin(uTime * 12.9898 + inPosition.z * 78.233) * 43758.5453);
			
			vec3 spread = uEmitterSpread * vec3(rand1 - 0.5, rand2 - 0.5, rand3 - 0.5);
			vec3 velocity = uEmitterDirection + spread;
			
			outPosition = uEmitterPosition;
			outVelocity = velocity;
			outColor = uColorStart;
			outSize = vec2(uSizeStart);
			outLife = uTime;
			outMaxLife = uLifeMin + rand1 * (uLifeMax - uLifeMin);
			outRotation = rand1 * 6.28318;
			outRotationSpeed = (rand2 - 0.5) * 2.0;
			outTextureIndex = uEmitterTextureIndex;
			outFlags = uEmitterFlags;
		}
		else
		{
			// Keep dead particle dead
			outPosition = inPosition;
			outVelocity = inVelocity;
			outColor = inColor;
			outSize = inSize;
			outLife = inLife;
			outMaxLife = inMaxLife;
			outRotation = inRotation;
			outRotationSpeed = inRotationSpeed;
			outTextureIndex = inTextureIndex;
			outFlags = inFlags;
		}
	}
	else
	{
		// Update existing particle
		float lifeRatio = age / inMaxLife;
		
		if (lifeRatio >= 1.0)
		{
			// Particle died
			outLife = -1.0;
			outPosition = inPosition;
			outVelocity = inVelocity;
			outColor = inColor;
			outSize = inSize;
			outMaxLife = inMaxLife;
			outRotation = inRotation;
			outRotationSpeed = inRotationSpeed;
			outTextureIndex = inTextureIndex;
			outFlags = inFlags;
		}
		else
		{
			// Physics update
			vec3 velocity = inVelocity;
			velocity.y += uGravity * uDeltaTime;
			velocity += uWind * uDeltaTime;
			
			vec3 position = inPosition + velocity * uDeltaTime;
			
			// Interpolate properties
			vec4 color = mix(inColor, vec4(0), lifeRatio);  // fade out
			vec2 size = mix(inSize, vec2(0), lifeRatio);
			float rotation = inRotation + inRotationSpeed * uDeltaTime;
			
			outPosition = position;
			outVelocity = velocity;
			outColor = color;
			outSize = size;
			outLife = inLife;
			outMaxLife = inMaxLife;
			outRotation = rotation;
			outRotationSpeed = inRotationSpeed;
			outTextureIndex = inTextureIndex;
			outFlags = inFlags;
		}
	}
}
)";

static const char* g_ParticleUpdateFS = R"(
#version 330 compatibility
// Transform feedback only - no fragment output needed
)";

static const char* g_ParticleRenderVS = R"(
#version 330 compatibility
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inVelocity;
layout(location = 2) in vec4 inColor;
layout(location = 3) in vec2 inSize;
layout(location = 4) in float inLife;
layout(location = 5) in float inMaxLife;
layout(location = 6) in float inRotation;
layout(location = 7) in float inRotationSpeed;
layout(location = 8) in uint inTextureIndex;
layout(location = 9) in uint inFlags;

uniform mat4 uViewProjection;
uniform vec3 uCameraPos;
uniform vec3 uCameraRight;
uniform vec3 uCameraUp;
uniform float uTime;

out vec4 vColor;
out vec2 vTexCoord;
out float vRotation;
out uint vTextureIndex;
out uint vFlags;

void main()
{
	if (inLife <= 0.0 || inLife >= inMaxLife)
	{
		gl_Position = vec4(0, 0, -10000, 1);
		return;
	}

	float lifeRatio = (uTime - inLife) / inMaxLife;
	if (lifeRatio >= 1.0)
	{
		gl_Position = vec4(0, 0, -10000, 1);
		return;
	}

	// Billboard - align to camera
	vec3 pos = inPosition;
	
	// Create quad corners in camera space
	vec3 right = uCameraRight * inSize.x;
	vec3 up = uCameraUp * inSize.y;
	
	// Rotate quad corners
	float cosR = cos(inRotation);
	float sinR = sin(inRotation);
	
	vec3 corners[4] = {
		-right - up,
		 right - up,
		 right + up,
		-right + up
	};
	
	// Apply rotation to corners
	for (int i = 0; i < 4; i++)
	{
		float x = corners[i].x * cosR - corners[i].y * sinR;
		float y = corners[i].x * sinR + corners[i].y * cosR;
		corners[i].x = x;
		corners[i].y = y;
	}
	
	// Select corner based on vertex ID
	int cornerID = gl_VertexID % 4;
	vec3 corner = corners[cornerID];
	
	vec4 worldPos = vec4(pos + corner, 1.0);
	gl_Position = uViewProjection * worldPos;
	
	// Texture coordinates
	vTexCoord = vec2(
		(cornerID == 1 || cornerID == 2) ? 1.0 : 0.0,
		(cornerID >= 2) ? 1.0 : 0.0
	);
	vColor = inColor;
	vRotation = inRotation;
	vTextureIndex = inTextureIndex;
	vFlags = inFlags;
}
)";

static const char* g_ParticleRenderGS = R"(
#version 330 compatibility
layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

in vec4 vColor[];
in vec2 vTexCoord[];
in float vRotation[];
in uint vTextureIndex[];
in uint vFlags[];

out vec4 gColor;
out vec2 gTexCoord;
out uint gTextureIndex;
out uint gFlags;

uniform mat4 uViewProjection;
uniform vec3 uCameraPos;
uniform vec3 uCameraRight;
uniform vec3 uCameraUp;

void main()
{
	if (gl_in[0].gl_Position.z < -9999.0)
		return;

	vec3 pos = gl_in[0].gl_Position.xyz;
	vec4 color = vColor[0];
	vec2 texCoordBase = vec2(0, 0);
	uint texIndex = vTextureIndex[0];
	uint flags = vFlags[0];

	vec3 right = uCameraRight;
	vec3 up = uCameraUp;

	vec3 corners[4] = {
		-right - up,
		 right - up,
		 right + up,
		-right + up
	};

	float rotation = vRotation[0];
	float cosR = cos(rotation);
	float sinR = sin(rotation);

	for (int i = 0; i < 4; i++)
	{
		vec3 c = corners[i];
		float x = c.x * cosR - c.y * sinR;
		float y = c.x * sinR + c.y * cosR;
		vec3 worldPos = pos + vec3(x, y, 0);
		
		gColor = color;
		gTexCoord = vec2((i == 1 || i == 2) ? 1.0 : 0.0, (i >= 2) ? 1.0 : 0.0);
		gTextureIndex = texIndex;
		gFlags = flags;
		
		gl_Position = uViewProjection * vec4(worldPos, 1.0);
		EmitVertex();
	}
	EndPrimitive();
}
)";

static const char* g_ParticleRenderFS = R"(
#version 330 compatibility
in vec4 gColor;
in vec2 gTexCoord;
in uint gTextureIndex;
in uint gFlags;

out vec4 outColor;

uniform sampler2D uTextureArray[32];

void main()
{
	vec4 texColor = texture(uTextureArray[gTextureIndex], gTexCoord);
	outColor = gColor * texColor;
	
	if ((gFlags & 1) != 0)
	{
		// Additive blending
		outColor.a = gColor.a * texColor.a;
	}
	else if ((gFlags & 2) != 0)
	{
		// Subtractive
		outColor.rgb = gColor.rgb * (1.0 - texColor.rgb);
	}
}
)";

GPUParticleSystem::GPUParticleSystem()
	: m_CurrentBuffer(0), m_AliveCount(0), m_ActiveEmitters(0)
	, m_Gravity(-9.8f), m_Time(0.0f)
{
	memset(m_Emitters.data(), 0, sizeof(ParticleEmitter) * PARTICLE_EMITTERS);
}

GPUParticleSystem::~GPUParticleSystem()
{
	Shutdown();
}

bool GPUParticleSystem::Initialize(ModernRenderBackend* backend)
{
	m_Backend = backend;
	if (!backend) return false;

	if (!CreateShaders()) return false;
	if (!CreateBuffers()) return false;

	return true;
}

void GPUParticleSystem::Shutdown()
{
	for (int i = 0; i < 2; ++i)
	{
		m_ParticleBuffer[i].Reset();
		m_ParticleVAO[i].Reset();
	}
	m_EmitterBuffer.Reset();
	m_AliveCount = 0;
	m_ActiveEmitters = 0;
}

bool GPUParticleSystem::CreateShaders()
{
	std::string error;
	// Note: In a real implementation, we'd use the ShaderManager
	// For now, shaders would be created through the backend
	return true;
}

bool GPUParticleSystem::CreateBuffers()
{
	// Particle data buffer (ping-pong for transform feedback)
	for (int i = 0; i < 2; ++i)
	{
		if (!m_ParticleBuffer[i].Create()) return false;
		m_ParticleBuffer[i].Bind();
		m_ParticleBuffer[i].Upload(
			MAX_GPU_PARTICLES * sizeof(ParticleData), 
			nullptr, 
			GL_DYNAMIC_DRAW
		);

		if (!m_ParticleVAO[i].Create()) return false;
		m_ParticleVAO[i].Bind();
		m_ParticleBuffer[i].Bind();
		
		// Position
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ParticleData), (void*)offsetof(ParticleData, position));
		// Velocity
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(ParticleData), (void*)offsetof(ParticleData, velocity));
		// Color
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(ParticleData), (void*)offsetof(ParticleData, color));
		// Size
		glEnableVertexAttribArray(3);
		glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(ParticleData), (void*)offsetof(ParticleData, size));
		// Life
		glEnableVertexAttribArray(4);
		glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(ParticleData), (void*)offsetof(ParticleData, life));
		// MaxLife
		glEnableVertexAttribArray(5);
		glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, sizeof(ParticleData), (void*)offsetof(ParticleData, maxLife));
		// Rotation
		glEnableVertexAttribArray(6);
		glVertexAttribPointer(6, 1, GL_FLOAT, GL_FALSE, sizeof(ParticleData), (void*)offsetof(ParticleData, rotation));
		// RotationSpeed
		glEnableVertexAttribArray(7);
		glVertexAttribPointer(7, 1, GL_FLOAT, GL_FALSE, sizeof(ParticleData), (void*)offsetof(ParticleData, rotationSpeed));
		// TextureIndex
		glEnableVertexAttribArray(8);
		glVertexAttribIPointer(8, 1, GL_UNSIGNED_INT, sizeof(ParticleData), (void*)offsetof(ParticleData, textureIndex));
		// Flags
		glEnableVertexAttribArray(9);
		glVertexAttribIPointer(9, 1, GL_UNSIGNED_INT, sizeof(ParticleData), (void*)offsetof(ParticleData, flags));
		
		glBindVertexArray(0);
	}

	// Emitter buffer
	if (!m_EmitterBuffer.Create()) return false;
	m_EmitterBuffer.Bind();
	m_EmitterBuffer.Upload(
		PARTICLE_EMITTERS * sizeof(ParticleEmitter),
		nullptr,
		GL_DYNAMIC_DRAW
	);

	return true;
}

void GPUParticleSystem::Update(float deltaTime)
{
	m_Time += deltaTime;
	UpdateEmitters(deltaTime);
	SimulateParticles(deltaTime);
}

void GPUParticleSystem::UpdateEmitters(float deltaTime)
{
	for (int i = 0; i < PARTICLE_EMITTERS; ++i)
	{
		ParticleEmitter& emitter = m_Emitters[i];
		if (!emitter.active) continue;

		emitter.emissionTimer += deltaTime;
		float rate = emitter.emissionRate > 0.1f ? emitter.emissionRate : 0.1f;
		float interval = 1.0f / rate;
		
		while (emitter.emissionTimer >= interval)
		{
			emitter.emissionTimer -= interval;
			// Emission handled in shader via transform feedback
		}
	}

	// Upload emitter data to GPU
	m_EmitterBuffer.Bind();
	m_EmitterBuffer.UploadSubData(0, PARTICLE_EMITTERS * sizeof(ParticleEmitter), m_Emitters.data());
}

void GPUParticleSystem::SimulateParticles(float deltaTime)
{
	// Transform feedback simulation would go here
	// For now, simplified - would use transform feedback in full implementation
}

void GPUParticleSystem::Render(ModernRenderBackend* backend, const vec3_t& cameraPos, const float* viewProjMatrix)
{
	if (m_AliveCount == 0) return;
	
	// Render using geometry shader for billboard expansion
	// Would bind particle VAO and draw points
}

int GPUParticleSystem::AddEmitter(const ParticleEmitter& emitter)
{
	for (int i = 0; i < PARTICLE_EMITTERS; ++i)
	{
		if (!m_Emitters[i].active)
		{
			m_Emitters[i] = emitter;
			m_Emitters[i].active = true;
			m_ActiveEmitters++;
			return i;
		}
	}
	return -1;
}

void GPUParticleSystem::RemoveEmitter(int emitterId)
{
	if (emitterId >= 0 && emitterId < PARTICLE_EMITTERS)
	{
		if (m_Emitters[emitterId].active)
		{
			m_Emitters[emitterId].active = false;
			m_ActiveEmitters--;
		}
	}
}

void GPUParticleSystem::UpdateEmitter(int emitterId, const ParticleEmitter& emitter)
{
	if (emitterId >= 0 && emitterId < PARTICLE_EMITTERS)
	{
		bool wasActive = m_Emitters[emitterId].active;
		m_Emitters[emitterId] = emitter;
		if (!wasActive && emitter.active) m_ActiveEmitters++;
		else if (wasActive && !emitter.active) m_ActiveEmitters--;
	}
}

GPUParticleSystem g_GPUParticleSystem;