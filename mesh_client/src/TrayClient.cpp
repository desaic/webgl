#include "TrayClient.h"
#include "stl_reader.h"
#include "TrigMesh.h"
#include <iostream>
#include <thread>

enum class MeshAttr {
  UV_COORD =1,
  TEX_IMAGE_ID=2
};

enum class SceneMember
{
  NUM_MESHES = 1
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

void TrayClient::SendCommand(const MeshCommand& cmd)
{
  SendMessage((const char *)cmd.buf.data(), cmd.buf.size());
}

void TrayClient::SendMesh(unsigned short meshId, const TrigMesh * m) {
  /// don't make mesh with more than 4 billion trigs.
  unsigned nTrig = unsigned(m->GetNumTrigs());
  size_t vertBytes = sizeof(float) * nTrig * 3 * 3;
  size_t headerSize = sizeof(uint16_t) + sizeof(meshId) + sizeof(nTrig);
  size_t msgSize = headerSize + vertBytes;
  MeshCommand cmd(CommandName::MESH, msgSize);

  //mesh message structure:
  //|command name 2 bytes| mesh id 2 bytes | nTrig 8 bytes | vertices
  cmd.AddArg(meshId);
  cmd.AddArg(nTrig);

  size_t bufIdx = headerSize;
  size_t vertexSize = sizeof(float) * 3;
  for (size_t ti = 0; ti < nTrig; ti++) {
    for (size_t vi = 0; vi < 3; vi++) {
      size_t vidx = size_t(m->t[3 * ti + vi]);
      cmd.AddArgArray((const char*)(&(m->v[3 * vidx])), vertexSize);
      bufIdx += vertexSize;
    }
  }

  SendCommand(cmd);
}

void TrayClient::SendMeshes()
{
  const std::vector<TrigMesh>& meshes = scene.GetMeshes();
  for (size_t i = 0; i < meshes.size(); i++) {
    SendMesh((unsigned short)i, &meshes[i]);
  }
}

int TrayClient::GetNumMeshes()
{
  size_t msgSize = 4;
  MeshCommand cmd(CommandName::GET, msgSize);
  cmd.AddArg((uint16_t)SceneMember::NUM_MESHES);
  SendCommand(cmd);
  MeshResponse resp;
  unsigned timeoutMs = 50;
  int ret = RecvResp(resp, timeoutMs);
  if (ret == 0) {
    ret = *(int*)(resp.buf.data());
  }
  return ret;
}

int TrayClient::ParseNumMeshes(MeshResponse& resp, int timeoutMs) {
  //NUM_MESHES reponse fixed 6 bytes: 2 bytes for header
  //and 4 bytes unsigned int for num meshes.
  size_t respSize = 4;
  recvBuf.Erase(2, timeoutMs);
  resp.buf.resize(respSize);
  int ret=recvBuf.Peek((uint8_t*)(resp.buf.data()), resp.buf.size(), timeoutMs);
  if (ret == 0) {
    recvBuf.Erase(respSize, timeoutMs);
  }
  return ret;
}

int TrayClient::RecvResp(MeshResponse & resp, int timeoutMs)
{
  size_t headerSize = sizeof(resp.name);
  uint16_t header = 0;
  recvBuf.Peek((uint8_t*)&header, sizeof(header), timeoutMs);
  ResponseName name = ResponseName(header);
  resp.name = header;
  int ret = 0;
  switch (name) {
  case ResponseName::NUM_MESHES:
    ret = ParseNumMeshes(resp, timeoutMs);
    break;
  }
  return ret;
}

void TrayClient::TCPFun()
{
  int pollInterval = 10; //ms
  //interval for looking for mesh server when disconnected.
  int connectInterval = 2000;
  while (running) {
    if (!client.SocketValid()) {
      int ret = client.Connect(connectInterval);
      if (ret < 0) {
        continue;
      }
      else {
        SendMeshes();
      }
    }
    const size_t BUF_LEN = 4096;
    std::vector<uint8_t> buf(BUF_LEN);
    int64_t bytes = client.Recv((char*)buf.data(), buf.size());
    if (bytes > 0) {
      recvBuf.Insert(buf.data(), bytes);
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
  running = false;
  if (simThread.joinable()) {
    simThread.join();
  }
  if (tcpThread.joinable()) {
    tcpThread.join();
  }
  client.Close();
}

void TrayClient::SetHost(const std::string& hostname, int port)
{
  client.SetHost(hostname, port);
}

TrayClient::~TrayClient()
{
  Stop();
}