#include <WinSock2.h>
#include "ChessBoard.h"
#include "SocketClient.h"
#include <iostream>
#include <mutex>
#include <deque> 
#include <vector>

SocketClient client;

enum State {
  STOP,
  DISCONNECTED,
  RUNNING
};

State state;
std::string IP = "localhost";
int port = 9001;

std::mutex qLock;
std::deque<char> q;

void LogFun(const std::string& msg, LogLevel level)
{
  std::cout<<level<<" " << msg << "\n";
}

///\return 0 on success. -1 on error.
int parseMsg(std::deque<char>& q, std::string& cmd) {
  cmd.clear();
  
  bool hasStartOfLine = false;
  //find start of a command. throw away anything before that.
  while (!q.empty()) {
    char c = q.front();
    if (c != '>') {
      q.pop_front();
    }
    else {
      q.pop_front();
      hasStartOfLine = true;
      break;
    }
  }
  if (!hasStartOfLine) {
    return -1;
  }

  bool hasEndOfLine = false;
  size_t i = 0;
  for (; i < q.size(); i++) {
    char c = q[i];
    if (c == '\n') {
      hasEndOfLine = true;
      break;
    }
  }
  if (hasEndOfLine && i > 0) {
    cmd.insert(cmd.end(), q.begin(), q.begin() + i);
    q.erase(q.begin(), q.begin() + i);
  }
  return 0;
}

void handleCmd(const std::string& cmd) {
  std::cout << "received: " << cmd << "\n";
}

void ClientLoop() {
  
  int interval = 30;
  int connInterval = 2000;
  while (state != STOP) {
    int ret;
    std::vector<char> buf;
    const size_t BUFLEN=1024;
    buf.resize(BUFLEN);

    switch (state){
    case DISCONNECTED: 
      ret = client.Connect(IP, port, connInterval);
      if (ret < 0) {
        //std::cout << "connect failed " << ret << "\n";
      }
      else {
        state = RUNNING;
      }
      break;
    case RUNNING:
      int64_t len = client.Recv(buf.data(), BUFLEN, 50) ;
      if (len < 0) {
        if (len != -WSAETIMEDOUT) {
          state = DISCONNECTED;
          std::cout << "sock disconnect\n";
        }
      }
      else if (len > 0) {
        qLock.lock();
        q.insert(q.end(), buf.begin(), buf.begin() + len);
        qLock.unlock();
      }
      break;
    }

    std::string cmd;
    ret = 0;
    {
      std::lock_guard<std::mutex> lock(qLock);
      ret = parseMsg(q, cmd);
    }
    if (ret == 0) {
      handleCmd(cmd);
    }
    
    std::this_thread::sleep_for(std::chrono::microseconds(interval));
  }
}
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

void SendCommand(const std::string & command) {
  std::string fullCmd = ">" + command + "\r\n";
  client.SendAll(fullCmd.data(), uint32_t(fullCmd.size()));
}

void TestFEN()
{
  ChessBoard board;
  Piece p;
  p.SetType(PieceType::PAWN);
  p.SetColor(PieceColor::WHITE);
  board.AddPiece(0, 1, p);
  board.AddPiece(0, 2, p);
  board.AddPiece(0, 3, p);
  std::string fen = board.GetFen();
  std::cout << fen << "\n";

  std::string inputFen = "5r1k/2r5/p2qp1Q1/1p1n4/3P4/P4P2/1P1B1P1P/6RK w - - 25 367";
  board.FromFen(inputFen);
  std::string outFen = board.GetFen();
  std::cout << "out fen " << outFen << "\n";
}

int main(int argc, char* argv[])
{
  TestFEN();
  initWSA();
  int ret = client.Connect(IP, port);
  if (ret < 0) {
    std::cout << "failed connect.\n";
  }
  client.SetLogCallback(&LogFun); 

  state = STOP;

  state = DISCONNECTED;
  std::thread clientThread(&ClientLoop);
  

  int pollInterval = 50;  // ms;
  while (state != STOP) {
    std::this_thread::sleep_for(std::chrono::milliseconds(pollInterval));
    std::string command;
    std::getline(std::cin, command);
    if (command.size() > 3) {
      SendCommand(command);
    }else if (command == "q") {
      state = STOP;
      break;
    }
  }
  clientThread.join();
  return 0;
}
