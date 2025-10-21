#include "TrigMesh.h"
#include "TriangulateContour.h"
#include "Transformations.h"

#define JSMN_HEADER
#include "jsmn.h"
#include <iostream>
#include <fstream>
#include "Vec3.h"
#include "BBox.h"

struct JsonData{
  std::string str;
  std::vector<jsmntok_t> t;
  int numTokens = 0;  
};

struct Instance {
  std::string fileName;
  BBox box;
  Vec3f pos;
  Vec3f rot;
};

JsonData LoadJson(std::string & filename) {
  std::ifstream in;
  in.open(filename);
  JsonData j;
  if (!in.good()) {
    return j;
  }
  const size_t MAX_TOKENS = 10000;
  jsmn_parser parser;
  jsmn_init(&parser);
  j.t.resize(MAX_TOKENS);

  in.seekg(0, std::ios::end);
  j.str.resize(in.tellg());
  in.seekg(0, std::ios::beg);
  in.read(&j.str[0], j.str.size());
  in.close();

  int r =
      jsmn_parse(&parser, j.str.data(), j.str.size(), j.t.data(), j.t.size());
  if (r < 0) {
    std::cout << "Failed to parse JSON: " << filename << " return code " << r
              << "\n";
    return j;
  }

  /* Assume the top-level element is an object */
  if (r < 1 || j.t[0].type != JSMN_OBJECT) {
    std::cout << "config does not begin with an object\n";
    return j;
  }
  j.numTokens = r;
  return j;
}

Vec3f ParseVec3f(const JsonData& j, int tok_index) {
  Vec3f v;
  for (int dim = 0; dim < 3; dim++) {
    const auto * tok = &j.t[tok_index+dim];
    std::string val(&j.str[tok->start], tok->end - tok->start);
    v[dim] = std::stod(val);    
  }
  return v;
}

void SaveInstances(const std::string & filename, const std::vector<Instance> & instances) {
  std::ofstream out(filename);
  for (size_t i = 0; i < instances.size(); i++) {
    out << instances[i].fileName <<"\n";
    out << instances[i].pos[0] << " " << instances[i].pos[1] << " "
        << instances[i].pos[2] <<"\n";
    out << instances[i].rot[0] << " " << instances[i].rot[1] << " "
        << instances[i].rot[2] << "\n";

  }

}

