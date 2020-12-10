#include "TrayClient.h"
#include "stl_reader.h"
#include "TrigMesh.h"
#include <iostream>
#include <thread>

enum class MessageType {
  MESH = 1, STATE = 2
};

TrayClient::TrayClient():running(false)
{}

void TrayClient::Load()
{
  std::string meshFile = "F:/homework/threejs/meshes/Eiffel.stl";
  std::vector<unsigned > solids;
  TrigMesh* mesh = new TrigMesh();
  bool success = false;
  try{
    success = stl_reader::ReadStlFile(meshFile.c_str(), mesh->verts, mesh->normals, mesh->trigs, solids);
  }
  catch(...){
  }
  if (!success) {
    std::cout << "error reading stl file " <<meshFile<<"\n";
    return;
  }
 
  //test non trivial message
  SendMesh(mesh);
}

void TrayClient::SimFun()
{
}

void TrayClient::SendMessage(const char * buf, size_t size)
{
  std::string header = "message_size ";
  header = header + std::to_string(size) + "\r\n";
  client.Send(header.c_str(), header.size());
  client.Send(buf, size);
}

void TrayClient::SendMesh(TrigMesh * m) {
  //message type
  int type = int(MessageType::MESH);
  size_t nTrig = m->trigs.size();
  size_t vertBytes = sizeof(float) * nTrig * 3 * 3;
  size_t msgSize = sizeof(type) + sizeof(nTrig) + vertBytes;
  //type 4bytes | nTrig 8 bytes| vertices
  std::vector<unsigned char> buf(msgSize);
  *(int*)(&buf[0]) = type;
  *(size_t*)(&buf[4]) = nTrig;
  SendMessage((char*)buf.data(), buf.size());
}

void TrayClient::TCPFun()
{
  int pollInterval = 10; //ms
  while (running) {
    std::this_thread::sleep_for(std::chrono::milliseconds(pollInterval));
  }
}

void TrayClient::Run()
{
  int ret = client.Connect();
  if (ret < 0) {
    std::cout << "failed to connect error " << ret << "\n";
  }
  running = true;
  if (!simThread.joinable()) {
    simThread = std::thread(&TrayClient::SimFun, this);
  }
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