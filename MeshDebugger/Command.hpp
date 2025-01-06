#ifndef COMMAND_HPP
#define COMMAND_HPP

#include <string>

struct Command {
  std::string _name = "";
  Command(const std::string& name) : _name(name) {}
  virtual const std::string& GetName() const { return _name; }
  virtual ~Command() {}
  virtual void Run() {}
};

#endif  // !COMMAND_HPP
