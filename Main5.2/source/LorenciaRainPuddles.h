#pragma once
#include "LorenciaWaterSSR.h"

// Visual-only pilot. Never changes terrain height, collisions or movement.
namespace LorenciaRainPuddles
{
    struct Patch { float x, y, rx, ry; float vertices[4][3]; };
    struct Ripple { float x, y, power; DWORD born; };
    struct State
    {
        Patch patches[24];
        Ripple ripples[8];
        int count, tileX, tileY, rainSlot, footSlot;
        DWORD tick, placementTick, footTick, rainTick;
        float wetness, footX, footY;
        bool active, footValid;
        State() : count(0), tileX(-1), tileY(-1), rainSlot(0), footSlot(0),
            tick(0), placementTick(0), footTick(0), rainTick(0), wetness(0),
            footX(0), footY(0), active(false), footValid(false) { memset(ripples, 0, sizeof(ripples)); }
    };
    inline State& Data() { static State s; return s; }
    inline unsigned Hash(unsigned n)
    {
        n ^= n >> 16; n *= 0x7feb352dU; n ^= n >> 15; n *= 0x846ca68bU; return n ^ (n >> 16);
    }
    inline bool Ground(float x, float y)
    {
        const int tx = (int)floorf(x / TERRAIN_SCALE), ty = (int)floorf(y / TERRAIN_SCALE);
        if (tx < 1 || ty < 1 || tx >= TERRAIN_SIZE - 1 || ty >= TERRAIN_SIZE - 1) return false;
        const int i = TERRAIN_INDEX_REPEAT(tx, ty);
        return !(TerrainWall[i] & (TW_NOMOVE | TW_NOGROUND | TW_HEIGHT | TW_WATER)) &&
            TerrainMappingLayer1[i] != 5 && !(TerrainMappingLayer2[i] == 5 && TerrainMappingAlpha[i] > 0.1f);
    }
    inline float Size() { return 0.35f + 0.65f * Data().wetness; }
    inline bool Contains(float x, float y)
    {
        State& s = Data();
        if (!s.active || s.wetness < 0.06f) return false;
        const float scale = Size();
        for (int i = 0; i < s.count; ++i)
        {
            const Patch& p = s.patches[i];
            const float dx = (x - p.x) / (p.rx * scale), dy = (y - p.y) / (p.ry * scale);
            // Stay inside the fully visible part of the feathered edge.
            const float edge = sqrtf(dx * dx + dy * dy) +
                0.16f * sinf(x * 0.043f + y * 0.027f) * sinf(y * 0.061f - x * 0.019f);
            if (edge < 0.62f) return true;
        }
        return false;
    }
    inline void AddRipple(float x, float y, bool foot)
    {
        State& s = Data();
        if (!s.active) return;
        const DWORD now = GetTickCount();
        if (!foot && now - s.rainTick < 120) return;
        const int slot = foot ? 6 + (s.footSlot++ % 2) : s.rainSlot++ % 6;
        Ripple& r = s.ripples[slot];
        r.x = x; r.y = y; r.power = foot ? 0.24f : 0.10f; r.born = now;
        if (!foot) s.rainTick = now;
    }
    inline void Candidate(int tx, int ty)
    {
        State& s = Data();
        if (s.count == 24 || tx < 1 || ty < 1 || tx >= TERRAIN_SIZE - 1 || ty >= TERRAIN_SIZE - 1) return;
        const unsigned hash = Hash((unsigned)(tx + ty * TERRAIN_SIZE));
        if (hash % 5 != 0) return;
        Patch p;
        p.x = (tx + 0.5f) * TERRAIN_SCALE; p.y = (ty + 0.5f) * TERRAIN_SCALE;
        p.rx = 38.f + (hash % 27); p.ry = 30.f + ((hash >> 8) % 25);
        if (!Ground(p.x, p.y)) return;
        const float center = RequestTerrainHeight(p.x, p.y);
        for (int i = 0; i < 4; ++i)
        {
            const float x = p.x + (i == 1 || i == 2 ? p.rx : -p.rx);
            const float y = p.y + (i >= 2 ? p.ry : -p.ry);
            if (!Ground(x, y)) return;
            const float z = RequestTerrainHeight(x, y);
            if (fabsf(z - center) > 3.5f) return;
            p.vertices[i][0] = x; p.vertices[i][1] = y; p.vertices[i][2] = z + 1.0f;
        }
        s.patches[s.count++] = p;
    }
    inline void Update(bool enabled, float rain)
    {
        State& s = Data();
        const DWORD now = GetTickCount();
        if (!enabled || Hero == NULL)
        {
            if (s.active) s = State();
            LorenciaWaterSSR::Data().rippleCount = 0;
            return;
        }
        s.active = true;
        const float dt = s.tick == 0 ? 0.f : fminf((now - s.tick) * 0.001f, 0.25f);
        s.tick = now;
        const float rainfall = fminf(1.f, fmaxf(0.f, rain));
        // Sustained full rain fills in ~35 seconds, dry weather drains in ~3 minutes.
        s.wetness = fminf(1.f, fmaxf(0.f, s.wetness + dt * (rainfall > 0.02f ? rainfall / 35.f : -1.f / 180.f)));
        const float hx = Hero->Object.Position[0], hy = Hero->Object.Position[1];
        const int tx = (int)floorf(hx / TERRAIN_SCALE), ty = (int)floorf(hy / TERRAIN_SCALE);
        if (s.wetness >= 0.06f && (s.count == 0 || tx != s.tileX || ty != s.tileY) && now - s.placementTick >= 500)
        {
            s.tileX = tx; s.tileY = ty; s.placementTick = now; s.count = 0;
            for (int ring = 0; ring <= 9 && s.count < 24; ++ring)
                for (int y = -ring; y <= ring && s.count < 24; ++y)
                    for (int x = -ring; x <= ring && s.count < 24; ++x)
                        if (abs(x) == ring || abs(y) == ring) Candidate(tx + x, ty + y);
        }
        if (s.footValid)
        {
            const float dx = hx - s.footX, dy = hy - s.footY;
            const float moved = dx * dx + dy * dy;
            if (moved > 26.f * 26.f && now - s.footTick >= 140)
            {
                s.footX = hx; s.footY = hy; s.footTick = now;
                const bool flying = (!Hero->SafeZone && Hero->Wing.Type != -1) ||
                    Hero->Object.movementType == Movement::Dinorant || Hero->Object.movementType == Movement::Uniria;
                if (moved < 220.f * 220.f && !flying && Contains(hx, hy) &&
                    fabsf(Hero->Object.Position[2] - RequestTerrainHeight(hx, hy)) < 50.f)
                    AddRipple(hx, hy, true);
            }
        }
        else { s.footX = hx; s.footY = hy; s.footValid = true; }
        LorenciaWaterSSR::State& renderer = LorenciaWaterSSR::Data();
        renderer.rippleCount = 0;
        for (int i = 0; i < 8; ++i)
        {
            const Ripple& r = s.ripples[i];
            const float age = (now - r.born) * 0.001f;
            if (r.born == 0 || age >= 1.35f) continue;
            float* out = renderer.ripples[renderer.rippleCount++];
            out[0] = r.x; out[1] = r.y; out[2] = age; out[3] = r.power;
        }
    }
    inline void Queue()
    {
        State& s = Data();
        if (!s.active || s.wetness < 0.06f) return;
        const float scale = Size();
        const float alpha = fminf(0.88f, s.wetness * 1.4f);
        const float coverage[4] = {alpha, alpha, alpha, alpha};
        for (int i = 0; i < s.count; ++i)
        {
            const Patch& p = s.patches[i];
            if (!TestFrustrum2D(p.x / TERRAIN_SCALE, p.y / TERRAIN_SCALE, 0.f)) continue;
            float vertices[4][3];
            for (int j = 0; j < 4; ++j)
            {
                vertices[j][0] = p.x + (p.vertices[j][0] - p.x) * scale;
                vertices[j][1] = p.y + (p.vertices[j][1] - p.y) * scale;
                vertices[j][2] = RequestTerrainHeight(vertices[j][0], vertices[j][1]) + 1.0f;
            }
            LorenciaWaterSSR::Add(vertices, coverage, true);
        }
    }
}
