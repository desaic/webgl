#include "ChessBoard.h"
#include "ChessBot.h"
#include "ChessClient.h"
#include <iostream>
#include <map>
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
  bot.SearchMoves();
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

void ApplyMove(ChessBoard & board, Move m, std::vector<UndoMove> & undoStack) {
  UndoMove u = board.GetUndoMove(m);
  board.ApplyMove(m);
  undoStack.push_back(u);
}

void TestMove() {
  std::string fen = "8/1p4q1/p4pkp/P6Q/3P4/5K2/8/8 b - - 19 54";
  ChessBoard cb;
  cb.FromFen(fen);
  std::vector<Move> moves=cb.GetMoves();
  for (size_t i = 0; i < moves.size(); i++) {
    std::cout << moves[i].src.ToString() << " " << moves[i].dst.ToString()<<" ";
  }
}

struct MoveCounts {
  size_t moves = 0;
  size_t captures = 0;
  size_t castles = 0;
  size_t checks = 0;
  size_t mates = 0;
  std::string ToString() {
    std::stringstream oss;
    oss << "moves " << moves << ", caps " << captures << ", castles " << castles << ", checks "
        << checks<<", mates "<<mates;
    return oss.str();
  }
};

std::string MoveToString(const ChessBoard& b, const Move& m) {
  Piece p = b.board[m.src.coord];
  std::string s;
  std::string name = "";
  std::string PieceName[PIECE_NUM_TYPES] = {"", "", "R", "N", "B", "Q", "K"};
  name = PieceName[p.type()];
  std::string dst = m.dst.ToString();
  return name+dst;
}

void EnumerateMoves(ChessBoard& board, int depth, std::vector<MoveCounts> & moveStats)
{
  if (depth == 0) {
    return;
  }
  std::vector<Move> moves = board.GetMoves();
  if (board.IsInCheck()) {
    moveStats[depth-1].checks++;
  }
  moveStats[depth - 1].moves += moves.size();
  if (moves.size() == 0) {
    if (board.IsInCheck()) {
      moveStats[depth - 1].mates++;
    }
  }
  for (size_t i = 0; i < moves.size(); i++) {
    UndoMove u = board.GetUndoMove(moves[i]);
    const Move& m = moves[i];
    Piece p = board.board[moves[i].dst.coord];
    bool isEnpassant = false;
    Piece srcp = board.board[moves[i].src.coord];
    if (srcp.type() == PIECE_PAWN && p.isEmpty() && 
      m.src.Col() != m.dst.Col()) {
      isEnpassant = true;
    }
    if ((!p.isEmpty())||isEnpassant) {
      moveStats[depth - 1].captures++;
    }
    board.ApplyMove(moves[i]);
    EnumerateMoves(board, depth - 1, moveStats);
    board.Undo(u);
  }
}

void TestMovePerft() { 
  ChessBoard b;
  b.SetStartPos();
  b.FromFen("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -");
  
  int depth = 5;
  std::vector<MoveCounts>moveStats(depth);
  EnumerateMoves(b, depth, moveStats);
  for (size_t i = 0; i < moveStats.size(); i++) {
    std::cout << "depth " << (i + 1) << " " << moveStats[i].ToString() << "\n";
  }
}

void TestHash()
{
  ChessBoard board;
  BoardHash::Init();

  board.SetStartPos();
  board.InitHash();

  uint64_t h = board.HashVal();
  std::cout << h << "\n";
  Move m1("e2", "e4");
  Move m2("e7", "e5");
  Move m3("d2", "d4");

  UndoMove u = board.GetUndoMove(m1);
  std::vector<UndoMove> undoStack;
  ApplyMove(board, m1, undoStack);
  ApplyMove(board, m2, undoStack);
  ApplyMove(board, m3, undoStack);
  uint64_t h0 = board.HashVal();
  std::cout << h0 << "\n";
  board.Undo(undoStack[2]);
  board.Undo(undoStack[1]);
  board.Undo(undoStack[0]);

  board.ApplyMove(m3);
  board.ApplyMove(m2);
  board.ApplyMove(m1);
  uint64_t h1 = board.HashVal();
  std::cout << "h0 == h1 " << int(h0 == h1) << " " << h0 << " " << h1 << "\n";

  // test sequence 2
  board.SetStartPos();
  board.InitHash();
  m1 = Move("e2", "e4");
  m2 = Move("a7", "a6");
  m3 = Move("e4", "e5");
  Move m4 = Move("f7", "f5");

  board.ApplyMove(m1);
  board.ApplyMove(m2);
  board.ApplyMove(m3);
  board.ApplyMove(m4);
  h0 = board.HashVal();

  board.SetStartPos();
  board.InitHash();
  m1 = Move("e2", "e3");
  m2 = Move("a7", "a6");
  m3 = Move("e3", "e4");
  m4 = Move("f7", "f6");
  Move m5 = Move("e4", "e5");
  Move m6 = Move("f6", "f5");

  board.ApplyMove(m1);
  board.ApplyMove(m2);
  board.ApplyMove(m3);
  board.ApplyMove(m4);
  board.ApplyMove(m5);
  board.ApplyMove(m6);
  h1 = board.HashVal();
  std::cout << "h0 != h1 " << int(h0 != h1) << " " << h0 << " " << h1 << "\n";

  //test sequence 
  m1 = Move("e2", "e3");
  m2 = Move("a7", "a5");
  m3 = Move("e3", "e4");

  board.ApplyMove(m1);
  board.ApplyMove(m2);
  board.ApplyMove(m3);
  h0 = board.HashVal();


}
