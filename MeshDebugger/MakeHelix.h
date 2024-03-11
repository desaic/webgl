#pragma once
#ifndef MAKE_HELIX_H
#define MAKE_HELIX_H

#include "TrigMesh.h"

struct HelixSpec {
  //#6-32 UNC
  float inner_diam = 2.56;
  float outer_diam = 3.505;
  float length = 5;
  float pitch = 0.795;
  float inner_width = 0.695;
  float outer_width = 0.1;
  //divisions per revolution
  unsigned divs = 36;
  bool rod = true;
  bool tube = false;
};

int MakeHelix(const HelixSpec& spec, TrigMesh& mesh);
#endif