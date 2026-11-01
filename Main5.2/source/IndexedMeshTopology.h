#pragma once
#include <map>
#include <tuple>
#include <vector>
namespace IndexedMeshTopology {
struct Topology { std::vector<unsigned int> indices; std::vector<int> corners; };
template<class Triangle> bool Build(const Triangle* triangles,int count,int vertices,int normals,int uvs,Topology& result) {
 result.indices.clear();result.corners.clear();
 if(!triangles || count<=0 || vertices<=0 || normals<=0 || uvs<=0)return false;
 std::map<std::tuple<int,int,int>,unsigned int> unique;
 for(int t=0;t<count;++t){
  if(triangles[t].Polygon!=3){result.indices.clear();result.corners.clear();return false;}
  for(int c=0;c<3;++c){
   const int v=triangles[t].VertexIndex[c], n=triangles[t].NormalIndex[c], uv=triangles[t].TexCoordIndex[c];
   if(v<0 || v>=vertices || n<0 || n>=normals || uv<0 || uv>=uvs){result.indices.clear();result.corners.clear();return false;}
   auto entry=unique.emplace(std::make_tuple(v,n,uv),static_cast<unsigned int>(result.corners.size()));
   if(entry.second)result.corners.push_back(t*3+c);
   result.indices.push_back(entry.first->second);
  }
 }
 return result.corners.size()<result.indices.size();
}
}
