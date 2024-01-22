#include "cgal_wrapper.h"
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Surface_mesh.h>
#include <CGAL/Polygon_mesh_processing/corefinement.h>
#include <CGAL/Polygon_mesh_processing/remesh.h>
#include <iostream>
typedef CGAL::Exact_predicates_inexact_constructions_kernel K;
typedef CGAL::Surface_mesh<K::Point_3>                      Mesh;

void copy_mesh_ptr_to_cgal_mesh(mesh_ptr mp, Mesh& mesh) {
  for (size_t i = 0; i < mp.nv; i++) {
    auto index = mesh.add_vertex(K::Point_3(mp.v[3 * i], mp.v[3 * i + 1], mp.v[3 * i + 2]));
  }
  for (size_t i = 0; i < mp.nt; i++) {
    mesh.add_face(CGAL::SM_Vertex_index(mp.t[3 * i]), CGAL::SM_Vertex_index(mp.t[3 * i + 1]), CGAL::SM_Vertex_index(mp.t[3 * i + 2]));
  }
}

mesh_ptr copy_from_cgal_mesh(Mesh& mesh) {
  mesh_ptr mp;
  mp.nv = mesh.num_vertices();
  mp.nt = mesh.num_faces();
  Mesh::Vertex_range r = mesh.vertices();
  Mesh::Vertex_range::iterator  vb, ve;
  vb = r.begin();
  ve = r.end();
  mp.v = new float[mp.nv * 3];
  size_t vCount = 0;
  for (auto vit = vb; vit != ve; vit++) {
    auto pt = mesh.point(*vit);
    mp.v[3 * vCount] = float(pt.x());
    mp.v[3 * vCount+1] = float(pt.y());
    mp.v[3 * vCount+2] = float(pt.z());
    vCount++;
  }
  mp.t = new unsigned[3 * mp.nt];
  unsigned tCount = 0;

  for (auto fit : mesh.faces()) {
    CGAL::Vertex_around_face_circulator<Mesh> vcirc(mesh.halfedge(fit), mesh), done(vcirc);
    unsigned j = 0;
    do {
      unsigned idx = unsigned(*vcirc);
      mp.t[3 * tCount + j] = idx;
      vcirc++;
      j++;
    } while (vcirc != done  && j<3);

    tCount++;
  }
  return mp;
}

int mesh_diff(mesh_ptr ma, mesh_ptr mb, mesh_ptr& out) {
  Mesh tm1;
  Mesh tm2;
  Mesh mesh_out;
  copy_mesh_ptr_to_cgal_mesh(ma, tm1);
  copy_mesh_ptr_to_cgal_mesh(mb, tm2);   
  bool success = CGAL::Polygon_mesh_processing::corefine_and_compute_difference(tm1,
    tm2,
    mesh_out);
  out = copy_from_cgal_mesh(mesh_out);
  return 0;
}

int mesh_union(mesh_ptr ma, mesh_ptr mb, mesh_ptr& out) {
  Mesh tm1;
  Mesh tm2;
  Mesh mesh_out;
  copy_mesh_ptr_to_cgal_mesh(ma, tm1);
  copy_mesh_ptr_to_cgal_mesh(mb, tm2);
  bool success = CGAL::Polygon_mesh_processing::corefine_and_compute_union(tm1,
    tm2,
    mesh_out);
  out = copy_from_cgal_mesh(mesh_out);
  return 0;
}

void mesh_ptr::free() {
  if (v) {
    delete v;
    v = nullptr;
  }
  if (t) {
    delete[]t;
    t = nullptr;
  }
}