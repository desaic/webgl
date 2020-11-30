#pragma once
#include "SocketClient.h"
#include "RigidBodySim.h"

class TrayClient {
public:
  TrayClient();
  void Load();
  void Run();
  void Stop();
  void SetHost(const std::string& hostname, int port);
  void SendMesh(TrigMesh* m);
  void SendMessage(const char* buf, size_t size);

  void SimFun();
  void TCPFun();
  std::thread simThread, tcpThread;

  ~TrayClient();

  bool running;
  SocketClient client;
  RigidBodySim sim;
};