#include "Timer.h"

Timer::Timer() : start_t{}, started(false) {}

void Timer::start() {
  started = true;
  start_t = std::chrono::system_clock::now();
}

float Timer::elapsedSeconds() const {
  std::chrono::time_point<std::chrono::system_clock> t =
      std::chrono::system_clock::now();
  std::chrono::duration<double> diff = t - start_t;
  return diff.count();
}
