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

void TestEvalWithSearch(std::string fen)
{
  ChessBoard board;
  board.FromFen(fen);
  ChessBot bot;
  bot.SetBoard(board);
  bot.SetMaxDepth(5);
  bot.InitEval();

  size_t numSteps = 10000000;
  for (size_t s = 0; s < numSteps; s++) {
    int ret = bot.EvalStep();
    if (ret < 0) {
      break;
    }
  }
  Move bestMove = bot.CurrentBestMove();
  std::cout << bestMove.ToString() << " \n";

}

void TestEvalWithSearch()
{
  std::cout << "TestEvalWithSearch()\n";

  std::string inputFen = "r3r1k1/p1p2ppp/3q1n2/1p3b2/4PN2/3p1P2/PP3KPP/1RB1Q2R b - - 0 17";
  std::string inputFenWhite = "r3r1k1/p1p2ppp/3q4/1p3b2/4nN2/3p1P2/PP3KPP/1RB1Q2R w - - 0 18";
  std::string fen3Black = "r3r1k1/p1p2ppp/3q4/1p3b2/4PN2/3p4/PP3KPP/1RB1Q2R b - - 0 18";
  std::string fen4White = "r5k1/p1p2ppp/3q4/1p3b2/4rN2/3p4/PP3KPP/1RB1Q2R w - - 0 19";
  std::string fen4WhiteAdvantage = "r3r1k1/p1p2ppp/8/1p3b2/4Pq2/3p4/PP3KPP/1RB1Q2R w - - 0 19";
  std::string fen5Check = "r3r1k1/p1p2ppp/8/1p3b2/4Pq2/8/PP1p1KPP/1RB1Q1R1 w - - 0 20";
  
  std::string fen6 = "2k1r3/1ppb3Q/3p4/p2P1p2/2P4P/PP1B1PP1/6K1/4q3 b K - 8 28";
  std::string fen7 = "1k2r3/1ppb3Q/3p4/p2P1p2/2P4P/PP1B1PK1/8/8 w K - 11 30";
  //TestEvalWithSearch(inputFen);
  TestEvalWithSearch(fen6);
  //TestEvalWithSearch(fen4);
}

void TestHash()
{
  ChessBoard board;
  BoardHash::Init();

  board.SetStartPos();
  board.InitHash();
  
  uint64_t h = board.HashVal();
  std::cout << h << "\n";
  Move m1("e2","e4");
  Move m2("e7", "e5");
  Move m3("d2", "d4");

  UndoMove u = board.GetUndoMove(m1);
  std::vector<UndoMove> undoStack;
  board.ApplyMove(m1);
  board.ApplyMove(m2);
  board.ApplyMove(m3);
  std::cout << board.HashVal() << "\n";

  board.SetStartPos();
  board.InitHash();
  std::cout << board.HashVal() << "\n";
  board.ApplyMove(m3);
  board.ApplyMove(m2);
  board.ApplyMove(m1);
  std::cout << board.HashVal() << "\n";
}

int main(int argc, char* argv[])
{
 
  TestHash();
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
