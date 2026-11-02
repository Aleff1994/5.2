#pragma once
namespace CrowdMeshBudget {
struct Counters {unsigned skippedPasses,skippedSecondaryCalls,gpuBaseIndexDraws,clientBaseIndexDraws,clientSmallMeshDraws;unsigned long long skippedFaces,flatShadowSamples,fullShadowSamples;Counters():skippedPasses(0),skippedSecondaryCalls(0),gpuBaseIndexDraws(0),clientBaseIndexDraws(0),clientSmallMeshDraws(0),skippedFaces(0),flatShadowSamples(0),fullShadowSamples(0){} };
inline Counters& Stats(){static Counters s;return s;}
inline void BeginFrame(){Stats()=Counters();}
struct Entry{const void* model;int mesh;bool base;unsigned extra;};
struct State{Entry entries[128];const void* glowModels[64];int glowTypes[64];unsigned glowCount;bool textureOnly;unsigned count,limit,modelLimit;State():glowCount(0),textureOnly(false),count(0),limit(1),modelLimit(2){} };
inline State*& Current(){static State* p=0;return p;}
class Scope {State state;State* previous;public:explicit Scope(bool enabled,bool textureOnly=false):previous(Current()){state.textureOnly=textureOnly;Current()=enabled?&state:0;}~Scope(){Current()=previous;}private:Scope(const Scope&);Scope& operator=(const Scope&);};
// Skip dispatch only when every mesh already has an opaque textured base.
// Models with chrome-only or transparent parts must keep their first material.
inline bool SkipTexturedModelGlow(const void* model,int meshes,int flags,float alpha){
 State* s=Current();
 const int decorative=RENDER_CHROME|RENDER_METAL|RENDER_CHROME2|RENDER_CHROME3|RENDER_CHROME4|RENDER_CHROME5|RENDER_CHROME6|RENDER_CHROME7|RENDER_CHROME8|RENDER_OIL;
 if(!s || !s->textureOnly || !model || meshes<=0 || meshes>128 || alpha<.98f || !(flags&decorative) || (flags&(RENDER_TEXTURE|RENDER_COLOR|RENDER_DARK|RENDER_SHADOWMAP|RENDER_NODEPTH|RENDER_WAVE|RENDER_WAVE_EXT)))return false;
 for(int mesh=0;mesh<meshes;++mesh){
  bool base=false;
  for(unsigned i=0;i<s->count;++i)if(s->entries[i].model==model && s->entries[i].mesh==mesh && s->entries[i].base){base=true;break;}
  if(!base)return false;
 }
 ++Stats().skippedSecondaryCalls;return true;
}
inline bool SkipSecondaryGlow(const void* model,int type,int flags){
 State* s=Current();
 const int decorative=RENDER_CHROME|RENDER_METAL|RENDER_CHROME2|RENDER_CHROME3|RENDER_CHROME4|RENDER_CHROME5|RENDER_CHROME6|RENDER_CHROME7|RENDER_CHROME8|RENDER_OIL;
 if(!s || !model || type<0 || !(flags&decorative) || (flags&(RENDER_TEXTURE|RENDER_COLOR|RENDER_DARK|RENDER_SHADOWMAP|RENDER_NODEPTH|RENDER_WAVE|RENDER_WAVE_EXT)))return false;
 for(unsigned i=0;i<s->glowCount;++i)if(s->glowModels[i]==model && s->glowTypes[i]==type){++Stats().skippedSecondaryCalls;return true;}
 if(s->glowCount>=64)return false;
 s->glowModels[s->glowCount]=model;s->glowTypes[s->glowCount++]=type;
 return false;
}
inline bool Skip(const void* model,int mesh,int flags,float alpha,unsigned faces){
 State* s=Current();
 if(!s || !model || mesh<0 || alpha<.98f || (flags&(RENDER_COLOR|RENDER_DARK|RENDER_SHADOWMAP|RENDER_NODEPTH|RENDER_WAVE|RENDER_WAVE_EXT)))return false;
 const int decorative=RENDER_CHROME|RENDER_METAL|RENDER_CHROME2|RENDER_CHROME3|RENDER_CHROME4|RENDER_CHROME5|RENDER_CHROME6|RENDER_CHROME7|RENDER_CHROME8|RENDER_OIL;
 if(!(flags&(decorative|RENDER_TEXTURE)))return false;
 Entry* e=0;
 for(unsigned i=0;i<s->count;++i)if(s->entries[i].model==model&&s->entries[i].mesh==mesh){e=&s->entries[i];break;}
 if(!e){if(s->count>=128)return false;e=&s->entries[s->count++];e->model=model;e->mesh=mesh;e->base=false;e->extra=0;}
 if(flags&decorative){
  // Never discard a sole chrome material: a textured base must precede it.
  if(!e->base)return false;
  if(s->textureOnly){++Stats().skippedPasses;Stats().skippedFaces+=faces;return true;}
  unsigned modelExtras=0;
  for(unsigned i=0;i<s->count;++i)if(s->entries[i].model==model)modelExtras+=s->entries[i].extra;
  if(e->extra<s->limit && modelExtras<s->modelLimit){++e->extra;return false;}
  ++Stats().skippedPasses;Stats().skippedFaces+=faces;return true;
 }
 if(flags&RENDER_TEXTURE){e->base=true;e->extra=0;}
 return false;
}
}
