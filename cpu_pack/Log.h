#pragma once

#include <iostream>

/// verbosity control. benchmarks run at SILENT so that per-placement
/// cout does not distort timing. loading ~944 placements at INFO
/// prints one line each.
enum LogLevel { LOG_SILENT = 0, LOG_INFO = 1, LOG_DEBUG = 2 };

void SetLogLevel(int level);
int GetLogLevel();

inline bool LogEnabled(int level) {
  return GetLogLevel() >= level;
}

// usage: LOGI("placed " << name << "\n");
#define LOGI(x)                 \
  do {                          \
    if (LogEnabled(LOG_INFO)) { \
      std::cout << x;           \
    }                           \
  } while (0)

#define LOGD(x)                  \
  do {                           \
    if (LogEnabled(LOG_DEBUG)) { \
      std::cout << x;            \
    }                            \
  } while (0)
