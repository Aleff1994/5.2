#pragma once
#include "LorenciaWaterMaterial.h"

// Screen-space reflection: only geometry present in the current color/depth
// buffers can be reflected. No second scene traversal or gameplay updates.
namespace LorenciaWaterSSR
{
    struct Vertex { float position[3]; float coverage, localX, localY, puddle; };
    const int Capacity = 4096 * 6;
    struct State
    {
        Vertex vertices[Capacity];
        int count, width, height;
        GLuint program, color, depth;
        bool tried, enabled;
        float model[16], projection[16];
        GLint seconds, camera, viewport, steps, colorSampler, depthSampler, rippleUniform, rippleCountUniform, skyUniform;
        float ripples[8][4];
        int rippleCount;
        State() : count(0), width(0), height(0), program(0), color(0), depth(0),
                  tried(false), enabled(false), rippleCount(0) {}
    };
    inline State& Data() { static State s; return s; }

    inline void Reset(bool enabled)
    {
        State& s = Data();
        s.count = 0;
        s.enabled = enabled;
        if (enabled)
        {
            glGetFloatv(GL_MODELVIEW_MATRIX, s.model);
            glGetFloatv(GL_PROJECTION_MATRIX, s.projection);
        }
    }

    inline void Add(const float positions[4][3], const float coverage[4], bool puddle = false)
    {
        State& s = Data();
        if (!s.enabled || s.count + 6 > Capacity) return;
        const int order[6] = {0, 1, 2, 0, 2, 3};
        for (int i = 0; i < 6; ++i)
        {
            Vertex& v = s.vertices[s.count++];
            memcpy(v.position, positions[order[i]], sizeof(v.position));
            v.coverage = coverage[order[i]];
            v.localX = order[i] == 1 || order[i] == 2 ? 1.f : -1.f;
            v.localY = order[i] >= 2 ? 1.f : -1.f;
            v.puddle = puddle ? 1.f : 0.f;
        }
    }

