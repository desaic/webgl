#include <iostream>
#include <vector>
#include <array>
#include <sstream>
#include <queue>

//https://www.spoj.com/problems/HOTLINE/

int sp13() {
  //int main() {
  int T;
  std::cin >> T;
  for (int t = 0; t < T; t++) {
    std::string line;
    bool endOfDialogue = false;
    while (!endOfDialogue) {
      std::getline(std::cin, line,'\n');
      if (line.back() == '!') {
        endOfDialogue = true;
        break;
      }

    }
  }
  return 0;
}
