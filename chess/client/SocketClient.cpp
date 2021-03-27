#include "SocketClient.h"

#include <WS2tcpip.h>
#include <WinSock2.h>

#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

namespace {
// @todo(mike) replace this with verbosity levels at some point
const uint32_t ONLY_LOG = LOG_ERROR | LOG_INFO;

/// RAII wrapper for addrinfo
struct AddrInfoDeleter {
  void operator()(addrinfo *a) {
    if (a) freeaddrinfo(a);
  }
};
using ScopedAddrInfo = std::unique_ptr<addrinfo, AddrInfoDeleter>;

struct timeval create_timeval(long timeout_ms) {
  return {
      timeout_ms / 1000,
      1000 * (timeout_ms % 1000),
  };
}

static std::string BufferString(const char *buf, size_t len,
                                size_t print_limit = 100,
                                bool useascii = true) {
  std::ostringstream ss;
  ss << std::setfill('0') << std::hex;
  size_t i = 0;
  while (i < print_limit && i < len) {
    if (useascii && isprint((unsigned char)buf[i])) {
      ss << buf[i];
    } else {
      ss << "\\x" << std::setw(2) << (0xFF & buf[i]);
    }
    i++;
  }

  if (i < len) {
    ss << "... <" << std::dec << (len - i) << " more bytes>";
  }

  return ss.str();
};

}  // namespace

SocketClient::SocketClient()
    : _mutex(),
      _socket(INVALID_SOCKET),
      _hostname(""),
      _port(0),
      _loggingCallback() {}

SocketClient::~SocketClient() { Close(); }

int SocketClient::Connect(const std::string hostname, uint16_t port,
                          long timeout_ms) {
  // @todo(mike) probably shouldn't lock the whole method?
  std::lock_guard<std::mutex> lock(_mutex);

  int ret;

  // create new socket

  if (_socket != INVALID_SOCKET) {
    // can't create a new one
    Log("Connect: already connected", LOG_DEBUG);  // @todo(mike) verbose?
    return 0;
  }

  _socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

  if (_socket == INVALID_SOCKET) {
    LogLastError("failed to create socket: ", LOG_DEBUG);
    return SOCKET_ERROR;
  }

  // translate host name to address

  struct addrinfo hints;
  std::memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  _hostname = hostname;
  _port = port;

  std::string port_string = std::to_string(port);
  struct addrinfo *result = nullptr;
  // so we don't have to freeaddrinfo everywhere
  ScopedAddrInfo resultPtr(nullptr);
  ret = getaddrinfo(_hostname.c_str(), port_string.c_str(), &hints, &result);

  if (ret != 0) {
    LogLastError("failed to getaddrinfo: ", LOG_DEBUG);
    return ret;
  }

  // set non blocking

  u_long is_non_blocking = 1;
  ret = ioctlsocket(_socket, FIONBIO, &is_non_blocking);
  if (ret != 0) {
    LogLastError("failed to temporarily set non-blocking: ", LOG_DEBUG);
    closesocket(_socket);
    _socket = INVALID_SOCKET;
    return SOCKET_ERROR;
  }

  // connect

  struct addrinfo *addr;  // linked list of possible addresses
  int err = 0;
  for (addr = result; addr != nullptr; addr = addr->ai_next) {
    ret = connect(_socket, addr->ai_addr, static_cast<int>(addr->ai_addrlen));
    err = WSAGetLastError();
    if (ret == 0 || err == WSAEWOULDBLOCK) {
      break;
    }
  }

  if (! (ret == 0 || err == WSAEWOULDBLOCK)) {
    LogLastError("unable to connect: ", LOG_DEBUG);
    closesocket(_socket);
    _socket = INVALID_SOCKET;
    return SOCKET_ERROR;
  }

  bool connected = WaitWriteable(timeout_ms);

  if (!connected) {
    Log("unable to connect: timeout", LOG_DEBUG);
    closesocket(_socket);
    _socket = INVALID_SOCKET;
    return SOCKET_ERROR;
  }
  Log("Connected", LOG_INFO);

  // set to blocking again

  is_non_blocking = 0;
  ret = ioctlsocket(_socket, FIONBIO, &is_non_blocking);
  if (ret != 0) {
    LogLastError("failed to reset blocking: ", LOG_DEBUG);
    closesocket(_socket);
    _socket = INVALID_SOCKET;
    return SOCKET_ERROR;
  }

  return ret;
}

