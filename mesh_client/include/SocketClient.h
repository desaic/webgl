/// @file SocketClient.h
/// @author Desai Chen (desaic@inkbit3d.com)
/// @author Mike Hebert (mike@inkbit3d.com)
///
/// @date 2020-05-21
///
/// Class for creating client TCP connections.

#ifndef SOCKET_CLIENT_H
#define SOCKET_CLIENT_H

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

#include "LoggingCallback.h"

/// @todo(mike) provide automatic reconnect capabilities
/// @todo(mike) add some hierarchy; add UDP sockets and listener TCP sockets
/// @todo(mike) combine with WinsockServer.h in the scanner code
/// @todo(mike) combine with TcpClient in the GIS code
class SocketClient {
  static const long DEFAULT_TIMEOUT_MS = 500;

public:
  /// Construct a new Socket Client
  SocketClient();

  /// Closes the socket
  ~SocketClient();

  /// @return <0 on error. number of bytes sent on success.
  int64_t Send(const char* buf, uint32_t nBytes,
    long timeout_ms = DEFAULT_TIMEOUT_MS);

  /// Either sends nBytes with successives Send() calls, or errors.
  /// @return <0 on error. number of bytes sent on success.
  int64_t SendAll(const char* buf, uint32_t nBytes,
    long timeout_ms = DEFAULT_TIMEOUT_MS);

  void SetHost(const std::string h, uint16_t p) {
    _hostname = h;
    _port = p;
  }
  /// @return <0 on error. number of bytes read on success.
  /// @param buf out parameter for the message
  /// @param len length of the buffer / number of bytes to recv
  /// @param timeout_ms if 0, blocks until a message is received or an error
  /// occurs
  int64_t Recv(char *buf, uint32_t len, long timeout_ms = 0);

  /// @return true if the socket is readable/listening.
  /// @param timeout_ms how long to wait for it
  bool WaitReadable(long timeout_ms = DEFAULT_TIMEOUT_MS);

  /// @return true if the socket is writeable/connected.
  /// @param timeout_ms how long to wait for it
  bool WaitWriteable(long timeout_ms = DEFAULT_TIMEOUT_MS);

  /// connect to server and run receiving loop in a new thread. If already
  /// connected, do nothing.
  /// @todo(mike) what happens if you give this a different hostname/port than
  /// we're currently connect to?
  /// @return socket api error code error. 0 on success or already connected
  virtual int Connect(long timeout_ms = 0);

  /// Close the socket. Called by destructor.
  /// @return <0 on error, 0 on success.
  virtual int Close();

  /// Logging callback is invoked with important event strings (disconnected,
  /// tx/rx errors, etc.)
  /// @see LoggingCallback
  void SetLogCallback(const LoggingCallback &cb) { _loggingCallback = cb; }

  bool SocketValid() const;

 private:
  std::mutex _mutex;
  std::atomic<uint64_t> _socket;

  std::string _hostname;
  int _port;

  bool ShouldLog(LogLevel level);

  // warning: windows nonsense
  std::string GetErrorString(int err, const char *prefix = "");

  // @return WSAGetLastError()
  int LogLastError(const char *prefix = "", LogLevel level = LOG_ERROR);

  // calls _loggingCallback
  virtual void Log(std::string str, LogLevel level = LOG_INFO);

  LoggingCallback _loggingCallback;
};

#endif
