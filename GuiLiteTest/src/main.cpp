// HelloWidgets.cpp : Defines the entry point for the application.
//
#include "WinUI.h"
#include <iostream>
#include <thread>

void DoStuff() {
  std::cout << "button clicked\n";
}

void DoStuff1() {
  std::cout << "button1 clicked\n";
}

int main(int argc, char * argv[])
{
  WinUI ui;
  ui.AddButton("button", DoStuff);
  ui.Run();
  while (ui.IsRunning()) {
    std::this_thread::yield();
  }
  std::cout << "exit\n";
}