int SocketClient::Close() {
  std::lock_guard<std::mutex> lock(_mutex);

  if (_socket == INVALID_SOCKET) {
    return 0;
  }

  // @todo(mike) closesocket on non-blocking?
  int ret = closesocket(_socket);
  _socket = INVALID_SOCKET;

  if (ret == 0) {
    Log("Close success", LOG_DEBUG);
    return 0;
  }

  LogLastError("Close error: ", LOG_ERROR);
  return ret;
}

int64_t SocketClient::Recv(char *buf, uint32_t len, long timeout_ms) {
  
  if (! WaitReadable(timeout_ms)) {
    int err = WSAGetLastError();
    Log("recv: timeout with error "+std::to_string(err), LOG_DEBUG);
    return -WSAETIMEDOUT;
  } 
    // @todo(mike) I don't think this needs the mutex but I'm not positive
  int ret = recv(_socket, buf, len, 0);

    if (ret == 0) {
        // remote closed
        Log("recv: the remote has closed the connection", LOG_ERROR);

        closesocket(_socket);
        _socket = INVALID_SOCKET;
    } else if (ret < 0) {
        int err = WSAGetLastError();
        LogLastError("recv: ", LOG_ERROR);
        closesocket(_socket);
        _socket = INVALID_SOCKET;
        ret = -err;
    } else {
        ///@TODO This could generate a ton of text for large data.
        Log("Recv: " + BufferString(buf, ret), LOG_DEBUG);
    }

  return ret;
}

int64_t SocketClient::Send(const char *buf, uint32_t nBytes, long timeout_ms) {
  int result = 0;
  // std::lock_guard<std::mutex> lock(_mutex);
  // @todo(mike) Not sure if this needs mutex, I think no

  result = send(_socket, buf, int(nBytes), 0);

  if (result < 0) {  // @todo(mike) == 0 case might not be ok
    LogLastError("Send error: ", LOG_ERROR);
    closesocket(_socket);
    _socket = INVALID_SOCKET;
    return result;
  }

  Log("Send: " + BufferString(buf, result), LOG_DEBUG);  // @todo verbose?

  return result;
}

int64_t SocketClient::SendAll(const char *buf, uint32_t nBytes,
                              long timeout_ms) {
  uint32_t sent = 0;

  while (sent < nBytes) {
    int64_t ret = Send(buf + sent, nBytes - sent, timeout_ms);

    if (ret <= 0) {  // @todo(mike) can this return 0?
      return ret;
    }

    sent += ret;
  }

  return sent;
}

bool SocketClient::WaitReadable(long timeout_ms) {
  struct timeval tv = create_timeval(timeout_ms);
  fd_set set;
  FD_ZERO(&set);
  FD_SET(_socket, &set);

  return select(0, &set, nullptr, nullptr, &tv) == 1;
}

bool SocketClient::WaitWriteable(long timeout_ms) {
  struct timeval tv = create_timeval(timeout_ms);
  fd_set set;
  FD_ZERO(&set);
  FD_SET(_socket, &set);

  return select(0, nullptr, &set, nullptr, &tv) == 1;
}

bool SocketClient::SocketValid() const { return _socket != INVALID_SOCKET; }

bool SocketClient::ShouldLog(LogLevel level) {
  return _loggingCallback && (level & ONLY_LOG);
}

std::string SocketClient::GetErrorString(int err, const char *prefix) {
  // from https://stackoverflow.com/a/16723307

  TCHAR *buf = nullptr;

  FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                    FORMAT_MESSAGE_IGNORE_INSERTS,          // flags
                NULL,                                       // lpsource
                err,                                        // message id
                MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT),  // languageid
                (LPTSTR)(&buf),                             // output buffer
                0,                                          // size of buf
                NULL);  // va_list of arguments

  // convert from unicode to ascii
  std::ostringstream ss;

  ss << prefix << buf;

  LocalFree(buf);

  return ss.str();
}

int SocketClient::LogLastError(const char *prefix, LogLevel level) {
  // note: WSAGetLastError is per-thread
  int err = WSAGetLastError();

  if (ShouldLog(level)) {
    Log(GetErrorString(err, prefix), level);
  }

  return err;
}

void SocketClient::Log(std::string str, LogLevel level) {
  if (ShouldLog(level)) {
    std::ostringstream ss;
    ss << "[" << _hostname << ":" << _port << "] " << str;
    _loggingCallback(ss.str(), level);
  }
}
