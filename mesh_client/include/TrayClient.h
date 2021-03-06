#pragma once

#include "SocketClient.h"
#include "TrigMesh.h"
#include "Scene.h"
#include "ThreadSafeQ.h"

enum class CommandName {
  MESH = 1, TRIGS = 2,
  ATTR, TEXTURE,
  TRANS, GET
};

enum class ResponseName
{
  NUM_MESHES=1
};

struct MeshCommand
{
  MeshCommand(CommandName c, size_t len) :
    cmd(unsigned short(c)), bufIdx(0)
  {
    buf.resize(len);
    AddArg(cmd);
  }

  template<typename T>
  void AddArg(const T& a) {
    *((T*)(&buf[bufIdx])) = a;
    bufIdx += sizeof(T);
  }

  /// @param dataPtr data source.
  void AddArgArray(const char * dataPtr,size_t byteCnt) {
    std::memcpy(&buf[bufIdx], dataPtr, byteCnt);
    bufIdx += byteCnt;
  }

  unsigned short cmd;
  std::vector<uint8_t> buf;
  size_t bufIdx;
};

/// response from mesh server
struct MeshResponse
{
  unsigned short name;
  std::vector<uint8_t> buf;
};

class TrayClient {
public:
  
  TrayClient();
  ~TrayClient();

  void RunTCPThread();
  void Stop();
  void SetHost(const std::string& hostname, int port);
  void SendMesh(unsigned short meshId, const TrigMesh* m);
  ///sends all meshes render server
  ///blocks until sending finished
  void SendMeshes();
  void SendMessage(const char* buf, size_t size);
  void SendCommand(const MeshCommand& cmd);
  void SimFun();
  void TCPFun();
  ///takes bytes off
  int RecvResp(MeshResponse & resp, int timeoutMs);
  int ParseNumMeshes(MeshResponse& resp, int timeoutMs);
  int GetNumMeshes();
  //wait response
  std::thread simThread, tcpThread;  

  Scene & GetScene() { return scene; }

private:
  bool running;
  SocketClient client;
  Scene scene;

  ThreadSafeQ recvBuf;
};