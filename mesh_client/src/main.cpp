#include <iostream>
#include <WinSock2.h>
#include "TrayClient.h"

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

void CommandLoop() {
  while (1) {
    std::cout << "> ";
    std::string line;
    std::getline(std::cin, line);
    if (line == "q" || line == "exit") {
      break;
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
  std::string meshFile = "F:/homework/threejs/meshes/Eiffel.stl";
  int status = mesh.LoadStl(meshFile);

  int meshId = scene.AddMesh(mesh);
  Vec3 rot(0, 0, 0);
  Vec3 initPos(0, 0, 10);
  int instanceId = scene.AddInstance(meshId, initPos, rot);
}

int main(){
  initWSA();
  int port = 9001;
  TrayClient client;
  LoadTestScene(client);
  client.SetHost("localhost", port);
  client.RunTCPThread();
  client.SendScene();
  CommandLoop();
  client.Stop();

  return 0;	
}
