
//https://www.spoj.com/problems/CRYPTO4/

#include <iostream>
#include <string>
#include <vector>
#include <stdint.h>
#include <stdlib.h>
#define UNKNOWN_KEY 255
//for every char in orig compute its offset to encoded if both
//characters are available. use 255 if unavailable.
void ComputeKey(const std::string& orig, const std::string& encoded,
  std::vector<uint8_t>& key)
{
  key.resize(orig.size());
  std::fill(key.begin(), key.end(), 255);
  for (size_t i = 0; i < orig.size(); i++) {
    char c0 = orig[i];
    char c1 = encoded[i];
    //if the letter in either original or encoded string is missing
    //key can't be computed.
    if (c0 == '*' || c1 == '*') {
      continue;
    }
    if (c1 < c0) {
      c1 = c1 + 26;
    }
    uint8_t k = (c1 - c0);
    key[i] = k;
  }
}

int checkKey(const std::vector<uint8_t>& key, int keyLen,
  std::vector<uint8_t>& keyOut)
{
  keyOut.resize(keyLen, UNKNOWN_KEY);
  for (unsigned i = 0; i < keyLen; i++) {
    keyOut[i] = key[i];
  }
  for (unsigned i = keyLen; i < key.size(); i++) {
    if (key[i] == UNKNOWN_KEY) {
      continue;
    }
    unsigned keyIdx = i % keyLen;
    if (keyOut[keyIdx] != key[i]) {
      if (keyOut[keyIdx] == UNKNOWN_KEY) {
        keyOut[keyIdx] = key[i];
      }
      else {
        return -1;
      }
    }
  }
  return 0;
}

std::string Decode(const std::string& encoded, const std::vector<uint8_t>& key)
{
  std::string decoded;
  decoded.resize(encoded.size(), '*');
  for (size_t i = 0; i < encoded.size(); i++) {
    size_t keyIdx = i % key.size();
    char c0 = encoded[i];
    if (c0 == '*') {
      decoded[i] = '*';
      continue;
    }
    uint8_t k = key[keyIdx];
    if (k == UNKNOWN_KEY) {
      decoded[i] = '*';
    }
    else {
      if (c0 - 'A' < k) {
        c0 = c0 + 26;
      }
      char c1 = c0 - k;
      decoded[i] = c1;
    }
  }

  return decoded;
}

std::string ReconOrig(const std::string & orig, const std::string & encoded,
  int maxLen)
{
  //std::cout << "encoded: " << encoded << "\n";
//possible letters for each input position
//A-Z for decoded, *for unknown, !for multiple possibilities
  std::string recon = orig;
  if (maxLen > orig.size()) {
    maxLen = int(orig.size());
  }
  ///0-25 for key, 255 for unknown.
  std::vector<uint8_t> key(orig.size(), UNKNOWN_KEY);
  ComputeKey(orig, encoded, key);

  //figure out possible key lengths
  bool firstKey = true;
  for (int keyLen = 1; keyLen <= maxLen; keyLen++) {
    std::vector<uint8_t> decodeKey;
    int ret = checkKey(key, keyLen, decodeKey);
    if (ret < 0) {
      continue;
    }
    std::string candidate = Decode(encoded, decodeKey);
    
    for (size_t i = 0; i < recon.size(); i++) {
      char o = orig[i];
      if (o != '*') {
        continue;
      }
      char c = candidate[i];
      char r = recon[i];
      if (r == '!') {
        continue;
      }
      if (r == '*') {
        if (firstKey) {
          recon[i] = c;
        }
        else {
          recon[i] = '!';
        }
        continue;
      }
      if (r != c) {
        recon[i] = '!';
      }
    }
    firstKey = false;
  }
  for (size_t i = 0; i < recon.size(); i++) {
    if (recon[i] == '!') {
      recon[i] = '*';
    }
  }
  return recon;
}

int TestReconOneCase(const std::string& orig, const std::string& encoded,
  const std::vector<uint8_t>& key)
{
  float missProb = 0.5f;
  std::string missOrig = orig, missEnc = encoded;
  for (size_t i = 0; i < orig.size(); i++) {
    float r = rand() / double(RAND_MAX);
    if (r < missProb) {
      missOrig[i] = '*';
    }
    r = rand() / double(RAND_MAX);
    if (r < missProb) {
      missEnc[i] = '*';
    }
  }
  int maxLen = int(key.size() * 1.5f);
  std::string recon = ReconOrig(missOrig, missEnc, maxLen);

  for (size_t i = 0; i < recon.size(); i++) {
    if (recon[i] == '*') {
      continue;
    }
    if (recon[i] != orig[i]) {
      return -1;
    }
  }

  return 0;
}

void ReconTests()
{
  const int NUM_TESTS = 100;

  for (int t = 0; t < NUM_TESTS; t++) {
    size_t maxStrLen = 10000;
    size_t maxKeyLen = 100;
    float r = rand() / double(RAND_MAX);
    size_t len = size_t(r * maxStrLen);

    r = rand() / double(RAND_MAX);
    size_t keyLen = size_t(r * maxKeyLen);

    std::string orig, encoded;
    std::vector<uint8_t> key;
    orig.resize(len);
    encoded.resize(len);
    key.resize(keyLen);
    for (size_t i = 0; i < key.size(); i++) {
      float r = rand() / double(RAND_MAX);
      key[i] = int(r * 52) % 26;
    }

    for (size_t i = 0; i < orig.size(); i++) {
      float r = rand() / double(RAND_MAX);
      int letter = int(r * 52) % 26;
      orig[i] = char(letter % 26) + 'A';
      unsigned keyIdx = i % key.size();
      encoded[i] = 'A' + (((orig[i] - 'A') + key[keyIdx]) % 26);
    }
    for (size_t j = 0; j < 100; j++) {
      int ret = TestReconOneCase(orig, encoded, key);
      if (ret < 0) {
        std::cout << "Test failed " << orig << " " << encoded << " ";
        for (size_t i = 0; i < key.size(); i++) {
          std::cout << int(key[i]) << " ";
        }
        std::cout << "\n";
      }
    }
  }
}

int sp20()
//int main()
{
  //ReconTests();
  int t;
  std::cin >> t;
  for (int ca = 0; ca < t; ca++) {
    int maxLen;
    std::cin >> maxLen;
    std::string orig;
    std::string encoded;
    std::cin >> orig >> encoded;
    std::string recon = ReconOrig(orig, encoded, maxLen);
    std::cout << recon << "\n";
  }
  return 0;
}
