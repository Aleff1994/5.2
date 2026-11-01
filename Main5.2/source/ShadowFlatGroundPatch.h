#pragma once
// Exact constant-height region. Rebuilt for each projection; no inter-frame cache.
struct ShadowFlatGroundPatch {
 bool valid;float minX,minY,maxX,maxY,height;
 ShadowFlatGroundPatch():valid(false),minX(0),minY(0),maxX(0),maxY(0),height(0){}
 void Build(const float* heights,const unsigned short* attributes,int size,float scale,float x,float y,unsigned short specialMask){
  valid=false;if(!heights||!attributes||size<2||scale<=0||x<0||y<0||x>=size*scale||y>=size*scale)return;
  int cx=static_cast<int>(x/scale),cy=static_cast<int>(y/scale);
  int loX=cx>4?cx-4:0,loY=cy>4?cy-4:0;
  int hiX=cx+4<size?cx+4:size-1,hiY=cy+4<size?cy+4:size-1;
  if(hiX<=loX||hiY<=loY)return;
  const float h=heights[loY*size+loX];
  for(int yy=loY;yy<=hiY;++yy)for(int xx=loX;xx<=hiX;++xx){
   if(heights[yy*size+xx]!=h)return;
   if(xx<hiX&&yy<hiY&&(attributes[yy*size+xx]&specialMask))return;
  }
  height=h;minX=loX*scale;minY=loY*scale;maxX=hiX*scale;maxY=hiY*scale;valid=true;
 }
 bool Contains(float x,float y)const{return valid&&x>=minX&&y>=minY&&x<maxX&&y<maxY;}
};
