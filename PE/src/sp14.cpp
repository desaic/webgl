#include <iostream>
#include <vector>
#include <sstream>
#include <set>
//https://www.spoj.com/problems/IKEYB/

unsigned getOptIdx(unsigned key, unsigned pos, unsigned numKeys) {
  return key + pos * numKeys;
}

void ArrangeKeys(const std::vector<int> & freq, std::vector<std::vector<int> > & keys)
{
  unsigned numLetters = freq.size();
  unsigned numKeys = keys.size();
  //indexed by letter,  keyIdx + posInKey * numKeys.
  std::vector<std::vector<int> > opt;
  std::vector<std::vector<int> > prev;

  //first letter always in first spot.
  keys[0].push_back(0);
  if (freq.size() <= 1) {
    return;
  }

  if (keys.size() <= 1) {
    for (size_t j = 1; j < numLetters; j++) {
      keys[0].push_back(j);
    }
    return;
  }
  
  opt.resize(freq.size());
  opt[0].push_back(freq[0]);
  prev.resize(freq.size());
  prev[0].push_back(0);

  for (size_t i = 1; i < freq.size(); i++) {
    int f = freq[i];
    for (size_t k = 0; k < opt[i-1].size(); k++) {
      int prevOpt = opt[i-1][k];
      //impossible combo.
      if (prevOpt < 0) {
        continue;
      }
      unsigned prevKey = k % numKeys;
      unsigned prevPos = k / numKeys;
      if (prevKey < numKeys - 1) {
        //start using a new key
        unsigned newKey = prevKey + 1;
        unsigned newPos = 0;
        unsigned linIdx = newKey;
        if (linIdx >= opt[i].size()) {
          opt[i].resize(linIdx + 1, -1);
        }
        int newVal = prevOpt + f;
        if (opt[i][linIdx] < 0 || newVal <= opt[i][linIdx]) {
          opt[i][linIdx] = prevOpt + f;
          if (linIdx >= prev[i].size()) {
            prev[i].resize(linIdx + 1, -1);
          }
          prev[i][linIdx] = k;
        }
      }
      //try appending to the same key
      unsigned newKey = prevKey;
      unsigned newPos = prevPos + 1;
      unsigned linIdx = getOptIdx(newKey, newPos, numKeys);
      if (linIdx >= opt[i].size()) {
        opt[i].resize(linIdx + 1, -1);
      }
      opt[i][linIdx] = prevOpt + f * (newPos + 1);

      if (linIdx >= prev[i].size()) {
        prev[i].resize(linIdx + 1, -1);
      }
      prev[i][linIdx] = k;
    }
  }

  unsigned last = freq.size() - 1;
  unsigned minIdx = 0;
  int minVal = -1;
    
  for (size_t i = 0; i < opt[last].size(); i++) {
    int o = opt[last][i];
    if (o < 0) {
      continue;
    }
    if (o <= minVal || minVal < 0) {
      minVal = o;
      minIdx = i;
    }
  }

  for (size_t l = opt.size() - 1; l >= 1; l--) {
    unsigned k = minIdx % numKeys;
    unsigned pos = minIdx / numKeys;
    if (pos >= keys[k].size()) {
      keys[k].resize(pos + 1, '?');
    }    
    keys[k][pos] = l;
    minIdx = prev[l][minIdx];
  }
}

int sp14() {
//int main() {
  int T;
  std::cin >> T;
  for (int t = 0; t < T; t++) {
    int numKeys, numAlphabet;
    std::cin >> numKeys;
    std::cin >> numAlphabet;
    std::string keys, alphabet;
    std::getline(std::cin, keys);
    std::getline(std::cin, keys);
    std::getline(std::cin, alphabet);
    std::vector<int> freq(alphabet.size(), 0);
    for (unsigned letter = 0; letter < alphabet.size(); letter++) {
      std::cin >> freq[letter];
    }
    std::cout << "Keypad #" << (t + 1) << ":\n";
    std::vector<std::vector<int> > keyMap;
    keyMap.resize(keys.size());
    ArrangeKeys(freq, keyMap);
    for (size_t k = 0; k < keyMap.size(); k++) {
      std::cout << keys[k] << ": ";
      for (size_t l = 0; l < keyMap[k].size(); l++) {
        std::cout << alphabet[keyMap[k][l]];
      }
      std::cout << "\n";
    }
    std::cout << "\n";
  }
  return 0;
}
