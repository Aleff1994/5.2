#pragma once

// Compatibility-profile water material. Uses existing terrain geometry/batches.
// Environment reflection is an analytic sky, NOT a mirrored scene capture.
namespace LorenciaWaterMaterial
{
    struct Program
    {
        GLuint id;
        GLint time, eye, sun, sky, detail, texture, fog;
        bool attempted;
        Program() : id(0), attempted(false) {}
    };

    inline void Log(const char* stage, const char* message)
    {
        FILE* file = fopen("WaterMaterial.log", "a");
        if (!file) return;
        fprintf(file, "%s: %s\n", stage, message);
        fclose(file);
    }

    inline GLuint Compile(GLenum type, const char* source)
    {
        GLuint shader = glCreateShader(type);
        if (!shader) return 0;
        glShaderSource(shader, 1, &source, NULL);
        glCompileShader(shader);
        GLint ok = GL_FALSE;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
        if (ok) return shader;
        char message[2048] = {};
        glGetShaderInfoLog(shader, sizeof(message), NULL, message);
        Log("compile failed; legacy water retained", message);
        glDeleteShader(shader);
        return 0;
    }

    inline Program& GetProgram()
    {
        static Program p;
        if (p.attempted) return p;
        p.attempted = true;
        if (!GLEW_VERSION_2_0)
        {
            Log("unavailable", "OpenGL 2.0 required; legacy water retained");
            return p;
        }
        const char* vertex = R"GLSL(#version 120
uniform float seconds;
varying vec3 worldPosition;
varying vec2 waterUV;
varying vec4 tint;
varying float fogDistance;
void main() {
    vec4 surface=gl_Vertex;
    float a=dot(surface.xy,vec2(.012,.007))+seconds*.85;
    float b=dot(surface.xy,vec2(-.019,.023))-seconds*1.15;
    surface.z+=2.4*sin(a)+1.8*sin(b);
    worldPosition = surface.xyz;
    waterUV = gl_MultiTexCoord0.xy;
    tint = gl_Color;
    fogDistance = abs((gl_ModelViewMatrix * surface).z);
    gl_Position = gl_ModelViewProjectionMatrix * surface;
}
)GLSL";
        const char* fragment = R"GLSL(#version 120
uniform sampler2D waterTexture;
uniform float seconds;
uniform vec3 eyePosition;
uniform vec3 sunDirection;
uniform vec3 skyColor;
uniform float detailLevel;
uniform int fogEnabled;
varying vec3 worldPosition;
varying vec2 waterUV;
varying vec4 tint;
varying float fogDistance;
void main() {
    vec2 p = worldPosition.xy;
    // Analytic slopes of crossing wave trains, continuous across tile edges.
    float a = dot(p, vec2(0.012, 0.007)) + seconds * 0.85;
    float b = dot(p, vec2(-0.019, 0.023)) - seconds * 1.15;
    vec2 slope = vec2(0.11, 0.06417) * cos(a)
               + vec2(-0.057, 0.069) * cos(b);
    if (detailLevel > 0.5) {
        float c = dot(p, vec2(0.057, 0.043)) + seconds * 2.15;
        float d = dot(p, vec2(-0.083, 0.067)) - seconds * 2.7;
        float footprint = max(length(dFdx(p)), length(dFdy(p)));
        float fineFade = 1.0 - smoothstep(5.0, 18.0, footprint);
        slope += (vec2(0.0342, 0.0258) * cos(c)
                + vec2(-0.018, 0.0145) * cos(d)) * fineFade;
    }
    vec3 n = normalize(vec3(-slope, 1.0));
    vec3 view = normalize(eyePosition - worldPosition + vec3(0.0, 0.0, 0.001));
    vec3 reflected = reflect(-view, n);
    float facing = clamp(dot(n, view), 0.0, 1.0);
    float fresnel = 0.02 + 0.98 * pow(1.0 - facing, 5.0);
    vec3 horizon = skyColor * 0.85 + vec3(0.10, 0.12, 0.13);
    vec3 sky = mix(horizon, skyColor, clamp(reflected.z, 0.0, 1.0));
    vec3 sun = normalize(sunDirection + vec3(0.0, 0.0, 0.001));
    // Filter the highlight footprint to limit sparkling at distant pixels.
    float sunDot = max(dot(reflected, sun), 0.0);
    float exponent = mix(140.0, 48.0, clamp(fwidth(sunDot) * 160.0, 0.0, 1.0));
    float highlight = pow(sunDot, exponent) * (exponent / 140.0);
    vec4 original = texture2D(waterTexture, waterUV + slope * 0.025);
    // Analytic surface tone; the old photograph RGB does not shape the material.
    float surfaceDetail=.38+.16*sin(a*.43+b*.67+seconds*.18)+.10*length(slope);
    vec3 body = mix(vec3(0.018, 0.095, 0.12), vec3(0.045, 0.22, 0.25), surfaceDetail);
    float light = clamp(dot(tint.rgb, vec3(0.299, 0.587, 0.114)), 0.12, 1.35);
    vec3 color = mix(body * light * 1.10, sky, fresnel);
    color += vec3(1.0, 0.86, 0.62) * highlight * clamp(sun.z * 3.0, 0.0, 1.0);
    if (fogEnabled != 0) {
        float fogFactor;
        if (gl_Fog.scale > 0.0)
            fogFactor = clamp((gl_Fog.end - fogDistance) * gl_Fog.scale, 0.0, 1.0);
        else fogFactor = 1.0;
        color = mix(gl_Fog.color.rgb, color, fogFactor);
    }
    gl_FragColor = vec4(color, original.a * tint.a);
}
)GLSL";
        GLuint vs = Compile(GL_VERTEX_SHADER, vertex);
        GLuint fs = Compile(GL_FRAGMENT_SHADER, fragment);
        if (!vs || !fs)
        {
            if (vs) glDeleteShader(vs);
            if (fs) glDeleteShader(fs);
            return p;
        }
        p.id = glCreateProgram();
        if (p.id)
        {
            glAttachShader(p.id, vs);
            glAttachShader(p.id, fs);
            glLinkProgram(p.id);
            GLint ok = GL_FALSE;
            glGetProgramiv(p.id, GL_LINK_STATUS, &ok);
            if (!ok)
            {
                char message[2048] = {};
                glGetProgramInfoLog(p.id, sizeof(message), NULL, message);
                Log("link failed; legacy water retained", message);
                glDeleteProgram(p.id);
                p.id = 0;
            }
        }
        glDeleteShader(vs);
        glDeleteShader(fs);
        if (!p.id) return p;
        p.time = glGetUniformLocation(p.id, "seconds");
        p.eye = glGetUniformLocation(p.id, "eyePosition");
        p.sun = glGetUniformLocation(p.id, "sunDirection");
        p.sky = glGetUniformLocation(p.id, "skyColor");
        p.detail = glGetUniformLocation(p.id, "detailLevel");
        p.texture = glGetUniformLocation(p.id, "waterTexture");
        p.fog = glGetUniformLocation(p.id, "fogEnabled");
        Log("ready", "Lorencia per-pixel water; analytic sky reflection; no scene reflection");
        return p;
    }

    class Scope
    {
        GLint previous;
        bool active;
    public:
        Scope(bool enabled, float seconds, const float* eye, const float* sun,
              const float* sky, float fps) : previous(0), active(false)
        {
            if (!enabled) return;
            Program& p = GetProgram();
            if (!p.id) return;
            glGetIntegerv(GL_CURRENT_PROGRAM, &previous);
            // Do not replace another material owned by an active renderer.
            if (previous != 0) return;
            GLint unit = GL_TEXTURE0;
            glGetIntegerv(GL_ACTIVE_TEXTURE, &unit);
            glUseProgram(p.id);
            glUniform1f(p.time, seconds);
            glUniform3fv(p.eye, 1, eye);
            glUniform3fv(p.sun, 1, sun);
            glUniform3fv(p.sky, 1, sky);
            glUniform1f(p.detail, fps <= 0.f || fps >= 58.f ? 1.f : 0.f);
            glUniform1i(p.texture, unit - GL_TEXTURE0);
            glUniform1i(p.fog, glIsEnabled(GL_FOG) ? 1 : 0);
            active = true;
        }
        ~Scope() { if (active) glUseProgram(previous); }
    private:
        Scope(const Scope&);
        Scope& operator=(const Scope&);
    };
}
