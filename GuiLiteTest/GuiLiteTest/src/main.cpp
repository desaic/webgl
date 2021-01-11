// HelloWidgets.cpp : Defines the entry point for the application.
//
#include "WinUI.h"
#include <thread>

int main(int argc, char * argv[])
{
  WinUI ui;
  ui.Run();
  while (ui.IsRunning()) {
    std::this_thread::yield();
  }
}
