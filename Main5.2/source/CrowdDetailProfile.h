#pragma once
#include <array>
#include <chrono>
#include <cstdio>

// Sample only the existing one-frame-per-five-seconds character measurement.
// Timings are inclusive, may overlap, and must not be added to character CPU time.
namespace CrowdDetailProfile {
enum Category { WingLink, OtherLink, ClothSimulation, ClothRender, Count };
struct State {
    bool active=false;
    std::array<double,Count> milliseconds{};
    std::array<unsigned,Count> calls{};
};
inline State& Get(){static State state;return state;}
inline void Begin(bool active){Get()=State{};Get().active=active;}
class Scope {
    Category category;
    bool active;
    std::chrono::steady_clock::time_point start;
public:
    explicit Scope(Category value):category(value),active(Get().active) {
        if(active)start=std::chrono::steady_clock::now();
    }
    ~Scope(){
        if(!active)return;
        auto& state=Get();
        state.milliseconds[category]+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        ++state.calls[category];
    }
    Scope(const Scope&)=delete;
    Scope& operator=(const Scope&)=delete;
};
inline void End(unsigned long tick,int world,int players,float fps){
    auto& state=Get();
    if(!state.active)return;
    state.active=false;
    FILE* file=std::fopen("CrowdDetail.log","a");
    if(!file)return;
    std::fprintf(file,"Time=%lu World=%d Players=%d FPS=%.2f Inclusive=1 WingLinkMs=%.3f WingLinkCalls=%u OtherLinkMs=%.3f OtherLinkCalls=%u ClothSimMs=%.3f ClothSimCalls=%u ClothRenderMs=%.3f ClothRenderCalls=%u\n",
        tick,world,players,fps,state.milliseconds[WingLink],state.calls[WingLink],
        state.milliseconds[OtherLink],state.calls[OtherLink],state.milliseconds[ClothSimulation],state.calls[ClothSimulation],
        state.milliseconds[ClothRender],state.calls[ClothRender]);
    std::fclose(file);
}
}
