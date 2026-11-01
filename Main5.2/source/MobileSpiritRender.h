#pragma once

// Dedicated visual identifiers: other spirit-based skills keep their PC renderer.
constexpr int MOBILE_EVIL_SPIRIT_SUBTYPE = 27;
constexpr int MOBILE_EVIL_HEAD_SUBTYPE = 4;
extern bool FogEnable;

class MobileSpiritRenderScope
{
    bool active;
    bool previousFogSetting;
    GLboolean previousFog;
    GLint previousClamp;
    bool clampSupported;
public:
    explicit MobileSpiritRenderScope(bool enabled, GLint* clampSnapshot = nullptr)
        : active(enabled), previousFogSetting(false), previousFog(GL_FALSE),
          previousClamp(GL_TRUE), clampSupported(false)
    {
        if (!active) return;
        previousFogSetting = FogEnable;
        previousFog = glIsEnabled(GL_FOG);
        FogEnable = false; // The mobile shader does not apply legacy fixed-function fog.
        glDisable(GL_FOG);
        clampSupported = (glClampColorARB != nullptr);
        if (clampSupported)
        {
            if (clampSnapshot && *clampSnapshot >= 0) previousClamp = *clampSnapshot;
            else
            {
                glGetIntegerv(GL_CLAMP_VERTEX_COLOR_ARB, &previousClamp);
                if (clampSnapshot) *clampSnapshot = previousClamp;
            }
            // Mobile multiplies texture * floating point color before final clamping.
            glClampColorARB(GL_CLAMP_VERTEX_COLOR_ARB, GL_FALSE);
        }
    }
    ~MobileSpiritRenderScope()
    {
        if (!active) return;
        if (clampSupported) glClampColorARB(GL_CLAMP_VERTEX_COLOR_ARB, previousClamp);
        FogEnable = previousFogSetting;
        if (previousFog) glEnable(GL_FOG); else glDisable(GL_FOG);
    }
    MobileSpiritRenderScope(const MobileSpiritRenderScope&) = delete;
    MobileSpiritRenderScope& operator=(const MobileSpiritRenderScope&) = delete;
};
