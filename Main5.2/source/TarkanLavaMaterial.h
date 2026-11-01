#pragma once
// Trial restricted to Tarkan/Lost Tower animated terrain. No extra geometry/pass.
namespace TarkanLavaMaterial {
struct Settings {
 bool enabled, lostTower, material, adaptive; int quality; float lowFps;
 Settings() {
  enabled=GetPrivateProfileIntA("LavaVisual","TarkanTrial",0,".\\config.ini")!=0;
  lostTower=GetPrivateProfileIntA("LavaVisual","LostTowerTrial",0,".\\config.ini")!=0;
  material=GetPrivateProfileIntA("LavaVisual","Material",1,".\\config.ini")!=0;
  adaptive=GetPrivateProfileIntA("LavaVisual","Adaptive",1,".\\config.ini")!=0;
  quality=GetPrivateProfileIntA("LavaVisual","Quality",3,".\\config.ini");
  if(quality<0)quality=0;if(quality>3)quality=3;
  int fps=GetPrivateProfileIntA("LavaVisual","LowFPS",55,".\\config.ini");
  if(fps<30)fps=30;if(fps>60)fps=60;lowFps=static_cast<float>(fps);
 }
};
inline const Settings& Config(){static const Settings c;return c;}
inline bool ForMap(int world){return (world==8 && Config().enabled)||(world==4 && Config().lostTower);}
inline void Log(const char* message){FILE* f=fopen("ThermalMaterial.log","a");if(f){fprintf(f,"%s\n",message);fclose(f);}}
inline GLuint Compile(GLenum type,const char* text){
 GLuint s=glCreateShader(type);if(!s)return 0;glShaderSource(s,1,&text,NULL);glCompileShader(s);
 GLint ok=0;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);if(ok)return s;
 char error[2048]={};glGetShaderInfoLog(s,sizeof(error),NULL,error);Log(error);glDeleteShader(s);return 0;
}
struct Program {GLuint id;GLint time,detail,texture,fog;bool tried;Program():id(0),tried(false){} };
inline Program& GetProgram(){
 static Program p;if(p.tried)return p;p.tried=true;
 if(!GLEW_VERSION_2_0){Log("OpenGL 2.0 unavailable: detailed lava texture retained without shader.");return p;}
 const char* vertex=R"GLSL(#version 120
varying vec2 uv;varying vec4 tint;varying float depth;
void main(){gl_Position=ftransform();uv=gl_MultiTexCoord0.xy;tint=gl_Color;depth=abs((gl_ModelViewMatrix*gl_Vertex).z);}
)GLSL";
 const char* fragment=R"GLSL(#version 120
uniform sampler2D lavaTexture;uniform float seconds,detailLevel;uniform int fogEnabled;
varying vec2 uv;varying vec4 tint;varying float depth;
float hash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
float noise(vec2 p){vec2 i=floor(p),f=fract(p);f=f*f*(3.0-2.0*f);return mix(mix(hash(i),hash(i+vec2(1,0)),f.x),mix(hash(i+vec2(0,1)),hash(i+vec2(1,1)),f.x),f.y);}
void main(){
 // Continuous slow advection; large crust plates and emissive molten channels.
 vec2 drift=vec2(seconds*.008,-seconds*.005);
 vec2 flow=vec2(sin(uv.y*9.0+seconds*.24),sin(uv.x*8.0-seconds*.19))*.014;
 vec2 q=uv+drift+flow;
 vec3 base=texture2D(lavaTexture,q).rgb;
 float moltenMask=smoothstep(.12,.58,base.r-base.b);
 float crust=noise(q*11.0+vec2(seconds*.025,0.0));
 float pulse=.86+.14*sin(seconds*.85+q.x*8.0+q.y*6.0);
 float heat=clamp(moltenMask*(.8+.2*crust)*pulse,0.0,1.0);
 vec3 rock=base*max(tint.rgb,vec3(.20))*.66;
 vec3 molten=mix(vec3(.58,.035,.001),vec3(1.0,.42,.012),smoothstep(.12,.8,heat));
 molten=mix(molten,vec3(1.0,.82,.20),smoothstep(.72,.98,heat));
 if(detailLevel>.5){
  vec3 micro=texture2D(lavaTexture,q+vec2(.0015,0.0)).rgb;
  float relief=clamp((base.r-micro.r)*4.0,-.20,.20);
  rock*=1.0+relief;
  float fine=noise(q*36.0-vec2(0.0,seconds*.06));
  molten*=.90+.10*fine;
 }
 vec3 color=mix(rock,molten,moltenMask);
 if(fogEnabled!=0){float f=clamp((gl_Fog.end-depth)*gl_Fog.scale,0.0,1.0);color=mix(gl_Fog.color.rgb,color,f);}
 gl_FragColor=vec4(clamp(color,0.0,1.0),tint.a);
}
)GLSL";
 GLuint vs=Compile(GL_VERTEX_SHADER,vertex),fs=Compile(GL_FRAGMENT_SHADER,fragment);
 if(!vs||!fs){if(vs)glDeleteShader(vs);if(fs)glDeleteShader(fs);return p;}
 p.id=glCreateProgram();if(p.id){glAttachShader(p.id,vs);glAttachShader(p.id,fs);glLinkProgram(p.id);GLint ok=0;glGetProgramiv(p.id,GL_LINK_STATUS,&ok);
 if(!ok){char error[2048]={};glGetProgramInfoLog(p.id,sizeof(error),NULL,error);Log(error);glDeleteProgram(p.id);p.id=0;}}
 glDeleteShader(vs);glDeleteShader(fs);if(!p.id)return p;
 p.time=glGetUniformLocation(p.id,"seconds");p.detail=glGetUniformLocation(p.id,"detailLevel");p.texture=glGetUniformLocation(p.id,"lavaTexture");p.fog=glGetUniformLocation(p.id,"fogEnabled");
 Log("Lava V3 ready: advected crust, thermal channels and filtered relief; existing terrain pass.");return p;
}
class Scope {
 GLint previous;bool active;
public:
 Scope(bool enabled,float seconds,float fps):previous(0),active(false){
  if(!enabled || !Config().material || Config().quality==0)return;
  static bool budgetAvailable=true;
  if(Config().adaptive && fps>0.f){if(fps<Config().lowFps)budgetAvailable=false;else if(fps>=fminf(60.f,Config().lowFps+3.f))budgetAvailable=true;}
  if(Config().adaptive && !budgetAvailable)return;
  Program& p=GetProgram();if(!p.id)return;glGetIntegerv(GL_CURRENT_PROGRAM,&previous);if(previous!=0)return;
  GLint unit=GL_TEXTURE0;glGetIntegerv(GL_ACTIVE_TEXTURE,&unit);glUseProgram(p.id);
  glUniform1f(p.time,seconds);glUniform1f(p.detail,Config().quality>=3?1.f:0.f);
  glUniform1i(p.texture,unit-GL_TEXTURE0);glUniform1i(p.fog,glIsEnabled(GL_FOG)?1:0);active=true;
 }
 ~Scope(){if(active)glUseProgram(previous);}
private: Scope(const Scope&);Scope& operator=(const Scope&);
};
}
