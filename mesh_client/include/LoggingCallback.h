/// @file LoggingCallback.h
/// @author Desai Chen (desaic@inkbit3d.com)
///
/// @date 2020-05-21
///
/// @todo(mike) don't use this for new code - this should probably be moved to a
/// loggin lib whenever that gets solidifed

#ifndef LOGGING_CALLBACK_H
#define LOGGING_CALLBACK_H

#include <functional>
#include <string>

enum LogLevel {
  LOG_DEBUG = 1,
  LOG_WARNING = 2,
  LOG_ERROR = 4,
  LOG_INFO = 8
};

using LoggingCallback =
    std::function<void(const std::string &msg, LogLevel level)>;

#endif
