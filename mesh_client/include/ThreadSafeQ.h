#ifndef THREAD_SAFE_Q_H
#define THREAD_SAFE_Q_H
#include <mutex>
#include <deque>

class ThreadSafeQ
{
public:

  std::mutex mtx;
  std::deque<uint8_t> buf;
  std::condition_variable cv;

  int Erase(size_t numBytes, uint32_t timeoutMs);

  int Peek(uint8_t* dst, size_t numBytes, uint32_t timeoutMs);

  void Insert(uint8_t* data, size_t len);

  size_t GetSize();

};

#endif