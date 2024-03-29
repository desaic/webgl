#include <WinSock2.h>
#include "ChessBoard.h"
#include "ChessBot.h"
#include "ChessClient.h"
#include <iostream>
#include <iomanip>
#include <mutex>
#include <vector>
#include <string>
#include <sstream>

extern void TestMovePerft();

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

extern void TestHash();
extern void TestMove();
int main(int argc, char* argv[]) {
  //TestMove();
  //TestMovePerft();
  BoardHash::Init();
  std::cout << sizeof(ChessBoard) << "\n";
  std::cout << sizeof(Move) << "\n";
  initWSA(); 
  ChessClient client;
  client.Init();
  client.HandleCli("start");
  int pollInterval = 50;  // ms;
  while (1) {
    std::this_thread::sleep_for(std::chrono::milliseconds(pollInterval));
    std::string command;
    std::getline(std::cin, command);
    if (command == "q") {
      break;
    }else if (command.size() > 0) {
      client.HandleCli(command);
    }
  }
  return 0;
}
