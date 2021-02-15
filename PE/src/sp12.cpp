#include <iostream>
#include <vector>
#include <array>
#include <sstream>
#include <queue>

//https://www.spoj.com/problems/MMIND/

struct Score {
  //correct value at correct position
  int exact; 
  //this digit is present
  int present;
  Score() :exact(0), present(0) {
  }
  std::string toString() {
    std::stringstream oss;
    oss << exact << " " << present;
    return oss.str();
  }
};

struct DataPoint{
  std::vector<int> guess;
  Score s;
};

Score EvalGuess(const std::vector<int>& truth, const std::vector<int> & guess)
{
  Score s;
  std::vector<uint8_t> used(truth.size(), 0);
  for (unsigned int i = 0; i < truth.size(); i++) {
    if (truth[i] == guess[i]) {
      used[i] = 1;
      s.exact++;
    }
  }
  for (unsigned int i = 0; i < guess.size(); i++) {
    if (truth[i] == guess[i]) {
      continue;
    }
    for (unsigned int j = 0; j < truth.size(); j++) {
      if (j == i) {
        continue;
      }
      if (used[j]) {
        continue;
      }
      if (guess[i] == truth[j]) {
        used[j] = 1;
        s.present++;
        break;
      }
    }
  }
  return s;
}

Score EvalPartial(const std::vector<int>& partial, const std::vector<int>& guess)
{
  Score s;
  std::vector<uint8_t> used(partial.size(), 0);
  for (unsigned int i = 0; i < partial.size(); i++) {
    if (partial[i] == guess[i]) {
      used[i] = 1;
      s.exact++;
    }
  }
  for (unsigned int i = 0; i < guess.size(); i++) {
    if (i<partial.size() && partial[i] == guess[i]) {
      continue;
    }
    for (unsigned int j = 0; j < partial.size(); j++) {
      if (j == i) {
        continue;
      }
      if (used[j]) {
        continue;
      }
      if (guess[i] == partial[j]) {
        used[j] = 1;
        s.present++;
        break;
      }
    }
  }
  return s;
}

///is partial sequence compatible with the guess and the score
bool compatible(const std::vector<int>& partial, 
  const std::vector<int>& guess,const Score& s)
{
  Score partialScore = EvalPartial(partial, guess);
  //too many matches
  if (partialScore.exact > s.exact) {
    return false;
  }
  int missingExact = s.exact - partialScore.exact;
  if (partialScore.present - missingExact > s.present) {
    return false;
  }
  int openSpots = guess.size() - partial.size();
  //too few matches
  if (partialScore.exact + openSpots < s.exact) {
    return false;
  }
  openSpots = openSpots + partialScore.exact - s.exact;

  if (partialScore.present + openSpots < s.present) {
    return false;
  }
  return true;
}

///is partial sequence compatible with the guess and the score
bool compatible(const std::vector<int>& partial,
  const std::vector<DataPoint> & d)
{
  for (size_t i = 0; i < d.size(); i++) {
    bool c = compatible(partial, d[i].guess, d[i].s);
    if (!c) {
      return false;
    }
  }
  return true;
}

///@return -1 if no more to increment
int increment(std::vector<int>& v, int base)
{
  int i = int(v.size()) - 1;
  bool carry = true;
  while (carry && i>=0) {
    v[i] ++;
    if (v[i] >= base) {
      v[i] = 0;
      i--;
    }
    else {
      carry = false;
    }
  }
  if (i < 0) {
    return -1;
  }
  return 0;
}

void TestEval()
{
  std::vector<int> partial(3, 0);
  std::vector<int> guess(4, 1);

  partial[0] = 2;
  partial[1] = 1;
  partial[2] = 1;
  guess[0] = 1;
  guess[1] = 3;
  guess[2] = 5;
  guess[3] = 8;
  Score s = EvalPartial(partial, guess);
  std::vector<int> truth(4, 1);
  truth[0] = 6;
  truth[1] = 0;
  truth[2] = 7;
  truth[3] = 4;

  guess[0] = 1;
  guess[1] = 3;
  guess[2] = 5;
  guess[3] = 8;
  s = EvalGuess(truth, guess);
  std::cout << s.toString() << "\n";

}

void TestIncrement(std::vector<int> & n, int base)
{
  int ret = increment(n, base);
  for (unsigned i = 0; i < n.size(); i++) {
    std::cout << n[i] << " ";
  }
  std::cout << "\n";
  std::cout << "has more : " << ret << "\n";
}

void TestInc()
{
  std::vector<int> n(3, 9);
  TestIncrement(n, 10);
  n.resize(4);
  n[0] = 9;
  n[1] = 10;
  n[2] = 9;
  n[3] = 10;
  TestIncrement(n, 11);
}

int
DFSGuess(std::vector<int> & partial, int numDigit, int maxDigit, const std::vector<DataPoint>& d)
{
  bool c = compatible(partial, d);
  if (!c) {
    return -1;
  }
  if (partial.size() == numDigit) {
    return 0;
  }
  for (int i = 0; i < maxDigit; i++) {
    std::vector<int> v = partial;
    v.push_back(i);
    int ret = DFSGuess(v, numDigit, maxDigit, d);
    if (ret == 0) {
      partial = v;
      return 0;
    }
  }
  return -1;
}

std::vector<int>
DFSGuess(int numDigit, int maxDigit, const std::vector<DataPoint> & d)
{
  std::vector<int> v(1,0);
  for (int i = 0; i < maxDigit; i++) {
    v[0] = i;
    int ret = DFSGuess(v, numDigit, maxDigit, d);
    if (ret == 0) {
      return v;
    }
  }
  return std::vector<int>(0);
}

int sp12() {
//int main() {
  //TestEval();
  int numCases;
  std::cin >> numCases;
  for (int c = 0; c < numCases; c++) {
    int numDigit;
    int maxDigit;
    int numGuess;
    std::cin >> numDigit >> maxDigit >> numGuess;
    std::vector<DataPoint> d(numGuess);
    for (int l = 0; l < numGuess; l++) {
      d[l].guess.resize(numDigit);
      for (int i = 0; i < numDigit; i++) {
        std::cin >> d[l].guess[i];
        //from 1-based to 0-based;
        d[l].guess[i]--;
      }
      std::cin >> d[l].s.exact;
      std::cin >> d[l].s.present;
    }
    std::vector<int> v = DFSGuess(numDigit, maxDigit, d);
    if (v.size() == 0) {
      std::cout << "You are cheating!\n";
    }
    else {
      for (unsigned i = 0; i < v.size(); i++) {
        //output 1-based
        std::cout << (v[i]+1);
        if (i + 1 < v.size()) {
          std::cout << " ";
        }
      }
      std::cout << "\n";
    }
  }
  return 0;
}