    inline bool Initialize()
    {
        State& s = Data();
        if (s.tried) return s.program != 0;
        s.tried = true;
        if (!GLEW_VERSION_2_0) return false;
        const char* vertex = R"GLSL(#version 120
uniform float seconds;
varying vec3 position;
varying float coverage;
varying vec3 patch;
void main() {
    vec4 surface=gl_Vertex;
    if(gl_MultiTexCoord0.w<.5){
        surface.z+=2.4*sin(dot(surface.xy,vec2(.012,.007))+seconds*.85)
            +1.8*sin(dot(surface.xy,vec2(-.019,.023))-seconds*1.15);
    }
    position = surface.xyz;
    coverage = gl_MultiTexCoord0.x;
    patch = gl_MultiTexCoord0.yzw;
    gl_Position = gl_ModelViewProjectionMatrix * surface;
}
)GLSL";
        const char* fragment = R"GLSL(#version 120
uniform sampler2D sceneColor;
uniform sampler2D sceneDepth;
uniform vec3 camera;
uniform vec4 viewport;
uniform float seconds;
uniform int maxSteps;
uniform vec4 ripples[8];
uniform int rippleCount;
uniform vec3 skyColor;
varying vec3 position;
varying float coverage;
varying vec3 patch;
float viewDepth(float depth) {
    return gl_ProjectionMatrix[3][2] / (depth * 2.0 - 1.0 + gl_ProjectionMatrix[2][2]);
}
vec2 projectUV(vec3 point) {
    vec4 clip = gl_ProjectionMatrix * vec4(point, 1.0);
    return clip.xy / clip.w * 0.5 + 0.5;
}
void main() {
    float opacity = coverage;
    if (patch.z > 0.5) {
        float edge = length(patch.xy);
        edge += 0.16 * sin(position.x * 0.043 + position.y * 0.027) * sin(position.y * 0.061 - position.x * 0.019);
        opacity *= 1.0 - smoothstep(0.62, 0.92, edge);
    }
    if (opacity < 0.01) discard;
    vec2 uv = (gl_FragCoord.xy - viewport.xy) / viewport.zw;
    float a = dot(position.xy, vec2(0.012, 0.007)) + seconds * 0.85;
    float b = dot(position.xy, vec2(-0.019, 0.023)) - seconds * 1.15;
    vec2 slope = vec2(0.11, 0.06417) * cos(a) + vec2(-0.057, 0.069) * cos(b);
    // Match the material normals so the scene reflection follows the waves.
    if (patch.z < 0.5 && maxSteps > 16) {
        float c = dot(position.xy, vec2(0.057, 0.043)) + seconds * 2.15;
        float d = dot(position.xy, vec2(-0.083, 0.067)) - seconds * 2.7;
        float footprint = max(length(dFdx(position.xy)), length(dFdy(position.xy)));
        float fineFade = 1.0 - smoothstep(5.0, 18.0, footprint);
        slope += (vec2(0.0342, 0.0258) * cos(c)
                + vec2(-0.018, 0.0145) * cos(d)) * fineFade;
    }
    if (patch.z > 0.5) slope *= 0.18;
    for (int r = 0; r < 8; ++r) {
        if (r >= rippleCount) break;
        vec2 delta = position.xy - ripples[r].xy;
        float distance = length(delta);
        float age = ripples[r].z;
        float ring = distance - age * 90.0;
        if (abs(ring) < 22.0 && distance > 0.01) {
            float envelope = (1.0 - abs(ring) / 22.0) * max(0.0, 1.0 - age / 1.35);
            slope += delta / distance * sin(ring * 0.22) * envelope * ripples[r].w;
        }
    }
    vec3 normal = normalize(vec3(-slope, 1.0));
    vec3 view = normalize(camera - position);
    vec3 worldRay = reflect(-view, normal);
    vec3 ray = normalize(mat3(gl_ModelViewMatrix) * worldRay);
    vec3 origin = (gl_ModelViewMatrix * vec4(position, 1.0)).xyz;
    vec3 hitColor = vec3(0.0);
    float confidence = 0.0;
    float lastDistance = 18.0;
    float lastDelta = -1.0;
    // Bounded trace. Lower quality reduces range and samples, not gameplay.
    for (int i = 0; i < 32; ++i) {
        if (i >= maxSteps || worldRay.z <= 0.01) break;
        float distance = 28.0 + float(i) * 24.0 + float(i * i) * 1.25;
        vec3 point = origin + ray * distance;
        if (point.z >= -1.0) break;
        vec2 sampleUV = projectUV(point);
        if (sampleUV.x <= 0.002 || sampleUV.y <= 0.002 || sampleUV.x >= 0.998 || sampleUV.y >= 0.998) break;
        float depth = texture2D(sceneDepth, sampleUV).r;
        float delta = -point.z - viewDepth(depth);
        if (i > 0 && depth < 0.99999 && delta > 0.0 && lastDelta <= 0.0 && worldRay.z * distance > 12.0) {
            float lo = lastDistance, hi = distance;
            for (int j = 0; j < 4; ++j) {
                float mid = (lo + hi) * 0.5;
                vec3 q = origin + ray * mid;
                float d = texture2D(sceneDepth, projectUV(q)).r;
                if (-q.z > viewDepth(d)) hi = mid; else lo = mid;
            }
            vec3 q = origin + ray * hi;
            vec2 hitUV = projectUV(q);
            float thickness = -q.z - viewDepth(texture2D(sceneDepth, hitUV).r);
            if (thickness >= 0.0 && thickness < 35.0) {
                float edge = min(min(hitUV.x, hitUV.y), min(1.0-hitUV.x, 1.0-hitUV.y));
                confidence = smoothstep(0.0, 0.08, edge) * (1.0 - smoothstep(12.0, 35.0, thickness));
                hitColor = texture2D(sceneColor, hitUV).rgb;
                break;
            }
        }
        lastDistance = distance;
        lastDelta = delta;
    }
    vec3 base = texture2D(sceneColor, uv).rgb;
    // A thin water film keeps the existing pavement/soil visible underneath.
    if (patch.z > 0.5) base *= vec3(0.83);
    // Small screen-space distortion, rejected if it would pull foreground
    // geometry into the water. This is not an underwater terrain capture.
    vec2 bentUV = clamp(uv + slope * (patch.z>.5?.009:.018), vec2(0.001), vec2(0.999));
    float bentDepth = viewDepth(texture2D(sceneDepth, bentUV).r);
    if (bentDepth >= -origin.z - 4.0)
        base = mix(base, texture2D(sceneColor, bentUV).rgb, patch.z>.5?.28:.40);
    float facing = clamp(dot(normal, view), 0.0, 1.0);
    // Restore river reflection strength; thin puddles retain subtle Fresnel.
    float reflectance = 0.08 + 0.82 * pow(1.0 - facing, 3.0);
    if (patch.z > 0.5) reflectance = 0.0204 + 0.9796 * pow(1.0 - facing, 5.0);
    vec3 result = mix(base, hitColor, confidence * reflectance);
    if (patch.z > 0.5) result = mix(base, mix(skyColor * 0.65, hitColor, confidence), reflectance);
    gl_FragColor = vec4(result, clamp(opacity, 0.0, 1.0));
}
)GLSL";
        GLuint vs = LorenciaWaterMaterial::Compile(GL_VERTEX_SHADER, vertex);
        GLuint fs = LorenciaWaterMaterial::Compile(GL_FRAGMENT_SHADER, fragment);
        if (!vs || !fs)
        {
            if (vs) glDeleteShader(vs);
            if (fs) glDeleteShader(fs);
            return false;
        }
        s.program = glCreateProgram();
        if (s.program)
        {
            glAttachShader(s.program, vs); glAttachShader(s.program, fs);
            glLinkProgram(s.program);
            GLint ok = 0;
            glGetProgramiv(s.program, GL_LINK_STATUS, &ok);
            if (!ok)
            {
                char message[2048] = {};
                glGetProgramInfoLog(s.program, sizeof(message), NULL, message);
                LorenciaWaterMaterial::Log("SSR link failed", message);
                glDeleteProgram(s.program); s.program = 0;
            }
        }
        glDeleteShader(vs); glDeleteShader(fs);
        if (!s.program) return false;
        s.seconds = glGetUniformLocation(s.program, "seconds");
        s.camera = glGetUniformLocation(s.program, "camera");
        s.viewport = glGetUniformLocation(s.program, "viewport");
        s.steps = glGetUniformLocation(s.program, "maxSteps");
        s.colorSampler = glGetUniformLocation(s.program, "sceneColor");
        s.depthSampler = glGetUniformLocation(s.program, "sceneDepth");
        s.rippleUniform = glGetUniformLocation(s.program, "ripples[0]");
        s.rippleCountUniform = glGetUniformLocation(s.program, "rippleCount");
        s.skyUniform = glGetUniformLocation(s.program, "skyColor");
        glGenTextures(1, &s.color); glGenTextures(1, &s.depth);
        LorenciaWaterMaterial::Log("SSR ready", "visible-scene reflections; off-screen objects unavailable");
        return true;
    }

    inline void Render(float seconds, const float* camera, float fps, const float* sky = NULL)
    {
        State& s = Data();
        const int count = s.count;
        s.count = 0;
        if (!s.enabled || count == 0 || !Initialize()) return;
        GLint previousProgram = 0, samples = 0, viewport[4];
        glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
        glGetIntegerv(GL_SAMPLES, &samples);
        glGetIntegerv(GL_VIEWPORT, viewport);
        // The copy path requires a resolved, ordinary color/depth framebuffer.
        if (previousProgram || samples > 1 || viewport[2] <= 0 || viewport[3] <= 0)
        {
            static bool reportedSkip = false;
            if (!reportedSkip)
            {
                LorenciaWaterMaterial::Log("SSR skipped", "active external shader, multisample framebuffer or invalid viewport");
                reportedSkip = true;
            }
            return;
        }
        GLint unit, binding[2], buffer, matrixMode;
        glGetIntegerv(GL_ACTIVE_TEXTURE, &unit);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &buffer);
        glGetIntegerv(GL_MATRIX_MODE, &matrixMode);
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
        for (int i = 0; i < 2; ++i)
        {
            glActiveTexture(GL_TEXTURE0 + i);
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding[i]);
            glBindTexture(GL_TEXTURE_2D, i == 0 ? s.color : s.depth);
            if (s.width != viewport[2] || s.height != viewport[3])
            {
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, i == 0 ? GL_LINEAR : GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, i == 0 ? GL_LINEAR : GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexImage2D(GL_TEXTURE_2D, 0, i == 0 ? GL_RGBA8 : GL_DEPTH_COMPONENT24,
                    viewport[2], viewport[3], 0, i == 0 ? GL_RGBA : GL_DEPTH_COMPONENT,
                    i == 0 ? GL_UNSIGNED_BYTE : GL_UNSIGNED_INT, NULL);
            }
            glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, viewport[0], viewport[1], viewport[2], viewport[3]);
        }
        s.width = viewport[2]; s.height = viewport[3];
        glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadMatrixf(s.projection);
        glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadMatrixf(s.model);
        glUseProgram(s.program);
        glUniform1i(s.colorSampler, 0); glUniform1i(s.depthSampler, 1);
        glUniform1f(s.seconds, seconds); glUniform3fv(s.camera, 1, camera);
        const float fallbackSky[3] = {0.12f, 0.22f, 0.30f};
        glUniform3fv(s.skyUniform, 1, sky ? sky : fallbackSky);
        glUniform1i(s.rippleCountUniform, s.rippleCount);
        if (s.rippleCount) glUniform4fv(s.rippleUniform, s.rippleCount, &s.ripples[0][0]);
        glUniform4f(s.viewport, (float)viewport[0], (float)viewport[1], (float)viewport[2], (float)viewport[3]);
        glUniform1i(s.steps, fps > 0.f && fps < 58.f ? 16 : 32);
        glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL); glDepthMask(GL_FALSE);
        glDisable(GL_ALPHA_TEST); glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_POLYGON_OFFSET_FILL); glPolygonOffset(-1.f, -1.f);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glClientActiveTexture(GL_TEXTURE0);
        glEnableClientState(GL_VERTEX_ARRAY); glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glDisableClientState(GL_COLOR_ARRAY); glDisableClientState(GL_NORMAL_ARRAY);
        glVertexPointer(3, GL_FLOAT, sizeof(Vertex), s.vertices[0].position);
        glTexCoordPointer(4, GL_FLOAT, sizeof(Vertex), &s.vertices[0].coverage);
        glDrawArrays(GL_TRIANGLES, 0, count);
        glUseProgram(previousProgram);
        glMatrixMode(GL_MODELVIEW); glPopMatrix();
        glMatrixMode(GL_PROJECTION); glPopMatrix();
        glMatrixMode(matrixMode);
        for (int i = 0; i < 2; ++i)
        {
            glActiveTexture(GL_TEXTURE0 + i); glBindTexture(GL_TEXTURE_2D, binding[i]);
        }
        glActiveTexture(unit);
        glPopClientAttrib(); glBindBuffer(GL_ARRAY_BUFFER, buffer);
        glPopAttrib();
        static bool reportedDraw = false;
        if (!reportedDraw)
        {
            LorenciaWaterMaterial::Log("SSR draw submitted", "scene color/depth captured; water overlay submitted (visual validation pending)");
            reportedDraw = true;
        }
    }
}
