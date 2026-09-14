#pragma once
#include "Mesh/TrigMesh.h"
#include "Math/Array3D.h"

/// @brief not super fast. don't do it all the time 
/// @param input
/// @param holeRadius - holes and openings smaller than this radius are filled
/// @return - wrapped mesh. emtpy if input is empty
TrigMesh ShrinkWrap(const TrigMesh& input, float holeRadius, float voxelSize = 1.0f);


class AdapUDF;

/// @brief approximate distance field by voxelizing the shell.
/// flood the ouside with positive values and inside with negative.
/// @param maxDistVox maximum distance needed in number of voxels.
/// @param udf computed distance field stored here. distance values inside the shell
/// are just to some negative constant and ignored.
void ApproxShellDistanceField(const TrigMesh& mesh, AdapUDF* udf, float maxDistVox);

/// @brief helper that computes the distance field for shrinkwrap.
void ShrinkWrapDistanceField(const TrigMesh& input, float holeRadius, AdapUDF* udf);

/// @brief helper function that labels voxels outside of the mesh by more than
/// distThres.
Array3D8u FloodOutside(const Array3D<short>& dist, float distThresh);