void TestBLD() {
  std::string bldFile = "F:/meshes/skeleton/left_and_center/build.json";
  JsonData j = LoadJson(bldFile);
  int mmp_token_index = -1;
  size_t skipTo = 0;
  // The root object is token 0. We start searching from token 1.
  for (int i = 1; i < j.numTokens; i++) {
    if (j.t[i].start < skipTo) {
      //skip unneeded objects.
      continue;
    }
    std::string key(&j.str[j.t[i].start], j.t[i].end - j.t[i].start);
    std::cout << key << "\n";
    if (key == "mmp") {
      mmp_token_index = i;
      break;
    } else {
      skipTo = j.t[i].end;
    }    
  }
  jsmntok_t* mmp_obj = &j.t[mmp_token_index + 1];
  size_t numInst = mmp_obj->size;
  std::vector<Instance> instances(numInst);
  int instances_token_index = mmp_token_index + 2;
  int objectEnd = 0;
  int inst_tok_index = instances_token_index;
  for (int index = 0; index < mmp_obj->size; index++) {
    jsmntok_t* instance_obj = &j.t[inst_tok_index];
    objectEnd = instance_obj->end;
    
    for (int fieldIndex = inst_tok_index + 1; fieldIndex < j.numTokens;
         fieldIndex++) {
      jsmntok_t* tok = &j.t[fieldIndex];
      std::string key(&j.str[tok->start],
                      tok->end - tok->start);
      if (key == "fileName") {
        fieldIndex ++;
        tok = &j.t[fieldIndex];
        std::string val(&j.str[tok->start], tok->end - tok->start);
        instances[index].fileName = val;        
      } else if (key == "instance") {
        fieldIndex++;
        tok = &j.t[fieldIndex];
        int instanceEnd = tok->end;
        //ignore "ARRAY" and assume 1 instance per mmp.
        fieldIndex+=2;
        for (; fieldIndex < j.numTokens; fieldIndex++) {
          tok = &j.t[fieldIndex];
          int propertyStart = tok->start;
          if (propertyStart >= instanceEnd) {
            fieldIndex--;
            break;
          }
          key= std::string(&j.str[tok->start], tok->end - tok->start);
          if (key == "boundingBox") {
            BBox box;
            fieldIndex++;
            tok = &j.t[fieldIndex];
            int boxEnd = tok->end;
            fieldIndex++;
            for (; fieldIndex < j.numTokens; fieldIndex++) {
              tok = &j.t[fieldIndex];
              int tokStart = tok->start;
              if (tokStart >= boxEnd) {
                fieldIndex--;
                break;
              }
              key = std::string(&j.str[tok->start], tok->end - tok->start);
              if (key == "max") {
                fieldIndex += 2;
                box.vmax = ParseVec3f(j, fieldIndex);
                fieldIndex += 2;
              } else if (key == "min") {
                fieldIndex += 2;
                box.vmin = ParseVec3f(j, fieldIndex);
                fieldIndex += 2;
              } 
            }
            instances[index].box = box;
          } else if (key == "position") {
            fieldIndex += 2;
            instances[index].pos = ParseVec3f(j, fieldIndex);
            fieldIndex += 2;     
          } else if (key == "rotation") {
            fieldIndex += 2;     
            instances[index].rot = ParseVec3f(j, fieldIndex);
            fieldIndex += 2;     
          } else {
            fieldIndex++;
            tok = &j.t[fieldIndex];
            fieldIndex += tok->size;
          }
        }
      }
      //leaving an mmp object
      if (j.t[fieldIndex].end >= objectEnd) {
        inst_tok_index = fieldIndex;
        break;
      }
    }    
  }
  std::cout << instances_token_index << "\n";
  
  const float INPUT_MESH_SCALE = 10.0f;
  const std::string meshDir = "F:/meshes/skeleton/left/";
  const std::string meshOutDir = "F:/meshes/skeleton/leftOut/";
  
  //move instances to center around z plane.
  //move obj meshes so that its origin is bounding box center
  for (size_t i = 0; i < instances.size(); i++) {
    auto& inst = instances[i]; 
    std::string name = inst.fileName;
    size_t strPos = name.find('.');
    name = name.substr(0, strPos);
    std::cout << "prefix " << name << "\n";
    std::string objIn = meshDir + name + ".obj";
    TrigMesh mesh;
    mesh.LoadObj(objIn);
    mesh.scale(INPUT_MESH_SCALE);
    Box3f box = ComputeBBox(mesh.v);
    Vec3f dx = -0.5f * (box.vmax+box.vmin);
    // move mesh so that its bounding box is centered.
    mesh.translate(dx[0], dx[1], dx[2]);
    std::string objOut = meshOutDir + name + ".obj";
    mesh.SaveObj(objOut);
    
    TrigMesh rotated = mesh;
    rotateVerts(mesh.v, rotated.v, inst.rot[0], inst.rot[1], inst.rot[2]);
    Box3f rotBox = ComputeBBox(rotated.v);
    inst.pos[0] -= rotBox.vmin[0];
    inst.pos[1] -= rotBox.vmin[1];
    inst.pos[2] -= rotBox.vmin[2];
    //convert instance position from frontend coordinates to world coordinates
    //move instance so that its z coordinate is 0.
    
    //inst.pos[2] = 0;
    inst.fileName = name;
  }

  SaveInstances("F:/meshes/skeleton/left_pos.txt" ,instances);
}

