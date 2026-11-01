#pragma once
// Animated flame density, not a sampled flame photograph. One additive draw.
namespace SceneFlameMaterial {
inline void Log(const char* text){FILE* f=fopen("SceneFlameMaterial.log","a");if(f){fprintf(f,"%s\n",text);fclose(f);}}
inline GLuint Compile(GLenum kind,const char* text){GLuint s=glCreateShader(kind);if(!s)return 0;glShaderSource(s,1,&text,NULL);glCompileShader(s);GLint ok=0;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);if(ok)return s;char error[2048]={};glGetShaderInfoLog(s,sizeof(error),NULL,error);Log(error);glDeleteShader(s);return 0;}
struct Program{GLuint id;GLint seconds,seed,detail;bool tried;Program():id(0),tried(false){}};
inline Program& Get(){
 static Program p;if(p.tried)return p;p.tried=true;if(!GLEW_VERSION_2_0){Log("OpenGL2 unavailable; animated sprite fallback.");return p;}
 const char* vertex=R"GLSL(#version 120
varying vec2 uv;varying vec4 tint;
void main(){gl_Position=ftransform();uv=gl_MultiTexCoord0.xy;tint=gl_Color;}
)GLSL";
 const char* fragment=R"GLSL(#version 120
uniform float seconds,seed,detailLevel;
varying vec2 uv;varying vec4 tint;
float hash(vec2 p){vec3 q=fract(vec3(p.xyx)*.1031);q+=dot(q,q.yzx+33.33);return fract((q.x+q.y)*q.z);}
float noise(vec2 p){vec2 i=floor(p),f=fract(p);f=f*f*(3.0-2.0*f);return mix(mix(hash(i),hash(i+vec2(1,0)),f.x),mix(hash(i+vec2(0,1)),hash(i+vec2(1,1)),f.x),f.y);}
float tongue(vec2 p,float offset,float height,float width,float t){
 float h=p.y/height;
 float drift=(noise(vec2(h*3.2+seed+offset*7.0,t*.8))-.5)*.35*h;
 float radius=width*pow(max(1.0-h,0.0),.65);
 float edge=abs(p.x-offset-drift);
 float inside=1.0-smoothstep(radius*.48,radius+.025,edge);
 float n=noise(vec2(p.x*8.0+seed,p.y*7.0-t*2.8));
 if(detailLevel>.5)n=n*.7+noise(vec2(p.x*17.0-seed,p.y*14.0-t*4.1))*.3;
 return inside*smoothstep(-.02,.035,p.y)*(1.0-smoothstep(.65,1.03,h+(n-.5)*.30))*(.45+.55*n);
}
void main(){
 vec2 p=vec2((uv.x-.5)*2.0,(1.0-uv.y-.08)/.84);
 float t=seconds*1.5;
 float sway=sin(t*2.2+seed)*.035;
 p.x-=sway*p.y;
 float center=tongue(p,0.0,.94+.07*sin(t*1.8+seed),.44,t);
 float left=tongue(p,-.23,.62+.14*sin(t*2.5+seed+2.0),.23,t+1.7);
 float right=tongue(p,.22,.68+.12*sin(t*2.0+seed+4.0),.24,t+3.1);
 float density=clamp(center+left*.7+right*.7,0.0,1.0);
 float core=exp(-p.x*p.x*30.0)*exp(-max(p.y,0.0)*4.5)*density;
 vec3 color=vec3(1.0,.17,.012)*density;
 color+=vec3(.35,.52,.13)*core;
 color+=vec3(.22,.20,.08)*pow(density,3.0)*(1.0-clamp(p.y,0.0,1.0));
 // Black outside the density field; additive blend cannot leave a rectangle.
 gl_FragColor=vec4(color*tint.rgb,1.0);
}
)GLSL";
 GLuint vs=Compile(GL_VERTEX_SHADER,vertex),fs=Compile(GL_FRAGMENT_SHADER,fragment);if(!vs||!fs){if(vs)glDeleteShader(vs);if(fs)glDeleteShader(fs);return p;}
 p.id=glCreateProgram();glAttachShader(p.id,vs);glAttachShader(p.id,fs);glLinkProgram(p.id);GLint ok=0;glGetProgramiv(p.id,GL_LINK_STATUS,&ok);glDeleteShader(vs);glDeleteShader(fs);
 if(!ok){char error[2048]={};glGetProgramInfoLog(p.id,sizeof(error),NULL,error);Log(error);glDeleteProgram(p.id);p.id=0;return p;}
 p.seconds=glGetUniformLocation(p.id,"seconds");p.seed=glGetUniformLocation(p.id,"seed");p.detail=glGetUniformLocation(p.id,"detailLevel");Log("Ready V3: three turbulent flame tongues, luminous core, fixed emitter, one pass.");return p;
}
class Scope{GLint previous;bool active;public:
 Scope(float time,float seed,float fps):previous(0),active(false){Program& p=Get();if(!p.id)return;glGetIntegerv(GL_CURRENT_PROGRAM,&previous);if(previous)return;glUseProgram(p.id);glUniform1f(p.seconds,time);glUniform1f(p.seed,seed);glUniform1f(p.detail,fps<=0.f||fps>=58.f?1.f:0.f);active=true;}
 ~Scope(){if(active)glUseProgram(previous);}
private:Scope(const Scope&);Scope& operator=(const Scope&);
};
}
