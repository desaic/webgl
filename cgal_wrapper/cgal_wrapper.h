#pragma once

struct mesh_ptr {
  float* v = 0;
  unsigned* t = 0;
  unsigned nv = 0;
  unsigned nt = 0;
  //must be called manually on output mesh
  void free();
};

/// <summary>
/// caller manages the pointers.
/// </summary>
/// <param name="ma"></param>
/// <param name="mb"></param>
/// <param name="out">allocated by this function with new[].</param>
/// <returns></returns>
int mesh_diff(mesh_ptr ma, mesh_ptr mb, mesh_ptr& out);
int mesh_union(mesh_ptr ma, mesh_ptr mb, mesh_ptr& out);