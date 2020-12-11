#pragma once

#include "SocketClient.h"
#include "TrigMesh.h"
#include "Scene.h"

class TrayClient {
public:
  
  TrayClient();
  ~TrayClient();

  void RunTCPThread();
  void Stop();
  void SetHost(const std::string& hostname, int port);
  void SendMesh(TrigMesh* m);
  ///sends most recent scene to render server
  void SendScene();
  void SendMessage(const char* buf, size_t size);

  void SimFun();
  void TCPFun();
  std::thread simThread, tcpThread;  

  Scene & GetScene() { return scene; }

private:
  bool running;
  SocketClient client;
  Scene scene;
};