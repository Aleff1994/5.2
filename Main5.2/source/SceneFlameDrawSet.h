#pragma once
struct SceneFlameDrawSet {
    struct Point {float x,y,z;};Point points[256];unsigned count;
    SceneFlameDrawSet():count(0){}
    bool First(float x,float y,float z){
        for(unsigned i=0;i<count;++i)if(points[i].x==x && points[i].y==y && points[i].z==z)return false;
        if(count<256){points[count].x=x;points[count].y=y;points[count].z=z;++count;}
        return true;
    }
};
