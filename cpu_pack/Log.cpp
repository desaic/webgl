#include "Log.h"

static int g_logLevel = LOG_INFO;

void SetLogLevel(int level) {
  g_logLevel = level;
}

int GetLogLevel() {
  return g_logLevel;
}
