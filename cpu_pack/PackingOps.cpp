#include "PackingOps.h"
#include "AdapSDF.h"
#include "MeshOps.h"
#include "GridUtils.h"
#include "Array2D.h"

#include <iostream>

// from x y z index in convolved image to translation for fg mesh.
Vec3f GetDisplacement(Vec3u gridIdx, float dx, Vec3u fgSize, Vec3f fgOrigin, Vec3f bgOrigin) {
  Vec3f dxVec(dx);
  Vec3f s = fgSize.cast<float>() - Vec3f(1.0f);
  Vec3f origin = bgOrigin - fgOrigin - dxVec * s;
  Vec3f disp = dx * gridIdx.cast<float>() + origin;
  return disp;
}

bool AddUsingSDF(MeshConvo &bg, const TrigMesh &part, Vec3f &pos, const Vec3f &rot, 
  std::shared_ptr<AdapSDF> sdf, float factor) {
  Matrix3f rotMat = RotationMatrixRad(rot[0], rot[1], rot[2]);
  TrigMesh rotated = part;
  TransformVerts(part.v, rotated.v, rotMat);
  float dx = bg.dx;

  MeshConvo fg;
  fg.SetMeshPtr(&rotated);
  fg.Voxelize(dx);
  // makes values in convo smaller.
  ThreshInPlace(fg.vox, 1);
  const unsigned FFT_ALIGNMENT = 8;
  Vec3u bgSize = bg.GridSize();
  Vec3u fgSize = fg.GridSize();
  // use circular fft/ntt. no need to pad with fg size.
  Vec3u totalSize = bgSize;
  //+fgSize;
  std::cout << "total grid size " << totalSize[0] << " " << totalSize[1] << " " << totalSize[2] << "\n";
  Vec3u gridSize = PadSizes(totalSize, FFT_ALIGNMENT);

  bg.FFT(gridSize);
  Reverse(fg.vox);
  fg.gridReversed = true;
  fg.FFT(gridSize);
  Dot(bg.fft, fg.fft);
  Array3Df conv = IFFT(fg.fft);
  Array3D8u collision = Quantize(conv);
  const float score0 = -1e6f;
  
  float highScore = score0;
  Vec3u bestPos(0);

  Vec3f center = (gridSize.cast<float>() + fgSize.cast<float>());
  center = 0.5f * center - Vec3f(1);
  float maxDist = gridSize.cast<float>().norm();
  // index from 0 to fgSize-2 are outside of the container.
  // fgSize -1 is inside the container.
    
  Vec3f vmin = fg.meshBox.vmin;
  Vec3f vmax = fg.meshBox.vmax;  
  Vec3f meshCenter = fg.GetMeshCenter();
  std::cout<<"fg grid size "<<fg.GridSize()[0]<<" "<<fg.GridSize()[1]<<" "<<fg.GridSize()[2]<<"\n";
  std::cout<<"fg origin " <<fg.GetOrigin()[0]<<" "<<fg.GetOrigin()[1] <<" "<<fg.GetOrigin()[2] <<"\n";

  Array2D8u debugSlice (gridSize[0], gridSize[1]);
  debugSlice.Fill(0);
  Array2D8u collSlice (gridSize[0], gridSize[1]);

  // add a little attraction towards bottom left.
  // using normalized x y z coordinate .
  float positionWeight = -1.0f;
  for (unsigned z = fgSize[2] - 1; z < gridSize[2]; z++) {
    for (unsigned y = fgSize[1] - 1; y < gridSize[1]; y++) {
      for (unsigned x = fgSize[0] - 1; x < gridSize[0]; x++) {
        if(z == gridSize[2]/2){
          collSlice(x,y) = collision(x,y,z);
        }
        if (collision(x, y, z) > 0) {
          continue;
        }
        Vec3f disp = GetDisplacement(Vec3u(x,y,z), dx, fgSize, fg.GetOrigin(), bg.GetOrigin());
        Vec3f coord = disp + meshCenter;
    
        float dist = sdf->GetCoarseDist(coord);
        if (dist >= 32766) {
          dist = -dist;
        }
        float score = factor * dist + positionWeight * (std::abs(coord[0])+ std::abs(coord[1])+ std::abs(coord[2]));
        if(z == gridSize[2]/2){
          debugSlice(x,y) = score * 10;
        }
        if (score > highScore) {
          highScore = score;
          bestPos = Vec3u(x, y, z);
        }
      }
    }
  }

  float voxRes[3]={dx,dx,dx};
  // SavePngGrey("F:/meshes/fruit_hand/colli_slice.png", collSlice);
  // SavePngGrey("F:/meshes/fruit_hand/score_slice.png", debugSlice);
  std::cout << "high score " << highScore << "\n" ;
  bool found = (highScore > score0);
  if (found) {    
    pos = GetDisplacement(bestPos, dx, fg.vox.GetSize(), fg.GetOrigin(), bg.GetOrigin());
    // offset of fg grid to be unioned into bg grid. 
    // e.g. if fg grid size is 1, then offset is just bestPos.
    Vec3i offset = bestPos.cast<int>() - fgSize.cast<int>() + Vec3i(1);
    if (fg.gridReversed) {
      UnionReversed(bg, offset, fg);
    } else {
      Union(bg, offset, fg.vox);
    }
  }
  return found;
}