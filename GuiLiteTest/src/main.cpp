// HelloWidgets.cpp : Defines the entry point for the application.
//
#include "WinUI.h"
#include <iostream>
#include <thread>

void DoStuff() {
  std::cout << "button clicked\n";
}

int main(int argc, char * argv[])
{
  WinUI ui;
  ui.AddButton("Test", DoStuff);
  ui.Run();
  while (ui.IsRunning()) {
    std::this_thread::yield();
  }
  std::cout << "exit\n";
}
