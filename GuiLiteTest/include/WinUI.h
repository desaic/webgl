#pragma once

#include <functional>
#include <string>
#include <vector>
#include <thread>
#include <windows.h>

class CUIBlock;
class ui_widgets;
using ButtonCallback = std::function<void()>;
///windows wrapper for CUIBlock
class WinUI {
public:

  //initialize global ui variables.
  static int InitGlobal();
  
  static bool globalUIInitialized;

  WinUI();
  ~WinUI();

  void Run();
  void Stop();
  bool IsRunning();
  void MsgLoop();
  BOOL InitInstance(HINSTANCE hInstance, int nCmdShow);
  void AddButton(const std::string& label, ButtonCallback cb);
  CUIBlock* GetUIBlock() { return ui_block; }
private:

  bool running;
  CUIBlock* ui_block;
  HINSTANCE hInst;
  HWND hWnd;

  unsigned UI_WIDTH;
  unsigned UI_HEIGHT;

  std::thread msgThread;
};
