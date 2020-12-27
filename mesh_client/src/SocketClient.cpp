#include "SocketClient.h"

#include <WS2tcpip.h>
#include <WinSock2.h>

#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

SocketClient::SocketClient()
    : _mutex(),
      _socket(INVALID_SOCKET),
      _hostname(""),
      _port(0),
      _loggingCallback() {}

SocketClient::~SocketClient() { Close(); }

int SocketClient::Connect() {
  // @todo(mike) probably shouldn't lock the whole method?
  std::lock_guard<std::mutex> lock(_mutex);

  int ret;

  // create new socket

  if (_socket != INVALID_SOCKET) {
    //already connected.
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

  std::string port_string = std::to_string(_port);
  struct addrinfo *result = nullptr;
  ret = getaddrinfo(_hostname.c_str(), port_string.c_str(), &hints, &result);

  if (ret != 0) {
    LogLastError("failed to getaddrinfo: ", LOG_DEBUG);
    if (result) {
      freeaddrinfo(result);
    }
    return ret;
  }

  // connect

  struct addrinfo *addr;  // linked list of possible addresses
  for (addr = result; addr != nullptr; addr = addr->ai_next) {
    ret = connect(_socket, addr->ai_addr, static_cast<int>(addr->ai_addrlen));
    if (ret == 0) {
      break;
    }
  }

  if (ret != 0) {
    LogLastError("unable to connect: ", LOG_DEBUG);
    closesocket(_socket);
    _socket = INVALID_SOCKET;
    return SOCKET_ERROR;
  }

  freeaddrinfo(result);
  Log("Connected", LOG_INFO);

  return ret;
}

int SocketClient::Close() {
  std::lock_guard<std::mutex> lock(_mutex);

  if (_socket == INVALID_SOCKET) {
    return 0;
  }

  int ret = closesocket(_socket);
  _socket = INVALID_SOCKET;

  if (ret == 0) {
    Log("Close success", LOG_DEBUG);
    return 0;
  }

  LogLastError("Close error: ", LOG_ERROR);
  return ret;
}

int64_t SocketClient::Recv(char *buf, uint32_t len, unsigned timeout_ms) {
  // std::lock_guard<std::mutex> lock(_mutex);
  int ret;

  // set timeout
  DWORD timeout_dw = timeout_ms;
  ret = setsockopt(_socket, SOL_SOCKET, SO_RCVTIMEO,
                   reinterpret_cast<const char *>(&timeout_dw),
                   sizeof(timeout_dw));

  if (ret != 0) {
    LogLastError("recv: setsockopt timeout failed: ", LOG_DEBUG);
    return ret;
  }

  // @todo(mike) I don't think this needs the mutex but I'm not positive
  ret = recv(_socket, buf, len, 0);

  if (ret == 0) {
    // remote closed
    Log("recv: the remote has closed the connection", LOG_DEBUG);

    closesocket(_socket);
    _socket = INVALID_SOCKET;

    // @todo(mike) WSAGetLastError is meaningless here because of above
    // closesocket call resetting it
    return ret;
  } else if (ret < 0) {
    int err = WSAGetLastError();
    //time out is totally fine meaning server didn't send data.
    if(err != WSAEWOULDBLOCK && err != WSAETIMEDOUT){
        LogLastError("recv: ", LOG_DEBUG);
        closesocket(_socket);
        _socket = INVALID_SOCKET;
    }
    ret = -err;
  } else {
    
  }

  return ret;
}

int64_t SocketClient::Send(const char *buf, uint32_t nBytes) {
  // std::lock_guard<std::mutex> lock(_mutex);
  // @todo(mike) Not sure if this needs mutex, I think no
  int result = send(_socket, buf, int(nBytes), 0);

  if (result < 0) {  // @todo(mike) == 0 case might not be ok
    LogLastError("Send error: ", LOG_DEBUG);
    closesocket(_socket);
    _socket = INVALID_SOCKET;
    return result;
  }
  return result;
}

int64_t SocketClient::SendAll(const char *buf, uint32_t nBytes) {
  uint32_t sent = 0;

  while (sent < nBytes) {
    int64_t ret = Send(buf + sent, nBytes - sent);

    if (ret <= 0) {  // @todo(mike) can this return 0?
      return ret;
    }

    sent += ret;
  }

  return sent;
}

bool SocketClient::SocketValid()
{
    return _socket != INVALID_SOCKET;
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
  Log(GetErrorString(err, prefix), level);
  return err;
}

void SocketClient::Log(std::string str, LogLevel level) {
  if (_loggingCallback) {
    std::ostringstream ss;
    ss << "[" << _hostname << ":" << _port << "] " << str;
    _loggingCallback(ss.str(), level);
  }
}
