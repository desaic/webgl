#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <math.h>
#include "Vec3.h"
#include "Vec2.h"

int LoadOBJFile(std::string filename, std::vector<uint32_t>& t_ref, std::vector<float>& v_ref, std::vector<Vec2f>& uv_ref) {
  // If the file is not an .obj file return false
  if (filename.substr(filename.size() - 4, 4) != ".obj") return false;

  std::ifstream file(filename);

  if (!file.is_open()) return false;

  std::vector<uint32_t> t;
  std::vector<float> v;
  std::vector<Vec2f> uv_orderless;
  std::vector<Vec2f> uv;
  std::vector<int> uv_t;
  std::string curline;

  while (std::getline(file, curline)) {
    auto iss = std::istringstream(curline);
    std::string first_token;
    iss >> first_token;
    if (first_token == "v") {
      for (int i = 0; i < 3; i++) {
        float vf;
        iss >> vf;
        v.push_back(vf);
      }
    } 
    else if (first_token == "f") {
      for (int i = 0; i < 3; i++) {
        std::string idxs;
        iss >> idxs;
        std::istringstream idxss(idxs);

        std::string idx;
        std::getline(idxss, idx, '/');
        t.push_back(std::stoi(idx) - 1);

        //different uv indices - reorders later
        std::getline(idxss, idx, '/');
        if (idx != "") {
          uv_t.push_back(std::stoi(idx) - 1); 
        }
      }
    } 
    else if (first_token == "vt") {
      Vec2f add;
      float vf;
      iss >> vf;
      add[0] = vf;
      iss >> vf;
      add[1] = vf;
      uv_orderless.push_back(add);
    }
  }

  //order uvs properly
  for (int uv_idx : uv_t) {
    uv.push_back(uv_orderless[uv_idx]);
  }

  t_ref = t;
  v_ref = v;
  uv_ref = uv;
  return 0;
}