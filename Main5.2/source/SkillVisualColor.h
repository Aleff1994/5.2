#pragma once

// Change existing vertex colors only. No additional geometry, lights,
// particles, texture passes or shaders. The original light is restored
// before update logic can run or use it to create child effects.
namespace SkillVisualColor
{
    inline float Strength()
    {
        static const float strength = []() {
            if (!GetPrivateProfileIntA("Renderer", "SkillColorVivid", 1, ".\\config.ini")) return 0.0f;
            // Vertex-color adjustment only: stronger skill hues without adding
            // particles, geometry, lights or extra blend passes.
            int percent = GetPrivateProfileIntA("Renderer", "SkillColorStrength", 22, ".\\config.ini");
            if (percent < 0) percent = 0;
            if (percent > 30) percent = 30;
            return percent * 0.01f;
        }();
        return strength;
    }

    // -1 means leave the effect untouched; 0 preserves its existing hue.
    inline int EffectPalette(int type, int subtype)
    {
        if (type == MODEL_SKILL_WHEEL2) return 4;
        if (type == MODEL_FIRE && (subtype == 0 || subtype == 5 || subtype == 6)) return 1;
        if (type == MODEL_DARK_SCREAM || type == MODEL_DARK_SCREAM_FIRE) return 1;
        if (type == MODEL_BLIZZARD) return 2;
        if (type == MODEL_DARKLORD_SKILL) return 3;
        if (type == MODEL_SKILL_FURY_STRIKE || type == MODEL_SKILL_JAVELIN ||
            type == MODEL_SWORD_FORCE || type == MODEL_MAGIC1 || type == MODEL_CIRCLE ||
            type == MODEL_CIRCLE_LIGHT || type == MODEL_FISSURE || type == MODEL_FISSURE_LIGHT ||
            type == MODEL_MULTI_SHOT1 || type == MODEL_MULTI_SHOT2 || type == MODEL_MULTI_SHOT3)
            return 0;
        // Subtractive ghosts and dark ground marks deliberately retain their colors.
        return -1;
    }

    inline int ParticlePalette(int type)
    {
        if (type == BITMAP_FIRE || type == BITMAP_FLAME || type == BITMAP_SPARK) return 1;
        if (type == BITMAP_LIGHTNING || type == BITMAP_LIGHTNING + 1) return 3;
        if (type == BITMAP_MAGIC || type == BITMAP_MAGIC + 1 || type == BITMAP_SHINY + 6)
            return 0;
        return -1;
    }

    class ScopedColor
    {
        float* m_Light;
        float m_Original[3];
    public:
        ScopedColor(float* light, int palette) : m_Light(NULL)
        {
            if (palette < 0 || SceneFlag != MAIN_SCENE) return;
            const float strength = Strength();
            if (strength <= 0.0f) return;
            // Preserve negative/subtractive colors and invalid data.
            for (int c = 0; c < 3; ++c)
                if (!(light[c] >= 0.0f && light[c] <= 4.0f)) return;
            float peak = light[0], floor = light[0];
            for (int c = 1; c < 3; ++c)
            {
                if (light[c] > peak) peak = light[c];
                if (light[c] < floor) floor = light[c];
            }
            if (peak < 0.0001f) return; // Faded/black remains black.
            m_Light = light;
            for (int c = 0; c < 3; ++c) m_Original[c] = light[c];
            const bool neutral = peak - floor < peak * 0.08f;
            const float midpoint = (peak + floor) * 0.5f;
            const float limit = peak * (1.0f + strength * 0.25f);
            static const float neutralTint[5][3] = {
                {1.0f, 1.0f, 1.0f}, {1.0f, 0.64f, 0.30f},
                {0.65f, 0.87f, 1.0f}, {0.80f, 0.70f, 1.0f},
                {0.90f, 0.96f, 1.0f}
            };
            for (int c = 0; c < 3; ++c)
            {
                float value = midpoint + (light[c] - midpoint) * (1.0f + strength);
                value *= 1.0f + strength * 0.20f;
                if (neutral && palette > 0)
                    value *= 1.0f + (neutralTint[palette][c] - 1.0f) * (strength / 0.30f);
                if (value < 0.0f) value = 0.0f;
                if (value > limit) value = limit;
                light[c] = value;
            }
        }
        ~ScopedColor()
        {
            if (m_Light)
                for (int c = 0; c < 3; ++c) m_Light[c] = m_Original[c];
        }
        ScopedColor(const ScopedColor&) = delete;
        ScopedColor& operator=(const ScopedColor&) = delete;
    };
}
