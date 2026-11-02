#pragma once
namespace NaturalSurfaceLight {
inline float Clamp(float x,float lo,float hi){return x<lo?lo:x>hi?hi:x;}
// Mild directional + hemispheric fill, blended to retain the original art.
// Reuses transformed normals; no new textures, lights or render passes.
inline float Blend(float original,float directional,float up){
 const float sky=Clamp(up*.5f+.5f,0.f,1.f);
 const float natural=.30f+.12f*sky+.66f*Clamp(directional,0.f,1.f);
 return original*.65f+natural*.35f;
}
}
