#include <iostream>
#include <vector>
#include <array>
#include <sstream>
#include <queue>

bool isVar(char c) {
  return c >= 'a' && c <= 'z';
}

bool isOp(char c)
{
  return (c == '+') || (c == '-') || (c == '*') || (c == '/');
}

const char ROOT_SYMBOL = '@';

class ExprNode
{
public:
  char c;
  std::array<ExprNode*,2> child;
  ExprNode() :c('?') ,
    child( { nullptr,nullptr }) {}
  ExprNode ( char symbol): c(symbol),
    child({ nullptr,nullptr }) {}
};

void FreeChildren(ExprNode* n)
{
  for (size_t i = 0; i < 2; i++) {
    if (n->child[i] != nullptr) {
      FreeChildren(n->child[i]);
      delete n->child[i];
      n->child[i] = nullptr;
    }
  }
}

void PrintSpace(int count) {
  std::string s(count, ' ');
  std::cout << s;
}

void Print(ExprNode * root, int level) {
  PrintSpace(level);
  std::cout << "|";
  char c = root->c;
  std::cout << c << "\n";
  if (isVar(c)) {
  }
  else if (isOp(c)) {
    Print(root->child[0], level + 1);
    Print(root->child[1], level + 1);
  }
  else {
    //is parenthesis
    Print(root->child[0], level + 1);
  }
}

void PrintFlat(ExprNode* root) {
  char c = root->c;
  if (c == ROOT_SYMBOL) {
    PrintFlat(root->child[0]);
  }else if (isVar(c)) {
    std::cout << c;
  }
  else if (isOp(c)) {
    PrintFlat(root->child[0]);
    std::cout << c ;
    PrintFlat(root->child[1]);
  }
  else {
    //is parenthesis
    std::cout << "(";
    PrintFlat(root->child[0]);
    std::cout << ")";
  }
}

int OpPrecedence(char op) {
  if (op == '*' || op == '/') {
    return 2;
  }
  return 1;
}

///reduce binary ops.
///\return false if there is no bin op.
bool ReduceBinOp(std::vector<ExprNode*>& stack)
{
  if (stack.size() < 3) {
    return false;
  }
  size_t lastIdx = stack.size() - 1;
  ExprNode* opNode = stack[lastIdx - 1];
  if (!isOp(opNode->c)) {
    return false;
  }
  opNode->child[0] = stack[lastIdx - 2];
  opNode->child[1] = stack[lastIdx];
  stack.pop_back();
  stack.pop_back();
  stack.pop_back();
  stack.push_back(opNode);
  return true;
}

void AddExpr(std::vector<ExprNode*>& stack, ExprNode* node, char nextc) {
  if (stack.empty()) {
    stack.push_back(node);
    return;
  }
  ExprNode* stackNode = stack.back();

  //only op or '(' can precede
  if (stackNode->c == '(') {
    stack.push_back(node);
    return;
  }

  int stackPrec = OpPrecedence(stackNode->c);

  if (nextc == ')') {
    stack.push_back(node);
    return;
  }
  //only op or ')' can follow
  int nextPrec = OpPrecedence(nextc);
  stack.push_back(node);
  while (stackPrec >= nextPrec) {
    ReduceBinOp(stack);
    if (stack.size() < 2) {
      break;
    }
    ExprNode* opNode = stack[stack.size() - 2];
    if (!isOp(opNode->c)) {
      break;
    }
    stackPrec = OpPrecedence(opNode->c);
  }
}

void ParseVar(std::vector<ExprNode*> & stack, char var, char nextc) 
{
  ExprNode* node = new ExprNode(var);
  AddExpr(stack, node, nextc);
}

void ParseOp(std::vector<ExprNode*>& stack, char op, char nextc)
{
  ExprNode* node = new ExprNode(op);
  stack.push_back(node);
}

///removes node at child idx,
///adopts grand child
void DeleteParenthesis(ExprNode * parent, unsigned childIdx)
{
  ExprNode* child = parent->child[childIdx];
  while (child->c == '(') {
    child = child->child[0];
    delete parent->child[childIdx];
    parent->child[childIdx] = child;
  }
}

void DeleteAllParen(ExprNode*node){
  char c = node->c;
  if (isVar(c) ){
    return;
  }
  if (isOp(c)) {
    int prec = OpPrecedence(c);

    ExprNode* child = node->child[0];
    if (child->c == '(') {
      DeleteParenthesis(child, 0);
      ExprNode* grandChild = child->child[0];
      if (isVar(grandChild->c)) {
        DeleteParenthesis(node, 0);
      }
      else {
        int grandPrec = OpPrecedence(grandChild->c);
        if (prec <= grandPrec) {
          DeleteParenthesis(node, 0);
        }
      }
    }
    DeleteAllParen(node->child[0]);

    child = node->child[1];
    if (child->c == '(') {
      DeleteParenthesis(child, 0);
      ExprNode* grandChild = child->child[0];
      if (isVar(grandChild->c)) {
        DeleteParenthesis(node, 1);
      }
      else {
        char grandc = grandChild->c;
        int grandPrec = OpPrecedence(grandc);
        if (prec < grandPrec ) {
          DeleteParenthesis(node, 1);
        }
        else if (prec == grandPrec && c != '-' && c != '/') {
          DeleteParenthesis(node, 1);
        }
      }
    }
    DeleteAllParen(node->child[1]);
    return;
  }
  if (c == '(') {
    DeleteAllParen(node->child[0]);
    return;
  }
  if (c == ROOT_SYMBOL) {
    DeleteParenthesis(node, 0);
    DeleteAllParen(node->child[0]);
  }
}

ExprNode ParseExpr(const std::string& line)
{
  ExprNode root(ROOT_SYMBOL);
  std::vector<ExprNode*> stack;
  for (size_t i = 0; i < line.size(); i++) {
    if (line[i] == '\n') {
      break;
    }
    if (i == 18) {
      //std::cout << "debug\n";
    }
    char c = line[i];
    char nextc = '\0';
    if (i < line.size() - 1) {
      nextc = line[i + 1];
    }
    if (isVar(c)) {
      ParseVar(stack, c, nextc);
    }
    else if (isOp(c)) {
      ParseOp(stack, c, nextc);
    }
    else if(c=='('){
      ExprNode* node = new ExprNode(c);
      stack.push_back(node);
    }
    else if (c == ')') {
      while (ReduceBinOp(stack)) {}
      ExprNode* innerExpr = stack.back();
      stack.pop_back();
      ExprNode* parenExpr = stack.back();
      parenExpr->child[0] = innerExpr;
      stack.pop_back();
      AddExpr(stack, parenExpr, nextc);
    }
    else {
      std::cout << "unknown character.\n";
    }
  }
  //while (ReduceBinOp(stack)) {}
  root.child[0] = stack.back();
  return root;
}

//int sp10() {
int main() {
  int numCases;
  std::cin >> numCases;
  for (int c = 0; c < numCases; c++) {
    std::string line;
    std::cin >> line;
    //std::cout << c << " " << line << "\n";
    ExprNode root = ParseExpr(line);
    //Print(&root, 0);
    //PrintFlat(&root);
    //std::cout << "\n";
    DeleteAllParen(&root);
    PrintFlat(&root);
    std::cout << "\n";
    FreeChildren(&root);    
  }
  return 0;
}
