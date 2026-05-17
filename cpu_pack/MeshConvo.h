#pragma once

#include "TrigMesh.h"
#include "BBox.h"
#include "Array3D.h"
#include <complex>

// data for computing convolution for a mesh.
class MeshConvo {
  public:
    // can be bigger than actual mesh box
    // box of the grid in world space.
    Box3f box;
    // original boundingbox for the mesh
    Box3f meshBox;
    Array3D8u vox;
    Array3D<std::complex<float>> fft;
    float dx = 1e-3;

    // for linear convolution,
    // the voxel grid is temporarily padded to the
    // sum of two grid sizes.
    Vec3u paddedSize;
    bool gridReversed = false;
    // allocates and fills vox also updates box.
    // does nothing if mesh is null.
    void Voxelize(float voxelSize);

    void SetMeshPtr(TrigMesh *m) {
      mesh = m;
    }
    TrigMesh *GetMesh() {
      return mesh;
    }

    Vec3f GetOrigin() const {
      return box.vmin;
    }

    // original center of mesh bounding box
    // before padding the grid
    Vec3f GetMeshCenter()const{
      return 0.5f *(meshBox.vmin + meshBox.vmax);
    }

    Vec3u GridSize() const {
      return vox.GetSize();
    }
    Array3D8u PadVox(Vec3u newSize) const ;

    // since we only care about collision, all images are integer valued.
    // we should use NTT instead and always pad to powers of 2 up to 1024.
    // should limit to binary values to prevent overflow of coefficients to > 2^32
    // can do check for min(pixel_count1, pixel_count2)
    // if too many pixels in both , the result is invalid.
    void FFT(Vec3u paddedSize);

    // some distance metric.
    Array3D8u dist;

  private:
    TrigMesh *mesh = nullptr;
};

std::array<unsigned, 3> PadSizes(Vec3u &s, unsigned alignment);

std::array<unsigned, 3> ComputeGridSize(const Box3f &box, float dx, unsigned alignment);

void Union(MeshConvo &bg, Vec3i offset, const Array3D8u &fg);

void UnionReversed(MeshConvo &bg, Vec3i offset, const MeshConvo &fg) ;