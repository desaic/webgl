#ifndef COMMAND_HPP
#define COMMAND_HPP

#include <string>
#include <memory>
#include "threadsafe_queue.h"

struct Command {
  std::string _name = "";
  Command(const std::string& name) : _name(name) {}
  virtual const std::string& GetName() const { return _name; }
  virtual ~Command() {}
  virtual void Run() {}
};

typedef std::shared_ptr<Command> CmdPtr;
typedef threadsafe_queue<CmdPtr> CmdQueue;

#endif  // !COMMAND_HPP
