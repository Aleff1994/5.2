#pragma once
namespace MeshSubmissionPolicy {
// Intel HD 4600 paired offscreen test: 600 small draws, median 13.4 ms with
// client arrays versus 20.8 ms with streamed vertices/GPU indices. This is a
// driver-specific threshold, not a reduction in mesh or character detail.
inline bool PreferClientArrays(bool intel,int vertices,int indices,bool enabled=true){
 return enabled && intel && vertices>0 && vertices<=512 && indices>0 && indices<=1536;
}
}
