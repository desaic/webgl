#include "TrayClient.h"
#include "stl_reader.h"
#include "TrigMesh.h"
#include <iostream>
#include <thread>

enum class CommandName {
  MESH = 1, TRIGS = 2,
  ATTR, TEXTURE,
  TRANS
};

enum class MeshAttr {
  UV_COORD =1,
  TEX_IMAGE_ID=2
};

void LogStdOut(const std::string& msg, LogLevel level) {
  std::cout << "Sock client: " << msg << "\n";
}

TrayClient::TrayClient():running(false)
{
  client.SetLogCallback(LogStdOut);
}

void TrayClient::SimFun()
{
}

void TrayClient::SendMessage(const char * buf, size_t size)
{
  std::string header = "message_size ";
  header = header + std::to_string(size) + "\r\n";
  client.Send(header.c_str(), uint32_t(header.size()) );
  client.Send(buf, uint32_t(size) );
}

void TrayClient::SendMesh(const TrigMesh * m) {
  //message type
  unsigned short cmd = unsigned short(CommandName::MESH);
  unsigned short meshId = 1;
  /// don't make mesh with more than 4 billion trigs.
  unsigned nTrig = unsigned(m->GetNumTrigs());
  size_t vertBytes = sizeof(float) * nTrig * 3 * 3;
  size_t headerSize = sizeof(cmd) + sizeof(meshId) + sizeof(nTrig);
  size_t msgSize = headerSize + vertBytes;
  //mesh message structure:
  //|command name 2 bytes| mesh id 2 bytes | nTrig 8 bytes | vertices
  std::vector<unsigned char> buf(msgSize);
  *(short*)(&buf[0]) = cmd;
  *(short*)(&buf[2]) = meshId;
  *(unsigned*)(&buf[4]) = nTrig;

  size_t bufIdx = headerSize;
  size_t vertexSize = sizeof(float) * 3;
  for (size_t ti = 0; ti < nTrig; ti++) {
    for (size_t vi = 0; vi < 3; vi++) {
      size_t vidx = size_t(m->trigs[3 * ti + vi]);
      std::memcpy(&buf[bufIdx], &m->verts[3 * vidx], vertexSize);
      bufIdx += vertexSize;
    }
  }

  SendMessage((char*)buf.data(), buf.size());
}

void TrayClient::SendMeshes()
{
  const std::vector<TrigMesh>& meshes = scene.GetMeshes();
  for (size_t i = 0; i < meshes.size(); i++) {
    SendMesh(&meshes[i]);
  }
}

void TrayClient::TCPFun()
{
  int pollInterval = 10; //ms
  //interval for looking for mesh server when disconnected.
  int connectInterval = 2000;
  while (running) {
    if (!client.Connected()) {
      int ret = client.Connect();
      if (ret < 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(connectInterval));
        continue;
      }
      else {
        SendMeshes();
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(pollInterval));
  }
}

void TrayClient::RunTCPThread()
{
  running = true;
  if (!tcpThread.joinable()) {
    tcpThread = std::thread(&TrayClient::TCPFun, this);
  }
}

void TrayClient::Stop()
{
  client.Close();
  running = false;
  if (simThread.joinable()) {
    simThread.join();
  }
  if (tcpThread.joinable()) {
    tcpThread.join();
  }
}

void TrayClient::SetHost(const std::string& hostname, int port)
{
  client.SetHost(hostname, port);
}

TrayClient::~TrayClient()
{
  Stop();
}