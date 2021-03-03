#include "TrayClient.h"
#include "ConfigFile.hpp"

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
  std::cout << "WSAStartup done " << iResult << "\n";;
  return 0;
}

void TestNumMeshes(TrayClient* client)
{
  int numMeshes = client -> GetNumMeshes();
  std::cout << "num meshes " << numMeshes << "\n";
}

void TestMesh(TrayClient* client)
{
  int numMeshes = client->GetNumMeshes();
  std::cout << "num meshes " << numMeshes << "\n";
  if (numMeshes == 0) {
    return;
  }
  const int iter = 100;
  Scene& scene = client->GetScene();
  std::vector<TrigMesh>& meshes = scene.GetMeshes();
  if (meshes.size() == 0) {
    return;
  }
  std::vector<float> verts = meshes[0].verts;
  for (int i = 0; i < iter; i++) {
    for (size_t j = 0; j < meshes[0].verts.size(); j++) {
      meshes[0].verts[j] = verts[j] * (1 + i / float(iter));
    }
    client->SendMeshes();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  meshes[0].verts = verts;
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
      TestMesh(client);
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
  std::string meshFile = "F:/homework/threejs/meshes/Eiffel.stl";
  int status = mesh.LoadStl(meshFile);

  int meshId = scene.AddMesh(mesh);
  Vec3 rot(0, 0, 0);
  Vec3 initPos(0, 0, 10);
  int instanceId = scene.AddInstance(meshId, initPos, rot);
}

int main(){
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
