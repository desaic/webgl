#include "TrayClient.h"
#include "ConfigFile.hpp"
#include "tests.h"

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

int main(){
  TestTrigIter();
  TestTetSlicer();
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
