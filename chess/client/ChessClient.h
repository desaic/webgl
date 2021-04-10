#include "SocketClient.h"
#include "ChessBoard.h"

#include <deque>
#include <iostream>

void LogFun(const std::string& msg, LogLevel level);

class ChessClient {
public:
  ChessClient():IP("localhost"),
    port(9001){
  }

  ~ChessClient() {
    Stop();
    if (loopThread.joinable()) {
      loopThread.join();
    }
  }

  void Init() {
    int ret = sock.Connect(IP, port);
    if (ret < 0) {
      std::cout << "failed connect.\n";
    }
    sock.SetLogCallback(&LogFun);
    state = DISCONNECTED;
    loopThread = std::thread(&ChessClient::Loop, this);
  }

  void Stop(){
    state = STOP;
  }

  void HandleCmd(const std::string& cmd);

  void SendCmd(const std::string& command);

  void Loop();

  SocketClient sock;
  enum State {
    STOP,
    DISCONNECTED,
    RUNNING
  };
  State state;
  std::string IP;
  int port;
  std::mutex qLock;
  std::deque<char> q;

  ChessBoard board;
  std::thread loopThread;
};
