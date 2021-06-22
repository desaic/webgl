#include "TrayClient.h"
#include "ConfigFile.hpp"
#include "tests.h"
#include "heap.hpp"
#include "LineIter2D.h"
#include "TrigIter2D.h"
#include "Array2D.h"
#include "BBox.h"
#include "ZTetSlicer.h"

#include <iostream>
#include <thread>
#include <WinSock2.h>

int initWSA() {
  WSADATA wsaData;
  int iResult;
  iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (iResult != 0) {
    std::cout << "WSAStartup failed: " << iResult << "\n";
    return 1;
  }
  std::cout << "WSAStartup done " << iResult << "\n";
  return 0;
}

void CommandLoop(TrayClient * client) {
  while (1) {
    std::cout << "> ";
    std::string line;
    std::getline(std::cin, line);
    if (line == "q" || line == "exit") {
      break;
    }
    else if (line == "test") {
      TestCPT(client);
    }
    else if (line == "testpng") {
      TestPng();
    }
    if (line.size() < 2) {
      continue;
    }
  }
}

void LoadTestScene(TrayClient & client)
{
  Scene& scene = client.GetScene();
  
  TrigMesh mesh;
  ///\todo change to config instead of hardcoded.
  std::string meshFile = "F:/dolphin/meshes/tiltedSlab.stl";
  int status = mesh.LoadStl(meshFile);
  int meshId = scene.AddMesh(mesh);
  Vec3f rot(0, 0, 0);
  Vec3f initPos(0, 0, 10);
  int instanceId = scene.AddInstance(meshId, initPos, rot);
}

void TestHeap() {
  heap h;
  h.insert(0.1, 0);
  h.insert(0.2, 1);
  h.insert(0.4, 2);
  h.insert(0.5, 3);

  h.update(0.6, 1);
  h.printTree();
}

void TestLineIter() {
  std::vector<Vec2f> v(3);
  v[0]= Vec2f(1.5, 2);
  v[1]= Vec2f(5.7, -3.7);
  v[2]= Vec2f(-4.1, -3.7);
  TrigIter2D trigIter(v[0], v[1], v[2]);
  BBox2D box;
  box.Compute(v);
  Vec2<int> origin( int(std::round(box.mn[0]) ), int( std::round(box.mn[1]) ) );
  Vec2<int> topRight(int(std::round(box.mx[0])), int(std::round(box.mx[1])));
  Vec2<int> size = topRight - origin + Vec2<int>(1, 1);
  Array2D<uint8_t> img(size[0], size[1]);
  img.Fill(0);

  while (trigIter()) {
    int x = trigIter.x();
    int y = trigIter.y();
    std::cout << "("<<x<<", "<<y<<") ";
    img(x - origin[0], y - origin[1]) = 1;
    ++trigIter;
  }
  std::cout << "\n";
  for (size_t row = 0; row < img.GetSize()[1]; row++) {
    for (size_t col = 0; col < img.GetSize()[0]; col++) {
      std::cout << int(img(col, row)) << " ";
    }
    std::cout << "\n";
  }

  std::cout << "\n";
}

int main(){
  TestLineIter();
  initWSA();
  ///\todo add a config file.
  ConfigFile conf;
  
  int port = 9001;
  TrayClient client;
  LoadTestScene(client);
  client.SetHost("localhost", port);
  client.RunTCPThread();
  CommandLoop(&client);
  client.Stop();

  return 0;	
}
