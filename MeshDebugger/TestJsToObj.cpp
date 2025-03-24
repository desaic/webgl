#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
void JsToObj() { 
  std::string indexFile = "F:/meshes/motor_air/shipIdx.txt";
  std::string vertFile = "F:/meshes/motor_air/shipVert.txt";
  std::ifstream in(indexFile);
  std::string line;
  bool startGather = false;
  std::vector<unsigned> t;
  while (std::getline(in, line)) {
    
    if (line.find("array") != line.npos) {
      startGather = true;
      continue;
    }
    if(startGather){
      std::istringstream iss(line);
      int idx = 0;
      if (iss >> idx) {
        t.push_back(idx);
      }
    }
  }
  std::cout << t[0] << " " << t.back() << "\n";
  in.close();
  in.open(vertFile);
  startGather = false;
  std::vector<float> v;
  while (std::getline(in, line)) {
    if (line.find("array") != line.npos) {
      startGather = true;
      continue;
    }
    if (startGather) {
      std::istringstream iss(line);
      float f = 0;
      if (iss >> f) {
        v.push_back(f);
      }
    }
  }
  std::cout << v[0] << " " << v.back() << "\n";
  in.close();

  std::ofstream out("F:/meshes/motor_air/out.obj");
  for (size_t i = 0; i < v.size(); i += 3) {
    out << "v " << v[i] << " " << v[i + 1] << " " << v[i + 2] << "\n";
  }
  for (size_t i = 0; i < t.size(); i += 3) {
    out << "f " << (1+t[i]) << " " << (1+t[i + 1]) << " " << (1+t[i + 2]) << "\n";
  }
}