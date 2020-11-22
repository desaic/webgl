#include "TrayClient.h"
#include "stl_reader.h"
#include "TrigMesh.h"
#include <iostream>
#include <thread>

TrayClient::TrayClient():running(false)
{}

void TrayClient::Load()
{
  //std::string meshFile = "F:/homework/meshes/Eiffel.stl";
  //std::vector<unsigned > solids;
  //TrigMesh * mesh = new TrigMesh();
  //bool success = stl_reader::ReadStlFile(meshFile.c_str(), mesh->verts, mesh->normals, mesh->trigs, solids);
  //if (!success) {
  //  std::cout << "error reading stl file.\n";
  //  return;
  //}
  //sim.meshes.push_back(mesh);

  sim.Load();
}

void TrayClient::SimFun()
{
  int NUM_STEPS = 200;
  for (size_t i = 0; i < NUM_STEPS; i++) {
    sim.Step();
    if (!running) {
      break;
    }
  }
}

void TrayClient::SendMessage(const char * buf, size_t size)
{
  std::string header = "message_size ";
  header = header + std::to_string(size) + "\r\n";
  client.Send(header.c_str(), header.size());
  client.Send(buf, size);
}

void TrayClient::TCPFun()
{
  int pollInterval = 10; //ms
  std::string msg = "hello";
  SendMessage(msg.c_str(), msg.size());

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