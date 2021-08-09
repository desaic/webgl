#include "SocketClient.h"
#include "ChessBoard.h"
#include "ChessBot.h"

#include <deque>
#include <iostream>

void LogFun(const std::string& msg, LogLevel level);

class ChessClient {
public:
  ChessClient():IP("localhost"),
    port(9001){
    board.SetStartPos();
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
    bot.Run();
  }

  void Stop(){
    state = STOP;
  }

  ///send current board position to ui server.
  void SendBoard();

  void HandleCmd(const std::string& cmd);

  ///handle cli commands on the chess client.
  void HandleCli(const std::string& cmd);

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
  ChessBot bot;
  std::vector<UndoMove> undoStack;
  std::thread loopThread;
};
