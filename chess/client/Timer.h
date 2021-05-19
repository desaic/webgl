#ifndef TIMER_HPP
#define TIMER_HPP
#include <chrono>

class Timer {
 public:
  Timer();
  void start();
  float elapsedSeconds() const;
  auto startTime() const {
    return start_t;
  }
  bool isRunning() const { return started; }

 private:
  std::chrono::time_point<std::chrono::system_clock> start_t;
  bool started;
};

#endif
