#pragma once
#include "Array3D.h"
#include <array>
#include <complex>
#include <stdint.h>
// similar to array utils lol.

Vec3f AlignOriginToGrid(const Vec3f &o, float dx) ;

std::array<float, 3> ToArray(const Vec3f &v);

Array3D8u Thresh(const Array3Df &f, float thresh);

void ThreshInPlace(Array3D8u &arr , uint8_t thresh);

Array3D8u Quantize(const Array3Df &f);

Array3D8u ThreshLessThan(const Array3Df &f, float thresh);

void Dot(const Array3D<std::complex<float>> &src, Array3D<std::complex<float>> &dst);

void Scale(Array3Df &arr, float scale);

void Reverse(Array3D8u &arr);

Vec3f ToFloat(const Vec3u &v);

Vec3u ToVec3u(const Vec3f &fvec);

bool InBound(Vec3f x, Vec3u size);

bool InBound(Vec3u x, Vec3u size);

void SaveSlice(const std::string &filename, const Array3D8u &arr, unsigned z, float scale);

// makes the exterior of the container unavailable for packing.
// opposite of packed items.
// boundary value is kept so that no item touches the boundary.
void InvertContainer(Array3D8u &vox, uint8_t boundaryVal);

// seeds classVal into cls wherever vox is nonzero. cls must already be
// allocated to vox's size, e.g. right after InvertContainer to seed
// VOX_CONTAINER before anything is placed.
void ClassFromBinary(const Array3D8u &vox, Array3D8u &cls, uint8_t classVal);

// mirrors Union (MeshConvo.cpp): stamps cls into dst wherever mask is
// nonzero, offset the same way Union offsets fg into bg. A voxel is only
// overwritten if its current class has a set bit in overwriteMask, e.g.
// (1u << VOX_CONTAINER) | (1u << VOX_INNER) so a placed item overwrites the
// container and inner-shell classes but not another item's class.
void StampClass(Array3D8u &dst, Vec3i offset, const Array3D8u &mask,
                uint8_t cls, uint32_t overwriteMask);

// assumes input size is even.
Array3Df IFFT(const Array3D<std::complex<float>> &coeff);