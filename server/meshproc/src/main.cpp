#include <iostream>
#include <WinSock2.h>
#include "TrayClient.h"

int initWSA() {
  WSADATA wsaData;
  int iResult;
  iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (iResult != 0) {
    std::cout << "WSAStartup failed: " << iResult << "\n";
    return 1;
  }
  std::cout << "WSAStartup done " << iResult << "\n";;
  return 0;
}

int main(){
  initWSA();
  int port = 9001;
  TrayClient client;
  client.Load();
  client.SetHost("localhost", port);
  client.Run();


  while (1) {
    std::cout << "> ";
    std::string line;
    std::getline(std::cin, line);
    if (line == "q" || line == "exit") {
      break;
    }
    if (line.size() < 2) {
      continue;
    }
  }
  client.Stop();

  return 0;	
}
