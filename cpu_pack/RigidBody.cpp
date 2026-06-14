#include "RigidBody.h"

RigidBody::RigidBody(TrigMesh &input) {
  // Compute center of mass
  Vec3f com(0, 0, 0);
  float totalVolume = 0;

  for (size_t i = 0; i < input.GetNumTrigs(); i++) {
    Vec3u tri = input.Trig(i);
    Vec3f v0 = input.Vert(tri[0]);
    Vec3f v1 = input.Vert(tri[1]);
    Vec3f v2 = input.Vert(tri[2]);

    // Tetrahedron volume with origin
    float tetVolume = v0.dot(v1.cross(v2)) / 6.0f;
    Vec3f tetCenter = (v0 + v1 + v2) * 0.25f;

    com += tetCenter * tetVolume;
    totalVolume += tetVolume;
  }
  vol = totalVolume;
  com /= totalVolume;
  oldOrigin = com;
  // Transform mesh to inertia frame
  auto const &mesh = input;

  // Compute inertia tensor about center of mass
  Matrix3f I;
  for (size_t i = 0; i < mesh.GetNumTrigs(); i++) {
    Vec3u tri = mesh.Trig(i);
    Vec3f v0 = mesh.Vert(tri[0]) - com;
    Vec3f v1 = mesh.Vert(tri[1]) - com;
    Vec3f v2 = mesh.Vert(tri[2]) - com;

    float tetVolume = v0.dot(v1.cross(v2)) / 6.0f;

    // Inertia tensor for tetrahedron
    for (int j = 0; j < 3; j++) {
      for (int k = 0; k < 3; k++) {
        float val = 0;
        for (int l = 0; l < 4; l++) {
          Vec3f p;
          if (l == 0)
            p = Vec3f(0, 0, 0);
          else if (l == 1)
            p = v0;
          else if (l == 2)
            p = v1;
          else
            p = v2;

          val += p[j] * p[k];
        }
        val *= tetVolume / 10.0f;
        I(j, k) += val;
      }
    }
  }

  // Convert to moment of inertia tensor
  float trace = I(0, 0) + I(1, 1) + I(2, 2);
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      if (i == j) {
        I(i, j) = trace - I(i, j);
      } else {
        I(i, j) = -I(i, j);
      }
    }
  }

  // Diagonalize inertia tensor to get principal axes
  double I_array[3][3];
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      I_array[i][j] = I(i, j);
    }
  }

  double eigenvalues[3];
  eigvalss(I_array, eigenvalues);

  // Compute eigenvectors
  Matrix3f eigenvectors = eigVecFromVal(I, eigenvalues);
  R0 = eigenvectors;
  inertia(0, 0) = eigenvalues[0];
  inertia(1, 1) = eigenvalues[1];
  inertia(2, 2) = eigenvalues[2];

}

// Helper to find a non-zero vector perpendicular to a given vector
Vec3f findPerpendicular(const Vec3f &v) {
  // Find the smallest component to avoid division by zero/instability
  int min_idx = 0;
  if (std::abs(v[1]) < std::abs(v[min_idx]))
    min_idx = 1;
  if (std::abs(v[2]) < std::abs(v[min_idx]))
    min_idx = 2;

  Vec3f target(0, 0, 0);
  target[min_idx] = 1.0;

  // Cross product guarantees perpendicularity
  Vec3f p = v.cross(target);
  p.normalize();
  return p;
}

