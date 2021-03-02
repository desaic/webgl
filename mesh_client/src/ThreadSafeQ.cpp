#include "ThreadSafeQ.h"


int ThreadSafeQ::Erase(size_t numBytes, uint32_t timeoutMs) {
  std::unique_lock<std::mutex> lk(mtx);
  bool success = cv.wait_for(lk, std::chrono::milliseconds(timeoutMs),
    [this, numBytes] {return buf.size() >= numBytes; });
  if (success) {
    buf.erase(buf.begin(), buf.begin() + numBytes);
    return 0;
  }
  else {
    //time out
    return -1;
  }
}

int ThreadSafeQ::Peek(uint8_t* dst, size_t numBytes, uint32_t timeoutMs) {
  std::unique_lock<std::mutex> lk(mtx);
  bool success = cv.wait_for(lk, std::chrono::milliseconds(timeoutMs),
    [this, numBytes] {return buf.size() >= numBytes; });
  if (success) {
    for (size_t i = 0; i < numBytes; i++) {
      dst[i] = buf[i];
    }
  }
  else {
    return -1;
  }
  return 0;
}

void ThreadSafeQ::Insert(uint8_t* data, size_t len) {
  std::unique_lock<std::mutex> lk(mtx);
  buf.insert(buf.end(), data, data + len);
  cv.notify_all();
}

size_t ThreadSafeQ::GetSize() {
  std::unique_lock<std::mutex> lk(mtx);
  return buf.size();
}
