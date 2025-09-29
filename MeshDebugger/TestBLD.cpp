#include "TrigMesh.h"
#include "TriangulateContour.h"
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

void TestBLD() {
  std::string bldFile = "F:/meshes/skeleton/center_bld/build.json";
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
      if (key == "filename") {
        instances[index].fileName;
      }
      if (j.t[fieldIndex].end >= objectEnd) {
        inst_tok_index = fieldIndex; 
        break;
      }
    }    
  }
  std::cout << instances_token_index << "\n";
  
}