Matrix3f eigVecFromVal(const Matrix3f &A_in, const double *eigenvalues) {
  Matrix3f eigenvectors;

  for (int k = 0; k < 3; k++) {
    double lambda = eigenvalues[k];

    // 1. Reset and Shift Matrix M = A - lambda * I
    double M[3][3];
    for (int i = 0; i < 3; ++i) {
      for (int j = 0; j < 3; ++j) {
        M[i][j] = A_in(i, j);
      }
      M[i][i] -= lambda;
    }

    // 2. Row Reduction with Full Pivoting to find the Kernel
    // Track column swaps to map solution back correctly
    int col_order[3] = {0, 1, 2};
    int rank = 0;
    const double eps = 1e-9;

    for (int step = 0; step < 3; step++) {
      // Find max pivot element in remaining submatrix
      int p_row = step, p_col = step;
      double max_val = std::abs(M[step][step]);
      for (int r = step; r < 3; r++) {
        for (int c = step; c < 3; c++) {
          if (std::abs(M[r][c]) > max_val) {
            max_val = std::abs(M[r][c]);
            p_row = r;
            p_col = c;
          }
        }
      }

      if (max_val < eps)
        break; // Remaining submatrix is effectively zero

      // Swap rows
      if (p_row != step) {
        for (int c = 0; c < 3; c++)
          std::swap(M[step][c], M[p_row][c]);
      }
      // Swap columns
      if (p_col != step) {
        for (int r = 0; r < 3; r++)
          std::swap(M[r][step], M[r][p_col]);
        std::swap(col_order[step], col_order[p_col]);
      }

      rank++;

      // Eliminate lower rows
      for (int r = step + 1; r < 3; r++) {
        double factor = M[r][step] / M[step][step];
        for (int c = step; c < 3; c++) {
          M[r][c] -= factor * M[step][c];
        }
      }
    }

    // 3. Extract the Kernel vector 'v' based on structural Rank
    Vec3f v;

    if (rank == 2) {
      // Case A: Rank 2 -> Unique eigenvalue. Null space is a 1D line.
      // Solve back-substitution assuming v_permuted[2] = 1.0
      double v_perm[3];
      v_perm[2] = 1.0;
      v_perm[1] = -M[1][2] * v_perm[2] / M[1][1];
      v_perm[0] = (-M[0][1] * v_perm[1] - M[0][2] * v_perm[2]) / M[0][0];

      // Map back via tracking array
      v[col_order[0]] = v_perm[0];
      v[col_order[1]] = v_perm[1];
      v[col_order[2]] = v_perm[2];
    } else if (rank <= 1) {
      // Case B: Rank 1 -> Repeated eigenvalue (Your Rod problem).
      // Null space is a 2D plane. All rows became parallel.
      // M[0] is the only valid normal to this plane.
      Vec3f normal(M[0][0], M[0][1], M[0][2]);

      // Re-map normal out of permuted space
      Vec3f true_normal;
      true_normal[col_order[0]] = normal[0];
      true_normal[col_order[1]] = normal[1];
      true_normal[col_order[2]] = normal[2];

      if (true_normal.norm2() < eps) {
        // Total degeneracy (all entries 0), fallback to standard axis
        v = Vec3f(k == 0, k == 1, k == 2);
      } else {
        true_normal.normalize();

        if (k == 1 && eigenvalues[0] == eigenvalues[1]) {
          // For the second identical eigenvalue, pick a vector that is
          // perpendicular both to the plane normal AND the first eigenvector
          Vec3f prev_v(eigenvectors(0, 0), eigenvectors(1, 0), eigenvectors(2, 0));
          v = true_normal.cross(prev_v);
        } else {
          // First identical eigenvalue: pick any vector on the plane
          v = findPerpendicular(true_normal);
        }
      }
    } else { // rank == 3, theoretically impossible for an exact eigenvalue, handle gracefully
      v = Vec3f(1, 0, 0);
    }

    v.normalize();
    eigenvectors(0, k) = v[0];
    eigenvectors(1, k) = v[1];
    eigenvectors(2, k) = v[2];
  }

  // 4. Gram-Schmidt Orthogonalization step to force precise 90-degree axes
  // Essential for rod dynamics where numerical drift bleeds axes into each other.
  Vec3f v0(eigenvectors(0, 0), eigenvectors(1, 0), eigenvectors(2, 0));
  Vec3f v1(eigenvectors(0, 1), eigenvectors(1, 1), eigenvectors(2, 1));

  v0.normalize();

  // Project v1 onto v0 and subtract to make it orthogonal
  double dot01 = v0[0] * v1[0] + v0[1] * v1[1] + v0[2] * v1[2];
  v1 = Vec3f(v1[0] - dot01 * v0[0], v1[1] - dot01 * v0[1], v1[2] - dot01 * v0[2]);
  v1.normalize();

  // v2 is uniquely forced by the cross product of the first two
  Vec3f v2 = v0.cross(v1);
  v2.normalize();

  for (int i = 0; i < 3; ++i) {
    eigenvectors(i, 0) = v0[i];
    eigenvectors(i, 1) = v1[i];
    eigenvectors(i, 2) = v2[i];
  }

  return eigenvectors;
}

void TestInertiaFrame(){
  const std::string fruitFile = "F:/meshes/fruit_hand/papaya_debug.obj";
  TrigMesh mesh;
  mesh.LoadObj(fruitFile);
  RigidBody rb(mesh);
  TrigMesh inertiaMesh;

  inertiaMesh.SaveObj("F:/meshes/fruit_hand/debug_inertia_papaya.obj");
  TrigMesh roundTrip;
  roundTrip.SaveObj("F:/meshes/fruit_hand/debug_around_papaya.obj");
}
