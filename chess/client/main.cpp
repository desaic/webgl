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

void TestFEN()
{
  ChessBoard board;
  Piece p;
  p.SetType(PIECE_PAWN);
  p.SetColor(PIECE_WHITE);
  board.AddPiece(ChessCoord(0, 1), p);
  board.AddPiece(ChessCoord(0, 2), p);
  board.AddPiece(ChessCoord(0, 3), p);
  std::string fen = board.GetFen();
  std::cout << fen << "\n";

  std::string inputFen = "5r1k/2r5/p2qp1Q1/1p1n4/3P4/P4P2/1P1B1P1P/6RK w - - 25 367";
  board.FromFen(inputFen);
  std::string outFen = board.GetFen();
  std::cout << "out fen " << outFen << "\n";
}

void TestEvalDirect() {
  ChessBoard board;
  ChessBot bot;
  std::string inputFen = "r3r1k1/p1p2ppp/3q1n2/1p3b2/4PN2/3p1P2/PP3KPP/1RB1Q2R b - - 0 17";
  board.FromFen(inputFen);
  std::cout << "score:" << bot.EvalDirect(board) << "\n";
}

void TestEvalWithSearch()
{
  std::cout << "TestEvalWithSearch()\n";
  ChessBoard board;
  std::string inputFen = "r3r1k1/p1p2ppp/3q1n2/1p3b2/4PN2/3p1P2/PP3KPP/1RB1Q2R b - - 0 17";
  board.FromFen(inputFen);
  ChessBot bot;
  bot.SetBoard(board);
  bot.SetMaxDepth(4);
  bot.InitEval();
  
  size_t numSteps = 1000000;
  for (size_t s = 0; s < numSteps; s++) {
    if (s == 35599) {
      std::cout << "debug step " << s << "\n";
    }
    int ret = bot.EvalStep();
    if (ret < 0) {
      break;
    }
  }
  Move bestMove = bot.CurrentBestMove();
  std::cout << bestMove.ToString() << " \n";
}

int main(int argc, char* argv[])
{
  TestFEN();
  TestEvalWithSearch();

  initWSA();
 
  ChessClient client;
  client.Init();

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
