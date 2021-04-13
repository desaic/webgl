#include <WinSock2.h>
#include "ChessClient.h"
#include <iostream>
#include <mutex>
#include <vector>
#include <string>
#include <sstream>

void LogFun(const std::string& msg, LogLevel level)
{
  std::cout << level << " " << msg << "\n";
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

std::vector<std::string> split(const std::string& str)
{
  std::vector<std::string>tokens;
  std::istringstream iss(str);
  bool fail = false;
  while (!fail) {
    std::string token;
    iss >> token;
    fail = iss.fail();
    if (token.size() > 0) {
      tokens.push_back(token);
    }
  }
  return tokens;
}

void ChessClient::HandleCmd(const std::string& cmd) {
  std::cout << "received: " << cmd << "\n";
  std::vector<std::string> tokens;
  tokens = split(cmd);
  if (tokens.size() == 0) {
    return;
  }
  if (tokens[0] == "move") {
    //check if move is legal.
    Move attempt;
    if (tokens.size() >= 3) {
      attempt.src.Set(tokens[1]);
      attempt.dst.Set(tokens[2]);
    }
    if (tokens.size() >= 4) {
      attempt.SetPromo(tokens[3][0]);
    }
    bool legal = false;
    std::vector<Move> moves = board.GetMoves();
    for (auto m : moves) {
      if (m == attempt) {
        legal = true;
        break;
      }
    }
    if (!legal) {
      std::cout << "illegal move " + attempt.toString() + "\n";
    }
    else {
      board.ApplyMove(attempt);
    }
    SendBoard();
  }
  else if (tokens[0] == "fen") {
    SendBoard();
  }
}

void ChessClient::HandleCli(const std::string& cmd)
{
  if (cmd == "start") {
    board.SetStartPos();
    SendBoard();
  }
  else {
    SendCmd(cmd);
  }
}

void ChessClient::SendBoard()
{
  std::string fen = board.GetFen();
  std::string cmd = "fen " + fen;
  SendCmd(cmd);
}

void ChessClient::SendCmd(const std::string& command)
{
  std::string fullCmd = ">" + command + "\r\n";
  sock.SendAll(fullCmd.data(), uint32_t(fullCmd.size()));
}

void ChessClient::Loop()
{

  int interval = 30;
  int connInterval = 2000;
  while (state != STOP) {
    int ret;
    std::vector<char> buf;
    const size_t BUFLEN = 1024;
    buf.resize(BUFLEN);

    switch (state) {
    case DISCONNECTED:
      ret = sock.Connect(IP, port, connInterval);
      if (ret < 0) {
        //std::cout << "connect failed " << ret << "\n";
      }
      else {
        state = RUNNING;
        SendBoard();
      }
      break;
    case RUNNING:
      int64_t len = sock.Recv(buf.data(), BUFLEN, 50);
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
      HandleCmd(cmd);
    }

    std::this_thread::sleep_for(std::chrono::microseconds(interval));
  }
}
