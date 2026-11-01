#pragma once
#include <cmath>
namespace TooltipGeometry {
struct Point {float x,y;};
inline float ClampStart(float start,float extent,float viewport,float margin=8){
 const float maximum=viewport-margin-extent;
 if(maximum<margin)return margin;
 return start<margin?margin:start>maximum?maximum:start;
}
inline float FitRatio(float width,float height,float availableWidth,float availableHeight){
 float r=1.0f;
 if(width>availableWidth && width>0)r=availableWidth/width;
 if(height>availableHeight && height>0 && availableHeight/height<r)r=availableHeight/height;
 return r<0.01f?0.01f:r;
}
inline float Beside(float left,float right,float width,float viewport,float gap){
 if(left-gap-width>=4)return left-gap-width;
 if(right+gap+width<=viewport-4)return right+gap;
 const float candidate=left-gap-width;
 const float maximum=viewport-width-4;
 return candidate<4?4:candidate>maximum?(maximum<4?4:maximum):candidate;
}
inline Point Border(float x,float y,float w,float h,float distance){
 const float p=2*(w+h);if(p<=0)return {x,y};
 float d=std::fmod(distance,p);if(d<0)d+=p;
 if(d<=w)return {x+d,y};d-=w;
 if(d<=h)return {x+w,y+d};d-=h;
 if(d<=w)return {x+w-d,y+h};d-=w;
 return {x,y+h-d};
}
}
